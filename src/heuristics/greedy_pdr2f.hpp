/*******************************************************************************
 * greedy_pdr2f.hpp: heuristica gulosa para o PDR2F.
 *
 * E' o guloso de Djukanovic, Kapunac, Kartelj e Matic (2025, secao 2.2), o
 * mesmo que o VNS usa como construcao inicial e que a PLI do Prof. Atilio usa
 * como warm start. Usar o mesmo guloso mantem o ponto de partida comparavel
 * entre as tres abordagens.
 *
 * A ideia: enquanto houver vertice nao coberto, escolha o vertice v que cobre
 * mais vertices ainda descobertos (contando ele mesmo), atribua
 * f(v) = min(3, ganho) e marque N[v] como coberto.
 *
 * O resultado e' VIAVEL por construcao, e isso se demonstra:
 *
 *   - Um vertice termina com rotulo 0 se, e somente se, nunca foi escolhido.
 *     Ele foi coberto por algum v escolhido com ganho >= 2 (se o ganho fosse
 *     1, v teria coberto apenas a si mesmo), logo f(v) >= 2 e v protege esse
 *     vertice: a Condicao 1 vale sempre.
 *
 *   - Se um vertice v tem f(v) = 2, seu ganho era exatamente 2: ele mesmo e
 *     UM vizinho descoberto u. Qualquer outro vizinho w de v com rotulo 0 ja'
 *     estava coberto por outro vertice escolhido, de ganho >= 2, e portanto
 *     tem protetor diferente de v. Logo v e' protetor exclusivo de no maximo
 *     um vizinho: a Condicao 2 vale sempre.
 *
 * Vertices isolados recebem ganho 1 e, com isso, rotulo 1 -- exatamente o que
 * o problema exige deles.
 *
 * O desempate entre vertices de mesmo ganho e' aleatorio, controlado pelo
 * gerador recebido por parametro, como no codigo original (que percorre os
 * vertices em ordem embaralhada). Por isso o guloso serve tambem para gerar
 * solucoes distintas para a populacao inicial do BRKGA.
 ******************************************************************************/

#ifndef GREEDY_PDR2F_HPP_
#define GREEDY_PDR2F_HPP_

#include "pdr2f/pdr2f_instance.hpp"

#include <random>
#include <vector>

struct PDR2F_Solution {
    unsigned weight;                 ///< omega(f) = soma dos rotulos
    std::vector<unsigned> labels;    ///< f(v) para cada vertice
};

/// Executa o guloso. O gerador e' usado apenas no desempate.
PDR2F_Solution greedy_pdr2f(const PDR2F_Instance& instance, std::mt19937& rng);

#endif // GREEDY_PDR2F_HPP_
