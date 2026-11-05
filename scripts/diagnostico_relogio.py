#!/usr/bin/env python3
"""diagnostico_relogio.py: detecta saltos do relogio e pausas da WSL.

Roda 6 processos ocupados (como o irace) e, num processo separado, acorda a
cada 50 ms comparando dois relogios:

  - time.time()       relogio de parede (o que a API BRKGA usa: system_clock)
  - time.monotonic()  relogio monotono (nunca salta)

Interpretacao de cada intervalo anormal (> 0,5 s):
  - SALTO DO RELOGIO: o relogio de parede avancou muito, o monotono nao.
  - PAUSA: os dois avancaram juntos -> o processo ficou sem CPU.

Uso:  python3 scripts/diagnostico_relogio.py [segundos]   (padrao: 60)
"""
import multiprocessing as mp
import sys
import time


def ocupado(fim):
    x = 0
    while time.monotonic() < fim:
        x += 1


def main():
    duracao = float(sys.argv[1]) if len(sys.argv) > 1 else 60.0
    fim = time.monotonic() + duracao
    procs = [mp.Process(target=ocupado, args=(fim,)) for _ in range(6)]
    for p in procs:
        p.start()

    saltos, pausas, n = [], [], 0
    t0w, t0m = time.time(), time.monotonic()
    ant_w, ant_m = t0w, t0m
    while time.monotonic() < fim:
        time.sleep(0.05)
        w, m = time.time(), time.monotonic()
        dw, dm = w - ant_w, m - ant_m
        n += 1
        if abs(dw - dm) > 0.5:
            saltos.append((round(m - t0m, 1), round(dw, 2), round(dm, 2)))
        elif dm > 0.5:
            pausas.append((round(m - t0m, 1), round(dm, 2)))
        ant_w, ant_m = w, m

    for p in procs:
        p.join()

    print(f"{n} medicoes em {duracao:.0f} s, com 6 processos ocupados")
    print(f"deriva total parede - monotono: {(time.time() - t0w) - (time.monotonic() - t0m):+.3f} s")
    print(f"SALTOS do relogio de parede: {len(saltos)}  (instante s, avanco parede s, avanco monotono s)")
    for s in saltos[:10]:
        print("   ", s)
    print(f"PAUSAS (processo sem CPU > 0,5 s): {len(pausas)}  (instante s, duracao s)")
    for p in pausas[:10]:
        print("   ", p)


if __name__ == "__main__":
    main()
