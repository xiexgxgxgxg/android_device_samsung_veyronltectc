#include <android/sensor.h>
#include <stdio.h>
#include <unistd.h>
int main(void) {
    ASensorManager *mgr = ASensorManager_getInstance();
    ASensor const *s = ASensorManager_getDefaultSensor(mgr, ASENSOR_TYPE_LIGHT);
    if (!s) { printf("no light sensor\n"); return 1; }
    printf("light sensor: %s\n", ASensor_getName(s));
    ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ASensorEventQueue *q = ASensorManager_createEventQueue(mgr, looper, 1, NULL, NULL);
    ASensorEventQueue_enableSensor(q, s);
    ASensorEventQueue_setEventRate(q, s, 200000);
    int n = 0;
    for (int i = 0; i < 200 && n < 12; i++) {
        if (ALooper_pollAll(300, NULL, NULL, NULL) >= 0) {
            ASensorEvent ev;
            while (ASensorEventQueue_getEvents(q, &ev, 1) > 0) {
                if (ev.type == ASENSOR_TYPE_LIGHT) {
                    printf("lux=%.1f\n", ev.data[0]);
                    n++;
                }
            }
        }
    }
    return 0;
}
