/*
 * Compatibility shim for the stock 6.0.1 vendor RIL library.
 *
 * libsec-ril.so was built against the Android 6 update of libutils and uses
 * strdup8to16()/strdup16to8(), which were dropped from libutils long before
 * Android 11.  The library is loaded by AOSP's rild (which carries the IRadio
 * HIDL implementation), so the missing symbols are provided here and injected
 * with LD_PRELOAD instead of replacing A11's libutils, whose ABI the rest of
 * the process (libril, binder, ...) depends on.
 *
 * These are plain C entry points in the old libutils (utils/Unicode.h), i.e.
 * they are exported without C++ mangling, and the caller frees the result with
 * free().
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

extern "C" {

uint16_t* strdup8to16(const char* s, size_t* out_len)
{
    size_t n, i = 0, o = 0;
    uint16_t* out;

    if (s == NULL)
        return NULL;

    n = strlen(s);
    out = (uint16_t*)malloc((n + 1) * sizeof(uint16_t));
    if (out == NULL)
        return NULL;

    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        size_t len, k;

        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            free(out);
            return NULL;
        }

        if (i + len > n) {
            free(out);
            return NULL;
        }

        for (k = 1; k < len; k++) {
            unsigned char cc = (unsigned char)s[i + k];

            if ((cc & 0xC0) != 0x80) {
                free(out);
                return NULL;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += len;

        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            free(out);
            return NULL;
        }

        if (cp <= 0xFFFF) {
            out[o++] = (uint16_t)cp;
        } else {
            cp -= 0x10000;
            out[o++] = (uint16_t)(0xD800 + (cp >> 10));
            out[o++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
        }
    }

    out[o] = 0;
    if (out_len != NULL)
        *out_len = o;

    return out;
}

char* strdup16to8(const uint16_t* s)
{
    size_t i = 0, o = 0;
    char* out;

    if (s == NULL)
        return NULL;

    /* worst case: every UTF-16 unit becomes a 3-byte sequence */
    for (i = 0; s[i] != 0; i++)
        ;
    out = (char*)malloc(i * 3 + 1);
    if (out == NULL)
        return NULL;

    for (i = 0; s[i] != 0; i++) {
        uint32_t cp = s[i];

        if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] >= 0xDC00 &&
                s[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            i++;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            free(out);
            return NULL;
        }

        if (cp < 0x80) {
            out[o++] = (char)cp;
        } else if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    out[o] = 0;
    return out;
}

}  /* extern "C" */

/*
 * Samsung's peripheral-manager client (libperipheral_client.so).
 *
 * The vendor RIL registers itself as a "QCRIL" client of pm-service right after
 * RIL_Init.  On this boot flow pm-service never learns that the modem is up
 * (its QMI service side does not receive the modem registration, it keeps
 * reporting "modem state: is off-line"), and the vendor library then walks a
 * path that ends in a call through a NULL function pointer - the rild process
 * dies with pc == 0 about 90 ms after "QCRIL registered".
 *
 * Stub the registration so the library still comes up.  The modem state that
 * matters for telephony comes from QMI anyway; this only removes the
 * peripheral-manager notification channel.
 */
extern "C" int pm_client_register(void* client, const char* name,
                                  const char* name2, const char* desc,
                                  void* out, void* extra)
{
    (void)client;
    (void)name;
    (void)name2;
    (void)desc;
    (void)out;
    (void)extra;
    return 0;  /* success, no events will be delivered */
}

extern "C" int pm_client_unregister(void* client)
{
    (void)client;
    return 0;
}

extern "C" int pm_client_event_acknowledge(void* client, int event, void* data)
{
    (void)client;
    (void)event;
    (void)data;
    return 0;
}

/*
 * Diagnostic: log every RIL request completion so we can see what the vendor
 * library actually answers (in particular RIL_REQUEST_BASEBAND_VERSION, whose
 * response has a single string payload).
 */
#include <dlfcn.h>

typedef void (*ril_on_request_complete_t)(void*, int, void*, size_t);

extern "C" void RIL_onRequestComplete(void* token, int error, void* response,
                                      size_t responselen)
{
    static ril_on_request_complete_t real_fn;
    static int inited;

    if (!inited) {
        real_fn = (ril_on_request_complete_t)dlsym(RTLD_NEXT, "RIL_onRequestComplete");
        inited = 1;
    }

    __android_log_print(ANDROID_LOG_INFO, "RILSHIM", "complete err=%d len=%zu",
                        error, responselen);
    if (error == 0 && responselen == sizeof(char*) && response != NULL) {
        const char* str = *(const char**)response;
        if (str != NULL && str[0] != '\0') {
            __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                                "completion single-string: %s", str);
        }
    } else if (error != 0) {
        __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                            "completion error=%d len=%zu", error, responselen);
    }

    if (real_fn != NULL) {
        real_fn(token, error, response, responselen);
    }
}
