#include <unistd.h>

int main() {
    char buffer[100];
    int n;

    n = read(0, buffer, sizeof(buffer));

    if (n > 0) {
        write(1, buffer, n);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q06.c -o stdin_stdout
// ./stdin_stdout
// (type some text and press Enter - it is echoed back using only read/write)
//
// Test with input redirection too:
// echo "hello system calls" | ./stdin_stdout
