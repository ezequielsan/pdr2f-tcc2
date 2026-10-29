/*******************************************************************************
 * pdr2f_encoding.hpp: codificacao de uma solucao em chaves (warm start).
 *
 * Para semear a populacao inicial do BRKGA com solucoes gulosas, e' preciso
 * fazer o caminho INVERSO do decodificador: dada uma rotulacao, produzir um
 * cromossomo que a gere.
 *
 * CADA DECODIFICADOR PEDE UMA CODIFICACAO DIFERENTE, e as duas tem precisao
 * diferente:
 *
 *   LIMIARES -- EXATA. O rotulo e' min(floor(4*key), 3), portanto a chave
 *   (rotulo + r)/4, com r sorteado em [0,1), reproduz o rotulo exatamente. O
 *   ruido r e' deliberado: sem ele, todas as solucoes injetadas cairiam em
 *   quatro pontos do hipercubo, o que destruiria a diversidade e degeneraria
 *   as distancias usadas pelo IPR.
 *
 *   PERMUTACAO -- APROXIMADA. Ali a chave define apenas a ORDEM de percurso,
 *   e nao o rotulo; nao existe cromossomo que force uma rotulacao arbitraria.
 *   O que fazemos e' colocar os vertices que sao protetores na solucao
 *   (rotulo >= 2) na frente da ordem, para que a varredura gulosa os escolha
 *   primeiro. O resultado decodificado tende a ser proximo da solucao
 *   original, mas NAO e' garantidamente igual.
 *
 * Por isso o main_complete DECODIFICA o cromossomo injetado e reporta o peso
 * obtido (campo InitialWeight do CSV), em vez de supor que ele vale o mesmo
 * que a solucao de origem. O que a medicao mostra e' o que vai para a tabela.
 *
 * NOTA SOBRE O WARM START. No TCC 1, injetar muitas copias do guloso PIOROU
 * o resultado (media 8,15 contra 7,20 sem warm start), porque a elite enchia
 * de quase duplicatas e a diversidade colapsava. Daqui vem a decisao de o
 * numero de solucoes injetadas ser parametro, e de eliminar duplicatas antes
 * de injetar.
 ******************************************************************************/

#ifndef PDR2F_ENCODING_HPP_
#define PDR2F_ENCODING_HPP_

#include <random>
#include <vector>

/// Codifica uma rotulacao para o decodificador por limiares. Exata.
inline std::vector<double> encode_threshold(const std::vector<unsigned>& f,
                                            std::mt19937& rng)
{
    std::uniform_real_distribution<double> noise(0.0, 1.0);

    std::vector<double> keys(f.size());
    for(std::size_t v = 0; v < f.size(); ++v)
        keys[v] = (static_cast<double>(f[v]) + noise(rng)) / 4.0;

    return keys;
}

/// Codifica uma rotulacao para o decodificador por permutacao. Aproximada:
/// coloca os protetores (rotulo >= 2) na frente da ordem de percurso.
inline std::vector<double> encode_permutation(const std::vector<unsigned>& f,
                                              std::mt19937& rng)
{
    std::uniform_real_distribution<double> front(0.0, 0.5);
    std::uniform_real_distribution<double> back(0.5, 1.0);

    std::vector<double> keys(f.size());
    for(std::size_t v = 0; v < f.size(); ++v)
        keys[v] = (f[v] >= 2) ? front(rng) : back(rng);

    return keys;
}

#endif // PDR2F_ENCODING_HPP_
