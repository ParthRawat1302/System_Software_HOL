#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Hand-rolled FNV-1a hash used instead of a crypto library -- this project
 * is about OS mechanisms, not cryptography, so a simple, self-written hash
 * is used to avoid pulling in an external hashing/crypto dependency. */
unsigned long simple_hash(const char *str);

/* Splits `payload` in place on '|' separators. Fills `fields` with pointers
 * into `payload` and returns the number of fields found (<= max_fields).
 * NOTE: if the input contains more '|' than expected, any extras end up
 * folded into the LAST field rather than being rejected -- callers that
 * expect an exact field count must check the last field with
 * has_reserved_char() to catch that case (see docs/protocol.md). */
int split_fields(char *payload, char *fields[], int max_fields);

/* Counts occurrences of `c` in `s`. Used to verify a payload has exactly
 * the number of '|' separators a fixed-field opcode expects *before*
 * split_fields() runs -- split_fields on its own silently folds any extra
 * separators into the last field instead of rejecting them, which lets a
 * '|' embedded in an earlier field (e.g. a name) silently shift every
 * field after it. Combined with the rule that no field value may itself
 * contain '|', an exact count match means the split is unambiguous. */
int count_char(const char *s, char c);

/* Returns 1 if `s` contains '|' or ';' (the request/response field and
 * record delimiters), or is empty. Used to reject values that would
 * otherwise silently corrupt the wire protocol's field boundaries or the
 * ADMIN_LIST response format. */
int has_reserved_char(const char *s);

/* Strictly parses `s` as a finite decimal amount. Rejects empty/garbage
 * input, trailing junk, and non-finite values (NaN/Infinity) that atof()
 * would otherwise accept silently. If `allow_negative` is 0, negative
 * values are also rejected. On success writes the value to *out and
 * returns 1; returns 0 and leaves *out untouched on any invalid input. */
int parse_amount(const char *s, double *out, int allow_negative);

#endif
