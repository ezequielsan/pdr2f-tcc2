#!/usr/bin/env bash
###############################################################################
# roda_irace.sh: executa a calibracao do BRKGA-MP-IPR com o irace.
#
# Uso:
#   ./irace/roda_irace.sh --check   valida cenario e parametros, sem executar
#   ./irace/roda_irace.sh --teste   teste de fumaca: 300 execucoes de 2 s
#   ./irace/roda_irace.sh           calibracao completa (~7 h)
#
# A calibracao completa grava em irace/exec/: o log do irace, o arquivo
# irace.Rdata (todos os dados da calibracao) e execucoes.csv (uma linha por
# execucao do BRKGA).
###############################################################################

set -eu
cd "$(dirname "$0")"

command -v irace > /dev/null || { echo "ERRO: irace nao esta no PATH (ver ~/.zshrc)." >&2; exit 1; }
[ -x ../main_complete ]      || { echo "ERRO: ../main_complete nao existe. Rode 'make' na raiz." >&2; exit 1; }
[ -s treino.txt ]            || { echo "ERRO: treino.txt ausente. Rode scripts/sorteia_treino.py." >&2; exit 1; }

case "${1:-}" in
    --check)
        mkdir -p exec
        irace --scenario scenario.txt --check
        ;;
    --teste)
        rm -rf exec-teste && mkdir -p exec-teste
        IRACE_MAXTIME=2 IRACE_REGISTRO="$PWD/exec-teste/execucoes.csv" \
            irace --scenario scenario.txt --exec-dir ./exec-teste \
                  --max-experiments 300 2>&1 | tee exec-teste/irace.log
        ;;
    "")
        if [ -e exec/irace.Rdata ]; then
            echo "ERRO: exec/irace.Rdata ja' existe. Mova ou apague exec/ antes" \
                 "de uma nova calibracao." >&2
            exit 1
        fi
        mkdir -p exec
        irace --scenario scenario.txt 2>&1 | tee exec/irace.log
        ;;
    *)
        echo "Uso: $0 [--check | --teste]" >&2; exit 1 ;;
esac
