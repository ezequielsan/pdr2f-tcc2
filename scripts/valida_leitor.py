#!/usr/bin/env python3
"""valida_leitor.py: valida o leitor de instancias contra os CSVs do PLI.

O PLI (pli/integer_linear_program/output/<classe>/<instancia>.csv) registra
|V|, |E|, density, maxDegree e minDegree de cada instancia. Este script compara
esses cinco campos com o que o nosso leitor em C++ extrai, para garantir que o
PLI e o BRKGA operam sobre exatamente o mesmo grafo.

Uso:
    make instance_info
    ./instance_info instances/*/*.txt > leitor.csv
    python3 scripts/valida_leitor.py leitor.csv

Codigo de saida: 0 se tudo bate, 1 se houver divergencia.
"""

import csv
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SAIDA_PLI = RAIZ / "pli" / "integer_linear_program" / "output"

CAMPOS = [
    # (nome exibido, coluna no CSV do leitor, coluna no CSV do PLI, tolerancia)
    ("|V|",       "NumNodes",  "|V|",       0.0),
    ("|E|",       "NumEdges",  "|E|",       0.0),
    ("densidade", "Density",   "density",   1e-9),
    ("grau max",  "MaxDegree", "maxDegree", 0.0),
    ("grau min",  "MinDegree", "minDegree", 0.0),
]


def carrega_pli():
    """Devolve {nome_da_instancia: linha_do_csv_do_pli}."""
    dados = {}
    for csv_path in SAIDA_PLI.glob("*/*.csv"):
        with csv_path.open() as fh:
            for linha in csv.DictReader(fh):
                dados[linha["G"]] = linha
    return dados


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    pli = carrega_pli()
    if not pli:
        print(f"ERRO: nenhum CSV do PLI encontrado em {SAIDA_PLI}", file=sys.stderr)
        return 1

    with open(sys.argv[1]) as fh:
        nossos = list(csv.DictReader(fh))

    comparadas = 0
    sem_referencia = []
    divergencias = []

    for linha in nossos:
        nome = linha["Instance"]
        if nome not in pli:
            sem_referencia.append(nome)
            continue

        comparadas += 1
        for exibido, col_nosso, col_pli, tol in CAMPOS:
            nosso = float(linha[col_nosso])
            deles = float(pli[nome][col_pli])
            if abs(nosso - deles) > tol:
                divergencias.append((nome, exibido, nosso, deles))

    print(f"instancias lidas pelo nosso leitor : {len(nossos)}")
    print(f"instancias com referencia no PLI   : {comparadas}")
    print(f"sem referencia no PLI              : {len(sem_referencia)}")
    print(f"campos comparados                  : {comparadas * len(CAMPOS)}")
    print(f"DIVERGENCIAS                       : {len(divergencias)}")

    if sem_referencia:
        print("\nsem referencia no PLI:", ", ".join(sorted(sem_referencia)[:10]),
              "..." if len(sem_referencia) > 10 else "")

    if divergencias:
        print(f"\n{'instancia':<28}{'campo':<12}{'nosso':>18}{'PLI':>18}")
        for nome, campo, nosso, deles in divergencias[:40]:
            print(f"{nome:<28}{campo:<12}{nosso:>18}{deles:>18}")
        if len(divergencias) > 40:
            print(f"... e outras {len(divergencias) - 40}")
        return 1

    print("\nOK: o leitor concorda com o PLI em todos os campos comparados.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
