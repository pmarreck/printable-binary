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
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

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
    bool help_mode;
    bool spaces_mode;
    int format_group;
    int format_groups_per_line;
    mappings_mode_t mappings_mode;
    char *input_file;
} options_t;

static void parse_format_spec(options_t *opts, const char *format_str) {
    if (!format_str || format_str[0] == '\0') {
        fprintf(stderr, "Error: --format requires a value like 8x10\n");
        exit(1);
    }

    while (*format_str == '=' || isspace((unsigned char)*format_str)) {
        format_str++;
    }

    if (*format_str == '\0') {
        fprintf(stderr, "Error: --format requires a value like 8x10\n");
        exit(1);
    }

    int group = 0;
    int groups_per_line = 0;
    if (sscanf(format_str, "%dx%d", &group, &groups_per_line) != 2 ||
        group <= 0 || groups_per_line <= 0) {
        fprintf(stderr, "Invalid format specification: %s\n", format_str);
        fprintf(stderr, "Expected format like: -f=8x10\n");
        exit(1);
    }

    opts->format_mode = true;
    opts->format_group = group;
    opts->format_groups_per_line = groups_per_line;
}

static void set_mappings_mode(options_t *opts, mappings_mode_t new_mode) {
    if (opts->mappings_mode != MAPPINGS_NONE && opts->mappings_mode != new_mode) {
        fprintf(stderr, "Error: Only one mappings output option can be specified\n");
        exit(1);
    }
    opts->mappings_mode = new_mode;
}

static bool long_option_equals(const char *name, size_t len, const char *option) {
    size_t option_len = strlen(option);
    return len == option_len && strncmp(name, option, option_len) == 0;
}

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
static buffer_t encode_data(const uint8_t *input, size_t input_len, bool spaces_mode) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    for (size_t i = 0; i < input_len; i++) {
        if (spaces_mode && input[i] == ' ') {
            buffer_append_char(&output, ' ');
            continue;
        }
        utf8_sequence_t seq = encode_table[input[i]];
        if (seq.length > 0) {
            buffer_append(&output, seq.bytes, seq.length);
        }
    }

    return output;
}

// Decode printable UTF-8 back to binary
static buffer_t decode_data(const uint8_t *input, size_t input_len, bool spaces_mode) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    size_t i = 0;
    while (i < input_len) {
        if (spaces_mode && input[i] == ' ') {
            buffer_append_char(&output, ' ');
            i++;
            continue;
        }
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
static buffer_t format_output(const buffer_t *input, int group_size, int groups_per_line, bool spaces_mode) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    size_t char_count = 0;
    size_t i = 0;
    const char group_separator = spaces_mode ? '\t' : ' ';

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
                buffer_append_char(&output, group_separator);
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

    // Fast path for stdin: use unbuffered read() to avoid libc quirks (notably in Cosmopolitan APE)
    if (!filename || strcmp(filename, "-") == 0) {
        char temp[8192];
        while (1) {
            ssize_t bytes_read = read(STDIN_FILENO, temp, sizeof(temp));
            if (bytes_read > 0) {
                if (buf.size + (size_t)bytes_read >= buf.capacity) {
                    buf.capacity = (buf.size + (size_t)bytes_read) * 2;
                    buf.data = realloc(buf.data, buf.capacity);
                    if (!buf.data) {
                        fprintf(stderr, "Memory allocation failed\n");
                        exit(1);
                    }
                }
                memcpy(buf.data + buf.size, temp, (size_t)bytes_read);
                buf.size += (size_t)bytes_read;
                continue;
            }
            if (bytes_read == 0) {
                break; // EOF
            }
            if (errno == EINTR
#ifdef EAGAIN
                || errno == EAGAIN || errno == EWOULDBLOCK
#endif
            ) {
                continue;
            }
            perror("Error reading input");
            exit(1);
        }
        buffer_prepare_return(&buf);
        return buf;
    }

    // File path case: use stdio for portability
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char temp[8192];
    while (1) {
        size_t bytes_read = fread(temp, 1, sizeof(temp), file);
        if (bytes_read > 0) {
            if (buf.size + bytes_read >= buf.capacity) {
                buf.capacity = (buf.size + bytes_read) * 2;
                buf.data = realloc(buf.data, buf.capacity);
                if (!buf.data) {
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(1);
                }
            }
            memcpy(buf.data + buf.size, temp, bytes_read);
            buf.size += bytes_read;
            continue;
        }

        if (feof(file)) {
            break;
        }

        if (ferror(file)) {
            if (errno == EINTR
#ifdef EAGAIN
                || errno == EAGAIN || errno == EWOULDBLOCK
#endif
            ) {
                clearerr(file);
                continue;
            }
            perror("Error reading file");
            fclose(file);
            exit(1);
        }
    }

    fclose(file);
    buffer_prepare_return(&buf);
    return buf;
}

// Clean input for decoding (remove whitespace)
static buffer_t clean_decode_input(const buffer_t *input, bool spaces_mode) {
    buffer_t output;
    // Start with reasonable initial size, will grow as needed
    buffer_init(&output, INITIAL_BUFFER_SIZE);

    bool warned = false;
    char prev1 = '\0';
    char prev2 = '\0';

    // Simple whitespace removal for now
    for (size_t i = 0; i < input->size; i++) {
        char c = input->data[i];

        if (spaces_mode && !warned && c == ' ' && prev1 == ' ' && (prev2 == '\n' || prev2 == '\r')) {
            fprintf(stderr, "Warning: spaces after newline are treated as data in --spaces mode\n");
            warned = true;
        }

        if (c == '\n' || c == '\r') {
            // skip
        } else if (c == '\t') {
            // skip
        } else if (c == ' ' && !spaces_mode) {
            // skip
        } else {
            buffer_append_char(&output, c);
        }

        prev2 = prev1;
        prev1 = c;
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
    fprintf(stderr, "  -s, --spaces     Preserve literal spaces (decoder still ignores tabs/newlines/CR)\n");
    fprintf(stderr, "  -f[=NxM], --format[=NxM]   Format output in groups\n");
    fprintf(stderr, "                    Default: 8x10 (groups of 8 chars, 10 groups per line)\n");
    fprintf(stderr, "  --mappings       Show the byte-to-character mapping table\n");
    fprintf(stderr, "  --mappings-json  Output mappings as JSON\n");
    fprintf(stderr, "  --mappings-csv   Output mappings as CSV\n");
    fprintf(stderr, "  -h, --help       Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If no file is specified, input is read from stdin.\n");
    fprintf(stderr, "Output is written to stdout, unless --passthrough is used.\n\n");
    fprintf(stderr, "PrintableBinary encodes every byte (quotes, backslashes, tabs, CR/LF, etc.) into visible, but clearly related, glyphs.\n");
    fprintf(stderr, "You can format the encoded text freely—spaces, newlines, indentation—because the decoder ignores real whitespace.\n");
    fprintf(stderr, "With --spaces, literal spaces are treated as data (and we warn on indented lines).\n");
    fprintf(stderr, "Examples: SPACE→␣, TAB→⇥, CR→⏎, LF→↧, single quote→ʼ, double quote→˵, backslash→⧷.\n");
    fprintf(stderr, "This avoids shell-escaping surprises while keeping context obvious.\n\n");
    fprintf(stderr, "When --passthrough is used:\n");
    fprintf(stderr, "  - Original binary data is passed unchanged to stdout\n");
    fprintf(stderr, "  - Encoded representation is sent to stderr\n");
    fprintf(stderr, "  - This allows using the tool in pipelines to monitor binary data\n\n");
    fprintf(stderr, "Environment variables:\n");
    fprintf(stderr, "  PRINTABLE_BINARY_MAP        Override character map lookup path\n");
    fprintf(stderr, "  PRINTABLE_BINARY_MUTE_STATS Set to 1/true/yes to suppress stderr stats\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s binary_file               # Encode binary to UTF-8\n", program_name);
    fprintf(stderr, "  %s -d encoded_file           # Decode UTF-8 to binary\n", program_name);
    fprintf(stderr, "  %s -f=4x10 binary_file       # Encode with formatting\n", program_name);
    fprintf(stderr, "  %s --passthrough file | tool # Monitor binary stream\n", program_name);
}

// Parse command line options
static options_t parse_options(int argc, char *argv[]) {
    options_t opts = {
        .decode_mode = false,
        .passthrough_mode = false,
        .format_mode = false,
        .help_mode = false,
        .spaces_mode = false,
        .format_group = 8,
        .format_groups_per_line = 10,
        .mappings_mode = MAPPINGS_NONE,
        .input_file = NULL
    };

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            if (i + 1 < argc) {
                if (opts.input_file) {
                    fprintf(stderr, "Error: Multiple input files specified\n");
                    exit(1);
                }
                opts.input_file = argv[++i];
            }
            break;
        }

        if (arg[0] != '-' || strcmp(arg, "-") == 0) {
            if (opts.input_file) {
                fprintf(stderr, "Error: Multiple input files specified (%s)\n", arg);
                exit(1);
            }
            opts.input_file = arg;
            continue;
        }

        if (arg[1] == '-') {
            const char *name = arg + 2;
            const char *eq = strchr(name, '=');
            size_t name_len = eq ? (size_t)(eq - name) : strlen(name);
            const char *value = eq ? eq + 1 : NULL;

            if (long_option_equals(name, name_len, "decode")) {
                opts.decode_mode = true;
            } else if (long_option_equals(name, name_len, "passthrough")) {
                opts.passthrough_mode = true;
            } else if (long_option_equals(name, name_len, "spaces")) {
                opts.spaces_mode = true;
            } else if (long_option_equals(name, name_len, "format")) {
                if (value && value[0] != '\0') {
                    parse_format_spec(&opts, value);
                } else {
                    opts.format_mode = true;
                }
            } else if (long_option_equals(name, name_len, "mappings")) {
                set_mappings_mode(&opts, MAPPINGS_TABLE);
            } else if (long_option_equals(name, name_len, "mappings-json")) {
                set_mappings_mode(&opts, MAPPINGS_JSON);
            } else if (long_option_equals(name, name_len, "mappings-csv")) {
                set_mappings_mode(&opts, MAPPINGS_CSV);
            } else if (long_option_equals(name, name_len, "help")) {
                opts.help_mode = true;
            } else {
                fprintf(stderr, "Unknown option: --%.*s\n", (int)name_len, name);
                exit(1);
            }
            continue;
        }

        size_t pos = 1;
        while (arg[pos] != '\0') {
            char opt = arg[pos];
            switch (opt) {
                case 'd':
                    opts.decode_mode = true;
                    pos++;
                    break;
                case 'p':
                    opts.passthrough_mode = true;
                    pos++;
                    break;
                case 's':
                    opts.spaces_mode = true;
                    pos++;
                    break;
                case 'h':
                    opts.help_mode = true;
                    pos++;
                    break;
                case 'f': {
                    if (arg[pos + 1] != '\0') {
                        const char *value = &arg[pos + 1];
                        pos = strlen(arg);
                        parse_format_spec(&opts, value);
                    } else {
                        opts.format_mode = true;
                        pos++;
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Unknown option: -%c\n", opt);
                    exit(1);
            }
        }
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
        buffer_t cleaned = clean_decode_input(&input, opts.spaces_mode);
        if (stats_enabled) {
            if (opts.spaces_mode) {
                fprintf(stderr, "After whitespace removal (tabs/newlines/CR): %zu bytes\n", cleaned.size);
            } else {
                fprintf(stderr, "After whitespace removal: %zu bytes\n", cleaned.size);
            }
        }

        buffer_t decoded = decode_data((uint8_t*)cleaned.data, cleaned.size, opts.spaces_mode);
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

        // Encode the data
        buffer_t encoded = encode_data((uint8_t*)input.data, input.size, opts.spaces_mode);
        if (stats_enabled) {
            fprintf(stderr, "Encoded %zu bytes of input to %zu bytes\n", input.size, encoded.size);
        }

        buffer_t *output = &encoded;
        buffer_t formatted;

        // Apply formatting if requested
        if (opts.format_mode) {
            formatted = format_output(&encoded, opts.format_group, opts.format_groups_per_line, opts.spaces_mode);
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
