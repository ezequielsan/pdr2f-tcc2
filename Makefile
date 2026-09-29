###############################################################################
# Makefile do projeto PDR2F (TCC 2).
###############################################################################

OPT = opt

# Reprodutibilidade: o resultado depende apenas da semente.
USER_FLAGS += -DMATING_SEED_ONLY

API_DIR = external/brkga_mp_ipr_cpp

INCLUDES = \
	-I./src \
	-I$(API_DIR) \
	-I$(API_DIR)/brkga_mp_ipr

# Fontes compartilhadas (preencher conforme os arquivos forem criados).
OBJS =

CXX = g++

USER_FLAGS += -std=c++20

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

.PHONY: all clean check-api
.SUFFIXES: .cpp .o

all: check-api

check-api:
	@test -f $(API_DIR)/brkga_mp_ipr/brkga_mp_ipr.hpp || \
		(echo "ERRO: API nao encontrada em $(API_DIR). Rode:"; \
		 echo "  git clone --depth 1 https://github.com/ceandrade/brkga_mp_ipr_cpp $(API_DIR)"; \
		 exit 1)

.cpp.o:
	@echo "--> Compilando $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "--> Limpando..."
	rm -rf $(OBJS) *.o bin/
