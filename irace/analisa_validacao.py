#!/usr/bin/env python3
"""analisa_validacao.py: escolhe a configuracao final de cada braco.

Le irace/validacao/execucoes.csv (gerado por valida_finalistas.sh).

REGRA, fixada antes de ver os resultados:
  1. em cada bloco (instancia x semente), as finalistas do braco sao
     ordenadas pelo custo do irace: peso + 0,999 * t_melhor / 120
     (empates recebem o rank medio);
  2. vence a finalista de MENOR RANK MEDIO sobre todos os blocos completos.

Tambem reporta, sem mudar a regra:
  - teste de Friedman (as finalistas diferem?) e a diferenca critica de
    Nemenyi (alfa = 0,05): se a vencedora nao fica a mais de 1 CD das outras,
    a diferenca entre elas nao e' estatisticamente significativa;
  - rank medio por classe, quantos alvos atingidos e soma dos pesos.

Uso:  python3 irace/analisa_validacao.py
"""
import collections
import csv
import math
import sys
from pathlib import Path

ARQ = Path(__file__).resolve().parent / "validacao" / "execucoes.csv"

# Valores criticos q_0,05 de Nemenyi (Demsar, 2006, tabela 5a), por k.
Q005 = {2: 1.960, 3: 2.343, 4: 2.569, 5: 2.728, 6: 2.850, 7: 2.949, 8: 3.031, 9: 3.102, 10: 3.164}


def gammq(a, x):
    """Funcao gama incompleta superior regularizada Q(a, x)."""
    if x <= 0:
        return 1.0
    if x < a + 1:                                   # serie
        soma = termo = 1.0 / a
        ap = a
        for _ in range(1000):
            ap += 1
            termo *= x / ap
            soma += termo
            if abs(termo) < abs(soma) * 1e-14:
                break
        return 1.0 - soma * math.exp(-x + a * math.log(x) - math.lgamma(a))
    b = x + 1 - a                                   # fracao continuada
    c, d = 1e300, 1.0 / b
    h = d
    for i in range(1, 1000):
        an = -i * (i - a)
        b += 2
        d = an * d + b
        d = 1e-300 if abs(d) < 1e-300 else d
        c = b + an / c
        c = 1e-300 if abs(c) < 1e-300 else c
        d = 1.0 / d
        h *= d * c
        if abs(d * c - 1) < 1e-14:
            break
    return math.exp(-x + a * math.log(x) - math.lgamma(a)) * h


def ranks(valores):
    ordem = sorted(range(len(valores)), key=lambda i: valores[i])
    r = [0.0] * len(valores)
    i = 0
    while i < len(ordem):
        j = i
        while j + 1 < len(ordem) and valores[ordem[j + 1]] == valores[ordem[i]]:
            j += 1
        for k in range(i, j + 1):
            r[ordem[k]] = (i + j) / 2 + 1
        i = j + 1
    return r


def main():
    if not ARQ.exists():
        print(f"{ARQ} nao existe; rode irace/valida_finalistas.sh")
        return 1
    linhas = list(csv.DictReader(ARQ.open()))
    inviaveis = [l for l in linhas if l["Feasible"] != "yes"]
    abaixo = [l for l in linhas if int(l["Target"]) > 0 and int(l["Weight"]) < int(l["Target"])]
    print(f"execucoes: {len(linhas)} | inviaveis: {len(inviaveis)} | abaixo do alvo: {len(abaixo)}")

    for braco in sorted({l["Arm"] for l in linhas}):
        L = [l for l in linhas if l["Arm"] == braco]
        configs = list(dict.fromkeys(l["ConfigId"] for l in L))  # ordem do irace
        blocos = collections.defaultdict(dict)
        info = {}
        for l in L:
            chave = (l["Instance"], l["Seed"])
            blocos[chave][l["ConfigId"]] = l
            info[l["Instance"]] = l["Class"]
        completos = {k: v for k, v in blocos.items() if len(v) == len(configs)}

        soma = collections.defaultdict(float)
        por_classe = collections.defaultdict(lambda: collections.defaultdict(list))
        for (inst, _), v in completos.items():
            r = ranks([float(v[c]["Cost"]) for c in configs])
            for c, rc in zip(configs, r):
                soma[c] += rc
                por_classe[c][info[inst]].append(rc)

        N, k = len(completos), len(configs)
        medio = {c: soma[c] / N for c in configs} if N else {}
        print(f"\n=== {braco}: {k} finalistas, {N} blocos completos "
              f"({len({i for i, _ in completos})} instancias) de {len(blocos)} ===")
        if N == 0:
            continue

        # Friedman (Demsar, 2006).
        chi2 = 12 * N / (k * (k + 1)) * (sum(m * m for m in medio.values()) - k * (k + 1) ** 2 / 4)
        p = gammq((k - 1) / 2, chi2 / 2)
        cd = Q005.get(k, float("nan")) * math.sqrt(k * (k + 1) / (6 * N))
        print(f"Friedman: chi2 = {chi2:.2f}, p = {p:.4f} | diferenca critica de Nemenyi (0,05): {cd:.3f}")

        classes = sorted({c for d in por_classe.values() for c in d})
        cab = f"{'config':>7}{'rank medio':>12}" + "".join(f"{c[:11]:>13}" for c in classes) + f"{'no alvo':>9}{'soma pesos':>12}"
        print(cab)
        vencedora = min(configs, key=lambda c: medio[c])
        for c in sorted(configs, key=lambda c: medio[c]):
            alvos = sum(1 for v in completos.values() if v[c]["StopReason"] == "target")
            pesos = sum(int(v[c]["Weight"]) for v in completos.values())
            linha = f"{c:>7}{medio[c]:>12.3f}" + "".join(
                f"{sum(por_classe[c][cl]) / len(por_classe[c][cl]):>13.2f}" for cl in classes)
            print(linha + f"{alvos:>9}{pesos:>12}" + ("   <- vencedora" if c == vencedora else ""))
        segunda = sorted(configs, key=lambda c: medio[c])[1]
        dif = medio[segunda] - medio[vencedora]
        print(f"vencedora: {vencedora} | vantagem sobre a 2a ({segunda}): {dif:.3f} "
              f"{'> CD: significativa' if dif > cd else '<= CD: NAO significativa'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
