# Calibração com irace

**Duas calibrações independentes**, uma para cada braço, no mesmo cenário
(protocolo de 05/11/2026):

| Braço | Parâmetros | Base | Saída |
|---|---|---|---|
| BRKGA-MP-IPR | `parametros-ipr.txt` (8) | `base-ipr.conf` | `exec-ipr/` |
| BRKGA-MP | `parametros-mp.txt` (7, sem IPR) | `base-mp.conf` (`ipr_interval 0`) | `exec-mp/` |

As duas usam as mesmas 86 instâncias, o mesmo tempo, o mesmo custo, o mesmo
orçamento e a mesma semente. Assim nenhum braço recebe mais esforço de
calibração que o outro, e cada algoritmo é comparado na sua melhor
configuração.

## Separação entre treino e teste

As instâncias de calibração (`treino.txt`) **não entram na bateria final**,
para não reportar resultado numa instância usada para escolher os parâmetros
(*over-tuning*). O `scripts/run_experiment.sh` lê esta lista e pula essas
instâncias.

| | |
|---|---|
| Fração | 30% das 287 instâncias únicas → **86** (as outras 201 são o teste) |
| Sorteio | estratificado por classe (método do maior resto), só entre n ≥ 30 |
| Semente | `20261102` (fixa); `scripts/sorteia_treino.py` reproduz a lista |
| Composição | 50 geoinstances, 11 harwell-boeing, 11 random, 8 random_cubic, 6 wireless-networks |

## Cenário

| Item | Valor |
|---|---|
| Tempo por execução | **120 s** (20% dos 600 s da bateria) |
| Parada | 120 s ou alvo atingido (`results/baseline/alvos.csv`: ótimo provado pelo PLI ou ⌈2n/3⌉ nas cúbicas) |
| Custo | **o peso** da melhor solução (sem desempate por tempo; ver abaixo) |
| Orçamento | 8000 execuções por calibração |
| `elitistLimit` | **0** (sem limite de testes seguidos sem eliminação; ver abaixo) |
| Paralelismo | 6 processos, 1 thread cada |
| Semente do irace | `20261102` nas duas |

## Parâmetros calibrados

| Parâmetro | Faixa |
|---|---|
| `population_size` | 10 – 2000 (escala logarítmica) |
| `elite_percentage` | 0,10 – 0,50 |
| `mutants_percentage` | 0,01 – 0,30 |
| `num_elite_parents` | 1 – 5 |
| `total_parents` | 2 – 15 |
| `bias_type` | CONSTANT, LINEAR, QUADRATIC, CUBIC, EXPONENTIAL, LOGINVERSE |
| `num_independent_populations` | 1 – 5 |
| `ipr_interval` | 10 – 200 (só no BRKGA-MP-IPR) |

Restrições (seção `[forbidden]`): `num_elite_parents < total_parents`; a
elite precisa ter ao menos `num_elite_parents` indivíduos; e
`elite + total_parents − num_elite_parents ≤ population_size`. A última não é
verificada pela API, que toma os pais não-elite por posição e leria fora do
vetor. Os demais parâmetros ficam fixos nas bases.

## Histórico das calibrações

| Rodada | Treino | Orçamento | `elitistLimit` | Instâncias usadas | Pasta |
|---|---|---|---|---|---|
| 1ª (02–03/11) | 10% (29) | 2000, só IPR | 2 | 13 | `historico/2026-11-02_treino10pct/` |
| 2ª (05–06/11) | 30% (86) | 4000 por braço | 2 | 16 (IPR), 20 (MP) | `historico/2026-11-05_treino30pct_limite2/` |
| 3ª | 30% (86) | 8000 por braço | **0** | — | `exec-ipr/`, `exec-mp/` |

Na 2ª rodada, a população vencedora do BRKGA-MP (51) ficou no limite da
faixa antiga (50–2000) e a cobertura de instâncias foi baixa. A simulação em
`simulacao/simula_cobertura.sh`, com o irace verdadeiro e um avaliador
sintético, mostrou que o limite vinha de `elitistLimit = 2` e não do
orçamento: com `elitistLimit = 0`, a configuração vencedora passa pelas 86
instâncias.

**Custo só pelo peso (desde a 3ª rodada).** A 2ª rodada usou
`peso + 0,999 × t_melhor / 120`, para o tempo desempatar. Como o irace compara
por ranks, qualquer diferença de tempo vale uma posição inteira, tanto quanto
uma solução melhor. Na validação das finalistas nas 86 instâncias
(`valida_finalistas.sh`), todas tinham o mesmo peso em 67% dos blocos; ali só a
velocidade decidia, e a finalista de melhor qualidade (rank só pelo peso,
Friedman p = 0,001) ficou em último pelo custo. Com custo = peso, quem chega ao
mesmo peso empata, e a escolha é feita pela qualidade da solução.

## Tempo

Na 2ª rodada, cada execução levou em média ~9,5 s de relógio com 6 processos
(muitas param no alvo). Com 8000 execuções: **~21 h por braço, ~42 h as
duas**. Pior caso, se nenhuma parasse antes dos 120 s: ~49 h por braço.

## Preparação (uma vez por máquina)

O executável `irace` fica dentro do pacote do R. Para colocá-lo no PATH do
bash, acrescente ao `~/.bashrc`:

```bash
export IRACE_HOME="$(Rscript -e "cat(system.file(package='irace'))")"
export PATH="$IRACE_HOME/bin:$PATH"
```

e abra um terminal novo. `irace --version` deve mostrar 4.5.1.

## Como rodar

A partir da raiz do repositório, depois de `make main_complete`:

```bash
python3 scripts/gera_alvos.py        # gera results/baseline/alvos.csv
python3 scripts/sorteia_treino.py    # gera irace/treino.txt
./irace/roda_irace.sh --check        # valida os arquivos dos dois braços
./irace/roda_irace.sh --teste        # fumaça: 300 execuções de 2 s por braço
./irace/roda_irace.sh                # as duas calibrações, em sequência
```

Rode a calibração dentro do `tmux`. A segunda calibração (BRKGA-MP) começa
sozinha quando a primeira (BRKGA-MP-IPR) termina sem erro. Para rodar só uma:
`./irace/roda_irace.sh ipr` ou `./irace/roda_irace.sh mp`.

## Saídas (em `exec-<braço>/`, não versionadas)

| Arquivo | Conteúdo |
|---|---|
| `irace.log` | saída completa do irace, com as configurações de elite ao final |
| `irace.Rdata` | todos os dados da calibração, para análise no R |
| `execucoes.csv` | uma linha por execução do BRKGA, com o custo e as métricas do `main_complete` |

Ao final, a melhor configuração de cada braço vira o
`configs/<braço>/final_perm.conf`, e o log, o `irace.Rdata` e o
`execucoes.csv` de cada calibração são copiados para `historico/`, para
reprodutibilidade.

A calibração anterior (02–03/11/2026, 10% de treino, só o BRKGA-MP-IPR) está
em `historico/2026-11-02_treino10pct/`. Ela não vale mais, porque o conjunto
de treino mudou.

## Referência

López-Ibáñez, M.; Dubois-Lacoste, J.; Pérez Cáceres, L.; Birattari, M.;
Stützle, T. The irace package: Iterated Racing for Automatic Algorithm
Configuration. *Operations Research Perspectives*, 2016.
DOI: 10.1016/j.orp.2016.09.002.
