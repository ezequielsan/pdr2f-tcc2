#!/usr/bin/env bash
###############################################################################
# roda_irace.sh: calibracao do BRKGA-MP e do BRKGA-MP-IPR com o irace.
#
# Duas calibracoes independentes, no mesmo cenario (scenario.txt), cada uma com
# seus parametros (parametros-<braco>.txt) e sua base (base-<braco>.conf).
#
# Uso:
#   ./irace/roda_irace.sh --check            valida os arquivos dos dois bracos
#   ./irace/roda_irace.sh --teste [braco]    fumaca: 300 execucoes de 2 s
#   ./irace/roda_irace.sh [braco]            calibracao completa
#
#   braco = ipr | mp | ambos (padrao: ambos, primeiro ipr e depois mp)
#
# Cada calibracao grava em irace/exec-<braco>/: o log do irace, o irace.Rdata
# (todos os dados da calibracao) e execucoes.csv (uma linha por execucao).
# Com "ambos", a segunda comeca sozinha quando a primeira termina sem erro.
###############################################################################

set -euo pipefail
cd "$(dirname "$0")"

command -v irace > /dev/null || { echo "ERRO: irace nao esta no PATH (ver irace/README.md)." >&2; exit 1; }
[ -x ../main_complete ]                  || { echo "ERRO: ../main_complete nao existe. Rode 'make main_complete' na raiz." >&2; exit 1; }
[ -s treino.txt ]                        || { echo "ERRO: treino.txt ausente. Rode scripts/sorteia_treino.py." >&2; exit 1; }
[ -s ../results/baseline/alvos.csv ]     || { echo "ERRO: alvos.csv ausente. Rode scripts/gera_alvos.py." >&2; exit 1; }

bracos() {
    case "${1:-ambos}" in
        ipr|mp) echo "$1" ;;
        ambos)  echo "ipr mp" ;;
        *)      echo "ERRO: braco invalido: $1 (use ipr, mp ou ambos)" >&2; exit 1 ;;
    esac
}

# Variaveis que o target-runner le.
prepara() {
    export IRACE_BRACO="$1"
    export IRACE_BASE="$PWD/base-$1.conf"
    [ -f "$IRACE_BASE" ]          || { echo "ERRO: $IRACE_BASE ausente" >&2; exit 1; }
    [ -f "parametros-$1.txt" ]    || { echo "ERRO: parametros-$1.txt ausente" >&2; exit 1; }
}

case "${1:-}" in
    --check)
        for b in ipr mp; do
            prepara "$b"
            mkdir -p "exec-$b"
            echo "=== --check: $b ==="
            irace --scenario scenario.txt --parameter-file "parametros-$b.txt" \
                  --exec-dir "./exec-$b" --check
        done
        ;;
    --teste)
        for b in $(bracos "${2:-ambos}"); do
            prepara "$b"
            rm -rf "exec-teste-$b" && mkdir -p "exec-teste-$b"
            echo "=== teste de fumaca: $b ==="
            IRACE_MAXTIME=2 IRACE_REGISTRO="$PWD/exec-teste-$b/execucoes.csv" \
                irace --scenario scenario.txt --parameter-file "parametros-$b.txt" \
                      --exec-dir "./exec-teste-$b" --max-experiments 300 \
                      2>&1 | tee "exec-teste-$b/irace.log"
        done
        ;;
    ""|ipr|mp|ambos)
        lista=$(bracos "${1:-ambos}")
        for b in $lista; do
            if [ -e "exec-$b/irace.Rdata" ]; then
                echo "ERRO: exec-$b/irace.Rdata ja' existe. Mova exec-$b/ antes" \
                     "de uma nova calibracao." >&2
                exit 1
            fi
        done
        for b in $lista; do
            prepara "$b"
            mkdir -p "exec-$b"
            echo "=== calibracao: $b | inicio $(date '+%d/%m %H:%M') ===" | tee "exec-$b/irace.log"
            IRACE_REGISTRO="$PWD/exec-$b/execucoes.csv" \
                irace --scenario scenario.txt --parameter-file "parametros-$b.txt" \
                      --exec-dir "./exec-$b" 2>&1 | tee -a "exec-$b/irace.log"
            echo "=== calibracao: $b | fim $(date '+%d/%m %H:%M') ===" | tee -a "exec-$b/irace.log"
        done
        ;;
    *)
        echo "Uso: $0 [--check | --teste [braco] | braco]   (braco: ipr, mp, ambos)" >&2; exit 1 ;;
esac
