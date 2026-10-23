#include "role_access.h"

int role_is_admin(const ClientSession *session) {
    return session->role == SESSION_ADMIN;
}

int role_is_manager(const ClientSession *session) {
    return role_is_admin(session) ||
           (session->role == SESSION_STAFF && session->staff_role == STAFF_MANAGER);
}

int role_is_employee(const ClientSession *session) {
    return role_is_admin(session) ||
           (session->role == SESSION_STAFF && session->staff_role == STAFF_EMPLOYEE);
}

int role_is_staff_any(const ClientSession *session) {
    return role_is_admin(session) || session->role == SESSION_STAFF;
}

int role_actor_id(const ClientSession *session) {
    if (session->role == SESSION_ADMIN) return session->admin_id;
    if (session->role == SESSION_STAFF) return session->staff_id;
    return 0;
}
