#!/usr/bin/env python3
"""sorteia_treino.py: sorteia as instancias de TREINO do irace.

Separacao entre treino e teste, decidida em 02/11/2026:

  - 10% das 287 instancias unicas vao para o treino (29 instancias);
  - o sorteio e' feito SOMENTE entre instancias com n >= 30: nas menores,
    todas as configuracoes atingem o otimo em segundos e empatam, e o irace
    gastaria orcamento sem aprender nada;
  - o sorteio e' ESTRATIFICADO por classe, proporcional ao tamanho de cada
    classe no benchmark (metodo do maior resto), para o treino representar o
    teste;
  - a semente e' FIXA, para o sorteio ser reprodutivel e auditavel;
  - as instancias de treino SAEM da bateria final (run_experiment.sh le a
    lista e as pula), evitando o vies de reportar resultado em instancia
    usada na calibracao (over-tuning).

As duplicatas gn-* das geoinstances nao participam: nao fazem parte do
benchmark efetivo.

Saida: irace/treino.txt, uma instancia por linha, com caminho relativo ao
diretorio irace/ (de onde o irace e' executado).

Uso:
    python3 scripts/sorteia_treino.py           # grava irace/treino.txt
    python3 scripts/sorteia_treino.py --mostrar # so' mostra, nao grava
"""

import csv
import random
import sys
from collections import Counter, defaultdict
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
REFERENCIA = RAIZ / "results" / "baseline" / "pli_todas.csv"
SAIDA = RAIZ / "irace" / "treino.txt"

FRACAO = 0.10
N_MINIMO = 30
SEMENTE = 20261102


def faixa(n):
    if n < 30:
        return "300 s"
    if n < 100:
        return "600 s"
    return "1200 s"


def main():
    so_mostrar = "--mostrar" in sys.argv

    with REFERENCIA.open() as fh:
        instancias = [
            (linha["Class"], linha["Instance"], int(linha["NumNodes"]))
            for linha in csv.DictReader(fh)
            if not linha["Instance"].startswith("gn-")
        ]

    total = len(instancias)
    alvo = round(FRACAO * total)

    tamanho_classe = Counter(c for c, _, _ in instancias)

    # Cota por classe pelo metodo do maior resto: parte inteira primeiro,
    # depois as vagas restantes para as classes de maior parte fracionaria.
    exato = {c: alvo * k / total for c, k in tamanho_classe.items()}
    cota = {c: int(v) for c, v in exato.items()}
    sobra = alvo - sum(cota.values())
    for c in sorted(exato, key=lambda c: exato[c] - cota[c], reverse=True)[:sobra]:
        cota[c] += 1

    # Candidatas: n >= 30, em ordem fixa (para o sorteio nao depender da ordem
    # em que o sistema de arquivos lista os arquivos).
    candidatas = defaultdict(list)
    for classe, nome, n in sorted(instancias):
        if n >= N_MINIMO:
            candidatas[classe].append((nome, n))

    rng = random.Random(SEMENTE)
    treino = []
    for classe in sorted(cota):
        if cota[classe] > len(candidatas[classe]):
            sys.exit(f"ERRO: {classe} tem so' {len(candidatas[classe])} candidatas"
                     f" para {cota[classe]} vagas")
        for nome, n in sorted(rng.sample(candidatas[classe], cota[classe])):
            treino.append((classe, nome, n))

    # ---- relatorio --------------------------------------------------------
    print(f"instancias unicas no benchmark : {total}")
    print(f"fracao para treino             : {FRACAO:.0%} -> {alvo} instancias")
    print(f"sorteio entre                  : n >= {N_MINIMO}")
    print(f"semente                        : {SEMENTE}\n")

    print(f"{'classe':<20}{'no benchmark':>14}{'candidatas':>12}"
          f"{'treino':>8}{'teste':>8}")
    print("-" * 62)
    for classe in sorted(cota):
        print(f"{classe:<20}{tamanho_classe[classe]:>14}"
              f"{len(candidatas[classe]):>12}{cota[classe]:>8}"
              f"{tamanho_classe[classe] - cota[classe]:>8}")
    print("-" * 62)
    print(f"{'total':<20}{total:>14}"
          f"{sum(len(v) for v in candidatas.values()):>12}"
          f"{alvo:>8}{total - alvo:>8}")

    print(f"\nfaixas de tempo no treino: "
          f"{dict(Counter(faixa(n) for _, _, n in treino))}")

    print("\ninstancias de treino:")
    for classe, nome, n in treino:
        print(f"  {classe:<20}{nome:<28}n = {n}")

    if so_mostrar:
        print("\n(--mostrar: nada foi gravado)")
        return 0

    SAIDA.parent.mkdir(parents=True, exist_ok=True)
    with SAIDA.open("w") as fh:
        for classe, nome, _ in treino:
            fh.write(f"../instances/{classe}/{nome}.txt\n")
    print(f"\ngravado: {SAIDA.relative_to(RAIZ)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
