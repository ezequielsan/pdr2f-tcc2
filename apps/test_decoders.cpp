/*******************************************************************************
 * test_decoders.cpp: testes dos dois decodificadores e da busca local.
 *
 * Quatro blocos:
 *
 *   1. REGRESSAO. Os dois exemplos trabalhados a mao em
 *      claude/melhorias_decodificador_explicado.md precisam sair exatamente
 *      como la': o caminho P7 em que a reducao leva o peso de 9 ao otimo 6, e
 *      a estrela em que o reparo do Torres gasta o dobro do otimo e a reducao
 *      NAO conserta -- limitacao conhecida, documentada como teste.
 *
 *   2. REDUCAO RAPIDA x REFERENCIA. A reducao usa verificacao incremental,
 *      reverificando apenas o que a mudanca pode ter quebrado. A
 *      implementacao de referencia chama a verificacao global O(n+m) a cada
 *      tentativa. As duas tem de produzir a MESMA rotulacao final. Este e' o
 *      teste central deste arquivo.
 *
 *   3. INVARIANTES. Os dois decodificadores sempre devolvem solucao viavel,
 *      com e sem reducao; a reducao nunca aumenta o peso; e a decodificacao e'
 *      deterministica.
 *
 *   4. REPARO. O reparo do Torres, isoladamente, deixa a rotulacao viavel.
 *
 * Uso:  ./test_decoders [<instancia> ...]
 ******************************************************************************/

#include "decoders/pdr2f_perm_decoder.hpp"
#include "decoders/pdr2f_threshold_decoder.hpp"
#include "pdr2f/pdr2f_feasibility.hpp"
#include "pdr2f/pdr2f_instance.hpp"
#include "pdr2f/pdr2f_reduction.hpp"

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
    if(ok) { std::cout << "  ok    " << name << '\n'; ++passed; }
    else {
        std::cout << "  FALHA " << name;
        if(!detail.empty()) std::cout << "  -> " << detail;
        std::cout << '\n';
        ++failed;
    }
}

std::string to_text(const std::vector<unsigned>& f) {
    std::string s;
    for(const unsigned x : f) { if(!s.empty()) s += ' '; s += char('0' + x); }
    return s;
}

std::string write_matrix(const std::string& name,
                         const std::vector<std::vector<unsigned>>& m)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("pdr2f_dec_" + name + ".txt");
    std::ofstream out(path);
    for(const auto& row : m) {
        for(std::size_t j = 0; j < row.size(); ++j)
            out << (j ? " " : "") << row[j];
        out << '\n';
    }
    return path.string();
}

std::vector<std::vector<unsigned>> from_edges(
        unsigned n, const std::vector<std::pair<unsigned, unsigned>>& edges)
{
    std::vector<std::vector<unsigned>> m(n, std::vector<unsigned>(n, 0));
    for(const auto& [u, v] : edges) { m[u][v] = 1; m[v][u] = 1; }
    return m;
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==============================================================\n"
                 " Testes dos decodificadores e da busca local de reducao\n"
                 "==============================================================\n";

    /*-------------------------------------------------------------------------
     * 1. Regressao dos exemplos trabalhados
     *-----------------------------------------------------------------------*/
    std::cout << "\n[1] Exemplos trabalhados a mao\n";

    {   // Caminho P7, otimo 6.
        const PDR2F_Instance p7(write_matrix("p7",
            from_edges(7, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6}})));
        const std::vector<double> keys =
            {0.31, 0.62, 0.11, 0.88, 0.40, 0.09, 0.55};

        const PDR2F_Solution sem = decode_threshold(p7, keys, false);
        report(sem.weight == 9 && to_text(sem.labels) == "1 2 0 3 1 0 2",
               "P7: quartis mais reparo dao 1 2 0 3 1 0 2, peso 9",
               "obtido " + to_text(sem.labels) + ", peso "
               + std::to_string(sem.weight));

        const PDR2F_Solution com = decode_threshold(p7, keys, true);
        report(com.weight == 6 && to_text(com.labels) == "0 2 0 2 0 0 2",
               "P7: com reducao dao 0 2 0 2 0 0 2, peso 6 (o otimo)",
               "obtido " + to_text(com.labels) + ", peso "
               + std::to_string(com.weight));
    }

    {   // Estrela: centro 0 e cinco folhas; otimo 3.
        const PDR2F_Instance star(write_matrix("star",
            from_edges(6, {{0,1},{0,2},{0,3},{0,4},{0,5}})));
        const std::vector<double> keys =
            {0.30, 0.10, 0.20, 0.15, 0.40, 0.05};

        const PDR2F_Solution sem = decode_threshold(star, keys, false);
        report(sem.weight == 6,
               "Estrela: reparo do Torres gasta 6, o dobro do otimo 3",
               "obtido " + to_text(sem.labels) + ", peso "
               + std::to_string(sem.weight));

        const PDR2F_Solution com = decode_threshold(star, keys, true);
        report(com.weight == 6,
               "Estrela: a reducao NAO conserta este caso (limitacao conhecida)",
               "obtido " + to_text(com.labels) + ", peso "
               + std::to_string(com.weight));

        // O decodificador por permutacao atinge o otimo SE a ordem colocar o
        // centro na frente: ali o ganho dele e' 6 e o rotulo vira 3. Com as
        // chaves acima o centro e' o quinto da ordem, e o resultado e' o
        // mesmo 6. A qualidade depende da ORDEM -- e e' exatamente a ordem
        // que a evolucao otimiza.
        const std::vector<double> center_first =
            {0.05, 0.30, 0.10, 0.20, 0.15, 0.40};
        const PDR2F_Solution bom = decode_permutation(star, center_first, true);
        report(bom.weight == 3,
               "Estrela: permutacao com o centro primeiro atinge o otimo 3",
               "obtido " + to_text(bom.labels) + ", peso "
               + std::to_string(bom.weight));

        const PDR2F_Solution ruim = decode_permutation(star, keys, true);
        report(ruim.weight == 6,
               "Estrela: permutacao com o centro em quinto gasta 6",
               "obtido " + to_text(ruim.labels) + ", peso "
               + std::to_string(ruim.weight));
    }

    /*-------------------------------------------------------------------------
     * 2. Reducao rapida x reducao de referencia
     *-----------------------------------------------------------------------*/
    std::cout << "\n[2] Reducao incremental x verificacao global (referencia)\n";

    std::vector<std::string> graphs;
    std::vector<std::string> names;

    graphs.push_back(write_matrix("p12", from_edges(12,
        {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,8},{8,9},{9,10},{10,11}})));
    names.push_back("caminho P12");

    graphs.push_back(write_matrix("c11", from_edges(11,
        {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,8},{8,9},{9,10},{10,0}})));
    names.push_back("ciclo C11");

    graphs.push_back(write_matrix("star12", from_edges(12,
        {{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,9},{0,10},{0,11}})));
    names.push_back("estrela S12");

    graphs.push_back(write_matrix("petersen", from_edges(10,
        {{0,1},{1,2},{2,3},{3,4},{4,0},{5,7},{7,9},{9,6},{6,8},{8,5},
         {0,5},{1,6},{2,7},{3,8},{4,9}})));
    names.push_back("Petersen");

    graphs.push_back(write_matrix("misto", from_edges(8, {{0,1},{1,2},{2,3}})));
    names.push_back("P4 mais 4 isolados");

    {   // K7
        std::vector<std::pair<unsigned,unsigned>> e;
        for(unsigned i = 0; i < 7; ++i)
            for(unsigned j = i + 1; j < 7; ++j) e.emplace_back(i, j);
        graphs.push_back(write_matrix("k7", from_edges(7, e)));
        names.push_back("completo K7");
    }

    for(int i = 1; i < argc; ++i) {
        graphs.push_back(argv[i]);
        names.push_back(std::string("instancia ") + argv[i]);
    }

    std::mt19937 rng(20261029);
    constexpr unsigned TRIALS = 2000;
    unsigned long compared = 0;

    for(std::size_t g = 0; g < graphs.size(); ++g) {
        const PDR2F_Instance instance(graphs[g]);
        std::uniform_real_distribution<double> key(0.0, 1.0);

        unsigned divergences = 0;
        unsigned long total_reduction = 0;

        for(unsigned t = 0; t < TRIALS; ++t) {
            std::vector<double> keys(instance.num_nodes);
            for(double& k : keys) k = key(rng);

            // Parte-se sempre de uma rotulacao VIAVEL, que e' a pre-condicao
            // da reducao. Alternam-se os dois decodificadores para cobrir
            // perfis diferentes de solucao de entrada.
            const PDR2F_Solution base = (t % 2 == 0)
                ? decode_threshold(instance, keys, false)
                : decode_permutation(instance, keys, false);

            std::vector<unsigned> fast = base.labels;
            std::vector<unsigned> slow = base.labels;

            const unsigned r_fast = reduce_pass(instance, fast);
            const unsigned r_slow = reduce_pass_reference(instance, slow);

            if(fast != slow || r_fast != r_slow) ++divergences;
            total_reduction += r_fast;
            ++compared;
        }

        report(divergences == 0,
               names[g] + " (n=" + std::to_string(instance.num_nodes) + ", "
               + std::to_string(TRIALS) + " rotulacoes, reducao media "
               + std::to_string(total_reduction / TRIALS) + ")",
               std::to_string(divergences) + " divergencias");
    }

    std::cout << "      total: " << compared << " rotulacoes reduzidas pelos"
                 " dois caminhos\n";

    /*-------------------------------------------------------------------------
     * 3. Invariantes dos decodificadores
     *-----------------------------------------------------------------------*/
    std::cout << "\n[3] Invariantes: viabilidade, monotonicidade, determinismo\n";

    for(std::size_t g = 0; g < graphs.size(); ++g) {
        const PDR2F_Instance instance(graphs[g]);
        std::uniform_real_distribution<double> key(0.0, 1.0);

        unsigned infeasible = 0, increased = 0, nondeterministic = 0;

        for(unsigned t = 0; t < 500; ++t) {
            std::vector<double> keys(instance.num_nodes);
            for(double& k : keys) k = key(rng);

            for(const bool reduction : {false, true}) {
                const PDR2F_Solution th = decode_threshold(instance, keys, reduction);
                const PDR2F_Solution pm = decode_permutation(instance, keys, reduction);

                if(!is_feasible(instance, th.labels)) ++infeasible;
                if(!is_feasible(instance, pm.labels)) ++infeasible;
                if(th.weight != weight(th.labels)) ++infeasible;
                if(pm.weight != weight(pm.labels)) ++infeasible;

                // Determinismo: decodificar de novo da' o mesmo.
                if(decode_threshold(instance, keys, reduction).labels != th.labels)
                    ++nondeterministic;
                if(decode_permutation(instance, keys, reduction).labels != pm.labels)
                    ++nondeterministic;
            }

            // A reducao nunca aumenta o peso.
            if(decode_threshold(instance, keys, true).weight
                 > decode_threshold(instance, keys, false).weight) ++increased;
            if(decode_permutation(instance, keys, true).weight
                 > decode_permutation(instance, keys, false).weight) ++increased;
        }

        report(infeasible == 0 && increased == 0 && nondeterministic == 0,
               names[g] + " (500 cromossomos x 2 decodificadores x 2 modos)",
               std::to_string(infeasible) + " inviaveis, "
               + std::to_string(increased) + " aumentos de peso, "
               + std::to_string(nondeterministic) + " nao deterministicos");
    }

    /*-------------------------------------------------------------------------
     * 4. O reparo do Torres, isoladamente, deixa a rotulacao viavel
     *-----------------------------------------------------------------------*/
    std::cout << "\n[4] O reparo de Torres deixa a rotulacao viavel\n";

    for(std::size_t g = 0; g < graphs.size(); ++g) {
        const PDR2F_Instance instance(graphs[g]);
        std::uniform_int_distribution<unsigned> label(0, 3);

        unsigned infeasible = 0;

        for(unsigned t = 0; t < 2000; ++t) {
            std::vector<unsigned> f(instance.num_nodes);
            for(unsigned v = 0; v < instance.num_nodes; ++v) f[v] = label(rng);

            repair_threshold(instance, f);
            if(!is_feasible(instance, f)) ++infeasible;
        }

        report(infeasible == 0,
               names[g] + " (2000 rotulacoes aleatorias reparadas)",
               std::to_string(infeasible) + " ainda inviaveis depois do reparo");
    }

    std::cout << "--------------------------------------------------------------\n"
              << " passou: " << passed << "    falhou: " << failed << '\n'
              << "==============================================================\n";

    return failed == 0 ? 0 : 1;
}
