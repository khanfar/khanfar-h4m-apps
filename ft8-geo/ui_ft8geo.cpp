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

#include "ui_ft8geo.hpp"

#include "audio.hpp"
#include "baseband_api.hpp"
#include "file_path.hpp"
#include "string_format.hpp"
#include "ui_font_fixed_5x8.hpp"

#include <math.h>
#include <string.h>

/* The map view registers itself here while it is open, so the decoder view
 * underneath can push new spots into it in real time. The H4M shows one view
 * at a time, so a single static pointer is enough. */
namespace ui::external_app::ft8geo {
class FT8GeoMapView;
static FT8GeoMapView* active_map_view = nullptr;
}  // namespace ui::external_app::ft8geo

using namespace portapack;

namespace ui::external_app::ft8geo {

void FT8GeoSpots::add(float lat, float lon, const char* call, const char* grid, uint8_t type) {
    size_t idx;
    for (idx = 0; idx < count; idx++) {
        if (strncmp(spot[idx].call, call, sizeof(spot[idx].call)) == 0)
            break;  // same station heard again: refresh in place
    }
    FT8Spot* s;
    if (idx < count) {
        s = &spot[idx];
    } else if (count < MAX_SPOTS) {
        idx = count++;
        s = &spot[idx];
    } else {
        idx = oldest;  // full: recycle the oldest entry
        oldest = (oldest + 1) % MAX_SPOTS;
        s = &spot[idx];
    }
    s->lat = lat;
    s->lon = lon;
    s->type = type;
    strncpy(s->call, call, sizeof(s->call) - 1);
    s->call[sizeof(s->call) - 1] = '\0';
    strncpy(s->grid, grid, sizeof(s->grid) - 1);
    s->grid[sizeof(s->grid) - 1] = '\0';
}

/* A 4-char Maidenhead locator is two letters A-R then two digits, and in an
 * FT8 message it is the last token ("CQ ON1AEY JO11", "EK7DY DA0O JO30").
 * "RR73" is a report, not a grid. The station's callsign is the token before
 * the locator. Returns false when the message carries no locator. */
static bool parse_spot(const char* text, char* call_out, char* grid_out, float* lat_out, float* lon_out) {
    const char* last_grid = nullptr;
    const char* prev_token = nullptr;
    size_t prev_len = 0;

    const char* p = text;
    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        const char* tok = p;
        while (*p && *p != ' ')
            p++;
        const size_t len = p - tok;

        if (len == 4 &&
            tok[0] >= 'A' && tok[0] <= 'R' && tok[1] >= 'A' && tok[1] <= 'R' &&
            tok[2] >= '0' && tok[2] <= '9' && tok[3] >= '0' && tok[3] <= '9' &&
            strncmp(tok, "RR73", 4) != 0) {
            last_grid = tok;
            if (prev_token && prev_len < 11 && !(prev_len == 2 && strncmp(prev_token, "CQ", 2) == 0)) {
                memcpy(call_out, prev_token, prev_len);
                call_out[prev_len] = '\0';
            } else {
                call_out[0] = '\0';
            }
        }
        prev_token = tok;
        prev_len = len;
    }

    if (!last_grid)
        return false;

    memcpy(grid_out, last_grid, 4);
    grid_out[4] = '\0';

    // Square center, same math as the QTH entry in the map view.
    *lon_out = (last_grid[0] - 'A') * 20.0f + (last_grid[2] - '0') * 2.0f - 179.0f;
    *lat_out = (last_grid[1] - 'A') * 10.0f + (last_grid[3] - '0') - 89.5f;
    return true;
}

/* Callsign prefix -> full country name. Longest prefix wins, so "A6" beats
 * "A". Lives in flash, costs no RAM. Not the full ITU table — the common DX
 * prefixes; unknown calls show "---". Names capped at 15 chars to fit the
 * list column. */
struct CountryPrefix {
    const char* prefix;
    const char* code;
};
static const CountryPrefix country_table[] = {
    {"4L", "Georgia"}, {"4S", "Sri Lanka"}, {"4X", "Israel"}, {"4Z", "Israel"},
    {"5A", "Libya"}, {"5H", "Tanzania"}, {"5N", "Nigeria"}, {"5R", "Madagascar"},
    {"5T", "Mauritania"}, {"5X", "Uganda"}, {"5Z", "Kenya"}, {"6W", "Senegal"},
    {"6Y", "Jamaica"}, {"7O", "Yemen"}, {"7P", "Lesotho"}, {"7X", "Algeria"},
    {"8P", "Barbados"}, {"8Q", "Maldives"}, {"9A", "Croatia"}, {"9G", "Ghana"},
    {"9H", "Malta"}, {"9J", "Zambia"}, {"9K", "Kuwait"}, {"9L", "Sierra Leone"},
    {"9M", "Malaysia"}, {"9N", "Nepal"}, {"9Q", "DR Congo"}, {"9V", "Singapore"},
    {"9Y", "Trinidad-Tobago"}, {"A2", "Botswana"}, {"A4", "Oman"}, {"A5", "Bhutan"},
    {"A6", "Arab Emirates"}, {"A7", "Qatar"}, {"A9", "Bahrain"}, {"AP", "Pakistan"},
    {"BS", "China"}, {"BV", "Taiwan"}, {"BY", "China"}, {"BA", "China"},
    {"BD", "China"}, {"BG", "China"}, {"BH", "China"}, {"BI", "China"},
    {"BJ", "China"}, {"BL", "China"}, {"BT", "China"}, {"B", "China"},
    {"C2", "Nauru"}, {"C6", "Bahamas"}, {"CE", "Chile"}, {"CM", "Cuba"},
    {"CN", "Morocco"}, {"CO", "Cuba"}, {"CP", "Bolivia"}, {"CT", "Portugal"},
    {"CX", "Uruguay"}, {"D2", "Angola"}, {"DL", "Germany"}, {"DU", "Philippines"},
    {"DZ", "Philippines"}, {"E5", "Cook Isl."}, {"E7", "Bosnia"}, {"EA", "Spain"},
    {"EI", "Ireland"}, {"EK", "Armenia"}, {"EL", "Liberia"}, {"EP", "Iran"},
    {"ER", "Moldova"}, {"ES", "Estonia"}, {"ET", "Ethiopia"}, {"EU", "Belarus"},
    {"EX", "Kyrgyzstan"}, {"EY", "Tajikistan"}, {"EZ", "Turkmenistan"}, {"F", "France"},
    {"FK", "New Caledonia"}, {"FO", "Fr.Polynesia"}, {"G", "England"}, {"GD", "Isle of Man"},
    {"GI", "N.Ireland"}, {"GM", "Scotland"}, {"GW", "Wales"}, {"H4", "Solomon Isl."},
    {"HB", "Switzerland"}, {"HC", "Ecuador"}, {"HI", "Dominican Rep."}, {"HK", "Colombia"},
    {"HL", "South Korea"}, {"HP", "Panama"}, {"HR", "Honduras"}, {"HS", "Thailand"},
    {"HZ", "Saudi Arabia"}, {"I", "Italy"}, {"J2", "Djibouti"}, {"J3", "Grenada"},
    {"J6", "St.Lucia"}, {"J7", "Dominica"}, {"JA", "Japan"}, {"JY", "Jordan"},
    {"K", "United States"}, {"KH6", "Hawaii"}, {"KL7", "Alaska"}, {"KP4", "Puerto Rico"},
    {"LA", "Norway"}, {"LU", "Argentina"}, {"LX", "Luxembourg"}, {"LY", "Lithuania"},
    {"LZ", "Bulgaria"}, {"N", "United States"}, {"OA", "Peru"}, {"OD", "Lebanon"},
    {"OE", "Austria"}, {"OH", "Finland"}, {"OK", "Czech Rep."}, {"OM", "Slovakia"},
    {"ON", "Belgium"}, {"OX", "Greenland"}, {"OY", "Faroe Isl."}, {"OZ", "Denmark"},
    {"P2", "Papua N Guinea"}, {"P4", "Aruba"}, {"PA", "Netherlands"}, {"PJ", "Curacao"},
    {"PY", "Brazil"}, {"PZ", "Suriname"}, {"S5", "Slovenia"}, {"SM", "Sweden"},
    {"SP", "Poland"}, {"ST", "Sudan"}, {"SU", "Egypt"}, {"SV", "Greece"},
    {"T7", "San Marino"}, {"TA", "Turkey"}, {"TF", "Iceland"}, {"TG", "Guatemala"},
    {"TI", "Costa Rica"}, {"TJ", "Cameroon"}, {"TL", "Centr.Afr.Rep"}, {"TN", "Congo"},
    {"TR", "Gabon"}, {"TU", "Ivory Coast"}, {"TY", "Benin"}, {"TZ", "Mali"},
    {"UA", "Russia"}, {"RA", "Russia"}, {"R", "Russia"}, {"UJ", "Uzbekistan"},
    {"UK", "Uzbekistan"}, {"UN", "Kazakhstan"}, {"UR", "Ukraine"}, {"V2", "Antigua"},
    {"V3", "Belize"}, {"V4", "St.Kitts"}, {"V5", "Namibia"}, {"V6", "Micronesia"},
    {"V7", "Marshall Isl."}, {"V8", "Brunei"}, {"VE", "Canada"}, {"VK", "Australia"},
    {"VP2", "Anguilla"}, {"VP5", "Turks&Caicos"}, {"VQ9", "Diego Garcia"}, {"VU", "India"},
    {"W", "United States"}, {"XE", "Mexico"}, {"XU", "Cambodia"}, {"XV", "Vietnam"},
    {"XW", "Laos"}, {"XX", "Macao"}, {"XZ", "Myanmar"}, {"YB", "Indonesia"},
    {"YI", "Iraq"}, {"YJ", "Vanuatu"}, {"YL", "Latvia"}, {"YN", "Nicaragua"},
    {"YO", "Romania"}, {"YS", "El Salvador"}, {"YU", "Serbia"}, {"YV", "Venezuela"},
    {"Z2", "Zimbabwe"}, {"Z3", "Macedonia"}, {"ZA", "Albania"}, {"ZB", "Gibraltar"},
    {"ZL", "New Zealand"}, {"ZP", "Paraguay"}, {"ZS", "South Africa"}, {"A", "United States"},
};

static const char* country_for_call(const char* call) {
    size_t best = 0;
    const char* code = "---";
    for (const auto& e : country_table) {
        const size_t l = strlen(e.prefix);
        if (l > best && strncmp(call, e.prefix, l) == 0) {
            best = l;
            code = e.code;
        }
    }
    return code;
}

static ui::Color spot_type_color(uint8_t type) {
    switch (type) {
        case SPOT_CQ:
            return ui::Color::green();
        case SPOT_73:
            return ui::Color::red();
        default:
            return ui::Color::yellow();
    }
}

/* Distance and bearing from home, equirectangular approximation. */
static uint32_t spot_distance_km(float home_lat, float home_lon, float lat, float lon) {
    const float dlat = (lat - home_lat) * 0.0174532925f;
    const float dlon = (lon - home_lon) * 0.0174532925f;
    const float m = cosf((lat + home_lat) * 0.5f * 0.0174532925f);
    return (uint32_t)(sqrtf(dlat * dlat + dlon * dlon * m * m) * 6371.0f + 0.5f);
}

static uint16_t spot_bearing_deg(float home_lat, float home_lon, float lat, float lon) {
    const float dlon = (lon - home_lon) * 0.0174532925f;
    const float y = sinf(dlon) * cosf(lat * 0.0174532925f);
    const float x = cosf(home_lat * 0.0174532925f) * sinf(lat * 0.0174532925f) -
                    sinf(home_lat * 0.0174532925f) * cosf(lat * 0.0174532925f) * cosf(dlon);
    float brg = atan2f(y, x) * 57.2957795f;
    if (brg < 0.0f)
        brg += 360.0f;
    return (uint16_t)(brg + 0.5f) % 360;
}

/* FT8GeoSpotList ***********************************************************/

void FT8GeoSpotList::follow_tail() {
    if (!spots || spots->count == 0) {
        selected = -1;
        scroll = 0;
        return;
    }
    selected = (int8_t)spots->count - 1;
    scroll = selected >= ROWS ? selected - ROWS + 1 : 0;
}

void FT8GeoSpotList::paint(Painter& painter) {
    const auto r = screen_rect();
    painter.fill_rectangle(r, ui::Color::black());
    const int n = spots ? (int)spots->count : 0;

    painter.draw_string({r.left() + 2, r.top()}, ui::font::fixed_5x8,
                        ui::Color::white(), ui::Color::black(),
                        to_string_dec_uint(n) + " SPOTS" + (selected >= 0 ? "  SEL:" + to_string_dec_uint(selected + 1) : std::string{}));

    for (int8_t row = 0; row < ROWS; row++) {
        const int idx = scroll + row;
        if (idx >= n)
            break;
        const FT8Spot& s = spots->spot[idx];
        const int16_t y = r.top() + 8 + row * 8;
        const bool sel = (idx == selected);
        const auto fg = spot_type_color(s.type);
        const auto bg = sel ? ui::Color::dark_blue() : ui::Color::black();
        if (sel)
            painter.fill_rectangle({{r.left(), y}, {r.width(), 8}}, bg);

        painter.draw_string({r.left() + 2, y}, ui::font::fixed_5x8, fg, bg, s.call);
        painter.draw_string({r.left() + 56, y}, ui::font::fixed_5x8, fg, bg, s.grid);
        painter.draw_string({r.left() + 84, y}, ui::font::fixed_5x8, fg, bg, country_for_call(s.call));
        if (home_lat <= 90.0f) {
            painter.draw_string({r.left() + 166, y}, ui::font::fixed_5x8, fg, bg,
                                to_string_dec_uint(spot_distance_km(home_lat, home_lon, s.lat, s.lon)) + "KM");
            painter.draw_string({r.left() + 206, y}, ui::font::fixed_5x8, fg, bg,
                                to_string_dec_uint(spot_bearing_deg(home_lat, home_lon, s.lat, s.lon)) + "o");
        }
    }
}

bool FT8GeoSpotList::on_encoder(const EncoderEvent delta) {
    if (!spots || spots->count == 0)
        return false;
    int sel = selected < 0 ? (int)spots->count - 1 : selected;
    sel += delta > 0 ? 1 : -1;
    if (sel < 0)
        sel = 0;
    if (sel >= (int)spots->count)
        sel = (int)spots->count - 1;
    selected = (int8_t)sel;
    if (selected < scroll)
        scroll = selected;
    if (selected >= scroll + ROWS)
        scroll = selected - ROWS + 1;
    set_dirty();
    if (on_change)
        on_change();
    return true;
}

bool FT8GeoSpotList::on_touch(const TouchEvent event) {
    if (event.type != TouchEvent::Type::Start || !spots)
        return false;
    const int row = (event.point.y() - screen_rect().top() - 8) / 8;
    const int idx = scroll + row;
    if (row < 0 || row >= ROWS || idx >= (int)spots->count)
        return false;
    selected = (int8_t)idx;
    set_dirty();
    if (on_change)
        on_change();
    return true;
}

/* Touch-drag panning: report the drag delta in screen pixels to the view,
 * which converts it to a new map center. Swallow Start/Move/End on the map so
 * touches don't leak to widgets behind it. */
bool FT8GeoMap::on_touch(const TouchEvent event) {
    switch (event.type) {
        case TouchEvent::Type::Start:
            pan_start_ = event.point;
            panning_ = true;
            return true;
        case TouchEvent::Type::Move:
            if (panning_ && on_pan) {
                on_pan(event.point.x() - pan_start_.x(), event.point.y() - pan_start_.y());
                pan_start_ = event.point;
            }
            return true;
        case TouchEvent::Type::End:
            panning_ = false;
            return true;
        default:
            return false;
    }
}

void FT8GeoView::focus() {
    field_frequency.focus();
}

FT8GeoView::~FT8GeoView() {
    receiver_model.disable();
    audio::output::stop();
    baseband::shutdown();
}

FT8GeoView::FT8GeoView(NavigationView& nav)
    : nav_{nav} {
    add_children({&field_rf_amp,
                  &field_lna,
                  &field_vga,
                  &rssi,
                  &channel,
                  &field_volume,
                  &field_frequency,
                  &options_band,
                  &text_status,
                  &text_decodes,
                  &button_map,
                  &console});

    channel.set_overload_threshold(-3);

    field_frequency.set_step(100);

    /* Default home QTH on first run (nothing saved yet): KM72. The user can
     * change it in the map view; SAVE persists it. */
    if (!qth_locator_)
        qth_locator_ = ((uint32_t)'K' << 24) | ((uint32_t)'M' << 16) |
                       ((uint32_t)'7' << 8) | (uint32_t)'2';

    button_map.on_select = [this](Button&) {
        nav_.push<FT8GeoMapView>(qth_locator_, [this]() {
            SettingBindings bindings{{"qth_locator"sv, &qth_locator_}};
            save_settings("rx_ft8geo", bindings);
        }, &spots_);
    };

    /* Going through the frequency field rather than the model keeps the two in step:
     * the field retunes the receiver itself and redraws with the new dial. */
    options_band.on_change = [this](size_t, OptionsField::value_t v) {
        if (v != 0)
            field_frequency.set_value(v);
    };
    field_frequency.updated = [this](rf::Frequency f) {
        options_band.set_by_value(f);
    };
    options_band.set_by_value(receiver_model.target_frequency());

    baseband::run_prepared_image(portapack::memory::map::m4_code.base());

    /* FT8 occupies 50 Hz per signal inside a 2.5 kHz sub-band, but the decoder needs the
     * whole sub-band at once, so the receiver runs wide and the channel filter narrows it.
     * set_modulation() is deliberately not called: it would replace the FT8 baseband image. */
    receiver_model.set_sampling_rate(sampling_rate);
    receiver_model.set_baseband_bandwidth(baseband_bandwidth);
    receiver_model.enable();

    audio::output::start();
}

void FT8GeoView::on_packet(const FT8PacketMessage* message) {
    if (message->text[0] == '\0')
        return;

    /* The message takes the left of the line and the audio frequency the right, so the
     * frequencies line up in a column down the screen and the passband can be read at a
     * glance. A message too long to leave room for the column keeps its text, and the
     * frequency moves down a row and stays in the column. */
    std::string text{message->text};
    const std::string freq = to_string_dec_uint(message->frequency) + "Hz";
    /* screen_width is set at runtime, so the column count is taken here rather than
     * fixed at compile time. The font is the 8 px fixed one the rest of the layout uses. */
    const size_t columns = screen_width / 8;

    /* Padding is measured before the colour escape goes on, because the escape occupies
     * two bytes of the string and no columns on screen. */
    if (text.length() + freq.length() < columns)
        text.append(columns - freq.length() - text.length(), ' ');
    else
        text += '\n' + std::string(columns - freq.length(), ' ');

    /* A CQ is the line an operator can answer, so its text is picked out of the list. The
     * frequency stays white: it reads as a column down the screen, and colouring it would
     * break that column into stripes. */
    const bool calling = std::string{message->text}.compare(0, 3, "CQ ") == 0;
    if (calling)
        console.writeln(STR_COLOR_GREEN + text + STR_COLOR_WHITE + freq);
    else
        console.writeln(text + freq);

    /* Plot the station on the map: pull the Maidenhead locator out of the
     * message and remember the spot. If the map is open it refreshes at once.
     * Colors follow the reference: CQ green, QSO yellow, 73 red. */
    char call[11];
    char grid[5];
    float lat, lon;
    if (parse_spot(message->text, call, grid, &lat, &lon)) {
        const size_t len = strlen(message->text);
        uint8_t type = SPOT_QSO;
        if (strncmp(message->text, "CQ", 2) == 0)
            type = SPOT_CQ;
        else if (len >= 2 && message->text[len - 1] == '3' && message->text[len - 2] == '7' &&
                 (len == 2 || message->text[len - 3] == ' '))
            type = SPOT_73;
        spots_.add(lat, lon, call, grid, type);
        if (active_map_view)
            active_map_view->refresh_spots();
    }
}

void FT8GeoView::on_status(const FT8RxStatusMessage* message) {
    decodes_total += message->decode_count;

    switch (message->state) {
        case FT8RxStatusMessage::SyncState::Searching:
            text_status.set_style(Theme::getInstance()->fg_red);
            text_status.set("Searching");
            break;
        case FT8RxStatusMessage::SyncState::Heard:
            text_status.set_style(Theme::getInstance()->fg_yellow);
            text_status.set("Heard, no decode");
            break;
        case FT8RxStatusMessage::SyncState::Syncing:
            text_status.set_style(Theme::getInstance()->fg_yellow);
            text_status.set("Syncing");
            break;
        case FT8RxStatusMessage::SyncState::Locked:
            text_status.set_style(Theme::getInstance()->fg_green);
            text_status.set("Locked");
            break;
    }

    if (decodes_total > 0) {
        text_decodes.set_style(Theme::getInstance()->fg_light);
        text_decodes.set(to_string_dec_uint(decodes_total) + " RX");
    }
}

/* FT8GeoMapView ************************************************************/

uint32_t FT8GeoMapView::packed_locator() const {
    return ((uint32_t)opt_f1.selected_index_value() << 24) |
           ((uint32_t)opt_f2.selected_index_value() << 16) |
           ((uint32_t)opt_d1.selected_index_value() << 8) |
           (uint32_t)opt_d2.selected_index_value();
}

void FT8GeoMapView::apply_locator() {
    // 4-char Maidenhead: field (20x10 deg) + square (2x1 deg), square center.
    const float lon = (opt_f1.selected_index_value() - 'A') * 20.0f +
                      (opt_d1.selected_index_value() - '0') * 2.0f - 179.0f;
    const float lat = (opt_f2.selected_index_value() - 'A') * 10.0f +
                      (opt_d2.selected_index_value() - '0') - 89.5f;
    home_lat_ = lat;
    home_lon_ = lon;
    center_lat_ = lat;
    center_lon_ = lon;
    spot_list.home_lat = lat;
    spot_list.home_lon = lon;
    spot_list.set_dirty();
    geomap.move(lon, lat);
    // The home dot is drawn by draw_overlay(); GeoMap's own my_pos marker
    // renders as a bearing triangle, which we don't want.
}

void FT8GeoMapView::refresh_spots() {
    if (!spots_)
        return;
    spot_list.home_lat = home_lat_;
    spot_list.home_lon = home_lon_;
    spot_list.follow_tail();  // jump to the newest spot
    spot_list.set_dirty();
    geomap.refresh();
}

void FT8GeoMapView::pan_map(int dx, int dy) {
    // Screen px -> map px depends on zoom; then map px -> degrees.
    // 360/2048 = 0.17578125 deg per map pixel of longitude; for latitude the
    // Mercator scale adds a cos(lat) factor (same constant, neat coincidence:
    // 180*2/2048). Drag direction follows the finger.
    const float zeff = zoom_cur_ > 0 ? (float)zoom_cur_ : 1.0f / (float)(-zoom_cur_);
    const float map_dx = dx / zeff;
    const float map_dy = dy / zeff;
    center_lon_ -= map_dx * 0.17578125f;
    center_lat_ += map_dy * cosf(center_lat_ * 0.0174532925f) * 0.17578125f;
    if (center_lon_ > 180.0f) center_lon_ = 180.0f;
    if (center_lon_ < -180.0f) center_lon_ = -180.0f;
    if (center_lat_ > 85.0f) center_lat_ = 85.0f;
    if (center_lat_ < -85.0f) center_lat_ = -85.0f;
    geomap.move(center_lon_, center_lat_);
    geomap.refresh();
}

static bool clip_line(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1, int w, int h);

/* Maidenhead field letters on the map edges, like the reference: A..R across
 * the top (20 deg columns) and down the left (10 deg rows). Only when zoomed
 * out — zoomed in they would clutter. */
void FT8GeoMapView::draw_grid_labels(Painter& painter, const ui::Rect& r) {
    if (zoom_cur_ > 1)
        return;
    char letter[2] = {'A', '\0'};
    for (int i = 0; i < 18; i++) {  // lon fields A..R, centered in each column
        const int16_t x = geomap.geo_to_pixel(center_lat_, -180.0f + i * 20.0f + 10.0f).x();
        if (x >= 4 && x < r.width() - 6) {
            letter[0] = 'A' + i;
            painter.draw_string({int16_t(r.left() + x - 2), int16_t(r.top() + 1)},
                                ui::font::fixed_5x8, ui::Color::white(), ui::Color::black(), letter);
        }
    }
    for (int j = 0; j < 18; j++) {  // lat fields A..R, centered in each row
        const int16_t y = geomap.geo_to_pixel(90.0f - j * 10.0f - 5.0f, center_lon_).y();
        if (y >= 12 && y < r.height() - 8) {
            letter[0] = 'A' + j;
            painter.draw_string({int16_t(r.left() + 1), int16_t(r.top() + y - 3)},
                                ui::font::fixed_5x8, ui::Color::white(), ui::Color::black(), letter);
        }
    }
}

void FT8GeoMapView::draw_overlay(Painter& painter) {
    if (!spots_)
        return;
    const auto r = geomap.screen_rect();
    const bool have_home = home_lat_ <= 90.0f;
    ui::Point home;
    if (have_home)
        home = geomap.geo_to_pixel(home_lat_, home_lon_);

    draw_grid_labels(painter, r);

    for (size_t i = 0; i < spots_->count; i++) {
        const FT8Spot& s = spots_->spot[i];
        const ui::Point dst = geomap.geo_to_pixel(s.lat, s.lon);
        if (dst.x() < 0 || dst.x() >= r.width() || dst.y() < 0 || dst.y() >= r.height())
            continue;  // off the visible map

        const bool sel = ((int8_t)i == spot_list.selected);
        const auto line_color = sel ? ui::Color::white() : spot_type_color(s.type);

        if (have_home) {
            int16_t x0 = home.x(), y0 = home.y(), x1 = dst.x(), y1 = dst.y();
            if (clip_line(x0, y0, x1, y1, r.width(), r.height())) {
                display.draw_line({x0 + r.left(), y0 + r.top()}, {x1 + r.left(), y1 + r.top()}, line_color);
                if (sel) {  // selected station: thicker line
                    int16_t x2 = x0, y2 = y0 + 1, x3 = x1, y3 = y1 + 1;
                    if (clip_line(x2, y2, x3, y3, r.width(), r.height()))
                        display.draw_line({x2 + r.left(), y2 + r.top()}, {x3 + r.left(), y3 + r.top()}, line_color);
                }
            }
        }

        const ui::Point sp{int16_t(dst.x() + r.left()), int16_t(dst.y() + r.top())};
        display.fill_rectangle({sp - Point(2, 2), {5, 5}}, sel ? ui::Color::white() : line_color);
        // No callsign tags on the map: with many stations they overlap and
        // become unreadable. The list below holds the text.
    }

    if (have_home && home.x() >= 0 && home.x() < r.width() && home.y() >= 0 && home.y() < r.height()) {
        const ui::Point hp{int16_t(home.x() + r.left()), int16_t(home.y() + r.top())};
        display.fill_rectangle({hp - Point(2, 2), {5, 5}}, Color::green());
    }
}

/* Cohen-Sutherland clip of a line to the map rect, so spot lines never run
 * over the buttons or the QTH bar. */
static bool clip_line(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1, int w, int h) {
    const auto code = [&](int16_t x, int16_t y) {
        return (x < 0 ? 1 : 0) | (x >= w ? 2 : 0) | (y < 0 ? 4 : 0) | (y >= h ? 8 : 0);
    };
    int c0 = code(x0, y0), c1 = code(x1, y1);
    while (true) {
        if (!(c0 | c1))
            return true;
        if (c0 & c1)
            return false;
        const int c = c0 ? c0 : c1;
        int32_t x, y;
        if (c & 8) {
            x = x0 + (int32_t)(x1 - x0) * (h - 1 - y0) / (y1 - y0);
            y = h - 1;
        } else if (c & 4) {
            x = x0 + (int32_t)(x1 - x0) * (0 - y0) / (y1 - y0);
            y = 0;
        } else if (c & 2) {
            y = y0 + (int32_t)(y1 - y0) * (w - 1 - x0) / (x1 - x0);
            x = w - 1;
        } else {
            y = y0 + (int32_t)(y1 - y0) * (0 - x0) / (x1 - x0);
            x = 0;
        }
        if (c == c0) {
            x0 = x;
            y0 = y;
            c0 = code(x0, y0);
        } else {
            x1 = x;
            y1 = y;
            c1 = code(x1, y1);
        }
    }
}

void FT8GeoMapView::step_zoom_preset(int dir) {
    // Zoom-in is a lower preset index (smaller view width).
    if (dir > 0 && preset_idx_ > 0)
        preset_idx_--;
    else if (dir < 0 && preset_idx_ < 6)
        preset_idx_++;
    apply_preset();
}

void FT8GeoMapView::apply_preset() {
    const int target = zoom_presets_[preset_idx_];

    /* Rank orders zoom levels from most zoomed-in to most zoomed-out and must
     * be strictly monotonic (10 - z is), or the loop below can step the wrong
     * way where GeoMap's own ladder jumps between positive and negative zooms.
     * The mirror of GeoMap::on_encoder's step rules must stay exact, or the
     * label and the map would drift apart. */
    const auto rank = [](int z) { return 10 - z; };
    while (zoom_cur_ != target) {
        if (rank(target) > rank(zoom_cur_)) {
            geomap.GeoMap::on_encoder(-1);
            zoom_cur_ = (zoom_cur_ == 1) ? -2 : (zoom_cur_ > 10 ? zoom_cur_ / 2 : zoom_cur_ - 1);
        } else {
            geomap.GeoMap::on_encoder(1);
            zoom_cur_ = (zoom_cur_ == -2) ? 1 : zoom_cur_ + (zoom_cur_ >= 10 ? zoom_cur_ : 1);
        }
    }

    // 2048 px map = 40075 km at the equator, 240 px screen: 4696 km wide at
    // zoom 1 on the equator, scaled by cos(latitude) and the zoom factor.
    float lat = 0.0f;
    if (qth_locator_)
        lat = (((qth_locator_ >> 16) & 0xFF) - 'A') * 10.0f +
              (((qth_locator_ >> 8) & 0xFF) - '0') - 89.5f;
    const float width_km = 4696.0f * cosf(lat * 0.0174532925f) *
                           (target > 0 ? 1.0f / target : (float)(-target));
    text_scale.set("VIEW ~" + to_string_dec_uint((uint32_t)(width_km + 0.5f)) + " KM");
}

FT8GeoMapView::FT8GeoMapView(NavigationView& nav, uint32_t& qth_locator, std::function<void()> on_save,
                             FT8GeoSpots* spots)
    : nav_(nav), qth_locator_(qth_locator), on_save_(std::move(on_save)), spots_(spots) {
    active_map_view = this;
    add_children({&text_qth,
                  &opt_f1,
                  &opt_f2,
                  &opt_d1,
                  &opt_d2,
                  &button_zoom_out,
                  &button_zoom_in,
                  &button_save,
                  &button_reset,
                  &geomap,
                  &spot_list,
                  &text_scale});

    spot_list.spots = spots_;
    spot_list.set_focusable(true);
    spot_list.on_change = [this]() { geomap.refresh(); };

    // Focusable so the user can arrow down to the map and zoom with the
    // rotary. Without ANY focusable widget holding focus, the FocusManager
    // stops dispatching keys entirely - the view feels frozen, no back key.
    geomap.set_focusable(true);

    // We draw our own home dot; the built-in center marker is a red triangle.
    geomap.set_hide_center_marker(true);

    // Set fields before wiring on_change, so restoring the saved locator does
    // not fire apply_locator() before the map is initialized.
    if (qth_locator_) {
        opt_f1.set_by_value((qth_locator_ >> 24) & 0xFF);
        opt_f2.set_by_value((qth_locator_ >> 16) & 0xFF);
        opt_d1.set_by_value((qth_locator_ >> 8) & 0xFF);
        opt_d2.set_by_value(qth_locator_ & 0xFF);
    }

    const auto on_locator = [this](size_t, OptionsField::value_t) {
        apply_locator();
        qth_locator_ = packed_locator();
        apply_preset();  // refresh the scale label for the new latitude
    };
    opt_f1.on_change = on_locator;
    opt_f2.on_change = on_locator;
    opt_d1.on_change = on_locator;
    opt_d2.on_change = on_locator;

    button_zoom_out.on_select = [this](Button&) { step_zoom_preset(-1); };
    button_zoom_in.on_select = [this](Button&) { step_zoom_preset(1); };
    geomap.on_zoom_step = [this](int dir) { step_zoom_preset(dir); };
    geomap.on_paint_overlay = [this](Painter& painter) { draw_overlay(painter); };
    geomap.on_pan = [this](int dx, int dy) { pan_map(dx, dy); };
    button_save.on_select = [this](Button&) {
        qth_locator_ = packed_locator();
        on_save_();
        nav_.display_modal("Saved", "QTH locator saved");
    };
    button_reset.on_select = [this](Button&) {
        spots_->clear();
        spot_list.selected = -1;
        spot_list.scroll = 0;
        spot_list.set_dirty();
        geomap.refresh();
    };

    // FT8-geo uses its own light map so the big hi-res world_map.bin stays
    // untouched for ADS-B.
    geomap.set_map_file(adsb_dir / u"ft8geo_map.bin");

    if (!geomap.init()) {
        nav_.display_modal("No map", "Put ft8geo_map.bin in\nthe /ADSB folder", ABORT);
        return;
    }

    if (qth_locator_)
        apply_locator();
    apply_preset();
    refresh_spots();
}

FT8GeoMapView::~FT8GeoMapView() {
    active_map_view = nullptr;
}

void FT8GeoMapView::focus() {
    opt_f1.focus();
}

}  // namespace ui::external_app::ft8geo
