/*******************************************************************************
 * main_complete.cpp: execucao completa do BRKGA-MP(-IPR) no PDR2F, com saida
 * em CSV para as baterias de experimento.
 *
 * Uso:
 *   ./main_complete --config <arq> --seed <n> --instance <arq>
 *                   --decoder <threshold|permutation>
 *                   [--maxtime <s>] [--generations <n>] [--warmstart <n>]
 *                   [--no-reduction] [--threads <n>] [--quiet] [--header]
 *
 *   --maxtime      se omitido, usa a FAIXA DO PROTOCOLO conforme n:
 *                  n < 30 -> 300 s; 30 <= n < 100 -> 600 s; n >= 100 -> 1200 s
 *                  (Djukanovic et al., 2025)
 *   --generations  limite adicional de geracoes (0 = so' tempo)
 *   --warmstart    quantas solucoes gulosas injetar na populacao inicial
 *   --no-reduction desliga a busca local (para estudos de ablacao)
 *   --header       imprime apenas o cabecalho do CSV e termina
 *
 * O ALGORITMO E' DEDUZIDO DO CONFIG, nao informado na linha de comando:
 * ipr_interval = 0 significa BRKGA-MP, e qualquer outro valor significa
 * BRKGA-MP-IPR. Assim o rotulo no CSV nunca discorda do que foi de fato
 * executado.
 *
 * A ultima linha da saida padrao e' sempre a linha de CSV, para o script de
 * bateria poder captura-la com tail -1.
 ******************************************************************************/

#include "decoders/pdr2f_brkga_decoder.hpp"
#include "distances/pdr2f_distance.hpp"     // inclui a API
#include "heuristics/greedy_pdr2f.hpp"
#include "pdr2f/pdr2f_encoding.hpp"
#include "pdr2f/pdr2f_feasibility.hpp"
#include "pdr2f/pdr2f_instance.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Ver armadilha 12 em claude/estudo_api_brkga_cpp.md.
using BRKGA::operator<<;

namespace {

const char* const CSV_HEADER =
    "Instance,Class,NumNodes,NumEdges,Algorithm,Decoder,Reduction,Seed,"
    "Weight,Feasible,TimeLimit,WarmStart,InitialWeight,"
    "Generations,LastUpdateGeneration,TotalTime,LastUpdateTime,"
    "LargestIterationOffset,StalledIterations,"
    "IPRTime,IPRCalls,IPRHomogenities,IPRBestImprovements,"
    "IPREliteImprovements,Exchanges,Shakes,Resets";

/// Faixa de tempo do protocolo, em segundos, conforme o tamanho da instancia.
unsigned protocol_time_limit(unsigned n) {
    if(n < 30)  return 300;
    if(n < 100) return 600;
    return 1200;
}

/// Nome da instancia sem diretorio nem extensao.
std::string instance_name(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path
                                                    : path.substr(slash + 1);
    if(name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0)
        name.erase(name.size() - 4);
    return name;
}

/// Classe da instancia: o nome do diretorio que a contem.
std::string instance_class(const std::string& path) {
    const std::size_t last = path.find_last_of("/\\");
    if(last == std::string::npos) return "?";
    const std::size_t prev = path.find_last_of("/\\", last - 1);
    return (prev == std::string::npos) ? path.substr(0, last)
                                       : path.substr(prev + 1, last - prev - 1);
}

std::map<std::string, std::string> parse_arguments(int argc, char* argv[]) {
    std::map<std::string, std::string> args;

    for(int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if(key.rfind("--", 0) != 0)
            throw std::runtime_error("argumento inesperado: " + key);
        key.erase(0, 2);

        // Opcoes sem valor.
        if(key == "no-reduction" || key == "quiet" || key == "header") {
            args[key] = "1";
            continue;
        }
        if(i + 1 >= argc)
            throw std::runtime_error("a opcao --" + key + " exige um valor");
        args[key] = argv[++i];
    }
    return args;
}

const std::string& get_required(const std::map<std::string, std::string>& args,
                                const std::string& key)
{
    const auto it = args.find(key);
    if(it == args.end())
        throw std::runtime_error("a opcao --" + key + " e' obrigatoria");
    return it->second;
}

std::string get_optional(const std::map<std::string, std::string>& args,
                         const std::string& key,
                         const std::string& fallback)
{
    const auto it = args.find(key);
    return (it == args.end()) ? fallback : it->second;
}

} // namespace

int main(int argc, char* argv[]) try {
    const auto args = parse_arguments(argc, argv);

    if(args.count("header")) {
        std::cout << CSV_HEADER << '\n';
        return 0;
    }
    if(args.empty()) {
        std::cerr << "Uso: " << argv[0]
                  << " --config <arq> --seed <n> --instance <arq>"
                     " --decoder <threshold|permutation>\n"
                     "     [--maxtime <s>] [--generations <n>]"
                     " [--warmstart <n>] [--no-reduction]\n"
                     "     [--threads <n>] [--quiet] [--header]\n";
        return 1;
    }

    const std::string config_file   = get_required(args, "config");
    const std::string instance_file = get_required(args, "instance");
    const unsigned seed =
        static_cast<unsigned>(std::stoul(get_required(args, "seed")));
    const auto decoder_kind =
        PDR2F_BrkgaDecoder::kind_from_name(get_required(args, "decoder"));

    const unsigned generations =
        static_cast<unsigned>(std::stoul(get_optional(args, "generations", "0")));
    const unsigned warm_start =
        static_cast<unsigned>(std::stoul(get_optional(args, "warmstart", "0")));
    const unsigned num_threads =
        static_cast<unsigned>(std::stoul(get_optional(args, "threads", "1")));
    const bool reduction = !args.count("no-reduction");
    const bool quiet     = args.count("quiet") > 0;

    // Em modo silencioso tudo o que seria log vai para um buffer descartado,
    // inclusive os avisos do readConfiguration da API. Assim a saida padrao
    // contem apenas a linha de CSV, que e' o que o script de bateria le.
    std::ostringstream sink;
    std::ostream& log = quiet ? static_cast<std::ostream&>(sink)
                              : static_cast<std::ostream&>(std::cout);

    ////////////////////////////////////////////////////////////////////////////
    // Instancia, faixa de tempo e decodificador
    ////////////////////////////////////////////////////////////////////////////

    const PDR2F_Instance instance(instance_file);

    const unsigned max_time = args.count("maxtime")
        ? static_cast<unsigned>(std::stoul(args.at("maxtime")))
        : protocol_time_limit(instance.num_nodes);

    PDR2F_BrkgaDecoder decoder(instance, decoder_kind, reduction);

    log << "Instancia: " << instance_name(instance_file)
        << " (" << instance_class(instance_file) << ")"
        << " | n = " << instance.num_nodes
        << " | m = " << instance.num_edges << '\n'
        << "Limite de tempo: " << max_time << " s"
        << (args.count("maxtime") ? " (da linha de comando)"
                                  : " (faixa do protocolo)") << '\n';

    ////////////////////////////////////////////////////////////////////////////
    // Parametros
    ////////////////////////////////////////////////////////////////////////////

    auto [brkga_params, control_params] =
        BRKGA::readConfiguration(config_file, log);

    if(brkga_params.bias_type == BRKGA::BiasFunctionType::CUSTOM)
        throw std::runtime_error(
            "bias_type CUSTOM exige setBiasCustomFunction(), que este main nao"
            " chama; use uma das funcoes embutidas");

    if(brkga_params.pr_distance_function_type
           == BRKGA::PathRelinking::DistanceFunctionType::CUSTOM) {
        brkga_params.pr_distance_function =
            std::make_shared<PDR2F_ThresholdDistance>();
        log << "Funcao de distancia CUSTOM atribuida: Hamming sobre os 4"
               " rotulos.\n";
    }

    const bool is_threshold =
        decoder_kind == PDR2F_BrkgaDecoder::Kind::THRESHOLD;

    if(control_params.ipr_interval > 0) {
        const bool pr_ok = is_threshold
            ? brkga_params.pr_type == BRKGA::PathRelinking::Type::DIRECT
            : brkga_params.pr_type == BRKGA::PathRelinking::Type::PERMUTATION;
        const bool dist_ok = is_threshold
            ? brkga_params.pr_distance_function_type
                  == BRKGA::PathRelinking::DistanceFunctionType::CUSTOM
            : brkga_params.pr_distance_function_type
                  == BRKGA::PathRelinking::DistanceFunctionType::KENDALLTAU;

        if(!pr_ok || !dist_ok)
            std::cerr << "AVISO: IPR incoerente com o decodificador '"
                      << PDR2F_BrkgaDecoder::name_of(decoder_kind)
                      << "': esperado "
                      << (is_threshold ? "DIRECT + CUSTOM"
                                       : "PERMUTATION + KENDALLTAU")
                      << ", config traz " << brkga_params.pr_type << " + "
                      << brkga_params.pr_distance_function_type << ".\n";
    }

    control_params.maximum_running_time = std::chrono::seconds{max_time};

    // O rotulo do algoritmo vem do config, nao da linha de comando.
    const std::string algorithm_name =
        (control_params.ipr_interval == 0) ? "brkga-mp" : "brkga-mp-ipr";

    ////////////////////////////////////////////////////////////////////////////
    // Algoritmo
    ////////////////////////////////////////////////////////////////////////////

    BRKGA::BRKGA_MP_IPR<PDR2F_BrkgaDecoder> algorithm(
        decoder, BRKGA::Sense::MINIMIZE, seed,
        instance.num_nodes, brkga_params, num_threads
    );

    ////////////////////////////////////////////////////////////////////////////
    // Warm start
    ////////////////////////////////////////////////////////////////////////////

    unsigned initial_weight = 0;

    if(warm_start > 0) {
        std::mt19937 rng(seed);

        // Duplicatas sao eliminadas: no TCC 1, injetar muitas copias quase
        // identicas do guloso encheu a elite de clones e PIOROU o resultado.
        std::set<std::vector<unsigned>> distinct;
        std::vector<BRKGA::Chromosome> chromosomes;

        for(unsigned i = 0; i < warm_start * 4 && chromosomes.size() < warm_start;
            ++i)
        {
            const PDR2F_Solution greedy = greedy_pdr2f(instance, rng);
            if(!distinct.insert(greedy.labels).second) continue;

            chromosomes.push_back(is_threshold
                ? encode_threshold(greedy.labels, rng)
                : encode_permutation(greedy.labels, rng));
        }

        algorithm.setInitialPopulation(chromosomes);

        // O que vale e' o peso do cromossomo DECODIFICADO, nao o da solucao
        // gulosa de origem: para o decodificador por permutacao a codificacao
        // e' aproximada (ver src/pdr2f/pdr2f_encoding.hpp).
        for(const auto& chromosome : chromosomes) {
            const unsigned w = decoder.solution(chromosome).weight;
            if(initial_weight == 0 || w < initial_weight) initial_weight = w;
        }

        log << "Warm start: " << chromosomes.size() << " solucoes distintas"
               " injetadas, melhor peso decodificado = " << initial_weight
            << '\n';
    }

    ////////////////////////////////////////////////////////////////////////////
    // Criterios de parada e observador
    ////////////////////////////////////////////////////////////////////////////

    if(generations > 0)
        algorithm.setStoppingCriteria(
            [generations](const BRKGA::AlgorithmStatus& status) {
                return status.current_iteration >= generations;
            }
        );

    if(!quiet)
        // ARMADILHA DA API: a documentacao diz que retornar true PARA a
        // otimizacao. O codigo faz "run &= callback(status)", logo e' false
        // que para. Retornamos true para continuar.
        algorithm.addNewSolutionObserver(
            [](const BRKGA::AlgorithmStatus& status) {
                std::cout << "* melhora: geracao " << status.current_iteration
                          << " | peso " << status.best_fitness
                          << " | " << status.current_time.count() << " s\n";
                return true;
            }
        );

    ////////////////////////////////////////////////////////////////////////////
    // Execucao
    ////////////////////////////////////////////////////////////////////////////

    const BRKGA::AlgorithmStatus status =
        algorithm.run(control_params, quiet ? nullptr : &std::cout);

    ////////////////////////////////////////////////////////////////////////////
    // Verificacao e saida
    ////////////////////////////////////////////////////////////////////////////

    const PDR2F_Solution best = decoder.solution(status.best_chromosome);
    const PDR2F_Violation violation = check_feasibility(instance, best.labels);
    const bool feasible = violation.kind == PDR2F_Violation::NONE;

    const unsigned api_weight =
        static_cast<unsigned>(status.best_fitness + 0.5);

    if(!feasible)
        std::cerr << "ERRO: melhor solucao INVIAVEL -- "
                  << violation_text(violation) << '\n';
    if(best.weight != api_weight)
        std::cerr << "ERRO: peso recalculado (" << best.weight
                  << ") difere do fitness da API (" << status.best_fitness
                  << ")\n";

    if(!quiet) std::cout << '\n';

    std::cout << std::fixed << std::setprecision(4)
              << instance_name(instance_file) << ','
              << instance_class(instance_file) << ','
              << instance.num_nodes << ','
              << instance.num_edges << ','
              << algorithm_name << ','
              << PDR2F_BrkgaDecoder::name_of(decoder_kind) << ','
              << (reduction ? "yes" : "no") << ','
              << seed << ','
              << best.weight << ','
              << (feasible ? "yes" : "NO") << ','
              << max_time << ','
              << warm_start << ','
              << initial_weight << ','
              << status.current_iteration << ','
              << status.last_update_iteration << ','
              << status.current_time.count() << ','
              << status.last_update_time.count() << ','
              << status.largest_iteration_offset << ','
              << status.stalled_iterations << ','
              << status.path_relink_time.count() << ','
              << status.num_path_relink_calls << ','
              << status.num_homogenities << ','
              << status.num_best_improvements << ','
              << status.num_elite_improvements << ','
              << status.num_exchanges << ','
              << status.num_shakes << ','
              << status.num_resets << '\n';

    return (feasible && best.weight == api_weight) ? 0 : 2;
}
catch(const std::exception& e) {
    std::cerr << "ERRO: " << e.what() << '\n';
    return 1;
}
