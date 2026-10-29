#include "heuristics/greedy_pdr2f.hpp"

#include <algorithm>
#include <numeric>

PDR2F_Solution greedy_pdr2f(const PDR2F_Instance& instance, std::mt19937& rng)
{
    const unsigned n = instance.num_nodes;

    std::vector<unsigned> f(n, 0);
    std::vector<char> covered(n, 0);
    unsigned num_covered = 0;

    // A ordem embaralhada e' o mecanismo de desempate: entre vertices de
    // mesmo ganho, vence o primeiro desta ordem. E' o que o codigo original
    // faz ao percorrer shuffled_nodes antes de tomar o maximo.
    std::vector<unsigned> order(n);
    std::iota(order.begin(), order.end(), 0u);
    std::shuffle(order.begin(), order.end(), rng);

    while(num_covered < n) {
        unsigned best = n;        // n = "nenhum ainda"
        unsigned best_gain = 0;

        for(const unsigned v : order) {
            if(covered[v]) continue;

            // ganho = |N[v] \ coberto|, contando v, que esta' descoberto.
            unsigned gain = 1;
            for(const unsigned u : instance.adj[v])
                if(!covered[u]) ++gain;

            // Comparacao estrita: empate fica com quem apareceu primeiro na
            // ordem embaralhada.
            if(gain > best_gain) {
                best_gain = gain;
                best = v;
            }
        }

        // O laco acima sempre encontra alguem enquanto num_covered < n:
        // existe vertice descoberto, e seu ganho e' no minimo 1.
        f[best] = std::min(3u, best_gain);

        if(!covered[best]) { covered[best] = 1; ++num_covered; }
        for(const unsigned u : instance.adj[best])
            if(!covered[u]) { covered[u] = 1; ++num_covered; }
    }

    const unsigned total = std::accumulate(f.begin(), f.end(), 0u);
    return PDR2F_Solution{total, std::move(f)};
}
