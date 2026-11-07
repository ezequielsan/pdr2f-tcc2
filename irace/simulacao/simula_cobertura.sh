#!/usr/bin/env bash
###############################################################################
# simula_cobertura.sh: quantas das 86 instancias de treino o irace usaria em
# cada cenario, rodando o irace VERDADEIRO com um avaliador sintetico
# (target-runner-falso). Nao executa o BRKGA: cada cenario leva poucos minutos
# e usa 1 nucleo.
#
# O cenario "atual" reproduz a calibracao feita (4000 execucoes, opcoes
# padrao); se ele der perto de 16-20 instancias, o modelo esta' razoavel.
#
# Uso (a partir da raiz do repositorio):  ./irace/simulacao/simula_cobertura.sh
###############################################################################
set -euo pipefail
cd "$(dirname "$0")/.."      # irace/
command -v irace > /dev/null || { echo "ERRO: irace nao esta no PATH" >&2; exit 1; }
OUT="simulacao/resultado.txt"
: > "$OUT"

cenario() {   # nome orcamento opcoes...
    local nome="$1" orc="$2"; shift 2
    for b in ipr mp; do
        local dir="simulacao/exec-$nome-$b"
        rm -rf "$dir" && mkdir -p "$dir"
        export SIMULA_LOG="$PWD/$dir/chamadas.txt"; : > "$SIMULA_LOG"
        irace --scenario scenario.txt --parameter-file "parametros-$b.txt" \
              --target-runner ./simulacao/target-runner-falso --exec-dir "./$dir" \
              --max-experiments "$orc" --parallel 1 "$@" > "$dir/irace.log" 2>&1 || {
            echo "$nome $b: irace falhou (ver $dir/irace.log)" | tee -a "$OUT"; continue; }
        local inst cfg
        inst=$(awk '{print $2}' "$SIMULA_LOG" | sort -u | wc -l)
        cfg=$(awk '{print $1}' "$SIMULA_LOG" | sort -u | wc -l)
        printf '%-26s %-4s orcamento %6s | instancias %3s de 86 | configuracoes %4s\n' \
               "$nome" "$b" "$orc" "$inst" "$cfg" | tee -a "$OUT"
    done
}

cenario atual             4000
cenario orc8000           8000
cenario limite0_4000      4000 --elitist-limit 0
cenario limite0_8000      8000 --elitist-limit 0
cenario lim0_novas5_8000  8000 --elitist-limit 0 --elitist-new-instances 5
cenario lim0_novas5_ft10  8000 --elitist-limit 0 --elitist-new-instances 5 --first-test 10
cenario lim0_novas10_8000 8000 --elitist-limit 0 --elitist-new-instances 10
cenario lim0_novas5_6000  6000 --elitist-limit 0 --elitist-new-instances 5
echo "resultado em irace/$OUT"
