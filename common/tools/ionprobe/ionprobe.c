#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

typedef int ion_user_handle_t;
struct ion_allocation_data { size_t len; size_t align; unsigned int heap_id_mask; unsigned int flags; ion_user_handle_t handle; };
struct ion_fd_data { ion_user_handle_t handle; int fd; };
struct ion_handle_data { ion_user_handle_t handle; };
#define ION_IOC_MAGIC 'I'
#define ION_IOC_ALLOC _IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)
#define ION_IOC_FREE  _IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)
#define ION_IOC_MAP   _IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)
#define ION_IOC_SHARE _IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)
#define ION_IOC_IMPORT _IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)

int main(int argc, char **argv) {
    unsigned heap = (1u << 25); /* ION_SYSTEM_HEAP_ID aka ION_IOMMU_HEAP_ID */
    if (argc > 1) heap = (unsigned)strtoul(argv[1], NULL, 0);
    printf("sizeof(alloc)=%zu sizeof(fd_data)=%zu heap=0x%x\n",
           sizeof(struct ion_allocation_data), sizeof(struct ion_fd_data), heap);
    int fd = open("/dev/ion", O_RDONLY);
    printf("open=%d errno=%d(%s)\n", fd, errno, strerror(errno));
    if (fd < 0) return 1;
    struct ion_allocation_data a;
    memset(&a, 0, sizeof(a));
    a.len = 4096; a.align = 4096; a.heap_id_mask = heap; a.flags = 0;
    int rc = ioctl(fd, ION_IOC_ALLOC, &a);
    printf("alloc rc=%d errno=%d(%s) handle=%d\n", rc, errno, strerror(errno), a.handle);
    if (rc) return 2;
    struct ion_fd_data f; memset(&f, 0, sizeof(f));
    f.handle = a.handle; f.fd = -12345;
    rc = ioctl(fd, ION_IOC_MAP, &f);
    printf("map rc=%d errno=%d(%s) fd=%d\n", rc, errno, strerror(errno), f.fd);
    if (rc == 0 && f.fd >= 0) {
        void *p = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, f.fd, 0);
        printf("mmap=%p errno=%d(%s)\n", p, errno, strerror(errno));
        if (p != MAP_FAILED) { memset(p, 0x5a, 4096); munmap(p, 4096); }
        struct ion_fd_data im; memset(&im, 0, sizeof(im));
        im.fd = f.fd; im.handle = -99;
        rc = ioctl(fd, ION_IOC_IMPORT, &im);
        printf("import rc=%d errno=%d(%s) handle=%d\n", rc, errno, strerror(errno), im.handle);
    }
    return 0;
}
