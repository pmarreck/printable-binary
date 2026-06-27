/*
 * container_json.h — pure, transport-resistant flat-JSON helpers for the
 * printable-binary-file.json container (issue #1). Shared by the C FFI CLI and
 * the standalone C CLI. No encode/decode/crc32 here (each caller supplies those
 * via its own path); just the string handling. The `data` value is read to its
 * closing quote regardless of whitespace a transport injected inside it, then
 * canonicalized — the same natural transport-resistance as raw printable-binary.
 */
#ifndef CONTAINER_JSON_H
#define CONTAINER_JSON_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Strip transport whitespace (space/tab/CR/LF). malloc'd result (caller frees);
 * *out_len set. Clean payloads (no literal whitespace) are unchanged. */
static char *cj_canonical(const char *data, size_t len, size_t *out_len) {
    char *out = (char *)malloc(len ? len : 1);
    if (!out) { *out_len = 0; return NULL; }
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') out[n++] = c;
    }
    *out_len = n;
    return out;
}

/* Raw string value of "key" in a flat JSON object: pointer into `json` (not
 * NUL-terminated), *out_len set; NULL if absent. Whitespace-tolerant. */
static const char *cj_get_string(const char *json, size_t json_len, const char *key, size_t *out_len) {
    char needle[80];
    int kl = snprintf(needle, sizeof needle, "\"%s\"", key);
    if (kl <= 0 || (size_t)kl >= sizeof needle) return NULL;
    const char *p = NULL;
    const char *end = json + json_len;
    for (size_t i = 0; (size_t)kl <= json_len && i <= json_len - (size_t)kl; i++) {
        if (memcmp(json + i, needle, (size_t)kl) == 0) { p = json + i + (size_t)kl; break; }
    }
    if (!p) return NULL;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')) p++;
    if (p >= end || *p != '"') return NULL;
    p++;
    const char *start = p;
    while (p < end) {
        if (*p == '\\') { p += 2; continue; }
        if (*p == '"') break;
        p++;
    }
    if (p >= end) return NULL;
    *out_len = (size_t)(p - start);
    return start;
}

/* Write `s` JSON-escaped to `fp`. */
static void cj_fputs_escaped(FILE *fp, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  fputs("\\\"", fp); break;
            case '\\': fputs("\\\\", fp); break;
            case '\n': fputs("\\n", fp);  break;
            case '\r': fputs("\\r", fp);  break;
            case '\t': fputs("\\t", fp);  break;
            default:   fputc(c, fp);      break;
        }
    }
}

/* Basename after the last '/' or '\\' (pointer into `path`). */
static const char *cj_basename(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') b = p + 1;
    }
    return b;
}

#endif /* CONTAINER_JSON_H */
