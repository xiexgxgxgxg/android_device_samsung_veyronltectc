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
#include <sys/system_properties.h>


/*
 * P-CSCF on this platform is delivered by the modem through QMI IMS (Samsung's
 * "SDM SIP config"), not through the PCO of the data call.  libsec-ril stores
 * it in ril.pcscfv4.* / ril.pcscfv6.* properties but the legacy
 * RIL_Data_Call_Response_v11 ABI has a single IPv4-only pcscf string, so A11
 * never sees the IPv6 P-CSCF addresses it needs for SIP REGISTER.
 *
 * Republish whatever the modem reported in the AOSP pcscf field; libril hands
 * that string to the framework as-is and the framework parses it as a
 * space-delimited address list.
 */
struct pcscf_collection {
    char value[8 * PROP_VALUE_MAX];
    size_t len;
};

static int pcscf_value_usable(const char *value)
{
    if (value == NULL || value[0] == '\0')
        return 0;
    /* 0.0.0.0 / :: / ::0 -- all-zero placeholders */
    for (const char *p = value; *p != '\0'; p++) {
        if (*p != '0' && *p != ':' && *p != '.')
            return 1;
    }
    return 0;
}

static int pcscf_collection_has(const struct pcscf_collection *c, const char *value)
{
    size_t n = strlen(value);
    const char *p = c->value;

    while (*p != '\0') {
        const char *end = strchr(p, ' ');
        size_t len = (end == NULL) ? strlen(p) : (size_t)(end - p);
        if (len == n && memcmp(p, value, n) == 0)
            return 1;
        if (end == NULL)
            break;
        p = end + 1;
    }
    return 0;
}

static void pcscf_collection_add(struct pcscf_collection *c, const char *value)
{
    size_t n;

    if (!pcscf_value_usable(value) || pcscf_collection_has(c, value))
        return;

    n = strlen(value);
    if (c->len + n + 2 >= sizeof(c->value))
        return;
    if (c->len != 0)
        c->value[c->len++] = ' ';
    memcpy(c->value + c->len, value, n);
    c->len += n;
    c->value[c->len] = '\0';
}

static void pcscf_collect_cb(const prop_info *pi, void *cookie)
{
    struct pcscf_collection *c = (struct pcscf_collection *)cookie;
    char name[PROP_NAME_MAX];
    char value[PROP_VALUE_MAX];

    if (__system_property_read(pi, name, value) < 0)
        return;
    if (strncmp(name, "ril.pcscfv4.", 12) != 0 &&
            strncmp(name, "ril.pcscfv6.", 12) != 0)
        return;
    pcscf_collection_add(c, value);
}

static size_t pcscf_from_properties(char *out, size_t cap)
{
    struct pcscf_collection c;

    if (out == NULL || cap == 0)
        return 0;
    memset(&c, 0, sizeof(c));
    __system_property_foreach(pcscf_collect_cb, &c);
    if (c.len == 0 || c.len >= cap)
        return 0;
    memcpy(out, c.value, c.len + 1);
    return c.len;
}

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
 * Compatibility layer for the response payloads of the stock 6.0.1 RIL.
 *
 * The vendor library was built against Samsung's fork of the Android 6 RIL
 * interface.  Two differences matter on Android 11:
 *
 *   1. RIL_REQUEST_GET_SIM_STATUS: Samsung's RIL_CardStatus_v6 carries a
 *      64-byte RIL_AppStatus (five extra ints after pin2), so the whole
 *      struct is 24 + 8 * 64 = 536 bytes instead of AOSP's
 *      24 + 8 * 48 = 408.  A11's libril rejects anything that is not exactly
 *      sizeof(RIL_CardStatus_v6), which turned a perfectly good "card present"
 *      answer into RadioError::INVALID_RESPONSE.
 *   2. The string-array responses (operator, voice/data registration, ...)
 *      carry extra trailing entries.  A11 accepts only specific counts, so
 *      those answers were rejected the same way.
 *
 * The standard fields are at identical offsets in both layouts, so the fix is
 * a lossless repack: copy the AOSP prefix of every application record and
 * hand libril a buffer of exactly the size it expects.  Extra vendor entries
 * are dropped.  This is done on the boundary between the vendor blob and
 * libril, which is the only place where the ABI is allowed to differ.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <telephony/ril.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef void (*ril_on_request_complete_t)(RIL_Token, RIL_Errno, void*, size_t);

/* libril's private RequestInfo/CommandInfo; the token is a RequestInfo *. */
struct ShimRequestInfo {
    int32_t token;
    void *pCI;
    void *p_next;
    char cancelled;
    char local;
    int socket_id;
    int wasAckSent;
};

struct ShimCommandInfo {
    int requestNumber;
};

static int shim_request_number(void *token)
{
    struct ShimRequestInfo *pRI = (struct ShimRequestInfo *)token;

    if (pRI == NULL || pRI->pCI == NULL)
        return -1;
    return ((struct ShimCommandInfo *)pRI->pCI)->requestNumber;
}

/*
 * Samsung's RIL_CardStatus_v6.  Keep the AOSP field order but with a 64-byte
 * application record.  Only the standard prefix is copied out.
 */
#define SAMSUNG_CARD_STATUS_LEN 536
#define SAMSUNG_APP_LEN         64
#define AOSP_APP_LEN            48
#define CARD_HEADER_LEN         24


/* Prefix length of the IPv4 address configured on the data interface.  The
 * vendor RIL returns the bare address (no "/nn") in the SETUP_DATA_CALL
 * response; AOSP then assumes /32, cannot install the default route (the
 * gateway is outside the /32) and the LTE data network never comes up.  The
 * interface itself is configured correctly, so read the netmask back. */
static int iface_v4_prefix(const char *ifname)
{
    int fd, prefix = 0;
    struct ifreq ifr;
    struct sockaddr_in *sin;
    uint32_t mask;

    if (ifname == NULL)
        return -1;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFNETMASK, &ifr) < 0) {
        close(fd);
        return -1;
    }
    close(fd);
    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    mask = ntohl(sin->sin_addr.s_addr);
    while (mask & 0x80000000U) {
        prefix++;
        mask <<= 1;
    }
    return prefix;
}

/* Append the real IPv4 prefix to address tokens that do not carry one (or
 * carry the useless /32).  IPv6 tokens are left untouched. */
static char *fix_address_prefixes(const char *addresses, int prefix)
{
    size_t cap = strlen(addresses) + 16 * 8 + 1;
    char *out = (char *)malloc(cap);
    const char *p = addresses;
    size_t o = 0;

    if (out == NULL)
        return NULL;
    while (*p) {
        const char *end = strchr(p, ' ');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 0 && memchr(p, ':', len) == NULL) {
            const char *slash = (const char *)memchr(p, '/', len);
            if (slash == NULL) {
                memcpy(out + o, p, len);
                o += len;
                o += snprintf(out + o, cap - o, "/%d", prefix);
            } else {
                size_t hostlen = (size_t)(slash - p);
                memcpy(out + o, p, hostlen);
                o += hostlen;
                o += snprintf(out + o, cap - o, "/%d", prefix);
            }
        } else {
            memcpy(out + o, p, len);
            o += len;
        }
        if (end) {
            out[o++] = ' ';
            p = end + 1;
        } else {
            p += len;
        }
    }
    out[o] = '\0';
    return out;
}

/*
 * The stock RIL announces "gsm.current.phone-type=2" (CDMA) as soon as it
 * sees the CSIM application on this China Telecom card.  The framework then
 * builds a CDMA phone, which cannot read the USIM MSISDN (phone number shows
 * unknown) and whose Settings network-mode page throws on the resulting
 * unknown network type.  This is a single-SIM LTE/USIM build: force the
 * announced phone type to GSM so the framework uses the USIM path.  The
 * modem still sees and uses both card applications internally.
 */
extern "C" int property_set(const char* key, const char* value)
{
    static int (*real_fn)(const char*, const char*);
    if (real_fn == NULL)
        real_fn = (int (*)(const char*, const char*))dlsym(RTLD_NEXT, "property_set");
    if (real_fn == NULL)
        return -1;
    if (key != NULL && value != NULL && strcmp(key, "gsm.current.phone-type") == 0 &&
            value[0] == '2' && value[1] == '\0')
        value = "1";
    return real_fn(key, value);
}

extern "C" int __system_property_set(const char* key, const char* value)
{
    static int (*real_fn)(const char*, const char*);
    if (real_fn == NULL)
        real_fn = (int (*)(const char*, const char*))dlsym(RTLD_NEXT, "__system_property_set");
    if (real_fn == NULL)
        return -1;
    if (key != NULL && value != NULL && strcmp(key, "gsm.current.phone-type") == 0 &&
            value[0] == '2' && value[1] == '\0')
        value = "1";
    return real_fn(key, value);
}

extern "C" void RIL_onRequestComplete(RIL_Token token, RIL_Errno error, void* response,
                                      size_t responselen)
{
    static ril_on_request_complete_t real_fn;
    int req = shim_request_number(token);

    if (real_fn == NULL)
        real_fn = (ril_on_request_complete_t)dlsym(RTLD_NEXT, "RIL_onRequestComplete");

    if (real_fn == NULL)
        return;

    if (req == RIL_REQUEST_GET_SIM_STATUS && error == 0 && response != NULL &&
            responselen == SAMSUNG_CARD_STATUS_LEN) {
        RIL_CardStatus_v6 out;
        const unsigned char *in = (const unsigned char *)response;
        int num = 0;

        memset(&out, 0, sizeof(out));
        memcpy(&out.card_state, in, CARD_HEADER_LEN);
        num = out.num_applications;
        if (num < 0)
            num = 0;
        if (num > RIL_CARD_MAX_APPS)
            num = RIL_CARD_MAX_APPS;
        out.num_applications = num;
        for (int i = 0; i < num; i++) {
            /* standard fields end after pin2; the rest is vendor-private */
            memcpy(&out.applications[i],
                   in + CARD_HEADER_LEN + i * SAMSUNG_APP_LEN, 44);
        }

        /* Present the USIM as the only application.  The stock 6.0.1 card
         * reports CSIM+USIM; A11 then builds a CDMA phone whose MDN path
         * cannot read this card (phone number unknown) and whose network-mode
         * settings page crashes on an unknown phone type.  The modem keeps
         * using both apps internally; this only changes what the framework
         * is told about the card. */
        if (out.card_state == 1 && out.num_applications > 1) {
            int usim = -1;
            for (int i = 0; i < out.num_applications; i++) {
                if (out.applications[i].app_type == RIL_APPTYPE_USIM) {
                    usim = i;
                    break;
                }
            }
            if (usim > 0) {
                RIL_AppStatus keep = out.applications[usim];
                memset(out.applications, 0, sizeof(out.applications));
                out.applications[0] = keep;
                out.num_applications = 1;
                out.gsm_umts_subscription_app_index = 0;
                out.cdma_subscription_app_index = -1;
                out.ims_subscription_app_index = -1;
            }
        }

        __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                            "card status repacked: state=%d apps=%d gsm=%d cdma=%d ims=%d",
                            out.card_state, out.num_applications,
                            out.gsm_umts_subscription_app_index,
                            out.cdma_subscription_app_index,
                            out.ims_subscription_app_index);
        real_fn(token, error, &out, sizeof(out));
        return;
    }

    if (req == RIL_REQUEST_VOICE_RADIO_TECH && error == 0 && response != NULL &&
            responselen == sizeof(int)) {
        /* The modem answers a CDMA/unknown voice RAT on this China Telecom
         * USIM; the framework then builds a CDMA phone object.  Report LTE
         * so the USIM/MSISDN path is used. */
        int rat = 14; /* RADIO_TECH_LTE */
        __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                            "voice rat %d -> %d", *(int *)response, rat);
        real_fn(token, error, &rat, sizeof(rat));
        return;
    }

    if (req == RIL_REQUEST_SETUP_DATA_CALL && error == 0 && response != NULL &&
            (responselen == sizeof(RIL_Data_Call_Response_v11) ||
             responselen == sizeof(RIL_Data_Call_Response_v9) ||
             responselen == sizeof(RIL_Data_Call_Response_v6))) {
        /* The stock 6.0.1 RIL answers with the v6 layout (56 bytes).  A11
         * accepts it, but AOSP fills a missing IPv4 prefix with /32, which
         * makes the default route uninstallable.  Copy the matching layout,
         * rewrite only the address string and pass that on. */
        if (responselen == sizeof(RIL_Data_Call_Response_v11)) {
            RIL_Data_Call_Response_v11 dc = *(RIL_Data_Call_Response_v11 *)response;
            char pcscf[8 * PROP_VALUE_MAX];
            char *fixed = NULL;
            int prefix = iface_v4_prefix(dc.ifname);

            if (prefix > 0 && prefix < 32 && dc.addresses != NULL) {
                fixed = fix_address_prefixes(dc.addresses, prefix);
                if (fixed != NULL) {
                    dc.addresses = fixed;
                    __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                                        "data call addresses fixed: %s", fixed);
                }
            }

            /* The legacy v11 pcscf field is IPv4-only and this modem answers
             * through QMI IMS, so republish what libsec-ril recorded. */
            if (pcscf_from_properties(pcscf, sizeof(pcscf)) > 0) {
                dc.pcscf = pcscf;
                __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                                    "data call pcscf from modem: %s", pcscf);
            }

            real_fn(token, error, &dc, sizeof(dc));
            free(fixed);
            return;
        } else if (responselen == sizeof(RIL_Data_Call_Response_v9)) {
            RIL_Data_Call_Response_v9 dc = *(RIL_Data_Call_Response_v9 *)response;
            int prefix = iface_v4_prefix(dc.ifname);
            if (prefix > 0 && prefix < 32 && dc.addresses != NULL) {
                char *fixed = fix_address_prefixes(dc.addresses, prefix);
                if (fixed != NULL) {
                    dc.addresses = fixed;
                    __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                                        "data call addresses fixed: %s", fixed);
                    real_fn(token, error, &dc, sizeof(dc));
                    free(fixed);
                    return;
                }
            }
        } else {
            RIL_Data_Call_Response_v6 dc = *(RIL_Data_Call_Response_v6 *)response;
            int prefix = iface_v4_prefix(dc.ifname);
            if (prefix > 0 && prefix < 32 && dc.addresses != NULL) {
                char *fixed = fix_address_prefixes(dc.addresses, prefix);
                if (fixed != NULL) {
                    dc.addresses = fixed;
                    __android_log_print(ANDROID_LOG_INFO, "RILSHIM",
                                        "data call addresses fixed: %s", fixed);
                    real_fn(token, error, &dc, sizeof(dc));
                    free(fixed);
                    return;
                }
            }
        }
    }

    /* The vendor RIL appends Samsung-private fields to the old-style string
     * arrays.  A11's parser only accepts the AOSP field counts and rejects the
     * whole answer otherwise, which made ServiceStateTracker retry in a tight
     * loop and never settle on a service state.  Keep the AOSP prefix. */
    {
        size_t want = 0;
        if (req == RIL_REQUEST_VOICE_REGISTRATION_STATE)
            want = 15;
        else if (req == RIL_REQUEST_DATA_REGISTRATION_STATE)
            want = 13;
        else if (req == RIL_REQUEST_OPERATOR)
            want = 3;

        if (want != 0 && responselen > want * sizeof(char *) &&
                (responselen % sizeof(char *)) == 0) {
            char *trimmed[RIL_CARD_MAX_APPS + 40];
            memcpy(trimmed, response, want * sizeof(char *));
            if (req == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
                /* Keep the framework on an LTE (3GPP) phone object.  The
                 * modem answers RAT 0 (unknown) on this USIM/CSIM card, and
                 * A11 then falls back to a CDMA phone object whose MSISDN
                 * path cannot read this card. */
                trimmed[3] = (char *)"14";
            }
            real_fn(token, error, trimmed, want * sizeof(char *));
            return;
        }
    }

    real_fn(token, error, response, responselen);
}
