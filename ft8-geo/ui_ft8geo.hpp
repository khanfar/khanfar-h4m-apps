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

#ifndef __UI_FT8GEO_H__
#define __UI_FT8GEO_H__

#include "app_settings.hpp"
#include "message.hpp"
#include "radio_state.hpp"
#include "receiver_model.hpp"
#include "ui.hpp"
#include "ui_channel.hpp"
#include "ui_freq_field.hpp"
#include "ui_geomap.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_rssi.hpp"
#include "ui_widget.hpp"

namespace ui::external_app::ft8geo {

/* Spot type by message content, colors follow the reference design:
 * CQ = green, QSO exchange = yellow, 73 = red. */
enum FT8SpotType : uint8_t {
    SPOT_CQ = 0,
    SPOT_QSO = 1,
    SPOT_73 = 2,
};

/* One plotted station: position decoded from its Maidenhead locator, plus its
 * callsign and grid for the map tag and the last-heard list. */
struct FT8Spot {
    float lat{0};
    float lon{0};
    char call[11]{};
    char grid[5]{};
    uint8_t type{SPOT_QSO};
};

/* Fixed-size session store shared by the decoder view (writes on each decode)
 * and the map view (reads). No heap, no std::string: safe for the H4M's
 * small RAM. Dedupes by callsign; overwrites the oldest entry when full. */
struct FT8GeoSpots {
    static constexpr size_t MAX_SPOTS = 24;
    FT8Spot spot[MAX_SPOTS]{};
    size_t count{0};
    size_t oldest{0};

    void add(float lat, float lon, const char* call, const char* grid, uint8_t type);
    void clear() {
        count = 0;
        oldest = 0;
    }
};

/* GeoMap subclass that routes rotary zoom through the view's preset ladder, so
 * the map can never be zoomed off the presets and the on-screen scale label
 * stays in sync. Buttons call the view directly; this catches the rotary.
 * Touch drags are reported to the view for panning, and the view draws the
 * spot overlay (lines, dots, tags) on top of the map. */
class FT8GeoMap : public ui::GeoMap {
   public:
    using ui::GeoMap::GeoMap;
    std::function<void(int)> on_zoom_step{};
    std::function<void(Painter&)> on_paint_overlay{};
    std::function<void(int, int)> on_pan{};  // drag delta in screen pixels

    bool on_encoder(const EncoderEvent d) override {
        if (on_zoom_step && d != 0) {
            on_zoom_step(d > 0 ? 1 : -1);
            return true;
        }
        return false;
    }

    bool on_touch(const TouchEvent event) override;

    void paint(Painter& painter) override {
        ui::GeoMap::paint(painter);
        if (on_paint_overlay)
            on_paint_overlay(painter);
    }

   private:
    ui::Point pan_start_{};
    bool panning_{false};
};

/* The last-heard list under the map: header with the total, then 4 rows in
 * the small 5x8 font, newest at the bottom. Rotary moves the selection when
 * focused (auto-scrolls), touch selects a row directly. The selected station
 * is highlighted on the map via on_change. */
class FT8GeoSpotList : public Widget {
   public:
    FT8GeoSpotList(Rect parent_rect)
        : Widget{parent_rect} {
    }

    void paint(Painter& painter) override;
    bool on_encoder(const EncoderEvent delta) override;
    bool on_touch(const TouchEvent event) override;

    FT8GeoSpots* spots{nullptr};
    int8_t selected{-1};
    int8_t scroll{0};  // index of the top visible row
    float home_lat{200.0f};
    float home_lon{0.0f};
    std::function<void()> on_change{};

    /* Follow the newest spot (called when a spot was added). */
    void follow_tail();

   private:
    static constexpr int8_t ROWS = 5;
};

/* World map popup: built-in GeoMap widget (reads its own light map
 * /ADSB/ft8geo_map.bin, leaving world_map.bin for ADS-B), centered on the
 * user's home QTH. The 4-character Maidenhead locator (e.g. KM72) is edited
 * with the 4 option fields and stored packed as 4 chars in a uint32_t
 * (0 = unset), persisted via app settings. */
class FT8GeoMapView : public View {
   public:
    FT8GeoMapView(NavigationView& nav, uint32_t& qth_locator, std::function<void()> on_save,
                  FT8GeoSpots* spots);
    ~FT8GeoMapView();
    void focus() override;
    std::string title() const override { return "FT8-geo MAP"; }

    /* Rebuild the map markers from the spot store and repaint. Called by the
     * decoder view whenever a new station was decoded while the map is open. */
    void refresh_spots();

   private:
    NavigationView& nav_;
    uint32_t& qth_locator_;
    std::function<void()> on_save_;
    FT8GeoSpots* spots_;  // owned by the decoder view underneath

    /* Home position and current map center in degrees (center moves when the
     * user touch-drags the map). home 200 = unset (same as INVALID_LAT_LON). */
    float home_lat_{200.0f};
    float home_lon_{200.0f};
    float center_lat_{2.2f};  // middle of the 2048px map file (pixel 1024,1024)
    float center_lon_{0.0f};

    Text text_qth{
        {0, 2, 4 * 8, 16},
        "QTH"};

    OptionsField opt_f1{
        {5 * 8, 0},
        1,
        {{"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"F", 'F'},
         {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
         {"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}}};

    OptionsField opt_f2{
        {7 * 8, 0},
        1,
        {{"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"F", 'F'},
         {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
         {"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}}};

    OptionsField opt_d1{
        {9 * 8, 0},
        1,
        {{"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
         {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'}}};

    OptionsField opt_d2{
        {11 * 8, 0},
        1,
        {{"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
         {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'}}};

    Button button_zoom_out{
        {13 * 8, 0, 3 * 8, 16},
        "Z-"};

    Button button_zoom_in{
        {16 * 8, 0, 3 * 8, 16},
        "Z+"};

    Button button_save{
        {20 * 8, 0, 6 * 8, 16},
        "SAVE"};

    Button button_reset{
        {26 * 8, 0, 4 * 8, 16},
        "RST"};

    FT8GeoMap geomap{
        {0, 16, screen_width, screen_height - 96}};

    /* Last-heard list under the map: total header + 5 rows of 5x8 font.
     * Newest at the bottom, rotary scrolls, touch selects. */
    FT8GeoSpotList spot_list{
        {0, screen_height - 80, screen_width, 48}};

    Text text_scale{
        {0, screen_height - 32, screen_width, 16},
        ""};

    /* Zoom presets as GeoMap map_zoom values for the light 2048x2048 map:
     * 10, 5, 2, 1, -2, -4, -9 give roughly 400 / 800 / 2000 / 4000 / 8000 /
     * 16000 km and the whole world at 32N. 10 is the widget's map resolution
     * limit (beyond it the bitmap is hidden) and -9 shows the full 2048 px
     * width on the 240 px screen. */
    static constexpr int zoom_presets_[7] = {10, 5, 2, 1, -2, -4, -9};
    uint8_t preset_idx_{5};  // start one step in from the whole-world view
    int zoom_cur_{1};        // mirror of GeoMap's private map_zoom

    uint32_t packed_locator() const;
    void apply_locator();
    void step_zoom_preset(int dir);
    void apply_preset();
    void pan_map(int dx, int dy);
    void draw_overlay(Painter& painter);
    void draw_grid_labels(Painter& painter, const ui::Rect& r);
};

class FT8GeoView : public View {
   public:
    FT8GeoView(NavigationView& nav);
    ~FT8GeoView();

    void focus() override;
    std::string title() const override { return "FT8-geo"; }

   private:
    /* The decoder needs the whole 2.5 kHz FT8 sub-band, which is why the receiver runs at
     * a wide sampling rate and the baseband narrows it rather than the radio. */
    static constexpr uint32_t sampling_rate = 3072000;
    static constexpr uint32_t baseband_bandwidth = 1750000;

    NavigationView& nav_;

    RxRadioState radio_state_{
        7074000 /* 40 m FT8 calling frequency */,
        baseband_bandwidth,
        sampling_rate};

    app_settings::SettingsManager settings_{
        "rx_ft8geo",
        app_settings::Mode::RX,
        {{"qth_locator"sv, &qth_locator_}}};

    uint32_t qth_locator_{0};  // packed 4-char Maidenhead locator, 0 = unset

    /* Session spot store: every decoded station whose message carries a
     * locator lands here and is drawn on the map (cross + callsign tag, plus
     * a line from home). Fixed size, no heap. */
    FT8GeoSpots spots_{};

    void on_packet(const FT8PacketMessage* message);
    void on_status(const FT8RxStatusMessage* message);

    RFAmpField field_rf_amp{
        {11 * 8, 0}};

    LNAGainField field_lna{
        {13 * 8, 0}};

    VGAGainField field_vga{
        {16 * 8, 0}};

    RSSI rssi{
        {19 * 8 - 4, 0, 74, 4}};

    /* Channel power, in the pair the other receivers draw. This app needs it more than
     * most: there is no AGC in front of the decoder, and too much gain drives waterfall
     * bins to the top of the byte scale, where ft8_sync_score, built from differences
     * between neighbouring bins, collapses. The overload threshold turns the bar red
     * before that happens. */
    Channel channel{
        {19 * 8 - 4, 5, 74, 4}};

    AudioVolumeField field_volume{
        {UI_POS_X_RIGHT(2), 0}};

    RxFrequencyField field_frequency{
        {0, 0},
        nav_};

    /* The standard FT8 dial frequencies. Picking one retunes the receiver; the frequency
     * field above stays usable for anything else, and simply shows no band once the dial
     * no longer matches an entry. */
    OptionsField options_band{
        {0, 1 * 16},
        3,
        /* set_by_value falls back to the first entry when the dial matches nothing, so
         * that entry has to mean "no band" rather than 160 m, or tuning by hand would
         * snap the receiver onto the first preset. */
        {{"---", 0},
         {"160", 1840000},
         {"80m", 3573000},
         {"60m", 5357000},
         {"40m", 7074000},
         {"30m", 10136000},
         {"20m", 14074000},
         {"17m", 18100000},
         {"15m", 21074000},
         {"12m", 24915000},
         {"10m", 28074000},
         {"6m ", 50313000}}};

    /* State on the left, count on the right with a gap between them: the count sits
     * directly above the decoded-message column, and run together they read as one line. */
    Text text_status{
        {4 * 8, 1 * 16, 17 * 8, 16},
        "Searching"};

    Text text_decodes{
        {22 * 8, 1 * 16, 8 * 8, 16},
        ""};

    /* Map button row: opens the world map popup centered on the home QTH. */
    Button button_map{
        {22 * 8, 2 * 16, 8 * 8, 16},
        "Map"};

    /* NavigationView sits below the 16 px system status bar, so a view's own y = 0 is
     * screen y = 16 and the last usable row is screen_height - 16. Console hands its
     * screen rect straight to the display's hardware scroll region, so a rect that runs
     * past that row sets the region's bottom fixed area negative and the panel then shows
     * frame rows nothing has written. */
    Console console{
        {0, 3 * 16, screen_width, screen_height - 4 * 16}};

    uint32_t decodes_total{0};

    MessageHandlerRegistration message_handler_packet{
        Message::ID::FT8Packet,
        [this](const Message* const p) {
            this->on_packet(static_cast<const FT8PacketMessage*>(p));
        }};

    MessageHandlerRegistration message_handler_status{
        Message::ID::FT8RxStatus,
        [this](const Message* const p) {
            this->on_status(static_cast<const FT8RxStatusMessage*>(p));
        }};
};

}  // namespace ui::external_app::ft8geo

#endif  // __UI_FT8GEO_H__
