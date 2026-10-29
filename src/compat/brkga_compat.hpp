/*******************************************************************************
 * brkga_compat.hpp: compatibilidade da API BRKGA-MP-IPR com GCC anterior ao 13.
 *
 * O PROBLEMA. A API imprime duracoes de std::chrono em varios pontos (por
 * exemplo, ao escrever os ControlParams e o AlgorithmStatus). Para isso ela
 * conta com o operator<< de std::chrono::duration, que a libstdc++ passou a
 * fornecer apenas no GCC 13. A propria API traz um operator<< de reserva, mas
 * ele esta' dentro de um "#ifdef __clang__": com GCC, a API assume que a
 * biblioteca padrao oferece o operador.
 *
 * Em GCC 11 e 12, portanto, a API NAO COMPILA -- apenas incluir
 * brkga_mp_ipr.hpp produz dezenas de erros, mesmo num programa vazio.
 * Verificado em g++ 11.4 com -std=c++20: 72 erros.
 *
 * Nao e' so' questao de o operador faltar. Mesmo declarando-o no escopo
 * global, ele ficaria INVISIVEL de dentro do namespace BRKGA: a API inclui
 * third_part/enum_io.hpp dentro desse namespace, o que introduz outros
 * operator<< ali, e a busca de nome nao qualificada para no primeiro escopo
 * que tem o nome, sem seguir para o escopo global. E' exatamente por isso que
 * a API faz "using ::operator<<" dentro do namespace BRKGA -- mas so' para
 * clang.
 *
 * A SOLUCAO. Declaramos o operador no escopo global e o importamos para
 * dentro do namespace BRKGA, reproduzindo para o GCC o que a API ja' faz para
 * o clang. Nada da API e' modificado, o que preserva a licenca e a
 * procedencia dela.
 *
 * Em GCC 13 ou superior, e em clang, todo este arquivo vira nada: a condicao
 * de pre-processador o desliga, e a biblioteca padrao (ou a propria API)
 * cuida do assunto.
 *
 * ESTE CABECALHO PRECISA SER INCLUIDO ANTES DE brkga_mp_ipr.hpp, porque a
 * importacao para o namespace BRKGA tem de existir antes de a API usar o
 * operador.
 ******************************************************************************/

#ifndef BRKGA_COMPAT_HPP_
#define BRKGA_COMPAT_HPP_

#include <chrono>
#include <ostream>
#include <version>

#if !defined(__clang__) \
    && (!defined(__cpp_lib_chrono) || __cpp_lib_chrono < 201907L)

/// Mesmo formato que a API usa na sua versao para clang: o valor e um "s".
template <class Rep, class Period>
inline std::ostream& operator<<(std::ostream& os,
                                const std::chrono::duration<Rep, Period>& dur)
{
    os << dur.count() << "s";
    return os;
}

namespace BRKGA { using ::operator<<; }

#endif

#endif // BRKGA_COMPAT_HPP_
