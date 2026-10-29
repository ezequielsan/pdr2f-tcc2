/*******************************************************************************
 * pdr2f_threshold_decoder.hpp: decodificador por LIMIARES (quartis) de
 * Torres (2026), acrescido da busca local de reducao.
 *
 * Tres etapas:
 *
 *   1. QUARTIS. Cada chave decide o rotulo do seu vertice por faixas de
 *      amplitude igual: f(v) = min(floor(4 * key), 3).
 *
 *   2. REPARO. A rotulacao resultante costuma ser inviavel, e o reparo a
 *      conserta por duas regras (Torres, 2026, Algoritmo 5):
 *
 *        Regra 1: se f(v) = 0 e v nao tem protetor, promove v
 *                 -- a 3 se o grau de v e' maior que 3, a 1 caso contrario;
 *        Regra 2: se f(v) = 2 e v e' protetor exclusivo de dois ou mais
 *                 vizinhos de rotulo 0, promove v a 3.
 *
 *   3. REDUCAO. A passagem de reducao de pdr2f_reduction.hpp. Esta e' a
 *      contribuicao propria: o reparo do Torres so' aumenta rotulos, de modo
 *      que uma solucao cara mas valida permanece cara.
 *
 * DIVERGENCIA DOCUMENTADA EM RELACAO AO CODIGO DE TORRES. O codigo dele
 * escreve o teste da Regra 1 como len(G.nodes[node]) > 3, que mede o tamanho
 * do DICIONARIO DE ATRIBUTOS do vertice -- sempre 2 -- e nao o grau. Com isso
 * o ramo "promove a 3" nunca dispara no codigo dele, e todo vertice
 * desprotegido vira 1. Seguimos o PSEUDOCODIGO do TCC (grau maior que 3), que
 * e' o que o texto descreve. A diferenca e' relevante em grafos cubicos, onde
 * todo vertice tem grau exatamente 3 e o ramo tambem nao dispararia.
 *
 * POR QUE OS QUARTIS SAO UM PONTO DE PARTIDA RUIM. As chaves de um BRKGA sao
 * uniformes em [0,1], logo cada rotulo sai com probabilidade 1/4 e o peso
 * esperado de um cromossomo aleatorio e' 1,5n -- para QUALQUER grafo. Mas o
 * otimo varia muito com a estrutura: cerca de 0,67n num cubico, 0,8n num
 * caminho, e 3 numa estrela, independentemente de n. O decodificador comeca a
 * busca sistematicamente acima do alvo, e o reparo, que so' aumenta rotulos,
 * nao ajuda a descer. E' precisamente essa lacuna que a reducao preenche.
 *
 * CONSEQUENCIA PARA O IPR. Como as chaves atuam por limiar, a configuracao
 * correta e' pr_type = DIRECT com distancia customizada (Hamming sobre os 4
 * rotulos) e shaking_type = CHANGE.
 ******************************************************************************/

#ifndef PDR2F_THRESHOLD_DECODER_HPP_
#define PDR2F_THRESHOLD_DECODER_HPP_

#include "heuristics/greedy_pdr2f.hpp"   // PDR2F_Solution
#include "pdr2f/pdr2f_instance.hpp"

#include <vector>

/// Rotulo de uma chave: quatro faixas de amplitude igual.
/// A chave 1.0 cairia na faixa 4, por isso o min.
inline unsigned threshold_label(double key) {
    const unsigned q = static_cast<unsigned>(key * 4.0);
    return q < 3u ? q : 3u;
}

/// Reparo de Torres (2026, Algoritmo 5). Deixa f viavel.
void repair_threshold(const PDR2F_Instance& instance, std::vector<unsigned>& f);

/// Decodifica o vetor de chaves: quartis, reparo e, opcionalmente, reducao.
/// Nao modifica as chaves.
PDR2F_Solution decode_threshold(const PDR2F_Instance& instance,
                                const std::vector<double>& keys,
                                bool apply_reduction);

#endif // PDR2F_THRESHOLD_DECODER_HPP_
