/*******************************************************************************
 * main_minimal.cpp: o menor programa que roda o BRKGA-MP(-IPR) no PDR2F.
 *
 * Uso:
 *   ./main_minimal <semente> <config> <tempo-max-s> <instancia> <decodificador>
 *                  [<geracoes>]
 *
 *   <decodificador>  threshold | permutation
 *   <geracoes>       se informado, para tambem ao atingir este numero de
 *                    geracoes (criterio combinado com o de tempo)
 *
 * Exemplo:
 *   ./main_minimal 0 configs/brkga-mp/dev_threshold.conf 60 \
 *                  instances/random/100_1.txt threshold 100
 *
 * Este arquivo e' o UNICO que inclui brkga_mp_ipr.hpp (via
 * distances/pdr2f_distance.hpp): incluir a API em mais de um .cpp gera
 * simbolos duplicados.
 ******************************************************************************/

#include "decoders/pdr2f_brkga_decoder.hpp"
#include "distances/pdr2f_distance.hpp"     // inclui a API
#include "pdr2f/pdr2f_feasibility.hpp"
#include "pdr2f/pdr2f_instance.hpp"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

// ARMADILHA DA API: os operadores de I/O dos enums vem de
// third_part/enum_io.hpp, que a API inclui DENTRO do namespace BRKGA. Como os
// tipos BRKGA::PathRelinking::Type e companhia estao no namespace interno
// BRKGA::PathRelinking, a busca por argumento (ADL) nao alcanca o operador
// daqui de fora, e imprimir um desses enums nao compila. O using resolve.
using BRKGA::operator<<;

int main(int argc, char* argv[]) {
    if(argc < 6 || argc > 7) {
        std::cerr
            << "Uso: " << argv[0]
            << " <semente> <config> <tempo-max-s> <instancia> <decodificador>"
               " [<geracoes>]\n"
               "     <decodificador> = threshold | permutation\n";
        return 1;
    }

    try {
        const unsigned seed = static_cast<unsigned>(std::stoul(argv[1]));
        const std::string config_file = argv[2];
        const unsigned max_time = static_cast<unsigned>(std::stoul(argv[3]));
        const std::string instance_file = argv[4];
        const auto decoder_kind = PDR2F_BrkgaDecoder::kind_from_name(argv[5]);
        const unsigned generations =
            (argc == 7) ? static_cast<unsigned>(std::stoul(argv[6])) : 0;

        ////////////////////////////////////////////////////////////////////////
        // Instancia e decodificador
        ////////////////////////////////////////////////////////////////////////

        const PDR2F_Instance instance(instance_file);

        std::cout << "Instancia: " << instance_file
                  << " | n = " << instance.num_nodes
                  << " | m = " << instance.num_edges << '\n';

        // A busca local de reducao fica SEMPRE ativa: a decisao de projeto e'
        // que os dois decodificadores a recebem, para que a comparacao entre
        // eles isole a representacao.
        PDR2F_BrkgaDecoder decoder(instance, decoder_kind, true);

        ////////////////////////////////////////////////////////////////////////
        // Parametros
        ////////////////////////////////////////////////////////////////////////

        auto [brkga_params, control_params] =
            BRKGA::readConfiguration(config_file);

        // ARMADILHA DA API: com bias_type CUSTOM e sem chamar
        // setBiasCustomFunction(), a API cai silenciosamente na funcao
        // CONSTANT, embora os parametros continuem exibindo CUSTOM. Em vez de
        // deixar isso passar, recusamos o config.
        if(brkga_params.bias_type == BRKGA::BiasFunctionType::CUSTOM) {
            std::cerr << "ERRO: bias_type CUSTOM exige setBiasCustomFunction(),"
                         " que este main nao chama. Use uma das funcoes"
                         " embutidas (CONSTANT, LINEAR, QUADRATIC, CUBIC,"
                         " EXPONENTIAL, LOGINVERSE).\n";
            return 1;
        }

        // A distancia customizada precisa ser atribuida ANTES de construir o
        // algoritmo. Ela so' faz sentido com o decodificador por limiares.
        if(brkga_params.pr_distance_function_type
               == BRKGA::PathRelinking::DistanceFunctionType::CUSTOM) {
            brkga_params.pr_distance_function =
                std::make_shared<PDR2F_ThresholdDistance>();
        }

        // Coerencia entre representacao e IPR. A escolha do decodificador e a
        // do tipo de religacao sao uma decisao so'; um par errado nao quebra a
        // execucao, apenas desperdica o IPR em silencio.
        if(control_params.ipr_interval > 0) {
            const bool is_threshold =
                decoder_kind == PDR2F_BrkgaDecoder::Kind::THRESHOLD;

            const bool pr_ok = is_threshold
                ? brkga_params.pr_type == BRKGA::PathRelinking::Type::DIRECT
                : brkga_params.pr_type == BRKGA::PathRelinking::Type::PERMUTATION;

            const bool dist_ok = is_threshold
                ? brkga_params.pr_distance_function_type
                      == BRKGA::PathRelinking::DistanceFunctionType::CUSTOM
                : brkga_params.pr_distance_function_type
                      == BRKGA::PathRelinking::DistanceFunctionType::KENDALLTAU;

            if(!pr_ok || !dist_ok) {
                std::cerr << "AVISO: configuracao de IPR incoerente com o"
                             " decodificador '"
                          << PDR2F_BrkgaDecoder::name_of(decoder_kind)
                          << "'. O esperado e' "
                          << (is_threshold ? "DIRECT + CUSTOM"
                                           : "PERMUTATION + KENDALLTAU")
                          << ", e o config traz " << brkga_params.pr_type
                          << " + " << brkga_params.pr_distance_function_type
                          << ".\n";
            }
        }

        // O tempo da linha de comando sobrescreve o do config.
        control_params.maximum_running_time = std::chrono::seconds{max_time};

        ////////////////////////////////////////////////////////////////////////
        // Algoritmo
        ////////////////////////////////////////////////////////////////////////

        // Uma unica thread: o protocolo do artigo do VNS e' single-threaded, e
        // dar ao BRKGA varias threads sob o mesmo limite de tempo lhe daria um
        // orcamento de CPU varias vezes maior.
        constexpr unsigned NUM_THREADS = 1;

        BRKGA::BRKGA_MP_IPR<PDR2F_BrkgaDecoder> algorithm(
            decoder, BRKGA::Sense::MINIMIZE, seed,
            instance.num_nodes, brkga_params, NUM_THREADS
        );

        if(generations > 0) {
            // Parada combinada: tempo OU numero de geracoes, o que vier
            // primeiro.
            algorithm.setStoppingCriteria(
                [generations](const BRKGA::AlgorithmStatus& status) {
                    return status.current_iteration >= generations;
                }
            );
        }

        std::cout << "Decodificador: "
                  << PDR2F_BrkgaDecoder::name_of(decoder_kind)
                  << " com busca local de reducao\n"
                     "Parada: " << max_time << " s";
        if(generations > 0) std::cout << " OU " << generations << " geracoes";
        std::cout << "\n\nRodando...\n";

        const BRKGA::AlgorithmStatus status =
            algorithm.run(control_params, &std::cout);

        ////////////////////////////////////////////////////////////////////////
        // Resultado
        ////////////////////////////////////////////////////////////////////////

        // O decodificador nao guarda a rotulacao: decode() devolve so' o peso.
        // Para relatar e verificar a solucao, decodificamos de novo o melhor
        // cromossomo. Como o decodificador e' deterministico, isso reproduz
        // exatamente a mesma rotulacao que gerou aquele fitness.
        const PDR2F_Solution best = decoder.solution(status.best_chromosome);
        const PDR2F_Violation violation =
            check_feasibility(instance, best.labels);

        std::cout << "\n--------------------------------------------------\n"
                  << "Peso da melhor solucao : " << best.weight << '\n'
                  << "Fitness devolvido pela API: " << status.best_fitness << '\n'
                  << "Viabilidade            : " << violation_text(violation)
                  << '\n'
                  << "Geracoes               : " << status.current_iteration << '\n'
                  << "Ultima melhora         : geracao "
                  << status.last_update_iteration << '\n'
                  << "Tempo total            : " << status.current_time.count()
                  << " s\n"
                  << "Tempo da ultima melhora: "
                  << status.last_update_time.count() << " s\n"
                  << "Chamadas de IPR        : "
                  << status.num_path_relink_calls << '\n'
                  << "Melhoras pelo IPR      : "
                  << status.num_best_improvements << " no melhor, "
                  << status.num_elite_improvements << " na elite\n"
                  << "Homogeneidades no IPR  : " << status.num_homogenities << '\n'
                  << "Trocas de elite        : " << status.num_exchanges << '\n'
                  << "--------------------------------------------------\n";

        // Dois erros graves e silenciosos que valem uma verificacao explicita:
        // uma solucao inviavel, ou um fitness que nao corresponde a' rotulacao.
        if(violation.kind != PDR2F_Violation::NONE) {
            std::cerr << "ERRO: a melhor solucao e' INVIAVEL.\n";
            return 2;
        }
        // Comparacao feita em inteiros: o fitness e' um peso, inteiro por
        // natureza, e comparar doubles por igualdade seria fragil.
        const unsigned api_weight =
            static_cast<unsigned>(status.best_fitness + 0.5);
        if(best.weight != api_weight) {
            std::cerr << "ERRO: o peso recalculado (" << best.weight
                      << ") difere do fitness da API ("
                      << status.best_fitness << ").\n";
            return 2;
        }
    }
    catch(const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
