# Modelo de programação linear inteira (PLI)

Este diretório é uma **cópia não modificada** do repositório do Prof. Dr. Atílio
Gomes Luiz, incluída aqui para reprodutibilidade dos experimentos do TCC 2.

| | |
|---|---|
| Origem | https://github.com/Atilio-Luiz/dominacao_romana_2forte |
| Autor | Atílio Gomes Luiz |
| Licença | GNU GPL-3.0 (texto integral em `pli/LICENSE`) |
| Commit copiado | `d350447` — 2026-10-09 |
| Data da cópia | 2026-10-29 |

## O que foi copiado

- `greedy/Greedy_algorithm.jl` — heurística gulosa, usada como *warm start* do PLI
- `integer_linear_program/pli.jl` — modelo em JuMP + CPLEX
- `integer_linear_program/run_all.sh` — script original do autor, preservado como veio
- `integer_linear_program/docs/model.pdf` — formulação
- `integer_linear_program/output/` — resultados do autor, 377 CSVs
- `LICENSE`, `UPSTREAM-README.md`

## O que NÃO foi copiado, e por quê

O diretório `input/` do repositório de origem (377 instâncias, ~110 MB) foi
omitido: suas instâncias são **byte a byte idênticas** às de `instances/` deste
repositório (verificado por MD5, ignorando fim de linha). Manter uma única
cópia evita divergência entre as duas e 110 MB de redundância. O script
`scripts/run_pli.sh` deste projeto roda o PLI lendo direto de `instances/`.

## Nenhum arquivo deste diretório foi modificado

Qualquer adaptação necessária foi feita em `scripts/`, fora daqui. A GPL-3.0
cobre os arquivos deste diretório; as implementações BRKGA são obra separada,
apenas agregada no mesmo repositório, não derivada.

## Leitura do grafo

A função `read_simple_graph` de `pli.jl` (versão de 09/10/2026) define o
contrato de leitura que o lado C++ deve reproduzir exatamente, para que PLI e
BRKGA operem sobre o mesmo grafo:

- número de vértices = número de linhas **não vazias**;
- valores lidos como `Float64`, aceitando tanto `1` quanto `1.0`;
- **qualquer valor diferente de zero é uma aresta**;
- a **diagonal é ignorada**, mesmo quando contém valores não nulos;
- linha com número de valores diferente de *n* é erro.

As duas convenções de diagonal presentes nas instâncias deste repositório:

| Classe | Diagonal | Tipo no arquivo |
|---|---|---|
| `random`, `wireless-networks` | `1.0` | ponto flutuante |
| `random_cubic`, `harwell-boeing`, `geoinstances` | `0` | inteiro |

## Resultados do autor

`output/<classe>/<instancia>.csv`, com o cabeçalho:

```
G,|V|,|E|,density,maxDegree,minDegree,weight,time(milliseconds),status
```

Limite de **10 minutos** por instância (`TIME_LIMIT_MINUTES` em `pli.jl`),
CPLEX com *warm start* guloso. Dos 377 resultados: **297 `OPTIMAL`** e
**80 `TIME_LIMIT`**, assim distribuídos:

| Classe | CSVs | OPTIMAL | TIME_LIMIT |
|---|---|---|---|
| `random` | 35 | 35 | 0 |
| `wireless-networks` | 20 | 20 | 0 |
| `geoinstances` | 258 | 229 | 29 |
| `harwell-boeing` | 38 | 10 | 28 |
| `random_cubic` | 26 | 3 | 23 |

**Atenção ao usar esses números como referência:** nas linhas `TIME_LIMIT`, o
campo `weight` é a melhor solução viável encontrada em 10 minutos, **não o
ótimo**, e o CSV não registra o *dual bound* — nessas 80 instâncias não há gap
de otimalidade comprovado. Apenas as 297 linhas `OPTIMAL` servem como valor
ótimo para cálculo de gap.
