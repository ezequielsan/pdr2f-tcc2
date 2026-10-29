#!/usr/bin/env python3
"""analisa_bateria.py: compara os resultados do BRKGA com o PLI e com o VNS.

Le o CSV produzido por scripts/run_experiment.sh e as duas referencias de
results/baseline/, e produz:

  1. gap medio em relacao ao otimo do PLI, por classe e por braco;
  2. comparacao direta com o VNS (vitorias, empates, derrotas);
  3. verificacao de corretude: nenhuma solucao inviavel, nenhuma abaixo do
     otimo provado.

Uso:
    python3 scripts/analisa_bateria.py results/bateria.csv

Codigo de saida: 0 se a corretude passa, 1 caso contrario.
"""

import collections
import csv
import statistics
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
BASELINE = RAIZ / "results" / "baseline"


def carrega(caminho, chave="Instance"):
    with open(caminho) as fh:
        return {linha[chave]: linha for linha in csv.DictReader(fh)}


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    pli = carrega(BASELINE / "pli_todas.csv")
    vns = carrega(BASELINE / "vns_k2.csv")

    with open(sys.argv[1]) as fh:
        execucoes = list(csv.DictReader(fh))

    if not execucoes:
        print("CSV vazio.")
        return 1

    # ---- corretude -------------------------------------------------------
    inviaveis = [r for r in execucoes if r["Feasible"] != "yes"]
    abaixo = []

    # ---- melhor de cada (instancia, braco) -------------------------------
    melhor = {}
    for r in execucoes:
        chave = (r["Instance"], r["Algorithm"], r["Decoder"], r["Reduction"])
        peso = int(r["Weight"])
        if chave not in melhor or peso < melhor[chave][0]:
            melhor[chave] = (peso, r)

        ref = pli.get(r["Instance"])
        if ref and ref["Status"] == "OPTIMAL" and peso < int(ref["Weight"]):
            abaixo.append((r["Instance"], peso, ref["Weight"]))

    bracos = sorted({(a, d, red) for _, a, d, red in melhor})
    classes = sorted({pli[i]["Class"] for i, _, _, _ in melhor if i in pli})

    print(f"execucoes lidas        : {len(execucoes)}")
    print(f"instancias distintas   : {len({i for i, _, _, _ in melhor})}")
    print(f"bracos                 : {len(bracos)}")
    print(f"solucoes INVIAVEIS     : {len(inviaveis)}")
    print(f"abaixo do otimo        : {len(abaixo)} {abaixo[:5]}")

    def rotulo(braco):
        a, d, red = braco
        return f"{a}/{d}" + ("" if red == "yes" else " (sem reducao)")

    # ---- 1. gap em relacao ao otimo do PLI -------------------------------
    print("\n" + "=" * 78)
    print(" GAP MEDIO EM RELACAO AO OTIMO DO PLI (instancias com otimo provado)")
    print("=" * 78)

    gaps = collections.defaultdict(lambda: collections.defaultdict(list))
    for (inst, a, d, red), (peso, _) in melhor.items():
        ref = pli.get(inst)
        if not ref or ref["Status"] != "OPTIMAL":
            continue
        otimo = int(ref["Weight"])
        gaps[(a, d, red)][ref["Class"]].append(100.0 * (peso - otimo) / otimo)

    cab = f"{'braco':<34}{'inst':>6}" + "".join(f"{c[:13]:>15}" for c in classes)
    print(cab)
    print("-" * len(cab))
    for braco in bracos:
        todos = [x for c in classes for x in gaps[braco][c]]
        if not todos:
            continue
        linha = f"{rotulo(braco)[:33]:<34}{len(todos):>6}"
        for c in classes:
            v = gaps[braco][c]
            linha += f"{statistics.mean(v):>14.1f}%" if v else f"{'-':>15}"
        print(linha)
        print(f"{'  -> media geral':<34}{'':>6}{statistics.mean(todos):>14.1f}%")

    # ---- 2. comparacao com o VNS ----------------------------------------
    print("\n" + "=" * 78)
    print(" COMPARACAO COM O VNS (melhor contra melhor, k = 2)")
    print("=" * 78)
    print(f"{'braco':<34}{'inst':>6}{'ganha':>8}{'empata':>8}{'perde':>8}"
          f"{'media nossa':>13}{'media VNS':>11}")
    print("-" * 88)

    for braco in bracos:
        ganha = empata = perde = 0
        nossos, deles = [], []
        for (inst, a, d, red), (peso, _) in melhor.items():
            if (a, d, red) != braco or inst not in vns:
                continue
            v = int(vns[inst]["VNSBest"])
            if peso < v:   ganha += 1
            elif peso == v: empata += 1
            else:           perde += 1
            nossos.append(peso); deles.append(v)
        if not nossos:
            continue
        print(f"{rotulo(braco)[:33]:<34}{len(nossos):>6}{ganha:>8}{empata:>8}"
              f"{perde:>8}{statistics.mean(nossos):>13.2f}"
              f"{statistics.mean(deles):>11.2f}")

    # ---- 3. instancias resolvidas no otimo ------------------------------
    print("\n" + "=" * 78)
    print(" INSTANCIAS RESOLVIDAS NO OTIMO")
    print("=" * 78)
    for braco in bracos:
        todos = [x for c in classes for x in gaps[braco][c]]
        if not todos:
            continue
        otimas = sum(1 for g in todos if abs(g) < 1e-9)
        print(f"  {rotulo(braco):<40}{otimas:>5} de {len(todos)}")

    # ---- fecho -----------------------------------------------------------
    if inviaveis or abaixo:
        print("\nFALHA de corretude:")
        for r in inviaveis[:10]:
            print(f"  INVIAVEL: {r['Instance']} {r['Algorithm']}/{r['Decoder']}"
                  f" semente {r['Seed']}")
        for a in abaixo[:10]:
            print(f"  ABAIXO DO OTIMO: {a[0]} peso {a[1]} < otimo {a[2]}")
        return 1

    print("\nOK: todas as solucoes viaveis e nenhuma abaixo do otimo provado.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
