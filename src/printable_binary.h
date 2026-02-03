/*
 * PrintableBinary - FFI Header
 * C API for encoding/decoding binary data as printable UTF-8
 */

#ifndef PRINTABLE_BINARY_H
#define PRINTABLE_BINARY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Whitespace handling flags for validation.
 * Can be combined with bitwise OR.
 */
typedef enum {
    PB_WS_REJECT_ALL  = 0,       /**< Reject all whitespace characters */
    PB_WS_ALLOW_SPACE = 1 << 0,  /**< Allow space (0x20) */
    PB_WS_ALLOW_TAB   = 1 << 1,  /**< Allow tab (0x09) */
    PB_WS_ALLOW_LF    = 1 << 2,  /**< Allow line feed (0x0A) */
    PB_WS_ALLOW_CR    = 1 << 3,  /**< Allow carriage return (0x0D) */
    PB_WS_ALLOW_ALL   = 0x0F     /**< Allow all whitespace */
} pb_whitespace_flags_t;

/**
 * Result of validating a printable-binary encoded string.
 */
typedef struct {
    int is_valid;           /**< 0 = invalid, 1 = valid */
    int64_t error_position; /**< -1 if valid, else byte offset of first invalid char */
    uint32_t error_codepoint; /**< The invalid codepoint, or 0 if valid */
} pb_validation_result_t;

/**
 * Initialize the printable-binary library.
 * Must be called before using pb_validate().
 *
 * @param argv0 Path to the executable (for finding character map), or NULL
 */
void pb_init(const char *argv0);

/**
 * Validate that a string contains only valid printable-binary encoded characters.
 *
 * This function checks if the input consists entirely of:
 * - Valid UTF-8 sequences that are in the printable-binary character map
 * - Optionally, whitespace characters as specified by ws_flags
 *
 * @param input     Pointer to the UTF-8 encoded string to validate
 * @param input_len Length of the input in bytes
 * @param ws_flags  Bitfield of pb_whitespace_flags_t values
 * @return          Validation result with error details if invalid
 *
 * Example:
 * @code
 *   pb_init(NULL);
 *   const char *encoded = "Hello␣World";
 *   pb_validation_result_t result = pb_validate(encoded, strlen(encoded), PB_WS_REJECT_ALL);
 *   if (result.is_valid) {
 *       printf("Valid printable-binary string\n");
 *   } else {
 *       printf("Invalid at position %lld, codepoint U+%04X\n",
 *              (long long)result.error_position, result.error_codepoint);
 *   }
 * @endcode
 */
pb_validation_result_t pb_validate(const char *input, size_t input_len, unsigned int ws_flags);

#ifdef __cplusplus
}
#endif

#endif /* PRINTABLE_BINARY_H */
