###############################################################################
# Makefile do projeto PDR2F (TCC 2).
#
# Alvos:
#   make                 compila tudo o que ja' existe
#   make instance_info   leitura e inspecao de instancias
#   make greedy_info     roda o guloso e imprime os pesos em CSV
#   make decoder_info    compara os 4 decodificadores com cromossomos aleatorios
#   make test_pdr2f      testes do verificador de viabilidade e do guloso
#   make test_decoders   testes dos decodificadores e da busca local
#   make main_minimal    o BRKGA-MP(-IPR) minimo (precisa da API)
#   make main_complete   o BRKGA-MP(-IPR) com CSV para as baterias
#   make test            roda TODA a bateria de testes e validacoes
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
	src/pdr2f/pdr2f_instance.o \
	src/pdr2f/pdr2f_feasibility.o \
	src/pdr2f/pdr2f_reduction.o \
	src/decoders/pdr2f_perm_decoder.o \
	src/decoders/pdr2f_threshold_decoder.o \
	src/heuristics/greedy_pdr2f.o

# Objetos dos programas principais, um por executavel.
APP_OBJS = \
	apps/instance_info.o \
	apps/greedy_info.o \
	apps/decoder_info.o \
	apps/test_pdr2f.o \
	apps/test_decoders.o \
	apps/main_minimal.o \
	apps/main_complete.o

# Instancias pequenas usadas nos testes: o teste de equivalencia com a forca
# bruta e' O(n^2) por rotulacao, entao roda em grafos pequenos.
TEST_INSTANCES = \
	instances/random/10_1.txt instances/random/10_2.txt \
	instances/random/15_1.txt instances/random/15_2.txt \
	instances/random/20_1.txt instances/random/20_2.txt \
	instances/geoinstances/surrey.txt instances/geoinstances/alameda.txt \
	instances/geoinstances/akron.txt instances/geoinstances/esztergom.txt

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

all: instance_info greedy_info decoder_info test_pdr2f test_decoders \
     main_minimal main_complete

# Nenhum destes usa a API BRKGA-MP-IPR ainda.
instance_info: $(OBJS) apps/instance_info.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

greedy_info: $(OBJS) apps/greedy_info.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

decoder_info: $(OBJS) apps/decoder_info.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test_pdr2f: $(OBJS) apps/test_pdr2f.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test_decoders: $(OBJS) apps/test_decoders.o
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

# Os dois unicos alvos que dependem da API BRKGA-MP-IPR. O check-api entra
# como pre-requisito ORDER-ONLY (depois do |): sendo .PHONY, se fosse
# pre-requisito normal forcaria o religamento em todo make.
main_minimal: $(OBJS) apps/main_minimal.o | check-api
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) apps/main_minimal.o -o $@

main_complete: $(OBJS) apps/main_complete.o | check-api
	@echo "--> Ligando $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) apps/main_complete.o -o $@

# Bateria completa: formato das instancias, leitor, verificador, guloso.
test: all
	@./scripts/teste_leitor.sh
	@echo ""
	@python3 scripts/valida_formato.py
	@echo ""
	@./test_pdr2f $(TEST_INSTANCES)
	@echo ""
	@./test_decoders $(TEST_INSTANCES)
	@echo ""
	@./greedy_info --seeds 10 instances/*/*.txt > guloso.csv
	@python3 scripts/valida_guloso.py guloso.csv
	@echo ""
	@./decoder_info --seeds 10 instances/*/*.txt > decoders.csv
	@python3 scripts/valida_decoders.py decoders.csv

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
		apps/*.o apps/*.d src/*/*.o src/*/*.d *.o *.d bin/ \
		instance_info greedy_info decoder_info test_pdr2f test_decoders \
		main_minimal main_complete

# Dependencias de cabecalho geradas por -MMD. O '-' silencia a primeira
# compilacao, quando os .d ainda nao existem.
-include $(OBJS:.o=.d)
-include $(APP_OBJS:.o=.d)
