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

## Situação atual (03/11/2026)

| Etapa | Situação |
|---|---|
| Leitor de instâncias, verificador de viabilidade, guloso | concluído e validado |
| Decodificadores e busca local de redução | concluído e validado |
| Integração com a API BRKGA-MP-IPR (`main_minimal`, `main_complete`) | concluído |
| Referências consolidadas (PLI e VNS k = 2) | concluído (`results/baseline/`) |
| Desenho experimental | fechado (seção [Metodologia experimental](#metodologia-experimental)) |
| Calibração com irace | **concluída** em 02–03/11/2026 (seção [Resultado da calibração](#resultado-da-calibração)) |
| Bateria final (BRKGA-MP e BRKGA-MP-IPR) | **a executar** (~56 h de relógio) |
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
configs/      parâmetros: uma pasta por algoritmo (dev_*.conf e final_perm.conf)
irace/        calibração: cenário, parâmetros, target-runner, treino e resultado
pli/          modelo de programação linear inteira (cópia de terceiros, ver pli/README.md)
instances/    instâncias, por classe
results/      referências consolidadas (baseline/) e saídas dos experimentos
scripts/      validações, sorteio do treino, bateria de execução e análise
external/     API BRKGA-MP-IPR (não versionada)
docs/         anotações e material de apoio
```

## Instâncias

As instâncias são **matrizes de adjacência**, uma linha por vértice. Qualquer
valor não nulo é aresta e a diagonal é ignorada; ver `pli/README.md` para o
contrato completo de leitura, que o lado C++ reproduz (validado campo a campo
contra o leitor do PLI).

| Classe | Instâncias únicas | Baseline VNS (k = 2) | Ótimo provado pelo PLI | Execuções por instância |
|---|---|---|---|---|
| `random` | 35 | sim | 35 | 10 |
| `wireless-networks` | 20 | sim | 20 | 10 |
| `geoinstances` | 168 | sim | 152 | 1 |
| `harwell-boeing` | 38 | não | 10 | 1 |
| `random_cubic` | 26 | não | 3 | 10 |
| **total** | **287** | **223** | **220** | |

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
  de 27 campos por execução (peso, viabilidade auditada, gerações, tempos,
  chamadas e melhorias do IPR etc.). O limite de tempo padrão é a **faixa do
  protocolo** (ver Metodologia); o rótulo `brkga-mp`/`brkga-mp-ipr` é deduzido
  de `ipr_interval`.
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

## Metodologia experimental

### Princípio

A **bateria final segue a metodologia do artigo do VNS** (Djukanović, Kapunac,
Kartelj, Matić, *Knowledge-Based Systems*, 2025; arXiv:2403.17108, §3), para
que os nossos números sejam comparáveis aos publicados. As classes que não
estão no artigo (`harwell-boeing`, `random_cubic`) seguem a mesma regra.

| Item | Artigo do VNS | Este trabalho |
|---|---|---|
| Limite de tempo, `n < 30` | 300 s | 300 s |
| Limite de tempo, `30 ≤ n < 100` | 600 s | 600 s |
| Limite de tempo, `n ≥ 100` | 1200 s | 1200 s |
| Execuções, classes sintéticas | 10 (`random`, `wireless`) | 10 (`random`, `wireless`, `random_cubic`) |
| Execuções, classes reais | 1 (`geoinstances`) | 1 (`geoinstances`, `harwell-boeing`) |
| Threads por execução | 1 | 1 |
| Critério de parada | tempo (o VNS tem também limite de 5000 iterações) | **somente tempo** |
| Máquina | Intel Xeon E5-2640, 2,40 GHz | AMD Ryzen 7 5700G, WSL Ubuntu |
| Testes estatísticos | Friedman + pós-teste de Nemenyi, diagramas de diferença crítica com α = 0,05 | os mesmos |

As faixas de tempo foram conferidas também nos tempos brutos dos três
conjuntos do repositório do artigo. O tempo de leitura da instância fica fora
do orçamento (acontece antes de `algorithm.run()`).

**Diferença de máquina:** as CPUs são diferentes, então o mesmo limite de
tempo não significa o mesmo esforço computacional. É uma limitação inerente à
comparação com resultados publicados e será declarada no texto.

### Dois braços

| Braço | Algoritmo | Decodificador | Busca local | Configuração |
|---|---|---|---|---|
| 1 | BRKGA-MP | permutação | sim | `configs/brkga-mp/final_perm.conf` |
| 2 | BRKGA-MP-IPR | permutação | sim | `configs/brkga-mp-ipr/final_perm.conf` |

Os dois arquivos são **idênticos exceto por `ipr_interval`** (0 no BRKGA-MP),
para que a diferença entre os braços meça apenas o efeito do IPR. Sem *warm
start*. As mesmas sementes são usadas nos dois braços (comparação pareada).

### Calibração (irace)

O artigo do VNS **não usa um configurador automático**: os parâmetros do VNS
foram fixados com base em experimentos preliminares (§3.2). Aqui a calibração
é feita com o **irace** (López-Ibáñez et al., 2016), mantendo a lógica de
**orçamento por faixa de tamanho** do artigo, reduzida a 10%:

| | |
|---|---|
| Ferramenta | irace 4.5.1 (R 4.5.2), teste de Friedman nas corridas |
| Algoritmo calibrado | BRKGA-MP-IPR; o BRKGA-MP herda os parâmetros com `ipr_interval = 0` |
| Parâmetros | 8 (tabela abaixo); os demais fixos em `irace/base.conf` |
| Restrições | `num_elite_parents < total_parents`; elite com ao menos `num_elite_parents` indivíduos |
| Orçamento | 2000 execuções do BRKGA |
| Tempo por execução | **10% da faixa do protocolo**: 60 s (faixa de 600 s) e 120 s (faixa de 1200 s) |
| Paralelismo | 7 processos (núcleos físicos − 1), 1 thread cada |
| Semente do irace | `20261102` |

Parâmetros calibrados e faixas:

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

Fixos: IPR do tipo PERMUTATION com distância KENDALLTAU, `pr_selection
BESTSOLUTION`, `pr_minimum_distance 10`, `alpha_block_size 1.0`,
`pr_percentage 1.0`; troca de elite entre populações a cada 50 gerações sem
melhora (2 indivíduos); sem *shake* e sem *reset*.

**Separação treino/teste.** Por indicação do orientador, as instâncias usadas
na calibração **não entram na bateria final**, para não reportar resultado em
instância usada para escolher os parâmetros (*over-tuning*; López-Ibáñez et
al., 2016; Birattari, 2009):

| | |
|---|---|
| Fração | 10% das 287 instâncias únicas → **29 instâncias** (`irace/treino.txt`) |
| Sorteio | estratificado por classe (método do maior resto), **só entre n ≥ 30** |
| Semente | `20261102`, fixa; `scripts/sorteia_treino.py` reproduz a lista |
| Composição | 17 geoinstances, 4 harwell-boeing, 3 random, 3 random_cubic, 2 wireless-networks |

O sorteio é restrito a `n ≥ 30` porque, nas instâncias menores, todas as
configurações chegam ao ótimo em segundos e empatam, sem informar o irace.
Detalhes em `irace/README.md`.

### Bateria final

| Classe | Instâncias de teste | Execuções | Sementes | Com ótimo do PLI | Com baseline VNS |
|---|---|---|---|---|---|
| `random` | 32 | 10 | 0–9 | 32 | 32 |
| `wireless-networks` | 18 | 10 | 0–9 | 18 | 18 |
| `geoinstances` | 151 | 1 | 0 | 137 | 151 |
| `harwell-boeing` | 34 | 1 | 0 | 10 | — |
| `random_cubic` | 23 | 10 | 0–9 | 3 | — |
| **total** | **258** | | | **200** | **201** |

**Volume:** 915 execuções por braço, **1830 no total**; estimativa de
**~390 h de CPU e ~56 h de relógio** com 7 processos em paralelo.

### Comparações previstas

1. **Contra o PLI:** gap ao ótimo nas 200 instâncias de teste com ótimo
   provado; nas demais, comparação com a melhor solução do CPLEX, que é apenas
   um limite superior.
2. **Contra o VNS (k = 2):** nas 201 instâncias de teste com baseline, melhor
   contra melhor e, em `random` e `wireless-networks`, média contra média.
3. **BRKGA-MP × BRKGA-MP-IPR:** efeito do IPR, pareado por semente.
4. **Testes estatísticos:** Friedman e pós-teste de Nemenyi com α = 0,05, como
   no artigo.

O guloso com desempate aleatório entra como linha própria nas tabelas, para
separar o efeito da construção inicial do efeito da meta-heurística.

## Resultado da calibração

Executada de **02/11/2026, 23h38, a 03/11/2026, 7h46** (8h08 de relógio,
51,9 h de CPU). Dados completos em `irace/resultado/`: `irace.log`,
`irace.Rdata` e `execucoes.csv` (uma linha por execução do BRKGA).

| | |
|---|---|
| Execuções | 2000 |
| Soluções viáveis | **2000 de 2000** |
| Configurações avaliadas | 257 |
| Instâncias de treino efetivamente usadas | 13 de 29 |

### Configuração vencedora (ID 226)

| Parâmetro | Valor |
|---|---|
| `population_size` | **705** |
| `elite_percentage` | **0,3547** |
| `mutants_percentage` | **0,0785** |
| `num_elite_parents` | **3** |
| `total_parents` | **7** |
| `bias_type` | **LINEAR** |
| `num_independent_populations` | **3** |
| `ipr_interval` | **50** |

Gerou `configs/brkga-mp-ipr/final_perm.conf` e, com `ipr_interval 0`,
`configs/brkga-mp/final_perm.conf`.

As quatro configurações de elite finais convergiram para o mesmo perfil:

| ID | pop. | elite | mutantes | pais elite / total | viés | pops. | IPR |
|---|---|---|---|---|---|---|---|
| 226 | 705 | 0,355 | 0,079 | 3 / 7 | LINEAR | 3 | 50 |
| 132 | 556 | 0,379 | 0,074 | 3 / 7 | LINEAR | 3 | 64 |
| 176 | 992 | 0,158 | 0,075 | 3 / 6 | QUADRATIC | 2 | 112 |
| 184 | 781 | 0,387 | 0,049 | 3 / 7 | LINEAR | 3 | 52 |

Padrão: população grande (556–992), elite grande (~35–39%, exceto a 176),
poucos mutantes (5–8%), três pais de elite entre seis ou sete e IPR frequente
(a cada ~50–112 gerações). O teste de fumaça, com 2 s por execução,
favorecia populações de 57 a 130, o que mostra que calibrar com orçamento
curto demais distorceria a escolha.

**IPR na configuração vencedora:** em 13 execuções, 10 melhorias da melhor
solução, e só 0,6% das chamadas terminaram por homogeneidade das populações.

### Ressalvas

1. **`num_independent_populations` convergiu para o limite superior da faixa
   (3)**, e `total_parents` (7) ficou perto do seu limite (8). Parâmetro na
   borda indica que o melhor valor pode estar fora da faixa. Não foi
   reexplorado; será declarado como limitação (ou testado com 4 e 5
   populações, se houver tempo).
2. **A escolha foi decidida, na prática, por 5 instâncias grandes**
   (`london-underground`, `minneapolis-cities`, `dwt__361`, `cubic_650`,
   `nos6`). As outras 8 usadas empataram entre todas as configurações; a
   configuração está, portanto, ajustada às instâncias grandes.
3. **Os valores obtidos nas instâncias de treino não são resultados do
   trabalho**: essas instâncias serviram para escolher os parâmetros e por
   isso estão fora da bateria final.

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
incluído no projeto. Ambiente usado nos experimentos: g++ 15.2 na WSL (Ubuntu).

Para a calibração: R com o pacote `irace` (versão 4.5.1 usada aqui).
`scripts/verifica_ambiente.sh` confere o ambiente.

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

Sem `--maxtime`, o limite é a faixa do protocolo (300/600/1200 s). Outras
opções: `--generations <n>`, `--warmstart <n>`, `--no-reduction` (só para
ablação), `--quiet`, `--header` (imprime o cabeçalho do CSV e termina).

### Calibração

```bash
./irace/roda_irace.sh --check   # valida os arquivos, sem executar nada
./irace/roda_irace.sh --teste   # fumaça: execuções de 2 s, alguns minutos
./irace/roda_irace.sh           # calibração completa (~8 h)
```

A calibração definitiva já foi feita; o resultado está em `irace/resultado/`.

### Bateria final

Dentro do `tmux`, a partir da raiz:

```bash
./scripts/run_experiment.sh -n -p configs/brkga-mp/final_perm.conf      # simulação, não executa
./scripts/run_experiment.sh -p configs/brkga-mp/final_perm.conf     -o results/bateria_final.csv
./scripts/run_experiment.sh -p configs/brkga-mp-ipr/final_perm.conf -o results/bateria_final.csv
python3 scripts/analisa_bateria.py results/bateria_final.csv
```

O `run_experiment.sh` aplica sozinho as regras do protocolo: exclui as
instâncias de treino (`irace/treino.txt`) e as duplicatas `gn-`, usa 10 ou 1
semente conforme a classe e roda 7 processos em paralelo com 1 thread cada.
É **retomável**: se for interrompido, basta repetir o comando, e as execuções
já registradas no CSV são puladas.

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
- **Paralelismo:** use no máximo *núcleos físicos − 1* processos (7 no Ryzen 7
  5700G). Com SMT, dois processos no mesmo núcleo físico dividem o núcleo, e
  cada um faz menos trabalho dentro do mesmo limite de tempo. Pelo mesmo
  motivo, evite tarefas pesadas na máquina durante a calibração e a bateria.
- Se o Windows marcar arquivos baixados como bloqueados, no PowerShell:
  `Get-ChildItem -Recurse -File | Unblock-File`. O script
  `scripts/limpar-zone-identifier.sh` faz o equivalente no WSL, removendo
  arquivos `*:Zone.Identifier`.
