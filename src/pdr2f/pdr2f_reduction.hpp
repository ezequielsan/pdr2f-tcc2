/*******************************************************************************
 * pdr2f_reduction.hpp: busca local de reducao de pesos.
 *
 * Esta e' a contribuicao propria do trabalho sobre o decodificador por
 * limiares de Torres (2026). O reparo dele so' AUMENTA rotulos: nao existe
 * nenhum f[v] <- f[v] - 1 em lugar nenhum. Em consequencia, uma solucao cara
 * mas ja' valida permanece cara, e o decodificador por quartis entrega
 * solucoes caras por construcao (o peso esperado de um cromossomo aleatorio
 * e' 1,5n, qualquer que seja o grafo).
 *
 * A ideia, em uma frase: depois de reparar, percorra os vertices tentando
 * BAIXAR cada rotulo, e aceite a reducao sempre que a solucao continuar
 * viavel.
 *
 * Decisoes fixadas por medicao (ver claude/melhorias_decodificador_explicado.md):
 *
 *   - UMA passagem. Iterar ate' nada mais mudar da' a mesma qualidade
 *     (gap medio 1,7%) e custa 35% mais tempo.
 *   - ORDEM FIXA (0, 1, ..., n-1). Percorrer na ordem das chaves do
 *     cromossomo foi duas vezes pior (3,4% contra 1,7%), porque acopla a
 *     atribuicao de rotulos com a ordem de reducao.
 *   - As chaves do cromossomo NUNCA sao reescritas com o resultado. Andrade
 *     et al. (2021, secao 3.1) alertam que esse "chromosome adjustment"
 *     invalida o IPR.
 *
 * CUSTO. Verificar viabilidade do zero custa O(n + m); fazer isso a cada
 * tentativa de reducao daria O(n(n+m)) por decodificacao, inviavel. Em vez
 * disso mantemos o vetor de CONTAGEM DE PROTETORES de forma incremental e
 * reverificamos apenas o que a mudanca pode ter quebrado. Quais condicoes
 * cada tipo de reducao pode violar:
 *
 *   3 -> 2  As contagens de protetores nao mudam (3 e 2 sao ambos >= 2), logo
 *           a Condicao 1 continua valendo em todo lugar e nenhum outro rotulo
 *           2 muda de situacao. Basta checar a Condicao 2 em v.
 *
 *   2 -> 1  v deixa de ser protetor: a contagem cai para todos os vizinhos.
 *           Checa-se a Condicao 1 nos vizinhos de rotulo 0, e a Condicao 2
 *           nos protetores que passaram a ser exclusivos de algum deles.
 *
 *   1 -> 0  As contagens nao mudam (1 e 0 sao ambos < 2). v passa a precisar
 *           de protetor (Condicao 1 em v) e, se tiver exatamente um, esse
 *           protetor ganha um dependente exclusivo (Condicao 2 nele).
 *
 * A equivalencia entre essa verificacao incremental e a verificacao global e'
 * testada em apps/test_decoders.cpp, comparando o resultado com uma
 * implementacao de referencia que chama check_feasibility a cada tentativa.
 ******************************************************************************/

#ifndef PDR2F_REDUCTION_HPP_
#define PDR2F_REDUCTION_HPP_

#include "pdr2f/pdr2f_instance.hpp"

#include <vector>

/// Executa uma passagem de reducao sobre f, em ordem fixa, mantendo a
/// viabilidade. Pressupoe que f JA' E' VIAVEL na entrada; o comportamento com
/// entrada inviavel nao e' definido.
///
/// Devolve quantas reducoes de uma unidade foram aceitas, isto e', quanto o
/// peso diminuiu.
unsigned reduce_pass(const PDR2F_Instance& instance, std::vector<unsigned>& f);

/// Versao de REFERENCIA, lenta: tenta cada reducao e usa a verificacao global
/// O(n+m) para decidir. Existe para validar a versao rapida nos testes.
unsigned reduce_pass_reference(const PDR2F_Instance& instance,
                               std::vector<unsigned>& f);

#endif // PDR2F_REDUCTION_HPP_
