/*******************************************************************************
 * test_pdr2f.cpp: testes do verificador de viabilidade e do guloso.
 *
 * Tres blocos de teste:
 *
 *   1. CASOS CONHECIDOS. Rotulacoes otimas da literatura (caminho, ciclo,
 *      estrela, grafo completo, grafo sem arestas) precisam ser aceitas, e
 *      rotulacoes que violam cada uma das duas condicoes precisam ser
 *      recusadas, com a condicao certa apontada.
 *
 *   2. EQUIVALENCIA COM A FORCA BRUTA. A caracterizacao local (O(n+m)) e'
 *      comparada com a definicao original do problema -- testar TODOS os
 *      pares de alvos -- em milhares de rotulacoes aleatorias. Este e' o
 *      teste central: toda a eficiencia do decodificador depende de a
 *      caracterizacao local ser mesmo equivalente.
 *
 *   3. GULOSO. O resultado do guloso precisa ser viavel em toda instancia e
 *      com toda semente, como a demonstracao em greedy_pdr2f.hpp afirma.
 *
 * Uso:
 *   ./test_pdr2f                          # so' os grafos sinteticos
 *   ./test_pdr2f instances/random/10_*.txt  # tambem instancias reais
 ******************************************************************************/

#include "heuristics/greedy_pdr2f.hpp"
#include "pdr2f/pdr2f_feasibility.hpp"
#include "pdr2f/pdr2f_instance.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

unsigned passed = 0;
unsigned failed = 0;

void report(bool ok, const std::string& name, const std::string& detail = "") {
    if(ok) {
        std::cout << "  ok    " << name << '\n';
        ++passed;
    } else {
        std::cout << "  FALHA " << name;
        if(!detail.empty()) std::cout << "  -> " << detail;
        std::cout << '\n';
        ++failed;
    }
}

/*******************************************************************************
 * Forca bruta: a definicao original do problema.
 *
 * Para cada par de alvos {a,b}, verifica se existe defesa. Como o ataque tem
 * 2 alvos, so' os de rotulo 0 precisam de socorro, e ha tres casos:
 *
 *   - nenhum alvo com rotulo 0: ambos se autodefendem;
 *   - um alvo x com rotulo 0: precisa de algum protetor, PN(x) != vazio;
 *   - os dois com rotulo 0: precisa de DOIS protetores distintos (um para
 *     cada), ou de um unico protetor comum com capacidade 2, isto e, rotulo 3.
 *
 * Note que um protetor atacado nao perde capacidade: com f(w) >= 2 ele guarda
 * uma legiao para si e ainda pode enviar f(w) - 1.
 ******************************************************************************/
bool feasible_bruteforce(const PDR2F_Instance& instance,
                         const std::vector<unsigned>& f)
{
    const unsigned n = instance.num_nodes;

    // PN(v) = vizinhos de v com rotulo >= 2.
    std::vector<std::vector<unsigned>> pn(n);
    for(unsigned v = 0; v < n; ++v)
        for(const unsigned u : instance.adj[v])
            if(f[u] >= 2) pn[v].push_back(u);

    for(unsigned a = 0; a < n; ++a) {
        for(unsigned b = a + 1; b < n; ++b) {
            const bool zero_a = (f[a] == 0);
            const bool zero_b = (f[b] == 0);

            if(!zero_a && !zero_b) continue;

            if(zero_a != zero_b) {
                const unsigned x = zero_a ? a : b;
                if(pn[x].empty()) return false;
                continue;
            }

            // Os dois alvos tem rotulo 0.
            if(pn[a].empty() || pn[b].empty()) return false;

            const bool same_single_protector =
                pn[a].size() == 1 && pn[b].size() == 1 && pn[a][0] == pn[b][0];

            if(same_single_protector && f[pn[a][0]] < 3) return false;
        }
    }
    return true;
}

/// Escreve uma matriz de adjacencia num arquivo temporario e devolve o caminho.
std::string write_matrix(const std::string& name,
                         const std::vector<std::vector<unsigned>>& m)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("pdr2f_test_" + name + ".txt");

    std::ofstream out(path);
    for(const auto& row : m) {
        for(std::size_t j = 0; j < row.size(); ++j)
            out << (j ? " " : "") << row[j];
        out << '\n';
    }
    return path.string();
}

/// Matriz de adjacencia a partir de uma lista de arestas.
std::vector<std::vector<unsigned>> from_edges(
        unsigned n, const std::vector<std::pair<unsigned, unsigned>>& edges)
{
    std::vector<std::vector<unsigned>> m(n, std::vector<unsigned>(n, 0));
    for(const auto& [u, v] : edges) { m[u][v] = 1; m[v][u] = 1; }
    return m;
}

/// Verifica a rotulacao e compara com o esperado (viavel ou a condicao ferida).
void expect(const PDR2F_Instance& instance, const std::vector<unsigned>& f,
            PDR2F_Violation::Kind expected, unsigned expected_weight,
            const std::string& name)
{
    const PDR2F_Violation got = check_feasibility(instance, f);
    const unsigned w = weight(f);

    if(got.kind != expected) {
        report(false, name,
               "esperada a violacao de tipo "
               + std::to_string(static_cast<int>(expected))
               + ", obtido: " + violation_text(got));
        return;
    }
    if(expected == PDR2F_Violation::NONE && w != expected_weight) {
        report(false, name, "peso esperado " + std::to_string(expected_weight)
                          + ", obtido " + std::to_string(w));
        return;
    }
    report(true, name);
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==============================================================\n"
                 " Testes do verificador de viabilidade e do guloso\n"
                 "==============================================================\n";

    /*-------------------------------------------------------------------------
     * 1. Casos conhecidos
     *-----------------------------------------------------------------------*/
    std::cout << "\n[1] Rotulacoes conhecidas\n";

    // Caminho P7: otimo = ceil(4*7/5) = 6, padrao 0 2 0 2 0 0 2.
    {
        const PDR2F_Instance p7(write_matrix("p7",
            from_edges(7, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6}})));
        expect(p7, {0,2,0,2,0,0,2}, PDR2F_Violation::NONE, 6,
               "P7: rotulacao otima 0 2 0 2 0 0 2 (peso 6)");
        expect(p7, {0,2,0,2,0,0,0}, PDR2F_Violation::UNPROTECTED_ZERO, 0,
               "P7: ultimo vertice sem protetor viola a Condicao 1");
        expect(p7, {1,1,1,1,1,1,1}, PDR2F_Violation::NONE, 7,
               "P7: solucao trivial com todos em 1 (peso 7)");
    }

    // Ciclo C5: otimo = 4. Um unico rotulo 2 e' insuficiente.
    {
        const PDR2F_Instance c5(write_matrix("c5",
            from_edges(5, {{0,1},{1,2},{2,3},{3,4},{4,0}})));
        expect(c5, {2,0,0,0,0}, PDR2F_Violation::UNPROTECTED_ZERO, 0,
               "C5: um unico rotulo 2 deixa vertices sem protetor");
        expect(c5, {2,0,2,0,0}, PDR2F_Violation::NONE, 4,
               "C5: rotulacao otima de peso 4");
    }

    // Estrela com centro 0 e 5 folhas: otimo = 3 (centro com rotulo 3).
    {
        const PDR2F_Instance star(write_matrix("star",
            from_edges(6, {{0,1},{0,2},{0,3},{0,4},{0,5}})));
        expect(star, {3,0,0,0,0,0}, PDR2F_Violation::NONE, 3,
               "Estrela: centro com rotulo 3 (peso 3, o otimo)");
        expect(star, {2,0,0,0,0,0}, PDR2F_Violation::OVERLOADED_TWO, 0,
               "Estrela: centro com rotulo 2 viola a Condicao 2");
        expect(star, {1,1,1,1,1,1}, PDR2F_Violation::NONE, 6,
               "Estrela: solucao trivial (peso 6, o dobro do otimo)");
    }

    // K4: um rotulo 3 cobre todos.
    {
        const PDR2F_Instance k4(write_matrix("k4",
            from_edges(4, {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}})));
        expect(k4, {3,0,0,0}, PDR2F_Violation::NONE, 3,
               "K4: um vertice com rotulo 3 (peso 3)");
        expect(k4, {2,0,0,0}, PDR2F_Violation::OVERLOADED_TWO, 0,
               "K4: um vertice com rotulo 2 viola a Condicao 2");
    }

    // Grafo sem arestas: todo vertice e' isolado e exige rotulo 1.
    {
        const PDR2F_Instance empty_graph(write_matrix("vazio",
            from_edges(4, {})));
        expect(empty_graph, {1,1,1,1}, PDR2F_Violation::NONE, 4,
               "Sem arestas: todos em 1 (peso = n, o otimo)");
        expect(empty_graph, {1,1,1,0}, PDR2F_Violation::UNPROTECTED_ZERO, 0,
               "Sem arestas: um isolado com rotulo 0 e' inviavel");
        expect(empty_graph, {3,3,3,3}, PDR2F_Violation::NONE, 12,
               "Sem arestas: rotulos altos sao viaveis, apenas caros");
    }

    // Rotulo invalido.
    {
        const PDR2F_Instance p3(write_matrix("p3", from_edges(3, {{0,1},{1,2}})));
        expect(p3, {0,4,0}, PDR2F_Violation::INVALID_LABEL, 0,
               "Rotulo 4 e' recusado");
        expect(p3, {0,2}, PDR2F_Violation::WRONG_SIZE, 0,
               "Vetor de rotulos com tamanho errado e' recusado");
    }

    /*-------------------------------------------------------------------------
     * 2. Caracterizacao local x forca bruta
     *-----------------------------------------------------------------------*/
    std::cout << "\n[2] Caracterizacao local O(n+m) x definicao por pares\n";

    std::vector<std::string> graphs;
    std::vector<std::string> names;

    graphs.push_back(write_matrix("eq_p10",
        from_edges(10, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,8},{8,9}})));
    names.push_back("caminho P10");

    graphs.push_back(write_matrix("eq_c9",
        from_edges(9, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,8},{8,0}})));
    names.push_back("ciclo C9");

    graphs.push_back(write_matrix("eq_star",
        from_edges(9, {{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8}})));
    names.push_back("estrela S9");

    {   // K6
        std::vector<std::pair<unsigned,unsigned>> e;
        for(unsigned i = 0; i < 6; ++i)
            for(unsigned j = i + 1; j < 6; ++j) e.emplace_back(i, j);
        graphs.push_back(write_matrix("eq_k6", from_edges(6, e)));
        names.push_back("completo K6");
    }

    {   // Petersen
        const std::vector<std::pair<unsigned,unsigned>> e = {
            {0,1},{1,2},{2,3},{3,4},{4,0},
            {5,7},{7,9},{9,6},{6,8},{8,5},
            {0,5},{1,6},{2,7},{3,8},{4,9}};
        graphs.push_back(write_matrix("eq_petersen", from_edges(10, e)));
        names.push_back("Petersen");
    }

    {   // Dois isolados grudados num caminho P4: mistura os dois regimes.
        graphs.push_back(write_matrix("eq_misto",
            from_edges(6, {{0,1},{1,2},{2,3}})));
        names.push_back("P4 mais 2 isolados");
    }

    // Instancias reais passadas na linha de comando.
    for(int i = 1; i < argc; ++i) {
        graphs.push_back(argv[i]);
        names.push_back(std::string("instancia ") + argv[i]);
    }

    std::mt19937 rng(12345);
    constexpr unsigned LABELINGS = 3000;
    unsigned long total_checks = 0;
    unsigned long total_feasible = 0;

    for(std::size_t g = 0; g < graphs.size(); ++g) {
        const PDR2F_Instance instance(graphs[g]);
        std::uniform_int_distribution<unsigned> label(0, 3);

        unsigned divergences = 0;
        unsigned feasible_here = 0;

        for(unsigned t = 0; t < LABELINGS; ++t) {
            std::vector<unsigned> f(instance.num_nodes);
            for(unsigned v = 0; v < instance.num_nodes; ++v) f[v] = label(rng);

            const bool local = is_feasible(instance, f);
            const bool brute = feasible_bruteforce(instance, f);

            if(local != brute) ++divergences;
            if(brute) ++feasible_here;
            ++total_checks;
        }
        total_feasible += feasible_here;

        report(divergences == 0,
               names[g] + " (n=" + std::to_string(instance.num_nodes) + ", "
               + std::to_string(LABELINGS) + " rotulacoes, "
               + std::to_string(feasible_here) + " viaveis)",
               std::to_string(divergences) + " divergencias");
    }

    std::cout << "      total: " << total_checks << " rotulacoes comparadas, "
              << total_feasible << " viaveis\n";

    /*-------------------------------------------------------------------------
     * 3. O guloso devolve solucao viavel
     *-----------------------------------------------------------------------*/
    std::cout << "\n[3] Viabilidade do guloso\n";

    for(std::size_t g = 0; g < graphs.size(); ++g) {
        const PDR2F_Instance instance(graphs[g]);

        unsigned infeasible = 0;
        unsigned best = 0, worst = 0;

        for(unsigned seed = 0; seed < 50; ++seed) {
            std::mt19937 local_rng(seed);
            const PDR2F_Solution solution = greedy_pdr2f(instance, local_rng);

            if(!is_feasible(instance, solution.labels)) ++infeasible;
            if(solution.weight != weight(solution.labels)) ++infeasible;

            if(seed == 0) { best = worst = solution.weight; }
            best = std::min(best, solution.weight);
            worst = std::max(worst, solution.weight);
        }

        report(infeasible == 0,
               names[g] + " (50 sementes, peso de " + std::to_string(best)
               + " a " + std::to_string(worst) + ")",
               std::to_string(infeasible) + " solucoes inviaveis");
    }

    std::cout << "--------------------------------------------------------------\n"
              << " passou: " << passed << "    falhou: " << failed << '\n'
              << "==============================================================\n";

    return failed == 0 ? 0 : 1;
}
