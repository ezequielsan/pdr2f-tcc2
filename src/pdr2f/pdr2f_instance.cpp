#include "pdr2f/pdr2f_instance.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {

/// Espaco, tabulacao, virgula e o '\r' do CRLF separam valores.
inline bool is_separator(char c) {
    return c == ' ' || c == '\t' || c == ',' || c == '\r';
}

/// Um valor e' aresta se for diferente de zero. 
inline bool is_nonzero(const char* ini, const char* fim) {
    for(const char* p = ini; p != fim; ++p)
        if(*p >= '1' && *p <= '9') return true;
    return false;
}

}

PDR2F_Instance::PDR2F_Instance(const std::string& filename):
    num_nodes(0),
    num_edges(0),
    adj()
{
    std::ifstream file(filename, std::ios::binary);
    if(!file)
        throw std::runtime_error("nao foi possivel abrir " + filename);

    const std::string data(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
    );

    // As linhas nao vazias sao os vertices. Uma linha so' com separadores
    // conta como vazia.
    std::vector<std::pair<std::size_t, std::size_t>> lines;
    for(std::size_t i = 0; i < data.size(); ) {
        std::size_t j = data.find('\n', i);
        if(j == std::string::npos) j = data.size();

        bool empty = true;
        for(std::size_t k = i; k < j; ++k)
            if(!is_separator(data[k])) { empty = false; break; }

        if(!empty) lines.emplace_back(i, j);
        i = j + 1;
    }

    if(lines.empty())
        throw std::runtime_error("a matriz de adjacencia esta vazia: " + filename);

    num_nodes = static_cast<unsigned>(lines.size());
    adj.assign(num_nodes, std::vector<unsigned>());

    // Percorre apenas o triangulo SUPERIOR (col > v): isso ignora a diagonal
    // e evita inserir a mesma aresta duas vezes, exatamente como o pli.jl.
    // As colunas sao contadas ate' o fim da linha para validar se a matriz e'
    // quadrada.
    for(unsigned v = 0; v < num_nodes; ++v) {
        const char* p = data.data() + lines[v].first;
        const char* const end = data.data() + lines[v].second;
        unsigned col = 0;

        while(p != end) {
            while(p != end && is_separator(*p)) ++p;
            if(p == end) break;

            const char* const token = p;
            while(p != end && !is_separator(*p)) ++p;

            // A guarda col < num_nodes e' obrigatoria: se a linha tiver MAIS
            // de n valores, col passa do fim de adj e o acesso a adj[col]
            // seria fora dos limites. Aqui a contagem continua (para que a
            // mensagem de erro abaixo informe o total real), mas sem indexar.
            if(col < num_nodes && col > v && is_nonzero(token, p)) {
                adj[v].push_back(col);
                adj[col].push_back(v);
                ++num_edges;
            }
            ++col;
        }

        if(col != num_nodes)
            throw std::runtime_error(
                "a linha " + std::to_string(v + 1) + " de " + filename + " tem "
                + std::to_string(col) + " valores, mas eram esperados "
                + std::to_string(num_nodes) + " (a matriz deve ser n x n)"
            );
    }

    // Cada lista recebeu vizinhos da propria linha e das linhas anteriores.
    for(auto& list : adj)
        std::sort(list.begin(), list.end());
}
