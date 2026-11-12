#!/usr/bin/env bash
###############################################################################
# bateria.sh: bateria FINAL do TCC 2 -- BRKGA-MP x BRKGA-MP-IPR nas instancias
# de TESTE (as 287 com alvo/baseline em results/baseline/alvos.csv menos as 86
# de treino em irace/treino.txt = 201 instancias).
#
# Protocolo (claude/protocolo_experimental_fechado.md e piloto de 11/2026):
#   - configuracoes finais do irace (configs/<braco>/final_perm.conf),
#   - 10 sementes (0 a 9), AS MESMAS nos dois bracos,
#   - 1 thread por execucao, sem warm start,
#   - para no PRIMEIRO destes eventos:
#       1. 600 s de tempo REAL (relogio de parede, robusto aos saltos da WSL),
#       2. alvo atingido (otimo provado pelo PLI ou ceil(2n/3) das cubicas),
#       3. limite de geracoes da categoria de tamanho (configs/
#          limites_geracoes.csv, definido no piloto: regra 1,5 x maximo),
#   - curva de convergencia gravada (uma linha a cada melhora).
#
# Uso (a partir da raiz):
#   ./scripts/bateria.sh          roda
#   ./scripts/bateria.sh -n       simulacao: conta e lista as tarefas
#   ./scripts/bateria.sh -j 4     outro numero de processos (padrao 6)
#
# RETOMAVEL: execucoes ja' gravadas em results/bateria/execucoes.csv sao
# puladas; se a maquina desligar, basta rodar de novo.
###############################################################################

set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"

BINARIO="$RAIZ/main_complete"
ALVOS="$RAIZ/results/baseline/alvos.csv"
TREINO="$RAIZ/irace/treino.txt"
LIMITES="$RAIZ/configs/limites_geracoes.csv"
SAIDA_DIR="$RAIZ/results/bateria"
SAIDA="$SAIDA_DIR/execucoes.csv"
CONFIG_MP="configs/brkga-mp/final_perm.conf"
CONFIG_IPR="configs/brkga-mp-ipr/final_perm.conf"

SEMENTES=10
TEMPO=600
JOBS=6
SIMULA=0
while getopts "j:n" op; do
    case "$op" in
        j) JOBS="$OPTARG" ;;
        n) SIMULA=1 ;;
        *) echo "Uso: $0 [-j processos] [-n]" >&2; exit 1 ;;
    esac
done

[ -x "$BINARIO" ] || { echo "ERRO: $BINARIO nao existe; rode 'make main_complete'" >&2; exit 1; }
for f in "$ALVOS" "$TREINO" "$LIMITES" "$CONFIG_MP" "$CONFIG_IPR"; do
    [ -s "$f" ] || { echo "ERRO: $f ausente" >&2; exit 1; }
done
mkdir -p "$SAIDA_DIR/convergencia/brkga-mp" "$SAIDA_DIR/convergencia/brkga-mp-ipr"

# Limite de geracoes do braco $1 para uma instancia de $2 vertices.
limite() {
    awk -F, -v b="$1" -v n="$2" \
        'NR > 1 && $1 == b && n >= $2 && n <= $3 { print $4; achou = 1 }
         END { if (!achou) exit 1 }' "$LIMITES"
}

# Execucoes ja' feitas: Instance|Algorithm|Seed (campos 1, 5 e 8 do CSV).
FEITAS="$SAIDA_DIR/.feitas"
if [ -s "$SAIDA" ]; then
    awk -F, 'NR > 1 { print $1 "|" $5 "|" $8 }' "$SAIDA" | sort -u > "$FEITAS"
else
    : > "$FEITAS"
fi

# Instancias de treino, so' o nome (para exclui-las).
NOMES_TREINO="$SAIDA_DIR/.treino"
sed 's#.*/##; s#\.txt$##' "$TREINO" | sort -u > "$NOMES_TREINO"

# Tarefas: prioridade 0 para as instancias sem otimo provado (rodam o tempo
# todo), depois as maiores primeiro; as curtas ficam no fim da fila, o que
# equilibra os processos no final.
TAREFAS="$SAIDA_DIR/tarefas.txt"
: > "$TAREFAS"
NINST=0
while IFS=, read -r nome classe n alvo fonte; do
    grep -qxF "$nome" "$NOMES_TREINO" && continue
    arq="$RAIZ/instances/$classe/$nome.txt"
    [ -s "$arq" ] || { echo "ERRO: $arq ausente" >&2; exit 1; }
    NINST=$((NINST + 1))
    [ -n "$alvo" ] || alvo=0
    prio=1; [ "$fonte" = "PLI_OPTIMAL" ] || prio=0
    for par in "brkga-mp|$CONFIG_MP" "brkga-mp-ipr|$CONFIG_IPR"; do
        algo="${par%%|*}"; conf="${par#*|}"
        ger=$(limite "$algo" "$n") || { echo "ERRO: sem limite para $algo n=$n" >&2; exit 1; }
        for s in $(seq 0 $((SEMENTES - 1))); do
            grep -qxF "$nome|$algo|$s" "$FEITAS" && continue
            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                "$prio" "$n" "$algo" "$conf" "$arq" "$alvo" "$s" "$nome" "$ger" >> "$TAREFAS"
        done
    done
done < <(tail -n +2 "$ALVOS")
sort -t$'\t' -k1,1n -k2,2nr -o "$TAREFAS" "$TAREFAS"

TOTAL=$(wc -l < "$TAREFAS")
echo "bateria: $NINST instancias de teste | $TOTAL execucoes pendentes | $SEMENTES sementes | $TEMPO s | $JOBS processos"
echo "pior caso (todas ate' $TEMPO s): $(awk -v t="$TOTAL" -v T="$TEMPO" -v j="$JOBS" 'BEGIN { printf "%.1f h", t * T / j / 3600 }')"
if [ "$SIMULA" = 1 ]; then
    awk -F'\t' '{ c[$3 " | limite " $9 " geracoes"]++ } END { for (k in c) printf "  %-40s %5d execucoes\n", k, c[k] }' "$TAREFAS" | sort
    echo "primeiras da fila:"
    head -4 "$TAREFAS" | awk -F'\t' '{ printf "  %-14s %-24s n=%-5s semente %s\n", $3, $8, $2, $7 }'
    exit 0
fi
[ "$TOTAL" -gt 0 ] || { echo "nada a fazer"; exit 0; }
[ -s "$SAIDA" ] || "$BINARIO" --header > "$SAIDA"

executa() {
    IFS=$'\t' read -r _p _n algo conf arq alvo s nome ger <<< "$1"
    local conv="$SAIDA_DIR/convergencia/$algo/${nome}_s${s}.csv"
    local linha
    linha=$("$BINARIO" --config "$conf" --instance "$arq" --decoder permutation \
                       --seed "$s" --maxtime "$TEMPO" --target "$alvo" \
                       --generations "$ger" --convlog "$conv" \
                       --threads 1 --quiet 2> /dev/null | tail -1)
    if [ "$(echo "$linha" | awk -F, '{print NF}')" != 29 ]; then
        echo "FALHOU: $algo $nome semente $s" >&2; return 0
    fi
    ( flock 9; echo "$linha" >> "$SAIDA" ) 9> "$SAIDA.lock"
}
export -f executa
export BINARIO SAIDA SAIDA_DIR TEMPO

echo "inicio: $(date '+%d/%m %H:%M')"
xargs -d '\n' -P "$JOBS" -I{} bash -c 'executa "$1"' _ {} < "$TAREFAS"
FEITO=$(($(wc -l < "$SAIDA") - 1))
echo "fim: $(date '+%d/%m %H:%M') | $FEITO execucoes gravadas"
