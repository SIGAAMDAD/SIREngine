#ifndef SIR_TEST_RELATIVE_POINTER_PROTOCOL_H
#define SIR_TEST_RELATIVE_POINTER_PROTOCOL_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct zwp_relative_pointer_manager_v1;
struct zwp_relative_pointer_v1;
extern const struct wl_interface zwp_relative_pointer_manager_v1_interface;
struct zwp_relative_pointer_v1_listener
{
	void ( *relative_motion )( void *, struct zwp_relative_pointer_v1 *, uint32_t, uint32_t,
	                          wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t );
};
struct zwp_relative_pointer_v1 *zwp_relative_pointer_manager_v1_get_relative_pointer(
    struct zwp_relative_pointer_manager_v1 *, struct wl_pointer * );
int zwp_relative_pointer_v1_add_listener( struct zwp_relative_pointer_v1 *,
    const struct zwp_relative_pointer_v1_listener *, void * );
void zwp_relative_pointer_v1_destroy( struct zwp_relative_pointer_v1 * );
void zwp_relative_pointer_manager_v1_destroy( struct zwp_relative_pointer_manager_v1 * );
#ifdef __cplusplus
}
#endif
#endif
