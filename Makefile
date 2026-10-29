###############################################################################
# Makefile do projeto PDR2F (TCC 2).
#
# Alvos:
#   make                 compila tudo o que ja' existe
#   make instance_info   utilitario de leitura/inspecao de instancias
#   make test            testes do leitor + auditoria do formato das instancias
#   make check-api       verifica se a API BRKGA-MP-IPR foi clonada
#   make clean           remove objetos, dependencias e binarios
#
# Compilacao com simbolos de depuracao:  make OPT=dbg
###############################################################################

OPT = opt

# Reprodutibilidade: o resultado depende apenas da semente.
USER_FLAGS += -DMATING_SEED_ONLY

API_DIR = external/brkga_mp_ipr_cpp

INCLUDES = \
	-I./src \
	-I$(API_DIR) \
	-I$(API_DIR)/brkga_mp_ipr

# Fontes compartilhadas por todos os programas.
OBJS = \
	src/pdr2f/pdr2f_instance.o

# Objetos dos programas principais, um por executavel.
APP_OBJS = apps/instance_info.o

CXX = g++

USER_FLAGS += -std=c++20

# -MMD -MP: o compilador gera os arquivos .d com as dependencias de cabecalho,
# para que editar um .hpp recompile quem o inclui. Sem isso, mudar um
# cabecalho nao dispara recompilacao e o binario fica inconsistente em
# silencio.
USER_FLAGS += -MMD -MP

ifneq ($(OPT), opt)
	USER_FLAGS += -ggdb3 -fexceptions -fno-omit-frame-pointer \
		-fno-optimize-sibling-calls -fno-inline
else
	USER_FLAGS += -O3 -fomit-frame-pointer -funroll-loops
	ifeq ($(CXX), g++)
		USER_FLAGS += -ftracer -fpeel-loops -fprefetch-loop-arrays -flto=auto
	endif
endif

USER_FLAGS += -pthread -fopenmp

USER_FLAGS += -Wall -Wextra -Wcast-align -Wcast-qual -Wdisabled-optimization \
	-Wformat=2 -Winit-self -Wmissing-format-attribute -Wshadow \
	-Wpointer-arith -Wredundant-decls -Wstrict-aliasing=2 \
	-Wfloat-equal -Weffc++

CXXFLAGS = $(USER_FLAGS)

.PHONY: all clean check-api test
.SUFFIXES: .cpp .o

all: instance_info

# O instance_info nao usa a API: le instancias e imprime suas caracteristicas.
instance_info: $(OBJS) apps/instance_info.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

# Testes do leitor (casos sinteticos) e auditoria do formato das instancias.
test: instance_info
	@./scripts/teste_leitor.sh
	@echo ""
	@python3 scripts/valida_formato.py

check-api:
	@test -f $(API_DIR)/brkga_mp_ipr/brkga_mp_ipr.hpp || \
		(echo "ERRO: API nao encontrada em $(API_DIR). Rode:"; \
		 echo "  git clone --depth 1 --branch v3.1.1 https://github.com/ceandrade/brkga_mp_ipr_cpp $(API_DIR)"; \
		 exit 1)
	@echo "API encontrada em $(API_DIR)."

.cpp.o:
	@echo "--> Compilando $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "--> Limpando..."
	rm -rf $(OBJS) $(APP_OBJS) $(OBJS:.o=.d) $(APP_OBJS:.o=.d) \
		apps/*.o apps/*.d *.o *.d bin/ instance_info

# Dependencias de cabecalho geradas por -MMD. O '-' silencia a primeira
# compilacao, quando os .d ainda nao existem.
-include $(OBJS:.o=.d)
-include $(APP_OBJS:.o=.d)
