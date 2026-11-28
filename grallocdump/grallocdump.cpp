/* debug helper: allocate a GraphicBuffer the way the camera app does and dump
 * the raw native_handle ints, to compare against private_handle_t layouts. */
#include <ui/GraphicBuffer.h>
#include <system/graphics.h>
#include <stdio.h>
#include <stdlib.h>

using namespace android;

int main(int argc, char **argv) {
    uint32_t w = (argc > 1) ? atoi(argv[1]) : 960;
    uint32_t h = (argc > 2) ? atoi(argv[2]) : 720;
    uint32_t fmt = (argc > 3) ? atoi(argv[3]) : HAL_PIXEL_FORMAT_YCbCr_420_888;
    uint32_t usage = (argc > 4) ? (uint32_t)strtoul(argv[4], NULL, 0) : 0x900;
    sp<GraphicBuffer> gb = new GraphicBuffer(w, h, fmt, usage, "dump");
    printf("err=%d stride=%d format=%d usage=0x%x\n", gb->initCheck(),
            gb->getStride(), gb->getPixelFormat(), gb->getUsage());
    native_handle_t *hd = const_cast<native_handle_t *>(gb->handle);
    if (hd == NULL) {
        printf("no handle\n");
        return 1;
    }
    printf("version=%d numFds=%d numInts=%d\n", hd->version, hd->numFds, hd->numInts);
    int n = hd->numFds + hd->numInts;
    for (int i = 0; i < n; i++) {
        printf("int[%d]=%d (0x%x)\n", i, hd->data[i], hd->data[i]);
    }
    return 0;
}
