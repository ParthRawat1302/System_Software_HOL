#ifndef SERVER_H
#define SERVER_H

/* Runs the accept() loop forever (thread-per-client via pthread_create).
 * Does not return under normal operation. */
int server_run(void);

#endif
