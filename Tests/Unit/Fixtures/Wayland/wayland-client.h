#ifndef SIR_TEST_WAYLAND_CLIENT_H
#define SIR_TEST_WAYLAND_CLIENT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_proxy;
struct wl_seat;
struct wl_output;
struct wl_keyboard;
struct wl_pointer;
struct wl_array
{
	size_t size;
	size_t alloc;
	void *data;
};
typedef int32_t wl_fixed_t;
struct wl_interface
{
	const char *name;
};
extern const struct wl_interface wl_compositor_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_output_interface;
#define WL_SEAT_CAPABILITY_POINTER 1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEY_STATE_PRESSED 1
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 1
#define WL_POINTER_BUTTON_STATE_PRESSED 1
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1
struct wl_surface_listener
{
	void ( *enter )( void *, struct wl_surface *, struct wl_output * );
	void ( *leave )( void *, struct wl_surface *, struct wl_output * );
};
struct wl_output_listener
{
	void ( *geometry )( void *, struct wl_output *, int32_t, int32_t, int32_t, int32_t, int32_t, const char *, const char *, int32_t );
	void ( *mode )( void *, struct wl_output *, uint32_t, int32_t, int32_t, int32_t );
	void ( *done )( void *, struct wl_output * );
	void ( *scale )( void *, struct wl_output *, int32_t );
};
struct wl_seat_listener
{
	void ( *capabilities )( void *, struct wl_seat *, uint32_t );
	void ( *name )( void *, struct wl_seat *, const char * );
};
struct wl_keyboard_listener
{
	void ( *keymap )( void *, struct wl_keyboard *, uint32_t, int32_t, uint32_t );
	void ( *enter )( void *, struct wl_keyboard *, uint32_t, struct wl_surface *, struct wl_array * );
	void ( *leave )( void *, struct wl_keyboard *, uint32_t, struct wl_surface * );
	void ( *key )( void *, struct wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t );
	void ( *modifiers )( void *, struct wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t );
	void ( *repeat_info )( void *, struct wl_keyboard *, int32_t, int32_t );
};
struct wl_pointer_listener
{
	void ( *enter )( void *, struct wl_pointer *, uint32_t, struct wl_surface *, wl_fixed_t, wl_fixed_t );
	void ( *leave )( void *, struct wl_pointer *, uint32_t, struct wl_surface * );
	void ( *motion )( void *, struct wl_pointer *, uint32_t, wl_fixed_t, wl_fixed_t );
	void ( *button )( void *, struct wl_pointer *, uint32_t, uint32_t, uint32_t, uint32_t );
	void ( *axis )( void *, struct wl_pointer *, uint32_t, uint32_t, wl_fixed_t );
	void ( *frame )( void *, struct wl_pointer * );
	void ( *axis_source )( void *, struct wl_pointer *, uint32_t );
	void ( *axis_stop )( void *, struct wl_pointer *, uint32_t, uint32_t );
	void ( *axis_discrete )( void *, struct wl_pointer *, uint32_t, int32_t );
};
struct wl_registry_listener
{
	void ( *global )( void *, struct wl_registry *, uint32_t, const char *, uint32_t );
	void ( *global_remove )( void *, struct wl_registry *, uint32_t );
};
struct wl_display *wl_display_connect( const char * );
void wl_display_disconnect( struct wl_display * );
struct wl_registry *wl_display_get_registry( struct wl_display * );
int wl_display_roundtrip( struct wl_display * );
int wl_display_prepare_read( struct wl_display * );
void wl_display_cancel_read( struct wl_display * );
int wl_display_dispatch_pending( struct wl_display * );
int wl_display_flush( struct wl_display * );
int wl_display_get_fd( struct wl_display * );
int wl_display_read_events( struct wl_display * );
int wl_registry_add_listener( struct wl_registry *, const struct wl_registry_listener *, void * );
void *wl_registry_bind( struct wl_registry *, uint32_t, const struct wl_interface *, uint32_t );
void wl_registry_destroy( struct wl_registry * );
struct wl_surface *wl_compositor_create_surface( struct wl_compositor * );
void wl_compositor_destroy( struct wl_compositor * );
int wl_output_add_listener( struct wl_output *, const struct wl_output_listener *, void * );
void wl_output_destroy( struct wl_output * );
int wl_surface_add_listener( struct wl_surface *, const struct wl_surface_listener *, void * );
void wl_surface_set_buffer_scale( struct wl_surface *, int32_t );
void wl_surface_commit( struct wl_surface * );
void wl_surface_destroy( struct wl_surface * );
void wl_proxy_destroy( struct wl_proxy * );
uint32_t wl_proxy_get_version( struct wl_proxy * );
void wl_seat_destroy( struct wl_seat * );
void wl_seat_release( struct wl_seat * );
int wl_seat_add_listener( struct wl_seat *, const struct wl_seat_listener *, void * );
struct wl_keyboard *wl_seat_get_keyboard( struct wl_seat * );
struct wl_pointer *wl_seat_get_pointer( struct wl_seat * );
int wl_keyboard_add_listener( struct wl_keyboard *, const struct wl_keyboard_listener *, void * );
int wl_pointer_add_listener( struct wl_pointer *, const struct wl_pointer_listener *, void * );
void wl_keyboard_destroy( struct wl_keyboard * );
void wl_keyboard_release( struct wl_keyboard * );
void wl_pointer_destroy( struct wl_pointer * );
void wl_pointer_release( struct wl_pointer * );
#ifdef __cplusplus
}
#endif
#endif
