/*
 * Tiny QSEECom probe used to check whether the fingerprint trustlet
 * ("dualfp") can be loaded on the W2017.
 *
 * libQSEEComAPI.so only exists as a vendor blob, so it is resolved at
 * runtime with dlopen()/dlsym() instead of being linked at build time.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

struct qseecom_handle {
	unsigned char *ion_sbuffer;
};

typedef int (*start_app_fn)(struct qseecom_handle **handle,
			    const char *path, const char *fname,
			    uint32_t sb_size);
typedef int (*shutdown_app_fn)(struct qseecom_handle **handle);
typedef int (*set_bw_fn)(struct qseecom_handle *handle, _Bool high);

static const char *kLibs[] = {
	"/vendor/lib64/libQSEEComAPI.so",
	"/vendor/lib/libQSEEComAPI.so",
	"libQSEEComAPI.so",
};

static void dump_node(const char *path)
{
	struct stat st;
	if (stat(path, &st) == 0)
		printf("  %s: rdev=%u:%u mode=%04o\n", path,
		       (unsigned)(st.st_rdev >> 8), (unsigned)(st.st_rdev & 0xff),
		       (unsigned)(st.st_mode & 07777));
	else
		printf("  %s: missing (%s)\n", path, strerror(errno));
}

int main(int argc, char **argv)
{
	const char *path = "/firmware/image";
	const char *fname = "dualfp";
	uint32_t sb_size = 16384;
	void *lib = NULL;
	start_app_fn start_app;
	shutdown_app_fn shutdown_app;
	set_bw_fn set_bw;
	struct qseecom_handle *h = NULL;
	int r;
	int i;

	if (argc > 1)
		fname = argv[1];
	if (argc > 2)
		sb_size = (uint32_t)strtoul(argv[2], NULL, 0);

	printf("== QSEECom %s test ==\n", fname);
	dump_node("/dev/qseecom");
	dump_node("/dev/esfp0");

	for (i = 0; i < (int)(sizeof(kLibs) / sizeof(kLibs[0])); i++) {
		lib = dlopen(kLibs[i], RTLD_NOW);
		if (lib) {
			printf("loaded %s\n", kLibs[i]);
			break;
		}
		printf("dlopen %s failed: %s\n", kLibs[i], dlerror());
	}
	if (!lib) {
		printf("RESULT: cannot load libQSEEComAPI\n");
		return 1;
	}

	start_app = (start_app_fn)dlsym(lib, "QSEECom_start_app");
	shutdown_app = (shutdown_app_fn)dlsym(lib, "QSEECom_shutdown_app");
	set_bw = (set_bw_fn)dlsym(lib, "QSEECom_set_bandwidth");
	if (!start_app || !shutdown_app) {
		printf("RESULT: missing QSEECom symbols\n");
		return 1;
	}

	r = start_app(&h, path, fname, sb_size);
	printf("QSEECom_start_app(%s,%s,%u) = %d handle=%p errno=%d (%s)\n",
	       path, fname, sb_size, r, (void *)h, errno, strerror(errno));
	if (r == 0) {
		if (set_bw)
			printf("QSEECom_set_bandwidth = %d\n", set_bw(h, 1));
		printf("sb_virt=%p\n", (void *)(h ? h->ion_sbuffer : NULL));
		printf("RESULT: OK\n");
		printf("QSEECom_shutdown_app = %d\n", shutdown_app(&h));
		return 0;
	}

	printf("RESULT: FAIL (%d)\n", r);
	return 2;
}
