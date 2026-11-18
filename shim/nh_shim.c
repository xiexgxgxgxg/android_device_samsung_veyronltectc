/*
 * The Samsung camera libraries (libuniplugin, libuniapi, libsensorlistener)
 * carry a DT_NEEDED on libnativehelper.so, but they do not reference a single
 * symbol from it: it is a leftover from the Android 6.0.1 build.  Android 11
 * only ships a 64-bit libnativehelper, and dropping the 6.0.1 32-bit one into
 * /system/lib breaks the 32-bit zygote, so the three blobs are patched to
 * depend on this empty shim instead (see the DT_NEEDED patch in the vendor
 * tree) and this library satisfies the loader.
 */
int libnh_shim_dummy;
