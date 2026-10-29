#!/usr/bin/env python3
"""valida_decoders.py: compara as quatro variantes de decodificador.

Le a saida de decoder_info e os otimos do PLI, e reporta o gap de cada
variante por classe de instancia. Valida tambem que nenhuma variante produz
solucao inviavel ou abaixo do otimo.

As quatro variantes usam cromossomos ALEATORIOS, sem evolucao. O objetivo e'
isolar o efeito do decodificador do efeito da meta-heuristica.

Uso:
    make decoder_info
    ./decoder_info --seeds 10 instances/*/*.txt > decoders.csv
    python3 scripts/valida_decoders.py decoders.csv

Codigo de saida: 0 se a corretude passa, 1 caso contrario.
"""

import collections
import csv
import statistics
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SAIDA_PLI = RAIZ / "pli" / "integer_linear_program" / "output"

VARIANTES = [
    ("limiares",          "ThrBest"),
    ("limiares+reducao",  "ThrRedBest"),
    ("permutacao",        "PermBest"),
    ("permutacao+reducao", "PermRedBest"),
]


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

    inviaveis = [l["Instance"] for l in nossos if int(l["Infeasible"]) > 0]
    abaixo = []
    gaps = {nome: collections.defaultdict(list) for nome, _ in VARIANTES}

    for linha in nossos:
        nome = linha["Instance"]
        if nome not in pli:
            continue
        ref = pli[nome]
        if ref["status"] != "OPTIMAL":
            continue
        otimo = float(ref["weight"])

        for variante, coluna in VARIANTES:
            valor = float(linha[coluna])
            if valor < otimo - 1e-9:
                abaixo.append((nome, variante, valor, otimo))
            gaps[variante][ref["classe"]].append(100.0 * (valor - otimo) / otimo)

    print(f"instancias avaliadas               : {len(nossos)}")
    print(f"solucoes INVIAVEIS                 : {len(inviaveis)} {inviaveis[:6]}")
    print(f"abaixo do otimo (impossivel)       : {len(abaixo)} {abaixo[:6]}")

    classes = sorted({c for v in gaps.values() for c in v})

    print(f"\nGAP MEDIO em relacao ao otimo do PLI, por classe"
          f" (cromossomos aleatorios, sem evolucao)\n")
    cab = f"{'classe':<22}{'inst':>6}"
    for nome, _ in VARIANTES:
        cab += f"{nome:>20}"
    print(cab)
    print("-" * len(cab))

    for classe in classes:
        n = len(gaps[VARIANTES[0][0]][classe])
        linha = f"{classe:<22}{n:>6}"
        for nome, _ in VARIANTES:
            v = gaps[nome][classe]
            linha += f"{statistics.mean(v):>19.1f}%" if v else f"{'-':>20}"
        print(linha)

    linha = f"{'TODAS':<22}"
    todos0 = [x for c in classes for x in gaps[VARIANTES[0][0]][c]]
    linha += f"{len(todos0):>6}"
    for nome, _ in VARIANTES:
        todos = [x for c in classes for x in gaps[nome][c]]
        linha += f"{statistics.mean(todos):>19.1f}%"
    print("-" * len(cab))
    print(linha)

    print("\nInstancias em que a variante ja' atinge o otimo:")
    for nome, _ in VARIANTES:
        todos = [x for c in classes for x in gaps[nome][c]]
        otimas = sum(1 for g in todos if g < 1e-9)
        print(f"  {nome:<22}{otimas:>5} de {len(todos)}")

    if inviaveis or abaixo:
        print("\nFALHA: ver acima.")
        return 1

    print("\nOK: nenhuma solucao inviavel e nenhuma abaixo do otimo.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
