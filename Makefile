CC = gcc
MPICC = mpicc
CFLAGS = -Wall -Wextra -Werror -I. -g -fopenmp
CFLAGS_OPT = -Wall -Wextra -Werror -I. -O3 -fopenmp

OBJ_DIR = obj
MAP ?= maps/map1.txt

# ============================================
# FILES & TARGETS
# ============================================

SERIAL_NAME = sudoku_serial.exe
OMP_NAME    = sudoku_omp.exe
MPI_NAME    = sudoku_mpi.exe

SERIAL_SRC = sudoku_serial.c
OMP_SRC    = sudoku_omp.c
MPI_SRC    = sudoku_mpi.c

BACKTRACK_SERIAL = backtracking/backtracking.c
BACKTRACK_OMP    = backtracking/backtracking_omp.c

COMMON_SRCS = \
	general_utils/get_next_line.c \
	general_utils/utils.c \
	validator/parser.c \
	validator/loader.c

# ============================================
# BUILD TARGETS
# ============================================

all: $(SERIAL_NAME) $(OMP_NAME) $(MPI_NAME)

$(SERIAL_NAME): $(OBJ_DIR)/sudoku_serial.o $(OBJ_DIR)/backtracking.o $(OBJ_DIR)/get_next_line.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/parser.o $(OBJ_DIR)/loader.o
	$(CC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ Serial version: $(SERIAL_NAME)"

$(OMP_NAME): $(OBJ_DIR)/sudoku_omp.o $(OBJ_DIR)/backtracking_omp.o $(OBJ_DIR)/get_next_line_omp.o $(OBJ_DIR)/utils_omp.o $(OBJ_DIR)/parser_omp.o $(OBJ_DIR)/loader_omp.o
	$(CC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ OMP version: $(OMP_NAME)"

$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o $(OBJ_DIR)/benchmark.o $(OBJ_DIR)/backtracking_omp.o $(OBJ_DIR)/get_next_line_mpi.o $(OBJ_DIR)/utils_mpi.o $(OBJ_DIR)/parser_mpi.o $(OBJ_DIR)/loader_mpi.o
	$(MPICC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ MPI version: $(MPI_NAME)"

# ============================================
# SERIAL COMPILATION
# ============================================

$(OBJ_DIR)/sudoku_serial.o: $(SERIAL_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/backtracking.o: $(BACKTRACK_SERIAL)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/get_next_line.o: general_utils/get_next_line.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/utils.o: general_utils/utils.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/parser.o: validator/parser.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/loader.o: validator/loader.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

# ============================================
# OMP COMPILATION
# ============================================

$(OBJ_DIR)/sudoku_omp.o: $(OMP_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/backtracking_omp.o: $(BACKTRACK_OMP)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/get_next_line_omp.o: general_utils/get_next_line.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/utils_omp.o: general_utils/utils.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/parser_omp.o: validator/parser.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/loader_omp.o: validator/loader.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

# ============================================
# MPI COMPILATION
# ============================================

$(OBJ_DIR)/sudoku_mpi.o: $(MPI_SRC)
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/get_next_line_mpi.o: general_utils/get_next_line.c
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/utils_mpi.o: general_utils/utils.c
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/parser_mpi.o: validator/parser.c
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/loader_mpi.o: validator/loader.c
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

# ============================================
# BENCHMARK COMPILATION
# ============================================

$(OBJ_DIR)/benchmark.o: benchmark/benchmark.c benchmark/benchmark.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

# ============================================
# CLEANUP TARGETS
# ============================================

clean:
	@rm -rf $(OBJ_DIR)
	@rm -f *.exe *.out
	@rm -f *.o *~ .*~
	@echo "✓ Cleaned object files"

clean-temp:
	@rm -f *.txt *.html *.py 2>/dev/null || true
	@rm -f serial_*.txt omp_*.txt test_*.txt dezaseis* 2>/dev/null || true
	@rm -f output*.txt *_time.txt 2>/dev/null || true
	@echo "✓ Cleaned temporary files"

fclean: clean clean-temp
	@rm -f $(SERIAL_NAME) $(OMP_NAME) $(MPI_NAME)
	@echo "✓ Full clean (binaries removed)"

deep-clean: fclean
	@rm -rf obj/
	@rm -f *.exe *.out *.o
	@echo "✓ Deep clean (complete reset)"

# ============================================
# TEST TARGETS
# ============================================

test-serial: $(SERIAL_NAME)
	@echo "=== Testing Serial Version ==="
	@./$(SERIAL_NAME) maps/map1.txt > /dev/null
	@./$(SERIAL_NAME) maps/map2.txt > /dev/null
	@echo "✓ All serial tests passed"

test-omp: $(OMP_NAME)
	@echo "=== Testing OMP Version ==="
	@./$(OMP_NAME) maps/map1.txt > /dev/null
	@./$(OMP_NAME) maps/map2.txt > /dev/null
	@OMP_NUM_THREADS=2 ./$(OMP_NAME) maps/map2.txt > /dev/null
	@echo "✓ All OMP tests passed"

test-mpi: $(MPI_NAME)
	@echo "=== Testing MPI Version ==="
	@mpirun -np 1 ./$(MPI_NAME) maps/map1.txt > /dev/null
	@mpirun -np 2 ./$(MPI_NAME) maps/map1.txt > /dev/null
	@mpirun -np 4 ./$(MPI_NAME) maps/map2.txt > /dev/null
	@echo "✓ All MPI tests passed"

test: test-serial test-omp test-mpi
	@echo "✓ All tests completed successfully"

# ============================================
# HELP & INFO
# ============================================

help:
	@echo "TARGETS DISPONÍVEIS:"
	@echo "  make all          - Compilar serial, OMP e MPI (padrão)"
	@echo "  make clean        - Remover ficheiros objeto"
	@echo "  make clean-temp   - Remover ficheiros temporários"
	@echo "  make fclean       - Remover tudo (limpa compilação)"
	@echo "  make deep-clean   - Reset completo"
	@echo ""
	@echo "TESTES:"
	@echo "  make test         - Testar serial, OMP e MPI"
	@echo "  make test-serial  - Testar só serial"
	@echo "  make test-omp     - Testar só OMP"
	@echo "  make test-mpi     - Testar só MPI"
	@echo ""
	@echo "EXECUÇÃO MANUAL MPI:"
	@echo "  mpirun -np 4 ./$(MPI_NAME) maps/map1.txt"

.PHONY: all clean clean-temp fclean deep-clean test test-serial test-omp test-mpi help
