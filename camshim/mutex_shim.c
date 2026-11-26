/*
 * Android 6.0.1 compatibility shim for the Samsung mm-camera2 blobs.
 *
 * libmmcamera2_isp_modules.so destroys an ISP session mutex twice while
 * mct_pipeline_stop_session() tears a session down.  bionic on Android 6
 * only returned EINVAL for that, but Android 11's bionic treats a second
 * pthread_mutex_destroy() as a FORTIFY violation and calls abort(), which
 * kills mm-qcamera-daemon on every camera close.  The daemon is then
 * restarted by init, re-probes both sensors and the camera HAL's session
 * setup hangs forever in msm_post_event().
 *
 * Only the *second* destroy is turned into a no-op: bionic marks a destroyed
 * mutex by storing 0xffff in its 16-bit state word (pthread_mutex_destroy()
 * calls HandleUsingDestroyedMutex() when it sees that), while every other
 * destroy keeps the normal libc semantics.  That keeps the workaround local
 * to the driver bug instead of neutering pthread_mutex_destroy() for every
 * other user in the daemon.
 */
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>

#define BIONIC_MUTEX_STATE_DESTROYED 0xffff

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	static int (*real_destroy)(pthread_mutex_t *);

	if (mutex == NULL) {
		return 22; /* EINVAL */
	}
	if (atomic_load_explicit((_Atomic uint16_t *)(void *)mutex,
			memory_order_relaxed) == BIONIC_MUTEX_STATE_DESTROYED) {
		return 0;
	}
	if (real_destroy == NULL) {
		real_destroy = (int (*)(pthread_mutex_t *))
			dlsym(RTLD_NEXT, "pthread_mutex_destroy");
		if (real_destroy == NULL) {
			return 0;
		}
	}
	return real_destroy(mutex);
}

/*
 * The Samsung 3A tuning libraries (libTsAe/libTsAwb/libTsAf and the
 * C12Q*_libTs* variants) were built with GCC 4.9 and reference the old
 * libsupc++ entry points.  Android 11's libstdc++.so no longer exports
 * them, so dlopen(..., RTLD_NOW) fails with "cannot locate symbol" and
 * mm-qcamera-daemon dereferences the NULL handle and dies on the front
 * camera.  Provide the (essentially unreachable) entry points so the
 * libraries can be loaded; an actual C++ exception from them is not
 * supported and aborts instead of crashing inside the unwinder.
 */
#include <stdlib.h>
#include <dlfcn.h>

void *__cxa_begin_cleanup(void *exc)
{
	(void)exc;
	return NULL;
}

void *__cxa_type_match(void *thrown, void *catch_type, _Bool is_ref,
		void **matched)
{
	(void)thrown;
	(void)catch_type;
	(void)is_ref;
	if (matched) {
		*matched = NULL;
	}
	return NULL;
}

void __cxa_call_unexpected(void *exc)
{
	(void)exc;
	abort();
}

void *__gnu_Unwind_Find_exidx(void *pc, int *count)
{
	(void)pc;
	if (count) {
		*count = 0;
	}
	return NULL;
}


