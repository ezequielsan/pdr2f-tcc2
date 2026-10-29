/*******************************************************************************
 * pdr2f_brkga_decoder.hpp: adaptador dos decodificadores para a API
 * BRKGA-MP-IPR.
 *
 * A API exige do decodificador um unico metodo:
 *
 *     BRKGA::fitness_t decode(BRKGA::Chromosome& chromosome, bool rewrite)
 *
 * Como BRKGA::Chromosome e' um alias de std::vector<double> e
 * BRKGA::fitness_t e' um alias de double, este arquivo NAO precisa incluir o
 * cabecalho da API. Isso importa: incluir brkga_mp_ipr.hpp em mais de um .cpp
 * gera simbolos duplicados na ligacao, de modo que ele e' incluido em um unico
 * lugar (apps/main_*.cpp).
 *
 * SOBRE O PARAMETRO rewrite. A API o passa como true apenas na decodificacao
 * final do melhor cromossomo, permitindo que o decodificador reescreva as
 * chaves com a solucao que de fato construiu. NOS IGNORAMOS rewrite de
 * proposito: Andrade et al. (2021, secao 3.1) alertam que esse "chromosome
 * adjustment" INVALIDA o IPR, e a nossa busca local de reducao o tornaria
 * especialmente agressivo. O decodificador e' puro: le as chaves e nao as
 * altera nunca.
 *
 * THREAD-SAFETY. decode() e' chamado em paralelo pelo OpenMP. Todo o estado
 * do objeto e' de leitura, e os vetores de trabalho sao locais a cada
 * chamada, portanto o metodo e' thread-safe.
 *
 * DETERMINISMO. A mesma chave produz sempre o mesmo rotulo, e o desempate na
 * ordenacao do decodificador por permutacao usa o indice do vertice. Logo o
 * mesmo cromossomo tem sempre o mesmo fitness, como a API exige.
 ******************************************************************************/

#ifndef PDR2F_BRKGA_DECODER_HPP_
#define PDR2F_BRKGA_DECODER_HPP_

#include "decoders/pdr2f_perm_decoder.hpp"
#include "decoders/pdr2f_threshold_decoder.hpp"
#include "pdr2f/pdr2f_instance.hpp"

#include <stdexcept>
#include <string>
#include <vector>

class PDR2F_BrkgaDecoder {
public:
    enum class Kind { THRESHOLD, PERMUTATION };

    static Kind kind_from_name(const std::string& name) {
        if(name == "threshold")   return Kind::THRESHOLD;
        if(name == "permutation") return Kind::PERMUTATION;
        throw std::runtime_error(
            "decodificador desconhecido: '" + name
            + "' (use 'threshold' ou 'permutation')");
    }

    static const char* name_of(Kind kind) {
        return kind == Kind::THRESHOLD ? "threshold" : "permutation";
    }

    PDR2F_BrkgaDecoder(const PDR2F_Instance& instance_reference,
                       Kind decoder_kind,
                       bool use_reduction):
        instance(instance_reference),
        kind(decoder_kind),
        reduction(use_reduction)
    {}

    /// Constroi a solucao completa (rotulos e peso) a partir das chaves.
    PDR2F_Solution solution(const std::vector<double>& chromosome) const {
        return kind == Kind::THRESHOLD
             ? decode_threshold(instance, chromosome, reduction)
             : decode_permutation(instance, chromosome, reduction);
    }

    /// A interface que a API consome. Devolve apenas o peso.
    double decode(std::vector<double>& chromosome, bool /* rewrite */) const {
        return static_cast<double>(solution(chromosome).weight);
    }

    Kind decoder_kind() const { return kind; }
    bool uses_reduction() const { return reduction; }

private:
    const PDR2F_Instance& instance;
    Kind kind;
    bool reduction;
};

#endif // PDR2F_BRKGA_DECODER_HPP_
