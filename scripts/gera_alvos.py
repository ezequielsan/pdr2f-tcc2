#!/usr/bin/env python3
"""gera_alvos.py: alvo de parada de cada instancia (results/baseline/alvos.csv).

O BRKGA para ao atingir o alvo (main_complete --target). Um alvo so' e' valido
se nenhuma solucao viavel puder ter peso menor; por isso so' entram:

  1. o otimo PROVADO pelo PLI (status OPTIMAL). Valores TIME_LIMIT ficam de
     fora: sao apenas a melhor solucao que o CPLEX achou em 10 min.
     Se a instancia so' tem otimo provado na rodada da duplicata gn-*, ele e'
     usado (as 90 gn-* sao grafos identicos, conferidos aresta por aresta);
  2. nas cubicas sem otimo provado, o limite inferior ceil(2n/3)
     (Nikolic et al., Teorema 1): se o BRKGA o atingir, a solucao e' otima.

Conferencias (o script aborta se alguma falhar):
  - nenhum valor do PLI (qualquer status) abaixo do limite inferior das cubicas;
  - nenhum otimo de uma rodada diferente do otimo da outra (instancia x gn-*);
  - nenhum valor do VNS publicado abaixo do alvo.

Uso:  python3 scripts/gera_alvos.py
"""

import csv
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
BASE = RAIZ / "results" / "baseline"
SAIDA_PLI = RAIZ / "pli" / "integer_linear_program" / "output" / "geoinstances"


def main():
    pli = {r["Instance"]: r for r in csv.DictReader((BASE / "pli_todas.csv").open())}
    vns = {r["Instance"]: r for r in csv.DictReader((BASE / "vns_k2.csv").open())}
    erros = []

    # Rodadas do PLI nas duplicatas gn-*: so' interessam as OPTIMAL.
    otimo_gn = {}
    for arq in sorted(SAIDA_PLI.glob("gn-*.csv")):
        linha = next(csv.DictReader(arq.open()))
        if linha["status"] == "OPTIMAL":
            otimo_gn[arq.stem[3:]] = round(float(linha["weight"]))

    linhas = []
    for nome, r in sorted(pli.items(), key=lambda kv: (kv[1]["Class"], kv[0])):
        n = int(r["NumNodes"])
        peso = int(r["Weight"])
        alvo, fonte = "", ""

        if r["Status"] == "OPTIMAL":
            alvo, fonte = peso, "PLI_OPTIMAL"
            if nome in otimo_gn and otimo_gn[nome] != peso:
                erros.append(f"{nome}: otimo {peso} difere do otimo da gn- ({otimo_gn[nome]})")
        elif nome in otimo_gn:
            alvo, fonte = otimo_gn[nome], "PLI_OPTIMAL_GN"

        if r["Class"] == "random_cubic":
            limite = -(-2 * n // 3)          # ceil(2n/3)
            if peso < limite:
                erros.append(f"{nome}: PLI {peso} abaixo do limite inferior {limite}")
            if alvo == "":
                alvo, fonte = limite, "CUBIC_LOWER_BOUND"

        if alvo != "" and nome in vns and int(vns[nome]["VNSBest"]) < alvo:
            erros.append(f"{nome}: VNS {vns[nome]['VNSBest']} abaixo do alvo {alvo}")

        linhas.append((nome, r["Class"], n, alvo, fonte))

    if erros:
        print("ERRO: conferencia falhou:\n  " + "\n  ".join(erros))
        return 1

    with (BASE / "alvos.csv").open("w", newline="") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["Instance", "Class", "NumNodes", "Target", "TargetSource"])
        w.writerows(linhas)

    contagem = {}
    for *_, fonte in linhas:
        contagem[fonte or "(sem alvo)"] = contagem.get(fonte or "(sem alvo)", 0) + 1
    print(f"gravado: results/baseline/alvos.csv ({len(linhas)} instancias)")
    for fonte, k in sorted(contagem.items()):
        print(f"  {fonte:<20}{k:>4}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
