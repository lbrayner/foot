#pragma once

#include <stdint.h>
#include <wayland-client.h>

#include "wayland.h"

extern const struct wl_keyboard_listener keyboard_listener;
extern const struct wl_pointer_listener pointer_listener;

void input_repeat(struct seat *seat, uint32_t key);

bool input_parse_key_binding(struct xkb_keymap *keymap, const char *combos,
                             key_binding_list_t *bindings);
bool input_parse_mouse_binding(struct xkb_keymap *keymap, const char *combos,
                               enum bind_action_normal action,
                               mouse_binding_list_t *bindings);
