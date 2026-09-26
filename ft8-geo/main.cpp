/*
 * Copyright (C) 2026 Dmytro Onyshko
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
#include "ui_ft8geo.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"

namespace ui::external_app::ft8geo {
void initialize_app(ui::NavigationView& nav) {
    nav.push<FT8GeoView>();
}
}  // namespace ui::external_app::ft8geo

extern "C" {

__attribute__((section(".external_app.app_ft8geo.application_information"), used)) application_information_t _application_information_ft8geo = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::ft8geo::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,

    /*.app_name = */ "FT8-geo",
    /* A globe: circle outline with a meridian and two latitude lines - the "geo"
     * sibling of the original FT8 app's Costas-array icon. */
    /*.bitmap_data = */ {
        0x00,
        0x00,
        0x07,
        0xC0,
        0x18,
        0x60,
        0x21,
        0x04,
        0x23,
        0x84,
        0x43,
        0x82,
        0x47,
        0xF2,
        0x43,
        0x82,
        0x43,
        0x82,
        0x47,
        0xF2,
        0x43,
        0x82,
        0x23,
        0x84,
        0x21,
        0x04,
        0x18,
        0x60,
        0x07,
        0xC0,
        0x00,
        0x00,
    },
    /*.icon_color = */ ui::Color::green().v,
    /*.menu_location = */ app_location_t::RX,
    /*.desired_menu_position = */ -1,

    /*.m4_app_tag = portapack::spi_flash::image_tag_ft8_rx */ {'P', 'F', 'T', '8'},
    /*.m4_app_offset = */ 0x00000000,  // will be filled at compile time
};
}
