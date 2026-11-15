#ifndef SIR_TEST_WAYLAND_TEXT_INPUT_V3_H
#define SIR_TEST_WAYLAND_TEXT_INPUT_V3_H
#include "wayland-client.h"
#ifdef __cplusplus
extern "C" {
#endif
struct zwp_text_input_manager_v3;
struct zwp_text_input_v3;
extern const struct wl_interface zwp_text_input_manager_v3_interface;
struct zwp_text_input_v3_listener
{
    void (*enter)(void *, struct zwp_text_input_v3 *, struct wl_surface *);
    void (*leave)(void *, struct zwp_text_input_v3 *, struct wl_surface *);
    void (*preedit_string)(void *, struct zwp_text_input_v3 *, const char *, int32_t, int32_t);
    void (*commit_string)(void *, struct zwp_text_input_v3 *, const char *);
    void (*delete_surrounding_text)(void *, struct zwp_text_input_v3 *, uint32_t, uint32_t);
    void (*done)(void *, struct zwp_text_input_v3 *, uint32_t);
};
struct zwp_text_input_v3 *zwp_text_input_manager_v3_get_text_input(struct zwp_text_input_manager_v3 *, struct wl_seat *);
int zwp_text_input_v3_add_listener(struct zwp_text_input_v3 *, const struct zwp_text_input_v3_listener *, void *);
void zwp_text_input_v3_enable(struct zwp_text_input_v3 *);
void zwp_text_input_v3_disable(struct zwp_text_input_v3 *);
void zwp_text_input_v3_commit(struct zwp_text_input_v3 *);
void zwp_text_input_v3_destroy(struct zwp_text_input_v3 *);
void zwp_text_input_manager_v3_destroy(struct zwp_text_input_manager_v3 *);
#ifdef __cplusplus
}
#endif
#endif
