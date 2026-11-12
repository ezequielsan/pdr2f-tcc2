#!/usr/bin/env python3
"""analisa_piloto.py: categorias de tamanho e limite de geracoes por braco.

Le results/piloto/execucoes.csv e as curvas de convergencia em
results/piloto/convergencia/<braco>/<instancia>_s<semente>.csv (geradas por
scripts/piloto.sh, nas 86 instancias de TREINO).

PARTE 1 - limite de geracoes (curvas cortadas em 600 s, o limite da bateria).
  Para cada conjunto de cortes de categoria e cada regra, o limite de uma
  categoria e' um quantil da "geracao da ultima melhora ate' 600 s" das
  execucoes daquela categoria e braco. Para cada combinacao, simula-se a
  bateria do piloto com esse limite e mede-se:
    - PERDA: quantas execucoes terminariam com peso pior que o de 600 s, e
      a soma dessas diferencas;
    - ECONOMIA: fracao do tempo de CPU poupado.
  O tempo em que uma execucao atinge uma geracao e' estimado por
  interpolacao linear entre os pontos da curva (geracao, tempo).

PARTE 2 - mais tempo: quantas execucoes ainda melhoram depois de 600 s
  (ate' 800, 1200 e 1800 s) e quanto.

TEMPO REAL: no piloto, o main_complete media o tempo com o relogio monotono
da WSL, que sob carga prolongada atrasou ~10% em relacao ao tempo real
(conferido pelos horarios dos arquivos, que vem do Windows). As execucoes
paradas por tempo terminaram pela rede de seguranca da API, aos 1860 s de
relogio de parede (= tempo real). Por isso todos os tempos sao convertidos
para tempo real: fator = 1860 / TotalTime em cada execucao parada assim; nas
demais (paradas no alvo, quase sempre em menos de 1 s), a mediana do braco.

Uso:  python3 scripts/analisa_piloto.py [--limite-bateria 600]
Saidas: relatorio na tela e em results/piloto/relatorio.txt.
"""
import bisect
import collections
import csv
import math
import statistics as st
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
PIL = RAIZ / "results" / "piloto"
T = 600.0
if "--limite-bateria" in sys.argv:
    T = float(sys.argv[sys.argv.index("--limite-bateria") + 1])
BRACOS = ["brkga-mp", "brkga-mp-ipr"]
saida = []


def out(s=""):
    print(s)
    saida.append(s)


def quantil(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, max(0, math.ceil(q * len(v)) - 1))]


class Execucao:
    def __init__(self, linha):
        self.inst = linha["Instance"]
        self.classe = linha["Class"]
        self.n = int(linha["NumNodes"])
        self.braco = linha["Algorithm"]
        self.semente = linha["Seed"]
        self.motivo = linha["StopReason"]
        self.total = float(linha["TotalTime"])
        arq = PIL / "convergencia" / self.braco / f"{self.inst}_s{self.semente}.csv"
        self.melhoras, self.fim = [], None
        for r in csv.DictReader(arq.open()):
            p = (int(r["Generation"]), float(r["Time"]), int(r["Weight"]))
            if r["Event"] == "improvement":
                self.melhoras.append(p)
            elif r["Event"] == "end":
                self.fim = p
        self.fator = 1.0
        self.tempos = [t for _, t, _ in self.melhoras]

    def converte(self, fator):
        """Passa os tempos do relogio monotono para tempo real."""
        self.fator = fator
        self.melhoras = [(g, t * fator, w) for g, t, w in self.melhoras]
        self.fim = (self.fim[0], self.fim[1] * fator, self.fim[2])
        self.total *= fator
        self.tempos = [t for _, t, _ in self.melhoras]

    def peso_em(self, t):
        """Peso da melhor solucao no instante t (None se ainda nao havia)."""
        i = bisect.bisect_right(self.tempos, t)
        return self.melhoras[i - 1][2] if i else None

    def ger_ultima_ate(self, t):
        i = bisect.bisect_right(self.tempos, t)
        return self.melhoras[i - 1][0] if i else 0

    def tempo_da_geracao(self, g):
        """Instante estimado em que a geracao g foi atingida (None se nao foi)."""
        pts = [(0, 0.0)] + [(a, b) for a, b, _ in self.melhoras] + [(self.fim[0], self.fim[1])]
        if g > pts[-1][0]:
            return None
        for (g0, t0), (g1, t1) in zip(pts, pts[1:]):
            if g0 <= g <= g1:
                return t0 if g1 == g0 else t0 + (t1 - t0) * (g - g0) / (g1 - g0)
        return pts[-1][1]

    def tempo_base(self):
        """Tempo usado com o limite de T s (sem limite de geracoes)."""
        return min(self.total, T)

    def com_limite(self, cap):
        """(peso, tempo) se a execucao parasse na geracao cap ou em T s.

        cap None = sem limite. Se a execucao parasse antes da primeira
        melhora, o peso devolvido e' o da primeira melhora (perda maxima
        observavel), para a perda nao passar despercebida."""
        if cap is None:
            return self.peso_em(T), self.tempo_base()
        tc = self.tempo_da_geracao(cap)
        tempo = self.tempo_base() if tc is None else min(tc, self.tempo_base())
        i = 0
        for k, (g, t, w) in enumerate(self.melhoras):
            if g <= cap and t <= T:
                i = k + 1
        peso = self.melhoras[i - 1][2] if i else self.melhoras[0][2]
        return peso, tempo


def cortes_tercis(ns):
    a, b = quantil(ns, 1 / 3), quantil(ns, 2 / 3)
    return (f"tercis ({a}/{b})", [a, b])


def categoria(n, cortes):
    for k, c in enumerate(cortes):
        if n <= c:
            return k
    return len(cortes)


def rotulo(cortes, k):
    if not cortes:
        return "todas"
    lo = 1 if k == 0 else cortes[k - 1] + 1
    return f"n<={cortes[0]}" if k == 0 else (f"{lo}-{cortes[k]}" if k < len(cortes) else f"n>={lo}")


def main():
    linhas = list(csv.DictReader((PIL / "execucoes.csv").open()))
    ex = [Execucao(l) for l in linhas]
    for b in BRACOS:
        api = [x for x in ex if x.braco == b and x.motivo == "time" and x.total < 1799]
        med = st.median([1860.0 / x.total for x in api]) if api else 1.0
        for x in ex:
            if x.braco == b:
                x.converte(1860.0 / x.total if x in api else med)
        out(f"  {b}: tempo real = monotono x {med:.3f} (mediana; {len(api)} execucoes paradas pela API)")
    out(f"piloto: {len(ex)} execucoes | limite da bateria simulado: {T:.0f} s")
    for b in BRACOS:
        e = [x for x in ex if x.braco == b]
        out(f"  {b}: {len(e)} execucoes | motivos {dict(collections.Counter(x.motivo for x in e))}")

    ns = sorted({x.n for x in ex if x.braco == BRACOS[0]})
    conjuntos = [("seus cortes (300/700)", [300, 700]),
                 ("artigo do VNS (29/99)", [29, 99]),
                 cortes_tercis([x.n for x in ex if x.braco == BRACOS[0]]),
                 ("categoria unica", [])]
    regras = [("p90", 0.90), ("p95", 0.95), ("p99", 0.99), ("max", 1.0), ("1,5 x max", None)]

    # ---------------- parte 1 ----------------
    out("\n" + "=" * 100)
    out(f" PARTE 1: LIMITE DE GERACOES (curvas cortadas em {T:.0f} s)")
    out("=" * 100)
    for b in BRACOS:
        e = [x for x in ex if x.braco == b]
        base = sum(x.tempo_base() for x in e)
        out(f"\n### {b}  (tempo de CPU sem limite de geracoes: {base / 3600:.1f} h)")
        for nome, cortes in conjuntos:
            ncat = len(cortes) + 1
            grupos = collections.defaultdict(list)
            for x in e:
                grupos[categoria(x.n, cortes)].append(x)
            desc = ", ".join(f"{rotulo(cortes, k)}: {len({x.inst for x in grupos[k]})} inst." for k in range(ncat))
            out(f"\n  {nome}  [{desc}]")
            out(f"    {'regra':<10}{'limites por categoria':<44}{'perdem':>8}{'soma perda':>12}{'max':>6}{'economia':>10}")
            for rn, q in regras:
                caps = {}
                for k in range(ncat):
                    g = [x.ger_ultima_ate(T) for x in grupos[k]]
                    if not g or max(g) == 0:
                        caps[k] = None          # sem melhoras dentro do corte: sem limite
                        continue
                    caps[k] = math.ceil(1.5 * max(g)) if q is None else quantil(g, q)
                perdem = soma = mx = 0
                novo = 0.0
                for x in e:
                    cap = caps[categoria(x.n, cortes)]
                    w0 = x.peso_em(T)
                    w, t = x.com_limite(cap)
                    novo += t
                    d = (w - w0) if (w is not None and w0 is not None) else 0
                    if d > 0:
                        perdem += 1; soma += d; mx = max(mx, d)
                cs = " / ".join("-" if caps[k] is None else str(caps[k]) for k in range(ncat))
                out(f"    {rn:<10}{cs:<44}{perdem:>8}{soma:>12}{mx:>6}{100 * (1 - novo / base):>9.1f}%")

    # ---------------- parte 2 ----------------
    out("\n" + "=" * 100)
    out(f" PARTE 2: MAIS TEMPO (execucoes que chegaram a {T:.0f} s sem atingir o alvo)")
    out("=" * 100)
    marcos = [800, 1200, 1800]
    for b in BRACOS:
        e = [x for x in ex if x.braco == b and x.total >= T - 1 and x.peso_em(T) is not None
             and not (x.motivo == "target" and x.total <= T)]
        out(f"\n### {b}: {len(e)} execucoes, {len({x.inst for x in e})} instancias")
        out(f"    {'ate':>6}{'melhoraram':>12}{'instancias':>12}{'ganho medio (peso)':>20}{'ganho medio (%)':>17}{'ganho max':>11}")
        for m in marcos:
            gan = [(x, x.peso_em(T) - x.peso_em(m)) for x in e]
            pos = [(x, d) for x, d in gan if d > 0]
            gm = st.mean([d for _, d in gan]) if gan else 0
            gp = st.mean([100 * d / x.peso_em(T) for x, d in gan]) if gan else 0
            out(f"    {m:>5}s{len(pos):>12}{len({x.inst for x, _ in pos}):>12}{gm:>20.2f}{gp:>16.2f}%{max([d for _, d in gan], default=0):>11}")
        por = collections.defaultdict(list)
        for x in e:
            por[(x.classe, x.inst, x.n)].append(x.peso_em(T) - x.peso_em(1800))
        top = sorted(por.items(), key=lambda kv: -st.mean(kv[1]))[:8]
        out(f"    maiores ganhos de {T:.0f} s para 1800 s (media das sementes): " +
            "; ".join(f"{i} (n={n}) {st.mean(v):.1f}" for (c, i, n), v in top))

    # ---------------- referencia: geracoes por segundo ----------------
    out("\n" + "=" * 100)
    out(" REFERENCIA: geracoes por segundo (execucoes paradas por tempo), por faixa de n")
    out("=" * 100)
    for b in BRACOS:
        e = [x for x in ex if x.braco == b and x.motivo == "time"]
        fx = collections.defaultdict(list)
        for x in e:
            fx[categoria(x.n, [100, 300, 700])].append(x.fim[0] / x.total)
        out(f"  {b}: " + " | ".join(f"{rotulo([100, 300, 700], k)}: mediana {st.median(v):.1f} ger/s ({len(v)} exec.)" for k, v in sorted(fx.items())))

    (PIL / "relatorio.txt").write_text("\n".join(saida) + "\n")
    print(f"\nrelatorio gravado em {PIL.relative_to(RAIZ)}/relatorio.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
