// Minimal GNSS HIDL client used to verify the W2017 GNSS HAL end to end:
// it registers a callback, calls start() and prints status / SV / NMEA /
// position reports for two minutes.
#include <android/hardware/gnss/1.0/IGnss.h>
#include <android/hardware/gnss/1.0/IGnssCallback.h>

#include <unistd.h>

#include <cstdio>
#include <cstdint>
#include <ctime>

using namespace android::hardware::gnss::V1_0;
using android::hardware::Return;
using android::hardware::hidl_string;
using android::hardware::hidl_bitfield;
using android::sp;

struct Cb : public IGnssCallback {
    Return<void> gnssLocationCb(const GnssLocation& l) override {
        printf("%ld LOC lat=%.7f lon=%.7f acc=%.1f alt=%.1f spd=%.2f\n",
               (long)time(nullptr), l.latitudeDegrees, l.longitudeDegrees,
               l.horizontalAccuracyMeters, l.altitudeMeters, l.speedMetersPerSec);
        fflush(stdout);
        return {};
    }
    Return<void> gnssStatusCb(GnssStatusValue s) override {
        printf("STATUS %d\n", (int)s);
        fflush(stdout);
        return {};
    }
    Return<void> gnssSvStatusCb(const GnssSvStatus& sv) override {
        printf("SV num=%d\n", (int)sv.numSvs);
        fflush(stdout);
        return {};
    }
    Return<void> gnssNmeaCb(int64_t, const hidl_string& n) override {
        printf("NMEA %s", n.c_str());
        fflush(stdout);
        return {};
    }
    Return<void> gnssSetCapabilitesCb(hidl_bitfield<IGnssCallback::Capabilities> c) override {
        printf("CAPS %u\n", (unsigned)c);
        fflush(stdout);
        return {};
    }
    Return<void> gnssAcquireWakelockCb() override { return {}; }
    Return<void> gnssReleaseWakelockCb() override { return {}; }
    Return<void> gnssRequestTimeCb() override { return {}; }
    Return<void> gnssSetSystemInfoCb(const IGnssCallback::GnssSystemInfo&) override { return {}; }
};

int main() {
    auto gnss = IGnss::getService();
    if (gnss == nullptr) {
        printf("no IGnss service\n");
        return 1;
    }
    printf("service ok\n");
    sp<Cb> cb = new Cb();
    printf("setCallback=%d\n", (int)gnss->setCallback(cb));
    printf("start=%d\n", (int)gnss->start());
    for (int i = 0; i < 120; i++) sleep(1);
    printf("stop=%d\n", (int)gnss->stop());
    return 0;
}
