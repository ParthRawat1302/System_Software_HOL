#include "util.h"

#include <math.h>
#include <stdlib.h>

unsigned long simple_hash(const char *str) {
    unsigned long hash = 1469598103934665603UL; /* FNV offset basis */
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 1099511628211UL; /* FNV prime */
    }
    return hash;
}

int split_fields(char *payload, char *fields[], int max_fields) {
    int count = 0;
    if (!payload || max_fields <= 0) return 0;

    char *p = payload;
    fields[count++] = p;
    while (*p && count < max_fields) {
        if (*p == '|') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

int count_char(const char *s, char c) {
    int n = 0;
    if (!s) return 0;
    for (const char *p = s; *p; p++) {
        if (*p == c) n++;
    }
    return n;
}

int has_reserved_char(const char *s) {
    if (!s || *s == '\0') return 1;
    for (const char *p = s; *p; p++) {
        if (*p == '|' || *p == ';') return 1;
    }
    return 0;
}

int parse_amount(const char *s, double *out, int allow_negative) {
    if (!s || *s == '\0') return 0;
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;   /* garbage or trailing junk */
    if (!isfinite(v)) return 0;                /* rejects "nan" / "inf" */
    if (!allow_negative && v < 0) return 0;
    *out = v;
    return 1;
}
