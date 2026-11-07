#!/usr/bin/env bash
###############################################################################
# valida_finalistas.sh: valida as configuracoes finalistas do irace em TODAS
# as instancias de treino.
#
# O irace usou so' parte das 86 instancias de treino (16 no BRKGA-MP-IPR, 20
# no BRKGA-MP). Esta etapa roda as finalistas de cada braco (o bloco "Best
# configurations as commandlines" do irace.log) em todas as 86, com varias
# sementes, nas mesmas condicoes da calibracao: 120 s, parada no alvo, 1 thread.
# As instancias de teste continuam intocadas.
#
# A regra de escolha e' fixada ANTES de ver os resultados (analisa_validacao.py):
# para cada par instancia x semente, as finalistas sao ordenadas pelo custo do
# irace; vence a de menor rank medio.
#
# Uso (a partir da raiz do repositorio):
#   ./irace/valida_finalistas.sh            os dois bracos, 2 sementes
#   ./irace/valida_finalistas.sh -n         simulacao: lista as tarefas
#   ./irace/valida_finalistas.sh -s 3       3 sementes
#
# E' RETOMAVEL: execucoes ja' gravadas em validacao/execucoes.csv sao puladas.
###############################################################################

set -euo pipefail
cd "$(dirname "$0")"
RAIZ="$(cd .. && pwd)"
BINARIO="$RAIZ/main_complete"
ALVOS="$RAIZ/results/baseline/alvos.csv"
SAIDA_DIR="$PWD/validacao"
SAIDA="$SAIDA_DIR/execucoes.csv"

SEMENTES=2
JOBS=6
SIMULA=0
TEMPO=120
while getopts "s:j:t:n" op; do
    case "$op" in
        s) SEMENTES="$OPTARG" ;;
        j) JOBS="$OPTARG" ;;
        t) TEMPO="$OPTARG" ;;
        n) SIMULA=1 ;;
        *) echo "Uso: $0 [-s sementes] [-j processos] [-t segundos] [-n]" >&2; exit 1 ;;
    esac
done

[ -x "$BINARIO" ]  || { echo "ERRO: $BINARIO nao existe; rode 'make main_complete'" >&2; exit 1; }
[ -s "$ALVOS" ]    || { echo "ERRO: $ALVOS ausente" >&2; exit 1; }
mkdir -p "$SAIDA_DIR/configs"

# --- configuracoes finalistas: do irace.log para arquivos .conf ------------
monta_config() {   # $1 = base, $2 = saida, resto = pares chave valor
    local base="$1" saida="$2"; shift 2
    local pares=""
    while [ $# -gt 0 ]; do pares+="$1=$2;"; shift 2; done
    awk -v pares="$pares" '
        BEGIN { n = split(pares, l, ";"); for(i = 1; i <= n; ++i) if(split(l[i], kv, "=") == 2) novo[kv[1]] = kv[2] }
        { c = tolower($1); if(c in novo) { print c " " novo[c]; usado[c] = 1 } else print }
        END { for(k in novo) if(!(k in usado)) { print "chave sem linha na base: " k > "/dev/stderr"; exit 3 } }
    ' "$base" > "$saida"
}

LISTA_CONFIGS=""
for b in ipr mp; do
    LOG="exec-$b/irace.log"
    [ -s "$LOG" ] || { echo "ERRO: $LOG ausente" >&2; exit 1; }
    # Linhas "ID chave valor ..." depois do cabecalho, ate' a primeira linha vazia.
    while read -r linha; do
        set -- $linha
        id="$1"; shift
        conf="$SAIDA_DIR/configs/${b}_${id}.conf"
        monta_config "base-$b.conf" "$conf" "$@"
        LISTA_CONFIGS+="$b $id $conf"$'\n'
    done < <(awk '/^# Best configurations as commandlines/ {f = 1; next} f && NF == 0 {exit} f' "$LOG")
done

echo "finalistas:"
echo "$LISTA_CONFIGS" | awk 'NF {printf "  %-4s %s\n", $1, $2}'

# --- tarefas: braco x finalista x instancia x semente ----------------------
# Ordenadas da maior instancia para a menor: as longas comecam primeiro e a
# fila termina com as curtas, o que aproveita melhor os processos.
TAREFAS="$SAIDA_DIR/tarefas.txt"
: > "$TAREFAS"
FEITAS="$SAIDA_DIR/.feitas"
if [ -s "$SAIDA" ]; then
    awk -F, 'NR > 1 { print $1 "|" $2 "|" $4 "|" $11 }' "$SAIDA" | sort -u > "$FEITAS"
else
    : > "$FEITAS"
fi

while read -r inst; do
    arq="$RAIZ/instances/${inst#../instances/}"
    nome="$(basename "$arq" .txt)"
    n=$(awk -F, -v i="$nome" 'NR > 1 && $1 == i { print $3 }' "$ALVOS")
    alvo=$(awk -F, -v i="$nome" 'NR > 1 && $1 == i { print $4 }' "$ALVOS")
    [ -n "$alvo" ] || alvo=0
    while read -r b id conf; do
        [ -n "$b" ] || continue
        for s in $(seq 1 "$SEMENTES"); do
            grep -qxF "$b|$id|$nome|$s" "$FEITAS" && continue
            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "$b" "$id" "$conf" "$arq" "$alvo" "$s" >> "$TAREFAS"
        done
    done <<< "$LISTA_CONFIGS"
done < treino.txt

sort -t$'\t' -k1,1nr -o "$TAREFAS" "$TAREFAS"
TOTAL=$(wc -l < "$TAREFAS")
echo "tarefas pendentes: $TOTAL | $JOBS processos | ${TEMPO} s por execucao"
echo "pior caso: $(awk -v t="$TOTAL" -v T="$TEMPO" -v j="$JOBS" 'BEGIN { printf "%.1f h", t * T / j / 3600 }')"
[ "$SIMULA" = 1 ] && { head -5 "$TAREFAS" | cut -f2,3,5,7; exit 0; }
[ "$TOTAL" -gt 0 ] || { echo "nada a fazer"; exit 0; }

[ -s "$SAIDA" ] || echo "Arm,ConfigId,Cost,$("$BINARIO" --header)" > "$SAIDA"

executa() {
    IFS=$'\t' read -r _n b id conf arq alvo s <<< "$1"
    linha=$("$BINARIO" --config "$conf" --instance "$arq" --decoder permutation \
                       --seed "$s" --maxtime "$TEMPO" --target "$alvo" \
                       --threads 1 --quiet 2>/dev/null | tail -1)
    [ "$(echo "$linha" | awk -F, '{print NF}')" = 29 ] || { echo "FALHOU: $b $id $(basename "$arq") $s" >&2; return 0; }
    peso=$(echo "$linha" | cut -d, -f9); t=$(echo "$linha" | cut -d, -f17)
    custo=$(awk -v p="$peso" -v t="$t" -v T="$TEMPO" 'BEGIN { if(t > T) t = T; printf "%.6f", p + 0.999 * t / T }')
    ( flock 9; echo "$b,$id,$custo,$linha" >> "$SAIDA" ) 9> "$SAIDA.lock"
}
export -f executa
export BINARIO SAIDA TEMPO

INICIO=$(date '+%d/%m %H:%M')
echo "inicio: $INICIO"
xargs -d '\n' -P "$JOBS" -I{} bash -c 'executa "$1"' _ {} < "$TAREFAS"
echo "fim: $(date '+%d/%m %H:%M') | analise: python3 irace/analisa_validacao.py"
