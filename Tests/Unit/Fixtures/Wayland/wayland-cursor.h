#ifndef SIR_TEST_WAYLAND_CURSOR_H
#define SIR_TEST_WAYLAND_CURSOR_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct wl_cursor_theme;
struct wl_cursor_image
{
    uint32_t width;
    uint32_t height;
    uint32_t hotspot_x;
    uint32_t hotspot_y;
};
struct wl_cursor
{
    uint32_t image_count;
    struct wl_cursor_image **images;
};
struct wl_cursor_theme *wl_cursor_theme_load(const char *, int32_t, struct wl_display *);
struct wl_cursor *wl_cursor_theme_get_cursor(struct wl_cursor_theme *, const char *);
void wl_cursor_theme_destroy(struct wl_cursor_theme *);
struct wl_buffer *wl_cursor_image_get_buffer(struct wl_cursor_image *);
#ifdef __cplusplus
}
#endif
#endif
