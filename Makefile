UNAME := $(shell uname -s)

CC       ?= cc
CXX      ?= c++
PYTHON   ?= uv run python

OPT      ?= -O3
ARCH     ?= -march=native
WARN     := -Wall -Wextra
INCLUDE  := -Iinclude
DEPFLAGS := -MMD -MP
CFLAGS   := $(OPT) $(ARCH) $(WARN) -std=c11 -D_POSIX_C_SOURCE=200809L -fPIC \
            $(INCLUDE) $(DEPFLAGS)
CXXFLAGS := $(OPT) $(ARCH) $(WARN) -std=c++17 -fPIC $(INCLUDE) $(DEPFLAGS)
LDFLAGS  :=
LDLIBS   := -lpthread

ifeq ($(UNAME),Darwin)
  SO     := dylib
  SHARED := -dynamiclib
else
  SO     := so
  SHARED := -shared
endif

BUILD    := build
LIB      := $(BUILD)/libarc.$(SO)
LIB_HARN := $(BUILD)/libarc_harness.$(SO)
LIB_FFI  := $(BUILD)/libarc_ffi.$(SO)
SMOKE    := $(BUILD)/scene_game_smoketest

CORE_SRC := $(wildcard src/*.c) $(wildcard src/games/*.c)
HARN_SRC := $(filter-out shims/scene_game_smoketest.c,$(wildcard shims/*.c))
FFI_SRC  := bindings/ffi_typed.cc

CORE_OBJ := $(CORE_SRC:%.c=$(BUILD)/%.o)
HARN_OBJ := $(HARN_SRC:%.c=$(BUILD)/%.o)
FFI_OBJ  := $(FFI_SRC:%.cc=$(BUILD)/%.o)
ALL_OBJ  := $(CORE_OBJ) $(HARN_OBJ) $(FFI_OBJ) $(BUILD)/shims/scene_game_smoketest.o

.PHONY: all lib harness ffi smoke lto pgo test bench format check-format clean clean-obj help
.DEFAULT_GOAL := all

all: lib

lib: $(LIB)
harness: $(LIB_HARN)
ffi: $(LIB_FFI)
smoke: $(SMOKE)

$(LIB): $(CORE_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(LIB_HARN): $(CORE_OBJ) $(HARN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(LIB_FFI): $(CORE_OBJ) $(HARN_OBJ) $(FFI_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(SHARED) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(SMOKE): $(BUILD)/shims/scene_game_smoketest.o $(CORE_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

JAX_INCLUDE := $(shell $(PYTHON) -c "import jax; print(jax.ffi.include_dir())" 2>/dev/null)

$(BUILD)/%.o: %.cc
	@mkdir -p $(dir $@)
	@test -n "$(JAX_INCLUDE)" || { echo "jax headers not found; install jax or set JAX_INCLUDE="; false; }
	$(CXX) $(CXXFLAGS) -isystem $(JAX_INCLUDE) -c $< -o $@

lto: OPT := -O3 -flto
lto: LDFLAGS += -flto
lto: clean $(LIB)

PGO_DIR   := $(BUILD)/pgo
PROFDATA  := $(PGO_DIR)/merged.profdata
PROFTOOL  ?= $(shell command -v llvm-profdata 2>/dev/null || echo "xcrun llvm-profdata")
PGO_GAMES ?= tu93 ls20 m0r0

pgo:
	@echo "=== PGO 1/3: instrumented build ==="
	@$(MAKE) clean-obj
	@$(MAKE) OPT="$(OPT) -fprofile-instr-generate" LDFLAGS="-fprofile-instr-generate" harness
	@echo "=== PGO 2/3: profiling workload ($(PGO_GAMES)) ==="
	@mkdir -p $(PGO_DIR)
	@rm -f $(PGO_DIR)/*.profraw
	LLVM_PROFILE_FILE="$(PGO_DIR)/prof-%p.profraw" $(PYTHON) bench/pgo_workload.py $(PGO_GAMES)
	$(PROFTOOL) merge -output=$(PROFDATA) $(PGO_DIR)/*.profraw
	@echo "=== PGO 3/3: optimized rebuild ==="
	@$(MAKE) clean-obj
	@$(MAKE) OPT="$(OPT) -fprofile-instr-use=$(PROFDATA)" harness
	@echo "PGO build complete"

GAMES := ar25 cd82 cn04 dc22 ft09 g50t ka59 lp85 ls20 m0r0 r11l re86 s5i5 sb26 \
         sc25 sk48 sp80 su15 tn36 tr87 tu93 vc33 wa30
ACTIONS ?= 80

test: $(LIB_HARN)
	$(PYTHON) -m harness
bench: $(LIB_HARN)
	$(PYTHON) bench/bench_all.py


SOURCES := $(wildcard include/arc/*.h) $(wildcard src/*.c) $(wildcard src/games/*.c) \
           $(wildcard src/games/*.h) $(wildcard shims/*.c) $(wildcard bindings/*.c) \
           $(wildcard bindings/*.cc)

format:
	clang-format -i $(SOURCES)

check-format:
	@clang-format --dry-run --Werror $(SOURCES) && echo "formatting is clean"

clean-obj:
	rm -rf $(CORE_OBJ) $(HARN_OBJ) $(FFI_OBJ) $(ALL_OBJ:.o=.d)

clean:
	rm -rf $(BUILD)

help:
	@echo "targets:"
	@echo "  all/lib   $(LIB) - engine, driver, vecenv, 25 games. no dependencies"
	@echo "  harness   $(LIB_HARN) - adds the shims the Python difftests call"
	@echo "  ffi       $(LIB_FFI) - adds the XLA custom call. needs jax headers"
	@echo "  smoke     $(SMOKE) - standalone scene-backend smoke test"
	@echo "  lto       rebuild the library with link-time optimization"
	@echo "  pgo       profile-guided build, 3 stages"
	@echo "  test      differential sweep against the official Python, all games"
	@echo "  bench     per-game throughput benchmark"
	@echo "  format    reformat the C sources in kernel style"
	@echo "  check-format  fail if any source is not in kernel style"
	@echo "  clean     remove $(BUILD)"
	@echo "vars: CC CXX OPT ARCH PYTHON JAX_INCLUDE ACTIONS PGO_GAMES"

-include $(ALL_OBJ:.o=.d)
