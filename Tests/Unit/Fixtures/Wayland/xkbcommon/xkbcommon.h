#ifndef SIR_TEST_XKBCOMMON_H
#define SIR_TEST_XKBCOMMON_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
#define XKB_CONTEXT_NO_FLAGS 0
#define XKB_KEYMAP_FORMAT_TEXT_V1 1
#define XKB_KEYMAP_COMPILE_NO_FLAGS 0
struct xkb_context *xkb_context_new( int nFlags );
void xkb_context_unref( struct xkb_context * );
struct xkb_keymap *xkb_keymap_new_from_string( struct xkb_context *, const char *, int, int );
void xkb_keymap_unref( struct xkb_keymap * );
struct xkb_state *xkb_state_new( struct xkb_keymap * );
void xkb_state_unref( struct xkb_state * );
uint32_t xkb_state_key_get_utf32( struct xkb_state *, uint32_t nKey );
uint32_t xkb_state_update_mask( struct xkb_state *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t );
#ifdef __cplusplus
}
#endif
#endif
