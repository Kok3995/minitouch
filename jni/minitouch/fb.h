struct fb {
    unsigned int bpp;
    unsigned int size;
    unsigned int width;
    unsigned int height;
    unsigned int red_offset;
    unsigned int red_length;
    unsigned int blue_offset;
    unsigned int blue_length;
    unsigned int green_offset;
    unsigned int green_length;
    unsigned int alpha_offset;
    unsigned int alpha_length;
    void* data;
};

typedef struct rgb888 {
        char r;
        char g;
        char b;
} rgb888_t;

typedef rgb888_t rgb24_t;

typedef struct argb8888 {
        char a;
        char r;
        char g;
        char b;
} argb8888_t;

typedef struct abgr8888 {
        char a;
        char b;
        char g;
        char r;
} abgr8888_t;

typedef struct bgra8888 {
        char b;
        char g;
        char r;
        char a;
} bgra8888_t;

typedef struct rgba8888 {
        char r;
        char g;
        char b;
        char a;
} rgba8888_t;

typedef struct rgb565 {
        short b:5;
        short g:6;
        short r:5;
} rgb565_t;

#define MAX_ALLOWED_FB_BUFFERS 3

int get_device_fb(const char* path, struct fb *fb);
int fb_read(const struct fb *fb, char *rgb_matrix);