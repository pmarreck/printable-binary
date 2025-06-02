# PrintableBinary

A Lua library for encoding arbitrary binary data into human-readable UTF-8 text, and then decoding it back to the original binary data.

## Overview

PrintableBinary is designed to serialize binary data into a visually distinct, human-readable format that is also copy-pastable. It's an alternative to hexadecimal encoding that offers better visual density and makes embedded ASCII text immediately recognizable.

This implementation allows you to view binary data directly in a terminal without breaking the display, making it particularly useful for debugging, logging, and sharing binary data in human-readable form.

## Features

- **Visually Distinct Characters**: Each of the 256 possible byte values maps to a unique, visually distinct UTF-8 character.
- **ASCII Passthrough**: Standard printable ASCII characters (32-126) largely remain themselves for immediate recognition.
- **Special Handling**: Space, double quotes, and backslashes have distinct representations to avoid escaping issues.
- **Single Character Width**: Each encoded representation renders as a single character wide in a monospace terminal.
- **Compactness**: Uses 1-3 byte UTF-8 characters for optimal space efficiency.
- **Usability**: Encoded strings are easily copyable, pastable, and printable.
- **Disassembly**: Optional disassembly of binary files with auto-architecture detection or manual selection.
- **Formatting**: Customizable output formatting with group size and line width options.
- **Universal Binary Support**: Detects and clearly identifies macOS universal binaries with multiple architectures.
- **Intelligent Pattern Recognition**: Recognizes common byte patterns (NUL, NOP, INT3) and provides context-aware analysis to distinguish between code and data.
- **Binary Safety**: Preserves all binary data, including NUL bytes, when encoding and decoding.

## Usage

### As a Command Line Tool

```bash
# Encode binary data
echo -n "Hello, World!" | ./printable_binary
# Output: Hello,␣World!

# Encode a file
./printable_binary somefile.bin > encoded.txt

# Encode with formatting (groups of 8 characters, 10 groups per line)
./printable_binary -f somefile.bin > formatted_encoded.txt

# Encode with custom formatting (groups of 4 characters, 16 groups per line)
./printable_binary -f=4x16 somefile.bin > custom_formatted.txt

# Encode with disassembly (auto-detects architecture)
./printable_binary -a executable.bin > disassembled.txt

# Encode with both formatting and disassembly
./printable_binary -a -f=8x8 executable.bin > formatted_disassembly.txt

# Encode with specific architecture (useful for universal binaries)
./printable_binary -a --arch x64 universal_binary.bin > x64_disassembly.txt

# NOTE: Disassembly only processes a portion of the binary
# Decoding from disassembly will not reconstruct the full binary
# For universal binaries, it will only show one architecture
./printable_binary universal_binary.bin > full_binary.txt  # Use this for full binary preservation

# Decode data (spaces and newlines are automatically ignored during decoding)
echo -n "Hello,␣World!" | ./printable_binary -d
# Output: Hello, World!

# Decode formatted data (formatting is ignored)
cat formatted_encoded.txt | ./printable_binary -d > original.bin

# Decode disassembled data (disassembly info is ignored)
cat disassembled.txt | ./printable_binary -d > original_executable.bin
```

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

## Character Encoding

- **Control Characters (0-31)**: Mapped to visually distinct symbols like ∅, ¯, «, », etc.
- **Space (32)**: Encoded as ␣ for visibility
- **Printable ASCII (33-126)**: Mostly unchanged except for double quote (34) → ˵ and backslash (92) → Ʌ
- **DEL (127)**: Encoded as ⌦
- **Extended Bytes (128-255)**: Mapped to characters from Latin-1 Supplement and Latin Extended-A blocks

### Complete Character Mapping Reference

This detailed mapping table is provided to help others create compatible encoders/decoders in different languages:

| Byte Value | Character | Unicode | UTF-8 Bytes (hex) | Description |
|------------|-----------|---------|-------------------|-------------|
| 0 (NUL)    | ∅         | U+2205  | E2 88 85          | Empty Set |
| 1 (SOH)    | ¯         | U+00AF  | C2 AF             | Macron |
| 2 (STX)    | «         | U+00AB  | C2 AB             | Left-Pointing Double Angle Quotation Mark |
| 3 (ETX)    | »         | U+00BB  | C2 BB             | Right-Pointing Double Angle Quotation Mark |
| 4 (EOT)    | ϟ         | U+03DF  | CF 9F             | Greek Small Letter Koppa |
| 5 (ENQ)    | ¿         | U+00BF  | C2 BF             | Inverted Question Mark |
| 6 (ACK)    | ¡         | U+00A1  | C2 A1             | Inverted Exclamation Mark |
| 7 (BEL)    | ª         | U+00AA  | C2 AA             | Feminine Ordinal Indicator |
| 8 (BS)     | ⌫         | U+232B  | E2 8C AB          | Erase to the Left |
| 9 (HT)     | ⇥         | U+21E5  | E2 87 A5          | Rightwards Arrow to Bar |
| 10 (LF)    | ⇩         | U+21E9  | E2 87 A9          | Downwards White Arrow |
| 11 (VT)    | ↧         | U+21A7  | E2 86 A7          | Downwards Arrow from Bar |
| 12 (FF)    | §         | U+00A7  | C2 A7             | Section Sign |
| 13 (CR)    | ⏎         | U+23CE  | E2 8F 8E          | Return Symbol |
| 14 (SO)    | ȯ         | U+022F  | C8 AF             | Latin Small Letter O with Dot Above |
| 15 (SI)    | ʘ         | U+0298  | CA 98             | Latin Letter Bilabial Click |
| 16 (DLE)   | Ɣ         | U+0194  | C6 94             | Latin Capital Letter Gamma |
| 17 (DC1)   | ¹         | U+00B9  | C2 B9             | Superscript One |
| 18 (DC2)   | ²         | U+00B2  | C2 B2             | Superscript Two |
| 19 (DC3)   | º         | U+00BA  | C2 BA             | Masculine Ordinal Indicator |
| 20 (DC4)   | ³         | U+00B3  | C2 B3             | Superscript Three |
| 21 (NAK)   | Ͷ         | U+0376  | CD B6             | Greek Capital Letter Pamphylian Digamma |
| 22 (SYN)   | ɨ         | U+0268  | C9 A8             | Latin Small Letter I with Stroke |
| 23 (ETB)   | ¬         | U+00AC  | C2 AC             | Not Sign |
| 24 (CAN)   | ©         | U+00A9  | C2 A9             | Copyright Sign |
| 25 (EM)    | ¦         | U+00A6  | C2 A6             | Broken Bar |
| 26 (SUB)   | Ƶ         | U+01B5  | C6 B5             | Latin Capital Letter Z with Stroke |
| 27 (ESC)   | ⎋         | U+238B  | E2 8E 8B          | Broken Circle with Northwest Arrow |
| 28 (FS)    | Ξ         | U+039E  | CE 9E             | Greek Capital Letter Xi |
| 29 (GS)    | ǁ         | U+01C1  | C7 81             | Latin Letter Lateral Click |
| 30 (RS)    | ǀ         | U+01C0  | C7 80             | Latin Letter Dental Click |
| 31 (US)    | ¶         | U+00B6  | C2 B6             | Pilcrow Sign |
| 32 (Space) | ␣         | U+2423  | E2 90 A3          | Open Box |
| 34 (")     | ˵         | U+02F5  | CB B5             | Double Quote |
| 92 (\\)    | Ʌ         | U+0245  | C9 85             | Latin Capital Letter Turned V |
| 127 (DEL)  | ⌦         | U+2326  | E2 8C A6          | Erase to the Right |
| 152        | Ō         | U+014C  | C5 8C             | Latin Capital Letter O with Macron |
| 184        | ŏ         | U+014F  | C5 8F             | Latin Small Letter O with Breve |

Bytes 33-126 (printable ASCII, except 34 and 92) are represented as themselves.

Bytes 128-191 (excluding 152 and 184) are encoded as UTF-8 sequences with first byte 0xC3 (195) followed by the original byte value.

Bytes 192-255 are encoded as UTF-8 sequences with first byte 0xC4 (196) followed by ((byte value - 192) + 128).

## Running Tests

To run the entire test suite, including basic tests, disassembly tests, and fuzz tests:

```bash
./test
```

You can also run individual test suites:

```bash
# Basic functionality tests
./test.sh

# Disassembly feature tests
./disasm_test.sh

# Fuzz tests with random data
./fuzz_test.sh
```

## Requirements

- LuaJIT (tested with LuaJIT 2.0.5)

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
- Standard printable ASCII characters (33-126, except " and \) remain themselves
- Special characters (space, double quote, backslash) get more visible representations
- Extended bytes (128-255) use a systematic mapping to Latin-1 Supplement and Latin Extended-A blocks

### Encoding/Decoding Maps

The implementation builds two lookup tables at initialization:
- `encode_map`: Maps byte values (0-255) to their UTF-8 string representations
- `decode_map`: Maps UTF-8 string representations back to byte values

These bidirectional maps ensure efficient and accurate conversion in both directions.

## License

This project is licensed under the MIT License - see the LICENSE file for details.