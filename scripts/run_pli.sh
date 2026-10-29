#!/usr/bin/env bash
###############################################################################
# run_pli.sh: roda o PLI (Julia + CPLEX) sobre as instancias do projeto.
#
# As instancias ficam em instances/<classe>/ e cada resultado vai para
# pli/integer_linear_program/output/<classe>/<instancia>.csv.
# Instancias ja' processadas sao puladas, entao o script pode ser
# interrompido e retomado.
#
# Uso:
#   ./scripts/run_pli.sh                          # todas as classes
#   ./scripts/run_pli.sh random wireless-networks # apenas estas
#
# Por que este script existe, em vez de usar o run_all.sh do autor: aquele
# le' de pli/integer_linear_program/input/, diretorio que NAO copiamos (as
# instancias ja' estao em instances/). O arquivo do autor fica preservado,
# sem modificacoes, por causa da GPL-3.0.
###############################################################################

set -u

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
PLI="$RAIZ/pli/integer_linear_program"

CLASSES=${*:-"random wireless-networks geoinstances harwell-boeing random_cubic"}

if ! command -v julia > /dev/null; then
    echo "ERRO: julia nao encontrado no PATH." >&2
    exit 1
fi

for classe in $CLASSES; do
    entrada_dir="$RAIZ/instances/$classe"
    saida_dir="$PLI/output/$classe"

    if [ ! -d "$entrada_dir" ]; then
        echo "ERRO: classe desconhecida: '$classe'" >&2
        exit 1
    fi

    mkdir -p "$saida_dir"

    for entrada in "$entrada_dir"/*.txt; do
        nome=$(basename "$entrada" .txt)
        saida="$saida_dir/$nome.csv"

        if [ -f "$saida" ]; then
            echo "pulando (ja' existe): $classe/$nome"
            continue
        fi

        echo "==> $classe/$nome"
        # O pli.jl faz include("../greedy/...") com caminho relativo ao
        # diretorio de trabalho, por isso o cd.
        ( cd "$PLI" && julia pli.jl "$entrada" "$saida_dir" )
    done
done
