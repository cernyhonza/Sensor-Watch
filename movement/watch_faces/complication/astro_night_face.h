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
 */

#ifndef ASTRO_NIGHT_FACE_H_
#define ASTRO_NIGHT_FACE_H_

/*
 * ASTRONOMICAL NIGHT FACE
 *
 * The Astronomical Night face is designed to display the start and end
 * of astronomical night (when the Sun is 18 degrees or more below the horizon)
 * for a given location. It also functions as an interface for setting the
 * location register, identical to the sunrise/sunset face.
 */

#include "movement.h"

typedef struct {
    uint8_t sign: 1;        // 0-1
    uint8_t hundreds: 1;    // 0-1, ignored for latitude
    uint8_t tens: 4;        // 0-9 (must wrap at 10)
    uint8_t ones: 4;        // 0-9 (must wrap at 10)
    uint8_t tenths: 4;      // 0-9 (must wrap at 10)
    uint8_t hundredths: 4;  // 0-9 (must wrap at 10)
} astro_night_lat_lon_settings_t;

typedef struct {
    uint8_t page;
    uint8_t rise_index;
    uint8_t active_digit;
    bool location_changed;
    watch_date_time rise_set_expires;
    astro_night_lat_lon_settings_t working_latitude;
    astro_night_lat_lon_settings_t working_longitude;
    uint8_t longLatToUse;
} astro_night_state_t;

void astro_night_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr);
void astro_night_face_activate(movement_settings_t *settings, void *context);
bool astro_night_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void astro_night_face_resign(movement_settings_t *settings, void *context);

#define astro_night_face ((const watch_face_t){ \
    astro_night_face_setup, \
    astro_night_face_activate, \
    astro_night_face_loop, \
    astro_night_face_resign, \
    NULL, \
})

typedef struct {
    char name[3];
    int16_t latitude;
    int16_t longitude;
} astro_night_long_lat_presets_t;

static const astro_night_long_lat_presets_t astroNightLongLatPresets[] =
{
    { .name = "  "},  // Default, the long and lat get replaced by what's set in the watch
};

#endif // ASTRO_NIGHT_FACE_H_
