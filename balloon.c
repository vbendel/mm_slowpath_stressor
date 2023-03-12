#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <oom_score_adj>\n", argv[0]);
        exit(1);
    }

    int oom_adj = atoi(argv[1]);

    char proc_file_path[64];
    sprintf(proc_file_path, "/proc/%d/oom_score_adj", getpid());

    int fd = open(proc_file_path, O_WRONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    char oom_str[16];
    sprintf(oom_str, "%d\n", oom_adj);

    ssize_t num_written = write(fd, oom_str, strlen(oom_str));
    if (num_written < 0) {
        perror("write");
        exit(1);
    }

    close(fd);

    printf("oom_score_adj set to %d\n", oom_adj);
    while (1)
        sleep(10);

    return 0;
}
