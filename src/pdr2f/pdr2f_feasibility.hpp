/*******************************************************************************
 * pdr2f_feasibility.hpp: viabilidade de uma rotulacao no PDR2F.
 *
 * Uma rotulacao e' f: V -> {0,1,2,3}. Um vertice com f(v) >= 1 se defende
 * sozinho; um vertice com f(v) >= 2 pode socorrer no maximo f(v) - 1 vizinhos
 * atacados de rotulo 0. O inimigo ataca DOIS vertices simultaneamente, e a
 * rotulacao precisa resistir a QUALQUER par de alvos.
 *
 * Verificar isso pela definicao custaria O(n^2) pares. Usamos a
 * CARACTERIZACAO LOCAL, que e' equivalente e custa O(n + m).
 *
 * Chamando de protetores de v o conjunto PN(v) = { w em N(v) : f(w) >= 2 }:
 *
 *   Condicao 1.  Se f(v) = 0, entao PN(v) != vazio.
 *                (todo vertice vazio precisa de pelo menos um socorrista)
 *
 *   Condicao 2.  Se f(v) = 2, entao existe no maximo UM vizinho w com
 *                f(w) = 0 e PN(w) = {v}.
 *                (um vertice com exatamente 2 legioes nao pode ser o
 *                 socorrista exclusivo de dois vizinhos vazios: ele so' tem
 *                 capacidade 1, pois precisa guardar uma legiao para si)
 *
 * O rotulo 3 nao aparece na Condicao 2 porque sua capacidade e' 2, e o ataque
 * tem apenas 2 alvos: ele da' conta de qualquer par.
 *
 * Vertices isolados sao tratados corretamente e sem caso especial: um isolado
 * com rotulo 0 tem PN(v) vazio e viola a Condicao 1, de modo que todo isolado
 * recebe obrigatoriamente rotulo >= 1 em qualquer solucao viavel.
 *
 * A equivalencia entre a caracterizacao local e a definicao por pares e'
 * testada por forca bruta em apps/test_pdr2f.cpp.
 ******************************************************************************/

#ifndef PDR2F_FEASIBILITY_HPP_
#define PDR2F_FEASIBILITY_HPP_

#include "pdr2f/pdr2f_instance.hpp"

#include <string>
#include <vector>

/// Resultado da verificacao: ou nao ha violacao, ou ha uma, com o vertice
/// onde ela foi detectada.
struct PDR2F_Violation {
    enum Kind {
        NONE,              ///< rotulacao viavel
        WRONG_SIZE,        ///< o vetor de rotulos nao tem num_nodes posicoes
        INVALID_LABEL,     ///< algum rotulo fora de {0,1,2,3}
        UNPROTECTED_ZERO,  ///< Condicao 1: rotulo 0 sem nenhum protetor
        OVERLOADED_TWO     ///< Condicao 2: rotulo 2 protetor exclusivo de 2+
    };

    Kind kind;
    unsigned vertex;       ///< vertice onde a violacao ocorre (0 se kind=NONE)
};

/// Verifica a rotulacao em O(n + m) e devolve a primeira violacao encontrada.
PDR2F_Violation check_feasibility(const PDR2F_Instance& instance,
                                  const std::vector<unsigned>& f);

/// Atalho para quando so' interessa viavel ou nao.
inline bool is_feasible(const PDR2F_Instance& instance,
                        const std::vector<unsigned>& f) {
    return check_feasibility(instance, f).kind == PDR2F_Violation::NONE;
}

/// Peso da rotulacao, omega(f) = soma dos rotulos. E' a funcao objetivo.
unsigned weight(const std::vector<unsigned>& f);

/// Mensagem legivel para relatorios e mensagens de erro.
std::string violation_text(const PDR2F_Violation& violation);

#endif // PDR2F_FEASIBILITY_HPP_
