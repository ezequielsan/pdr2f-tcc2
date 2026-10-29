/*******************************************************************************
 * instance_info.cpp: le instancias e imprime suas caracteristicas em CSV.
 *
 * Serve a dois propositos:
 *   1. inspecionar uma instancia rapidamente;
 *   2. validar o leitor contra os CSVs do PLI, que registram |V|, |E|,
 *      density, maxDegree e minDegree para as mesmas instancias
 *      (ver scripts/valida_leitor.py).
 *
 * Uso:
 *   ./instance_info instances/random/100_1.txt
 *   ./instance_info instances/random/x.txt instances/random/y.txt > leitor.csv
 ******************************************************************************/

#include "pdr2f/pdr2f_instance.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <instancia> [<instancia> ...]\n";
        return 1;
    }

    std::cout << "Instance,NumNodes,NumEdges,Density,MaxDegree,MinDegree,Isolated\n";
    std::cout << std::setprecision(17);

    int failures = 0;

    for(int i = 1; i < argc; ++i) {
        const std::string path(argv[i]);

        // Nome da instancia: basename sem a extensao, como no CSV do PLI.
        const std::size_t slash = path.find_last_of("/\\");
        std::string name = (slash == std::string::npos)
                         ? path : path.substr(slash + 1);
        if(name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0)
            name.erase(name.size() - 4);

        try {
            const PDR2F_Instance instance(path);

            unsigned max_degree = 0;
            unsigned min_degree = instance.num_nodes > 0 ? instance.num_nodes : 0;
            unsigned isolated = 0;

            for(unsigned v = 0; v < instance.num_nodes; ++v) {
                const unsigned d = instance.degree(v);
                max_degree = std::max(max_degree, d);
                min_degree = std::min(min_degree, d);
                if(d == 0) ++isolated;
            }

            // Mesma definicao do PLI: 2m / (n(n-1)).
            const double n = instance.num_nodes;
            const double density = (instance.num_nodes > 1)
                ? (2.0 * instance.num_edges) / (n * (n - 1.0))
                : 0.0;

            std::cout << name << ','
                      << instance.num_nodes << ','
                      << instance.num_edges << ','
                      << density << ','
                      << max_degree << ','
                      << min_degree << ','
                      << isolated << '\n';
        }
        catch(const std::exception& e) {
            std::cerr << "ERRO em " << path << ": " << e.what() << '\n';
            ++failures;
        }
    }

    return failures == 0 ? 0 : 2;
}
