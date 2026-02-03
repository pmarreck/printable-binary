/*
 * PrintableBinary - Thin C wrapper for Zig FFI
 *
 * This is a minimal CLI that calls the Zig library via C FFI.
 * It handles argument parsing and I/O, delegating all encoding/decoding
 * to the Zig implementation.
 *
 * Build: Link against the Zig static library (libprintable_binary.a)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "printable_binary.h"

/* Program options */
typedef struct {
    bool decode_mode;
    bool passthrough_mode;
    bool spaces_mode;
    bool tabs_mode;
    bool crlf_mode;
    bool strip_whitespace;
    bool format_mode;
    bool help_mode;
    int format_group;
    int format_groups_per_line;
    int mappings_mode; /* 0=none, 1=table, 2=json, 3=csv */
    char *preserve_chars;
    char *input_file;
} options_t;

static bool env_var_truthy(const char *value) {
    if (!value || !*value) return false;
    while (*value && isspace((unsigned char)*value)) value++;
    if (!*value) return false;
    if (*value == '1') return true;
    char lower[8] = {0};
    for (int i = 0; i < 7 && value[i]; i++) {
        lower[i] = (char)tolower((unsigned char)value[i]);
    }
    return strcmp(lower, "true") == 0 || strcmp(lower, "yes") == 0;
}

static void print_usage(const char *name) {
    fprintf(stderr, "PrintableBinary (Zig core) - Encode binary data as printable UTF-8\n\n");
    fprintf(stderr, "Usage: %s [options] [file]\n", name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --decode       Decode mode (default is encode mode)\n");
    fprintf(stderr, "  -p, --passthrough  Pass input to stdout unchanged, send encoded data to stderr\n");
    fprintf(stderr, "\nEncode options (preserve literal characters instead of encoding):\n");
    fprintf(stderr, "  -s, --spaces       Preserve literal spaces\n");
    fprintf(stderr, "  -t, --tabs         Preserve literal tabs\n");
    fprintf(stderr, "  -n, --crlf         Preserve literal CR/LF\n");
    fprintf(stderr, "  -w, --preserve-whitespace  Shorthand for -stn\n");
    fprintf(stderr, "  -P, --preserve=CHARS       Preserve specific characters\n");
    fprintf(stderr, "\nDecode options:\n");
    fprintf(stderr, "  -S, --strip-whitespace     Strip whitespace before decoding\n");
    fprintf(stderr, "\nFormat and output options:\n");
    fprintf(stderr, "  -f[=NxM], --format[=NxM]   Format output in groups (default: 8x10)\n");
    fprintf(stderr, "  --mappings         Show the byte-to-character mapping table\n");
    fprintf(stderr, "  --mappings-json    Output mappings as JSON\n");
    fprintf(stderr, "  --mappings-csv     Output mappings as CSV\n");
    fprintf(stderr, "  -h, --help         Show this help\n");
}

static void parse_format_spec(options_t *opts, const char *spec) {
    if (!spec || !*spec) {
        fprintf(stderr, "Error: --format requires a value like 8x10\n");
        exit(1);
    }
    while (*spec == '=' || isspace((unsigned char)*spec)) spec++;
    int group = 0, per_line = 0;
    if (sscanf(spec, "%dx%d", &group, &per_line) != 2 || group <= 0 || per_line <= 0) {
        fprintf(stderr, "Invalid format specification: %s\n", spec);
        exit(1);
    }
    opts->format_mode = true;
    opts->format_group = group;
    opts->format_groups_per_line = per_line;
}

static options_t parse_options(int argc, char *argv[]) {
    options_t opts = {
        .decode_mode = false,
        .passthrough_mode = false,
        .spaces_mode = false,
        .tabs_mode = false,
        .crlf_mode = false,
        .strip_whitespace = false,
        .format_mode = false,
        .help_mode = false,
        .format_group = 8,
        .format_groups_per_line = 10,
        .mappings_mode = 0,
        .preserve_chars = NULL,
        .input_file = NULL
    };

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            if (i + 1 < argc) opts.input_file = argv[++i];
            break;
        }

        if (arg[0] != '-' || strcmp(arg, "-") == 0) {
            if (opts.input_file) {
                fprintf(stderr, "Error: Multiple input files\n");
                exit(1);
            }
            opts.input_file = arg;
            continue;
        }

        if (arg[1] == '-') {
            /* Long options */
            const char *name = arg + 2;
            if (strncmp(name, "format=", 7) == 0) {
                parse_format_spec(&opts, name + 7);
            } else if (strncmp(name, "preserve=", 9) == 0) {
                opts.preserve_chars = strdup(name + 9);
            } else if (strcmp(name, "decode") == 0) {
                opts.decode_mode = true;
            } else if (strcmp(name, "passthrough") == 0) {
                opts.passthrough_mode = true;
            } else if (strcmp(name, "spaces") == 0) {
                opts.spaces_mode = true;
            } else if (strcmp(name, "tabs") == 0) {
                opts.tabs_mode = true;
            } else if (strcmp(name, "crlf") == 0) {
                opts.crlf_mode = true;
            } else if (strcmp(name, "preserve-whitespace") == 0) {
                opts.spaces_mode = opts.tabs_mode = opts.crlf_mode = true;
            } else if (strcmp(name, "strip-whitespace") == 0) {
                opts.strip_whitespace = true;
            } else if (strcmp(name, "format") == 0) {
                opts.format_mode = true;
            } else if (strcmp(name, "mappings") == 0) {
                opts.mappings_mode = 1;
            } else if (strcmp(name, "mappings-json") == 0) {
                opts.mappings_mode = 2;
            } else if (strcmp(name, "mappings-csv") == 0) {
                opts.mappings_mode = 3;
            } else if (strcmp(name, "help") == 0) {
                opts.help_mode = true;
            } else {
                fprintf(stderr, "Unknown option: --%s\n", name);
                exit(1);
            }
        } else {
            /* Short options */
            for (size_t j = 1; arg[j]; j++) {
                switch (arg[j]) {
                    case 'd': opts.decode_mode = true; break;
                    case 'p': opts.passthrough_mode = true; break;
                    case 's': opts.spaces_mode = true; break;
                    case 't': opts.tabs_mode = true; break;
                    case 'n': opts.crlf_mode = true; break;
                    case 'w': opts.spaces_mode = opts.tabs_mode = opts.crlf_mode = true; break;
                    case 'S': opts.strip_whitespace = true; break;
                    case 'h': opts.help_mode = true; break;
                    case 'f':
                        if (arg[j + 1]) {
                            parse_format_spec(&opts, arg + j + 1);
                            j = strlen(arg) - 1;
                        } else {
                            opts.format_mode = true;
                        }
                        break;
                    case 'P':
                        if (arg[j + 1]) {
                            opts.preserve_chars = strdup(arg + j + 1);
                            j = strlen(arg) - 1;
                        } else if (i + 1 < argc) {
                            opts.preserve_chars = strdup(argv[++i]);
                        } else {
                            fprintf(stderr, "Error: -P requires a value\n");
                            exit(1);
                        }
                        break;
                    default:
                        fprintf(stderr, "Unknown option: -%c\n", arg[j]);
                        exit(1);
                }
            }
        }
    }
    return opts;
}

/* ASCII names for mappings output */
static const char *ascii_names[] = {
    "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
    "BS","TAB","LF","VT","FF","CR","SO","SI",
    "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
    "CAN","EM","SUB","ESC","FS","GS","RS","US",
    "SPACE"
};

static void print_mappings(int mode) {
    switch (mode) {
        case 1: /* table */
            printf("Byte   Dec   ASCII        Mapping\n");
            for (int i = 0; i < 256; i++) {
                const char *ascii = (i < 33) ? ascii_names[i] :
                                    (i == 127) ? "DEL" :
                                    (i < 128) ? "" : "";
                char ascii_buf[4] = {0};
                if (i >= 33 && i < 127) {
                    ascii_buf[0] = '\'';
                    ascii_buf[1] = (char)i;
                    ascii_buf[2] = '\'';
                }
                printf("0x%02X   %-5d %-12s %.*s\n", i, i,
                       (i >= 33 && i < 127) ? ascii_buf : ascii,
                       (int)pb_get_mapping_len((uint8_t)i), pb_get_mapping((uint8_t)i));
            }
            break;
        case 2: /* json */
            printf("[\n");
            for (int i = 0; i < 256; i++) {
                const char *ascii = (i < 33) ? ascii_names[i] :
                                    (i == 127) ? "DEL" : "";
                char ascii_esc[16] = {0};
                if (i >= 33 && i < 127) {
                    if (i == '"' || i == '\\') {
                        snprintf(ascii_esc, sizeof(ascii_esc), "\\%c", (char)i);
                    } else {
                        ascii_esc[0] = (char)i;
                    }
                } else {
                    strncpy(ascii_esc, ascii, sizeof(ascii_esc) - 1);
                }
                printf("  {\"byte\": %d, \"ascii\": \"%s\", \"mapping\": \"%.*s\"}%s\n",
                       i, ascii_esc,
                       (int)pb_get_mapping_len((uint8_t)i), pb_get_mapping((uint8_t)i),
                       (i < 255) ? "," : "");
            }
            printf("]\n");
            break;
        case 3: /* csv */
            printf("byte,hex,dec,ascii,mapping\n");
            for (int i = 0; i < 256; i++) {
                const char *ascii = (i < 33) ? ascii_names[i] :
                                    (i == 127) ? "DEL" : "";
                char ascii_buf[4] = {0};
                if (i >= 33 && i < 127) {
                    ascii_buf[0] = (char)i;
                }
                printf("%d,0x%02X,%d,\"%s\",\"%.*s\"\n", i, i, i,
                       (i >= 33 && i < 127) ? ascii_buf : ascii,
                       (int)pb_get_mapping_len((uint8_t)i), pb_get_mapping((uint8_t)i));
            }
            break;
    }
}

/* Read entire input into buffer */
static char *read_input(const char *filename, size_t *len_out) {
    FILE *fp = stdin;
    if (filename && strcmp(filename, "-") != 0) {
        fp = fopen(filename, "rb");
        if (!fp) {
            perror("Error opening file");
            exit(1);
        }
    }

    size_t capacity = 8192;
    size_t len = 0;
    char *buf = malloc(capacity);
    if (!buf) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    while (1) {
        size_t n = fread(buf + len, 1, capacity - len, fp);
        if (n == 0) break;
        len += n;
        if (len >= capacity) {
            capacity *= 2;
            char *new_buf = realloc(buf, capacity);
            if (!new_buf) {
                free(buf);
                fprintf(stderr, "Memory allocation failed\n");
                exit(1);
            }
            buf = new_buf;
        }
    }

    if (fp != stdin) fclose(fp);
    *len_out = len;
    return buf;
}

int main(int argc, char *argv[]) {
    options_t opts = parse_options(argc, argv);
    const char *prog_name = argv[0] ? argv[0] : "printable_binary";
    bool stats_enabled = !env_var_truthy(getenv("PRINTABLE_BINARY_MUTE_STATS"));

    if (opts.help_mode) {
        print_usage(prog_name);
        return 0;
    }

    if (opts.mappings_mode) {
        print_mappings(opts.mappings_mode);
        return 0;
    }

    /* Check for TTY without input file */
    if (!opts.input_file && isatty(STDIN_FILENO)) {
        print_usage(prog_name);
        return 0;
    }

    /* Read input */
    size_t input_len;
    char *input = read_input(opts.input_file, &input_len);

    if (opts.decode_mode) {
        if (opts.passthrough_mode) {
            fprintf(stderr, "Warning: --passthrough ignored in decode mode\n");
        }

        if (stats_enabled) {
            fprintf(stderr, "Decoding mode: Input size is %zu bytes\n", input_len);
        }

        /* Warn about spaces after newlines in spaces + strip-whitespace mode */
        if (opts.spaces_mode && opts.strip_whitespace) {
            char prev1 = 0, prev2 = 0;
            for (size_t i = 0; i < input_len; i++) {
                char c = input[i];
                if (c == ' ' && prev1 == ' ' && (prev2 == '\n' || prev2 == '\r')) {
                    fprintf(stderr, "Warning: spaces after newline are treated as data in --spaces mode\n");
                    break;
                }
                prev2 = prev1;
                prev1 = c;
            }
        }

        /* Build decode flags */
        unsigned int flags = PB_DECODE_NONE;
        if (opts.spaces_mode) flags |= PB_DECODE_SPACES_MODE;
        if (opts.strip_whitespace) flags |= PB_DECODE_STRIP_WS;

        pb_ffi_result_t result = pb_decode(input, input_len, flags);
        if (result.error_code != 0 || !result.data) {
            fprintf(stderr, "Decode error\n");
            free(input);
            return 1;
        }

        if (stats_enabled) {
            fprintf(stderr, "Decoded result size: %zu bytes\n", result.len);
        }

        fwrite(result.data, 1, result.len, stdout);
        pb_free(result.data, result.len);
    } else {
        /* Encode mode */
        if (opts.passthrough_mode) {
            fwrite(input, 1, input_len, stdout);
        }

        /* Build encode flags */
        unsigned int flags = PB_ENCODE_NONE;
        if (opts.spaces_mode) flags |= PB_ENCODE_PRESERVE_SPACES;
        if (opts.tabs_mode) flags |= PB_ENCODE_PRESERVE_TABS;
        if (opts.crlf_mode) flags |= PB_ENCODE_PRESERVE_CRLF;

        size_t preserve_len = opts.preserve_chars ? strlen(opts.preserve_chars) : 0;
        pb_ffi_result_t result = pb_encode(input, input_len, flags,
                                           opts.preserve_chars, preserve_len);
        if (result.error_code != 0 || !result.data) {
            fprintf(stderr, "Encode error\n");
            free(input);
            return 1;
        }

        char *output = result.data;
        size_t output_len = result.len;
        pb_ffi_result_t formatted = {0};

        if (opts.format_mode) {
            formatted = pb_format(result.data, result.len,
                                  (size_t)opts.format_group,
                                  (size_t)opts.format_groups_per_line,
                                  opts.spaces_mode ? 1 : 0);
            if (formatted.error_code == 0 && formatted.data) {
                output = formatted.data;
                output_len = formatted.len;
            }
        }

        if (stats_enabled) {
            fprintf(stderr, "Encoded %zu bytes of input to %zu bytes\n",
                    input_len, output_len);
        }

        if (opts.passthrough_mode) {
            fwrite(output, 1, output_len, stderr);
        } else {
            fwrite(output, 1, output_len, stdout);
        }

        pb_free(result.data, result.len);
        if (formatted.data) {
            pb_free(formatted.data, formatted.len);
        }
    }

    free(input);
    if (opts.preserve_chars) free(opts.preserve_chars);
    return 0;
}
