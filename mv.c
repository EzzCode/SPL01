#include <fcntl.h>
int mv_main(int argc, char *argv[]) {
    // Write your code here
    // Do not write a main() function. Instead, deal with mv_main() as the main function of your program.
    if (argc != 3) {
        printf("Usage: %s <source> <destination>\n", argv[0]);
        return 1;
    }
    if (rename(argv[1], argv[2]) == 0) {
        return 0;
    }
    return -1;

}