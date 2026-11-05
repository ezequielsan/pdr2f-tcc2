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
| Custo | `peso + 0,999 × t_melhor / 120`: o peso decide, o tempo até a melhor solução desempata |
| Orçamento | 4000 execuções por calibração |
| Paralelismo | 6 processos, 1 thread cada |
| Semente do irace | `20261102` nas duas |

## Parâmetros calibrados

| Parâmetro | Faixa |
|---|---|
| `population_size` | 50 – 2000 (escala logarítmica) |
| `elite_percentage` | 0,10 – 0,50 |
| `mutants_percentage` | 0,01 – 0,30 |
| `num_elite_parents` | 1 – 5 |
| `total_parents` | 2 – 10 |
| `bias_type` | CONSTANT, LINEAR, QUADRATIC, CUBIC, EXPONENTIAL, LOGINVERSE |
| `num_independent_populations` | 1 – 5 |
| `ipr_interval` | 10 – 200 (só no BRKGA-MP-IPR) |

Restrições (seção `[forbidden]`): `num_elite_parents < total_parents`, e a
elite precisa ter ao menos `num_elite_parents` indivíduos. Os demais
parâmetros ficam fixos nas bases.

## Tempo

Pior caso, se nenhuma execução parar antes dos 120 s: 4000 × 120 s = 133 h de
CPU por calibração, **~24 h de relógio** com 6 processos, **~49 h as duas**.
Na prática deve ser menos, porque muitas execuções param no alvo.

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
