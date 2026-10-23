#ifndef RESPONSE_FORMAT_H
#define RESPONSE_FORMAT_H

#include <stddef.h>
#include "common.h"

typedef struct {
    char *buf;
    size_t cap;
    size_t pos;
    int first;
} ListBuilder;

void fmt_timestamp(char *out, size_t out_size);
void fmt_account_details(const Account *acc, char *out, size_t out_size);
void fmt_staff(const Staff *st, char *out, size_t out_size);
void fmt_loan(const Loan *ln, char *out, size_t out_size);
void fmt_loan_history(const Loan *ln, char *out, size_t out_size);
void list_builder_init(ListBuilder *lb, size_t cap);
void list_builder_add(ListBuilder *lb, const char *entry);
void list_builder_respond(int fd, unsigned char opcode, ListBuilder *lb);

#endif
