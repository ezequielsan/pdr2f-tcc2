#!/usr/bin/env python3
"""valida_guloso.py: valida o guloso contra os otimos do PLI.

Duas verificacoes de corretude e uma medicao.

Corretude:
  1. toda solucao do guloso e' viavel (coluna Feasible == yes);
  2. o peso do guloso NUNCA fica abaixo do otimo provado pelo PLI -- se
     ficasse, haveria erro no guloso, no verificador ou na referencia.

Medicao: o gap do guloso em relacao ao otimo, por classe. Esse numero importa
porque o guloso e' o MESMO que o VNS de Djukanovic et al. usa como construcao
inicial e que a PLI usa como warm start: ele e' o ponto de partida comum das
tres abordagens.

Uso:
    make greedy_info
    ./greedy_info --seeds 10 instances/*/*.txt > guloso.csv
    python3 scripts/valida_guloso.py guloso.csv

Codigo de saida: 0 se a corretude passa, 1 caso contrario.
"""

import collections
import csv
import statistics
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SAIDA_PLI = RAIZ / "pli" / "integer_linear_program" / "output"


def carrega_pli():
    dados = {}
    for caminho in SAIDA_PLI.glob("*/*.csv"):
        classe = caminho.parent.name
        with caminho.open() as fh:
            for linha in csv.DictReader(fh):
                linha["classe"] = classe
                dados[linha["G"]] = linha
    return dados


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    pli = carrega_pli()
    with open(sys.argv[1]) as fh:
        nossos = list(csv.DictReader(fh))

    inviaveis = [l["Instance"] for l in nossos if l["Feasible"] != "yes"]
    abaixo = []
    gaps = collections.defaultdict(list)
    sem_otimo = 0

    for linha in nossos:
        nome = linha["Instance"]
        if nome not in pli:
            continue
        ref = pli[nome]
        otimo = float(ref["weight"])
        melhor = float(linha["Best"])

        if ref["status"] != "OPTIMAL":
            sem_otimo += 1
            continue

        if melhor < otimo - 1e-9:
            abaixo.append((nome, melhor, otimo))

        gaps[ref["classe"]].append(100.0 * (melhor - otimo) / otimo)

    print(f"instancias avaliadas              : {len(nossos)}")
    print(f"com otimo provado pelo PLI        : {sum(len(v) for v in gaps.values())}")
    print(f"sem otimo provado (TIME_LIMIT)    : {sem_otimo}")
    print()
    print(f"solucoes INVIAVEIS                : {len(inviaveis)} "
          f"{inviaveis[:8]}")
    print(f"guloso ABAIXO do otimo (impossivel): {len(abaixo)} {abaixo[:8]}")

    if gaps:
        print()
        print(f"{'classe':<22}{'inst':>6}{'gap min':>10}{'gap medio':>12}"
              f"{'gap mediano':>14}{'gap max':>10}")
        todos = []
        for classe in sorted(gaps):
            v = gaps[classe]
            todos.extend(v)
            print(f"{classe:<22}{len(v):>6}{min(v):>9.1f}%{statistics.mean(v):>11.1f}%"
                  f"{statistics.median(v):>13.1f}%{max(v):>9.1f}%")
        print(f"{'TODAS':<22}{len(todos):>6}{min(todos):>9.1f}%"
              f"{statistics.mean(todos):>11.1f}%{statistics.median(todos):>13.1f}%"
              f"{max(todos):>9.1f}%")
        otimas = sum(1 for g in todos if g < 1e-9)
        print(f"\ninstancias em que o guloso ja' atinge o otimo: {otimas} de {len(todos)}")

    if inviaveis or abaixo:
        print("\nFALHA: ver acima.")
        return 1

    print("\nOK: todas as solucoes do guloso sao viaveis e nenhuma fica abaixo do otimo.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
