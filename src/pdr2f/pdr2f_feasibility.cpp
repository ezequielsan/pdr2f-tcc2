#include "pdr2f/pdr2f_feasibility.hpp"

#include <numeric>

PDR2F_Violation check_feasibility(const PDR2F_Instance& instance,
                                  const std::vector<unsigned>& f)
{
    const unsigned n = instance.num_nodes;

    if(f.size() != n)
        return PDR2F_Violation{PDR2F_Violation::WRONG_SIZE, 0};

    for(unsigned v = 0; v < n; ++v)
        if(f[v] > 3)
            return PDR2F_Violation{PDR2F_Violation::INVALID_LABEL, v};

    // num_protectors[v] = |PN(v)| = quantos vizinhos de v tem rotulo >= 2.
    // Guardar a CONTAGEM, e nao o conjunto, basta: a Condicao 2 precisa saber
    // apenas se o protetor de w e' unico, e nesse caso, se v e' vizinho de w
    // com f(v) >= 2, o unico protetor de w e' necessariamente v.
    std::vector<unsigned> num_protectors(n, 0);
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] < 2) continue;
        for(const unsigned u : instance.adj[v])
            ++num_protectors[u];
    }

    // Condicao 1: todo rotulo 0 precisa de ao menos um protetor.
    for(unsigned v = 0; v < n; ++v)
        if(f[v] == 0 && num_protectors[v] == 0)
            return PDR2F_Violation{PDR2F_Violation::UNPROTECTED_ZERO, v};

    // Condicao 2: um rotulo 2 nao pode ser protetor exclusivo de dois
    // vizinhos de rotulo 0. Como f[v] = 2 >= 2, todo vizinho w com
    // num_protectors[w] == 1 tem v como seu unico protetor.
    for(unsigned v = 0; v < n; ++v) {
        if(f[v] != 2) continue;

        unsigned exclusive_dependents = 0;
        for(const unsigned w : instance.adj[v]) {
            if(f[w] == 0 && num_protectors[w] == 1) {
                if(++exclusive_dependents > 1)
                    return PDR2F_Violation{PDR2F_Violation::OVERLOADED_TWO, v};
            }
        }
    }

    return PDR2F_Violation{PDR2F_Violation::NONE, 0};
}

unsigned weight(const std::vector<unsigned>& f) {
    return std::accumulate(f.begin(), f.end(), 0u);
}

std::string violation_text(const PDR2F_Violation& violation) {
    const std::string v = std::to_string(violation.vertex);

    switch(violation.kind) {
    case PDR2F_Violation::NONE:
        return "viavel";
    case PDR2F_Violation::WRONG_SIZE:
        return "o vetor de rotulos nao tem num_nodes posicoes";
    case PDR2F_Violation::INVALID_LABEL:
        return "rotulo fora de {0,1,2,3} no vertice " + v;
    case PDR2F_Violation::UNPROTECTED_ZERO:
        return "Condicao 1 violada: o vertice " + v
             + " tem rotulo 0 e nenhum vizinho com rotulo >= 2";
    case PDR2F_Violation::OVERLOADED_TWO:
        return "Condicao 2 violada: o vertice " + v
             + " tem rotulo 2 e e' protetor exclusivo de dois ou mais vizinhos"
               " de rotulo 0";
    }
    return "violacao desconhecida";
}
