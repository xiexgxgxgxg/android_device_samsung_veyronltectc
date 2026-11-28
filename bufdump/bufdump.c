/* debug helper: dump a dma-buf (ION) given a /proc/<pid>/fd/<n> path */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <fdpath> <size> <out>\n", argv[0]);
        return 1;
    }
    size_t size = strtoull(argv[2], NULL, 0);
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    int out = open(argv[3], O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out < 0) { perror("open out"); return 1; }
    ssize_t n = write(out, p, size);
    fprintf(stderr, "wrote %zd bytes\n", n);
    return 0;
}
