#ifndef ROLE_ACCESS_H
#define ROLE_ACCESS_H

#include "session.h"

/* Central authorization vocabulary used by every role module/handler. */
int role_is_admin(const ClientSession *session);
int role_is_manager(const ClientSession *session);
int role_is_employee(const ClientSession *session);
int role_is_staff_any(const ClientSession *session);
int role_actor_id(const ClientSession *session);

#endif
