#include "pdr2f/pdr2f_reduction.hpp"

#include "pdr2f/pdr2f_feasibility.hpp"

namespace {

/// Condicao 2 no vertice x, que se pressupoe ter rotulo 2: x pode ser
/// protetor exclusivo de no maximo um vizinho de rotulo 0.
bool condition2_ok(const PDR2F_Instance& instance,
                   const std::vector<unsigned>& f,
                   const std::vector<unsigned>& num_prot,
                   unsigned x)
{
    unsigned exclusive = 0;
    for(const unsigned w : instance.adj[x])
        if(f[w] == 0 && num_prot[w] == 1 && ++exclusive > 1)
            return false;
    return true;
}

/// Unico protetor de u, que se pressupoe ter num_prot[u] == 1.
unsigned sole_protector(const PDR2F_Instance& instance,
                        const std::vector<unsigned>& f,
                        unsigned u)
{
    for(const unsigned w : instance.adj[u])
        if(f[w] >= 2) return w;
    return instance.num_nodes;   // nao deveria acontecer
}

/// Tenta baixar f[v] em uma unidade. Devolve true se aceitou (e f ficou
/// alterado) ou false se desfez (e f voltou ao estado anterior).
bool try_decrement(const PDR2F_Instance& instance,
                   std::vector<unsigned>& f,
                   std::vector<unsigned>& num_prot,
                   unsigned v)
{
    const unsigned n = instance.num_nodes;

    // ---- 3 -> 2 : contagens inalteradas; so' a Condicao 2 em v -------------
    if(f[v] == 3) {
        f[v] = 2;
        if(!condition2_ok(instance, f, num_prot, v)) { f[v] = 3; return false; }
        return true;
    }

    // ---- 2 -> 1 : v deixa de ser protetor ---------------------------------
    if(f[v] == 2) {
        f[v] = 1;
        for(const unsigned u : instance.adj[v]) --num_prot[u];

        bool ok = true;

        // Condicao 1 nos vizinhos de rotulo 0 que perderam um protetor.
        for(const unsigned u : instance.adj[v]) {
            if(f[u] == 0 && num_prot[u] == 0) { ok = false; break; }
        }

        // Condicao 2 nos protetores que passaram a ser exclusivos.
        if(ok) {
            for(const unsigned u : instance.adj[v]) {
                if(f[u] != 0 || num_prot[u] != 1) continue;
                const unsigned x = sole_protector(instance, f, u);
                if(x < n && f[x] == 2
                         && !condition2_ok(instance, f, num_prot, x)) {
                    ok = false;
                    break;
                }
            }
        }

        if(!ok) {
            f[v] = 2;
            for(const unsigned u : instance.adj[v]) ++num_prot[u];
            return false;
        }
        return true;
    }

    // ---- 1 -> 0 : contagens inalteradas; v passa a precisar de protetor ---
    f[v] = 0;

    if(num_prot[v] == 0) { f[v] = 1; return false; }

    if(num_prot[v] == 1) {
        const unsigned x = sole_protector(instance, f, v);
        if(x < n && f[x] == 2 && !condition2_ok(instance, f, num_prot, x)) {
            f[v] = 1;
            return false;
        }
    }
    return true;
}

} // namespace

unsigned reduce_pass(const PDR2F_Instance& instance, std::vector<unsigned>& f)
{
    const unsigned n = instance.num_nodes;

    std::vector<unsigned> num_prot(n, 0);
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] < 2) continue;
        for(const unsigned u : instance.adj[v]) ++num_prot[u];
    }

    unsigned reductions = 0;

    // Ordem fixa. Dentro de cada vertice, desce o quanto puder.
    for(unsigned v = 0; v < n; ++v)
        while(f[v] > 0 && try_decrement(instance, f, num_prot, v))
            ++reductions;

    return reductions;
}

unsigned reduce_pass_reference(const PDR2F_Instance& instance,
                               std::vector<unsigned>& f)
{
    unsigned reductions = 0;

    for(unsigned v = 0; v < instance.num_nodes; ++v) {
        while(f[v] > 0) {
            --f[v];
            if(is_feasible(instance, f)) { ++reductions; continue; }
            ++f[v];
            break;
        }
    }
    return reductions;
}
