#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "fb.h"

int user_set_buffers_num = -1;

int get_device_fb(const char* path, struct fb *fb)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    unsigned char *raw;
    unsigned int bytespp;
    unsigned int raw_size;
    unsigned int raw_line_length;
    ssize_t read_size;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    if(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fd);
        return -1;
    }

    bytespp = vinfo.bits_per_pixel / 8;
    raw_line_length = finfo.line_length; // (xres + padding_offset) * bytespp
    raw_size = vinfo.yres * raw_line_length;

    // output data handler struct
    fb->bpp = vinfo.bits_per_pixel;
    fb->size = vinfo.xres * vinfo.yres * bytespp;
    fb->width = vinfo.xres;
    fb->height = vinfo.yres;
    fb->red_offset = vinfo.red.offset;
    fb->red_length = vinfo.red.length;
    fb->green_offset = vinfo.green.offset;
    fb->green_length = vinfo.green.length;
    fb->blue_offset = vinfo.blue.offset;
    fb->blue_length = vinfo.blue.length;
    fb->alpha_offset = vinfo.transp.offset;
    fb->alpha_length = vinfo.transp.length;

    // container for raw bits from the active frame buffer
    raw = malloc(raw_size);
    if (!raw) {
        close(fd);
        return -1;
    }

    // capture active buffer: n is 0 for first buffer, 1 for second
    // graphics.c -> set_active_framebuffer() -> vi.yoffset = n * vi.yres;
    unsigned int active_buffer_offset = 0;
    int num_buffers = user_set_buffers_num;
    if (num_buffers < 0) {
        // default: auto detect
        num_buffers = (int)(vinfo.yoffset / vinfo.yres);
        if (num_buffers > MAX_ALLOWED_FB_BUFFERS)
            num_buffers = 0;
    }

    if (finfo.smem_len >= (raw_size * (num_buffers + 1))) {
        active_buffer_offset = raw_size * num_buffers;
    }

    // copy the active frame buffer bits into the raw container
    lseek(fd, active_buffer_offset, SEEK_SET);
    read_size = read(fd, raw, raw_size);
    if (read_size < 0 || (unsigned)read_size != raw_size) {
        goto oops;
    }

/*
    Image padding (needed on some RGBX_8888 formats, maybe others?)
    we have padding_offset in bytes and bytespp = bits_per_pixel / 8
    raw_line_length = (width + padding_offset) * bytespp

    This gives: padding_offset = (raw_line_length / bytespp) - width
*/
    unsigned int padding_offset = (raw_line_length / bytespp) - fb->width;
    if (padding_offset) {
        unsigned char *data;
        unsigned char *pdata;
        unsigned char *praw;
        const unsigned char *data_buffer_end;
        const unsigned char *raw_buffer_end;

        // container for final aligned image data
        data = malloc(fb->size);
        if (!data) {
            goto oops;
        }

        pdata = data;
        praw = raw;
        data_buffer_end = data + fb->size;
        raw_buffer_end = raw + raw_size;

        // Add a margin to prevent buffer overflow during copy
        data_buffer_end -= bytespp * fb->width;
        raw_buffer_end -= raw_line_length;
        while (praw < raw_buffer_end && pdata < data_buffer_end) {
            memcpy(pdata, praw, bytespp * fb->width);
            pdata += bytespp * fb->width;
            praw += raw_line_length;
        }

        fb->data = data;
        free(raw);
    } else {
        fb->data = raw;
    }

    close(fd);
    return 0;

oops:
    free(raw);
    close(fd);
    return -1;
}

static int fb_get_format(const struct fb *fb)
{
    int ao = fb->alpha_offset;
    int ro = fb->red_offset;
    int go = fb->green_offset;
    int bo = fb->blue_offset;

#define FB_FORMAT_UNKNOWN   0
#define FB_FORMAT_RGB565    1
#define FB_FORMAT_ARGB8888  2
#define FB_FORMAT_RGBA8888  3
#define FB_FORMAT_ABGR8888  4
#define FB_FORMAT_BGRA8888  5
#define FB_FORMAT_RGBX8888  FB_FORMAT_RGBA8888

    /* TODO: use offset */
    if (fb->bpp == 16)
        return FB_FORMAT_RGB565;

    /* TODO: validate */
    if (ao == 0 && ro == 8)
        return FB_FORMAT_ARGB8888;

    if (ao == 0 && ro == 24 && go == 16 && bo == 8)
        return FB_FORMAT_RGBX8888;

    if (ao == 0 && bo == 8)
        return FB_FORMAT_ABGR8888;

    if (ro == 0)
        return FB_FORMAT_RGBA8888;

    if (bo == 0)
        return FB_FORMAT_BGRA8888;

    /* fallback */
    return FB_FORMAT_UNKNOWN;
}

int rgb565_to_rgb888(const char* src, char* dst, size_t pixel)
{
    struct rgb565  *from;
    struct rgb888  *to;

    from = (struct rgb565 *) src;
    to = (struct rgb888 *) dst;

    size_t i = 0;
    /* traverse pixel of the row */
    while(i++ < pixel) {

        to->r = from->r;
        to->g = from->g;
        to->b = from->b;
        /* scale */
        to->r <<= 3;
        to->g <<= 2;
        to->b <<= 3;

        to++;
        from++;
    }

    return 0;
}

int argb8888_to_rgb888(const char* src, char* dst, size_t pixel)
{
    size_t i;
    struct argb8888  *from;
    struct rgb888  *to;

    from = (struct argb8888 *) src;
    to = (struct rgb888 *) dst;

    i = 0;
    /* traverse pixel of the row */
    while(i++ < pixel) {

        to->r = from->r;
        to->g = from->g;
        to->b = from->b;

        to++;
        from++;
    }

    return 0;
}

int abgr8888_to_rgb888(const char* src, char* dst, size_t pixel)
{
    size_t i;
    struct abgr8888  *from;
    struct rgb888  *to;

    from = (struct abgr8888 *) src;
    to = (struct rgb888 *) dst;

    i = 0;
    /* traverse pixel of the row */
    while(i++ < pixel) {

        to->r = from->r;
        to->g = from->g;
        to->b = from->b;

        to++;
        from++;
    }

    return 0;
}

int bgra8888_to_rgb888(const char* src, char* dst, size_t pixel)
{
    size_t i;
    struct bgra8888  *from;
    struct rgb888  *to;

    from = (struct bgra8888 *) src;
    to = (struct rgb888 *) dst;

    i = 0;
    /* traverse pixel of the row */
    while(i++ < pixel) {

        to->r = from->r;
        to->g = from->g;
        to->b = from->b;

        to++;
        from++;
    }

    return 0;
}

int rgba8888_to_rgb888(const char* src, char* dst, size_t pixel)
{
    size_t i;
    struct rgba8888  *from;
    struct rgb888  *to;

    from = (struct rgba8888 *) src;
    to = (struct rgb888 *) dst;

    i = 0;
    /* traverse pixel of the row */
    while (i++ < pixel) {
        to->r = from->r;
        to->g = from->g;
        to->b = from->b;

        to++;
        from++;
    }

    return 0;
}

int fb_read(const struct fb *fb, char *rgb_matrix)
{
    int ret = -1;

    /* Allocate RGB Matrix. */
    rgb_matrix = malloc(fb->width * fb->height * 3);
    if(!rgb_matrix) {
        return -1;
    }

    int fmt = fb_get_format(fb);

    switch(fmt) {
        case FB_FORMAT_RGB565:
            /* emulator use rgb565 */
            ret = rgb565_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_ARGB8888:
            /* most devices use argb8888 */
            ret = argb8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_ABGR8888:
            ret = abgr8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_BGRA8888:
            ret = bgra8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_RGBA8888:
            ret = rgba8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        default:
            break;
    }

    free(fb->data);
    return ret;
}
