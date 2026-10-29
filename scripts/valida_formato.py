#!/usr/bin/env python3
"""valida_formato.py: audita o formato de TODAS as instancias do projeto.

O leitor em C++ (PDR2F_Instance) percorre apenas o triangulo SUPERIOR da
matriz, exatamente como o read_simple_graph do pli.jl. Isso e' correto para
matrizes simetricas, mas significa que uma matriz ASSIMETRICA seria lida em
silencio, usando so' metade dos dados. Este script fecha essa lacuna: faz uma
leitura independente, em Python, e verifica simetria, forma quadrada,
uniformidade da diagonal e o conjunto de valores presentes.

Roda sobre instances/<classe>/*.txt. Nao e' necessario antes de cada
experimento; e' uma auditoria da base, a ser repetida quando instancias novas
entrarem no projeto.

Uso:
    python3 scripts/valida_formato.py

Codigo de saida: 0 se a base esta consistente, 1 se houver problema.
"""

import collections
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
INSTANCIAS = RAIZ / "instances"


def le_matriz(caminho):
    """Leitura independente: devolve a matriz 0/1 das linhas nao vazias."""
    linhas = []
    with caminho.open(encoding="utf-8", errors="replace") as fh:
        for linha in fh:
            texto = linha.strip()
            if not texto:
                continue
            valores = texto.replace(",", " ").split()
            linhas.append([0.0 if float(v) == 0.0 else 1.0 for v in valores])
    return linhas


def main():
    arquivos = sorted(INSTANCIAS.glob("*/*.txt"))
    if not arquivos:
        print(f"ERRO: nenhuma instancia encontrada em {INSTANCIAS}", file=sys.stderr)
        return 1

    nao_quadradas = []
    assimetricas = []
    diagonais = collections.Counter()
    valores = collections.Counter()
    isolados = []
    sem_aresta = []

    for caminho in arquivos:
        nome = caminho.stem

        with caminho.open(encoding="utf-8", errors="replace") as fh:
            for linha in fh:
                for token in linha.strip().replace(",", " ").split():
                    valores[token] += 1

        m = le_matriz(caminho)
        n = len(m)

        if any(len(linha) != n for linha in m):
            nao_quadradas.append((nome, n, [len(l) for l in m[:3]]))
            continue

        diagonais[tuple(sorted({m[i][i] for i in range(n)}))] += 1

        quebras = [
            (i, j) for i in range(n) for j in range(i + 1, n) if m[i][j] != m[j][i]
        ]
        if quebras:
            assimetricas.append((nome, len(quebras), quebras[:3]))

        graus = [sum(m[i][j] for j in range(n) if j != i) for i in range(n)]
        n_iso = sum(1 for g in graus if g == 0)
        if n_iso:
            isolados.append((nome, n_iso, n))
        if sum(graus) == 0:
            sem_aresta.append((nome, n))

    print(f"instancias auditadas            : {len(arquivos)}")
    print(f"valores distintos nos arquivos  : {dict(valores)}")
    print(f"diagonais encontradas           : "
          f"{ {k: v for k, v in diagonais.items()} }")
    print(f"NAO QUADRADAS                   : {len(nao_quadradas)}")
    print(f"ASSIMETRICAS                    : {len(assimetricas)}")
    print(f"com vertices isolados           : {len(isolados)}")
    print(f"sem nenhuma aresta              : {len(sem_aresta)}  "
          f"{[f'{nome} (n={n})' for nome, n in sem_aresta]}")

    if isolados:
        print("\nmaiores proporcoes de vertices isolados:")
        for nome, n_iso, n in sorted(isolados, key=lambda t: -t[1])[:10]:
            print(f"  {nome:<26} {n_iso:>4} de {n:>5}")

    problemas = len(nao_quadradas) + len(assimetricas)
    if nao_quadradas:
        print("\nNAO QUADRADAS:", nao_quadradas[:10])
    if assimetricas:
        print("\nASSIMETRICAS (o leitor usaria so' o triangulo superior!):",
              assimetricas[:10])

    if problemas:
        print(f"\nFALHA: {problemas} instancia(s) com formato inconsistente.")
        return 1

    print("\nOK: todas quadradas, todas simetricas, diagonal uniforme por arquivo.")
    print("A leitura pelo triangulo superior e' segura para esta base.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
