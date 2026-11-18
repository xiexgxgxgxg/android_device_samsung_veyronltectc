#include "camera_parameters_shim.h"

#include <dlfcn.h>

namespace android {
const char CameraParameters::KEY_RT_HDR[] = "rt-hdr";
const char CameraParameters::RTHDR_AUTO[] = "auto";
const char CameraParameters::OIS_ON_ZOOM[] = "zoom";
const char CameraParameters::KEY_PHASE_AF[] = "phase-af";
const char CameraParameters::OIS_ON_STILL[] = "still";
const char CameraParameters::OIS_ON_VIDEO[] = "video";
const char CameraParameters::OIS_CENTERING[] = "center";
const char CameraParameters::OIS_ON_SINE_X[] = "sine_x";
const char CameraParameters::OIS_ON_SINE_Y[] = "sine_y";
const char CameraParameters::WHITE_BALANCE_K[] = "temperature";
const char CameraParameters::KEY_OIS_SUPPORTED[] = "ois-supported";
const char CameraParameters::KEY_SUPPORTED_RT_HDR[] = "rt-hdr-values";
const char CameraParameters::KEY_MAX_EXPOSURE_TIME[] = "max-exposure-time";
const char CameraParameters::KEY_MIN_EXPOSURE_TIME[] = "min-exposure-time";
const char CameraParameters::KEY_SUPPORTED_PHASE_AF[] = "phase-af-values";
const char CameraParameters::KEY_SUPPORTED_OIS_MODES[] = "ois-mode-values";
const char CameraParameters::KEY_DYNAMIC_RANGE_CONTROL[] = "dynamic-range-control";
const char CameraParameters::PIXEL_FORMAT_YUV420SP_NV21[] = "nv21";
const char CameraParameters::KEY_SUPPORTED_DYNAMIC_RANGE_CONTROL[] = "dynamic-range-control-values";
const char CameraParameters::DRC_ON[] = "on";
const char CameraParameters::PAF_ON[] = "on";
const char CameraParameters::DRC_OFF[] = "off";
const char CameraParameters::KEY_OIS[] = "ois";
const char CameraParameters::OIS_OFF[] = "off";
const char CameraParameters::PAF_OFF[] = "off";
const char CameraParameters::RTHDR_ON[] = "on";
const char CameraParameters::RTHDR_OFF[] = "off";

int64_t CameraParameters::getInt64(const char* key) const {
    // The Android 11 libcamera_client has getInt() but not getInt64(), and it
    // is not vendor_available, so resolve getInt at runtime instead of linking.
    typedef int (*GetIntFn)(const CameraParameters*, const char*);
    static GetIntFn get_int = reinterpret_cast<GetIntFn>(
            dlsym(RTLD_DEFAULT, "_ZNK7android16CameraParameters6getIntEPKc"));
    if (get_int == nullptr) return 0;
    return static_cast<int64_t>(get_int(this, key));
}
}  // namespace android
