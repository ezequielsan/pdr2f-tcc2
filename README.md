# Meta-heurísticas para o Problema da Dominação Romana 2-Forte (PDR2F)

Implementações em C++ do **BRKGA-MP** e do **BRKGA-MP-IPR** para o PDR2F,
construídas sobre a API [BRKGA-MP-IPR](https://github.com/ceandrade/brkga_mp_ipr_cpp)
de C. E. Andrade. Os resultados são comparados com:

- a heurística **VNS** de Djukanović et al. (2025), pelos resultados
  **publicados** para k = 2 (o VNS não é reexecutado);
- o **modelo de programação linear inteira (PLI)** do Prof. Dr. Atílio Gomes
  Luiz, pelos resultados já existentes (o PLI também não é reexecutado).

> Trabalho de Conclusão de Curso 2 — Engenharia de Software, UFC Quixadá.
> Autor: Ezequiel dos Santos Melo. Orientador: Prof. Dr. Atílio Gomes Luiz.

## Situação atual (09/11/2026)

| Etapa | Situação |
|---|---|
| Leitor de instâncias, verificador de viabilidade, guloso | concluído e validado |
| Decodificadores e busca local de redução | concluído e validado |
| Integração com a API BRKGA-MP-IPR (`main_minimal`, `main_complete`) | concluído |
| Referências consolidadas (PLI e VNS k = 2) e alvos de parada | concluído (`results/baseline/`) |
| Protocolo experimental | fechado com o orientador em 04–06/11 (seção [Metodologia experimental](#metodologia-experimental)) |
| **Calibração com irace** (uma por braço) | **concluída**: 3ª rodada em 07–08/11, validada nas 86 instâncias de treino em 09/11 (seção [Calibração](#calibração)) |
| Configurações finais | **definidas**: `configs/brkga-mp/final_perm.conf` e `configs/brkga-mp-ipr/final_perm.conf` |
| Piloto (categorias e limite de gerações) | a executar |
| Bateria final | a executar (~28–42 h de relógio) |
| Análise estatística e texto | a fazer |

## Estrutura

```
src/          código compartilhado
  pdr2f/        leitor de instância, verificador de viabilidade, busca local, codificação
  heuristics/   guloso de Djukanović et al.
  decoders/     decodificadores (permutação; limiares, fora dos experimentos)
  distances/    função de distância para o IPR
  compat/       compatibilidade da API com GCC < 13
apps/         programas: main_minimal, main_complete, ferramentas *_info e testes
configs/      parâmetros: uma pasta por braço (dev_*.conf e final_perm.conf)
irace/        calibração: cenário, parâmetros e base por braço, target-runner,
              treino, validação das finalistas, simulação de cobertura e
              historico/ com as três rodadas
pli/          modelo de programação linear inteira (cópia de terceiros, ver pli/README.md)
instances/    instâncias, por classe
results/      referências consolidadas e alvos (baseline/) e saídas dos experimentos
scripts/      validações, sorteio do treino, alvos, diagnóstico do relógio, bateria e análise
external/     API BRKGA-MP-IPR (não versionada)
docs/         anotações e material de apoio
```

## Instâncias

As instâncias são **matrizes de adjacência**, uma linha por vértice. Qualquer
valor não nulo é aresta e a diagonal é ignorada; ver `pli/README.md` para o
contrato completo de leitura, que o lado C++ reproduz (validado campo a campo
contra o leitor do PLI).

| Classe | Instâncias únicas | Baseline VNS (k = 2) | Ótimo provado pelo PLI | Treino (30%) | Teste (70%) |
|---|---|---|---|---|---|
| `random` | 35 | sim | 35 | 11 | 24 |
| `wireless-networks` | 20 | sim | 20 | 6 | 14 |
| `geoinstances` | 168 | sim | 152 | 50 | 118 |
| `harwell-boeing` | 38 | não | 10 | 11 | 27 |
| `random_cubic` | 26 | não | 3 | 8 | 18 |
| **total** | **287** | **223** | **220** | **86** | **201** |

A pasta `instances/geoinstances/` tem 258 arquivos: além das 168 instâncias,
há **90 duplicatas com prefixo `gn-`**, que não fazem parte do benchmark
efetivo e ficam fora das contagens, do sorteio do treino e da bateria final.

Nas instâncias em que o PLI parou no limite de 10 min (`TIME_LIMIT`), o valor
registrado é apenas a melhor solução viável encontrada pelo CPLEX, **não o
ótimo**; só as 220 instâncias `OPTIMAL` servem para calcular gap.

## O que foi feito até agora

### 1. Base do problema (`src/pdr2f/`)

- **Leitor de instâncias** (`pdr2f_instance`): lê a matriz de adjacência,
  considera só o triângulo superior e protege contra linhas mais longas que o
  número de vértices (correção de uma falha de segmentação encontrada na
  validação). Validado contra o leitor do PLI: **1885 campos comparados nas
  377 instâncias, 0 divergências**; todas as matrizes são quadradas e
  simétricas.
- **Verificador de viabilidade** (`pdr2f_feasibility`), pela caracterização
  local do PDR2F com k = 2:
  - **C1:** `f(v) = 0` ⇒ `v` tem ao menos um vizinho protetor (rótulo ≥ 2);
  - **C2:** `f(v) = 2` ⇒ `v` é protetor exclusivo de no máximo um vértice de
    rótulo 0;
  - vértice isolado recebe rótulo 1.

  A caracterização foi conferida por **força bruta em 48 mil rotulações**.
  O verificador audita, de forma independente, toda solução produzida.
- **Busca local de redução** (`pdr2f_reduction`): tenta baixar rótulos
  mantendo a viabilidade, numa passada de ordem fixa e verificação
  incremental. Conferida contra uma implementação de referência em **32 mil
  rotulações**.

### 2. Guloso (`src/heuristics/`)

Reimplementação do guloso de Djukanović et al. (2025, §2.2), o mesmo usado como
construção inicial pelo VNS e como *warm start* pelo PLI. É viável por
construção e foi verificado em 377 instâncias × 10 sementes: zero soluções
inviáveis. Gap médio ao ótimo de **5,1%** (melhor de 10 desempates
aleatórios, sobre as instâncias com ótimo provado, contando as duplicatas
`gn-`).

### 3. Decodificadores (`src/decoders/`)

- **Permutação** (`pdr2f_perm_decoder`): as chaves definem a ordem em que os
  vértices são considerados, e a solução nasce viável. Seguido da busca local
  de redução. **É o decodificador usado nos experimentos.**
- **Limiares** (`pdr2f_threshold_decoder`, de Torres): cada chave vira um
  rótulo por limiares, com reparo. **Excluído dos experimentos**; o código e
  os testes permanecem no repositório.
- `pdr2f_brkga_decoder.hpp` adapta os decodificadores à interface da API.

Experimento sem evolução (cromossomos aleatórios, 10 sementes, sobre as
instâncias com ótimo provado, **contando as duplicatas `gn-`**, 297 no total):

| Variante | Gap médio ao ótimo | Instâncias no ótimo |
|---|---|---|
| limiares | 205,5% | 0 |
| limiares + redução | 19,3% | 40 |
| permutação | 23,8% | 34 |
| **permutação + redução** | **16,8%** | **46** |

Esse experimento fundamentou a escolha do decodificador por permutação com
busca local.

### 4. Integração com a API BRKGA-MP-IPR (`apps/`)

- `main_minimal`: versão mínima, para conferir a integração.
- `main_complete`: versão usada nas baterias e no irace. Imprime uma linha CSV
  de 29 campos por execução (peso, viabilidade auditada, gerações, tempos,
  chamadas e melhorias do IPR, alvo e **motivo da parada**). Para ao atingir o
  alvo (`--target`), ao chegar a um número de gerações (`--generations`) ou aos
  600 s (padrão; `--maxtime`), e grava a curva de convergência
  (`--convlog`). O rótulo `brkga-mp`/`brkga-mp-ipr` é deduzido de
  `ipr_interval`.
- **O tempo é medido com relógio monótono** (`std::chrono::steady_clock`). Na
  WSL, o relógio de parede recua ~30 s e volta 0,2 s depois, a cada ~24 s
  (`scripts/diagnostico_relogio.py`); como a API usa o relógio de parede, uma
  execução poderia parar ~30 s antes da hora. O `main_complete` impõe o limite
  pelo relógio monótono e corrige o orçamento de tempo que a API passa ao IPR,
  que de outro modo podia passar do limite em minutos.
- `src/distances/pdr2f_distance.hpp`: distância usada pelo IPR.
- `src/compat/brkga_compat.hpp`: permite compilar a API com GCC < 13; é inerte
  em compiladores novos.
- O `Makefile` compila com `-DMATING_SEED_ONLY`, para que o resultado dependa
  apenas da semente.

### 5. Testes e validações

| Comando | O que verifica |
|---|---|
| `./test_pdr2f` | 47 testes do verificador e do guloso |
| `./test_decoders` | 54 testes dos decodificadores e da busca local |
| `scripts/teste_leitor.sh`, `valida_formato.py`, `valida_leitor.py` | leitor contra o PLI e formato das instâncias |
| `scripts/valida_guloso.py` | viabilidade e gap do guloso |
| `scripts/valida_decoders.py` | viabilidade e gap dos decodificadores |
| `make test` | roda tudo |

### 6. Referências consolidadas (`results/baseline/`)

- `pli_todas.csv`: resultados do PLI nas 287 instâncias únicas.
- `vns_k2.csv`: resultados brutos do VNS (repositório `StefanKapunac/ksrdp`),
  filtrados para k = 2, em 223 instâncias.

Validação cruzada: nas **177 instâncias** em que o ILP do artigo e o PLI do
Prof. Atílio provam otimalidade, **os valores coincidem todos**. Nenhum valor
do VNS fica abaixo do ótimo provado.

Achados documentados (detalhes em `results/baseline/README.md`):

- o VNS publicado fica acima do ótimo provado em **41 instâncias**;
- em `ad-hoc-net_150_0.5` e `ad-hoc-net_150_0.6` o VNS publicado é **pior que
  o próprio guloso** do artigo (inconsistência a declarar no texto);
- o nosso guloso, com desempate aleatório, supera o VNS publicado em 12
  instâncias. Parte da diferença entre o BRKGA e o VNS pode vir do desempate
  da construção inicial, e isso precisa ser dito na comparação.

### 7. Alvos de parada (`results/baseline/alvos.csv`)

Gerado por `scripts/gera_alvos.py`. O BRKGA para ao atingir o alvo, o que não
muda nenhum resultado (ninguém melhora um ótimo) e só economiza tempo:

- **220 instâncias**: ótimo provado pelo PLI (status `OPTIMAL`; os valores
  `TIME_LIMIT` não entram, porque não são ótimos);
- **23 cúbicas** sem ótimo provado: limite inferior ⌈2n/3⌉ (Nikolić et al.,
  Teorema 1);
- **44 instâncias** sem alvo.

O script confere que nenhum resultado do PLI ou do VNS fica abaixo de um alvo.

## Metodologia experimental

### Protocolo (definido com o orientador em 04–06/11/2026)

| Item | Valor | Observação |
|---|---|---|
| Divisão da base | **30% treino (86) / 70% teste (201)** | sorteio estratificado por classe, só entre n ≥ 30, semente `20261102` (`scripts/sorteia_treino.py`); o teste não é usado na calibração |
| Braços | **BRKGA-MP** e **BRKGA-MP-IPR** | decodificador por permutação com busca local de redução; sem warm start |
| Calibração | **uma por braço**, com irace | cada algoritmo é comparado na sua melhor configuração |
| Execuções por instância | **10** (sementes 0–9) | as mesmas sementes nos dois braços (comparação pareada) |
| Parada | **ótimo provado**, **limite de gerações da categoria** ou **600 s**, o que vier primeiro | o limite de gerações sai do piloto (abaixo) |
| Execução | 1 thread por execução, 6 processos em paralelo | notebook Intel Core i5-1345U, WSL |
| Registro | CSV por execução + curva de convergência | uma linha a cada melhora da melhor solução |

O protocolo anterior seguia o artigo do VNS (300/600/1200 s conforme n; 10
execuções nas classes sintéticas e 1 nas reais). Com 600 s para todas, as
instâncias com n < 30 têm o dobro do tempo do VNS e as com n ≥ 100 a metade;
isso é declarado e os resultados são reportados também por faixa.

### Piloto: categorias e limite de gerações

Antes da bateria, as duas configurações finais rodam nas **86 instâncias de
treino**, com **5 sementes**, até **1800 s** e com a curva de convergência
ligada (860 execuções, ~26 h). Com as curvas:

1. cortadas em 600 s: define-se, por braço, o limite de gerações de cada
   categoria de tamanho (percentil 95 da geração da última melhora), e
   escolhem-se os cortes de categoria com perda zero e maior economia;
2. até 1800 s: mede-se quanto se ganharia com mais tempo (por exemplo, 800 s).

### Bateria final

201 instâncias de teste × 10 sementes × 2 braços = **4020 execuções**. Pior
caso ~112 h; estimativa ~28–42 h, a partir do tempo até o ótimo medido na
validação das configurações finais.

### Comparações previstas

1. **BRKGA-MP × BRKGA-MP-IPR**, pareado por semente.
2. **Contra o PLI**: gap ao ótimo nas 154 instâncias de teste com ótimo
   provado; nas demais, comparação com a melhor solução do CPLEX.
3. **Contra o VNS publicado** (k = 2), nas 156 instâncias de teste com
   baseline: teste de Friedman + pós-teste de Nemenyi (α = 0,05), como no
   artigo, com o guloso como linha de referência.
4. Gráficos: convergência (mediana das 10 sementes ao longo do tempo), boxplots
   dos valores finais e diagrama de diferença crítica.

## Calibração

Detalhes em `irace/README.md`; dados de todas as rodadas em `irace/historico/`.

### Cenário da rodada definitiva (3ª)

| Item | Valor |
|---|---|
| Ferramenta | irace 4.5.1, uma calibração por braço, mesma semente |
| Treino | as 86 instâncias de treino |
| Tempo por execução | 120 s, com parada no alvo |
| Custo | **o peso** da melhor solução |
| Orçamento | 8000 execuções por braço |
| `elitistLimit` | 0 |
| Parâmetros | população 10–2000 (log), elite 0,10–0,50, mutantes 0,01–0,30, pais elite 1–5, total de pais 2–15, 6 vieses, populações 1–5; `ipr_interval` 10–200 só no IPR |

### As três rodadas

| Rodada | Custo | `elitistLimit` | Orçamento | Instâncias usadas (IPR / MP) | Populações finais (IPR / MP) |
|---|---|---|---|---|---|
| 1ª (02–03/11, Ryzen, 10% de treino, só IPR) | peso | 2 | 2000 | 13 | 556–992 |
| 2ª (05–06/11) | peso + 0,999·t/120 | 2 | 4000 | 16 / 20 | 64–94 / 51–79 |
| **3ª (07–08/11)** | **peso** | **0** | **8000** | **13 / 11** | **1413–1725 / 1480–1998** |

O que mudou de uma rodada para outra, e por quê:

- **Desempate por tempo retirado do custo.** Como o irace compara por ranks,
  qualquer diferença de tempo valia uma posição inteira. Na validação da 2ª
  rodada, todas as finalistas tinham o mesmo peso em 67% dos blocos; ali só a
  velocidade decidia, o que favorecia populações pequenas, e a finalista de
  melhor qualidade ficou em último pelo custo.
- **`elitistLimit = 0`**: o padrão (2) encerrava as corridas cedo.
- **Faixas ampliadas** (população a partir de 10, até 15 pais), porque a 2ª
  rodada parou na borda; e duas restrições que a API não verifica (pais
  não-elite fora do vetor; troca de elite com populações pequenas), conferidas
  por força bruta em 840 combinações.

Mesmo assim, a 3ª rodada usou poucas instâncias: com custo = peso, as
configurações empatam exatamente nas instâncias fáceis, o irace quase não
elimina e cada instância custa ~150–250 execuções. Por isso as finalistas
foram **validadas nas 86 instâncias de treino**.

### Validação das finalistas (`irace/valida_finalistas.sh`)

As finalistas de cada braço rodaram nas 86 instâncias × 2 sementes (1548
execuções, 0 inviáveis). Regra fixada antes de ver os resultados: menor rank
médio do peso por instância × semente (`irace/analisa_validacao.py`).

| Braço | Escolhida | Friedman entre as finalistas |
|---|---|---|
| BRKGA-MP-IPR | **829** | p = 0,94 (equivalentes) |
| BRKGA-MP | **1146** | p = 0,999 (equivalentes) |

### Configurações finais

| Parâmetro | BRKGA-MP (1146) | BRKGA-MP-IPR (829) |
|---|---|---|
| `population_size` | 1998 | 1725 |
| `elite_percentage` | 0,4534 | 0,1665 |
| `mutants_percentage` | 0,1558 | 0,0871 |
| `num_elite_parents` / `total_parents` | 1 / 9 | 1 / 8 |
| `bias_type` | EXPONENTIAL | QUADRATIC |
| `num_independent_populations` | 1 | 2 |
| `ipr_interval` | — | 65 |

Fixos nos dois: troca de 2 indivíduos de elite entre populações a cada 50
gerações sem melhora, sem *shake* e sem *reset*; no IPR, tipo permutação,
distância de Kendall tau, seleção pela melhor solução.

Na comparação com a 2ª rodada, nas mesmas 86 instâncias e sementes (treino,
não resultado): o BRKGA-MP-IPR novo é melhor em 37 blocos e pior em 6,
sobretudo nas harwell-boeing; o BRKGA-MP novo é melhor em 29 e pior em 16,
mas perde de 30 a 46 nas cúbicas de 776–876 vértices e na `lshp1009`,
instâncias que a sua calibração não viu.

### Limitações a declarar

1. Cobertura do irace baixa (11 e 13 das 86), compensada pela validação nas 86.
2. A população do BRKGA-MP (1998) está no limite superior da faixa (2000).
3. `num_elite_parents` = 1 em todas as finalistas, o mínimo possível.
4. A calibração usou 120 s; a bateria usa 600 s.

## Dependências externas e procedência

Este repositório contém e utiliza software e dados de terceiros. Nenhum deles
é de autoria do autor deste TCC.

| Componente | Origem | Autor | Licença | Versão | Como é obtido |
|---|---|---|---|---|---|
| API BRKGA-MP-IPR (C++) | [ceandrade/brkga_mp_ipr_cpp](https://github.com/ceandrade/brkga_mp_ipr_cpp) | Carlos Eduardo de Andrade | BSD-3 + citação obrigatória | `v3.1.1` | clonada em `external/`, **não versionada** aqui |
| PLI + guloso (Julia) | [Atilio-Luiz/dominacao_romana_2forte](https://github.com/Atilio-Luiz/dominacao_romana_2forte) | Atílio Gomes Luiz | GNU GPL-3.0 | commit `d350447` (2026-10-09) | **cópia** em `pli/`, ver `pli/README.md` |
| Resultados do VNS | [StefanKapunac/ksrdp](https://github.com/StefanKapunac/ksrdp) | Djukanović et al. | ver repositório de origem | — | filtrados para k = 2 em `results/baseline/vns_k2.csv` |
| Instâncias | repositório do PLI acima e [StefanKapunac/ksrdp](https://github.com/StefanKapunac/ksrdp) | — | ver repositórios de origem | — | versionadas em `instances/` |

### Obrigação de citação

A licença da API BRKGA-MP-IPR (cláusula 4) **exige** que qualquer publicação
que a utilize cite explicitamente:

> C. E. Andrade, R. F. Toso, J. F. Gonçalves, M. G. C. Resende. The Multi-Parent
> Biased Random-Key Genetic Algorithm with Implicit Path Relinking.
> *European Journal of Operational Research*, v. 289, n. 1, p. 17–30, 2021.
> DOI: [10.1016/j.ejor.2019.11.037](https://doi.org/10.1016/j.ejor.2019.11.037).

A baseline heurística e a metodologia de execução:

> M. Djukanović, S. Kapunac, A. Kartelj, D. Matić. Graph protection under
> multiple simultaneous attacks: A heuristic approach.
> *Knowledge-Based Systems*, 2025.
> DOI: [10.1016/j.knosys.2024.112791](https://doi.org/10.1016/j.knosys.2024.112791).

A calibração:

> M. López-Ibáñez, J. Dubois-Lacoste, L. Pérez Cáceres, M. Birattari,
> T. Stützle. The irace package: Iterated racing for automatic algorithm
> configuration. *Operations Research Perspectives*, 2016.
> DOI: [10.1016/j.orp.2016.09.002](https://doi.org/10.1016/j.orp.2016.09.002).
>
> M. Birattari. *Tuning Metaheuristics: A Machine Learning Perspective*.
> Springer, 2009. DOI: [10.1007/978-3-642-00483-4](https://doi.org/10.1007/978-3-642-00483-4).

## Instalação

A API é *header-only*: não há nada a compilar ou instalar, apenas o caminho de
inclusão. Ela não é versionada aqui (licença e autoria próprias), então é
preciso cloná-la:

```bash
git clone https://github.com/ezequielsan/pdr2f-tcc2.git
cd pdr2f-tcc2
git clone --depth 1 --branch v3.1.1 \
    https://github.com/ceandrade/brkga_mp_ipr_cpp external/brkga_mp_ipr_cpp
make
make main_complete
```

A tag é fixada de propósito: uma atualização futura da API não deve mudar os
resultados no meio do trabalho.

Requisitos de compilação, conforme a documentação da API: **GCC > 9.4** ou
**Clang > 10.0**, com suporte a C++20 e a OpenMP. Na prática, a API v3.1.1 não
compila com GCC < 13 sem o cabeçalho `src/compat/brkga_compat.hpp`, já
incluído no projeto. Ambiente usado nos experimentos: g++ 13.3 na WSL (Ubuntu 24.04).

Para a calibração: R com o pacote `irace` 4.5.1 e o executável `irace` no
PATH (ver `irace/README.md`). `scripts/verifica_ambiente.sh` confere o
ambiente.

## Execução

### Testes

```bash
make test
```

### Uma execução

```bash
./main_complete --config configs/brkga-mp-ipr/final_perm.conf \
                --instance instances/random/20_1.txt \
                --decoder permutation --seed 0 --threads 1 --maxtime 10
```

Sem `--maxtime`, o limite é de 600 s. Outras opções: `--target <peso>`
(para ao atingir o peso), `--generations <n>`, `--convlog <arq>` (curva de
convergência), `--warmstart <n>`, `--no-reduction` (só para ablação),
`--quiet` e `--header` (imprime o cabeçalho do CSV e termina).

### Calibração e validação

```bash
python3 scripts/gera_alvos.py        # alvos de parada
python3 scripts/sorteia_treino.py    # irace/treino.txt (86 instâncias)
./irace/roda_irace.sh --check        # valida os arquivos dos dois braços
./irace/roda_irace.sh --teste        # fumaça: 300 execuções de 2 s por braço
./irace/roda_irace.sh                # as duas calibrações, em sequência
./irace/valida_finalistas.sh         # finalistas nas 86 instâncias de treino
python3 irace/analisa_validacao.py   # escolha pela regra fixada
```

As calibrações e a validação definitivas já foram feitas; os resultados estão
em `irace/historico/`.

### Bateria final

Ainda a preparar: o `scripts/run_experiment.sh` vai receber as 10 sementes em
todas as classes, o alvo por instância, o limite de gerações por categoria e
braço (do piloto) e a curva de convergência. Ele exclui as instâncias de
treino e as duplicatas `gn-`, roda 6 processos de 1 thread e é retomável.

### PLI (Julia + CPLEX)

```bash
./scripts/run_pli.sh                          # todas as classes
./scripts/run_pli.sh random wireless-networks # apenas estas
```

Exige Julia com `JuMP`, `CPLEX`, `Graphs`, `DelimitedFiles` e
`MathOptInterface`, além de uma licença do CPLEX. Os resultados vão para
`pli/integer_linear_program/output/<classe>/`. Os resultados usados no TCC já
estão consolidados em `results/baseline/pli_todas.csv`.

## Notas de ambiente

- O repositório guarda todos os arquivos de texto em **LF** (ver
  `.gitattributes`). Use um único git sobre este diretório de trabalho —
  alternar entre o git do Windows e o do WSL no mesmo *worktree* produz
  diferenças espúrias de fim de linha e avisos de *dubious ownership*.
- **Máquina dos experimentos:** notebook Intel Core i5-1345U (2 núcleos de
  desempenho + 8 de eficiência), WSL com Ubuntu, g++ 13. A calibração e a
  bateria rodam na mesma máquina, porque a configuração calibrada vale para a
  velocidade da máquina em que foi calibrada.
- **Paralelismo:** 6 processos de 1 thread, deixando núcleos livres para o uso
  normal. A WSL está limitada a 7 núcleos (`%UserProfile%\.wslconfig`:
  `[wsl2]` / `processors=7`). Com SMT, dois processos no mesmo núcleo físico
  dividem o núcleo e cada um faz menos trabalho no mesmo limite de tempo; pelo
  mesmo motivo, evite tarefas pesadas durante a calibração e a bateria.
- **Relógio da WSL:** salta ~30 s periodicamente; ver
  `scripts/diagnostico_relogio.py`. O `main_complete` não depende dele.
- Rode as tarefas longas no `tmux` e deixe uma janela do Ubuntu aberta: se a
  WSL não tiver nenhuma janela, ela pode ser encerrada com tudo o que estiver
  rodando.
- A coluna `IPRTime` do CSV vem da API, que usa o relógio de parede; nesta
  máquina ela não é confiável. Use o número de chamadas e de melhorias do IPR.
- Se o Windows marcar arquivos baixados como bloqueados, no PowerShell:
  `Get-ChildItem -Recurse -File | Unblock-File`. O script
  `scripts/limpar-zone-identifier.sh` faz o equivalente no WSL, removendo
  arquivos `*:Zone.Identifier`.
