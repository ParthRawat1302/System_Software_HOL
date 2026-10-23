#ifndef ROLE_HANDLERS_H
#define ROLE_HANDLERS_H

#include "session.h"

/* Implementations remain private to the engine; role dispatch modules call
 * these declarations so transport code does not contain role routing. */
void handle_login(int fd, char *payload, ClientSession *session, int unused);
void handle_change_pw(int fd, char *payload, ClientSession *session, int unused);
void handle_deposit(int fd, char *payload, ClientSession *session, int unused);
void handle_withdraw(int fd, char *payload, ClientSession *session, int unused);
void handle_transfer(int fd, char *payload, ClientSession *session, int unused);
void handle_balance(int fd, ClientSession *session);
void handle_view_details(int fd, ClientSession *session);
void handle_txn_history(int fd, char *payload, ClientSession *session);
void handle_loan_apply(int fd, char *payload, ClientSession *session, int unused);
void handle_loan_my_list(int fd, ClientSession *session);
void handle_feedback_add(int fd, char *payload, ClientSession *session, int unused);
void handle_loan_assigned_list(int fd, ClientSession *session);
void handle_loan_decide(int fd, char *payload, ClientSession *session, int unused);
void handle_loan_assign(int fd, char *payload, ClientSession *session, int unused);
void handle_loan_pending_list(int fd, ClientSession *session);
void handle_loan_decision_history(int fd, ClientSession *session);
void handle_feedback_list(int fd, ClientSession *session);
void handle_account_set_status(int fd, char *payload, ClientSession *session, int unused);
void handle_admin_search(int fd, char *payload, ClientSession *session);
void handle_admin_list(int fd, ClientSession *session);
void handle_staff_list(int fd, ClientSession *session);
void handle_admin_add(int fd, char *payload, ClientSession *session, int unused);
void handle_admin_delete(int fd, char *payload, ClientSession *session, int unused);
void handle_admin_modify(int fd, char *payload, ClientSession *session, int unused);
void handle_staff_add(int fd, char *payload, ClientSession *session, int unused);
void handle_staff_modify(int fd, char *payload, ClientSession *session, int unused);

#endif
