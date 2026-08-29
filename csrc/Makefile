UNAME := $(shell uname -s)

CC      ?= cc
CXX     ?= c++
PYTHON  ?= uv run python

OPT     ?= -O3
ARCH    ?= -march=native
WARN    := -Wall -Wextra
CSTD    := -std=c11
CXXSTD  := -std=c++17
CFLAGS  := $(OPT) $(ARCH) $(WARN) $(CSTD) -fPIC -I.
CXXFLAGS := $(OPT) $(ARCH) $(WARN) $(CXXSTD) -fPIC -I.
LDFLAGS :=
LDLIBS  := -lpthread

ifeq ($(UNAME),Darwin)
  SO      := dylib
  SHARED  := -dynamiclib
else
  SO      := so
  SHARED  := -shared
endif

JAX_INCLUDE := $(shell $(PYTHON) -c "import jax; print(jax.ffi.include_dir())" 2>/dev/null)

BUILD   := build
LIB     := libarc.$(SO)
LIB_FFI := libarc_ffi.$(SO)
SCENELIB := libarc_scene.$(SO)

CORE_SRC  := engine.c game.c vecenv.c harness_helpers.c bench_driver.c shim.c \
             scene.c scene_game.c scene_shim.c
GAME_SRC  := $(wildcard games/*.c)
FFI_SRC   := ffi_typed.cc

CORE_OBJ  := $(addprefix $(BUILD)/,$(CORE_SRC:.c=.o))
GAME_OBJ  := $(addprefix $(BUILD)/,$(GAME_SRC:.c=.o))
FFI_OBJ   := $(BUILD)/ffi_typed.o

.PHONY: all lib ffi clean test bench pgo help
.DEFAULT_GOAL := all

all: lib

lib: $(LIB)

$(LIB): $(CORE_OBJ) $(GAME_OBJ)
	$(CC) $(SHARED) -o $@ $^ $(LDFLAGS) $(LDLIBS)

ffi: $(LIB_FFI)

$(LIB_FFI): $(CORE_OBJ) $(GAME_OBJ) $(FFI_OBJ)
	$(CXX) $(SHARED) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(FFI_OBJ): $(FFI_SRC) | $(BUILD)
	@test -n "$(JAX_INCLUDE)" || (echo "jax include dir not found; set JAX_INCLUDE=" && false)
	$(CXX) $(CXXFLAGS) -I$(JAX_INCLUDE) -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)/games

LTO_FLAGS := -flto
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
	@$(MAKE) OPT="$(OPT) -fprofile-instr-generate" LDFLAGS="-fprofile-instr-generate" lib
	@echo "=== PGO 2/3: profiling workload ($(PGO_GAMES)) ==="
	@mkdir -p $(PGO_DIR)
	@rm -f $(PGO_DIR)/*.profraw
	LLVM_PROFILE_FILE="$(PGO_DIR)/prof-%p.profraw" $(PYTHON) pgo_workload.py $(PGO_GAMES)
	$(PROFTOOL) merge -output=$(PROFDATA) $(PGO_DIR)/*.profraw
	@echo "=== PGO 3/3: optimized rebuild ==="
	@$(MAKE) clean-obj
	@$(MAKE) OPT="$(OPT) -fprofile-instr-use=$(PROFDATA)" lib
	@echo "PGO build complete: $(LIB)"

test:
	@for g in ar25 cd82 cn04 dc22 ft09 g50t ka59 lp85 ls20 m0r0 r11l re86 s5i5 sb26 sc25 sk48 sp80 su15 tn36 tr87 tu93 vc33 wa30; do \
	  printf "%-6s " $$g; \
	  $(PYTHON) ref_difftest.py $$g $(ACTIONS) 2>&1 | tail -1; \
	done
ACTIONS ?= 80

bench: lib
	$(PYTHON) env_scaling.py

clean-obj:
	rm -rf $(BUILD)/*.o $(BUILD)/games

clean:
	rm -rf $(BUILD) $(LIB) $(LIB_FFI)

help:
	@echo "targets:"
	@echo "  all/lib   build $(LIB) (engine + driver + games) - no JAX needed"
	@echo "  ffi       build $(LIB_FFI) (adds the XLA custom call; needs jax installed)"
	@echo "  lto       rebuild with link-time optimization"
	@echo "  pgo       profile-guided build (3 stages)"
	@echo "  test      differential sweep vs arcengine, all 23 games"
	@echo "  bench     throughput benchmark"
	@echo "  clean     remove build artifacts"
	@echo "vars: CC CXX OPT ARCH PYTHON JAX_INCLUDE ACTIONS"
