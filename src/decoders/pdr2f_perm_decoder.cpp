#include "decoders/pdr2f_perm_decoder.hpp"

#include "pdr2f/pdr2f_reduction.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

PDR2F_Solution decode_permutation(const PDR2F_Instance& instance,
                                  const std::vector<double>& keys,
                                  bool apply_reduction)
{
    const unsigned n = instance.num_nodes;

    if(keys.size() != n)
        throw std::runtime_error("o cromossomo deve ter num_nodes chaves");

    // A chave define a ordem. O indice do vertice entra como segundo criterio
    // para que chaves iguais produzam sempre a mesma ordem: o decodificador
    // precisa ser deterministico, ou o mesmo individuo teria fitness
    // diferente em geracoes diferentes.
    std::vector<std::pair<double, unsigned>> order(n);
    for(unsigned v = 0; v < n; ++v) order[v] = {keys[v], v};
    std::sort(order.begin(), order.end());

    std::vector<unsigned> f(n, 0);
    std::vector<char> covered(n, 0);

    for(const auto& [key, v] : order) {
        (void) key;
        if(covered[v]) continue;

        // ganho = |N[v] \ coberto|, contando v, que esta' descoberto.
        unsigned gain = 1;
        for(const unsigned u : instance.adj[v])
            if(!covered[u]) ++gain;

        f[v] = std::min(3u, gain);

        covered[v] = 1;
        for(const unsigned u : instance.adj[v]) covered[u] = 1;
    }

    if(apply_reduction) reduce_pass(instance, f);

    const unsigned total = std::accumulate(f.begin(), f.end(), 0u);
    return PDR2F_Solution{total, std::move(f)};
}
