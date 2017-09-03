// Copyright 2017 jem@seethis.link
// Licensed under the MIT license (http://opensource.org/licenses/MIT)

#include "key_handlers/key_hold.h"

#include <string.h>

#include "core/keyboard_report.h"
#include "core/keycode.h"
#include "core/matrix_interpret.h"
#include "core/timer.h"
#include "core/util.h"

// external keycode table structure:
// offset 0: delay
// offset 2: settings
// offset 4: held_keycode
// offset 6: tap_keycode
#define EKC_OFFSET_DELAY 0
#define EKC_OFFSET_SETTINGS 2
#define EKC_OFFSET_HELD_KEYCODE 4
#define EKC_OFFSET_TAP_KEYCODE 6

#define HOLD_KEY_TAP_AUTO_RELEASE_TIME 3

XRAM hold_event_t hold_event_list[MAX_NUM_HOLD_KEYS];
uint8_t hold_event_list_len;

void handle_hold_keycode(keycode_t keycode, key_event_t event) REENT;

void hold_key_delete_event(uint8_t i) {
    if (i >= hold_event_list_len) {
        return;
    }

    if (i != hold_event_list_len-1) {
        memcpy(
            &hold_event_list[i],
            &hold_event_list[hold_event_list_len-1],
            sizeof(hold_event_t)
            );
    }

    hold_event_list_len--;
}

#include "core/usb_commands.h"

void hold_key_task(void) {
    uint8_t i;
    uint16_t current_time;

    if (hold_event_list_len == 0) {
        return;
    }

    current_time = timer_read16_ms();

    // Check hold key events
    for (i = 0; i < hold_event_list_len; ++i) {
        hold_event_t *hold = &hold_event_list[i];
        if (hold->has_generated_event) {
            continue;
        }

        if (has_passed_time16(current_time, hold->end_time)) {
            keycode_t keycode;
            if (hold->has_been_tapped) {
                // this code is used to release tapped keys
                get_ekc_data(
                    (uint8_t*)&keycode,
                    hold->ekc_addr + EKC_OFFSET_TAP_KEYCODE,
                    sizeof(keycode_t)
                );
                queue_keycode_event(keycode, EVENT_RELEASED, hold->kb_id);
                // need to delete tap event now that it is finished.
                hold_key_delete_event(i);
                // since we deleted an event from the list we are processing and
                // the new event is now at the current list position, need to
                // decrement here.
                i--;
            } else if (!hold->has_been_held) {
                // this code is used to press held keys
                get_ekc_data(
                    (uint8_t*)&keycode,
                    hold->ekc_addr + EKC_OFFSET_HELD_KEYCODE,
                    sizeof(keycode_t)
                );
                hold->has_been_held = true;
                hold->has_generated_event = true;
                queue_keycode_event(keycode, EVENT_PRESSED, hold->kb_id);
            }
        }
    }
}

// The keycode should be the unique address of the key in the external keycode
// data table.

bit_t is_hold_keycode(keycode_t keycode) {
    return ( keycode == KC_HOLD_KEY );
}

#include "core/rf.h"

void handle_hold_keycode(keycode_t keycode, key_event_t event) REENT {
    if (event == EVENT_RESET) {
        hold_event_list_len = 0;
        hold_keycodes.is_timer_task_active = false;
        return;
    }
    // } else if (event == EVENT_TIMER_TASK) {
    //     hold_key_task();
    // }


    { // handle press and release events
        uint16_t this_ekc_addr = EKC_DATA_ADDR(keycode);
        uint8_t kb_id = get_active_keyboard_id();
        if (event == EVENT_PRESSED) {
            uint16_t delay;
            uint16_t settings;
            if (hold_event_list_len >= MAX_NUM_HOLD_KEYS) {
                return;
            }


            get_ekc_data((uint8_t*)&delay, this_ekc_addr+EKC_OFFSET_DELAY, sizeof(delay));
            get_ekc_data((uint8_t*)&settings, this_ekc_addr+EKC_OFFSET_SETTINGS, sizeof(settings));

            hold_event_list[hold_event_list_len].ekc_addr = this_ekc_addr;
            hold_event_list[hold_event_list_len].end_time = timer_read16_ms() + delay;
            hold_event_list[hold_event_list_len].kb_id = kb_id;
            hold_event_list[hold_event_list_len].activate_on_key_press =
                (uint8_t)settings & HOLD_KEY_SETTING_ACTIVATE_ON_KEY_PRESS;
            hold_event_list[hold_event_list_len].has_been_held = false;
            hold_event_list[hold_event_list_len].has_been_tapped = false;
            hold_event_list[hold_event_list_len].has_generated_event = false;

            hold_event_list_len++;
            hold_keycodes.is_timer_task_active = true;
        } else if (event == EVENT_RELEASED) {
            uint8_t i;
            for (i = 0; i < hold_event_list_len; ++i) {
                if (hold_event_list[i].ekc_addr == this_ekc_addr &&
                    hold_event_list[i].kb_id == kb_id ) {
                    keycode_t keycode;

                    if (hold_event_list[i].has_been_held) {
                        get_ekc_data(
                            (uint8_t*)&keycode,
                            this_ekc_addr + EKC_OFFSET_HELD_KEYCODE,
                            sizeof(keycode_t)
                        );
                        // held key
                        queue_keycode_event(keycode, EVENT_RELEASED, kb_id);
                        hold_key_delete_event(i);
                        i--; // account for deleted element from this list
                    } else {
                        // tapped key
                        hold_event_list[i].has_been_tapped = true;
                        hold_event_list[i].end_time = timer_read16_ms() + HOLD_KEY_TAP_AUTO_RELEASE_TIME;
                        get_ekc_data(
                            (uint8_t*)&keycode,
                            this_ekc_addr + EKC_OFFSET_TAP_KEYCODE,
                            sizeof(keycode_t)
                        );

                        // NOTE: other fields for this event have were already
                        // filled in when the press event was handled.
                        queue_keycode_event(keycode, EVENT_PRESSED, kb_id);
                    }

                }
            }

            if (hold_event_list_len == 0) {
                hold_keycodes.is_timer_task_active = false;
            }
        }
    }
}

XRAM keycode_callbacks_t hold_keycodes = {
    .checker = is_hold_keycode,
    .handler = handle_hold_keycode,
    .active_when_disabled = 1,
    .preserves_sticky_keys = 1,
};
