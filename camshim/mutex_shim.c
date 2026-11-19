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
