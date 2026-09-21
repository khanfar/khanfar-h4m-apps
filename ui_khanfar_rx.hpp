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

#ifndef __UI_KHANFAR_RX_H__
#define __UI_KHANFAR_RX_H__

#include "receiver_model.hpp"

#include "ui_receiver.hpp"
#include "ui_freq_field.hpp"
#include "ui_spectrum.hpp"
#include "ui_record_view.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "tone_key.hpp"
#include "ui_btngrid.hpp"
#include "ui_menu.hpp"

namespace ui::external_app::khanfarrx {

/* HF band plan tables *******************************************************/

enum class ModKind : uint8_t {
    AM,
    USB,
    LSB,
    NFM,
    WFM,
};

struct BandDef {
    const char* name;
    const char* range;
    rf::Frequency low;
    rf::Frequency high;
    ModKind mod;
};

struct BandCategory {
    const char* title;
    const BandDef* bands;
    size_t count;
};

enum Category : uint8_t {
    CAT_ALL_HF = 0,
    CAT_BROADCAST,
    CAT_HAM_HF,
    CAT_HAM_VHF,
    CAT_AIR,
    CAT_MARINE,
    CAT_COUNT,
};

extern const BandCategory band_categories[CAT_COUNT];

Color color_for(ModKind mod);

class KhanfarRxView;

/* EIBI broadcast schedule database (SD card /EIBI/eibi.bin) ******************/

struct EibiBand {
    const char* name;
    uint32_t low_hz;
    uint32_t high_hz;
};

// Order must match BANDS[] in eibi/make_eibi_bin.py.
static constexpr size_t EIBI_BAND_COUNT = 10;
extern const EibiBand eibi_bands[EIBI_BAND_COUNT];

/* Options views - like the stock Audio RX app (analog_audio_app), but the
 * SPEC and APT (AMFM/FMAM wefax) modes were dropped: the app must fit the
 * 32 KB external-app SRAM region together with the PWFM baseband image.
 * Those modes remain available in the stock Audio app. */

class AMOptionsView : public View {
   public:
    AMOptionsView(KhanfarRxView* view, Rect parent_rect, const Style* style);

   private:
    Text label_config{
        {UI_POS_X(0), UI_POS_Y(0), UI_POS_WIDTH(2), UI_POS_HEIGHT(1)},
        "BW",
    };

    OptionsField options_config{
        {UI_POS_X(3), UI_POS_Y(0)},
        6,  // Max option length
        {
            // Using common messages from freqman_ui.cpp
        }};

    OptionsField zoom_config{
        {UI_POS_X_RIGHT(7), UI_POS_Y(0)},
        7,
        {{"ZOOM x1", 0},
         {"ZOOM x2", 6}}  // offset index AM modes array FIR filters.
    };
};

class NBFMOptionsView : public View {
   public:
    NBFMOptionsView(Rect parent_rect, const Style* style);

   private:
    Text label_config{
        {UI_POS_X(0), UI_POS_Y(0), UI_POS_WIDTH(2), UI_POS_HEIGHT(1)},
        "BW",
    };
    OptionsField options_config{
        {UI_POS_X(3), UI_POS_Y(0)},
        3,  // Max option length
        {
            // Using common messages from freqman_ui.cpp
        }};

    Text text_squelch{
        {UI_POS_X(7), UI_POS_Y(0), UI_POS_WIDTH(8), UI_POS_HEIGHT(1)},
        "SQ   /99"};
    NumberField field_squelch{
        {UI_POS_X(10), UI_POS_Y(0)},
        2,
        {0, 99},
        1,
        ' ',
    };
};

class WFMOptionsView : public View {
   public:
    WFMOptionsView(Rect parent_rect, const Style* style);

   private:
    Text label_config{
        {UI_POS_X(0), UI_POS_Y(0), UI_POS_WIDTH(2), UI_POS_HEIGHT(1)},
        "BW",
    };
    OptionsField options_config{
        {UI_POS_X(3), UI_POS_Y(0)},
        4,  // Max option length
        {
            // Using common messages from freqman_ui.cpp
        }};
};

/* Frequency options strip (Step / PPM) with the KhanfarHF band button added
 * in the free space between the step selector and the PPM field. */
class KhanfarFreqOptionsView : public FrequencyOptionsView {
   public:
    KhanfarFreqOptionsView(Rect parent_rect, const Style* style, KhanfarRxView* rx);

   private:
    KhanfarRxView* rx_;

    Button button_hf{
        {UI_POS_X(13), UI_POS_Y(0), UI_POS_WIDTH(9), UI_POS_HEIGHT(1)},
        "KhanfarHF"};
};

class KhanfarRxView : public View {
   public:
    KhanfarRxView(NavigationView& nav);
    KhanfarRxView(const KhanfarRxView&) = delete;
    KhanfarRxView& operator=(const KhanfarRxView&) = delete;
    ~KhanfarRxView();

    void set_parent_rect(Rect new_parent_rect) override;
    void focus() override;

    std::string title() const override { return "Khanfar RX"; };

    uint8_t get_zoom_factor();
    void set_zoom_factor(uint8_t zoom);

    uint8_t get_previous_AM_mode_option();
    void set_previous_AM_mode_option(uint8_t mode);

    uint8_t get_previous_zoom_option();
    void set_previous_zoom_option(uint8_t zoom);

    // HF band lock: tune freely inside [low, high] with wrap-around at the
    // edges; DisableHF returns to normal Audio RX behavior.
    bool hf_band_active() const { return hf_band_active_; }
    void disable_hf_band() { hf_band_active_ = false; }
    void on_hf_button();
    void on_hf_band_selected(BandDef band);
    void on_eibi_station_selected(rf::Frequency freq_hz);

   private:
    static constexpr ui::Dim header_height = 3 * 16;

    NavigationView& nav_;
    RxRadioState radio_state_{};
    uint8_t zoom_factor_am{0};            // initial zoom factor in AM mode
    uint8_t previous_AM_mode_option{0};   // GUI 5 AM modes :  (0..4 ) (DSB9K, DSB6K, USB,LSB, CW). Used to select proper FIR filter (0..11) AM mode  + offset 0 (zoom+1) or +6 (if zoom+2)
    uint8_t previous_zoom{0};             // GUI ZOOM+1, ZOOM+2 , equivalent to two values offset 0 (zoom+1) or +6 (if zoom+2)

    bool hf_band_active_{false};
    rf::Frequency hf_low_{0};
    rf::Frequency hf_high_{0};

    app_settings::SettingsManager settings_{
        "rx_audio",
        app_settings::Mode::RX,
        {
            {"zoom_factor_am"sv, &zoom_factor_am},                    // we are saving and restoring AM ZOOM factor from Settings.
            {"previous_AM_mode_option"sv, &previous_AM_mode_option},  // we are saving and restoring AMFM ZOOM factor from Settings.
            {"previous_zoom"sv, &previous_zoom},                      // we are saving and restoring AMFM ZOOM factor from Settings.
        }};

    const Rect options_view_rect{UI_POS_X(0), UI_POS_Y(1), UI_POS_MAXWIDTH, UI_POS_HEIGHT(1)};
    const Rect nbfm_view_rect{UI_POS_X(0), UI_POS_Y(1), UI_POS_WIDTH(18), UI_POS_HEIGHT(1)};

    RSSI rssi{
        {UI_POS_X(21), 0, UI_POS_WIDTH_REMAINING(21) - UI_POS_WIDTH(2), 4}};

    Channel channel{
        {UI_POS_X(21), 5, UI_POS_WIDTH_REMAINING(21) - UI_POS_WIDTH(2), 4}};

    Audio audio{
        {UI_POS_X(21), 10, UI_POS_WIDTH_REMAINING(21) - UI_POS_WIDTH(2), 4}};

    RxFrequencyField field_frequency{
        {UI_POS_X(5), UI_POS_Y(0)},
        nav_};

    LNAGainField field_lna{
        {UI_POS_X(15), UI_POS_Y(0)}};

    VGAGainField field_vga{
        {UI_POS_X(18), UI_POS_Y(0)}};

    OptionsField options_modulation{
        {UI_POS_X(0), UI_POS_Y(0)},
        4,
        {
            {" AM ", toUType(ReceiverModel::Mode::AMAudio)},
            {"NFM ", toUType(ReceiverModel::Mode::NarrowbandFMAudio)},
            {"WFM ", toUType(ReceiverModel::Mode::WidebandFMAudio)},
        }};

    AudioVolumeField field_volume{
        {screen_width - 2 * 8, UI_POS_Y(0)}};

    Text text_ctcss{
        {UI_POS_X(16), UI_POS_Y(1), UI_POS_WIDTH(14), UI_POS_HEIGHT(1)},
        ""};

    std::unique_ptr<Widget> options_widget{};

    RecordView record_view{
        {UI_POS_X(0), UI_POS_Y(2), UI_POS_MAXWIDTH, UI_POS_HEIGHT(1)},
        u"AUD",
        u"AUDIO",
        RecordView::FileType::WAV,
        4096,
        4};

    spectrum::WaterfallView waterfall{true};

    void on_baseband_bandwidth_changed(uint32_t bandwidth_hz);
    void on_modulation_changed(ReceiverModel::Mode modulation);
    void on_show_options_frequency();
    void on_show_options_rf_gain();
    void on_show_options_modulation();
    void on_frequency_step_changed(rf::Frequency f);
    void on_reference_ppm_correction_changed(int32_t v);

    void remove_options_widget();
    void set_options_widget(std::unique_ptr<Widget> new_widget);

    void update_modulation(ReceiverModel::Mode modulation);

    void handle_coded_squelch(uint32_t value);

    void on_freqchg(int64_t freq);

    MessageHandlerRegistration message_handler_coded_squelch{
        Message::ID::CodedSquelch,
        [this](const Message* p) {
            const auto message = *reinterpret_cast<const CodedSquelchMessage*>(p);
            this->handle_coded_squelch(message.value);
        }};

    MessageHandlerRegistration message_handler_freqchg{
        Message::ID::FreqChangeCommand,
        [this](Message* const p) {
            const auto message = static_cast<const FreqChangeCommandMessage*>(p);
            this->on_freqchg(message->freq);
        }};
};

/* Band plan selector: category grid (screen 1). */
class KhanfarBandCatView : public BtnGridView {
   public:
    KhanfarBandCatView(NavigationView& nav, KhanfarRxView* rx);
    KhanfarBandCatView(const KhanfarBandCatView&) = delete;
    KhanfarBandCatView& operator=(const KhanfarBandCatView&) = delete;

    std::string title() const override { return "HF BAND PLAN"; };

   private:
    NavigationView& nav_;
    KhanfarRxView* rx_;

    void on_populate() override;
};

/* Band plan selector: bands within a category (screen 2). */
class KhanfarBandListView : public View {
   public:
    KhanfarBandListView(NavigationView& nav, KhanfarRxView* rx, uint8_t category);
    KhanfarBandListView(const KhanfarBandListView&) = delete;
    KhanfarBandListView& operator=(const KhanfarBandListView&) = delete;

    void focus() override;

    std::string title() const override { return title_; };

   private:
    KhanfarRxView* rx_;
    std::string title_{};

    MenuView menu_view{
        {0, 0, screen_width, screen_height - 16},
        true};
};

/* EIBI: broadcast band picker (screen 1). */
class KhanfarEibiBandView : public BtnGridView {
   public:
    KhanfarEibiBandView(NavigationView& nav, KhanfarRxView* rx);
    KhanfarEibiBandView(const KhanfarEibiBandView&) = delete;
    KhanfarEibiBandView& operator=(const KhanfarEibiBandView&) = delete;

    std::string title() const override { return "EIBI BANDS"; };

   private:
    NavigationView& nav_;
    KhanfarRxView* rx_;

    void on_populate() override;
};

/* EIBI: stations on the air right now, one band (screen 2). */
class KhanfarEibiListView : public View {
   public:
    KhanfarEibiListView(NavigationView& nav, KhanfarRxView* rx, uint8_t band_index);
    KhanfarEibiListView(const KhanfarEibiListView&) = delete;
    KhanfarEibiListView& operator=(const KhanfarEibiListView&) = delete;

    void focus() override;

    std::string title() const override { return title_; };

   private:
    KhanfarRxView* rx_;
    std::string title_{};

    Text text_info{
        {0, 0, screen_width, 16},
        ""};

    MenuView menu_view{
        {0, 16, screen_width, screen_height - 32},
        true};
};

}  // namespace ui::external_app::khanfarrx

#endif /*__UI_KHANFAR_RX_H__*/
