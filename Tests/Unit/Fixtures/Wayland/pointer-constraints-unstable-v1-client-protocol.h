#ifndef SIR_TEST_POINTER_CONSTRAINTS_PROTOCOL_H
#define SIR_TEST_POINTER_CONSTRAINTS_PROTOCOL_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct zwp_pointer_constraints_v1;
struct zwp_locked_pointer_v1;
extern const struct wl_interface zwp_pointer_constraints_v1_interface;
#define ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT 2
struct zwp_locked_pointer_v1_listener
{
	void ( *locked )( void *, struct zwp_locked_pointer_v1 * );
	void ( *unlocked )( void *, struct zwp_locked_pointer_v1 * );
};
struct zwp_locked_pointer_v1 *zwp_pointer_constraints_v1_lock_pointer( struct zwp_pointer_constraints_v1 *,
    struct wl_surface *, struct wl_pointer *, void *, uint32_t );
int zwp_locked_pointer_v1_add_listener( struct zwp_locked_pointer_v1 *,
    const struct zwp_locked_pointer_v1_listener *, void * );
void zwp_locked_pointer_v1_destroy( struct zwp_locked_pointer_v1 * );
void zwp_pointer_constraints_v1_destroy( struct zwp_pointer_constraints_v1 * );
#ifdef __cplusplus
}
#endif
#endif
