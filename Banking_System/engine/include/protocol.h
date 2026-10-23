#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

typedef enum {
    OP_LOGIN = 1,
    OP_DEPOSIT = 2,
    OP_WITHDRAW = 3,
    OP_BALANCE = 4,
    OP_CHANGE_PW = 5,
    OP_VIEW_DETAILS = 6,
    OP_LOGOUT = 7,
    OP_ADMIN_ADD = 8,
    OP_ADMIN_DELETE = 9,
    OP_ADMIN_MODIFY = 10,
    OP_ADMIN_SEARCH = 11,
    OP_ADMIN_LIST = 12,
    OP_TXN_HISTORY = 13,

    OP_TRANSFER = 14,             /* customer: atomic funds transfer to another account */

    OP_LOAN_APPLY = 15,           /* customer */
    OP_LOAN_MY_LIST = 16,         /* customer: own loan applications */
    OP_LOAN_ASSIGNED_LIST = 17,   /* employee/admin: loans assigned to this employee */
    OP_LOAN_DECIDE = 18,          /* employee/admin: approve/reject an assigned loan */
    OP_LOAN_ASSIGN = 19,          /* manager/admin: assign a pending loan to an employee */

    OP_FEEDBACK_ADD = 20,         /* customer */
    OP_FEEDBACK_LIST = 21,        /* manager/admin: review feedback */

    OP_ACCOUNT_SET_STATUS = 22,   /* manager/admin: activate/deactivate a customer account */

    OP_STAFF_ADD = 23,            /* admin: add a new Employee/Manager */
    OP_STAFF_MODIFY = 24,         /* admin: change a staff member's role/name/status */
    OP_STAFF_LIST = 25,           /* admin/manager: staff directory */
    OP_LOAN_PENDING_LIST = 26,    /* manager/admin: unassigned pending loans */
    OP_LOAN_DECISION_HISTORY = 27 /* manager/admin: all loan decisions/statuses */
} Opcode;

/* Wire format: [4-byte big-endian length][1-byte opcode][payload bytes]
 * length counts the opcode byte plus the payload. Payload is a plain
 * '|'-delimited text string (see docs/protocol.md) -- no JSON/protobuf
 * library, just our own framing and our own field splitting (util.c). */

/* Returns 0 on success, -1 on socket error. */
int send_frame(int fd, unsigned char opcode, const char *payload);

/* Reads one frame into payload_buf (NUL-terminated). Returns 0 on success,
 * -1 on error/disconnect. */
int recv_frame(int fd, unsigned char *opcode, char *payload_buf, size_t buf_size);

#endif
