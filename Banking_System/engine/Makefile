CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread
LDFLAGS = -pthread -lm

SRC_COMMON = src/common/util.c src/common/rwlock.c src/common/lock_table.c src/common/storage.c src/common/journal.c src/common/protocol.c src/common/auth_guard.c src/common/session_guard.c src/common/role_access.c src/common/response_format.c
OBJ_COMMON = $(SRC_COMMON:.c=.o)
SRC_ROLES = src/roles/auth/auth.c src/roles/customer/customer.c src/roles/staff/employee.c src/roles/staff/manager.c src/roles/staff/shared.c src/roles/admin/admin.c
OBJ_ROLES = $(SRC_ROLES:.c=.o)

.PHONY: all clean test

all: bankserver bankclient

test: all
	python3 ../tests/test_engine.py

bankserver: $(OBJ_COMMON) $(OBJ_ROLES) src/server.o src/main.o
	$(CC) $(LDFLAGS) -o $@ $^

bankclient: src/common/protocol.o src/roles/customer/client.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/common/*.o src/roles/auth/*.o src/roles/customer/*.o src/roles/staff/*.o src/roles/admin/*.o bankserver bankclient
