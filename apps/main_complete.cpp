/*******************************************************************************
 * main_complete.cpp: execucao completa do BRKGA-MP(-IPR) no PDR2F, com saida
 * em CSV para as baterias de experimento.
 *
 * Uso:
 *   ./main_complete --config <arq> --seed <n> --instance <arq>
 *                   --decoder <threshold|permutation>
 *                   [--maxtime <s>] [--generations <n>] [--target <peso>]
 *                   [--convlog <arq>] [--warmstart <n>]
 *                   [--no-reduction] [--threads <n>] [--quiet] [--header]
 *
 *   --maxtime      se omitido, 600 s para qualquer instancia (protocolo de
 *                  04/11/2026: tempo maximo de 10 min por execucao)
 *   --generations  limite adicional de geracoes (0 = so' tempo)
 *   --target       PARA ao atingir este peso (0 = sem alvo). Usado com o
 *                  otimo provado pelo PLI ou com o limite inferior das cubicas:
 *                  ninguem melhora um otimo, entao parar nele so' poupa tempo.
 *   --convlog      grava a curva de convergencia neste arquivo: uma linha a
 *                  cada melhora da melhor solucao e uma linha final
 *   --warmstart    quantas solucoes gulosas injetar na populacao inicial
 *   --no-reduction desliga a busca local (para estudos de ablacao)
 *
 * DESENHO EXPERIMENTAL (decisao de 02/11/2026): os experimentos usam apenas o
 * decodificador por PERMUTACAO, sempre COM a busca local de reducao, em dois
 * bracos -- BRKGA-MP e BRKGA-MP-IPR. O decodificador por limiares continua
 * disponivel no codigo (e nos testes), mas foi excluido dos experimentos.
 * --no-reduction existe apenas para estudos de ablacao.
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
#include <fstream>
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
    "IPREliteImprovements,Exchanges,Shakes,Resets,Target,StopReason";

/// Tempo maximo do protocolo, em segundos: 600 s para qualquer instancia.
/// (Ate' 04/11/2026 eram faixas de 300/600/1200 s conforme n, como no artigo
/// do VNS; o orientador fixou 10 min para todas.)
constexpr unsigned PROTOCOL_TIME_LIMIT = 600;

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
                     "     [--maxtime <s>] [--generations <n>] [--target <peso>]"
                     " [--convlog <arq>]\n"
                     "     [--warmstart <n>] [--no-reduction]\n"
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
    const unsigned target =
        static_cast<unsigned>(std::stoul(get_optional(args, "target", "0")));
    const std::string convlog = get_optional(args, "convlog", "");
    const unsigned warm_start =
        static_cast<unsigned>(std::stoul(get_optional(args, "warmstart", "0")));
    const unsigned num_threads =
        static_cast<unsigned>(std::stoul(get_optional(args, "threads", "1")));
    // A busca local de reducao fica ligada por padrao (ver o cabecalho).
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
        : PROTOCOL_TIME_LIMIT;

    PDR2F_BrkgaDecoder decoder(instance, decoder_kind, reduction);

    log << "Instancia: " << instance_name(instance_file)
        << " (" << instance_class(instance_file) << ")"
        << " | n = " << instance.num_nodes
        << " | m = " << instance.num_edges << '\n'
        << "Limite de tempo: " << max_time << " s"
        << (args.count("maxtime") ? " (da linha de comando)"
                                  : " (padrao do protocolo)") << '\n';
    if(target > 0)
        log << "Alvo: para ao atingir peso " << target << '\n';

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

    // RELOGIO: a API mede o tempo com std::chrono::system_clock, o relogio de
    // parede, que pode SALTAR. Na WSL do notebook ele recua ~30 s e volta
    // 0,2 s depois, a cada ~24 s (scripts/diagnostico_relogio.py). Uma
    // execucao iniciada dentro desse intervalo pararia ~30 s antes da hora.
    // Por isso o limite real e' imposto pelo criterio de parada abaixo, com
    // std::chrono::steady_clock (monotono, nunca salta), e o limite da API
    // fica 1 h acima, apenas como rede de seguranca.
    control_params.maximum_running_time =
        std::chrono::seconds{max_time + 3600};

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

    // Instante inicial no relogio monotono; atribuido logo antes do run().
    using SteadyClock = std::chrono::steady_clock;
    SteadyClock::time_point steady_start = SteadyClock::now();
    const auto elapsed = [&steady_start]() {
        return std::chrono::duration<double>(SteadyClock::now() - steady_start)
                   .count();
    };

    // Criterios de parada: tempo (monotono) e, se pedido, geracoes. A API
    // consulta esta funcao varias vezes por geracao.
    algorithm.setStoppingCriteria(
        [&elapsed, generations, max_time](const BRKGA::AlgorithmStatus& status) {
            return elapsed() >= static_cast<double>(max_time) ||
                   (generations > 0 && status.current_iteration >= generations);
        }
    );

    // Curva de convergencia: uma entrada a cada melhora da melhor solucao.
    // A API chama o observador em toda melhora, inclusive as encontradas pelo
    // IPR; entre duas melhoras a melhor solucao nao muda, entao estas entradas
    // reconstroem a curva exatamente.
    struct Improvement {
        unsigned generation;
        double time;
        double weight;
    };
    std::vector<Improvement> convergence;

    // ARMADILHA DA API: a documentacao diz que retornar true PARA a
    // otimizacao. O codigo faz "run &= callback(status)", logo e' false
    // que para. Retornamos false somente ao atingir o alvo.
    algorithm.addNewSolutionObserver(
        [&](const BRKGA::AlgorithmStatus& status) {
            convergence.push_back({
                static_cast<unsigned>(status.last_update_iteration),
                elapsed(),
                status.best_fitness
            });

            if(!quiet)
                std::cout << "* melhora: geracao " << status.current_iteration
                          << " | peso " << status.best_fitness
                          << " | " << elapsed() << " s\n";

            const bool reached = target > 0 && status.best_fitness <= target + 0.5;
            return !reached;
        }
    );

    ////////////////////////////////////////////////////////////////////////////
    // Execucao
    ////////////////////////////////////////////////////////////////////////////

    steady_start = SteadyClock::now();
    const BRKGA::AlgorithmStatus status =
        algorithm.run(control_params, quiet ? nullptr : &std::cout);
    const double total_time = elapsed();

    // Tempos medidos no relogio monotono (os da API usam o relogio de parede).
    const double last_update_time =
        convergence.empty() ? 0.0 : convergence.back().time;

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

    // Motivo da parada, na ordem em que os criterios sao testados.
    std::string stop_reason = "time";
    if(target > 0 && best.weight <= target)
        stop_reason = "target";
    else if(generations > 0 && status.current_iteration >= generations)
        stop_reason = "generations";
    else if(control_params.stall_offset > 0 &&
            status.stalled_iterations >= control_params.stall_offset)
        stop_reason = "stall";

    // Peso abaixo do alvo e' impossivel se o alvo for um otimo provado ou um
    // limite inferior valido: indica alvo errado ou solucao errada.
    const bool below_target = target > 0 && best.weight < target;
    if(below_target)
        std::cerr << "ERRO: peso " << best.weight << " ABAIXO do alvo "
                  << target << " -- alvo ou solucao incorretos\n";

    if(!convlog.empty()) {
        std::ofstream out(convlog);
        if(!out)
            throw std::runtime_error("nao foi possivel gravar " + convlog);
        out << std::fixed << std::setprecision(4)
            << "Instance,Algorithm,Seed,Event,Generation,Time,Weight\n";
        for(const auto& imp : convergence)
            out << instance_name(instance_file) << ',' << algorithm_name << ','
                << seed << ",improvement," << imp.generation << ','
                << imp.time << ',' << static_cast<unsigned>(imp.weight + 0.5)
                << '\n';
        // Linha final: estende a curva ate' o instante da parada.
        out << instance_name(instance_file) << ',' << algorithm_name << ','
            << seed << ",end," << status.current_iteration << ','
            << total_time << ',' << best.weight << '\n';
    }

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
              << total_time << ','
              << last_update_time << ','
              << status.largest_iteration_offset << ','
              << status.stalled_iterations << ','
              << status.path_relink_time.count() << ','
              << status.num_path_relink_calls << ','
              << status.num_homogenities << ','
              << status.num_best_improvements << ','
              << status.num_elite_improvements << ','
              << status.num_exchanges << ','
              << status.num_shakes << ','
              << status.num_resets << ','
              << target << ','
              << stop_reason << '\n';

    return (feasible && best.weight == api_weight && !below_target) ? 0 : 2;
}
catch(const std::exception& e) {
    std::cerr << "ERRO: " << e.what() << '\n';
    return 1;
}
