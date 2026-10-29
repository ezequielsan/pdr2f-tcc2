/*******************************************************************************
 * pdr2f_perm_decoder.hpp: decodificador por PERMUTACAO.
 *
 * Mudanca de filosofia em relacao ao decodificador por limiares: aqui a chave
 * nao diz o rotulo, a chave diz a ORDEM.
 *
 *   1. ordene os vertices pela chave;
 *   2. percorra nessa ordem; para cada vertice v ainda descoberto,
 *      f(v) <- min(3, |N[v] \ coberto|) e marque N[v] como coberto;
 *   3. (opcional) passagem de reducao.
 *
 * E' o guloso de Djukanovic et al. parametrizado pelo cromossomo: a estrutura
 * da decisao e' a mesma (cobrir o que falta), mas quem escolhe a ordem e' a
 * evolucao, e nao o ganho maximo.
 *
 * VIABILIDADE POR CONSTRUCAO. A demonstracao e' a mesma do guloso e nao
 * depende da ordem de percurso:
 *
 *   - um vertice termina com rotulo 0 apenas se nunca foi escolhido; quem o
 *     cobriu tinha ganho >= 2, logo rotulo >= 2, e o protege (Condicao 1);
 *   - um vertice com rotulo 2 tinha ganho exatamente 2 -- ele mesmo e um
 *     vizinho descoberto; qualquer outro vizinho de rotulo 0 ja' estava
 *     coberto por outro escolhido, de ganho >= 2, e tem protetor diferente
 *     (Condicao 2).
 *
 * Nenhum reparo e' necessario, e nenhuma regiao do hipercubo decodifica para
 * solucao invalida.
 *
 * CONSEQUENCIA PARA O IPR. Como as chaves induzem uma permutacao, infinitos
 * vetores de chaves geram a mesma ordem e, portanto, a mesma solucao. O IPR
 * direto (que troca blocos de chaves) rende pouco nesse regime; a configuracao
 * correta e' pr_type = PERMUTATION com distancia de Kendall tau e
 * shaking_type = SWAP. A escolha do decodificador e a do tipo de IPR sao uma
 * decisao so'.
 ******************************************************************************/

#ifndef PDR2F_PERM_DECODER_HPP_
#define PDR2F_PERM_DECODER_HPP_

#include "heuristics/greedy_pdr2f.hpp"   // PDR2F_Solution
#include "pdr2f/pdr2f_instance.hpp"

#include <vector>

/// Decodifica o vetor de chaves. Nao modifica as chaves.
/// Custo: O(n log n) para ordenar, mais O(n + m) para a varredura.
PDR2F_Solution decode_permutation(const PDR2F_Instance& instance,
                                  const std::vector<double>& keys,
                                  bool apply_reduction);

#endif // PDR2F_PERM_DECODER_HPP_
