/*******************************************************************************
 * decoder_info.cpp: compara os decodificadores com cromossomos ALEATORIOS.
 *
 * Para cada instancia e cada semente, gera um cromossomo uniforme em [0,1] e
 * decodifica das quatro formas, imprimindo o melhor peso de cada uma:
 *
 *   threshold        quartis mais reparo de Torres (o decodificador do TCC 1)
 *   threshold_red    o mesmo, mais a busca local de reducao
 *   perm             permutacao
 *   perm_red         permutacao mais reducao
 *
 * Isto NAO e' o BRKGA: nao ha evolucao nenhuma, apenas cromossomos sorteados.
 * Serve para isolar o efeito do decodificador do efeito da meta-heuristica, o
 * que responde a pergunta central do trabalho -- quanto do ganho vem do
 * decodificador e quanto vem dos multiplos pais e da religacao de caminhos.
 *
 * Uso:
 *   ./decoder_info [--seeds <n>] <instancia> [<instancia> ...]
 ******************************************************************************/

#include "decoders/pdr2f_perm_decoder.hpp"
#include "decoders/pdr2f_threshold_decoder.hpp"
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

namespace {

struct Stats {
    unsigned best;
    double sum;
    unsigned infeasible;
};

void update(Stats& s, const PDR2F_Instance& instance,
            const PDR2F_Solution& solution, bool first)
{
    if(first || solution.weight < s.best) s.best = solution.weight;
    s.sum += solution.weight;
    if(!is_feasible(instance, solution.labels)) ++s.infeasible;
}

} // namespace

int main(int argc, char* argv[]) {
    unsigned seeds = 10;
    int first_arg = 1;

    if(argc >= 3 && std::string(argv[1]) == "--seeds") {
        seeds = static_cast<unsigned>(std::atoi(argv[2]));
        first_arg = 3;
    }
    if(first_arg >= argc || seeds == 0) {
        std::cerr << "Uso: " << argv[0]
                  << " [--seeds <n>] <instancia> [<instancia> ...]\n";
        return 1;
    }

    std::cout << "Instance,NumNodes,NumEdges,Seeds,"
                 "ThrBest,ThrMean,ThrRedBest,ThrRedMean,"
                 "PermBest,PermMean,PermRedBest,PermRedMean,"
                 "Infeasible,TimeMs\n"
              << std::fixed << std::setprecision(2);

    int failures = 0;

    for(int i = first_arg; i < argc; ++i) {
        const std::string path(argv[i]);

        const std::size_t slash = path.find_last_of("/\\");
        std::string name = (slash == std::string::npos)
                         ? path : path.substr(slash + 1);
        if(name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0)
            name.erase(name.size() - 4);

        try {
            const PDR2F_Instance instance(path);

            Stats thr{0, 0.0, 0}, thr_red{0, 0.0, 0};
            Stats perm{0, 0.0, 0}, perm_red{0, 0.0, 0};

            const auto start = std::chrono::steady_clock::now();

            for(unsigned seed = 0; seed < seeds; ++seed) {
                std::mt19937 rng(seed);
                std::uniform_real_distribution<double> key(0.0, 1.0);

                std::vector<double> keys(instance.num_nodes);
                for(double& k : keys) k = key(rng);

                const bool is_first = (seed == 0);
                update(thr,      instance, decode_threshold(instance, keys, false),   is_first);
                update(thr_red,  instance, decode_threshold(instance, keys, true),    is_first);
                update(perm,     instance, decode_permutation(instance, keys, false), is_first);
                update(perm_red, instance, decode_permutation(instance, keys, true),  is_first);
            }

            const auto elapsed = std::chrono::steady_clock::now() - start;
            const double ms =
                std::chrono::duration<double, std::milli>(elapsed).count();

            const unsigned bad = thr.infeasible + thr_red.infeasible
                               + perm.infeasible + perm_red.infeasible;

            std::cout << name << ','
                      << instance.num_nodes << ',' << instance.num_edges << ','
                      << seeds << ','
                      << thr.best      << ',' << (thr.sum / seeds)      << ','
                      << thr_red.best  << ',' << (thr_red.sum / seeds)  << ','
                      << perm.best     << ',' << (perm.sum / seeds)     << ','
                      << perm_red.best << ',' << (perm_red.sum / seeds) << ','
                      << bad << ',' << ms << '\n';

            if(bad > 0) {
                std::cerr << "INVIAVEL em " << name << ": " << bad
                          << " solucoes\n";
                ++failures;
            }
        }
        catch(const std::exception& e) {
            std::cerr << "ERRO em " << path << ": " << e.what() << '\n';
            ++failures;
        }
    }

    return failures == 0 ? 0 : 2;
}
