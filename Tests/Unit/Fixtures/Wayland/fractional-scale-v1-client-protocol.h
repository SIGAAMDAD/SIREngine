#ifndef SIR_TEST_FRACTIONAL_SCALE_V1_H
#define SIR_TEST_FRACTIONAL_SCALE_V1_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
extern const struct wl_interface wp_fractional_scale_manager_v1_interface;
struct wp_fractional_scale_v1_listener
{
	void ( *preferred_scale )( void *, struct wp_fractional_scale_v1 *, uint32_t );
};
struct wp_fractional_scale_v1 *wp_fractional_scale_manager_v1_get_fractional_scale(
    struct wp_fractional_scale_manager_v1 *, struct wl_surface * );
void wp_fractional_scale_manager_v1_destroy( struct wp_fractional_scale_manager_v1 * );
int wp_fractional_scale_v1_add_listener( struct wp_fractional_scale_v1 *, const struct wp_fractional_scale_v1_listener *, void * );
void wp_fractional_scale_v1_destroy( struct wp_fractional_scale_v1 * );
#ifdef __cplusplus
}
#endif
#endif
