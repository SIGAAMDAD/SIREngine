#ifndef SIR_TEST_XDG_SHELL_CLIENT_PROTOCOL_H
#define SIR_TEST_XDG_SHELL_CLIENT_PROTOCOL_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
extern const struct wl_interface xdg_wm_base_interface;
struct xdg_wm_base_listener
{
	void ( *ping )( void *, struct xdg_wm_base *, uint32_t );
};
struct xdg_surface_listener
{
	void ( *configure )( void *, struct xdg_surface *, uint32_t );
};
struct xdg_toplevel_listener
{
	void ( *configure )( void *, struct xdg_toplevel *, int32_t, int32_t, struct wl_array * );
	void ( *close )( void *, struct xdg_toplevel * );
	void ( *configure_bounds )( void *, struct xdg_toplevel *, int32_t, int32_t );
	void ( *wm_capabilities )( void *, struct xdg_toplevel *, struct wl_array * );
};
int xdg_wm_base_add_listener( struct xdg_wm_base *, const struct xdg_wm_base_listener *, void * );
void xdg_wm_base_pong( struct xdg_wm_base *, uint32_t );
struct xdg_surface *xdg_wm_base_get_xdg_surface( struct xdg_wm_base *, struct wl_surface * );
void xdg_wm_base_destroy( struct xdg_wm_base * );
int xdg_surface_add_listener( struct xdg_surface *, const struct xdg_surface_listener *, void * );
struct xdg_toplevel *xdg_surface_get_toplevel( struct xdg_surface * );
void xdg_surface_ack_configure( struct xdg_surface *, uint32_t );
void xdg_surface_destroy( struct xdg_surface * );
int xdg_toplevel_add_listener( struct xdg_toplevel *, const struct xdg_toplevel_listener *, void * );
void xdg_toplevel_set_title( struct xdg_toplevel *, const char * );
void xdg_toplevel_set_minimized( struct xdg_toplevel * );
void xdg_toplevel_set_maximized( struct xdg_toplevel * );
void xdg_toplevel_set_fullscreen( struct xdg_toplevel *, struct wl_output * );
void xdg_toplevel_unset_fullscreen( struct xdg_toplevel * );
void xdg_toplevel_destroy( struct xdg_toplevel * );
#ifdef __cplusplus
}
#endif
#endif
