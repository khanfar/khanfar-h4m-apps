/*
 * Copyright (C) 2026 Khanfar
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui.hpp"
#include "ui_khanfar_rx.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"
#include "string_format.hpp"
#include <ch.h>

namespace ui::external_app::khanfarrx {
void initialize_app(ui::NavigationView& nav) {
    // Heap on the menu path is small and fragmented (~4-6 KB free); if the
    // ~2.3 KB view doesn't fit, operator new panics with "Out of Memory".
    // Check first so the user gets a message instead of a Guru Meditation.
    void* test = chHeapAlloc(NULL, sizeof(KhanfarRxView));
    if (test == nullptr) {
        nav.display_modal("KhanfarRX", "Not enough free memory.\nReboot and try again.");
        return;
    }
    chHeapFree(test);
    nav.push<KhanfarRxView>();
}
}  // namespace ui::external_app::khanfarrx

extern "C" {

__attribute__((section(".external_app.app_khanfarrx.application_information"), used)) application_information_t _application_information_khanfarrx = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::khanfarrx::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,

    /*.app_name = */ "KhanfarRX",
    /*.bitmap_data = */ {
        0x00,
        0x00,
        0x01,
        0x80,
        0x03,
        0xC0,
        0x07,
        0xE0,
        0x0F,
        0xF0,
        0x1F,
        0xF8,
        0x3F,
        0xFC,
        0x7F,
        0xFE,
        0x7F,
        0xFE,
        0x3F,
        0xFC,
        0x1F,
        0xF8,
        0x0F,
        0xF0,
        0x07,
        0xE0,
        0x03,
        0xC0,
        0x01,
        0x80,
        0x00,
        0x00,
    },
    /*.icon_color = */ ui::Color::cyan().v,
    /*.menu_location = */ app_location_t::RX,
    /*.desired_menu_position = */ -1,

    /* NB: the bundled M4 image reserves RAM ahead of this app's code (the loader
     * places the app right after it in the m4_code region). It must be the
     * LARGEST baseband image this app loads at runtime - otherwise a bigger
     * image loaded later via baseband::run_image() would overwrite the app's
     * own code and crash. This app loads PAMA/PNFM/PWFM/PSPE; PWFM is the
     * largest (18844 bytes). fmradio/scanner/level do the same. */
    /*.m4_app_tag = portapack::spi_flash::image_tag_wfm_audio */ {'P', 'W', 'F', 'M'},
    /*.m4_app_offset = */ 0x00000000,  // will be filled at compile time
};
}
