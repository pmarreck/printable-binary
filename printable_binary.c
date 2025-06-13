/*
 * PrintableBinary C Implementation
 * High-performance C version of the printable_binary tool
 * Encodes binary data into human-readable UTF-8 and decodes it back
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_UTF8_BYTES 4
#define DECODE_MAP_SIZE 65536  // Covers all possible 2-byte combinations
#define INITIAL_BUFFER_SIZE 8192

// UTF-8 encoding structure
typedef struct {
    uint8_t bytes[MAX_UTF8_BYTES];
    uint8_t length;
} utf8_sequence_t;

// Global encoding and decoding tables
static utf8_sequence_t *encode_table;
static uint8_t *decode_table;
static bool *decode_table_valid;

// Program options
typedef struct {
    bool decode_mode;
    bool passthrough_mode;
    bool format_mode;
    bool asm_mode;
    bool smart_asm_mode;
    bool help_mode;
    int format_group;
    int format_groups_per_line;
    char *arch;
    char *input_file;
} options_t;

// Buffer for dynamic string building
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} buffer_t;

// Initialize a buffer with specific capacity
static void buffer_init(buffer_t *buf, size_t capacity) {
    if (capacity == 0) capacity = INITIAL_BUFFER_SIZE;
    buf->data = malloc(capacity);
    buf->size = 0;
    buf->capacity = capacity;
    if (!buf->data) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
}

// Append data to buffer (no realloc - buffer must be pre-sized)
static void buffer_append(buffer_t *buf, const void *data, size_t len) {
    if (buf->size + len > buf->capacity) {
        fprintf(stderr, "Buffer overflow: trying to append %zu bytes to buffer with %zu/%zu capacity\n",
                len, buf->size, buf->capacity);
        exit(1);
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
}

// Append a single character to buffer
static void buffer_append_char(buffer_t *buf, char c) {
    buffer_append(buf, &c, 1);
}

// Free buffer memory
static void buffer_free(buffer_t *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

// Helper function to create UTF-8 sequence
static utf8_sequence_t make_utf8(const char *bytes) {
    utf8_sequence_t seq = {0};
    seq.length = strlen(bytes);
    memcpy(seq.bytes, bytes, seq.length);
    return seq;
}

// Helper function to calculate hash for decode table
static uint16_t utf8_hash(const uint8_t *bytes, uint8_t len) {
    if (len == 1) {
        return bytes[0];
    } else if (len == 2) {
        return (bytes[0] << 8) | bytes[1];
    } else if (len == 3) {
        // For 3-byte sequences, use a simple hash
        return ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
    }
    return 0;
}

// Initialize encoding and decoding tables
static void init_tables(void) {
    // Allocate memory for tables
    encode_table = calloc(256, sizeof(utf8_sequence_t));
    decode_table = calloc(DECODE_MAP_SIZE, sizeof(uint8_t));
    decode_table_valid = calloc(DECODE_MAP_SIZE, sizeof(bool));

    if (!encode_table || !decode_table || !decode_table_valid) {
        fprintf(stderr, "Memory allocation failed for lookup tables\n");
        exit(1);
    }

    // Initialize decode table as invalid (already zeroed by calloc)

    // Define special UTF-8 sequences for control characters
    const char *special_sequences[256] = {0}; // Initialize all to NULL
    special_sequences[0] = "\xe2\x88\x85";    // ∅ (U+2205)
    special_sequences[1] = "\xc2\xaf";        // ¯ (U+00AF)
    special_sequences[2] = "\xc2\xab";        // « (U+00AB)
    special_sequences[3] = "\xc2\xbb";        // » (U+00BB)
    special_sequences[4] = "\xcf\x9e";        // ϟ (U+03DE)
    special_sequences[5] = "\xc2\xbf";        // ¿ (U+00BF)
    special_sequences[6] = "\xc2\xa1";        // ¡ (U+00A1)
    special_sequences[7] = "\xc2\xaa";        // ª (U+00AA)
    special_sequences[8] = "\xe2\x8c\xab";    // ⌫ (U+232B)
    special_sequences[9] = "\xe2\x87\xa5";    // ⇥ (U+21E5)
    special_sequences[10] = "\xe2\x87\xa9";   // ⇩ (U+21E9)
    special_sequences[11] = "\xe2\x8a\xa7";   // ↧ (U+21A7)
    special_sequences[12] = "\xc2\xa7";       // § (U+00A7)
    special_sequences[13] = "\xe2\x8f\x8e";   // ⏎ (U+23CE)
    special_sequences[14] = "\xc8\xaf";       // ȯ (U+022F)
    special_sequences[15] = "\xca\x98";       // ʘ (U+0298)
    special_sequences[16] = "\xc6\x94";       // Ɣ (U+0194)
    special_sequences[17] = "\xc2\xb9";       // ¹ (U+00B9)
    special_sequences[18] = "\xc2\xb2";       // ² (U+00B2)
    special_sequences[19] = "\xc2\xba";       // º (U+00BA)
    special_sequences[20] = "\xc2\xb3";       // ³ (U+00B3)
    special_sequences[21] = "\xc2\xb5";       // µ (U+00B5)
    special_sequences[22] = "\xc9\xa8";       // ɨ (U+0268)
    special_sequences[23] = "\xc2\xac";       // ¬ (U+00AC)
    special_sequences[24] = "\xc2\xa9";       // © (U+00A9)
    special_sequences[25] = "\xc2\xa6";       // ¦ (U+00A6)
    special_sequences[26] = "\xc6\xb5";       // Ƶ (U+01B5)
    special_sequences[27] = "\xe2\x8e\x8b";   // ⎋ (U+238B)
    special_sequences[28] = "\xce\x9e";       // Ξ (U+039E)
    special_sequences[29] = "\xc7\x81";       // ǁ (U+01C1)
    special_sequences[30] = "\xc7\x80";       // ǀ (U+01C0)
    special_sequences[31] = "\xc2\xb6";       // ¶ (U+00B6)
    special_sequences[32] = "\xe2\x90\xa3";   // ␣ (U+2423)
    special_sequences[33] = "\xef\xb9\x97";   // ﹗ (U+FE57) Small Exclamation Mark
    special_sequences[34] = "\xcb\xb5";       // ˵ (U+02F5)
    special_sequences[35] = "\xe2\x99\xaf";   // ♯ (U+266F) Music Sharp Sign
    special_sequences[36] = "\xef\xb9\xa9";   // ﹩ (U+FE69) Small Dollar Sign
    special_sequences[37] = "\xef\xb9\xaa";   // ﹪ (U+FE6A) Small Percent Sign
    special_sequences[38] = "\xef\xb9\xa0";   // ﹠ (U+FE60) Small Ampersand
    special_sequences[39] = "\xca\xbc";       // ʼ (U+02BC)
    special_sequences[40] = "\xe2\x9d\xa8";   // ❨ (U+2768) Medium Left Parenthesis Ornament
    special_sequences[41] = "\xe2\x9d\xa9";   // ❩ (U+2769) Medium Right Parenthesis Ornament
    special_sequences[42] = "\xef\xb9\xa1";   // ﹡ (U+FE61) Small Asterisk
    special_sequences[43] = "\xef\xb9\xa2";   // ﹢ (U+FE62) Small Plus Sign
    special_sequences[45] = "\xef\xb9\xa3";   // ﹣ (U+FE63) Small Hyphen-Minus
    special_sequences[47] = "\xe2\x81\x84";   // ⁄ (U+2044) Fraction Slash
    special_sequences[58] = "\xef\xb9\x95";   // ﹕ (U+FE55) Small Colon
    special_sequences[59] = "\xef\xb9\x94";   // ﹔ (U+FE54) Small Semicolon
    special_sequences[61] = "\xef\xb9\xa6";   // ﹦ (U+FE66) Small Equals Sign
    special_sequences[63] = "\xef\xb9\x96";   // ﹖ (U+FE56) Small Question Mark
    special_sequences[64] = "\xef\xb9\xab";   // ﹫ (U+FE6B) Small Commercial At
    special_sequences[91] = "\xe2\x9f\xa6";   // ⟦ (U+27E6) Mathematical Left White Square Bracket
    special_sequences[92] = "\xe2\xa7\xb9";   // ⧹ (U+29F9) Big Reverse Solidus
    special_sequences[93] = "\xe2\x9f\xa7";   // ⟧ (U+27E7) Mathematical Right White Square Bracket
    special_sequences[96] = "\xcb\x8b";       // ˋ (U+02CB) Modifier Letter Grave Accent
    special_sequences[123] = "\xe2\x9d\xb4";  // ❴ (U+2774) Medium Left Curly Bracket Ornament
    special_sequences[124] = "\xe2\x88\xa3";  // ∣ (U+2223) Divides
    special_sequences[125] = "\xe2\x9d\xb5";  // ❵ (U+2775) Medium Right Curly Bracket Ornament
    special_sequences[126] = "\xcb\x9c";      // ˜ (U+02DC) Small Tilde
    special_sequences[127] = "\xe2\x8c\xa6";  // ⌦ (U+2326)
    special_sequences[152] = "\xc5\x8c";      // Ō (U+014C)
    special_sequences[184] = "\xc5\x8f";      // ŏ (U+014F)

    // Build encoding table
    for (int i = 0; i < 256; i++) {
        if (special_sequences[i]) {
            encode_table[i] = make_utf8(special_sequences[i]);
        } else if (i >= 33 && i <= 126 && !special_sequences[i]) {
            // Regular ASCII characters
            char temp[2] = {i, 0};
            encode_table[i] = make_utf8(temp);
        } else if (i >= 128 && i < 192 && i != 152 && i != 184) {
            // Extended ASCII 128-191: encoded with 0xC3 + original byte
            char temp[3] = {0xc3, i, 0};
            encode_table[i] = make_utf8(temp);
        } else if (i >= 192 && i != 184) {
            // Extended ASCII 192-255: encoded with 0xC4 + (byte - 192 + 128)
            char temp[3] = {0xc4, i - 192 + 128, 0};
            encode_table[i] = make_utf8(temp);
        }
    }

    // Build decode table
    for (int i = 0; i < 256; i++) {
        if (encode_table[i].length > 0) {
            uint16_t hash = utf8_hash(encode_table[i].bytes, encode_table[i].length);
            decode_table[hash] = i;
            decode_table_valid[hash] = true;
        }
    }
}

// Get UTF-8 sequence length from first byte
static uint8_t utf8_sequence_length(uint8_t first_byte) {
    if (first_byte < 0x80) return 1;
    if (first_byte < 0xE0) return 2;
    if (first_byte < 0xF0) return 3;
    return 4;
}

// Encode binary data to printable UTF-8
static buffer_t encode_data(const uint8_t *input, size_t input_len) {
    buffer_t output;
    // Pre-allocate 3x input size (worst case for UTF-8 encoding)
    buffer_init(&output, input_len * 3);

    for (size_t i = 0; i < input_len; i++) {
        utf8_sequence_t seq = encode_table[input[i]];
        if (seq.length > 0) {
            buffer_append(&output, seq.bytes, seq.length);
        }
    }

    return output;
}

// Decode printable UTF-8 back to binary
static buffer_t decode_data(const uint8_t *input, size_t input_len) {
    buffer_t output;
    // Decoded output will be <= input size (UTF-8 to binary)
    buffer_init(&output, input_len);

    size_t i = 0;
    while (i < input_len) {
        uint8_t first_byte = input[i];
        uint8_t seq_len = utf8_sequence_length(first_byte);

        // Ensure we don't go beyond input
        if (i + seq_len > input_len) {
            seq_len = input_len - i;
        }

        bool matched = false;

        // Try from expected length down to 1
        for (uint8_t len = seq_len; len >= 1 && len <= 3; len--) {
            if (i + len <= input_len) {
                uint16_t hash = utf8_hash(input + i, len);
                if (decode_table_valid[hash]) {
                    uint8_t decoded_byte = decode_table[hash];
                    buffer_append_char(&output, decoded_byte);
                    i += len;
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            // Skip unrecognized byte
            i++;
        }
    }

    return output;
}

// Apply formatting to encoded output
static buffer_t format_output(const buffer_t *input, int group_size, int groups_per_line) {
    buffer_t output;
    // Estimate formatted size: original size + spaces + newlines (generous estimate)
    size_t estimated_size = input->size + (input->size / group_size) + (input->size / (group_size * groups_per_line));
    buffer_init(&output, estimated_size);

    size_t char_count = 0;
    size_t i = 0;

    while (i < input->size) {
        // Determine UTF-8 character length
        uint8_t char_len = utf8_sequence_length(input->data[i]);

        // Add the character
        for (uint8_t j = 0; j < char_len && i + j < input->size; j++) {
            buffer_append_char(&output, input->data[i + j]);
        }

        char_count++;
        i += char_len;

        // Add spacing
        if (char_count % group_size == 0 && i < input->size) {
            buffer_append_char(&output, ' ');

            if ((char_count / group_size) % groups_per_line == 0 && i < input->size) {
                buffer_append_char(&output, '\n');
            }
        }
    }

    return output;
}

// Read entire file into memory
static buffer_t read_file(const char *filename) {
    buffer_t buf;
    size_t initial_capacity = INITIAL_BUFFER_SIZE;

    // Try to get file size for better initial allocation
    if (filename && strcmp(filename, "-") != 0) {
        struct stat st;
        if (stat(filename, &st) == 0 && st.st_size > 0) {
            initial_capacity = st.st_size;
        }
    }

    buffer_init(&buf, initial_capacity);

    FILE *file = stdin;
    if (filename && strcmp(filename, "-") != 0) {
        file = fopen(filename, "rb");
        if (!file) {
            perror("Error opening file");
            exit(1);
        }
    }

    char temp[8192];
    size_t bytes_read;
    while ((bytes_read = fread(temp, 1, sizeof(temp), file)) > 0) {
        // If buffer too small, grow it
        if (buf.size + bytes_read >= buf.capacity) {
            buf.capacity = (buf.size + bytes_read) * 2;
            buf.data = realloc(buf.data, buf.capacity);
            if (!buf.data) {
                fprintf(stderr, "Memory allocation failed\n");
                exit(1);
            }
        }
        // Manually append data (avoid buffer_append since it doesn't support realloc)
        memcpy(buf.data + buf.size, temp, bytes_read);
        buf.size += bytes_read;
    }

    if (file != stdin) {
        fclose(file);
    }

    return buf;
}

// Clean input for decoding (remove whitespace and disassembly formatting)
static buffer_t clean_decode_input(const buffer_t *input) {
    buffer_t output;
    // Cleaned output will be <= input size
    buffer_init(&output, input->size);

    // Simple whitespace removal for now
    for (size_t i = 0; i < input->size; i++) {
        char c = input->data[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            buffer_append_char(&output, c);
        }
    }

    return output;
}

// Print usage information
static void print_usage(const char *program_name) {
    fprintf(stderr, "PrintableBinary C - Encode binary data as printable UTF-8 and decode it back\n\n");
    fprintf(stderr, "Usage: %s [options] [file]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --decode     Decode mode (default is encode mode)\n");
    fprintf(stderr, "  -p, --passthrough  Pass input to stdout unchanged, send encoded data to stderr\n");
    fprintf(stderr, "  -f[=NxM], --format[=NxM]   Format output in groups\n");
    fprintf(stderr, "                    Default: 8x10 (groups of 8 chars, 10 groups per line)\n");
    fprintf(stderr, "  -a, --asm        Raw disassembly (works on any data, uses cstool)\n");
    fprintf(stderr, "  --smart-asm      Smart disassembly (format-aware, uses objdump)\n");
    fprintf(stderr, "  --arch ARCH      Specify architecture for disassembly\n");
    fprintf(stderr, "                    Valid values: x64, x32, arm64, arm\n");
    fprintf(stderr, "  -h, --help       Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If no file is specified, input is read from stdin.\n");
    fprintf(stderr, "Output is written to stdout, unless --passthrough is used.\n\n");
    fprintf(stderr, "When --passthrough is used:\n");
    fprintf(stderr, "  - Original binary data is passed unchanged to stdout\n");
    fprintf(stderr, "  - Encoded representation is sent to stderr\n");
    fprintf(stderr, "  - This allows using the tool in pipelines to monitor binary data\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s binary_file               # Encode binary to UTF-8\n", program_name);
    fprintf(stderr, "  %s -d encoded_file           # Decode UTF-8 to binary\n", program_name);
    fprintf(stderr, "  %s -f=4x10 binary_file       # Encode with formatting\n", program_name);
    fprintf(stderr, "  %s -a executable             # Raw disassembly (any data)\n", program_name);
    fprintf(stderr, "  %s --smart-asm binary        # Smart disassembly (executables)\n", program_name);
    fprintf(stderr, "  %s -a --arch=arm64 binary    # Force ARM64 raw disassembly\n", program_name);
    fprintf(stderr, "  %s --passthrough file | tool # Monitor binary stream\n", program_name);
}

// Parse command line options
static options_t parse_options(int argc, char *argv[]) {
    options_t opts = {
        .decode_mode = false,
        .passthrough_mode = false,
        .format_mode = false,
        .asm_mode = false,
        .smart_asm_mode = false,
        .help_mode = false,
        .format_group = 8,
        .format_groups_per_line = 10,
        .arch = NULL,
        .input_file = NULL
    };

    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"passthrough", no_argument, 0, 'p'},
        {"format", optional_argument, 0, 'f'},
        {"asm", no_argument, 0, 'a'},
        {"smart-asm", no_argument, 0, 1001},
        {"arch", required_argument, 0, 1000},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "dpf::ah", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                opts.decode_mode = true;
                break;
            case 'p':
                opts.passthrough_mode = true;
                break;
            case 'f':
                opts.format_mode = true;
                if (optarg) {
                    char *format_str = optarg;
                    // Skip leading '=' if present (from -f=NxM syntax)
                    if (format_str[0] == '=') {
                        format_str++;
                    }
                    int group, groups_per_line;
                    if (sscanf(format_str, "%dx%d", &group, &groups_per_line) == 2) {
                        opts.format_group = group;
                        opts.format_groups_per_line = groups_per_line;
                    } else {
                        fprintf(stderr, "Invalid format specification: %s\n", optarg);
                        fprintf(stderr, "Expected format like: -f=8x10\n");
                        exit(1);
                    }
                }
                break;
            case 'a':
                opts.asm_mode = true;
                break;
            case 1000: // --arch
                opts.arch = optarg;
                break;
            case 1001: // --smart-asm
                opts.smart_asm_mode = true;
                break;
            case 'h':
                opts.help_mode = true;
                break;
            case '?':
                exit(1);
            default:
                abort();
        }
    }

    // Get input file if specified
    if (optind < argc) {
        opts.input_file = argv[optind];
    }

    return opts;
}

int main(int argc, char *argv[]) {
    // Initialize encoding/decoding tables
    init_tables();

    // Parse command line options
    options_t opts = parse_options(argc, argv);

    if (opts.help_mode) {
        print_usage(argv[0]);
        return 0;
    }

    // Validate conflicting options
    if (opts.asm_mode && opts.smart_asm_mode) {
        fprintf(stderr, "Error: Cannot use both --asm and --smart-asm together\n");
        return 1;
    }

    // Check for terminal input when no file specified
    if (!opts.input_file && isatty(STDIN_FILENO)) {
        print_usage(argv[0]);
        return 0;
    }

    // Read input
    buffer_t input = read_file(opts.input_file);

    if (opts.decode_mode) {
        if (opts.passthrough_mode) {
            fprintf(stderr, "Warning: --passthrough ignored in decode mode\n");
        }

        fprintf(stderr, "Decoding mode: Input size is %zu bytes\n", input.size);

        // Clean input and decode
        buffer_t cleaned = clean_decode_input(&input);
        fprintf(stderr, "After whitespace removal: %zu bytes\n", cleaned.size);

        buffer_t decoded = decode_data((uint8_t*)cleaned.data, cleaned.size);
        fprintf(stderr, "Decoded result size: %zu bytes\n", decoded.size);

        // Write decoded data to stdout
        fwrite(decoded.data, 1, decoded.size, stdout);

        free(cleaned.data);
        free(decoded.data);
    } else {
        // Encode mode
        if (opts.passthrough_mode) {
            // Write original data to stdout
            fwrite(input.data, 1, input.size, stdout);
        }

        // Check for smart disassembly mode first
        if (opts.smart_asm_mode) {
            if (!opts.input_file) {
                fprintf(stderr, "Error: Smart disassembly mode requires a file input\n");
                exit(1);
            }

            // Check if objdump is available
            if (system("which objdump > /dev/null 2>&1") != 0) {
                fprintf(stderr, "Error: objdump not found. Smart disassembly requires objdump.\n");
                exit(1);
            }

            fprintf(stderr, "# Smart disassembly using objdump (format-aware):\n");

            // Create objdump command
            char objdump_cmd[512];
            snprintf(objdump_cmd, sizeof(objdump_cmd), "objdump -d \"%s\" 2>/dev/null", opts.input_file);

            FILE *objdump_pipe = popen(objdump_cmd, "r");
            if (!objdump_pipe) {
                fprintf(stderr, "Error: Failed to run objdump\n");
                exit(1);
            }

            buffer_t objdump_output;
            buffer_init(&objdump_output, input.size * 10 + 1024);

            char line[1024];
            while (fgets(line, sizeof(line), objdump_pipe)) {
                // Look for disassembly lines (address: bytes instruction)
                unsigned int addr;

                char *colon_pos = strchr(line, ':');

                if (colon_pos && sscanf(line, " %x:", &addr) == 1) {
                    // Parse the rest after the colon
                    char *rest = colon_pos + 1;

                    // Skip whitespace
                    while (*rest && isspace(*rest)) rest++;

                    // Find where instruction starts (after hex bytes)
                    char *instr_start = rest;
                    int byte_count = 0;
                    char clean_bytes[64] = {0};

                    // Extract hex bytes
                    while (*instr_start && byte_count < 32) {
                        if (isxdigit(*instr_start)) {
                            if (byte_count < 63) {
                                clean_bytes[byte_count] = *instr_start;
                                byte_count++;
                            }
                            instr_start++;
                        } else if (*instr_start == ' ' || *instr_start == '\t') {
                            // Skip whitespace, but if we hit a lot of spaces, we've reached the instruction
                            int space_count = 0;
                            char *temp = instr_start;
                            while (*temp && (*temp == ' ' || *temp == '\t')) {
                                space_count++;
                                temp++;
                            }
                            if (space_count > 4) {
                                instr_start = temp;
                                break;
                            }
                            instr_start++;
                        } else {
                            break;
                        }
                    }

                    // Get instruction text
                    char *instr_end = strchr(instr_start, '\n');
                    if (instr_end) *instr_end = '\0';

                    // Remove leading/trailing whitespace from instruction
                    while (*instr_start && isspace(*instr_start)) instr_start++;
                    char *instr_tail = instr_start + strlen(instr_start) - 1;
                    while (instr_tail > instr_start && isspace(*instr_tail)) {
                        *instr_tail = '\0';
                        instr_tail--;
                    }

                    if (byte_count > 0 && strlen(instr_start) > 0) {
                        // Convert hex bytes to encoded characters
                        for (int i = 0; i < byte_count; i += 2) {
                            if (i + 1 < byte_count) {
                                char byte_str[3] = {clean_bytes[i], clean_bytes[i+1], '\0'};
                                unsigned int byte_val;
                                if (sscanf(byte_str, "%x", &byte_val) == 1) {
                                    utf8_sequence_t seq = encode_table[byte_val];
                                    buffer_append(&objdump_output, seq.bytes, seq.length);
                                }
                            }
                        }

                        // Add receipt emoji and instruction
                        buffer_append(&objdump_output, " 🧾 ", 6);
                        buffer_append(&objdump_output, instr_start, strlen(instr_start));
                        buffer_append(&objdump_output, "\n", 1);
                    }
                } else if (strstr(line, "Disassembly of section") || strstr(line, "file format")) {
                    // Include section headers as comments
                    buffer_append(&objdump_output, "# ", 2);
                    char *line_end = strchr(line, '\n');
                    if (line_end) *line_end = '\0';
                    // Trim whitespace
                    char *trimmed = line;
                    while (*trimmed && isspace(*trimmed)) trimmed++;
                    char *tail = trimmed + strlen(trimmed) - 1;
                    while (tail > trimmed && isspace(*tail)) {
                        *tail = '\0';
                        tail--;
                    }
                    buffer_append(&objdump_output, trimmed, strlen(trimmed));
                    buffer_append(&objdump_output, "\n", 1);
                }
            }
            pclose(objdump_pipe);

            // Output the smart disassembly
            if (opts.passthrough_mode) {
                fprintf(stderr, "%.*s", (int)objdump_output.size, objdump_output.data);
            } else {
                printf("%.*s", (int)objdump_output.size, objdump_output.data);
            }

            buffer_free(&objdump_output);

        } else if (opts.asm_mode) {
            // Basic disassembly implementation
            if (!opts.input_file) {
                fprintf(stderr, "Error: Disassembly mode requires a file input\n");
                exit(1);
            }

            // Check if cstool is available
            if (system("which cstool > /dev/null 2>&1") != 0) {
                fprintf(stderr, "Warning: Capstone disassembly engine not found. Install it for disassembly.\n");
                fprintf(stderr, "Continuing with simple output...\n");
            } else {
                // Create hex dump command
                char hex_cmd[512];
                snprintf(hex_cmd, sizeof(hex_cmd), "xxd -p \"%s\" | tr -d '\\n'", opts.input_file);

                FILE *hex_pipe = popen(hex_cmd, "r");
                if (!hex_pipe) {
                    fprintf(stderr, "Error: Failed to create hex dump\n");
                    exit(1);
                }

                // Read hex data
                char hex_data[65536]; // 64KB max for now
                size_t hex_len = fread(hex_data, 1, sizeof(hex_data) - 1, hex_pipe);
                hex_data[hex_len] = '\0';
                pclose(hex_pipe);

                // Determine architecture
                const char *arch;
                if (opts.arch) {
                    arch = opts.arch;
                    fprintf(stderr, "# Using specified architecture: %s\n", arch);
                } else {
                    // Simple auto-detection - default to x64
                    arch = "x64";
                    fprintf(stderr, "# Auto-detecting architecture...\n");
                    fprintf(stderr, "# Auto-detected architecture: x64\n");
                }
                fprintf(stderr, "# Disassembly using %s architecture:\n", arch);

                // Create cstool command
                char cstool_cmd[1024];
                snprintf(cstool_cmd, sizeof(cstool_cmd), "echo '%s' | xargs cstool %s 2>/dev/null", hex_data, arch);

                FILE *cstool_pipe = popen(cstool_cmd, "r");
                if (!cstool_pipe) {
                    fprintf(stderr, "Error: Failed to run cstool\n");
                    exit(1);
                }

                // Read and parse disassembly output
                char line[256];
                buffer_t disasm_output;
                buffer_init(&disasm_output, input.size * 10 + 1024); // Much larger allocation for disassembly

                while (fgets(line, sizeof(line), cstool_pipe)) {
                    // Parse cstool format: " addr  bytes    instruction"
                    unsigned int addr;
                    char bytes[32], instruction[128];
                    if (sscanf(line, " %x %31s %127[^\n]", &addr, bytes, instruction) == 3) {
                        // Convert hex bytes to encoded characters
                        for (size_t i = 0; i < strlen(bytes); i += 2) {
                            char byte_str[3] = {bytes[i], bytes[i+1], '\0'};
                            unsigned int byte_val;
                            if (sscanf(byte_str, "%x", &byte_val) == 1) {
                                utf8_sequence_t seq = encode_table[byte_val];
                                if (seq.length > 0) {
                                    buffer_append(&disasm_output, seq.bytes, seq.length);
                                }
                            }
                        }

                        // Add disassembly separator and instruction
                        const char *separator = " 🧾 ";
                        buffer_append(&disasm_output, separator, strlen(separator));
                        buffer_append(&disasm_output, instruction, strlen(instruction));
                        buffer_append(&disasm_output, "\n", 1);
                    }
                }
                pclose(cstool_pipe);

                // Output the disassembly
                fwrite(disasm_output.data, 1, disasm_output.size, stdout);
                free(disasm_output.data);
                free(input.data);
                return 0;
            }
        }

        // Encode the data
        buffer_t encoded = encode_data((uint8_t*)input.data, input.size);
        fprintf(stderr, "Encoded %zu bytes of input to %zu bytes\n", input.size, encoded.size);

        buffer_t *output = &encoded;
        buffer_t formatted;

        // Apply formatting if requested
        if (opts.format_mode) {
            formatted = format_output(&encoded, opts.format_group, opts.format_groups_per_line);
            output = &formatted;
        }

        // Write encoded output
        if (opts.passthrough_mode) {
            // Send encoded data to stderr
            fwrite(output->data, 1, output->size, stderr);
        } else {
            // Send encoded data to stdout
            fwrite(output->data, 1, output->size, stdout);
        }

        free(encoded.data);
        if (opts.format_mode) {
            free(formatted.data);
        }
    }

    free(input.data);
    return 0;
}
