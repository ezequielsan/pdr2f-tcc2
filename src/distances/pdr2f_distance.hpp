/*******************************************************************************
 * pdr2f_distance.hpp: funcao de distancia para o IPR com o decodificador por
 * limiares.
 *
 * POR QUE A DISTANCIA DE HAMMING DA API NAO SERVE. A religacao de caminhos
 * implicita (IPR) precisa medir a distancia entre dois cromossomos para
 * decidir se vale percorrer o caminho entre eles. A HammingDistance da API
 * "binariza" as chaves com um limiar (0,5 por padrao) e conta posicoes
 * diferentes. Com o nosso decodificador por quartis, isso agrupa os rotulos
 * {0,1} de um lado e {2,3} do outro: dois cromossomos que produzem rotulacoes
 * COMPLETAMENTE diferentes -- um todo em 0, o outro todo em 1 -- teriam
 * distancia ZERO, e o IPR os descartaria como se fossem iguais.
 *
 * A distancia correta e' a de Hamming sobre os QUATRO ROTULOS: conta em
 * quantos vertices as duas rotulacoes diferem.
 *
 * ATENCAO AO VALOR DE pr_minimum_distance. Esta distancia e' uma CONTAGEM
 * ABSOLUTA de vertices, nao uma fracao. Com pr_minimum_distance 3, exige-se
 * que os dois cromossomos difiram em pelo menos 3 vertices. Um valor
 * fracionario como 0,15 significaria, na pratica, "pelo menos 1 vertice".
 *
 * affectSolution E' O PONTO DE CONTATO ENTRE O IPR E O PROBLEMA. O IPR troca
 * blocos de chaves e chama o decodificador a cada troca. A funcao
 * affectSolution responde antes: "trocar este bloco pode mudar a solucao?".
 * Se nao, o IPR pula a troca e economiza uma decodificacao inteira. Com
 * limiares a resposta e' barata: basta comparar o quartil de cada chave.
 *
 * Uma observacao de honestidade: com a busca local de reducao ativa, o mapa
 * cromossomo -> solucao fica ainda mais muitos-para-um, de modo que dois
 * blocos podem cair em quartis diferentes e AINDA ASSIM produzir a mesma
 * solucao final. Nossa affectSolution e', portanto, CONSERVADORA: as vezes
 * diz "sim, afeta" quando nao afeta. Isso e' seguro -- apenas perde um pouco
 * de eficiencia. O erro perigoso seria o contrario.
 *
 * O decodificador por permutacao NAO usa esta classe: para ele a configuracao
 * correta e' pr_distance_function_type KENDALLTAU, que a propria API fornece.
 *
 * Este cabecalho inclui a API, por causa da classe base, e por isso deve ser
 * incluido por um unico .cpp (o mesmo que inclui brkga_mp_ipr.hpp).
 ******************************************************************************/

#ifndef PDR2F_DISTANCE_HPP_
#define PDR2F_DISTANCE_HPP_

#include "decoders/pdr2f_threshold_decoder.hpp"   // threshold_label

// O compat PRECISA vir antes da API: ver src/compat/brkga_compat.hpp.
#include "compat/brkga_compat.hpp"
#include "brkga_mp_ipr.hpp"

#include <cstddef>

class PDR2F_ThresholdDistance: public BRKGA::DistanceFunctionBase {
public:
    PDR2F_ThresholdDistance() = default;
    ~PDR2F_ThresholdDistance() override = default;

    /// Numero de vertices em que as duas rotulacoes diferem.
    double distance(const BRKGA::Chromosome& v1,
                    const BRKGA::Chromosome& v2) override
    {
        const std::size_t n = v1.size() < v2.size() ? v1.size() : v2.size();

        unsigned different = 0;
        for(std::size_t i = 0; i < n; ++i)
            if(threshold_label(v1[i]) != threshold_label(v2[i])) ++different;

        return static_cast<double>(different);
    }

    /// Trocar key1 por key2 muda o rotulo daquele vertice?
    bool affectSolution(const BRKGA::Chromosome::value_type key1,
                        const BRKGA::Chromosome::value_type key2) override
    {
        return threshold_label(key1) != threshold_label(key2);
    }

    /// Versao em bloco: basta uma chave do bloco mudar de quartil.
    bool affectSolution(BRKGA::Chromosome::const_iterator v1_begin,
                        BRKGA::Chromosome::const_iterator v2_begin,
                        const std::size_t block_size) override
    {
        for(std::size_t i = 0; i < block_size; ++i, ++v1_begin, ++v2_begin)
            if(threshold_label(*v1_begin) != threshold_label(*v2_begin))
                return true;
        return false;
    }
};

#endif // PDR2F_DISTANCE_HPP_
