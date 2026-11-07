#!/usr/bin/env bash
###############################################################################
# simula_empates.sh: como simula_cobertura.sh, mas com o avaliador que devolve
# PESOS INTEIROS (empates exatos), como na 3a calibracao (custo = peso).
# Compara o cenario configurado (8000, elitistLimit 0) com mais instancias
# novas por iteracao. Usa 1 nucleo, ~15-20 min.
#
# Uso (a partir da raiz do repositorio):  ./irace/simulacao/simula_empates.sh
###############################################################################
set -euo pipefail
cd "$(dirname "$0")/.."      # irace/
command -v irace > /dev/null || { echo "ERRO: irace nao esta no PATH" >&2; exit 1; }
OUT="simulacao/resultado-empates.txt"
: > "$OUT"
cenario() {
    local nome="$1"; shift
    for b in ipr mp; do
        local dir="simulacao/emp-$nome-$b"
        rm -rf "$dir" && mkdir -p "$dir"
        export SIMULA_LOG="$PWD/$dir/chamadas.txt"; : > "$SIMULA_LOG"
        irace --scenario scenario.txt --parameter-file "parametros-$b.txt" \
              --target-runner ./simulacao/target-runner-falso-empates --exec-dir "./$dir" \
              --parallel 1 "$@" > "$dir/irace.log" 2>&1 || { echo "$nome $b: falhou" | tee -a "$OUT"; continue; }
        local best inst cfg nbest
        best=$(awk '/^# Best configurations as commandlines/ {getline; print $1; exit}' "$dir/irace.log")
        inst=$(awk '{print $2}' "$SIMULA_LOG" | sort -u | wc -l)
        cfg=$(awk '{print $1}' "$SIMULA_LOG" | sort -u | wc -l)
        nbest=$(awk -v b="$best" '$1 == b {print $2}' "$SIMULA_LOG" | sort -u | wc -l)
        printf '%-22s %-4s | instancias %3s de 86 | vencedora em %3s | configuracoes %4s\n' \
               "$nome" "$b" "$inst" "$nbest" "$cfg" | tee -a "$OUT"
    done
}
cenario configurado
cenario novas5   --elitist-new-instances 5
cenario novas10  --elitist-new-instances 10
echo "resultado em irace/$OUT"
