#define _POSIX_C_SOURCE 200809L
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

#include "character_map_embedded.h"

#ifdef __EMSCRIPTEN__
// Standalone WASM builds shouldn't depend on host-provided env functions.
__attribute__((used)) void emscripten_notify_memory_growth(int memory_index) {
    (void)memory_index;
}
#endif

static bool env_var_truthy(const char *value) {
    if (!value) {
        return false;
    }

    while (*value && isspace((unsigned char)*value)) {
        value++;
    }

    size_t len = strlen(value);
    while (len > 0 && isspace((unsigned char)value[len - 1])) {
        len--;
    }

    if (len == 0) {
        return false;
    }

    if (len == 1 && value[0] == '1') {
        return true;
    }

    if (len >= 8) {
        return false;
    }

    char lowered[8];
    for (size_t i = 0; i < len; i++) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[len] = '\0';

    return (strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0);
}

#define MAX_UTF8_BYTES 4
#define INITIAL_BUFFER_SIZE 8192
#define BUFFER_GROW_FACTOR 2
#define STACK_BUFFER_SIZE 4096

// UTF-8 encoding structure
typedef struct {
    uint8_t bytes[MAX_UTF8_BYTES];
    uint8_t length;
} utf8_sequence_t;

// Global encoding and decoding tables
static utf8_sequence_t *encode_table;

typedef struct {
    uint64_t key;
    uint8_t value;
} decode_entry_t;

static decode_entry_t decode_entries[256];
static size_t decode_entry_count = 0;

typedef enum {
    MAPPINGS_NONE = 0,
    MAPPINGS_TABLE,
    MAPPINGS_JSON,
    MAPPINGS_CSV
} mappings_mode_t;

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
    mappings_mode_t mappings_mode;
    char *arch;
    char *input_file;
} options_t;

// Dynamic growing buffer for string building
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    bool uses_stack;   // True if using stack allocation
    char stack_data[STACK_BUFFER_SIZE];  // Embedded stack buffer
} buffer_t;

// Initialize a buffer with stack allocation if small enough
static void buffer_init(buffer_t *buf, size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = INITIAL_BUFFER_SIZE;

    buf->size = 0;

    if (initial_capacity <= STACK_BUFFER_SIZE) {
        // Use embedded stack buffer for small data
        buf->data = buf->stack_data;
        buf->capacity = STACK_BUFFER_SIZE;
        buf->uses_stack = true;
    } else {
        // Use heap allocation for larger buffers
        buf->data = malloc(initial_capacity);
        buf->capacity = initial_capacity;
        buf->uses_stack = false;
        if (!buf->data) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
    }
}

// Grow buffer capacity
static void buffer_grow(buffer_t *buf, size_t min_additional) {
    size_t new_capacity = buf->capacity;
    size_t needed = buf->size + min_additional;

    // Keep growing until we have enough space
    while (new_capacity < needed) {
        new_capacity *= BUFFER_GROW_FACTOR;
    }

    char *new_data;
    if (buf->uses_stack) {
        // Transition from stack to heap
        new_data = malloc(new_capacity);
        if (!new_data) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        // Copy existing data from stack buffer
        memcpy(new_data, buf->stack_data, buf->size);
        buf->uses_stack = false;
    } else {
        // Regular heap reallocation
        new_data = realloc(buf->data, new_capacity);
        if (!new_data) {
            fprintf(stderr, "Memory reallocation failed\n");
            exit(1);
        }
    }

    buf->data = new_data;
    buf->capacity = new_capacity;
}

// Append data to buffer with automatic growth
static void buffer_append(buffer_t *buf, const void *data, size_t len) {
    if (buf->size + len > buf->capacity) {
        buffer_grow(buf, len);
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
}

// Append a single character to buffer
static void buffer_append_char(buffer_t *buf, char c) {
    buffer_append(buf, &c, 1);
}

// Prepare buffer for return - ensure data is heap-allocated
static void buffer_prepare_return(buffer_t *buf) {
    if (buf->uses_stack && buf->size > 0) {
        // Need to transition from stack to heap before returning
        char *heap_data = malloc(buf->size);
        if (!heap_data) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        memcpy(heap_data, buf->stack_data, buf->size);
        buf->data = heap_data;
        buf->capacity = buf->size;
        buf->uses_stack = false;
    }
}

// Free buffer memory
#ifndef __EMSCRIPTEN__
static void buffer_free(buffer_t *buf) {
    if (buf->data && !buf->uses_stack) {
        free(buf->data);
    }
    // Don't set data to NULL for stack buffers since it points to stack_data
    if (!buf->uses_stack) {
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}
#endif

// Helper function to create UTF-8 sequence
static utf8_sequence_t make_utf8(const char *bytes) {
    utf8_sequence_t seq = {0};
    seq.length = strlen(bytes);
    memcpy(seq.bytes, bytes, seq.length);
    return seq;
}

static uint64_t make_key(const uint8_t *bytes, uint8_t len) {
    uint64_t key = len;
    for (uint8_t i = 0; i < len; i++) {
        key = (key << 8) | bytes[i];
    }
    return key;
}

static int compare_decode_entries(const void *a, const void *b) {
    const decode_entry_t *ea = (const decode_entry_t *)a;
    const decode_entry_t *eb = (const decode_entry_t *)b;
    if (ea->key < eb->key) return -1;
    if (ea->key > eb->key) return 1;
    return 0;
}

static bool load_map_from_path(const char *path) {
    if (!path) {
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }

    char buffer[64];
    for (int i = 0; i < 256; i++) {
        if (!fgets(buffer, sizeof(buffer), fp)) {
            fprintf(stderr, "Error: character map '%s' must contain 256 lines\n", path);
            fclose(fp);
            exit(1);
        }

        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }

        if (len == 0) {
            fprintf(stderr, "Error: character map '%s' has an empty entry at index %d\n", path, i);
            fclose(fp);
            exit(1);
        }

        encode_table[i] = make_utf8(buffer);
    }

    fclose(fp);
    return true;
}

static void finalize_decode_entries(void) {
    decode_entry_count = 0;
    for (int i = 0; i < 256; i++) {
        utf8_sequence_t seq = encode_table[i];
        if (seq.length == 0) {
            fprintf(stderr, "Error: missing character mapping for byte %d\n", i);
            exit(1);
        }
        decode_entries[decode_entry_count].key = make_key(seq.bytes, seq.length);
        decode_entries[decode_entry_count].value = (uint8_t)i;
        decode_entry_count++;
    }

    qsort(decode_entries, decode_entry_count, sizeof(decode_entry_t), compare_decode_entries);

    for (size_t i = 1; i < decode_entry_count; i++) {
        if (decode_entries[i].key == decode_entries[i - 1].key) {
            fprintf(stderr, "Error: duplicate character mapping detected\n");
            exit(1);
        }
    }
}

static void load_map_from_embedded(void) {
    for (int i = 0; i < 256; i++) {
        const char *entry = embedded_character_map[i];
        if (!entry || entry[0] == '\0') {
            fprintf(stderr, "Error: embedded character map has an empty entry at index %d\n", i);
            exit(1);
        }
        encode_table[i] = make_utf8(entry);
    }
    finalize_decode_entries();
}

static void load_character_map(const char *argv0) {
    const char *env_path = getenv("PRINTABLE_BINARY_MAP");
    if (env_path && load_map_from_path(env_path)) {
        finalize_decode_entries();
        return;
    }

    if (argv0) {
        char path_buffer[512];
        const char *slash = strrchr(argv0, '/');
#ifdef _WIN32
        const char *backslash = strrchr(argv0, '\\');
        if (!slash || (backslash && backslash > slash)) {
            slash = backslash;
        }
#endif
        if (slash) {
            size_t dir_len = (size_t)(slash - argv0) + 1;
            if (dir_len + strlen("character_map.txt") < sizeof(path_buffer)) {
                memcpy(path_buffer, argv0, dir_len);
                strcpy(path_buffer + dir_len, "character_map.txt");
                if (load_map_from_path(path_buffer)) {
                    finalize_decode_entries();
                    return;
                }
            }
        }
    }

    if (load_map_from_path("character_map.txt")) {
        finalize_decode_entries();
        return;
    }

    load_map_from_embedded();
    return;

    fprintf(stderr, "Error: Unable to load character_map.txt. Set PRINTABLE_BINARY_MAP or place the file alongside the executable.\n");
    exit(1);
}

static const char *ascii_name_for_byte(uint8_t value, char *buffer, size_t buffer_size) {
    static const char *control_names[] = {
        "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
        "BS","TAB","LF","VT","FF","CR","SO","SI",
        "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
        "CAN","EM","SUB","ESC","FS","GS","RS","US"
    };

    if (value <= 0x1F) {
        return control_names[value];
    }
    if (value == 0x20) {
        return "SPACE";
    }
    if (value == 0x7F) {
        return "DEL";
    }
    if (value >= 0x21 && value <= 0x7E) {
        if (buffer_size < 4) {
            return "";
        }
        size_t idx = 0;
        buffer[idx++] = '\'';
        if (value == '\'' || value == '\\') {
            if (idx + 2 >= buffer_size) {
                buffer[0] = '\0';
                return buffer;
            }
            buffer[idx++] = '\\';
        }
        buffer[idx++] = (char)value;
        buffer[idx++] = '\'';
        buffer[idx] = '\0';
        return buffer;
    }
    snprintf(buffer, buffer_size, "0x%02X", value);
    return buffer;
}

static void utf8_sequence_to_string(const utf8_sequence_t *seq, char *buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return;
    }
    if (!seq || seq->length == 0) {
        buffer[0] = '\0';
        return;
    }
    size_t copy_len = seq->length;
    if (copy_len >= buffer_size) {
        copy_len = buffer_size - 1;
    }
    memcpy(buffer, seq->bytes, copy_len);
    buffer[copy_len] = '\0';
}

static void json_escape_and_print(FILE *out, const char *str) {
    for (const unsigned char *p = (const unsigned char *)str; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc(*p, out);
        } else if (*p >= 0x20) {
            fputc(*p, out);
        } else {
            fprintf(out, "\\u%04X", *p);
        }
    }
}

static void csv_escape_and_print(FILE *out, const char *str) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)str; *p; ++p) {
        if (*p == '"') {
            fputc('"', out);
            fputc('"', out);
        } else {
            fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void print_mappings_table(void) {
    printf("%-6s %-5s %-12s %s\n", "Byte", "Dec", "ASCII", "Mapping");
    for (int i = 0; i < 256; i++) {
        char hex_buf[6];
        snprintf(hex_buf, sizeof(hex_buf), "0x%02X", i);
        char ascii_buf[16];
        const char *ascii_name = ascii_name_for_byte((uint8_t)i, ascii_buf, sizeof(ascii_buf));
        char mapping_buf[32];
        utf8_sequence_to_string(&encode_table[i], mapping_buf, sizeof(mapping_buf));
        printf("%-6s %-5d %-12s %s\n", hex_buf, i, ascii_name, mapping_buf[0] ? mapping_buf : "");
    }
}

static void print_mappings_json(void) {
    fputs("[\n", stdout);
    for (int i = 0; i < 256; i++) {
        char hex_buf[6];
        snprintf(hex_buf, sizeof(hex_buf), "0x%02X", i);
        char ascii_buf[16];
        const char *ascii_name = ascii_name_for_byte((uint8_t)i, ascii_buf, sizeof(ascii_buf));
        char mapping_buf[32];
        utf8_sequence_to_string(&encode_table[i], mapping_buf, sizeof(mapping_buf));
        printf("  {\"byte\":%d,\"hex\":\"%s\",\"dec\":%d,\"ascii\":\"", i, hex_buf, i);
        json_escape_and_print(stdout, ascii_name);
        fputs("\",\"mapping\":\"", stdout);
        json_escape_and_print(stdout, mapping_buf);
        fprintf(stdout, "\"}%s\n", (i == 255) ? "" : ",");
    }
    fputs("]\n", stdout);
}

static void print_mappings_csv(void) {
    fputs("byte,hex,dec,ascii,mapping\n", stdout);
    for (int i = 0; i < 256; i++) {
        char hex_buf[6];
        snprintf(hex_buf, sizeof(hex_buf), "0x%02X", i);
        char ascii_buf[16];
        const char *ascii_name = ascii_name_for_byte((uint8_t)i, ascii_buf, sizeof(ascii_buf));
        char mapping_buf[32];
        utf8_sequence_to_string(&encode_table[i], mapping_buf, sizeof(mapping_buf));
        printf("%d,%s,%d,", i, hex_buf, i);
        csv_escape_and_print(stdout, ascii_name);
        fputc(',', stdout);
        csv_escape_and_print(stdout, mapping_buf);
        fputc('\n', stdout);
    }
}

static void print_mappings(mappings_mode_t mode) {
    switch (mode) {
        case MAPPINGS_TABLE:
            print_mappings_table();
            break;
        case MAPPINGS_JSON:
            print_mappings_json();
            break;
        case MAPPINGS_CSV:
            print_mappings_csv();
            break;
        default:
            break;
    }
}

// Helper function to calculate hash for decode table
// Initialize encoding and decoding tables
static void init_tables(const char *argv0) {
    encode_table = calloc(256, sizeof(utf8_sequence_t));
    if (!encode_table) {
        fprintf(stderr, "Memory allocation failed for lookup tables\n");
        exit(1);
    }

    load_character_map(argv0);
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
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

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
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    size_t i = 0;
    while (i < input_len) {
        uint8_t first_byte = input[i];
        uint8_t seq_len = utf8_sequence_length(first_byte);

        // Ensure we don't go beyond input
        if (i + seq_len > input_len) {
            seq_len = input_len - i;
        }

        bool matched = false;

        for (int len = seq_len; len >= 1; len--) {
            if (len > MAX_UTF8_BYTES) continue;
            if (i + (size_t)len <= input_len) {
                uint64_t key = make_key(input + i, (uint8_t)len);
                size_t left = 0, right = decode_entry_count;
                while (left < right) {
                    size_t mid = left + (right - left) / 2;
                    if (decode_entries[mid].key == key) {
                        uint8_t decoded_byte = decode_entries[mid].value;
                        buffer_append_char(&output, decoded_byte);
                        i += len;
                        matched = true;
                        break;
                    } else if (decode_entries[mid].key < key) {
                        left = mid + 1;
                    } else {
                        right = mid;
                    }
                }
                if (matched) {
                    break;
                }
            }
        }

        if (!matched) {
            i++;
        }
    }

    buffer_prepare_return(&output);
    return output;
}

// Apply formatting to encoded output
static buffer_t format_output(const buffer_t *input, int group_size, int groups_per_line) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    size_t char_count = 0;
    size_t i = 0;

    while (i < input->size) {
        // Determine UTF-8 character length
        uint8_t first_byte = (uint8_t)input->data[i];
        uint8_t char_len = utf8_sequence_length(first_byte);

        // Append the UTF-8 character
        for (uint8_t j = 0; j < char_len && i + j < input->size; j++) {
            buffer_append_char(&output, input->data[i + j]);
        }

        char_count++;
        i += char_len;

        // Add spacing after each group
        if (char_count % group_size == 0 && i < input->size) {
            if ((char_count / group_size) % groups_per_line == 0) {
                buffer_append_char(&output, '\n');
            } else {
                buffer_append_char(&output, ' ');
            }
        }
    }

    buffer_prepare_return(&output);
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

    buffer_prepare_return(&buf);
    return buf;
}

// Clean input for decoding (remove whitespace and disassembly formatting)
static buffer_t clean_decode_input(const buffer_t *input) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    // Simple whitespace removal for now
    for (size_t i = 0; i < input->size; i++) {
        char c = input->data[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            buffer_append_char(&output, c);
        }
    }

    buffer_prepare_return(&output);
    return output;
}

static const char *resolve_program_name(const char *argv0) {
#ifdef PRINTABLE_BINARY_HELP_NAME
    (void)argv0;
    return PRINTABLE_BINARY_HELP_NAME;
#else
    if (argv0 && argv0[0] != '\0') {
        return argv0;
    }
    return "printable_binary";
#endif
}

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
    fprintf(stderr, "  --mappings       Show the byte-to-character mapping table\n");
    fprintf(stderr, "  --mappings-json  Output mappings as JSON\n");
    fprintf(stderr, "  --mappings-csv   Output mappings as CSV\n");
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
        .mappings_mode = MAPPINGS_NONE,
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
        {"mappings", no_argument, 0, 1002},
        {"mappings-json", no_argument, 0, 1003},
        {"mappings-csv", no_argument, 0, 1004},
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
            case 1002: // --mappings
                if (opts.mappings_mode != MAPPINGS_NONE && opts.mappings_mode != MAPPINGS_TABLE) {
                    fprintf(stderr, "Error: Only one mappings output option can be specified\n");
                    exit(1);
                }
                opts.mappings_mode = MAPPINGS_TABLE;
                break;
            case 1003: // --mappings-json
                if (opts.mappings_mode != MAPPINGS_NONE && opts.mappings_mode != MAPPINGS_JSON) {
                    fprintf(stderr, "Error: Only one mappings output option can be specified\n");
                    exit(1);
                }
                opts.mappings_mode = MAPPINGS_JSON;
                break;
            case 1004: // --mappings-csv
                if (opts.mappings_mode != MAPPINGS_NONE && opts.mappings_mode != MAPPINGS_CSV) {
                    fprintf(stderr, "Error: Only one mappings output option can be specified\n");
                    exit(1);
                }
                opts.mappings_mode = MAPPINGS_CSV;
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
    // Parse command line options first (needed for help/usage)
    options_t opts = parse_options(argc, argv);
    const char *program_display_name = resolve_program_name(argv[0]);
    bool stats_enabled = !env_var_truthy(getenv("PRINTABLE_BINARY_MUTE_STATS"));

    // Initialize encoding/decoding tables (after options so argv[0] is available)
    init_tables(argv[0]);

    if (opts.help_mode) {
        print_usage(program_display_name);
        return 0;
    }

    if (opts.mappings_mode != MAPPINGS_NONE) {
        print_mappings(opts.mappings_mode);
        return 0;
    }

    // Validate conflicting options
    if (opts.asm_mode && opts.smart_asm_mode) {
        fprintf(stderr, "Error: Cannot use both --asm and --smart-asm together\n");
        return 1;
    }

    // Check for terminal input when no file specified
    if (!opts.input_file && isatty(STDIN_FILENO)) {
        print_usage(program_display_name);
        return 0;
    }

    // Read input
    buffer_t input = read_file(opts.input_file);

    if (opts.decode_mode) {
        if (opts.passthrough_mode) {
            fprintf(stderr, "Warning: --passthrough ignored in decode mode\n");
        }

        if (stats_enabled) {
            fprintf(stderr, "Decoding mode: Input size is %zu bytes\n", input.size);
        }

        // Clean input and decode
        buffer_t cleaned = clean_decode_input(&input);
        if (stats_enabled) {
            fprintf(stderr, "After whitespace removal: %zu bytes\n", cleaned.size);
        }

        buffer_t decoded = decode_data((uint8_t*)cleaned.data, cleaned.size);
        if (stats_enabled) {
            fprintf(stderr, "Decoded result size: %zu bytes\n", decoded.size);
        }

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

#ifdef __EMSCRIPTEN__
        if (opts.smart_asm_mode || opts.asm_mode) {
            fprintf(stderr, "Error: Disassembly modes are not supported in the WebAssembly build\n");
            free(input.data);
            return 1;
        }
#else
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
            buffer_init(&objdump_output, 0);  // Use default, will start with stack

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
            return 0;

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
                buffer_init(&disasm_output, 0); // Will grow as needed, start with stack

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
#endif

        // Encode the data
        buffer_t encoded = encode_data((uint8_t*)input.data, input.size);
        if (stats_enabled) {
            fprintf(stderr, "Encoded %zu bytes of input to %zu bytes\n", input.size, encoded.size);
        }

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
