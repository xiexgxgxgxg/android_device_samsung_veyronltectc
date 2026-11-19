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
 * Destroying a mutex only releases the object's own state; the memory is
 * owned by the module instance, so ignoring the call leaks nothing that
 * matters for the lifetime of the daemon.
 */
#include <pthread.h>

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
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


