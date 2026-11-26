/*
 * Android 6.0.1 -> Android 11 ABI compatibility for android::CameraMetadata.
 *
 * The Samsung 6.0.1 camera HAL (camera.msm8996.so) was compiled against the
 * Android 6 camera_metadata header, where android::CameraMetadata holds just
 * a single member:
 *
 *     class CameraMetadata { ... camera_metadata_t *mBuffer; };
 *
 * Android 11's libcamera_client.so implements the same class with an extra
 * `mutable bool mLocked` member (added in Android 8 for getAndLock()/unlock()).
 * The HAL's out-of-line CameraMetadata calls therefore resolve to the A11
 * implementations, which read and write `mLocked` at offset 4 - memory that
 * belongs to the *next* member of the embedding C++ object.  In practice that
 * byte reads as nonzero, so every update()/append() aborts with
 *
 *     E Camera2-Metadata: update: CameraMetadata is locked
 *
 * and release() returns NULL, which makes the A11 camera provider crash in
 * CameraModule::getCameraInfo() -> CameraMetadata::append(NULL).
 *
 * The HAL binary is therefore renamed (see tools/patch_camera_metadata_abi.py)
 * to reference android::LegacyMetadata instead, and this library provides that
 * class with the 6.0.1 layout (mBuffer only) and the 6.0.1 semantics: no
 * locking, all operations done directly on the camera_metadata C API.
 *
 * The rename keeps the symbol string lengths identical, so the blob's .dynstr
 * can be rewritten in place; and because the class name is unique, preloading
 * this library into cameraserver / the vendor provider cannot interfere with
 * the real android::CameraMetadata those processes use themselves.
 *
 * NOTE: every method is defined out of line.  Defining them inside the class
 * makes them implicitly inline, and the linker then drops all of them as
 * unused (only the two out-of-line helpers survived the first build).
 */
#define LOG_TAG "CamMetadataLegacy"

#include <log/log.h>
#include <utils/Errors.h>
#include <utils/String8.h>

#include <system/camera_metadata.h>

namespace android {

class LegacyMetadata {
  public:
    LegacyMetadata();
    LegacyMetadata(const LegacyMetadata &other);
    ~LegacyMetadata();

    LegacyMetadata &operator=(const LegacyMetadata &other);
    LegacyMetadata &operator=(const camera_metadata_t *buffer);

    status_t update(uint32_t tag, const void *data, size_t data_count);
    status_t update(uint32_t tag, const uint8_t *data, size_t data_count);
    status_t update(uint32_t tag, const int32_t *data, size_t data_count);
    status_t update(uint32_t tag, const float *data, size_t data_count);
    status_t update(uint32_t tag, const int64_t *data, size_t data_count);
    status_t update(uint32_t tag, const double *data, size_t data_count);
    status_t update(uint32_t tag, const camera_metadata_rational_t *data,
            size_t data_count);
    status_t update(uint32_t tag, const String8 &string);

    status_t append(const LegacyMetadata &other);
    status_t append(const camera_metadata_t *other);

    camera_metadata_t *release();
    void clear();

    size_t entryCount() const;
    size_t dataCount() const;
    bool isEmpty() const;
    bool exists(uint32_t tag) const;

    camera_metadata_entry_t find(uint32_t tag);
    camera_metadata_ro_entry_t find(uint32_t tag) const;

  private:
    status_t updateImpl(uint32_t tag, const void *data, size_t data_count);
    status_t resizeIfNeeded(size_t extraEntries, size_t extraData);

    camera_metadata_t *mBuffer;
};

LegacyMetadata::LegacyMetadata() : mBuffer(NULL) {
}

LegacyMetadata::LegacyMetadata(const LegacyMetadata &other) : mBuffer(NULL) {
    *this = other;
}

LegacyMetadata::~LegacyMetadata() {
    clear();
}

LegacyMetadata &LegacyMetadata::operator=(const LegacyMetadata &other) {
    return operator=(other.mBuffer);
}

LegacyMetadata &LegacyMetadata::operator=(const camera_metadata_t *buffer) {
    if (buffer != mBuffer) {
        camera_metadata_t *newBuffer = clone_camera_metadata(buffer);
        clear();
        mBuffer = newBuffer;
    }
    return *this;
}

status_t LegacyMetadata::update(uint32_t tag, const void *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const uint8_t *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const int32_t *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const float *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const int64_t *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const double *data,
        size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag,
        const camera_metadata_rational_t *data, size_t data_count) {
    return updateImpl(tag, data, data_count);
}

status_t LegacyMetadata::update(uint32_t tag, const String8 &string) {
    // String8::size() does not count the terminating NUL.
    return updateImpl(tag, string.string(), string.size() + 1);
}

status_t LegacyMetadata::append(const LegacyMetadata &other) {
    return append(other.mBuffer);
}

status_t LegacyMetadata::append(const camera_metadata_t *other) {
    if (other == NULL) {
        return BAD_VALUE;
    }
    size_t extraEntries = get_camera_metadata_entry_count(other);
    size_t extraData = get_camera_metadata_data_count(other);
    status_t res = resizeIfNeeded(extraEntries, extraData);
    if (res != OK) {
        return res;
    }
    return append_camera_metadata(mBuffer, other);
}

camera_metadata_t *LegacyMetadata::release() {
    camera_metadata_t *released = mBuffer;
    mBuffer = NULL;
    return released;
}

void LegacyMetadata::clear() {
    if (mBuffer != NULL) {
        free_camera_metadata(mBuffer);
        mBuffer = NULL;
    }
}

size_t LegacyMetadata::entryCount() const {
    return (mBuffer == NULL) ? 0 : get_camera_metadata_entry_count(mBuffer);
}

size_t LegacyMetadata::dataCount() const {
    return (mBuffer == NULL) ? 0 : get_camera_metadata_data_count(mBuffer);
}

bool LegacyMetadata::isEmpty() const {
    return entryCount() == 0;
}

bool LegacyMetadata::exists(uint32_t tag) const {
    camera_metadata_ro_entry_t entry;
    return find_camera_metadata_ro_entry(mBuffer, tag, &entry) == 0;
}

camera_metadata_entry_t LegacyMetadata::find(uint32_t tag) {
    status_t res;
    camera_metadata_entry_t entry;
    res = find_camera_metadata_entry(mBuffer, tag, &entry);
    if (res != OK) {
        entry.count = 0;
        entry.data.u8 = NULL;
    }
    return entry;
}

camera_metadata_ro_entry_t LegacyMetadata::find(uint32_t tag) const {
    status_t res;
    camera_metadata_ro_entry_t entry;
    res = find_camera_metadata_ro_entry(mBuffer, tag, &entry);
    if (res != OK) {
        entry.count = 0;
        entry.data.u8 = NULL;
    }
    return entry;
}

status_t LegacyMetadata::updateImpl(uint32_t tag, const void *data,
        size_t data_count) {
    status_t res;

    // The 6.0.1 HAL updates both AOSP and Samsung vendor tags.  Vendor tags
    // have no local section in this process, so get_local_..._tag_type()
    // returns -1 for them; use a worst-case size estimate instead of failing
    // the way the A11 implementation would.
    int type = get_local_camera_metadata_tag_type(tag, mBuffer);
    size_t data_size = (type == -1) ? data_count * 8 :
            calculate_camera_metadata_entry_data_size(type, data_count);

    res = resizeIfNeeded(1, data_size);
    if (res != OK) {
        return res;
    }

    camera_metadata_entry_t entry;
    res = find_camera_metadata_entry(mBuffer, tag, &entry);
    if (res == NAME_NOT_FOUND) {
        res = add_camera_metadata_entry(mBuffer, tag, data, data_count);
    } else if (res == OK) {
        res = update_camera_metadata_entry(mBuffer, entry.index, data,
                data_count, NULL);
    }

    if (res != OK) {
        ALOGE("%s: unable to update tag 0x%x (%d)", __FUNCTION__, tag, res);
    }
    return res;
}

status_t LegacyMetadata::resizeIfNeeded(size_t extraEntries, size_t extraData) {
    if (mBuffer == NULL) {
        mBuffer = allocate_camera_metadata(extraEntries * 2, extraData * 2);
        if (mBuffer == NULL) {
            ALOGE("%s: can't allocate metadata buffer", __FUNCTION__);
            return NO_MEMORY;
        }
        return OK;
    }

    size_t currentEntryCount = get_camera_metadata_entry_count(mBuffer);
    size_t currentEntryCap = get_camera_metadata_entry_capacity(mBuffer);
    size_t newEntryCount = currentEntryCount + extraEntries;
    newEntryCount = (newEntryCount > currentEntryCap) ?
            newEntryCount * 2 : currentEntryCap;

    size_t currentDataCount = get_camera_metadata_data_count(mBuffer);
    size_t currentDataCap = get_camera_metadata_data_capacity(mBuffer);
    size_t newDataCount = currentDataCount + extraData;
    newDataCount = (newDataCount > currentDataCap) ?
            newDataCount * 2 : currentDataCap;

    if (newEntryCount > currentEntryCap || newDataCount > currentDataCap) {
        camera_metadata_t *oldBuffer = mBuffer;
        mBuffer = allocate_camera_metadata(newEntryCount, newDataCount);
        if (mBuffer == NULL) {
            mBuffer = oldBuffer;
            ALOGE("%s: can't allocate metadata buffer", __FUNCTION__);
            return NO_MEMORY;
        }
        append_camera_metadata(mBuffer, oldBuffer);
        free_camera_metadata(oldBuffer);
    }
    return OK;
}

}  // namespace android

/*
 * Android 6.0.1 camera HAL compatibility: QCameraPerfLock.
 *
 * The HAL calls QCameraPerfLock::lock_acq_ss() from
 * QCamera3HardwareInterface::openCamera() right after it has talked to
 * ServiceManager, and the wrapped 6.0.1 object it passes is a NULL-based
 * pointer (this = 0x98): the perf-lock mutex then lives at an unmapped
 * address and the process dies in pthread_mutex_lock() before the camera
 * device is ever returned to the framework.  The 6.0.1 perf-lock class is a
 * CPU-boost hint for the closed Qualcomm perf daemon, which Android 11 does
 * not have (the AOSP power HAL does the boosting instead), so neutralising
 * the entry point keeps the camera functional; nothing else reads or writes
 * the C++ object.  Remove this once the legacy perf-lock call is understood.
 */
#include <stdint.h>

extern "C" int camera_perf_lock_noop(void *self, uint32_t type,
        uint32_t min_max, uint32_t enable)
        __asm__("_ZN7qcamera15QCameraPerfLock11lock_acq_ssE20perflock_type_enum_t24min_max_lock_type_enum_tj");

extern "C" int camera_perf_lock_noop(void *self, uint32_t type,
        uint32_t min_max, uint32_t enable)
{
    (void)self;
    (void)type;
    (void)min_max;
    (void)enable;
    return 0;
}
