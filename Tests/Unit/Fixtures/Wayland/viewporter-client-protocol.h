#ifndef SIR_TEST_VIEWPORTER_H
#define SIR_TEST_VIEWPORTER_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct wp_viewporter;
struct wp_viewport;
extern const struct wl_interface wp_viewporter_interface;
struct wp_viewport *wp_viewporter_get_viewport( struct wp_viewporter *, struct wl_surface * );
void wp_viewporter_destroy( struct wp_viewporter * );
void wp_viewport_set_destination( struct wp_viewport *, int32_t, int32_t );
void wp_viewport_destroy( struct wp_viewport * );
#ifdef __cplusplus
}
#endif
#endif
