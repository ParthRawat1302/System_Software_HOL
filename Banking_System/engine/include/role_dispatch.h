#ifndef ROLE_DISPATCH_H
#define ROLE_DISPATCH_H

#include "session.h"

/* Returns 1 when the opcode belongs to this module, 0 otherwise. */
int auth_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);
int customer_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);
int employee_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);
int manager_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);
int staff_shared_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);
int admin_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session);

#endif
