# PrintableBinary

A cross-platform utility (LuaJIT, C, and JavaScript implementations) for encoding arbitrary binary data into human-readable UTF-8 text, and then decoding it back to the original binary data.

## Overview

PrintableBinary is designed to [de]serialize binary data to/from a visually distinct, human-readable format that is also copy-pastable and embeddable in any UTF-8-aware context. It's an alternative to hexadecimal encoding that offers better visual density and makes embedded ASCII text immediately recognizable, while also making it possible to incorporate binary data into text-based formats (such as JSON, TOML, XML, YAML, etc.) without escaping issues.

This implementation allows you to view binary data directly in a terminal (it even has a pipe inspection mode with `--passthrough`) without breaking the display, making it particularly useful for debugging, logging, sharing binary data in human-readable form, and even dragging files into a web UI for instant encode/decode.

## Features

- **Triple Implementations**: Available as LuaJIT script, compiled C binary, and JavaScript module (shared by the browser UI and Node.js tooling) for maximum flexibility
- **Web & Node.js Tooling**: Drag-and-drop browser interface and a Node-based CLI wrapper share the same encode/decode core for cross-platform workflows
- **Visually Distinct Characters**: Each of the 256 possible byte values maps to a unique, visually distinct UTF-8 character
- **ASCII Passthrough**: Standard printable ASCII characters (32-126) largely remain themselves for immediate recognition
- **Shell-Safe Encoding**: Special characters that could cause shell issues are encoded with safe Unicode alternatives
- **Single Character Width**: Each encoded representation renders as a single character wide in a monospace terminal
- **Compactness**: Uses 1-3 byte UTF-8 characters for optimal space efficiency
- **Usability**: Encoded strings are easily copyable, pastable, and printable
- **Smart Disassembly**: Format-aware disassembly using objdump that understands binary file structures (Mach-O, ELF, PE)
- **Raw Disassembly**: Direct byte-to-instruction disassembly using Capstone with auto-architecture detection or manual selection
- **Formatting**: Customizable output formatting with group size and line width options
- **Universal Binary Support**: Detects and clearly identifies macOS universal binaries with multiple architectures
- **Intelligent Pattern Recognition**: Recognizes common byte patterns (NUL, NOP, INT3) and provides context-aware analysis to distinguish between code and data
- **Binary Safety**: Preserves all binary data, including NUL bytes, when encoding and decoding
- **Passthrough Mode**: Simultaneously outputs original binary data to stdout and encoded text to stderr for flexible processing pipelines

### Compared to Hexadecimal Encodings

- **Higher on-screen density**: Hex consumes two glyphs per byte; PrintableBinary maps each byte to a single visible character, so you see roughly twice as much data per line while still preserving UTF-8 safety.
- **ASCII stands out**: Printable ASCII bytes are left untouched (except for shell-hostile symbols, which use look-alike substitutes), so embedded text is immediately readable instead of needing to mentally decode hex pairs.
- **Control characters are labeled**: Bytes 0–31 and DEL render as mnemonic symbols (`⏎`, `↧`, `⌫`, etc.), making structure and control flow obvious without extra tooling.
- **Trade-off**: Hex expands data by exactly 2× in bytes. PrintableBinary averages about 1.8–1.9× on real-world binaries (thanks to the many 1- and 2-byte UTF-8 mappings) and only approaches 3× in the worst case. The small extra cost buys markedly better readability and paste safety.

## Usage

### As a Command Line Tool

```bash
# Use any implementation:
# LuaJIT version: ./printable_binary
# C version:     ./bin/printable_binary_c
# Node.js CLI:   ./printable_binary_node.js
# (Examples below use the LuaJIT version; the others accept the same flags.)

# Encode binary data
echo -n "Hello, World!" | ./printable_binary
# Output: Hello,␣World﹗

# Note: Direct encoding of binary data as command-line arguments is not supported
# because shell environments cannot represent all binary data (such as NUL bytes)
# Always pipe input or specify a file to encode

# Encode a file
./printable_binary somefile.bin > encoded.txt

# Encode with formatting (groups of 8 characters, 10 groups per line)
./printable_binary -f somefile.bin > formatted_encoded.txt

# Encode with custom formatting (groups of 4 characters, 16 groups per line)
./printable_binary -f=4x16 somefile.bin > custom_formatted.txt

# Encode with raw disassembly (auto-detects architecture)
./printable_binary -a executable.bin > disassembled.txt

# Encode with smart disassembly (format-aware)
./printable_binary --smart-asm executable.bin > smart_disassembled.txt

# Encode with both formatting and disassembly
./printable_binary -a -f=8x8 executable.bin > formatted_disassembly.txt

# Encode with specific architecture (useful for universal binaries)
./printable_binary -a --arch x64 universal_binary.bin > x64_disassembly.txt

# NOTE: Disassembly only processes a portion of the binary
# Decoding from disassembly will not reconstruct the full binary
# For universal binaries, it will only show one architecture
./printable_binary universal_binary.bin > full_binary.txt  # Use this for full binary preservation

# Decode data (spaces and newlines are automatically ignored during decoding)
echo -n "Hello,␣World﹗" | ./printable_binary -d
# Output: Hello, World!

# Decode formatted data (formatting is ignored)
cat formatted_encoded.txt | ./printable_binary -d > original.bin

# Decode disassembled data (disassembly info is ignored)
cat disassembled.txt | ./printable_binary -d > original_executable.bin

# Use passthrough mode to output both original binary (stdout) and encoded text (stderr)
# This is useful for binary data processing pipelines that need both representations
echo -n "Hello, World!" | ./printable_binary --passthrough 2>encoded.txt | wc -c
# Binary data goes to stdout, encoded text to stderr

# Use the C implementation for better performance on large files
./bin/printable_binary_c large_file.bin > encoded_large.txt
```

### Web Interface

- Live demo: <https://pmarreck.github.io/printable-binary/>
- Drag-and-drop or browse to encode any file; `.pbt` uploads are automatically decoded back to their original binary.
- Large outputs (>1 MB) skip the textarea to avoid browser jank—use the Download button to grab the UTF-8 text.
- Default wrapping is 75 characters per line to balance readability and density; copy/download buttons reuse the exact bytes produced by the CLI and Node implementations.
- To hack locally, open `docs/index.html` (or `index.html`) in any modern browser; the page loads the shared `printable_binary.js` module with no build step required.

### As a Lua Library

```lua
local PrintableBinary = require("printable_binary")

-- Encode binary data
local binary_data = "Hello, World!"
local encoded = PrintableBinary.encode(binary_data)
print(encoded)  -- Output: Hello,␣World!

-- Decode back to binary
local decoded = PrintableBinary.decode(encoded)
print(decoded)  -- Output: Hello, World!
```

### As a JavaScript Module

```js
import PrintableBinary from './printable_binary.js';

const pb = new PrintableBinary();
const input = new Uint8Array([0x00, 0xFF, 0x41]);

// Encode to printable UTF-8
const encoded = pb.encode(input, { format: '75x1' });
console.log(encoded);

// Decode back to bytes
const decoded = pb.decode(encoded);
console.log(Array.from(decoded)); // [0, 255, 65]
```

The same module powers the browser UI and can be run in Node.js (ESM) or bundled for other environments.

### JavaScript CLI

For command-line parity with the LuaJIT/C tools, use the Node-based wrapper:

```bash
# Encode (auto-detects stdin vs. file)
./printable_binary_node.js input.bin > encoded.pbt

# Decode (whitespace is ignored automatically)
./printable_binary_node.js --decode encoded.pbt > restored.bin

# Apply formatting (e.g., 75 characters per line)
./printable_binary_node.js --format 75x1 input.bin > formatted.pbt

# Pipe data through stdin
cat input.bin | ./printable_binary_node.js -f=8x10 > encoded.txt
```

Supported flags: `-d/--decode`, `-f/--format NxM`, `-h/--help`. The CLI shares the exact encode/decode implementation with the browser UI. Disassembly options (`-a`, `--smart-asm`, etc.) are not available in the Node wrapper; use the LuaJIT or C binaries when you need Capstone/objdump features.

### Inspecting Streams (Passthrough Mode)

One powerful trick is to drop PrintableBinary into a pipeline so you can watch the encoded stream on stderr while the raw bytes continue downstream untouched:

```bash
# Monitor traffic but keep the pipeline lossless
tcpdump -i en0 -w - | \
  ./printable_binary --passthrough > capture.raw 2> capture.pbt

# Alternatively inspect a decompression stream:
gzip -c bigfile > /tmp/data.gz
gzip -dc /tmp/data.gz | \
  ./printable_binary --passthrough | md5sum
# stdout (original bytes) flows into md5sum; stderr shows the printable view.
```

Because `--passthrough` sends the original binary to stdout, you can insert PrintableBinary anywhere in a Unix pipeline for observability without modifying the data flow.

## Disassembly Features

PrintableBinary offers two modes for disassembling binary files, each with different strengths:

### Smart Disassembly (`--smart-asm`)

Uses `objdump` for format-aware disassembly that understands binary file structures:

```bash
# Smart disassembly - recommended for most use cases
./printable_binary --smart-asm /usr/bin/ls
./printable_binary --smart-asm -f=4x8 binary_file.exe
```

**Advantages:**

- ✅ Format-aware (understands Mach-O, ELF, PE formats)
- ✅ Only disassembles actual executable code sections
- ✅ Accurate disassembly with proper architecture detection
- ✅ Includes section headers and file format information
- ✅ Best for analyzing complete, well-formed binaries

**Requirements:** `objdump` (usually part of binutils)

### Raw Disassembly (`-a, --asm`)

Uses `cstool` (Capstone) for direct byte-to-instruction disassembly:

```bash
# Raw disassembly with auto-detection
./printable_binary -a binary_file

# Force specific architecture
./printable_binary -a --arch=arm64 data_file.bin
./printable_binary -a --arch=x64 shellcode.bin
```

**Advantages:**

- ✅ Works on any binary data, including fragments
- ✅ Faster performance
- ✅ Good for shellcode, raw code fragments, or data analysis
- ✅ Useful for seeing "what would this data look like as code"
- ✅ Cross-architecture analysis

**Requirements:** `cstool` (part of Capstone framework)

### When to Use Each Mode

| Use Case                        | Recommended Mode | Reason                                         |
| ------------------------------- | ---------------- | ---------------------------------------------- |
| Analyzing executables/libraries | `--smart-asm`    | Format-aware, shows only real code             |
| Raw shellcode analysis          | `-a, --asm`      | Works on code fragments                        |
| Memory dumps                    | `-a, --asm`      | No file format structure                       |
| Cross-architecture analysis     | `-a, --asm`      | Force interpretation as different arch         |
| Data section analysis           | `-a, --asm`      | See what data looks like as code               |
| Quick analysis                  | `--smart-asm`    | More accurate results                          |
| Research/debugging              | `-a, --asm`      | Raw interpretation without format intelligence |

### Examples

**Smart disassembly of a macOS binary:**

```bash
./printable_binary --smart-asm /usr/libexec/rosetta/runtime
# Output includes proper ARM64 disassembly with section information
```

**Raw disassembly for shellcode analysis:**

```bash
# Analyze potential shellcode
echo -n "4889e5" | xxd -r -p | ./printable_binary -a --arch=x64
```

**Cross-architecture analysis:**

```bash
# See what ARM code looks like when interpreted as x86
./printable_binary -a --arch=x32 /usr/bin/arm_binary
```

## Format Compatibility

The PrintableBinary character set is specifically designed to be highly compatible with common text formats:

### ✅ **Excellent Compatibility With:**

- **JSON** - Perfect in quoted strings (we re-encode `"` as `˵`)
- **XML/HTML** - Perfect in text content and attributes (no `<>&` in our encodings)
- **TOML** - Perfect in quoted strings
- **YAML** - Perfect in quoted strings, good in unquoted context
- **C/C++/Java/etc.** - Perfect in string literals (we re-encode `\` as `⧹`)
- **Shell scripts** - Perfect in quoted strings (we re-encode `'` as `ʼ`)
- **SQL** - Perfect in quoted strings
- **Most UTF-8 aware text formats**

### 🎯 **Key Design Decisions for Compatibility:**

- **Double quotes** (34) → `˵` (U+02F5) - Avoids JSON/XML attribute conflicts
- **Single quotes** (39) → `ʼ` (U+02BC) - Avoids shell/SQL conflicts
- **Backslashes** (92) → `⧹` (U+29F9) - Avoids escape sequence issues
- **Control characters** → Safe Unicode symbols (∅, ⇩, ⏎, etc.)
- **No problematic delimiters** in our special encodings

### 📝 **Usage Recommendations:**

```bash
# JSON
echo '{"binary_data": "'$(./printable_binary file.bin)'"}'

# XML/HTML
echo '<data>'$(./printable_binary file.bin)'</data>'

# YAML
echo 'data: "'$(./printable_binary file.bin)'"'

# Shell variable
DATA="$(./printable_binary file.bin)"

# C string literal
printf 'char data[] = "%s";\n' "$(./printable_binary file.bin)"
```

**Note:** If your original binary contains problematic characters (like `<` or `{`), they'll appear as-is since they're printable ASCII. Use quoted contexts when embedding in structured formats.

## Character Encoding

- **Control Characters (0-31)**: Mapped to visually distinct symbols like ∅, ¯, «, », µ, etc.
- **Space (32)**: Encoded as ␣ for visibility
- **Shell-unsafe ASCII characters**: Mapped to safe Unicode alternatives:
  - Exclamation mark (33) → ﹗ (U+FE57) Small Exclamation Mark
  - Double quote (34) → ˵ (U+02F5) Modifier Letter Middle Double Grave Accent
  - Hash (35) → ♯ (U+266F) Music Sharp Sign
  - Dollar sign (36) → ﹩ (U+FE69) Small Dollar Sign
  - Percent (37) → ﹪ (U+FE6A) Small Percent Sign
  - Ampersand (38) → ﹠ (U+FE60) Small Ampersand
  - Single quote (39) → ʼ (U+02BC) Modifier Letter Apostrophe
  - Parentheses (40-41) → ❨❩ (U+2768-2769) Medium Parenthesis Ornaments
  - Asterisk (42) → ﹡ (U+FE61) Small Asterisk
  - Plus (43) → ﹢ (U+FE62) Small Plus Sign
  - Minus (45) → ﹣ (U+FE63) Small Hyphen-Minus
  - Slash (47) → ⁄ (U+2044) Fraction Slash
  - Colon (58) → ﹕ (U+FE55) Small Colon
  - Semicolon (59) → ﹔ (U+FE54) Small Semicolon
  - Equals (61) → ﹦ (U+FE66) Small Equals Sign
  - Question mark (63) → ﹖ (U+FE56) Small Question Mark
  - At sign (64) → ﹫ (U+FE6B) Small Commercial At
  - Backslash (92) → ⧹ (U+29F9) Big Reverse Solidus
  - Brackets (91, 93) → ⟦⟧ (U+27E6-27E7) Mathematical White Square Brackets
  - Backtick (96) → ˋ (U+02CB) Modifier Letter Grave Accent
  - Braces (123-125) → ❴∣❵ (Ornament and mathematical variants)
  - Tilde (126) → ˜ (U+02DC) Small Tilde
- **DEL (127)**: Encoded as ⌦
- **Extended Bytes (128-255)**: Mapped to characters from Latin-1 Supplement and Latin Extended-A blocks

### Complete Character Mapping Reference

This detailed mapping table is provided to help others create compatible encoders/decoders in different languages:

| Byte Value | Character | Unicode | UTF-8 Bytes (hex) | Description                                |
| ---------- | --------- | ------- | ----------------- | ------------------------------------------ |
| 0 (NUL)    | ∅         | U+2205  | E2 88 85          | Empty Set                                  |
| 1 (SOH)    | ¯         | U+00AF  | C2 AF             | Macron                                     |
| 2 (STX)    | «         | U+00AB  | C2 AB             | Left-Pointing Double Angle Quotation Mark  |
| 3 (ETX)    | »         | U+00BB  | C2 BB             | Right-Pointing Double Angle Quotation Mark |
| 4 (EOT)    | ϟ         | U+03DF  | CF 9F             | Greek Small Letter Koppa                   |
| 5 (ENQ)    | ¿         | U+00BF  | C2 BF             | Inverted Question Mark                     |
| 6 (ACK)    | ¡         | U+00A1  | C2 A1             | Inverted Exclamation Mark                  |
| 7 (BEL)    | ª         | U+00AA  | C2 AA             | Feminine Ordinal Indicator                 |
| 8 (BS)     | ⌫         | U+232B  | E2 8C AB          | Erase to the Left                          |
| 9 (HT)     | ⇥         | U+21E5  | E2 87 A5          | Rightwards Arrow to Bar                    |
| 10 (LF)    | ⇩         | U+21E9  | E2 87 A9          | Downwards White Arrow                      |
| 11 (VT)    | ↧         | U+21A7  | E2 86 A7          | Downwards Arrow from Bar                   |
| 12 (FF)    | §         | U+00A7  | C2 A7             | Section Sign                               |
| 13 (CR)    | ⏎         | U+23CE  | E2 8F 8E          | Return Symbol                              |
| 14 (SO)    | ȯ         | U+022F  | C8 AF             | Latin Small Letter O with Dot Above        |
| 15 (SI)    | ʘ         | U+0298  | CA 98             | Latin Letter Bilabial Click                |
| 16 (DLE)   | Ɣ         | U+0194  | C6 94             | Latin Capital Letter Gamma                 |
| 17 (DC1)   | ¹         | U+00B9  | C2 B9             | Superscript One                            |
| 18 (DC2)   | ²         | U+00B2  | C2 B2             | Superscript Two                            |
| 19 (DC3)   | º         | U+00BA  | C2 BA             | Masculine Ordinal Indicator                |
| 20 (DC4)   | ³         | U+00B3  | C2 B3             | Superscript Three                          |
| 21 (NAK)   | µ         | U+00B5  | C2 B5             | Micro Sign                                 |
| 22 (SYN)   | ɨ         | U+0268  | C9 A8             | Latin Small Letter I with Stroke           |
| 23 (ETB)   | ¬         | U+00AC  | C2 AC             | Not Sign                                   |
| 24 (CAN)   | ©         | U+00A9  | C2 A9             | Copyright Sign                             |
| 25 (EM)    | ¦         | U+00A6  | C2 A6             | Broken Bar                                 |
| 26 (SUB)   | Ƶ         | U+01B5  | C6 B5             | Latin Capital Letter Z with Stroke         |
| 27 (ESC)   | ⎋         | U+238B  | E2 8E 8B          | Broken Circle with Northwest Arrow         |
| 28 (FS)    | Ξ         | U+039E  | CE 9E             | Greek Capital Letter Xi                    |
| 29 (GS)    | ǁ         | U+01C1  | C7 81             | Latin Letter Lateral Click                 |
| 30 (RS)    | ǀ         | U+01C0  | C7 80             | Latin Letter Dental Click                  |
| 31 (US)    | ¶         | U+00B6  | C2 B6             | Pilcrow Sign                               |
| 32 (Space) | ␣         | U+2423  | E2 90 A3          | Open Box                                   |
| 34 (")     | ˵         | U+02F5  | CB B5             | Double Quote                               |
| 39 (')     | ʼ         | U+02BC  | CA BC             | Modifier Letter Apostrophe                 |
| 92 (\\)    | ⧹         | U+29F9  | E2 A7 B9          | Big Reverse Solidus                        |
| 127 (DEL)  | ⌦         | U+2326  | E2 8C A6          | Erase to the Right                         |
| 152        | Ę         | U+0118  | C4 98             | Latin Capital Letter E with Ogonek         |
| 184        | ĸ         | U+0138  | C4 B8             | Latin Small Letter Kra                     |

Bytes 33-126 (printable ASCII, except 34, 39, and 92) are represented as themselves.

Bytes 128-191 are encoded as UTF-8 sequences in the U+0100–U+013F range (`C4 80` through `C4 BF`).

Bytes 192-255 are encoded as UTF-8 sequences in the U+00C0–U+00FF range (`C3 80` through `C3 BF`).

## Running Tests

The project includes three types of test suites:

### Deterministic Unit Tests

These tests validate basic functionality and expected behavior:

```bash
./test
```

### Non-deterministic Fuzz Tests

These tests run randomized inputs to verify robustness:

```bash
./fuzz_test
```

### Performance Benchmark Tests

These tests measure encoding and decoding performance:

```bash
./benchmark_test
```

### Running All Tests

To run all test suites at once:

```bash
./test_all
```

## Utilities

The project includes several utility scripts in the `utils/` directory:

- `xxhash32`: Standard XXH32 hash utility (supports binary/hex/encoded output)
- `prng`: Deterministic pseudo-random number generator using XXH32 (supports seeded and auto-seeded generation)

## Requirements

### LuaJIT Implementation

- LuaJIT (tested with LuaJIT 2.0.5)

### C Implementation

- C99-compatible compiler (GCC, Clang)
- Standard C library

### Optional Dependencies (for disassembly features)

- `cstool` (Capstone disassembly engine) for raw disassembly (`-a/--asm`)
- `objdump` for smart disassembly (`--smart-asm`)

### Build

```bash
# Build C implementation
make

# Both implementations are included:
# ./printable_binary (LuaJIT script)
# ./bin/printable_binary_c (compiled C binary)
```

### Nix Development Environment

If you're using Nix, the included `flake.nix` provides a full development shell:

```bash
nix develop        # drops you into a shell with gcc/clang, LuaJIT, Deno, etc.
nix build          # builds the optimized C binary via the default package output
```

The shell hook lists the major tools (compilers, debuggers, benchmarking utilities) that are available. This is the easiest way to ensure all optional dependencies—such as LuaJIT for the script version and Deno/Node tooling for the JS implementation—are present.

## Implementation Details

### Algorithm Overview

For encoding:

1. Each byte of the input binary data is processed individually
2. The byte value (0-255) is used as a key to look up the corresponding UTF-8 representation
3. The encoded representations are concatenated to form the output string

For decoding:

1. The input string is processed from left to right
2. At each position, the decoder attempts to match the longest possible UTF-8 sequence (3, 2, or 1 bytes)
3. When a match is found, the corresponding byte value is output
4. This continues until the entire input is processed

### UTF-8 Encoding Strategy

This implementation uses a carefully chosen set of UTF-8 characters to represent each possible byte value:

- Control characters (0-31) use visually distinct symbols, primarily from Unicode blocks like Mathematical Symbols, Arrows, and Latin Extended
- Standard printable ASCII characters (33-126, except ", ', and \\) remain themselves
- Special characters (space, double quote, single quote, backslash) get more visible representations
- Extended bytes (128-255) use a systematic mapping to Latin-1 Supplement and Latin Extended-A blocks

### Encoding/Decoding Maps

The implementation builds two lookup tables at initialization:

- `encode_map`: Maps byte values (0-255) to their UTF-8 string representations
- `decode_map`: Maps UTF-8 string representations back to byte values

These bidirectional maps ensure efficient and accurate conversion in both directions.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
