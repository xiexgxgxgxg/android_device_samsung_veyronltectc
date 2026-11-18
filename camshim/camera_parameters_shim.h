/*
 * Compatibility shim for the Android 6.0.1 camera HAL: see Android.mk.
 */
#ifndef VEYRON_CAMERA_PARAMETERS_SHIM_H
#define VEYRON_CAMERA_PARAMETERS_SHIM_H

#include <stdint.h>

namespace android {
struct CameraParameters {
    static const char KEY_RT_HDR[];
    static const char RTHDR_AUTO[];
    static const char OIS_ON_ZOOM[];
    static const char KEY_PHASE_AF[];
    static const char OIS_ON_STILL[];
    static const char OIS_ON_VIDEO[];
    static const char OIS_CENTERING[];
    static const char OIS_ON_SINE_X[];
    static const char OIS_ON_SINE_Y[];
    static const char WHITE_BALANCE_K[];
    static const char KEY_OIS_SUPPORTED[];
    static const char KEY_SUPPORTED_RT_HDR[];
    static const char KEY_MAX_EXPOSURE_TIME[];
    static const char KEY_MIN_EXPOSURE_TIME[];
    static const char KEY_SUPPORTED_PHASE_AF[];
    static const char KEY_SUPPORTED_OIS_MODES[];
    static const char KEY_DYNAMIC_RANGE_CONTROL[];
    static const char PIXEL_FORMAT_YUV420SP_NV21[];
    static const char KEY_SUPPORTED_DYNAMIC_RANGE_CONTROL[];
    static const char DRC_ON[];
    static const char PAF_ON[];
    static const char DRC_OFF[];
    static const char KEY_OIS[];
    static const char OIS_OFF[];
    static const char PAF_OFF[];
    static const char RTHDR_ON[];
    static const char RTHDR_OFF[];
    // provided by libcamera_client
    int getInt(const char* key) const;
    int64_t getInt64(const char* key) const;
};
}  // namespace android

#endif
