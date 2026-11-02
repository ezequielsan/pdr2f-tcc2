# Calibração com irace

Calibração dos parâmetros do **BRKGA-MP-IPR com decodificador por permutação e
busca local de redução**. O BRKGA-MP usa os mesmos valores, com
`ipr_interval = 0`, para que a diferença entre os dois braços meça apenas o
efeito do IPR.

## Separação entre treino e teste

As instâncias de calibração (`treino.txt`) **não entram na bateria final**:
reportar resultado numa instância usada para calibrar daria um número
otimista (*over-tuning*). O `scripts/run_experiment.sh` lê esta lista e pula
essas instâncias.

| | |
|---|---|
| Fração | 10% das 287 instâncias únicas → **29** |
| Sorteio | estratificado por classe (método do maior resto), só entre n ≥ 30 |
| Semente | `20261102` (fixa e documentada) |
| Gerador | `scripts/sorteia_treino.py` — reexecutá-lo produz a mesma lista |

Composição: 17 geoinstances, 4 harwell-boeing, 3 random, 3 random_cubic,
2 wireless-networks. São 14 instâncias da faixa de 600 s e 15 da de 1200 s.

A semente **não deve ser trocada** em função de quais instâncias foram
sorteadas: isso equivaleria a escolher o treino à mão.

## Parâmetros calibrados

| Parâmetro | Faixa |
|---|---|
| `population_size` | 50 – 1000 (escala logarítmica) |
| `elite_percentage` | 0,10 – 0,50 |
| `mutants_percentage` | 0,01 – 0,30 |
| `num_elite_parents` | 1 – 5 |
| `total_parents` | 2 – 8 |
| `bias_type` | CONSTANT, LINEAR, QUADRATIC, CUBIC, EXPONENTIAL, LOGINVERSE |
| `num_independent_populations` | 1 – 3 |
| `ipr_interval` | 10 – 200 |

Restrição (`forbidden.txt`): `num_elite_parents < total_parents`, exigida pela
API. Os demais parâmetros ficam fixos em `base.conf`.

## Orçamento

- **2000 execuções**, cada uma com **10% da faixa do protocolo**: 60 s nas
  instâncias de 600 s e 120 s nas de 1200 s.
- Média de ~91 s por execução: **~50 h de CPU, ~7 h de relógio** com 7
  processos em paralelo.

## Como rodar

A partir da raiz do repositório, depois de `make`:

```bash
./irace/roda_irace.sh --check   # valida os arquivos, sem executar nada
./irace/roda_irace.sh --teste   # fumaça: 300 execuções de 2 s, alguns minutos
./irace/roda_irace.sh           # calibração completa, ~7 h
```

Rode a calibração completa num clone em `~/`, não em `/mnt/c`, e dentro do
`tmux`.

## Saídas (em `exec/`, não versionadas)

| Arquivo | Conteúdo |
|---|---|
| `irace.log` | saída completa do irace, com as configurações de elite ao final |
| `irace.Rdata` | todos os dados da calibração, para análise no R |
| `execucoes.csv` | uma linha por execução do BRKGA, com as métricas do `main_complete` |

Ao final, a melhor configuração vira `configs/brkga-mp-ipr/final_perm.conf`
e, com `ipr_interval 0`, `configs/brkga-mp/final_perm.conf`. O `irace.Rdata`
e o log da calibração definitiva devem ser preservados no repositório, para
reprodutibilidade.

## Referência

López-Ibáñez, M.; Dubois-Lacoste, J.; Pérez Cáceres, L.; Stützle, T.;
Birattari, M. The irace package: Iterated Racing for Automatic Algorithm
Configuration. *Operations Research Perspectives*, 2016.
DOI: 10.1016/j.orp.2016.09.002.
