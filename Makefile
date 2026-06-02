CC ?= gcc
AR ?= ar
PREFIX ?= /usr/local

CFLAGS ?= -O3
WARNFLAGS := -Wall -Wextra -std=c99
SANFLAGS := -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer

PROJECT_DIR := hyperedge-inclusion-forest
CORE_DIR := $(PROJECT_DIR)/Core Implementation
TEST_DIR := $(PROJECT_DIR)/Test Suite
BENCH_DIR := $(PROJECT_DIR)/Benchmarks
BUILD_DIR := build

HIF_C := $(CORE_DIR)/hif.c
HIF_H := $(CORE_DIR)/hif.h

.PHONY: all lib static shared example tests comprehensive-tests basic-tests benchmark sanitize clean install

all: lib example tests

lib: static shared

$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

static: $(BUILD_DIR)
	$(CC) $(WARNFLAGS) $(CFLAGS) -c "$(HIF_C)" -o "$(BUILD_DIR)/hif.o"
	$(AR) rcs "$(BUILD_DIR)/libhif.a" "$(BUILD_DIR)/hif.o"

shared: $(BUILD_DIR)
	$(CC) $(WARNFLAGS) $(CFLAGS) -fPIC -shared "$(HIF_C)" -o "$(BUILD_DIR)/libhif.so"

example: static
	$(CC) $(WARNFLAGS) $(CFLAGS) -I"$(CORE_DIR)" "$(CORE_DIR)/example.c" "$(BUILD_DIR)/libhif.a" -o "$(BUILD_DIR)/hif_example"

basic-tests: $(BUILD_DIR)
	$(CC) $(WARNFLAGS) $(CFLAGS) "$(TEST_DIR)/tests.c" -o "$(BUILD_DIR)/hif_basic_tests"
	"$(BUILD_DIR)/hif_basic_tests"

comprehensive-tests: static
	$(CC) $(WARNFLAGS) $(CFLAGS) -I"$(CORE_DIR)" "$(TEST_DIR)/comprehensive_tests.c" "$(BUILD_DIR)/libhif.a" -o "$(BUILD_DIR)/hif_comprehensive_tests"
	"$(BUILD_DIR)/hif_comprehensive_tests"

tests: basic-tests comprehensive-tests

benchmark: static
	$(CC) $(WARNFLAGS) $(CFLAGS) -I"$(CORE_DIR)" "$(BENCH_DIR)/benchmark_weighted.c" "$(BUILD_DIR)/libhif.a" -lm -o "$(BUILD_DIR)/hif_benchmark_weighted"
	$(CC) $(WARNFLAGS) $(CFLAGS) -I"$(CORE_DIR)" "$(BENCH_DIR)/nested_benchmark.c" "$(BUILD_DIR)/libhif.a" -o "$(BUILD_DIR)/hif_nested_benchmark"

sanitize: $(BUILD_DIR)
	$(CC) $(WARNFLAGS) $(SANFLAGS) -I"$(CORE_DIR)" "$(HIF_C)" "$(TEST_DIR)/comprehensive_tests.c" -o "$(BUILD_DIR)/hif_comprehensive_tests_asan"
	ASAN_OPTIONS=detect_leaks=0 "$(BUILD_DIR)/hif_comprehensive_tests_asan"

install: lib
	install -d "$(DESTDIR)$(PREFIX)/include" "$(DESTDIR)$(PREFIX)/lib"
	install -m 644 "$(HIF_H)" "$(DESTDIR)$(PREFIX)/include/hif.h"
	install -m 644 "$(BUILD_DIR)/libhif.a" "$(DESTDIR)$(PREFIX)/lib/libhif.a"
	install -m 755 "$(BUILD_DIR)/libhif.so" "$(DESTDIR)$(PREFIX)/lib/libhif.so"

clean:
	rm -rf "$(BUILD_DIR)"
