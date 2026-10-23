/* Minimal interactive CLI client for manually exercising the engine
 * directly over its socket protocol -- useful for demoing/debugging the C
 * server independent of the Python gateway / React frontend. */
#include "common.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef enum {
    CLI_LOGGED_OUT,
    CLI_CUSTOMER,
    CLI_EMPLOYEE,
    CLI_MANAGER,
    CLI_ADMIN
} CliRole;

static int connect_to_server(const char *host) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return -1;
    }
    return fd;
}

static void discard_input_line(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
}

static const char *do_request(int fd, unsigned char opcode, const char *payload) {
    static char resp[MAX_PAYLOAD];
    if (send_frame(fd, opcode, payload) != 0) {
        perror("send_frame");
        return NULL;
    }
    unsigned char resp_op;
    if (recv_frame(fd, &resp_op, resp, sizeof(resp)) != 0) {
        printf("<connection closed>\n");
        return NULL;
    }
    if (resp_op != opcode) {
        fprintf(stderr, "Warning: response opcode %u does not match request opcode %u\n",
                (unsigned int)resp_op, (unsigned int)opcode);
    }
    printf("%s\n", resp);
    return resp;
}

static int choice_allowed(CliRole role, int choice) {
    switch (role) {
        case CLI_LOGGED_OUT:
            return choice == 1;
        case CLI_CUSTOMER:
            return choice == 2 || choice == 3 || choice == 4 || choice == 5 ||
                   choice == 6 || choice == 7 || choice == 14 || choice == 15 ||
                   choice == 16 || choice == 20;
        case CLI_EMPLOYEE:
            return choice == 5 || choice == 8 || choice == 9 || choice == 10 ||
                   choice == 11 || choice == 12 || choice == 13 || choice == 17 ||
                   choice == 18;
        case CLI_MANAGER:
            return choice == 5 || choice == 11 || choice == 12 || choice == 19 ||
                   choice == 21 || choice == 22 || choice == 25 || choice == 26;
        case CLI_ADMIN:
            return choice == 5 || choice == 8 || choice == 9 || choice == 10 ||
                   choice == 11 || choice == 12 || choice == 13 || choice == 17 ||
                   choice == 18 || choice == 19 || choice == 21 || choice == 22 ||
                   choice == 23 || choice == 24 || choice == 25 || choice == 26;
        default:
            return 0;
    }
}

static void print_menu(CliRole role) {
    if (role == CLI_LOGGED_OUT) {
        printf("\n-- Bank Terminal --\n"
               "  1) Login\n"
               "  0) Exit\n> ");
        return;
    }

    printf("\n");
    switch (role) {
        case CLI_CUSTOMER:
            printf("-- Customer --\n"
                   "  2) Deposit\n  3) Withdraw\n  4) Balance Enquiry\n"
                   "  5) Password Change\n  6) View Account Details\n"
                   "  7) Transaction History\n  14) Transfer Funds\n"
                   "  15) Apply for a Loan\n  16) View My Loan Applications\n"
                   "  20) Add Feedback\n");
            break;
        case CLI_EMPLOYEE:
            printf("-- Employee --\n"
                   "  5) Password Change\n  8) Add Customer Account\n"
                   "  9) Close Customer Account\n  10) Modify Customer Account\n"
                   "  11) Search Customer Account\n  12) List Customer Accounts\n"
                   "  13) View Customer Passbook\n"
                   "  17) View Assigned Loan Applications\n  18) Approve/Reject Loan\n");
            break;
        case CLI_MANAGER:
            printf("-- Manager --\n"
                   "  5) Password Change\n  11) Search Customer Account\n"
                   "  12) List Customer Accounts\n  19) Assign Loan to Employee\n"
                   "  26) View Pending Loan Requests\n"
                   "  21) Review Customer Feedback\n"
                   "  22) Activate/Deactivate Customer Account\n"
                   "  25) List Staff\n");
            break;
        case CLI_ADMIN:
            printf("-- Administrator --\n"
                   "  5) Password Change\n  8) Add Customer Account\n"
                   "  9) Close Customer Account\n  10) Modify Customer Account\n"
                   "  11) Search Customer Account\n  12) List Customer Accounts\n"
                   "  13) View Customer Passbook\n  17) View Assigned Loan Applications\n"
                   "  18) Approve/Reject Loan\n  19) Assign Loan to Employee\n"
                   "  26) View Pending Loan Requests\n"
                   "  21) Review Customer Feedback\n"
                   "  22) Activate/Deactivate Customer Account\n"
                   "  23) Add Employee/Manager\n  24) Modify Staff\n  25) List Staff\n");
            break;
        default:
            break;
    }
    printf("  0) Logout and Exit\n> ");
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int fd = connect_to_server(host);
    if (fd < 0) return 1;

    printf("Connected to bank server at %s:%d\n", host, SERVER_PORT);

    char line[256];
    CliRole role = CLI_LOGGED_OUT;
    while (1) {
        print_menu(role);
        if (!fgets(line, sizeof(line), stdin)) break;
        int choice = atoi(line);
        char payload[MAX_PAYLOAD];

        if (choice == 0) {
            if (role != CLI_LOGGED_OUT) do_request(fd, OP_LOGOUT, "");
            close(fd);
            return 0;
        }
        if (!choice_allowed(role, choice)) {
            printf("That option is not available for this login.\n");
            continue;
        }

        switch (choice) {
            case 1: {
                char kind[16], id[16], pw[64];
                printf("kind (USER/EMPLOYEE/MANAGER/ADMIN): "); scanf("%15s", kind);
                printf("id: "); scanf("%15s", id);
                printf("password: "); scanf("%63s", pw);
                discard_input_line();
                for (size_t i = 0; kind[i] != '\0'; i++) {
                    kind[i] = (char)toupper((unsigned char)kind[i]);
                }
                snprintf(payload, sizeof(payload), "%s|%s|%s", kind, id, pw);
                const char *response = do_request(fd, OP_LOGIN, payload);
                if (!response || strncmp(response, "OK|", 3) != 0) break;
                if (strcmp(kind, "USER") == 0) role = CLI_CUSTOMER;
                else if (strcmp(kind, "ADMIN") == 0) role = CLI_ADMIN;
                else if (strcmp(kind, "MANAGER") == 0) role = CLI_MANAGER;
                else if (strcmp(kind, "EMPLOYEE") == 0 || strcmp(kind, "STAFF") == 0) role = CLI_EMPLOYEE;
                else printf("Unrecognized login role; please exit and try again.\n");
                break;
            }
            case 2: {
                double amt; printf("amount: "); scanf("%lf", &amt);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%.2f", amt);
                do_request(fd, OP_DEPOSIT, payload);
                break;
            }
            case 3: {
                double amt; printf("amount: "); scanf("%lf", &amt);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%.2f", amt);
                do_request(fd, OP_WITHDRAW, payload);
                break;
            }
            case 4:
                do_request(fd, OP_BALANCE, "");
                break;
            case 5: {
                char oldpw[64], newpw[64];
                printf("old password: "); scanf("%63s", oldpw);
                printf("new password: "); scanf("%63s", newpw);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s", oldpw, newpw);
                do_request(fd, OP_CHANGE_PW, payload);
                break;
            }
            case 6:
                do_request(fd, OP_VIEW_DETAILS, "");
                break;
            case 7:
                do_request(fd, OP_TXN_HISTORY, "");
                break;
            case 8: {
                char role[16], n1[64], p1[64], n2[64], p2[64]; double bal;
                printf("role (NORMAL/JOINT): "); scanf("%15s", role);
                printf("owner1 name: "); scanf("%63s", n1);
                printf("owner1 password: "); scanf("%63s", p1);
                printf("owner2 name (- if none): "); scanf("%63s", n2);
                printf("owner2 password (- if none): "); scanf("%63s", p2);
                printf("initial balance: "); scanf("%lf", &bal);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s|%s|%s|%s|%.2f", role, n1, p1, n2, p2, bal);
                do_request(fd, OP_ADMIN_ADD, payload);
                break;
            }
            case 9: {
                char id[16]; printf("account id: "); scanf("%15s", id); discard_input_line();
                do_request(fd, OP_ADMIN_DELETE, id);
                break;
            }
            case 10: {
                char id[16], field[32], value[64];
                printf("account id: "); scanf("%15s", id);
                printf("field (balance/owner1_name/owner2_name): "); scanf("%31s", field);
                printf("value: "); scanf("%63s", value);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s|%s", id, field, value);
                do_request(fd, OP_ADMIN_MODIFY, payload);
                break;
            }
            case 11: {
                char id[16]; printf("account id: "); scanf("%15s", id); discard_input_line();
                do_request(fd, OP_ADMIN_SEARCH, id);
                break;
            }
            case 12:
                do_request(fd, OP_ADMIN_LIST, "");
                break;
            case 13: {
                char id[16]; printf("account id: "); scanf("%15s", id); discard_input_line();
                do_request(fd, OP_TXN_HISTORY, id);
                break;
            }
            case 14: {
                char to[16]; double amt;
                printf("to account id: "); scanf("%15s", to);
                printf("amount: "); scanf("%lf", &amt);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%.2f", to, amt);
                do_request(fd, OP_TRANSFER, payload);
                break;
            }
            case 15: {
                double amt; char purpose[128];
                printf("amount: "); scanf("%lf", &amt);
                printf("purpose: "); scanf("%127s", purpose);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%.2f|%s", amt, purpose);
                do_request(fd, OP_LOAN_APPLY, payload);
                break;
            }
            case 16:
                do_request(fd, OP_LOAN_MY_LIST, "");
                break;
            case 17:
                do_request(fd, OP_LOAN_ASSIGNED_LIST, "");
                break;
            case 18: {
                char loan_id[16], decision[16];
                printf("loan id: "); scanf("%15s", loan_id);
                printf("decision (APPROVE/REJECT): "); scanf("%15s", decision);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s", loan_id, decision);
                do_request(fd, OP_LOAN_DECIDE, payload);
                break;
            }
            case 19: {
                char loan_id[16], employee_id[16];
                printf("loan id: "); scanf("%15s", loan_id);
                printf("employee id: "); scanf("%15s", employee_id);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s", loan_id, employee_id);
                do_request(fd, OP_LOAN_ASSIGN, payload);
                break;
            }
            case 20: {
                char text[200];
                printf("feedback: "); scanf(" %199[^\n]", text);
                discard_input_line();
                do_request(fd, OP_FEEDBACK_ADD, text);
                break;
            }
            case 21:
                do_request(fd, OP_FEEDBACK_LIST, "");
                break;
            case 22: {
                char id[16], status[16];
                printf("account id: "); scanf("%15s", id);
                printf("status (ACTIVE/INACTIVE): "); scanf("%15s", status);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s", id, status);
                do_request(fd, OP_ACCOUNT_SET_STATUS, payload);
                break;
            }
            case 23: {
                char role[16], name[64], pw[64];
                printf("role (EMPLOYEE/MANAGER): "); scanf("%15s", role);
                printf("name: "); scanf("%63s", name);
                printf("password: "); scanf("%63s", pw);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s|%s", role, name, pw);
                do_request(fd, OP_STAFF_ADD, payload);
                break;
            }
            case 24: {
                char id[16], field[16], value[64];
                printf("staff id: "); scanf("%15s", id);
                printf("field (role/name/status): "); scanf("%15s", field);
                printf("value: "); scanf("%63s", value);
                discard_input_line();
                snprintf(payload, sizeof(payload), "%s|%s|%s", id, field, value);
                do_request(fd, OP_STAFF_MODIFY, payload);
                break;
            }
            case 25:
                do_request(fd, OP_STAFF_LIST, "");
                break;
            case 26:
                do_request(fd, OP_LOAN_PENDING_LIST, "");
                break;
            case 0:
                do_request(fd, OP_LOGOUT, "");
                close(fd);
                return 0;
            default:
                printf("unknown choice\n");
        }
    }

    close(fd);
    return 0;
}
