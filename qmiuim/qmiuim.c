/*
 * qmiuim - development helper: ask the modem for its UIM card status over QMI.
 *
 * The stock RIL reports "no SIM" on this device even with a known good card,
 * and its own logs are compiled out.  This tool talks to the modem's UIM
 * service directly (through qmuxd, the same transport the RIL uses) and dumps
 * the raw QMI UIM_GET_CARD_STATUS response, so we can tell whether the modem
 * sees the card at all or whether the problem is further up in the RIL.
 *
 * The QMI libraries only exist as prebuilt vendor blobs, so everything is
 * resolved with dlopen()/dlsym() instead of at link time.
 *
 * Only used while bringing the baseband up; remove it once telephony works.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

typedef void *qmi_client_type;
typedef void *qmi_idl_service_object_type;

/* opaque service info (service_id + instance_id and padding) */
typedef struct {
    uint32_t data[8];
} qmi_service_info;

#define UIM_GET_CARD_STATUS_REQ 0x0020

static void dump(const char *tag, const unsigned char *p, unsigned int len)
{
    unsigned int i;

    printf("%s (%u bytes):", tag, len);
    for (i = 0; i < len; i++) {
        if (i % 16 == 0)
            printf("\n  %04x:", i);
        printf(" %02x", p[i]);
    }
    printf("\n");
}

static void parse_tlvs(const unsigned char *p, unsigned int len)
{
    unsigned int i = 0;

    while (i + 3 <= len) {
        unsigned int type = p[i];
        unsigned int tlen = p[i + 1] | (p[i + 2] << 8);
        const unsigned char *val = p + i + 3;

        if (i + 3 + tlen > len) {
            printf("  TLV 0x%02x truncated (len %u, remaining %u)\n", type,
                   tlen, len - i - 3);
            return;
        }

        switch (type) {
        case 0x02: /* result: uint16 result, uint16 error */
            if (tlen >= 4) {
                unsigned int result = val[0] | (val[1] << 8);
                unsigned int error = val[2] | (val[3] << 8);
                printf("  TLV 0x02 result=%u error=%u%s\n", result, error,
                       result == 0 ? "" : "   <-- REQUEST FAILED");
            }
            break;
        case 0x10: /* card status: card_state is the first byte */
            if (tlen >= 1) {
                static const char *states[] = {"ABSENT", "PRESENT", "ERROR",
                                               "UNKNOWN"};
                unsigned int s = val[0];
                printf("  TLV 0x10 card_state=%u (%s)\n", s,
                       s < 4 ? states[s] : "?");
            }
            break;
        default:
            printf("  TLV 0x%02x len=%u\n", type, tlen);
            break;
        }

        i += 3 + tlen;
    }
}

int main(void)
{
    void *h_svc, *h_cci;
    qmi_idl_service_object_type *svc_obj;
    qmi_idl_service_object_type (*get_svc_obj)(void);
    int (*client_init_instance)(qmi_idl_service_object_type, unsigned int,
                                void *, void *, void *, unsigned int,
                                qmi_client_type *);
    int (*send_raw)(qmi_client_type, unsigned int, void *, unsigned int, void *,
                    unsigned int, unsigned int *, unsigned int);
    int (*release)(qmi_client_type);
    void (*xport_register)(unsigned int);

    qmi_client_type client = NULL;
    unsigned char resp[1024];
    unsigned int resp_len = 0;
    int rc;

    h_svc = dlopen("/vendor/lib64/libqmiservices.so", RTLD_NOW);
    if (!h_svc) {
        printf("dlopen libqmiservices failed: %s\n", dlerror());
        return 1;
    }
    get_svc_obj = dlsym(h_svc, "uim_get_service_object_internal_v01");
    if (!get_svc_obj) {
        printf("uim_get_service_object_internal_v01 not found\n");
        return 1;
    }
    /* The exported data symbol *is* the service object; qmi_client_* take its
     * address.  (The _internal_ getter needs an IDL version argument, so it is
     * not used as the primary path.) */
    svc_obj = (qmi_idl_service_object_type *)dlsym(
            h_svc, "uim_qmi_idl_service_object_v01");
    if (!svc_obj) {
        qmi_idl_service_object_type obj = get_svc_obj();
        printf("fallback getter -> %p\n", obj);
        if (obj == NULL) {
            printf("no uim service object\n");
            return 1;
        }
        svc_obj = &obj;
    }

    h_cci = dlopen("/vendor/lib64/libqmi_cci.so", RTLD_NOW);
    if (!h_cci) {
        printf("dlopen libqmi_cci failed: %s\n", dlerror());
        return 1;
    }
    client_init_instance = dlsym(h_cci, "qmi_client_init_instance");
    send_raw = dlsym(h_cci, "qmi_client_send_raw_msg_sync");
    release = dlsym(h_cci, "qmi_client_release");
    xport_register = dlsym(h_cci, "qmi_cci_qmux_xport_register");
    if (!client_init_instance || !send_raw || !release) {
        printf("qmi_client_* symbols missing\n");
        return 1;
    }

    printf("uim service object: %p\n", (void *)svc_obj);

    /* Register the qmux transport, as the stock QMI clients do (netmgrd
     * registers it once per transport id it uses). */
    if (xport_register != NULL) {
        xport_register(0x80);
        printf("qmi_cci_qmux_xport_register(0x80) done\n");
    }

    /* 0xFFFF == QMI_CLIENT_INSTANCE_ANY, 10 s timeout, same as netmgrd */
    rc = client_init_instance((qmi_idl_service_object_type)svc_obj, 0xFFFF, NULL, NULL, NULL, 10000,
                              &client);
    printf("qmi_client_init_instance: rc=%d client=%p\n", rc, client);
    if (rc != 0 || client == NULL)
        return 1;

    memset(resp, 0, sizeof(resp));
    rc = send_raw(client, UIM_GET_CARD_STATUS_REQ, NULL, 0, resp, sizeof(resp),
                  &resp_len, 5000);
    printf("qmi_client_send_raw_msg_sync(GET_CARD_STATUS 0x%04x): rc=%d len=%u\n",
           UIM_GET_CARD_STATUS_REQ, rc, resp_len);

    if (rc == 0 && resp_len > 0) {
        dump("response", resp, resp_len);
        parse_tlvs(resp, resp_len);
    }

    release(client);
    return 0;
}
