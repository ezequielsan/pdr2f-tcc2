#include "decoders/pdr2f_threshold_decoder.hpp"

#include "pdr2f/pdr2f_reduction.hpp"

#include <numeric>
#include <stdexcept>
#include <utility>

void repair_threshold(const PDR2F_Instance& instance, std::vector<unsigned>& f)
{
    const unsigned n = instance.num_nodes;

    // num_prot[v] = quantos vizinhos de v tem rotulo >= 2.
    std::vector<unsigned> num_prot(n, 0);
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] < 2) continue;
        for(const unsigned u : instance.adj[v]) ++num_prot[u];
    }

    // ---- Regra 1: rotulo 0 sem protetor ----------------------------------
    // A promocao a 3 propaga: os vizinhos de v ganham um protetor, o que pode
    // resolver de antemao outros vertices da varredura.
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] != 0 || num_prot[v] > 0) continue;

        if(instance.degree(v) > 3) {
            f[v] = 3;
            for(const unsigned u : instance.adj[v]) ++num_prot[u];
        } else {
            f[v] = 1;   // rotulo 1 nao protege ninguem; num_prot nao muda
        }
    }

    // ---- Regra 2: rotulo 2 sobrecarregado --------------------------------
    // A Regra 1 cria apenas rotulos 1 e 3, nunca 2, logo o conjunto dos
    // rotulos 2 e' o mesmo da decodificacao. E promover v de 2 para 3 nao
    // altera num_prot (ambos sao >= 2), de modo que uma unica varredura
    // basta: nenhuma promocao pode criar uma nova violacao da Regra 2.
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] != 2) continue;

        unsigned exclusive = 0;
        for(const unsigned w : instance.adj[v])
            if(f[w] == 0 && num_prot[w] == 1) ++exclusive;

        if(exclusive > 1) f[v] = 3;
    }
}

PDR2F_Solution decode_threshold(const PDR2F_Instance& instance,
                                const std::vector<double>& keys,
                                bool apply_reduction)
{
    const unsigned n = instance.num_nodes;

    if(keys.size() != n)
        throw std::runtime_error("o cromossomo deve ter num_nodes chaves");

    std::vector<unsigned> f(n);
    for(unsigned v = 0; v < n; ++v) f[v] = threshold_label(keys[v]);

    repair_threshold(instance, f);

    if(apply_reduction) reduce_pass(instance, f);

    const unsigned total = std::accumulate(f.begin(), f.end(), 0u);
    return PDR2F_Solution{total, std::move(f)};
}
