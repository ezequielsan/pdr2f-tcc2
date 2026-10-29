/*******************************************************************************
 * pdr2f_instance.hpp: instancia do Problema da Dominacao Romana 2-Forte.
 *
 * O grafo e' lido de um arquivo com a MATRIZ DE ADJACENCIA, uma linha por
 * vertice. O contrato de leitura e' o mesmo de read_simple_graph() do pli.jl
 * (Atilio Gomes Luiz, 09/10/2026), para que o PLI e o BRKGA operem sobre
 * exatamente o mesmo grafo:
 *
 *   - o numero de vertices e' o numero de linhas NAO VAZIAS;
 *   - qualquer valor diferente de zero representa uma aresta;
 *   - a DIAGONAL E' IGNORADA, mesmo quando contem valores nao nulos;
 *   - uma linha com numero de valores diferente de n e' erro;
 *   - vertices isolados (linha toda zero) sao preservados.
 *
 * As duas convencoes de diagonal presentes nas instancias deste projeto:
 *
 *   random, wireless-networks ........ diagonal 1.0, valores em ponto flutuante
 *   random_cubic, harwell-boeing,
 *   geoinstances ..................... diagonal 0,   valores inteiros
 *
 * Separadores aceitos: espaco, tabulacao e virgula. O retorno de carro do
 * Windows ('\r') e' tratado como separador, de modo que arquivos em CRLF sao
 * lidos corretamente.
 ******************************************************************************/

#ifndef PDR2F_INSTANCE_HPP_
#define PDR2F_INSTANCE_HPP_

#include <string>
#include <vector>

class PDR2F_Instance {
public:
    /// Le a matriz de adjacencia do arquivo. Lanca std::runtime_error se o
    /// arquivo nao abrir, estiver vazio ou nao for quadrado.
    explicit PDR2F_Instance(const std::string& filename);

    /// Numero de vertices, rotulados de 0 a num_nodes - 1.
    unsigned num_nodes;

    /// Numero de arestas (nao orientadas, sem a diagonal).
    unsigned num_edges;

    /// Listas de adjacencia, ordenadas em ordem crescente.
    std::vector<std::vector<unsigned>> adj;

    /// Vizinhos de v.
    const std::vector<unsigned>& neighbors(unsigned v) const { return adj[v]; }

    /// Grau de v. Um vertice isolado tem grau 0 e, no PDR2F, recebe
    /// obrigatoriamente rotulo 1.
    unsigned degree(unsigned v) const {
        return static_cast<unsigned>(adj[v].size());
    }
};

#endif // PDR2F_INSTANCE_HPP_
