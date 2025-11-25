#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "zlib.h"          // <-- NEW: zlib for decompression

#include "ui_img_manager.h"

static const char *TAG = "UIIMG";

#ifndef ASSETS_SUBDIR
#define ASSETS_SUBDIR "assets_fairy_tale"
#endif

/**
 * Map logical path "S:..." to real SPIFFS path.
 *
 * Examples:
 *   "S:assets/ui_img_a_png.bin"
 *       -> "/spiffs/assets_fairy_tale/ui_img_a_png.bin"
 *
 *   "S:assets/ui_img_arrow_png.bin"
 *       -> "/spiffs/assets/ui_img_arrow_png.bin"  (special case)
 */
static void map_path(char *dst, size_t dst_sz, const char *src)
{
    if (src && src[0] == 'S' && src[1] == ':') {
        const char *p = src + 2;      // skip "S:"
        if (*p == '/') p++;           // S:/assets/... -> assets/...

        const char *assets = "assets/";
        const size_t assets_len = 7;

        // Special case: arrow icon lives in /spiffs/assets/ directly
        if (strstr(p, "ui_img_arrow_png.bin")) {
            snprintf(dst, dst_sz, "/spiffs/%s", p);
            return;
        }

        // For "assets/xxx" put files into /spiffs/ASSETS_SUBDIR/xxx
        if (strncmp(p, assets, assets_len) == 0) {
            const char *rest = p + assets_len;  // after "assets/"
            snprintf(dst, dst_sz, "/spiffs/%s/%s", ASSETS_SUBDIR, rest);
        } else {
            // All other "S:xxx" paths go under /spiffs/
            snprintf(dst, dst_sz, "/spiffs/%s", p);
        }
    } else {
        // Non-prefixed paths are used as-is
        snprintf(dst, dst_sz, "%s", src ? src : "");
    }
}

/**
 * Load image data from SPIFFS into PSRAM.
 *
 * Behavior:
 *   - If file size on flash == `size` (expected uncompressed size):
 *       treat file as RAW image and just read `size` bytes.
 *   - If file size != `size`:
 *       treat file as COMPRESSED zlib data; read the whole file and
 *       decompress into a PSRAM buffer of `size` bytes.
 *
 * Returns pointer to PSRAM buffer with uncompressed image or NULL on error.
 * Caller is responsible for freeing the buffer with heap_caps_free().
 */
uint8_t* _ui_load_binary_direct(const char* fname_S, uint32_t size)
{
    if (!fname_S || size == 0) {
        ESP_LOGE(TAG, "bad args: fname_S=%p size=%u",
                 (void*)fname_S, (unsigned)size);
        return NULL;
    }

    char real[256];
    map_path(real, sizeof(real), fname_S);

    ESP_LOGI(TAG, "load image: %s (uncompressed=%u bytes)",
             real, (unsigned)size);

    FILE *fp = fopen(real, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "fopen failed: %s", real);
        return NULL;
    }

    // Determine file size on flash
    if (fseek(fp, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "fseek(SEEK_END) failed: %s", real);
        fclose(fp);
        return NULL;
    }

    long file_size_long = ftell(fp);
    if (file_size_long <= 0) {
        ESP_LOGE(TAG, "ftell failed or empty file: %s", real);
        fclose(fp);
        return NULL;
    }

    size_t file_size = (size_t)file_size_long;
    rewind(fp);

    // Case 1: RAW image (file size matches expected uncompressed size)
    if (file_size == size) {
        ESP_LOGD(TAG, "image is RAW (%u bytes), reading directly", (unsigned)file_size);

        uint8_t *buf = (uint8_t*)heap_caps_malloc(size,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) {
            ESP_LOGE(TAG, "heap_caps_malloc(%u) failed", (unsigned)size);
            fclose(fp);
            return NULL;
        }

        size_t rd = fread(buf, 1, size, fp);
        fclose(fp);

        if (rd != size) {
            ESP_LOGE(TAG, "fread short: %u/%u", (unsigned)rd, (unsigned)size);
            heap_caps_free(buf);
            return NULL;
        }

        return buf;
    }

    // Case 2: COMPRESSED image (zlib)
    ESP_LOGD(TAG,
             "image assumed COMPRESSED (file=%u bytes, uncompressed=%u)",
             (unsigned)file_size, (unsigned)size);

    // Temporary buffer for compressed data (normal 8-bit RAM is enough)
    uint8_t *comp_buf = (uint8_t*)heap_caps_malloc(file_size, MALLOC_CAP_8BIT);
    if (!comp_buf) {
        ESP_LOGE(TAG, "heap_caps_malloc(comp_buf, %u) failed", (unsigned)file_size);
        fclose(fp);
        return NULL;
    }

    size_t rd = fread(comp_buf, 1, file_size, fp);
    fclose(fp);

    if (rd != file_size) {
        ESP_LOGE(TAG, "fread short (compressed): %u/%u",
                 (unsigned)rd, (unsigned)file_size);
        heap_caps_free(comp_buf);
        return NULL;
    }

    // Output buffer in PSRAM for UNCOMPRESSED image
    uint8_t *out_buf = (uint8_t*)heap_caps_malloc(size,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out_buf) {
        ESP_LOGE(TAG, "heap_caps_malloc(out_buf, %u) failed", (unsigned)size);
        heap_caps_free(comp_buf);
        return NULL;
    }

    uLongf dst_len = size;
    int zret = uncompress(out_buf, &dst_len, comp_buf, file_size);

    heap_caps_free(comp_buf);

    if (zret != Z_OK) {
        ESP_LOGE(TAG, "uncompress() failed: zret=%d", zret);
        heap_caps_free(out_buf);
        return NULL;
    }

    if (dst_len != size) {
        ESP_LOGE(TAG, "uncompress() size mismatch: got=%lu expected=%u",
                 (unsigned long)dst_len, (unsigned)size);
        heap_caps_free(out_buf);
        return NULL;
    }

    ESP_LOGD(TAG, "image decompressed OK: %s", real);
    return out_buf;
}
