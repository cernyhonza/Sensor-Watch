/*
 * MIT License
 *
 * Copyright (c) 2026 Jan Cerny
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Astronomical night calculations use Paul Schlyter's sunriset library.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "astro_night_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "sunriset.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

static const uint8_t _location_count = sizeof(astroNightLongLatPresets) / sizeof(astro_night_long_lat_presets_t);

static void _astro_night_set_expiration(astro_night_state_t *state, watch_date_time next_rise_set) {
    uint32_t timestamp = watch_utility_date_time_to_unix_time(next_rise_set, 0);
    state->rise_set_expires = watch_utility_date_time_from_unix_time(timestamp + 60, 0);
}

static void _astro_night_face_update(movement_settings_t *settings, astro_night_state_t *state) {
    char buf[14];
    double rise, set, minutes, seconds;
    bool show_next_match = false;
    movement_location_t movement_location;
    if (state->longLatToUse == 0 || _location_count <= 1)
        movement_location = (movement_location_t) watch_get_backup_data(1);
    else {
        movement_location.bit.latitude = astroNightLongLatPresets[state->longLatToUse].latitude;
        movement_location.bit.longitude = astroNightLongLatPresets[state->longLatToUse].longitude;
    }

    if (movement_location.reg == 0) {
        watch_display_string("nt  no Loc", 0);
        return;
    }

    watch_date_time date_time = watch_rtc_get_date_time(); // the current local date / time
    watch_date_time utc_now = watch_utility_date_time_convert_zone(date_time, movement_timezone_offsets[settings->bit.time_zone] * 60, 0); // the current date / time in UTC
    watch_date_time scratch_time; // scratchpad, contains different values at different times
    scratch_time.reg = utc_now.reg;

    int16_t lat_centi = (int16_t)movement_location.bit.latitude;
    int16_t lon_centi = (int16_t)movement_location.bit.longitude;

    double lat = (double)lat_centi / 100.0;
    double lon = (double)lon_centi / 100.0;

    // sunriset returns times as signed decimal hours in UTC.
    // We set aside the offset in hours, and add it back before converting it to a watch_date_time.
    double hours_from_utc = ((double)movement_timezone_offsets[settings->bit.time_zone]) / 60.0;

    // We loop twice because if it's after the events today, we need to recalculate to display values for tomorrow.
    for (int i = 0; i < 2; i++) {
        // astronomical_twilight computes:
        // rise = start of astronomical morning twilight (end of astronomical night)
        // set  = end of astronomical evening twilight (start of astronomical night)
        uint8_t result = astronomical_twilight(scratch_time.unit.year + WATCH_RTC_REFERENCE_YEAR, scratch_time.unit.month, scratch_time.unit.day, lon, lat, &rise, &set);

        if (result != 0) {
            watch_clear_colon();
            watch_clear_indicator(WATCH_INDICATOR_PM);
            watch_clear_indicator(WATCH_INDICATOR_24H);
            // result == 1: sun above -18 deg all 24h (no astronomical night, continuous twilight or day) -> "nt none"
            // result == -1: sun below -18 deg all 24h (polar astronomical night 24h) -> "nt 24h"
            if (result == 1) {
                sprintf(buf, "nt%2d none ", scratch_time.unit.day);
            } else {
                sprintf(buf, "nt%2d  24h ", scratch_time.unit.day);
            }
            watch_display_string(buf, 0);
            return;
        }

        watch_set_colon();
        if (settings->bit.clock_mode_24h && !settings->bit.clock_24h_leading_zero) watch_set_indicator(WATCH_INDICATOR_24H);

        rise += hours_from_utc;
        set += hours_from_utc;

        // rise: morning astronomical twilight start / astronomical night end ("Ed")
        minutes = 60.0 * fmod(rise, 1);
        seconds = 60.0 * fmod(minutes, 1);
        scratch_time.unit.hour = floor(rise);
        if (seconds < 30) scratch_time.unit.minute = floor(minutes);
        else scratch_time.unit.minute = ceil(minutes);

        if (scratch_time.unit.minute == 60) {
            scratch_time.unit.minute = 0;
            scratch_time.unit.hour = (scratch_time.unit.hour + 1) % 24;
        }

        if (date_time.reg < scratch_time.reg) _astro_night_set_expiration(state, scratch_time);

        if (date_time.reg < scratch_time.reg || show_next_match) {
            if (state->rise_index == 0 || show_next_match) {
                bool set_leading_zero = false;
                if (!settings->bit.clock_mode_24h) {
                    if (watch_utility_convert_to_12_hour(&scratch_time)) watch_set_indicator(WATCH_INDICATOR_PM);
                    else watch_clear_indicator(WATCH_INDICATOR_PM);
                } else if (settings->bit.clock_24h_leading_zero && scratch_time.unit.hour < 10) {
                    set_leading_zero = true;
                }
                sprintf(buf, "Ed%2d%2d%02d%s", scratch_time.unit.day, scratch_time.unit.hour, scratch_time.unit.minute, astroNightLongLatPresets[state->longLatToUse].name);
                watch_display_string(buf, 0);
                if (set_leading_zero)
                    watch_display_string("0", 4);
                return;
            } else {
                show_next_match = true;
            }
        }

        // set: evening astronomical twilight end / astronomical night start ("St")
        minutes = 60.0 * fmod(set, 1);
        seconds = 60.0 * fmod(minutes, 1);
        scratch_time.unit.hour = floor(set);
        if (seconds < 30) scratch_time.unit.minute = floor(minutes);
        else scratch_time.unit.minute = ceil(minutes);

        if (scratch_time.unit.minute == 60) {
            scratch_time.unit.minute = 0;
            scratch_time.unit.hour = (scratch_time.unit.hour + 1) % 24;
        }

        if (date_time.reg < scratch_time.reg) _astro_night_set_expiration(state, scratch_time);

        if (date_time.reg < scratch_time.reg || show_next_match) {
            if (state->rise_index == 0 || show_next_match) {
                bool set_leading_zero = false;
                if (!settings->bit.clock_mode_24h) {
                    if (watch_utility_convert_to_12_hour(&scratch_time)) watch_set_indicator(WATCH_INDICATOR_PM);
                    else watch_clear_indicator(WATCH_INDICATOR_PM);
                } else if (settings->bit.clock_24h_leading_zero && scratch_time.unit.hour < 10) {
                    set_leading_zero = true;
                }
                sprintf(buf, "St%2d%2d%02d%s", scratch_time.unit.day, scratch_time.unit.hour, scratch_time.unit.minute, astroNightLongLatPresets[state->longLatToUse].name);
                watch_display_string(buf, 0);
                if (set_leading_zero)
                    watch_display_string("0", 4);
                return;
            } else {
                show_next_match = true;
            }
        }

        // It's after tonight's night start. We need to display values for tomorrow.
        uint32_t timestamp = watch_utility_date_time_to_unix_time(utc_now, 0);
        timestamp += 86400;
        scratch_time = watch_utility_date_time_from_unix_time(timestamp, 0);
    }
}

static int16_t _astro_night_face_latlon_from_struct(astro_night_lat_lon_settings_t val) {
    int16_t retval = (val.sign ? -1 : 1) *
                        (
                            val.hundreds * 10000 +
                            val.tens * 1000 +
                            val.ones * 100 +
                            val.tenths * 10 +
                            val.hundredths
                        );
    return retval;
}

static astro_night_lat_lon_settings_t _astro_night_face_struct_from_latlon(int16_t val) {
    astro_night_lat_lon_settings_t retval;

    retval.sign = val < 0;
    val = abs(val);
    retval.hundredths = val % 10;
    val /= 10;
    retval.tenths = val % 10;
    val /= 10;
    retval.ones = val % 10;
    val /= 10;
    retval.tens = val % 10;
    val /= 10;
    retval.hundreds = val % 10;

    return retval;
}

static void _astro_night_face_update_location_register(astro_night_state_t *state) {
    if (state->location_changed) {
        movement_location_t movement_location;
        int16_t lat = _astro_night_face_latlon_from_struct(state->working_latitude);
        int16_t lon = _astro_night_face_latlon_from_struct(state->working_longitude);
        movement_location.bit.latitude = lat;
        movement_location.bit.longitude = lon;
        watch_store_backup_data(movement_location.reg, 1);
        state->location_changed = false;
    }
}

static void _astro_night_face_update_settings_display(movement_event_t event, astro_night_state_t *state) {
    char buf[12];

    switch (state->page) {
        case 0:
            return;
        case 1:
            sprintf(buf, "LA  %c %04d", state->working_latitude.sign ? '-' : '+', abs(_astro_night_face_latlon_from_struct(state->working_latitude)));
            break;
        case 2:
            sprintf(buf, "LO  %c%05d", state->working_longitude.sign ? '-' : '+', abs(_astro_night_face_latlon_from_struct(state->working_longitude)));
            break;
    }
    if (event.subsecond % 2) {
        buf[state->active_digit + 4] = ' ';
    }
    watch_display_string(buf, 0);
}

static void _astro_night_face_advance_digit(astro_night_state_t *state) {
    state->location_changed = true;
    switch (state->page) {
        case 1: // latitude
            switch (state->active_digit) {
                case 0:
                    state->working_latitude.sign++;
                    break;
                case 1:
                    // we skip this digit
                    break;
                case 2:
                    state->working_latitude.tens = (state->working_latitude.tens + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_latitude)) > 9000) {
                        state->working_latitude.ones = 0;
                        state->working_latitude.tenths = 0;
                        state->working_latitude.hundredths = 0;
                    }
                    break;
                case 3:
                    state->working_latitude.ones = (state->working_latitude.ones + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                    break;
                case 4:
                    state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                    break;
                case 5:
                    state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
                    break;
            }
            break;
        case 2: // longitude
            switch (state->active_digit) {
                case 0:
                    state->working_longitude.sign++;
                    break;
                case 1:
                    state->working_longitude.hundreds = (state->working_longitude.hundreds + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_longitude)) > 18000) {
                        state->working_longitude.tens = 8;
                        state->working_longitude.ones = 0;
                        state->working_longitude.tenths = 0;
                        state->working_longitude.hundredths = 0;
                    }
                    break;
                case 2:
                    state->working_longitude.tens = (state->working_longitude.tens + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tens = 0;
                    break;
                case 3:
                    state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                    break;
                case 4:
                    state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                    break;
                case 5:
                    state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                    if (abs(_astro_night_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                    break;
            }
            break;
    }
}

void astro_night_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr) {
    (void) settings;
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(astro_night_state_t));
        memset(*context_ptr, 0, sizeof(astro_night_state_t));
    }
}

void astro_night_face_activate(movement_settings_t *settings, void *context) {
    (void) settings;
    if (watch_tick_animation_is_running()) watch_stop_tick_animation();

#if __EMSCRIPTEN__
    int16_t browser_lat = EM_ASM_INT({
        return lat;
    });
    int16_t browser_lon = EM_ASM_INT({
        return lon;
    });
    if ((watch_get_backup_data(1) == 0) && (browser_lat || browser_lon)) {
        movement_location_t browser_loc;
        browser_loc.bit.latitude = browser_lat;
        browser_loc.bit.longitude = browser_lon;
        watch_store_backup_data(browser_loc.reg, 1);
    }
#endif

    astro_night_state_t *state = (astro_night_state_t *)context;
    movement_location_t movement_location = (movement_location_t) watch_get_backup_data(1);
    state->working_latitude = _astro_night_face_struct_from_latlon(movement_location.bit.latitude);
    state->working_longitude = _astro_night_face_struct_from_latlon(movement_location.bit.longitude);
}

bool astro_night_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    astro_night_state_t *state = (astro_night_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _astro_night_face_update(settings, state);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
        case EVENT_TICK:
            if (state->page == 0) {
                if (event.event_type == EVENT_LOW_ENERGY_UPDATE && !watch_tick_animation_is_running()) watch_start_tick_animation(1000);
                watch_date_time date_time = watch_rtc_get_date_time();
                if (date_time.reg >= state->rise_set_expires.reg) {
                    state->rise_index = 0;
                    _astro_night_face_update(settings, state);
                }
            } else {
                _astro_night_face_update_settings_display(event, state);
            }
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            if (state->page) {
                state->active_digit++;
                if (state->page == 1 && state->active_digit == 1) state->active_digit++;
                if (state->active_digit > 5) {
                    state->active_digit = 0;
                    state->page = (state->page + 1) % 3;
                    _astro_night_face_update_location_register(state);
                }
                _astro_night_face_update_settings_display(event, (astro_night_state_t *)context);
            } else if (_location_count <= 1) {
                movement_illuminate_led();
            }
            if (state->page == 0) {
                movement_request_tick_frequency(1);
                _astro_night_face_update(settings, state);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            if (_location_count <= 1) break;
            else if (!state->page) movement_illuminate_led();
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->page == 0 && _location_count > 1) {
                state->longLatToUse = (state->longLatToUse + 1) % _location_count;
                _astro_night_face_update(settings, state);
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->page) {
                _astro_night_face_advance_digit(state);
                _astro_night_face_update_settings_display(event, (astro_night_state_t *)context);
            } else {
                state->rise_index = (state->rise_index + 1) % 2;
                _astro_night_face_update(settings, state);
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->page == 0) {
                if (state->longLatToUse != 0) {
                    state->longLatToUse = 0;
                    _astro_night_face_update(settings, state);
                    break;
                }
                state->page++;
                state->active_digit = 0;
                watch_clear_display();
                movement_request_tick_frequency(4);
                _astro_night_face_update_settings_display(event, (astro_night_state_t *)context);
            } else {
                state->active_digit = 0;
                state->page = 0;
                _astro_night_face_update_location_register(state);
                _astro_night_face_update(settings, state);
            }
            break;
        case EVENT_TIMEOUT:
            if (watch_get_backup_data(1) == 0) {
                movement_move_to_face(0);
            } else if (state->page || state->rise_index) {
                state->page = 0;
                state->rise_index = 0;
                movement_request_tick_frequency(1);
                _astro_night_face_update(settings, state);
            }
            break;
        default:
            return movement_default_loop_handler(event, settings);
    }

    return true;
}

void astro_night_face_resign(movement_settings_t *settings, void *context) {
    (void) settings;
    astro_night_state_t *state = (astro_night_state_t *)context;
    state->page = 0;
    state->active_digit = 0;
    state->rise_index = 0;
    _astro_night_face_update_location_register(state);
}
