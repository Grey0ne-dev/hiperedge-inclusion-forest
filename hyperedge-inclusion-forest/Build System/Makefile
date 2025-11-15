# Hyperedge Inclusion Forest - Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OPTFLAGS = -O3
DEBUGFLAGS = -g -fsanitize=address

# Library
LIB_SRC = hif.c
LIB_OBJ = hif.o

# Main targets
all: example tests advanced_tests application_tests benchmark nested_benchmark

# Library object
$(LIB_OBJ): $(LIB_SRC) hif.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -c $(LIB_SRC)

# Example program
example: example.c $(LIB_OBJ)
	$(CC) $(CFLAGS) $(OPTFLAGS) -o example example.c $(LIB_OBJ)

# Test suites (self-contained)
tests: tests.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o tests tests.c

advanced_tests: advanced_tests.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o advanced_tests advanced_tests.c

application_tests: application_tests.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o application_tests application_tests.c

# Benchmarks (self-contained)
benchmark: benchmark.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o benchmark benchmark.c

nested_benchmark: nested_benchmark.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o nested_benchmark nested_benchmark.c

# Debug build
debug: example.c $(LIB_SRC) hif.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o example_debug example.c $(LIB_SRC)

# Run example
run: example
	./example

# Run all tests
test: tests advanced_tests application_tests
	@echo "=== Running basic tests ==="
	./tests
	@echo ""
	@echo "=== Running advanced tests ==="
	./advanced_tests
	@echo ""
	@echo "=== Running application tests ==="
	./application_tests

# Run benchmarks
bench: benchmark nested_benchmark
	@echo "=== Running general benchmarks ==="
	./benchmark
	@echo ""
	@echo "=== Running nested structure benchmarks ==="
	./nested_benchmark

# Clean
clean:
	rm -f example example_debug tests advanced_tests application_tests
	rm -f benchmark nested_benchmark
	rm -f *.o *.so *.a

# Install
install: example
	install -m 755 example /usr/local/bin/hif

.PHONY: all run test bench clean install debug
