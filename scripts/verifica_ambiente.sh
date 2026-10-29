#!/usr/bin/env bash
###############################################################################
# verifica_ambiente.sh: confere se a maquina tem o necessario para compilar e
# rodar o projeto.
#
# Uso:  ./scripts/verifica_ambiente.sh
#
# Sai com codigo 0 se o essencial esta' presente, 1 se algo obrigatorio falta.
# O que e' opcional (Julia, R, irace) nao faz o script falhar: e' necessario
# apenas para o PLI e para a calibracao.
###############################################################################

set -u

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

faltando=0
avisos=0

titulo() { printf '\n== %s ==\n' "$1"; }
ok()     { printf '  [ok]     %s\n' "$1"; }
falta()  { printf '  [FALTA]  %s\n' "$1"; faltando=$((faltando + 1)); }
aviso()  { printf '  [aviso]  %s\n' "$1"; avisos=$((avisos + 1)); }

echo "=============================================================="
echo " Verificacao de ambiente -- PDR2F / TCC 2"
echo "=============================================================="

###############################################################################
titulo "Ferramentas obrigatorias"
###############################################################################

if command -v g++ > /dev/null; then
    versao=$(g++ -dumpfullversion -dumpversion 2>/dev/null)
    maior=${versao%%.*}
    ok "g++ $versao"

    if [ "$maior" -ge 13 ]; then
        ok "  GCC >= 13: a API BRKGA-MP-IPR compila sem contorno"
    elif [ "$maior" -ge 11 ]; then
        aviso "  GCC $maior: a API SO' compila por causa de src/compat/brkga_compat.hpp"
        aviso "  (a API conta com o operator<< de std::chrono::duration, que a"
        aviso "   libstdc++ fornece a partir do GCC 13). Nao remova o compat."
    else
        falta "  GCC $maior e' antigo demais: a API exige C++20 (GCC >= 11)"
    fi
else
    falta "g++  ->  sudo apt update && sudo apt install build-essential"
fi

for cmd in make git python3; do
    if command -v "$cmd" > /dev/null; then
        ok "$cmd $("$cmd" --version 2>/dev/null | head -1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -1)"
    else
        case $cmd in
            make)    falta "make     ->  sudo apt install build-essential" ;;
            git)     falta "git      ->  sudo apt install git" ;;
            python3) falta "python3  ->  sudo apt install python3" ;;
        esac
    fi
done

###############################################################################
titulo "Compilacao de verdade: C++20, OpenMP e chrono"
###############################################################################

cat > "$TMP/probe.cpp" <<'EOF'
#include <omp.h>
#include <version>
#include <iostream>
#include <vector>
#include <ranges>
int main() {
    std::vector<int> v{3, 1, 2};
    auto pares = v | std::views::filter([](int x) { return x % 2 == 0; });
    int n = 0;
    for(const int x : pares) n += x;

    std::cout << "threads=" << omp_get_max_threads()
#ifdef __cpp_lib_chrono
              << " chrono=" << __cpp_lib_chrono
#else
              << " chrono=ausente"
#endif
              << " ranges=" << n << '\n';
    return 0;
}
EOF

if command -v g++ > /dev/null; then
    if g++ -std=c++20 -fopenmp "$TMP/probe.cpp" -o "$TMP/probe" 2> "$TMP/erro"; then
        saida=$("$TMP/probe")
        ok "compilou com -std=c++20 -fopenmp"
        ok "  $saida"

        chrono=${saida##*chrono=}; chrono=${chrono%% *}
        if [ "$chrono" = "ausente" ] || [ "$chrono" -lt 201907 ] 2>/dev/null; then
            aviso "  sem operator<< de std::chrono::duration na biblioteca padrao;"
            aviso "  o src/compat/brkga_compat.hpp e' obrigatorio aqui"
        else
            ok "  a biblioteca padrao fornece o operator<< de chrono"
        fi
    else
        falta "a compilacao de teste falhou:"
        sed 's/^/           /' "$TMP/erro" | head -5
        falta "  provavel solucao: sudo apt install build-essential"
    fi
fi

###############################################################################
titulo "API BRKGA-MP-IPR"
###############################################################################

API="$RAIZ/external/brkga_mp_ipr_cpp"
if [ -f "$API/brkga_mp_ipr/brkga_mp_ipr.hpp" ]; then
    tag=$(git -C "$API" describe --tags 2>/dev/null || echo "sem tag")
    ok "encontrada em external/brkga_mp_ipr_cpp ($tag)"
    [ "$tag" = "v3.1.1" ] || aviso "  o projeto foi desenvolvido com a tag v3.1.1"
else
    falta "API ausente  ->  git clone --depth 1 --branch v3.1.1 \\"
    falta "    https://github.com/ceandrade/brkga_mp_ipr_cpp external/brkga_mp_ipr_cpp"
fi

###############################################################################
titulo "Instancias e referencias"
###############################################################################

n_inst=$(find "$RAIZ/instances" -name '*.txt' 2>/dev/null | wc -l)
n_pli=$(find "$RAIZ/pli/integer_linear_program/output" -name '*.csv' 2>/dev/null | wc -l)

[ "$n_inst" -eq 377 ] && ok "377 instancias em instances/" \
    || aviso "instances/ tem $n_inst arquivos .txt (o esperado e' 377)"
[ "$n_pli" -eq 377 ] && ok "377 CSVs de resultado do PLI" \
    || aviso "pli/.../output tem $n_pli CSVs (o esperado e' 377)"

###############################################################################
titulo "Opcional: PLI (Julia) e calibracao (R + irace)"
###############################################################################

if command -v julia > /dev/null; then
    ok "julia $(julia --version | grep -oE '[0-9]+(\.[0-9]+)+')"
    ok "  pacotes necessarios: JuMP, CPLEX, Graphs, DelimitedFiles, MathOptInterface"
else
    aviso "julia ausente: so' necessario para REEXECUTAR o PLI."
    aviso "  Nao e' preciso: os 377 resultados ja' estao em pli/.../output/"
fi

if command -v R > /dev/null; then
    ok "R $(R --version | head -1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -1)"
    if R -e 'library(irace)' > /dev/null 2>&1; then
        ok "  pacote irace instalado"
    else
        aviso "  pacote irace ausente  ->  R -e 'install.packages(\"irace\")'"
    fi
else
    aviso "R ausente: necessario para a calibracao com irace (etapa futura)"
    aviso "  ->  sudo apt install r-base"
fi

###############################################################################
titulo "Maquina e local do repositorio"
###############################################################################

nucleos=$(nproc 2>/dev/null || echo "?")
ok "nucleos disponiveis: $nucleos"
ok "memoria: $(free -h 2>/dev/null | awk '/^Mem:/{print $2}' || echo '?')"

if [ "$nucleos" != "?" ] && [ "$nucleos" -lt 4 ]; then
    aviso "com menos de 4 nucleos as baterias de experimento vao demorar bastante"
fi

case "$RAIZ" in
    /mnt/[a-z]/*)
        aviso "o repositorio esta' em $RAIZ, isto e', no disco do Windows."
        aviso "  Ler arquivos grandes por /mnt/c passa pelo drvfs e e' lento."
        aviso "  Para DESENVOLVER, tudo bem. Para as BATERIAS com limite de"
        aviso "  tempo, prefira um clone no sistema de arquivos do Linux"
        aviso "  (por exemplo ~/pdr2f-tcc2), senao parte do orcamento de tempo"
        aviso "  e' gasto em leitura de arquivo."
        ;;
    *)
        ok "o repositorio esta' no sistema de arquivos do Linux"
        ;;
esac

###############################################################################
echo ""
echo "=============================================================="
if [ "$faltando" -eq 0 ]; then
    echo " ESSENCIAL COMPLETO. Avisos: $avisos"
    echo ""
    echo " Teste final, que exercita tudo de uma vez:"
    echo "   make clean && make && make test"
    echo "   ./main_minimal 0 configs/brkga-mp/dev_threshold.conf 15 \\"
    echo "                  instances/random/100_1.txt threshold 200"
    echo "=============================================================="
    exit 0
else
    echo " FALTAM $faltando item(ns) obrigatorio(s). Avisos: $avisos"
    echo "=============================================================="
    exit 1
fi
