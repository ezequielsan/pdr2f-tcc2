#!/usr/bin/env bash
###############################################################################
# run_experiment.sh: bateria de experimentos do BRKGA-MP(-IPR) no PDR2F.
#
# Roda o main_complete para cada combinacao de instancia, decodificador e
# semente, acumulando um CSV unico. Respeita o protocolo do artigo do VNS:
#
#   - limite de tempo por FAIXA DE TAMANHO (o main_complete cuida disso);
#   - 10 execucoes em random e wireless-networks, 1 nas demais classes;
#   - uma unica thread por execucao.
#
# E' RETOMAVEL: cada execucao ja' presente no CSV de saida e' pulada, de modo
# que a bateria pode ser interrompida e recomecada sem perder trabalho.
#
# E' PARALELA por instancia, nao por thread: cada processo roda com uma
# thread, o que preserva a comparabilidade com o artigo, e varios processos
# rodam ao mesmo tempo.
#
# Uso:
#   ./scripts/run_experiment.sh [opcoes]
#
# Opcoes:
#   -c <arq>   config do decodificador 'threshold'
#   -p <arq>   config do decodificador 'permutation'
#   -d "<...>" decodificadores               ["threshold permutation"]
#   -C "<...>" classes                       [todas as cinco]
#   -i <glob>  filtra instancias pelo nome    [todas]  ex.: -i '100_*'
#   -j <n>     processos em paralelo          [nucleos - 2]
#   -w <n>     solucoes gulosas no warm start [0]
#   -o <arq>   CSV de saida                   [results/bateria.csv]
#   -t <s>     forca um limite de tempo       [faixa do protocolo]
#   -g <n>     limite de geracoes             [0 = so' tempo]
#   -n         simulacao: lista o que seria executado e termina
#
# Exemplos:
#   # bateria completa do BRKGA-MP-IPR, os dois decodificadores
#   ./scripts/run_experiment.sh -c configs/brkga-mp-ipr/dev_threshold.conf \
#                               -p configs/brkga-mp-ipr/dev_perm.conf \
#                               -o results/mp-ipr.csv
#
#   # teste rapido: uma classe, tempo curto, poucas sementes
#   ./scripts/run_experiment.sh -C random -t 10 -o /tmp/teste.csv
###############################################################################

set -u

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"

CONFIG_THRESHOLD="configs/brkga-mp-ipr/dev_threshold.conf"
CONFIG_PERM="configs/brkga-mp-ipr/dev_perm.conf"
DECODERS="threshold permutation"
CLASSES="random wireless-networks geoinstances harwell-boeing random_cubic"
FILTRO="*"
JOBS=$(( $(nproc 2>/dev/null || echo 4) - 2 ))
[ "$JOBS" -lt 1 ] && JOBS=1
WARMSTART=0
OUTPUT="results/bateria.csv"
FORCE_TIME=""
GENERATIONS=0
DRY_RUN=0

while getopts "c:p:d:C:i:j:w:o:t:g:nh" opt; do
    case $opt in
        c) CONFIG_THRESHOLD="$OPTARG" ;;
        p) CONFIG_PERM="$OPTARG" ;;
        d) DECODERS="$OPTARG" ;;
        C) CLASSES="$OPTARG" ;;
        i) FILTRO="$OPTARG" ;;
        j) JOBS="$OPTARG" ;;
        w) WARMSTART="$OPTARG" ;;
        o) OUTPUT="$OPTARG" ;;
        t) FORCE_TIME="$OPTARG" ;;
        g) GENERATIONS="$OPTARG" ;;
        n) DRY_RUN=1 ;;
        h) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo "Use -h para a ajuda." >&2; exit 1 ;;
    esac
done

[ -x ./main_complete ] || { echo "ERRO: ./main_complete nao encontrado. Rode 'make'." >&2; exit 1; }

# As duplicatas gn-* das geoinstances NAO sao executadas: sao copias byte a
# byte das instancias de mesmo nome sem o prefixo, confirmado pelo artigo, que
# usa 168 e nao 258.
nao_e_duplicata() { case "$(basename "$1")" in gn-*) return 1 ;; *) return 0 ;; esac; }

# Numero de execucoes por instancia, conforme o protocolo do artigo.
sementes_da_classe() {
    case "$1" in
        random|wireless-networks) echo 10 ;;
        *)                        echo 1  ;;
    esac
}

mkdir -p "$(dirname "$OUTPUT")"
[ -s "$OUTPUT" ] || ./main_complete --header > "$OUTPUT"

# Chaves ja' presentes no CSV, para a retomada.
JA_FEITAS=$(mktemp); trap 'rm -f "$JA_FEITAS"' EXIT
awk -F, 'NR>1 {print $1"|"$5"|"$6"|"$8}' "$OUTPUT" | sort -u > "$JA_FEITAS"

# O rotulo do algoritmo vem do config, como no main_complete.
algoritmo_do_config() {
    local intervalo
    intervalo=$(awk '/^ipr_interval/ {print $2}' "$1")
    [ "${intervalo:-0}" = "0" ] && echo "brkga-mp" || echo "brkga-mp-ipr"
}

###############################################################################
# Monta a lista de execucoes
###############################################################################

TAREFAS=$(mktemp); trap 'rm -f "$JA_FEITAS" "$TAREFAS"' EXIT
total=0; puladas=0

for classe in $CLASSES; do
    [ -d "instances/$classe" ] || { echo "ERRO: classe '$classe' inexistente." >&2; exit 1; }
    n_sementes=$(sementes_da_classe "$classe")

    for instancia in instances/"$classe"/$FILTRO.txt; do
        [ -f "$instancia" ] || continue
        nao_e_duplicata "$instancia" || continue
        nome=$(basename "$instancia" .txt)

        for decoder in $DECODERS; do
            case $decoder in
                threshold)   config="$CONFIG_THRESHOLD" ;;
                permutation) config="$CONFIG_PERM" ;;
                *) echo "ERRO: decodificador '$decoder' desconhecido." >&2; exit 1 ;;
            esac
            [ -f "$config" ] || { echo "ERRO: config '$config' nao encontrado." >&2; exit 1; }
            algo=$(algoritmo_do_config "$config")

            for semente in $(seq 0 $((n_sementes - 1))); do
                if grep -qxF "$nome|$algo|$decoder|$semente" "$JA_FEITAS"; then
                    puladas=$((puladas + 1)); continue
                fi
                printf '%s\t%s\t%s\t%s\n' "$instancia" "$config" "$decoder" "$semente" >> "$TAREFAS"
                total=$((total + 1))
            done
        done
    done
done

echo "=============================================================="
echo " execucoes a fazer : $total"
echo " ja' no CSV        : $puladas  (serao puladas)"
echo " filtro            : $FILTRO"
echo " paralelismo       : $JOBS processos de 1 thread"
echo " warm start        : $WARMSTART"
echo " limite de tempo   : ${FORCE_TIME:-faixa do protocolo (300/600/1200 s)}"
echo " saida             : $OUTPUT"
echo "=============================================================="

if [ "$DRY_RUN" -eq 1 ]; then
    echo; echo "Simulacao (-n). Primeiras 10 execucoes:"
    head -10 "$TAREFAS" | awk -F'\t' '{printf "  %-46s %-12s semente %s\n", $1, $3, $4}'
    exit 0
fi
[ "$total" -eq 0 ] && { echo "Nada a fazer."; exit 0; }

###############################################################################
# Execucao
###############################################################################

executa_uma() {
    IFS=$'\t' read -r instancia config decoder semente <<< "$1"

    local extra=()
    [ -n "$FORCE_TIME" ] && extra+=(--maxtime "$FORCE_TIME")
    [ "$GENERATIONS" -gt 0 ] && extra+=(--generations "$GENERATIONS")
    [ "$WARMSTART" -gt 0 ] && extra+=(--warmstart "$WARMSTART")

    local linha
    if ! linha=$(./main_complete --config "$config" --instance "$instancia" \
                     --decoder "$decoder" --seed "$semente" --threads 1 \
                     --quiet "${extra[@]}" 2>/dev/null | tail -1); then
        echo "FALHOU: $(basename "$instancia" .txt) $decoder semente $semente" >&2
        return
    fi

    # A linha tem 27 campos; qualquer coisa diferente nao entra no CSV.
    if [ "$(echo "$linha" | awk -F, '{print NF}')" -ne 27 ]; then
        echo "SAIDA INESPERADA: $(basename "$instancia" .txt) $decoder semente $semente" >&2
        return
    fi

    # flock serializa a escrita entre os processos paralelos.
    ( flock 9; printf '%s\n' "$linha" >> "$OUTPUT" ) 9> "$OUTPUT.lock"
    echo "$linha" | awk -F, '{printf "  %-26s %-12s s=%-3s peso %-6s %6.1fs\n", $1, $6, $8, $9, $16}'
}
export -f executa_uma
export OUTPUT FORCE_TIME GENERATIONS WARMSTART

inicio=$(date +%s)
# shellcheck disable=SC2016
cat "$TAREFAS" | xargs -d '\n' -P "$JOBS" -I{} bash -c 'executa_uma "{}"'
fim=$(date +%s)

echo "=============================================================="
echo " concluido em $(( (fim - inicio) / 60 )) min"
echo " linhas no CSV: $(( $(wc -l < "$OUTPUT") - 1 ))"
echo " proximo passo: python3 scripts/analisa_bateria.py $OUTPUT"
echo "=============================================================="
