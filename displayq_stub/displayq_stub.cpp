/*
 * Minimal "display.qservice" provider for the wrapped 6.0.1 camera HAL.
 *
 * QCamera3HardwareInterface::openCamera() asks ServiceManager for the
 * Qualcomm display service ("display.qservice") and waits 5 s for it to show
 * up; when it is missing the HAL logs "getBinder: invalid binder object" and
 * runs with a NULL display client, which leaves the preview stream without a
 * producer.  The Android 11 display stack is AOSP HWC/HIDL and has no such
 * service, so provide the binder name the legacy HAL expects: createClient()
 * returns a dummy IQClient binder, which is all the HAL needs to stop waiting
 * and to keep its preview path happy.  Nothing else in the system looks up
 * this name.
 */
#include <binder/Binder.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <utils/String16.h>
#include <utils/Log.h>

#include <unistd.h>

using namespace android;

namespace {

enum { CREATE_CLIENT = IBinder::FIRST_CALL_TRANSACTION };

class DummyClient : public BBinder {
  protected:
    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
            uint32_t flags) override {
        (void)data;
        (void)flags;
        if (reply != nullptr) {
            reply->writeInt32(0);
        }
        ALOGV("display.qservice dummy client transaction %u", code);
        return NO_ERROR;
    }
};

class DisplayQService : public BBinder {
  protected:
    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
            uint32_t flags) override {
        if (code == CREATE_CLIENT) {
            if (reply != nullptr) {
                reply->writeStrongBinder(new DummyClient());
            }
            ALOGI("display.qservice: createClient() -> dummy client");
            return NO_ERROR;
        }
        return BBinder::onTransact(code, data, reply, flags);
    }
};

}  // namespace

int main() {
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("display.qservice: no servicemanager");
        return 1;
    }
    status_t res = sm->addService(String16("display.qservice"),
            new DisplayQService(), false, 0);
    if (res != NO_ERROR) {
        ALOGE("display.qservice: addService failed: %d", res);
        return 1;
    }
    ALOGI("display.qservice: registered");
    // Service binder transactions on a thread pool; without this the calls
    // the camera HAL makes on the returned client would block forever.
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
