#include <fcntl.h>

int cp_main(int argc, char *argv[]) {
    // Write your code here
    // Do not write a main() function. Instead, deal with cp_main() as the main function of your program.
    if (argc != 3) {
        printf("Usage: %s <source> <destination>\n", argv[0]);
        return 1;
    }
    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd == -1) {
        printf("Error opening source file");
        return 1;
    }
    int dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        printf("Error opening source file");
        close(src_fd);
        return 1;
    }
    char buffer[sizeof(int)];
    int bytes_read;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer)))) {
        if (bytes_read == -1) {
            printf("cp: failed to read from source file");
            close(src_fd);
            close(dst_fd);
            exit(-1);
        }
        int bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written == -1 || bytes_written != bytes_read) {
            printf("cp: failed to write to destination file");
            close(src_fd);
            close(dst_fd);
            exit(-1);
        }
    }
    close(src_fd);
    close(dst_fd);
    return 0;
}