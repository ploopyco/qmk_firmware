/* Copyright 2023 Colin Lam (Ploopy Corporation)
 * Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H

#include "rgblight.h"
#include "printf.h"

#include "../../../common/tmag5273wheel.h"
#include "pointing_device_gestures.h"

/* State variables for wheels. */
uint16_t leftwheel_deadzone_center = 0;
uint16_t rightwheel_deadzone_center = 0;

uint16_t leftwheel_current_position = 0;
uint16_t rightwheel_current_position = 0;
uint32_t last_scroll_time = 0;

/* Layer lighting, used to blink when making gestures. */
const rgblight_segment_t PROGMEM righty_nav_layer_colour[] =        RGBLIGHT_LAYER_SEGMENTS( {0, 2, HSV_NAVBLUE} );
const rgblight_segment_t PROGMEM lefty_nav_layer_colour[] =         RGBLIGHT_LAYER_SEGMENTS( {0, 2, HSV_NAVGREEN} );
const rgblight_segment_t PROGMEM control_layer_colour[] =           RGBLIGHT_LAYER_SEGMENTS( {0, 2, HSV_RED} );
const rgblight_segment_t PROGMEM gesture_layer_colour[] =           RGBLIGHT_LAYER_SEGMENTS( {0, 2, HSV_GESTUREYELLOW} );
const rgblight_segment_t PROGMEM option_changed_layer_colour[] =    RGBLIGHT_LAYER_SEGMENTS( {0, 2, HSV_OPTIONCHANGED} );

/* Lighting layer definitions. Later layers take precedence, so these
   are defined in the order that they will be displayed in. */
const rgblight_segment_t* const PROGMEM my_rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    righty_nav_layer_colour,
    lefty_nav_layer_colour,
    control_layer_colour,
    gesture_layer_colour,
    option_changed_layer_colour
);

/* Colour layer names. */
enum {
    RIGHTY_NAV_LAYER_COLOUR = 0,
    LEFTY_NAV_LAYER_COLOUR,
    CONTROL_LAYER_COLOUR,
    GESTURE_LAYER_COLOUR,
    OPTION_CHANGED_LAYER_COLOUR    
};

 /* EEPROM memory for the various persistent states that the A+ can have. 
    See eeconfig_init_user() for default values and meanings. */
typedef union {
  uint32_t raw;
  struct {
    bool    left_handed :1;
    bool    horizontal_scroll_arrows :1;
    bool    vertical_scroll_arrows :1;
    bool    drag_scroll_mode :1;
    uint8_t led_brightness: 8;
  };
} user_config_t;

user_config_t user_config;

/* Add custom keycodes for control layer. */
enum my_keycodes {
  PKC_TGL_MIRROR = SAFE_RANGE,
  PKC_TGL_VERT_SCRL,
  PKC_TGL_HORIZ_SCRL,
  PKC_TGL_DRAG_SCRL,
  PKC_GESTURE,
  PKC_DRAG_SCROLL,
  PKC_ADJUST_LED_BRIGHTNESS,
  PKC_BLINKY_DPI_CONFIG
};

/* Layer names. */
enum {
    LAYER_NAV_RIGHT_HANDED = 0,
    LAYER_NAV_LEFT_HANDED,
    LAYER_CONTROL
};

/* Send an editing shortcut (copy, paste, etc.) with the host's primary modifier:
   CMD on MacOs and iOS, CTRL everywhere else. */
static void tap_edit_shortcut(uint16_t keycode) {
    os_variant_t os = detected_host_os();
    if( os == OS_MACOS || os == OS_IOS ) {
        tap_code16( G(keycode) );
    }
    else {
        tap_code16( C(keycode) );
    }
}

/* Override default function in submodule code so that we can blink whenever
   a successful gesture is registered. */
void pointing_device_gestures_trigger(uint8_t direction) {
    
    /* Directions are numbered off 0-7, starting with east. */
    switch( direction ) {
        case 0: /* East, Next Virtual Desktop. */
            /* On Windows, send CTRL + LGUI + RIGHT. */
            if ( detected_host_os() == OS_WINDOWS ) {
                tap_code16( LCG( KC_RIGHT ) );
            }
            /* On Linux, send CTRL + ALT + RIGHT. */
            else if( detected_host_os() == OS_LINUX ) {
                tap_code16( LCA( KC_RIGHT ) );
            }
            /* On MacOs, send Control + RIGHT. */
            else {
                tap_code16( LCTL( KC_RIGHT ) );
            }
            break;
        case 1: /* South-East, Paste. */
            tap_edit_shortcut( KC_V );
            break;
        case 2: /* South, Copy. */
            tap_edit_shortcut( KC_C );
            break;
        case 3: /* South-West, Cut. */
            tap_edit_shortcut( KC_X );
            break;
        case 4: /* West, Previous Virtual Desktop. */
            /* On Windows, send CTRL + LGUI + LEFT. */
            if ( detected_host_os() == OS_WINDOWS ) {
                tap_code16( LCG( KC_LEFT ) );
            }
            /* On Linux, send CTRL + ALT + LEFT. */
            else if( detected_host_os() == OS_LINUX ) {
                tap_code16( LCA( KC_LEFT ) );
            }
            /* On MacOs, send Control + LEFT. */
            else {
                tap_code16( LCTL( KC_LEFT ) );
            }
            break;
        case 5: /* North-West, Undo. */
            tap_edit_shortcut( KC_Z );
            break;
        case 6: /* North, Play/Pause Audio. */
            tap_code16( KC_MEDIA_PLAY_PAUSE );
            break;
        case 7: /* North-East, Redo. */
            /* On Windows/Linux, send CTRL + Y. */
            if ( detected_host_os() == OS_WINDOWS || detected_host_os() == OS_LINUX ) {
                tap_code16( C(KC_Y) );
            }
            /* On MacOs, send CMD + SHIFT + Z. */
            else {
                register_code(KC_LCMD);
                register_code(KC_LSFT);
                tap_code(KC_Z);
                unregister_code(KC_LCMD);
                unregister_code(KC_LSFT);
            }
            break;
        default:
            break;
    }

    /* Flash light to indicate successful gesture processing. */
    rgblight_blink_layer(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT*2);
}

/* Mouse gestures keymap. Since we cannot use custom keycodes in this array and
   we've overridden the function that uses it, we define this here with
   KC_NO (no action) to prevent a compiler error. See pointing_device_gestures_trigger()
   for what the gestures actually do. */
const uint16_t PROGMEM pointing_device_gestures[NUM_GESTURE_DIRECTIONS] =
    GESTURES_CARDINAL_AND_ORDINAL_DIRECTIONS( KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO );

/* Keymap. */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // Base layer with all of the mouse-related stuff for everyday use.
    [LAYER_NAV_RIGHT_HANDED] = LAYOUT(  MS_BTN4, MS_BTN5, PKC_DRAG_SCROLL, MS_BTN2, 
                                        MS_BTN1, MS_BTN3, 
                                        PKC_GESTURE, TG(LAYER_CONTROL) ),
    // Mirror-image of right-handed layout for lefties.
    [LAYER_NAV_LEFT_HANDED] = LAYOUT(   MS_BTN2, PKC_DRAG_SCROLL, MS_BTN4, MS_BTN5, 
                                        MS_BTN3, MS_BTN1, 
                                        TG(LAYER_CONTROL), PKC_GESTURE ),
    // Layer for all of the customization options.
    [LAYER_CONTROL] = LAYOUT(           PKC_BLINKY_DPI_CONFIG, PKC_ADJUST_LED_BRIGHTNESS, PKC_TGL_VERT_SCRL, PKC_TGL_HORIZ_SCRL,
                                        PKC_TGL_MIRROR, PKC_TGL_DRAG_SCRL, 
                                        TG(LAYER_CONTROL), TG(LAYER_CONTROL) ),
    [3] = LAYOUT( _______, _______, _______, _______, _______, _______, _______, _______ ),
    [4] = LAYOUT( _______, _______, _______, _______, _______, _______, _______, _______ ),
    [5] = LAYOUT( _______, _______, _______, _______, _______, _______, _______, _______ ),
    [6] = LAYOUT( _______, _______, _______, _______, _______, _______, _______, _______ ),
    [7] = LAYOUT( _______, _______, _______, _______, _______, _______, _______, _______ )
};

/* Called whenever layers are modified. */
layer_state_t layer_state_set_user(layer_state_t state) {
    /* Set layer colours. */
    switch (get_highest_layer(state)) {
        case LAYER_NAV_RIGHT_HANDED:
            rgblight_set_layer_state(RIGHTY_NAV_LAYER_COLOUR, true);
            
            rgblight_set_layer_state(LEFTY_NAV_LAYER_COLOUR, false);
            rgblight_set_layer_state(CONTROL_LAYER_COLOUR, false);            

            dprintf("change layer: right-handed nav\n");
            break;
        case LAYER_NAV_LEFT_HANDED:
            rgblight_set_layer_state(LEFTY_NAV_LAYER_COLOUR, true);

            rgblight_set_layer_state(RIGHTY_NAV_LAYER_COLOUR, false);
            rgblight_set_layer_state(CONTROL_LAYER_COLOUR, false);  

            dprintf("change layer: left-handed nav\n");
            break;
        case LAYER_CONTROL:
            rgblight_set_layer_state(CONTROL_LAYER_COLOUR, true);
            
            rgblight_set_layer_state(RIGHTY_NAV_LAYER_COLOUR, false);
            rgblight_set_layer_state(LEFTY_NAV_LAYER_COLOUR, false); 

            dprintf("change layer: control\n");
            break;
        default:
            rgblight_setrgb(RGB_CYAN);
            dprintf("change layer: oopsie, invalid layer!\n");
            break;
    }
    return state;
}

/* Set sane defaults when EEPROM is reset. */
void eeconfig_init_user(void) {

    user_config.raw = 0;

    user_config.left_handed = false;                // Right-handed.
    user_config.horizontal_scroll_arrows = false;   // False = Scroll events, True = left/right arrows.
    user_config.vertical_scroll_arrows = false;     // False = Scroll events, True = up/down arrows.
    user_config.drag_scroll_mode = false;           // Hold to activate.
    user_config.led_brightness = 4;                 // Brightest mode by default.

    eeconfig_update_user(user_config.raw);
}

void keyboard_pre_init_user(void) {

    /* Set up GPIO for both TMAG sensors. */
    gpio_set_pin_output_push_pull(TMAG5273_D0_PWR_PIN);
    gpio_set_pin_output_push_pull(TMAG5273_D1_PWR_PIN);
    gpio_write_pin_low(TMAG5273_D0_PWR_PIN);
    gpio_write_pin_low(TMAG5273_D1_PWR_PIN);

}

void keyboard_post_init_user(void) {
    /* ============================ Debug Setup ============================ */

    /* If you turn these on, also uncomment CONSOLE_ENABLE = yes in
       post_rules.mk! */
    //debug_enable = true;
    //debug_matrix=true;
    //debug_keyboard=true;
    //debug_mouse=true;

    /* ========================== Scroll Wheel Setup ======================= */

    /* Start up the two wheels. */
    tmag5273_init();

    /* Init TMAG5273 D0 (left wheel) */
    gpio_write_pin_high(TMAG5273_D0_PWR_PIN);
    wait_us(370);  // TMAG tstart_power_up, plus 100us margin
    tmag5273_init_device(TMAG5273_D0_I2C_ADDRESS);
    wait_ms(20);
    leftwheel_current_position = tmag5273_get_angle(TMAG5273_D0_I2C_ADDRESS);
    leftwheel_deadzone_center = leftwheel_current_position;


    /* Init TMAG5273 D1 (right wheel) */
    gpio_write_pin_high(TMAG5273_D1_PWR_PIN);
    wait_us(370);  // TMAG tstart_power_up, plus 100us margin
    tmag5273_init_device(TMAG5273_D1_I2C_ADDRESS);
    wait_ms(20);
    rightwheel_current_position = tmag5273_get_angle(TMAG5273_D1_I2C_ADDRESS);
    rightwheel_deadzone_center = rightwheel_current_position;


    /* =================== Persistent Configuration Setup ================== */

    /* Load up configuration details from the EEPROM. */
    user_config.raw = eeconfig_read_user();

    /* Set the startup layer based on handed-ness. */
    if( user_config.left_handed ) {
        layer_on(LAYER_NAV_LEFT_HANDED);
    }


    /* ============================ Lighting Setup ========================= */

    /* Enable the LED layers. */
    rgblight_layers = my_rgb_layers;

    /* Enable RGB lighting for layer indicators. */
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);

    /* Set an initial colour to establish a baseline brightness for the RGB layers. */
    rgblight_sethsv(0, 0, RGBLIGHT_VAL_STEP * user_config.led_brightness);
}  

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case PKC_TGL_MIRROR:
            if (record->event.pressed) {
                /* Toggle handed-ness, record new state to EEPROM. */
                user_config.left_handed ^= 1;
                eeconfig_update_user(user_config.raw);

                /* Activate the correct navigation layer and blink the LEDs. */
                if( user_config.left_handed ) {
                    dprintf( "Lefty nav mode.\n");
                    layer_on(LAYER_NAV_LEFT_HANDED);
                    rgblight_blink_layer(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT*2);
                }
                else {
                    dprintf( "Righty nav mode.\n");
                    layer_off(LAYER_NAV_LEFT_HANDED);
                    rgblight_blink_layer_repeat(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT, 2);
                }
            }
            return true;
        case PKC_TGL_VERT_SCRL:
            if (record->event.pressed) {
                user_config.vertical_scroll_arrows ^= 1;
                eeconfig_update_user(user_config.raw);

                if( user_config.vertical_scroll_arrows ) {
                    rgblight_blink_layer(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT*2);
                    dprintf("Vert. scroll = arrows.\n");
                }
                else {
                    rgblight_blink_layer_repeat(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT, 2);
                    dprintf("Vert. scroll = scroll events.\n");
                }
            }
            return true;
        case PKC_TGL_HORIZ_SCRL:
            if (record->event.pressed) {
                user_config.horizontal_scroll_arrows ^= 1;
                eeconfig_update_user(user_config.raw);

                if( user_config.horizontal_scroll_arrows ) {
                    rgblight_blink_layer(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT*2);
                    dprintf("Horiz. scroll = arrows.\n");
                }
                else {
                    rgblight_blink_layer_repeat(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT, 2);
                    dprintf("Horiz. scroll = scroll events.\n");
                }
            }
            return true;
        case PKC_TGL_DRAG_SCRL:
            if (record->event.pressed) {
                user_config.drag_scroll_mode ^= 1;
                eeconfig_update_user(user_config.raw);

                if( user_config.drag_scroll_mode ) {
                    rgblight_blink_layer(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT*2);
                    dprintf("Drag scroll: toggle to activate.\n");
                }
                else {
                    rgblight_blink_layer_repeat(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT, 2);
                    dprintf("Drag scroll: hold to activate.\n");
                }
            }
            return true;
        case PKC_GESTURE:
            if (record->event.pressed) {
                /* Send correct gesture mode activation based on preference. */
                pointing_device_gestures_start();
                rgblight_set_layer_state(GESTURE_LAYER_COLOUR, true);
            }
            /* Switch is released. */
            else {
                pointing_device_gestures_end();
                rgblight_set_layer_state(GESTURE_LAYER_COLOUR, false);
            }
            return true; 
        case PKC_DRAG_SCROLL:
            if( user_config.drag_scroll_mode ) {
                if (record->event.pressed) {
                    toggle_drag_scroll();
                }
            }
            /* Press and hold mode. */
            else {
                is_drag_scroll = record->event.pressed;
            }
            return true;
        case PKC_ADJUST_LED_BRIGHTNESS:
            /* Increase brightness in RGBLIGHT_VAL_STEP steps. 
               Go to minimum brightness if the max brightness is hit. */
            if (record->event.pressed) {
                if( rgblight_get_val() >= RGBLIGHT_VAL_STEP * 4 ) {
                    rgblight_decrease_val_noeeprom();
                    rgblight_decrease_val_noeeprom();
                    rgblight_decrease_val_noeeprom();
                    user_config.led_brightness = 1;
                    eeconfig_update_user(user_config.raw);
                }
                else {
                    rgblight_increase_val_noeeprom();
                    user_config.led_brightness += 1;
                    eeconfig_update_user(user_config.raw);
                }

                dprintf("Adjusted LED brightness. Current brightness = %d/255\n", rgblight_get_val());
            }
            return true;
        case PKC_BLINKY_DPI_CONFIG:
            if (record->event.pressed) {
                cycle_dpi();

                dprintf("DPI changed. Current DPI = %d\n", dpi_array[keyboard_config.dpi_config]);
                rgblight_blink_layer_repeat(OPTION_CHANGED_LAYER_COLOUR, OPTION_CHANGE_BLINK_TIMEOUT, keyboard_config.dpi_config + 1);
            }
            return true;         
        default:
            return true;
    }
}

/* State variables for the scroll wheels. */
int16_t leftwheel_lowres_scroll_tick = 0;
int16_t rightwheel_lowres_scroll_tick = 0;

int16_t leftwheel_arrow_scroll_tick = 0;
int16_t rightwheel_arrow_scroll_tick = 0;

int16_t leftwheel_volume_scroll_tick = 0;
int16_t rightwheel_volume_scroll_tick = 0;

uint32_t leftwheel_timeout = 0;
uint32_t rightwheel_timeout = 0;

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    // throttle reads
    if (timer_elapsed32(last_scroll_time) > 10) {
        uint16_t leftwheel_rawangle = tmag5273_get_angle(TMAG5273_D0_I2C_ADDRESS);
        uint16_t rightwheel_rawangle = tmag5273_get_angle(TMAG5273_D1_I2C_ADDRESS);

        int16_t leftwheel_deadzone_distance = calculate_deadzone_distance( leftwheel_rawangle, &leftwheel_deadzone_center );
        int16_t rightwheel_deadzone_distance = calculate_deadzone_distance( rightwheel_rawangle, &rightwheel_deadzone_center );

        int16_t leftwheel_delta = calculate_wheel_delta( leftwheel_rawangle, leftwheel_current_position );
        leftwheel_current_position = leftwheel_rawangle;

        int16_t rightwheel_delta = calculate_wheel_delta( rightwheel_rawangle, rightwheel_current_position );
        rightwheel_current_position = rightwheel_rawangle;

        /* If either wheel moved, we reset the timeout counter. Else, we start 
           counting inactivity. */
        if( abs(leftwheel_delta) > 2 ) { 
            leftwheel_timeout = 0;
        }
        else {
            /* Don't increment forever, you'll get rollover. */
            if( leftwheel_timeout < TMAG5273_DEADZONE_PROTECTOR_TIMEOUT ) {
                leftwheel_timeout += timer_elapsed32(last_scroll_time);
            }
        }
        if( abs(rightwheel_delta) > 2 ) { 
            rightwheel_timeout = 0; 
        }
        else {
            /* Don't increment forever, you'll get rollover. */
            if( rightwheel_timeout < TMAG5273_DEADZONE_PROTECTOR_TIMEOUT ) {
                rightwheel_timeout += timer_elapsed32(last_scroll_time);
            }
        }

        /* Reset the counter so we know when to next perform all this scroll processing. */
        last_scroll_time = timer_read32();

        /* If either of the wheels times out, we move the deadzone window to
           where the position is. This prevents spurious scroll events. */
        if( leftwheel_timeout >= TMAG5273_DEADZONE_PROTECTOR_TIMEOUT && 
            leftwheel_timeout < TMAG5273_DEADZONE_PROTECTOR_TIMEOUT * 2 ) {

            leftwheel_deadzone_center = leftwheel_current_position;
            dprintf("Left wheel timeout, deadzone moved to: %d\n", leftwheel_deadzone_center);
            
            /* Set the timeout to a large number so that we only shift the window once. */
            leftwheel_timeout = TMAG5273_DEADZONE_PROTECTOR_TIMEOUT * 4;
        }
        if( rightwheel_timeout >= TMAG5273_DEADZONE_PROTECTOR_TIMEOUT && 
            rightwheel_timeout < TMAG5273_DEADZONE_PROTECTOR_TIMEOUT * 2 ) {

            rightwheel_deadzone_center = rightwheel_current_position;
            dprintf("Right wheel timeout, deadzone moved to: %d\n", rightwheel_deadzone_center);
            
            /* Set the timeout to a large number so that we only shift the window once. */
            rightwheel_timeout = TMAG5273_DEADZONE_PROTECTOR_TIMEOUT * 4;
        }

        /* If we're on the control layers, both wheels adjust the volume. */
        if( layer_state_is(LAYER_CONTROL) ) {

            leftwheel_volume_scroll_tick += leftwheel_delta;
            rightwheel_volume_scroll_tick += rightwheel_delta;

            if( leftwheel_volume_scroll_tick > TMAG5273_VOLUME_SCROLL_TICK_SIZE ) {
                tap_code(KC_VOLU);
                leftwheel_volume_scroll_tick = 0;
            }
            else if( leftwheel_volume_scroll_tick < -TMAG5273_VOLUME_SCROLL_TICK_SIZE ) {
                tap_code(KC_VOLD);
                leftwheel_volume_scroll_tick = 0;
            }
            if( rightwheel_volume_scroll_tick > TMAG5273_VOLUME_SCROLL_TICK_SIZE ) {
                tap_code(KC_VOLU);
                rightwheel_volume_scroll_tick = 0;
            }
            else if( rightwheel_volume_scroll_tick < -TMAG5273_VOLUME_SCROLL_TICK_SIZE ) {
                tap_code(KC_VOLD);
                rightwheel_volume_scroll_tick = 0;
            }

            /* Return early -- no other scrolling activity happens in the
               control layer. */
            return mouse_report;
        }

        /* If scroll arrow mode is activated for horizontal scrolling, then we do that. 
           This behaviour is the same across OSes. */
        if( user_config.horizontal_scroll_arrows ) {

            leftwheel_arrow_scroll_tick += leftwheel_delta;
            rightwheel_arrow_scroll_tick += rightwheel_delta;

            if( user_config.left_handed ) {
                if( leftwheel_arrow_scroll_tick > TMAG5273_HORIZ_SCROLL_TICK_SIZE ) {
                    tap_code(KC_RIGHT);
                    leftwheel_arrow_scroll_tick = 0;
                }
                else if( leftwheel_arrow_scroll_tick < -TMAG5273_HORIZ_SCROLL_TICK_SIZE ) {
                    tap_code(KC_LEFT);
                    leftwheel_arrow_scroll_tick = 0;
                }

                /* Set the delta to zero so we don't scroll *and* arrow at the same time. */
                leftwheel_delta = 0;
            }
            else {
                if( rightwheel_arrow_scroll_tick > TMAG5273_HORIZ_SCROLL_TICK_SIZE ) {
                    tap_code(KC_RIGHT);
                    rightwheel_arrow_scroll_tick = 0;
                }
                else if( rightwheel_arrow_scroll_tick < -TMAG5273_HORIZ_SCROLL_TICK_SIZE ) {
                    tap_code(KC_LEFT);
                    rightwheel_arrow_scroll_tick = 0;
                }
                /* Set the delta to zero so we don't scroll *and* arrow at the same time. */
                rightwheel_delta = 0;
            }
        }

        /* If scroll arrow mode is activated for vertical scrolling, then we do that. 
           This behaviour is the same across OSes. */
        if( user_config.vertical_scroll_arrows ) {

            leftwheel_arrow_scroll_tick += leftwheel_delta;
            rightwheel_arrow_scroll_tick += rightwheel_delta;

            if( user_config.left_handed ) {
                if( rightwheel_arrow_scroll_tick > TMAG5273_VERT_SCROLL_TICK_SIZE ) {
                    tap_code(KC_DOWN);
                    rightwheel_arrow_scroll_tick = 0;
                }
                else if( rightwheel_arrow_scroll_tick < -TMAG5273_VERT_SCROLL_TICK_SIZE ) {
                    tap_code(KC_UP);
                    rightwheel_arrow_scroll_tick = 0;
                }
                /* Set the delta to zero so we don't scroll *and* arrow at the same time. */
                rightwheel_delta = 0;
            }
            else {
                if( leftwheel_arrow_scroll_tick > TMAG5273_VERT_SCROLL_TICK_SIZE ) {
                    tap_code(KC_DOWN);
                    leftwheel_arrow_scroll_tick = 0;
                }
                else if( leftwheel_arrow_scroll_tick < -TMAG5273_VERT_SCROLL_TICK_SIZE ) {
                    tap_code(KC_UP);
                    leftwheel_arrow_scroll_tick = 0;
                }
                /* Set the delta to zero so we don't scroll *and* arrow at the same time. */
                leftwheel_delta = 0;
            }
        }

        /* If we're on Windows or Linux, send hi-res scroll events. */
        if ( (detected_host_os() == OS_WINDOWS || detected_host_os() == OS_LINUX) ) {
            
            if ( leftwheel_deadzone_distance > (TMAG5273_WHEEL_DEADZONE - 10) ||
                 leftwheel_deadzone_distance < (-TMAG5273_WHEEL_DEADZONE + 10)) {

                if( user_config.left_handed ) {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = leftwheel_delta / TMAG5273_HORIZONAL_WHEEL_SPEED_DIV;
                    }
                    
                }
                else {
                    mouse_report.v = -leftwheel_delta / TMAG5273_VERTICAL_WHEEL_SPEED_DIV;
                }
            }
            if ( rightwheel_deadzone_distance > (TMAG5273_WHEEL_DEADZONE - 10) ||
                 rightwheel_deadzone_distance < (-TMAG5273_WHEEL_DEADZONE + 10)) {

                if( user_config.left_handed ) {
                    mouse_report.v = -rightwheel_delta / TMAG5273_VERTICAL_WHEEL_SPEED_DIV;
                }
                else {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = rightwheel_delta / TMAG5273_HORIZONAL_WHEEL_SPEED_DIV;
                    }
                }
            }

        } else {
            /* In this case, we're on another OS, so we just send regular scroll events. */
            /* Certain operating systems, like MacOS, don't play well with the
               high-res scrolling implementation. For more details, see:
               https://github.com/qmk/qmk_firmware/issues/17585#issuecomment-2325248167
               128 gives the scroll wheels "ticks". */

            leftwheel_lowres_scroll_tick += leftwheel_delta;
            rightwheel_lowres_scroll_tick += rightwheel_delta;

            if( user_config.left_handed ) {
                if( leftwheel_lowres_scroll_tick > TMAG5273_LOWRES_TICK_SIZE ) {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = 1;
                    }
                    leftwheel_lowres_scroll_tick = 0;
                }
                else if( leftwheel_lowres_scroll_tick < -TMAG5273_LOWRES_TICK_SIZE ) {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = -1;
                    }
                    leftwheel_lowres_scroll_tick = 0;
                }

                if( rightwheel_lowres_scroll_tick > TMAG5273_LOWRES_TICK_SIZE ) {
                    mouse_report.v = -1;
                    rightwheel_lowres_scroll_tick = 0;
                }
                else if( rightwheel_lowres_scroll_tick < -TMAG5273_LOWRES_TICK_SIZE ) {
                    mouse_report.v = 1;
                    rightwheel_lowres_scroll_tick = 0;
                }
            }
            else {
                if( leftwheel_lowres_scroll_tick > TMAG5273_LOWRES_TICK_SIZE ) {
                    mouse_report.v = -1;
                    leftwheel_lowres_scroll_tick = 0;
                }
                else if( leftwheel_lowres_scroll_tick < -TMAG5273_LOWRES_TICK_SIZE ) {
                    mouse_report.v = 1;
                    leftwheel_lowres_scroll_tick = 0;
                }

                if( rightwheel_lowres_scroll_tick > TMAG5273_LOWRES_TICK_SIZE ) {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = 1;
                    }
                    rightwheel_lowres_scroll_tick = 0;
                }
                else if( rightwheel_lowres_scroll_tick < -TMAG5273_LOWRES_TICK_SIZE ) {
                    if( !user_config.horizontal_scroll_arrows ) {
                        mouse_report.h = -1;
                    }
                    rightwheel_lowres_scroll_tick = 0;
                }
            }
        }

        /* Set scroll data to zero if the corresponding button is 
           currently being held down. */

        /* Check left knob. */
        if( matrix_is_on(0,6) ) {
            if( user_config.left_handed ) {
                mouse_report.h = 0;
            }
            else {
                mouse_report.v = 0;
            }
        }
        /* Check right knob. */
        if( matrix_is_on(0,7) ) {
            if( user_config.left_handed ) {
                mouse_report.v = 0;
            }
            else {
                mouse_report.h = 0;
            }
        }

    }

    return mouse_report;
}