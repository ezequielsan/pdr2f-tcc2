/*******************************************************************************
 * greedy_info.cpp: roda o guloso em instancias e imprime os resultados em CSV.
 *
 * Para cada instancia, executa o guloso com varias sementes e reporta o melhor
 * peso, a media e o pior, mais a verificacao de viabilidade. A saida alimenta
 * scripts/valida_guloso.py, que compara com os otimos do PLI.
 *
 * Uso:
 *   ./greedy_info [--seeds <n>] <instancia> [<instancia> ...]
 *
 * Por que isto importa para o TCC: este e' o MESMO guloso que o VNS usa como
 * construcao inicial e que a PLI usa como warm start. O gap dele em relacao ao
 * otimo mede o ponto de partida comum das tres abordagens.
 ******************************************************************************/

#include "heuristics/greedy_pdr2f.hpp"
#include "pdr2f/pdr2f_feasibility.hpp"
#include "pdr2f/pdr2f_instance.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    unsigned seeds = 10;
    int first = 1;

    if(argc >= 3 && std::string(argv[1]) == "--seeds") {
        seeds = static_cast<unsigned>(std::atoi(argv[2]));
        first = 3;
    }
    if(first >= argc || seeds == 0) {
        std::cerr << "Uso: " << argv[0]
                  << " [--seeds <n>] <instancia> [<instancia> ...]\n";
        return 1;
    }

    std::cout << "Instance,NumNodes,NumEdges,Seeds,Best,Mean,Worst,Feasible,TimeMs\n"
              << std::fixed << std::setprecision(2);

    int failures = 0;

    for(int i = first; i < argc; ++i) {
        const std::string path(argv[i]);

        std::size_t slash = path.find_last_of("/\\");
        std::string name = (slash == std::string::npos)
                         ? path : path.substr(slash + 1);
        if(name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0)
            name.erase(name.size() - 4);

        try {
            const PDR2F_Instance instance(path);

            unsigned best = 0, worst = 0;
            double sum = 0.0;
            bool all_feasible = true;

            const auto start = std::chrono::steady_clock::now();

            for(unsigned seed = 0; seed < seeds; ++seed) {
                std::mt19937 rng(seed);
                const PDR2F_Solution solution = greedy_pdr2f(instance, rng);

                const PDR2F_Violation violation =
                    check_feasibility(instance, solution.labels);
                if(violation.kind != PDR2F_Violation::NONE) {
                    all_feasible = false;
                    std::cerr << "INVIAVEL em " << name << ", semente " << seed
                              << ": " << violation_text(violation) << '\n';
                }

                if(seed == 0) best = worst = solution.weight;
                best = std::min(best, solution.weight);
                worst = std::max(worst, solution.weight);
                sum += solution.weight;
            }

            const auto elapsed = std::chrono::steady_clock::now() - start;
            const double ms =
                std::chrono::duration<double, std::milli>(elapsed).count();

            std::cout << name << ','
                      << instance.num_nodes << ','
                      << instance.num_edges << ','
                      << seeds << ','
                      << best << ','
                      << (sum / seeds) << ','
                      << worst << ','
                      << (all_feasible ? "yes" : "NO") << ','
                      << ms << '\n';

            if(!all_feasible) ++failures;
        }
        catch(const std::exception& e) {
            std::cerr << "ERRO em " << path << ": " << e.what() << '\n';
            ++failures;
        }
    }

    return failures == 0 ? 0 : 2;
}
