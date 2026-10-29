# Meta-heurísticas para o Problema da Dominação Romana 2-Forte (PDR2F)

Implementações em C++ do **BRKGA-MP** e do **BRKGA-MP-IPR** para o PDR2F,
construídas sobre a API [BRKGA-MP-IPR](https://github.com/ceandrade/brkga_mp_ipr_cpp)
de C. E. Andrade, comparadas com a heurística VNS de Djukanović et al. (2025) e
com o modelo de programação linear inteira usado como referência exata.

> Trabalho de Conclusão de Curso 2 — Engenharia de Software, UFC Quixadá.

## Estrutura

```
src/          código compartilhado (instância, decodificadores, distâncias, heurísticas)
apps/         programas principais (main_*)
configs/      parâmetros: uma pasta por algoritmo
pli/          modelo de programação linear inteira (cópia de terceiros, ver pli/README.md)
instances/    instâncias, por classe
results/      saídas dos experimentos, por classe
scripts/      baterias de execução e análise
external/     API BRKGA-MP-IPR (não versionada)
docs/         anotações e material de apoio
```

As cinco classes de instâncias, espelhadas em `instances/` e em `results/`:

| Classe | Instâncias | Baseline VNS | Referência PLI |
|---|---|---|---|
| `random` | 35 | sim | 35 ótimos |
| `wireless-networks` | 20 | sim | 20 ótimos |
| `geoinstances` | 258 | sim | 229 ótimos |
| `harwell-boeing` | 38 | não | 10 ótimos |
| `random_cubic` | 26 | não | 3 ótimos |

As instâncias são **matrizes de adjacência**, uma linha por vértice. Qualquer
valor não nulo é aresta e a diagonal é ignorada; ver `pli/README.md` para o
contrato completo de leitura, que o lado C++ reproduz.

## Dependências externas e procedência

Este repositório contém e utiliza software de terceiros. Nenhum deles é de
autoria do autor deste TCC.

| Componente | Origem | Autor | Licença | Versão | Como é obtido |
|---|---|---|---|---|---|
| API BRKGA-MP-IPR (C++) | [ceandrade/brkga_mp_ipr_cpp](https://github.com/ceandrade/brkga_mp_ipr_cpp) | Carlos Eduardo de Andrade | BSD-3 + citação obrigatória | `v3.1.1` | clonada em `external/`, **não versionada** aqui |
| PLI + guloso (Julia) | [Atilio-Luiz/dominacao_romana_2forte](https://github.com/Atilio-Luiz/dominacao_romana_2forte) | Atílio Gomes Luiz | GNU GPL-3.0 | commit `d350447` (2026-10-09) | **cópia** em `pli/`, ver `pli/README.md` |
| Instâncias | repositório do PLI acima e [StefanKapunac/ksrdp](https://github.com/StefanKapunac/ksrdp) | — | ver repositórios de origem | — | versionadas em `instances/` |

### Obrigação de citação

A licença da API BRKGA-MP-IPR (cláusula 4) **exige** que qualquer publicação
que a utilize cite explicitamente:

> C. E. Andrade, R. F. Toso, J. F. Gonçalves, M. G. C. Resende. The Multi-Parent
> Biased Random-Key Genetic Algorithm with Implicit Path Relinking.
> *European Journal of Operational Research*, v. 289, n. 1, p. 17–30, 2021.
> DOI: [10.1016/j.ejor.2019.11.037](https://doi.org/10.1016/j.ejor.2019.11.037).

A baseline heurística da comparação:

> M. Djukanović, S. Kapunac, A. Kartelj, D. Matić. Graph protection under
> multiple simultaneous attacks: A heuristic approach.
> *Knowledge-Based Systems*, 2025.
> DOI: [10.1016/j.knosys.2024.112791](https://doi.org/10.1016/j.knosys.2024.112791).

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
```

A tag é fixada de propósito: uma atualização futura da API não deve mudar os
resultados no meio do trabalho.

Requisitos de compilação, conforme a documentação da API: **GCC > 9.4** ou
**Clang > 10.0**, com suporte a C++20 e a OpenMP para execução multi-thread.
Compiladores da Microsoft e da Intel não são testados pelo autor da API.

## Execução

### PLI (Julia + CPLEX)

```bash
./scripts/run_pli.sh                          # todas as classes
./scripts/run_pli.sh random wireless-networks # apenas estas
```

Exige Julia com `JuMP`, `CPLEX`, `Graphs`, `DelimitedFiles` e
`MathOptInterface`, além de uma licença do CPLEX. Os resultados vão para
`pli/integer_linear_program/output/<classe>/`.

### BRKGA

(a preencher)

## Notas de ambiente

- O repositório guarda todos os arquivos de texto em **LF** (ver
  `.gitattributes`). Use um único git sobre este diretório de trabalho —
  alternar entre o git do Windows e o do WSL no mesmo *worktree* produz
  diferenças espúrias de fim de linha e avisos de *dubious ownership*.
- Se o Windows marcar arquivos baixados como bloqueados, no PowerShell:
  `Get-ChildItem -Recurse -File | Unblock-File`. O script
  `scripts/limpar-zone-identifier.sh` faz o equivalente no WSL, removendo
  arquivos `*:Zone.Identifier`.
