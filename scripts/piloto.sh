#!/usr/bin/env bash
###############################################################################
# piloto.sh: piloto que define as categorias de tamanho e o limite de geracoes
# de cada categoria, por braco.
#
# Roda as configuracoes FINAIS dos dois bracos nas 86 instancias de TREINO
# (irace/treino.txt; o teste continua intocado), com:
#   - 5 sementes (0 a 4),
#   - ate' 1800 s por execucao (3x o limite da bateria),
#   - parada no alvo (otimo provado ou limite das cubicas),
#   - SEM limite de geracoes,
#   - curva de convergencia gravada (uma linha a cada melhora).
#
# As curvas cortadas em 600 s dao o limite de geracoes por categoria; ate'
# 1800 s, mostram quanto se ganharia com mais tempo (scripts/analisa_piloto.py).
#
# Uso (a partir da raiz):
#   ./scripts/piloto.sh          roda (pior caso ~72 h; estimativa ~26 h)
#   ./scripts/piloto.sh -n       simulacao: lista as tarefas
#   ./scripts/piloto.sh -s 2     2 sementes   | -t 600: outro limite | -j 4: processos
#
# RETOMAVEL: execucoes ja' gravadas em results/piloto/execucoes.csv sao puladas.
###############################################################################

set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"

BINARIO="$RAIZ/main_complete"
ALVOS="$RAIZ/results/baseline/alvos.csv"
TREINO="$RAIZ/irace/treino.txt"
SAIDA_DIR="$RAIZ/results/piloto"
SAIDA="$SAIDA_DIR/execucoes.csv"
CONFIG_MP="configs/brkga-mp/final_perm.conf"
CONFIG_IPR="configs/brkga-mp-ipr/final_perm.conf"

SEMENTES=5
TEMPO=1800
JOBS=6
SIMULA=0
while getopts "s:t:j:n" op; do
    case "$op" in
        s) SEMENTES="$OPTARG" ;;
        t) TEMPO="$OPTARG" ;;
        j) JOBS="$OPTARG" ;;
        n) SIMULA=1 ;;
        *) echo "Uso: $0 [-s sementes] [-t segundos] [-j processos] [-n]" >&2; exit 1 ;;
    esac
done

[ -x "$BINARIO" ] || { echo "ERRO: $BINARIO nao existe; rode 'make main_complete'" >&2; exit 1; }
for f in "$ALVOS" "$TREINO" "$CONFIG_MP" "$CONFIG_IPR"; do
    [ -s "$f" ] || { echo "ERRO: $f ausente" >&2; exit 1; }
done
mkdir -p "$SAIDA_DIR/convergencia/brkga-mp" "$SAIDA_DIR/convergencia/brkga-mp-ipr"

# Execucoes ja' feitas: Instance|Algorithm|Seed (campos 1, 5 e 8 do CSV).
FEITAS="$SAIDA_DIR/.feitas"
if [ -s "$SAIDA" ]; then
    awk -F, 'NR > 1 { print $1 "|" $5 "|" $8 }' "$SAIDA" | sort -u > "$FEITAS"
else
    : > "$FEITAS"
fi

# Tarefas: prioridade 0 para as instancias sem otimo provado (rodam o tempo
# todo), depois as maiores primeiro; as curtas ficam no fim da fila.
TAREFAS="$SAIDA_DIR/tarefas.txt"
: > "$TAREFAS"
while read -r inst; do
    [ -n "$inst" ] || continue
    arq="$RAIZ/instances/${inst#../instances/}"
    nome="$(basename "$arq" .txt)"
    IFS=, read -r n alvo fonte < <(awk -F, -v i="$nome" 'NR > 1 && $1 == i { print $3 "," $4 "," $5 }' "$ALVOS")
    [ -n "$alvo" ] || alvo=0
    prio=1; [ "$fonte" = "PLI_OPTIMAL" ] || prio=0
    for par in "brkga-mp|$CONFIG_MP" "brkga-mp-ipr|$CONFIG_IPR"; do
        algo="${par%%|*}"; conf="${par#*|}"
        for s in $(seq 0 $((SEMENTES - 1))); do
            grep -qxF "$nome|$algo|$s" "$FEITAS" && continue
            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$prio" "$n" "$algo" "$conf" "$arq" "$alvo" "$s" "$nome" >> "$TAREFAS"
        done
    done
done < "$TREINO"
sort -t$'\t' -k1,1n -k2,2nr -o "$TAREFAS" "$TAREFAS"

TOTAL=$(wc -l < "$TAREFAS")
echo "piloto: $TOTAL execucoes pendentes | $SEMENTES sementes | ate' $TEMPO s | $JOBS processos"
echo "pior caso: $(awk -v t="$TOTAL" -v T="$TEMPO" -v j="$JOBS" 'BEGIN { printf "%.1f h", t * T / j / 3600 }')"
if [ "$SIMULA" = 1 ]; then
    head -4 "$TAREFAS" | awk -F'\t' '{ printf "  %-14s %-22s semente %s\n", $3, $8, $7 }'
    exit 0
fi
[ "$TOTAL" -gt 0 ] || { echo "nada a fazer"; exit 0; }
[ -s "$SAIDA" ] || "$BINARIO" --header > "$SAIDA"

executa() {
    IFS=$'\t' read -r _p _n algo conf arq alvo s nome <<< "$1"
    local conv="$SAIDA_DIR/convergencia/$algo/${nome}_s${s}.csv"
    local linha
    linha=$("$BINARIO" --config "$conf" --instance "$arq" --decoder permutation \
                       --seed "$s" --maxtime "$TEMPO" --target "$alvo" \
                       --convlog "$conv" --threads 1 --quiet 2> /dev/null | tail -1)
    if [ "$(echo "$linha" | awk -F, '{print NF}')" != 29 ]; then
        echo "FALHOU: $algo $nome semente $s" >&2; return 0
    fi
    ( flock 9; echo "$linha" >> "$SAIDA" ) 9> "$SAIDA.lock"
}
export -f executa
export BINARIO SAIDA SAIDA_DIR TEMPO

echo "inicio: $(date '+%d/%m %H:%M')"
xargs -d '\n' -P "$JOBS" -I{} bash -c 'executa "$1"' _ {} < "$TAREFAS"
echo "fim: $(date '+%d/%m %H:%M') | analise: python3 scripts/analisa_piloto.py"
