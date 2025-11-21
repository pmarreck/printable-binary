# Makefile for PrintableBinary C Implementation
# Supports multiple compilers, optimization levels, and cross-platform builds

# Default compiler and flags
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I$(CURDIR)
LDFLAGS = 
BIN_DIR = bin
TARGET = printable_binary_c
SOURCE = src/printable_binary.c
WASM_TARGET = printable_binary.wasm
APE_TARGET = printable_binary_ape.com
MAP_COPY = $(BIN_DIR)/character_map.txt
STRIP ?= strip

COSMOCC_VERSION ?= 4.0.2
COSMOCC_URL ?= https://cosmo.zip/pub/cosmocc/cosmocc-$(COSMOCC_VERSION).zip
COSMOCC_DIR ?= $(CURDIR)/.cosmocc
COSMOCC_BIN ?= $(COSMOCC_DIR)/bin/cosmocc

# Only fetch Cosmopolitan toolchain when the caller hasn't overridden APE_CC
ifndef APE_CC
APE_CC := $(COSMOCC_BIN)
APE_CC_DEP := $(COSMOCC_BIN)
else
APE_CC_DEP :=
endif

APE_FLAGS = -O3 -DNDEBUG -I$(CURDIR)

# Optimization levels
CFLAGS_DEBUG = $(CFLAGS) -g -O0 -DDEBUG
CFLAGS_RELEASE = $(CFLAGS) -O3 -DNDEBUG -march=native -mtune=native
CFLAGS_SIZE = $(CFLAGS) -Os -DNDEBUG

# Whether to strip the APE output (set to 0 to keep symbols)
APE_STRIP ?= 1

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS specific flags
    CFLAGS += -mmacosx-version-min=10.9
endif
ifeq ($(UNAME_S),Linux)
    # Linux specific flags
    LDFLAGS += -static-libgcc
endif

# Emscripten configuration
EMCC ?= emcc
EMCFLAGS = $(CFLAGS)
EMFLAGS = -O3 -DNDEBUG \
	-s STANDALONE_WASM=1 \
	-s FILESYSTEM=1 \
	-s INITIAL_MEMORY=134217728 \
	-DPRINTABLE_BINARY_HELP_NAME=\"printable_binary\"
EM_CACHE_DIR ?= $(CURDIR)/.emscripten_cache

# Default target
.PHONY: all
all: release

# Release build (optimized)
.PHONY: release
release: $(TARGET)

$(TARGET): $(SOURCE) $(MAP_COPY) | $(BIN_DIR)
	$(CC) $(CFLAGS_RELEASE) $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# Debug build
.PHONY: debug
debug: $(TARGET)_debug

$(TARGET)_debug: $(SOURCE) $(MAP_COPY) | $(BIN_DIR)
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# Size-optimized build
.PHONY: size
size: $(TARGET)_size

$(TARGET)_size: $(SOURCE) $(MAP_COPY) | $(BIN_DIR)
	$(CC) $(CFLAGS_SIZE) $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# Compiler-specific builds
.PHONY: gcc
gcc:
	$(MAKE) CC=gcc release

.PHONY: clang
clang:
	$(MAKE) CC=clang release

# Cross-compilation targets
.PHONY: windows
windows: $(TARGET).exe

$(TARGET).exe: $(SOURCE) | $(BIN_DIR)
	x86_64-w64-mingw32-gcc $(CFLAGS_RELEASE) -o $(BIN_DIR)/$@ $<

# Static analysis
.PHONY: analyze
analyze:
	clang --analyze $(CFLAGS) $(SOURCE)
	cppcheck --enable=all --std=c99 $(SOURCE)

# Performance profiling build
.PHONY: profile
profile: $(TARGET)_profile

$(TARGET)_profile: $(SOURCE) | $(BIN_DIR)
	$(CC) $(CFLAGS_RELEASE) -pg $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# AddressSanitizer build
.PHONY: asan
asan: $(TARGET)_asan

$(TARGET)_asan: $(SOURCE) | $(BIN_DIR)
	$(CC) $(CFLAGS_DEBUG) -fsanitize=address -fno-omit-frame-pointer $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# Memory leak detection build
.PHONY: msan
msan: $(TARGET)_msan

$(TARGET)_msan: $(SOURCE) | $(BIN_DIR)
	clang $(CFLAGS_DEBUG) -fsanitize=memory -fno-omit-frame-pointer $(LDFLAGS) -o $(BIN_DIR)/$@ $<

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(MAP_COPY): character_map.txt | $(BIN_DIR)
	ln -sf ../$< $@

# WASM target (requires emscripten)
.PHONY: wasm
wasm: $(BIN_DIR)/$(WASM_TARGET)

$(BIN_DIR)/$(WASM_TARGET): $(SOURCE) $(MAP_COPY) | $(BIN_DIR)
	mkdir -p $(EM_CACHE_DIR)
	EM_CACHE=$(EM_CACHE_DIR) $(EMCC) $(EMCFLAGS) $(EMFLAGS) -o $@ $<

# Cosmopolitan APE target
.PHONY: ape
ape: $(BIN_DIR)/$(APE_TARGET)

$(COSMOCC_BIN):
	@if [ -z "$$CONFIRM_BIG_DEP_DOWNLOAD" ] || ! printf '%s\n' "$$CONFIRM_BIG_DEP_DOWNLOAD" | grep -Eiq '^(1|y|yes|true)$'; then \
		echo "cosmocc $(COSMOCC_VERSION) (~400MB) needs to be downloaded."; \
		echo "Set CONFIRM_BIG_DEP_DOWNLOAD=1 (or yes/true) to proceed."; \
		exit 1; \
	fi
	@echo "Downloading cosmocc $(COSMOCC_VERSION) to $(COSMOCC_DIR)..."
	mkdir -p $(COSMOCC_DIR)
	@if [ ! -f "$(COSMOCC_DIR)/cosmocc-$(COSMOCC_VERSION).zip" ]; then \
		curl -L "$(COSMOCC_URL)" -o $(COSMOCC_DIR)/cosmocc-$(COSMOCC_VERSION).zip; \
	else \
		echo "Reusing cached cosmocc-$(COSMOCC_VERSION).zip"; \
	fi
	cd $(COSMOCC_DIR) && unzip -oq -n cosmocc-$(COSMOCC_VERSION).zip

.PHONY: cosmocc
cosmocc: $(COSMOCC_BIN)
	@echo "Cosmopolitan toolchain ready at $(COSMOCC_BIN)"

$(BIN_DIR)/$(APE_TARGET): $(SOURCE) $(MAP_COPY) $(APE_CC_DEP) | $(BIN_DIR)
	$(APE_CC) $(APE_FLAGS) -o $@ $<
	chmod +x $@
	@if [ "$(APE_STRIP)" != "0" ]; then \
		$(STRIP) -s $@; \
	fi
	@if ! strings -a $@ | grep -q '.aarch64.elf'; then \
		echo "cosmocc did not embed the Apple Silicon/aarch64 slice; please use cosmocc >= 3.x or override COSMOCC_URL"; \
		exit 1; \
	fi

# Test targets
.PHONY: test
test: $(TARGET)
	cd test && IMPLEMENTATION_TO_TEST=../$(BIN_DIR)/$(TARGET) ./test_all

.PHONY: test-ape
test-ape: $(BIN_DIR)/$(APE_TARGET)
	cd test && IMPLEMENTATION_TO_TEST=../$(BIN_DIR)/$(APE_TARGET) ./test_all

# Performance comparison test
.PHONY: benchmark
benchmark: $(TARGET)
	@echo "Creating test file..."
	@dd if=/dev/urandom of=benchmark_test.bin bs=1024 count=100 2>/dev/null
	@echo "Benchmarking C version..."
	@echo "Encode:"
	@time $(BIN_DIR)/$(TARGET) benchmark_test.bin > benchmark_encoded.tmp 2>/dev/null
	@echo "Decode:"
	@time $(BIN_DIR)/$(TARGET) -d benchmark_encoded.tmp > benchmark_decoded.tmp 2>/dev/null
	@echo "Verifying roundtrip..."
	@if cmp benchmark_test.bin benchmark_decoded.tmp; then \
		echo "✓ Benchmark roundtrip successful"; \
	else \
		echo "✗ Benchmark roundtrip failed"; \
	fi
	@rm -f benchmark_test.bin benchmark_encoded.tmp benchmark_decoded.tmp

# Compare with LuaJIT version
.PHONY: compare
compare: $(TARGET)
	@if [ ! -f bin/printable_binary ]; then \
		echo "Error: LuaJIT version not found"; \
		exit 1; \
	fi
	@echo "Performance comparison between C and LuaJIT versions"
	@echo "==================================================="
	@echo "Creating 50KB test file..."
	@dd if=/dev/urandom of=compare_test.bin bs=1024 count=50 2>/dev/null
	@echo
	@echo "C version encoding:"
	@time $(BIN_DIR)/$(TARGET) compare_test.bin > compare_c_encoded.tmp 2>/dev/null
	@echo
	@echo "LuaJIT version encoding:"
	@time ./bin/printable_binary compare_test.bin > compare_lua_encoded.tmp 2>/dev/null
	@echo
	@echo "C version decoding:"
	@time $(BIN_DIR)/$(TARGET) -d compare_c_encoded.tmp > compare_c_decoded.tmp 2>/dev/null
	@echo
	@echo "LuaJIT version decoding:"
	@time ./bin/printable_binary -d compare_lua_encoded.tmp > compare_lua_decoded.tmp 2>/dev/null
	@echo
	@echo "Verifying output compatibility:"
	@if cmp compare_c_encoded.tmp compare_lua_encoded.tmp; then \
		echo "✓ Encoded outputs are identical"; \
	else \
		echo "✗ Encoded outputs differ"; \
	fi
	@if cmp compare_c_decoded.tmp compare_lua_decoded.tmp; then \
		echo "✓ Decoded outputs are identical"; \
	else \
		echo "✗ Decoded outputs differ"; \
	fi
	@rm -f compare_test.bin compare_*_encoded.tmp compare_*_decoded.tmp

# Hyperfine benchmark (if available)
.PHONY: hyperfine
hyperfine: $(TARGET)
	@if command -v hyperfine >/dev/null 2>&1; then \
		echo "Creating test file..."; \
		dd if=/dev/urandom of=hyperfine_test.bin bs=1024 count=50 2>/dev/null; \
		echo "Running hyperfine benchmark..."; \
		hyperfine --warmup 3 \
			"$(BIN_DIR)/$(TARGET) hyperfine_test.bin" \
		"./bin/printable_binary hyperfine_test.bin" \
			--export-markdown benchmark_results.md; \
		echo "Encode benchmark results saved to benchmark_results.md"; \
		$(BIN_DIR)/$(TARGET) hyperfine_test.bin > hyperfine_encoded.tmp 2>/dev/null; \
		hyperfine --warmup 3 \
			"$(BIN_DIR)/$(TARGET) -d hyperfine_encoded.tmp" \
		"./bin/printable_binary -d hyperfine_encoded.tmp" \
			--export-markdown decode_benchmark_results.md;
		echo "Decode benchmark results saved to decode_benchmark_results.md"; \
		rm -f hyperfine_test.bin hyperfine_encoded.tmp; \
	else \
		echo "hyperfine not found. Install it for detailed benchmarks."; \
		echo "On macOS: brew install hyperfine"; \
		echo "On Linux: apt-get install hyperfine or equivalent"; \
	fi

# Memory usage analysis
.PHONY: memcheck
memcheck: $(TARGET)_debug
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running memory check..."; \
		echo "Hello, World!" | valgrind --leak-check=full --show-leak-kinds=all $(BIN_DIR)/$(TARGET)_debug > /dev/null; \
	else \
		echo "Valgrind not available for memory checking"; \
	fi

# Install target
.PHONY: install
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BIN_DIR)/$(TARGET) $(DESTDIR)/usr/local/bin/

# Uninstall target
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

# Clean targets
.PHONY: clean
clean:
	rm -rf $(BIN_DIR)
	rm -f *.tmp *.o core
	rm -f *.plist  # Static analysis files
	rm -f gmon.out # Profiling files
	rm -f benchmark_results.md decode_benchmark_results.md
	rm -f *.bin *.txt  # Test artifacts
	rm -f encoded.txt decoded.txt formatted.txt pasted.txt
	rm -f email_*.txt test_*.txt test_*.bin reconstructed.bin decoded*.bin

.PHONY: distclean
distclean: clean
	rm -f *~

# Help target
.PHONY: help
help:
	@echo "PrintableBinary C Implementation Makefile"
	@echo "========================================"
	@echo ""
	@echo "Build targets:"
	@echo "  all           Build optimized release version (default)"
	@echo "  release       Build optimized release version"
	@echo "  debug         Build debug version with symbols"
	@echo "  size          Build size-optimized version"
	@echo "  wasm          Build WebAssembly module"
	@echo "  ape           Build Actually Portable Executable via cosmocc"
	@echo "  gcc           Build with GCC"
	@echo "  clang         Build with Clang"
	@echo "  windows       Cross-compile for Windows"
	@echo ""
	@echo "Analysis targets:"
	@echo "  analyze       Run static analysis"
	@echo "  profile       Build with profiling support"
	@echo "  asan          Build with AddressSanitizer"
	@echo "  msan          Build with MemorySanitizer"
	@echo "  memcheck      Run Valgrind memory check"
	@echo ""
	@echo "Test targets:"
	@echo "  test          Run basic functionality tests"
	@echo "  test-ape      Run full suite against the APE binary"
	@echo "  benchmark     Run performance benchmark"
	@echo "  compare       Compare with LuaJIT version"
	@echo "  hyperfine     Detailed benchmark with hyperfine"
	@echo ""
	@echo "Utility targets:"
	@echo "  install       Install to /usr/local/bin"
	@echo "  uninstall     Remove from /usr/local/bin"
	@echo "  clean         Remove build artifacts"
	@echo "  distclean     Remove all generated files"
	@echo "  help          Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build optimized version"
	@echo "  make debug              # Build debug version"
	@echo "  make CC=clang release   # Build with Clang"
	@echo "  make test               # Run tests"
	@echo "  make compare            # Compare with LuaJIT"
