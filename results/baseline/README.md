# Referências de comparação

Dois arquivos consolidados, usados por `scripts/analisa_bateria.py`. **Nenhum
deles contém resultado nosso** — são as referências contra as quais o BRKGA é
comparado.

## `pli_todas.csv` — referência exata (287 instâncias)

Consolidação dos 377 CSVs de `pli/integer_linear_program/output/`, **excluídas
as 90 duplicatas `gn-*`** das geoinstances.

| Coluna | Significado |
|---|---|
| `Weight` | peso da solução encontrada pelo CPLEX |
| `Status` | `OPTIMAL` ou `TIME_LIMIT` |
| `TimeMs` | tempo do solver, em milissegundos |

**Atenção:** nas linhas `TIME_LIMIT` (80 das 377; 10 min de limite), `Weight` é
a melhor solução viável encontrada, **não o ótimo**, e não há *dual bound*
registrado. Só as linhas `OPTIMAL` servem para calcular gap.

Procedência: Prof. Dr. Atílio Gomes Luiz,
`github.com/Atilio-Luiz/dominacao_romana_2forte`, commit `d350447`
(2026-10-09). Ver `pli/README.md`.

## `vns_k2.csv` — baseline heurística (223 instâncias)

Extraído dos resultados brutos de `github.com/StefanKapunac/ksrdp`,
`results/raw/`, **filtrando apenas k = 2** (o artigo cobre k ∈ {2,3,4,5} e, nas
geoinstances, também 6 e 8).

| Coluna | Significado |
|---|---|
| `PLIWeight`, `PLIStatus` | o mesmo do arquivo acima, para conveniência |
| `ILPTheirs`, `ILPTheirsStatus` | ILP **deles**; status `2` = ótimo provado, `0` = limite de tempo |
| `GreedyTheirs` | guloso **deles**, execução única determinística |
| `VNSRuns` | execuções por instância: **10** em random e wireless, **1** em geoinstances |
| `VNSBest`, `VNSMean`, `VNSWorst` | sobre essas execuções |
| `VNSMeanTime`, `VNSMeanBestTime` | tempo total e tempo até a melhor solução, em segundos |

Cobertura: 35 `random`, 20 `wireless-networks`, 168 `geoinstances`. As classes
`harwell-boeing` e `random_cubic` **não têm baseline do VNS** — para elas a
única referência é o PLI.

### Duas inconsistências conhecidas, documentadas

1. **`ad-hoc-net_150_0.5` e `ad-hoc-net_150_0.6`**: o VNS publicado é **pior
   que o guloso deles** (12 contra 11, e 12 contra 9). Um VNS iniciado pela
   própria construção gulosa não pode terminar pior que ela; provavelmente os
   dois arquivos de resultado vêm de configurações diferentes. Nessas duas
   instâncias o valor de VNS não deve ser usado como baseline sem ressalva.

2. **As 4 instâncias `ad-hoc-net_150_*`** estão nos dados brutos do
   repositório, mas o *texto* do artigo descreve o conjunto wireless com 16
   instâncias (`n ∈ {20,30,50,100}`). Usá-las significa usar dados do
   repositório oficial que podem não constar das tabelas publicadas.

Detalhes e a validação cruzada entre as duas referências (177 instâncias com
ótimo provado pelos dois modelos, **todas coincidentes**) estão em
`claude/protocolo_experimental_fechado.md` e
`claude/guloso_e_anomalias_do_baseline.md`.

## Como foram gerados

Consolidação programática a partir dos dois repositórios de origem, sem
nenhuma edição manual. Os valores podem ser reconferidos clonando
`StefanKapunac/ksrdp` e `Atilio-Luiz/dominacao_romana_2forte` e refazendo o
filtro de k = 2.
