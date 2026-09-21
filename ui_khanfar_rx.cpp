/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2018 Furrtek
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

#include "ui_khanfar_rx.hpp"

#include "audio.hpp"
#include "baseband_api.hpp"
#include "file.hpp"
#include "portapack.hpp"
#include "portapack_persistent_memory.hpp"
#include "rtc_time.hpp"
#include "string_format.hpp"
#include "ui_freqman.hpp"
#include "utility.hpp"
#include "radio.hpp"

using namespace portapack;
using namespace tonekey;

namespace ui::external_app::khanfarrx {

/* HF band plan tables *******************************************************/

static const BandDef bands_all_hf[] = {
    {"ALL HF", "0.1-30 MHz", 100'000, 30'000'000, ModKind::AM},
};

static const BandDef bands_broadcast[] = {
    {"FMEU", "87.5-108.0 MHz", 87'500'000, 108'000'000, ModKind::WFM},
    {"FMUS", "88.1-107.9 MHz", 88'100'000, 107'900'000, ModKind::WFM},
    {"LW", "153-279 kHz", 153'000, 279'000, ModKind::AM},
    {"MWEU", "522-1710 kHz", 522'000, 1'710'000, ModKind::AM},
    {"MWUS", "520-1720 kHz", 520'000, 1'720'000, ModKind::AM},
    {"120m", "2300-2498 kHz", 2'300'000, 2'498'000, ModKind::AM},
    {"90m", "3200-3400 kHz", 3'200'000, 3'400'000, ModKind::AM},
    {"75m", "3900-4000 kHz", 3'900'000, 4'000'000, ModKind::AM},
    {"60m", "4750-5060 kHz", 4'750'000, 5'060'000, ModKind::AM},
    {"49m", "5875-6200 kHz", 5'875'000, 6'200'000, ModKind::AM},
    {"41m", "7200-7450 kHz", 7'200'000, 7'450'000, ModKind::AM},
    {"31m", "9400-9900 kHz", 9'400'000, 9'900'000, ModKind::AM},
    {"25m", "11600-12100 kHz", 11'600'000, 12'100'000, ModKind::AM},
    {"22m", "13570-13870 kHz", 13'570'000, 13'870'000, ModKind::AM},
    {"19m", "15100-15800 kHz", 15'100'000, 15'800'000, ModKind::AM},
    {"16m", "17480-17900 kHz", 17'480'000, 17'900'000, ModKind::AM},
    {"15m", "18900-19020 kHz", 18'900'000, 19'020'000, ModKind::AM},
    {"13m", "21450-21850 kHz", 21'450'000, 21'850'000, ModKind::AM},
    {"11m", "25670-26100 kHz", 25'670'000, 26'100'000, ModKind::AM},
};

static const BandDef bands_ham_hf[] = {
    {"160m", "1830-1850 kHz", 1'830'000, 1'850'000, ModKind::LSB},
    {"80m", "3500-3800 kHz", 3'500'000, 3'800'000, ModKind::LSB},
    {"60m", "5350-5367 kHz", 5'350'000, 5'367'000, ModKind::LSB},
    {"40m", "7000-7200 kHz", 7'000'000, 7'200'000, ModKind::LSB},
    {"30m", "10000-10150 kHz", 10'000'000, 10'150'000, ModKind::USB},
    {"20m", "14000-14350 kHz", 14'000'000, 14'350'000, ModKind::USB},
    {"17m", "18068-18168 kHz", 18'068'000, 18'168'000, ModKind::USB},
    {"15m", "21000-21450 kHz", 21'000'000, 21'450'000, ModKind::USB},
    {"12m", "24890-24990 kHz", 24'890'000, 24'990'000, ModKind::USB},
    {"10m", "28000-29700 kHz", 28'000'000, 29'700'000, ModKind::USB},
    {"CB", "26965-27405 kHz", 26'965'000, 27'405'000, ModKind::AM},
};

static const BandDef bands_ham_vhf[] = {
    {"2m", "144-146 MHz", 144'000'000, 146'000'000, ModKind::NFM},
    {"70cm", "430-440 MHz", 430'000'000, 440'000'000, ModKind::NFM},
};

static const BandDef bands_air[] = {
    {"AIRBAND", "118-137 MHz", 118'000'000, 137'000'000, ModKind::AM},
};

static const BandDef bands_marine[] = {
    {"MARINE VHF", "156-162 MHz", 156'000'000, 162'000'000, ModKind::NFM},
};

const BandCategory band_categories[CAT_COUNT] = {
    {"ALL HF", bands_all_hf, sizeof(bands_all_hf) / sizeof(BandDef)},
    {"BROADCAST BANDS", bands_broadcast, sizeof(bands_broadcast) / sizeof(BandDef)},
    {"HAM HF BANDS", bands_ham_hf, sizeof(bands_ham_hf) / sizeof(BandDef)},
    {"HAM VHF BANDS", bands_ham_vhf, sizeof(bands_ham_vhf) / sizeof(BandDef)},
    {"AIR BAND", bands_air, sizeof(bands_air) / sizeof(BandDef)},
    {"MARINE BAND", bands_marine, sizeof(bands_marine) / sizeof(BandDef)},
};

Color color_for(ModKind mod) {
    switch (mod) {
        case ModKind::NFM:
            return Color::blue();
        case ModKind::WFM:
            return Color::yellow();
        case ModKind::USB:
        case ModKind::LSB:
            return Color::cyan();
        default:
            return Color::green();  // AM
    }
}

/* Tiny char-buffer formatters. std::string operator+ chains and snprintf cost
 * too much code for the 32 KB external-app region. */
static char* append_str(char* p, const char* s) {
    while (*s)
        *p++ = *s++;
    return p;
}

static char* append_uint(char* p, uint32_t v, uint8_t width, char fill) {
    char tmp[10];
    uint8_t n = 0;
    do {
        tmp[n++] = '0' + (v % 10);
        v /= 10;
    } while (v && n < sizeof(tmp));
    while (n < width)
        tmp[n++] = fill;
    while (n)
        *p++ = tmp[--n];
    return p;
}

/* EIBI broadcast bands - order must match BANDS[] in eibi/make_eibi_bin.py. */
const EibiBand eibi_bands[EIBI_BAND_COUNT] = {
    {"75m", 3'900'000, 4'000'000},
    {"60m", 4'750'000, 5'060'000},
    {"49m", 5'900'000, 6'200'000},
    {"41m", 7'200'000, 7'450'000},
    {"31m", 9'400'000, 9'900'000},
    {"25m", 11'600'000, 12'100'000},
    {"22m", 13'570'000, 13'870'000},
    {"19m", 15'100'000, 15'800'000},
    {"16m", 17'480'000, 17'900'000},
    {"13m", 21'450'000, 21'850'000},
};

/* On-disk record of /EIBI/eibi.bin (see eibi/make_eibi_bin.py). */
struct __attribute__((packed)) EibiRecord {
    uint32_t freq_hz;
    uint16_t start_min;  // UTC
    uint16_t stop_min;   // UTC, may be < start_min (overnight)
    uint8_t days;        // bit0=Mon .. bit6=Sun, 0x7F = daily
    uint8_t band;        // index into eibi_bands[]
    char station[20];    // ASCII, NUL-padded
    char lang[4];        // ASCII, NUL-padded
};
static_assert(sizeof(EibiRecord) == 34, "EIBI record layout changed");

/* AMOptionsView *********************************************************/

AMOptionsView::AMOptionsView(
    KhanfarRxView* view,
    Rect parent_rect,
    const Style* style)
    : View{parent_rect} {
    set_style(style);

    add_children({
        &label_config,
        &options_config,
        &zoom_config,
    });

    zoom_config.on_change = [this, view](size_t, OptionsField::value_t n) {            // n , has two option values. when GUI =zoom+1 => (0), when GUI=zoom+2 (6)
        receiver_model.set_am_configuration(view->get_previous_AM_mode_option() + n);  // n (0 or 6)
        view->set_zoom_factor(n);
        view->set_previous_zoom_option(n);
    };

    // restore zoom selection
    zoom_config.set_by_value(view->get_zoom_factor());

    freqman_set_bandwidth_option(AM_MODULATION, options_config);                                        // freqman.cpp to the options_config, only allowing 5 modes  freqman_bandwidths[AM]  {"DSB 9k", 0},  {"DSB 6k", 1},  {"USB+3k", 2}, {"LSB-3k", 3}, {"CW", 4},
    options_config.set_by_value(receiver_model.am_configuration() - view->get_previous_zoom_option());  // restore AM GUI option mode ,   AM FIR index filters (0..11) values ,  <baseband::AMConfig, 12> am_configs has 12 fir  index elements.
    options_config.on_change = [this, view](size_t, OptionsField::value_t n) {
        receiver_model.set_am_configuration(n + view->get_previous_zoom_option());  // we select proper FIR AM filter (0..11), = 0..4 GUI AM modes + offset +6 (if zoom+2)
        view->set_previous_AM_mode_option(n);                                       // (0..4) allowing 5 AM modes (DSB9K, DSB6K, USB,LSB, CW)
    };
}

/* NBFMOptionsView *******************************************************/

NBFMOptionsView::NBFMOptionsView(
    Rect parent_rect,
    const Style* style)
    : View{parent_rect} {
    set_style(style);

    add_children({&label_config,
                  &options_config,
                  &text_squelch,
                  &field_squelch});

    freqman_set_bandwidth_option(NFM_MODULATION, options_config);  // adding the common message from freqman.cpp to the options_config
    options_config.set_by_value(receiver_model.nbfm_configuration());
    options_config.on_change = [this](size_t, OptionsField::value_t n) {
        receiver_model.set_nbfm_configuration(n);
    };

    field_squelch.set_value(receiver_model.squelch_level());
    field_squelch.on_change = [this](int32_t v) {
        receiver_model.set_squelch_level(v);
    };
}

/* WFMOptionsView *******************************************************/

WFMOptionsView::WFMOptionsView(
    Rect parent_rect,
    const Style* style)
    : View{parent_rect} {
    set_style(style);

    add_children({
        &label_config,
        &options_config,
    });

    freqman_set_bandwidth_option(WFM_MODULATION, options_config);  // adding the common message from freqman.cpp to the options_config
    options_config.set_by_value(receiver_model.wfm_configuration());
    options_config.on_change = [this](size_t, OptionsField::value_t n) {
        receiver_model.set_wfm_configuration(n);
    };
}

/* KhanfarFreqOptionsView ****************************************************/

KhanfarFreqOptionsView::KhanfarFreqOptionsView(
    Rect parent_rect,
    const Style* style,
    KhanfarRxView* rx)
    : FrequencyOptionsView{parent_rect, style},
      rx_{rx} {
    add_child(&button_hf);

    button_hf.set_text(rx_->hf_band_active() ? "DisableHF" : "KhanfarHF");
    button_hf.on_select = [this](Button&) {
        if (rx_->hf_band_active()) {
            // Disable the HF lock in place. The strip must NOT be rebuilt from
            // its own button callback - set_options_widget() would delete this
            // view while its event handler is still running (use-after-free).
            rx_->disable_hf_band();
            button_hf.set_text("KhanfarHF");
        } else {
            rx_->on_hf_button();  // opens the band picker
        }
    };
}

/* KhanfarRxView *******************************************************/

KhanfarRxView::KhanfarRxView(
    NavigationView& nav)
    : nav_(nav) {
    // A baseband image _must_ be running before add waterfall view.
    baseband::run_image(portapack::spi_flash::image_tag_wideband_spectrum);

    add_children({&rssi,
                  &channel,
                  &audio,
                  &field_frequency,
                  &field_lna,
                  &field_vga,
                  &options_modulation,
                  &field_volume,
                  &text_ctcss,
                  &record_view,
                  &waterfall});

    // HF band lock: wrap around at the band edges instead of tuning out of
    // the band. The wrapping set_value() re-enters this handler once with
    // the in-range value, which then does nothing.
    field_frequency.updated = [this](rf::Frequency f) {
        if (!hf_band_active_)
            return;
        if (f > hf_high_)
            field_frequency.set_value(hf_low_);
        else if (f < hf_low_)
            field_frequency.set_value(hf_high_);
    };

    // Filename Datetime and Frequency
    record_view.set_filename_date_frequency(true);

    field_frequency.on_show_options = [this]() {
        this->on_show_options_frequency();
    };

    field_lna.on_show_options = [this]() {
        this->on_show_options_rf_gain();
    };

    field_vga.on_show_options = [this]() {
        this->on_show_options_rf_gain();
    };

    auto modulation = receiver_model.modulation();

    // This app handles only AM / NFM / WFM (SPEC and APT modes were dropped
    // to fit the 32 KB app region). Map anything else - e.g. a Capture or
    // SPEC mode persisted by another receiver app - to AM.
    if (modulation != ReceiverModel::Mode::AMAudio &&
        modulation != ReceiverModel::Mode::NarrowbandFMAudio &&
        modulation != ReceiverModel::Mode::WidebandFMAudio)
        modulation = ReceiverModel::Mode::AMAudio;

    options_modulation.set_by_value(toUType(modulation));
    options_modulation.on_change = [this](size_t, OptionsField::value_t v) {
        this->on_modulation_changed(static_cast<ReceiverModel::Mode>(v));
    };
    options_modulation.on_show_options = [this]() {
        this->on_show_options_modulation();
    };

    record_view.on_error = [&nav](std::string message) {
        nav.display_modal("Error", message);
    };

    waterfall.on_select = [this](int32_t offset) {
        field_frequency.set_value(receiver_model.target_frequency() + offset);
    };

    audio::output::start();

    // This call starts the correct baseband image to run
    // and sets the radio up as necessary for the given modulation.
    on_modulation_changed(modulation);
}

KhanfarRxView::~KhanfarRxView() {
    receiver_model.set_hidden_offset(0);
    audio::output::stop();
    receiver_model.disable();
    baseband::shutdown();
}

void KhanfarRxView::set_parent_rect(Rect new_parent_rect) {
    View::set_parent_rect(new_parent_rect);

    ui::Rect waterfall_rect{0, header_height, new_parent_rect.width(), new_parent_rect.height() - header_height};
    waterfall.set_parent_rect(waterfall_rect);
}

void KhanfarRxView::focus() {
    field_frequency.focus();
}

void KhanfarRxView::on_baseband_bandwidth_changed(uint32_t bandwidth_hz) {
    receiver_model.set_baseband_bandwidth(bandwidth_hz);
}

uint8_t KhanfarRxView::get_zoom_factor() {
    return zoom_factor_am;
}

void KhanfarRxView::set_zoom_factor(uint8_t zoom) {
    zoom_factor_am = zoom;
}

uint8_t KhanfarRxView::get_previous_AM_mode_option() {
    return previous_AM_mode_option;
}

void KhanfarRxView::set_previous_AM_mode_option(uint8_t mode) {
    previous_AM_mode_option = mode;
}

uint8_t KhanfarRxView::get_previous_zoom_option() {
    return previous_zoom;
}

void KhanfarRxView::set_previous_zoom_option(uint8_t zoom) {
    previous_zoom = zoom;
}

void KhanfarRxView::on_modulation_changed(ReceiverModel::Mode modulation) {
    baseband::spectrum_streaming_stop();
    update_modulation(modulation);
    on_show_options_modulation();
    baseband::spectrum_streaming_start();
}

void KhanfarRxView::remove_options_widget() {
    if (options_widget) {
        remove_child(options_widget.get());
        options_widget.reset();
    }

    field_lna.set_style(nullptr);
    options_modulation.set_style(nullptr);
    field_frequency.set_style(nullptr);
}

void KhanfarRxView::set_options_widget(std::unique_ptr<Widget> new_widget) {
    remove_options_widget();

    if (new_widget) {
        options_widget = std::move(new_widget);
    } else {
        // TODO: Lame hack to hide options view due to my bad paint/damage algorithm.
        options_widget = std::make_unique<Rectangle>(options_view_rect, Theme::getInstance()->option_active->background);
    }
    add_child(options_widget.get());
}

void KhanfarRxView::on_show_options_frequency() {
    auto widget = std::make_unique<KhanfarFreqOptionsView>(options_view_rect, Theme::getInstance()->option_active, this);

    widget->set_step(receiver_model.frequency_step());
    widget->on_change_step = [this](rf::Frequency f) {
        this->on_frequency_step_changed(f);
    };
    widget->set_reference_ppm_correction(persistent_memory::correction_ppb() / 1000);
    widget->on_change_reference_ppm_correction = [this](int32_t v) {
        this->on_reference_ppm_correction_changed(v);
    };

    set_options_widget(std::move(widget));
    field_frequency.set_style(Theme::getInstance()->option_active);
}

void KhanfarRxView::on_show_options_rf_gain() {
    auto widget = std::make_unique<RadioGainOptionsView>(options_view_rect, Theme::getInstance()->option_active);

    set_options_widget(std::move(widget));
    field_lna.set_style(Theme::getInstance()->option_active);
}

void KhanfarRxView::on_show_options_modulation() {
    std::unique_ptr<Widget> widget;

    const auto modulation = receiver_model.modulation();
    switch (modulation) {
        case ReceiverModel::Mode::AMAudio:
            widget = std::make_unique<AMOptionsView>(this, options_view_rect, Theme::getInstance()->option_active);
            waterfall.show_audio_spectrum_view(false);
            text_ctcss.hidden(true);
            break;

        case ReceiverModel::Mode::NarrowbandFMAudio:
            widget = std::make_unique<NBFMOptionsView>(nbfm_view_rect, Theme::getInstance()->option_active);
            waterfall.show_audio_spectrum_view(false);
            text_ctcss.hidden(false);
            break;

        case ReceiverModel::Mode::WidebandFMAudio:
            widget = std::make_unique<WFMOptionsView>(options_view_rect, Theme::getInstance()->option_active);
            waterfall.show_audio_spectrum_view(true);
            text_ctcss.hidden(true);
            break;

        default:
            chDbgPanic("Unhandled Mode");
            break;
    }

    set_options_widget(std::move(widget));
    options_modulation.set_style(Theme::getInstance()->option_active);
}

void KhanfarRxView::on_frequency_step_changed(rf::Frequency f) {
    receiver_model.set_frequency_step(f);
    field_frequency.set_step(f);
}

void KhanfarRxView::on_reference_ppm_correction_changed(int32_t v) {
    persistent_memory::set_correction_ppb(v * 1000);
}

void KhanfarRxView::update_modulation(ReceiverModel::Mode modulation) {
    audio::output::mute();
    record_view.stop();

    baseband::shutdown();

    portapack::spi_flash::image_tag_t image_tag;
    size_t sampling_rate;
    switch (modulation) {
        case ReceiverModel::Mode::NarrowbandFMAudio:
            image_tag = portapack::spi_flash::image_tag_nfm_audio;
            sampling_rate = 24000;
            break;
        case ReceiverModel::Mode::WidebandFMAudio:
            image_tag = portapack::spi_flash::image_tag_wfm_audio;
            sampling_rate = 48000;
            break;
        default:  // AMAudio
            image_tag = portapack::spi_flash::image_tag_am_audio;
            sampling_rate = 12000;
            break;
    }

    baseband::run_image(image_tag);

    receiver_model.set_modulation(modulation);

    receiver_model.set_sampling_rate(3072000);
    receiver_model.set_baseband_bandwidth(1750000);

    receiver_model.set_hidden_offset(0);

    receiver_model.enable();

    record_view.set_sampling_rate(sampling_rate);

    audio::output::unmute();
}

void KhanfarRxView::handle_coded_squelch(uint32_t value) {
    text_ctcss.set(tone_key_string_by_value(value, text_ctcss.parent_rect().width() / 8));
}

void KhanfarRxView::on_freqchg(int64_t freq) {
    field_frequency.set_value(freq);
}

/* HF band lock **************************************************************/

void KhanfarRxView::on_hf_button() {
    // Only the enable path arrives here (disabling is handled in place by
    // KhanfarFreqOptionsView, which must not rebuild itself from its own
    // button callback).
    nav_.push<KhanfarBandCatView>(this);
}

void KhanfarRxView::on_hf_band_selected(BandDef band) {
    hf_low_ = band.low;
    hf_high_ = band.high;
    hf_band_active_ = true;

    auto mode = ReceiverModel::Mode::AMAudio;
    uint8_t am_gui_option = 0;  // DSB 9k
    rf::Frequency step = 1'000;

    switch (band.mod) {
        case ModKind::NFM:
            mode = ReceiverModel::Mode::NarrowbandFMAudio;
            step = 12'500;
            break;
        case ModKind::WFM:
            mode = ReceiverModel::Mode::WidebandFMAudio;
            step = 100'000;
            break;
        case ModKind::USB:
            am_gui_option = 2;  // USB+3k
            break;
        case ModKind::LSB:
            am_gui_option = 3;  // LSB-3k
            break;
        default:
            break;  // AM -> DSB 9k
    }

    set_previous_AM_mode_option(am_gui_option);
    set_previous_zoom_option(0);
    set_zoom_factor(0);

    // set_by_value() fires on_change -> on_modulation_changed() loads the
    // matching baseband image and shows the modulation options strip.
    options_modulation.set_by_value(toUType(mode));

    if (mode == ReceiverModel::Mode::AMAudio)
        receiver_model.set_am_configuration(am_gui_option);

    on_frequency_step_changed(step);

    // Pop the band list and the category grid; this view stays in the stack,
    // so it is safe for it to run both pops.
    nav_.pop();
    nav_.pop();

    // Tune to the middle of the band and show the frequency strip with the
    // DisableHF button state.
    field_frequency.set_value((band.low + band.high) / 2);
    on_show_options_frequency();
}

/* Band plan selector views **************************************************/

KhanfarBandCatView::KhanfarBandCatView(NavigationView& nav, KhanfarRxView* rx)
    : nav_(nav), rx_(rx) {
    set_max_rows(2);  // 2 columns x 3 rows
}

void KhanfarBandCatView::on_populate() {
    add_items({
        {"ALL HF", Color::red(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_ALL_HF); }},
        {"BROADCAST", Color::yellow(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_BROADCAST); }},
        {"HAM HF", Color::green(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_HAM_HF); }},
        {"HAM VHF", Color::blue(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_HAM_VHF); }},
        {"AIR", Color::cyan(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_AIR); }},
        {"MARINE", Color::blue(), nullptr, [this]() { nav_.push<KhanfarBandListView>(rx_, CAT_MARINE); }},
        {"EIBI", Color::orange(), nullptr, [this]() { nav_.push<KhanfarEibiBandView>(rx_); }},
    });
}

KhanfarBandListView::KhanfarBandListView(NavigationView& nav, KhanfarRxView* rx, uint8_t category)
    : rx_(rx) {
    (void)nav;
    const auto& cat = band_categories[category < CAT_COUNT ? category : 0];
    title_ = cat.title;

    add_children({&menu_view});

    for (size_t i = 0; i < cat.count; i++) {
        const BandDef band = cat.bands[i];
        char line[32];
        char* p = append_str(line, band.name);
        *p++ = ' ';
        *p++ = ' ';
        p = append_str(p, band.range);
        *p = '\0';
        menu_view.add_item({line,
                            color_for(band.mod),
                            nullptr,
                            [this, band](KeyEvent) {
                                rx_->on_hf_band_selected(band);
                            }});
    }
}

void KhanfarBandListView::focus() {
    menu_view.focus();
}

/* EIBI ***********************************************************************/

void KhanfarRxView::on_eibi_station_selected(rf::Frequency freq_hz) {
    // Broadcast SW is plain AM: DSB 9 kHz, 1 kHz step. Drop any HF band lock
    // - it would immediately wrap the new frequency back into the old band.
    hf_band_active_ = false;

    set_previous_AM_mode_option(0);
    set_previous_zoom_option(0);
    set_zoom_factor(0);
    options_modulation.set_by_value(toUType(ReceiverModel::Mode::AMAudio));
    receiver_model.set_am_configuration(0);  // DSB 9k
    on_frequency_step_changed(1'000);

    // Pop the station list, the EIBI band grid and the HF category grid;
    // this view stays in the stack, so it is safe for it to run the pops
    // (same pattern as on_hf_band_selected).
    nav_.pop();
    nav_.pop();
    nav_.pop();

    field_frequency.set_value(freq_hz);
    on_show_options_frequency();  // strip shows "KhanfarHF" (lock off)
}

KhanfarEibiBandView::KhanfarEibiBandView(NavigationView& nav, KhanfarRxView* rx)
    : nav_(nav), rx_(rx) {
    set_max_rows(2);  // 2 columns x 5 rows
}

void KhanfarEibiBandView::on_populate() {
    for (size_t i = 0; i < EIBI_BAND_COUNT; i++) {
        add_item({eibi_bands[i].name,
                  Color::yellow(),
                  nullptr,
                  [this, i]() { nav_.push<KhanfarEibiListView>(rx_, i); }});
    }
}

KhanfarEibiListView::KhanfarEibiListView(NavigationView& nav, KhanfarRxView* rx, uint8_t band_index)
    : rx_(rx) {
    (void)nav;
    const auto& band = eibi_bands[band_index < EIBI_BAND_COUNT ? band_index : 0];
    {
        char t[16];
        char* p = append_str(t, "EIBI ");
        p = append_str(p, band.name);
        *p = '\0';
        title_ = t;
    }

    add_children({&text_info, &menu_view});

    const auto dt = rtc_time::now();
    const uint16_t now_min = dt.hour() * 60 + dt.minute();
    const uint8_t today_bit = 1 << rtc_time::day_of_week(dt.year(), dt.month(), dt.day());

    File file;
    const auto open_error = file.open(std::filesystem::path(u"/EIBI/eibi.bin"), true, false);
    if (open_error) {
        text_info.set("database missing!");
        menu_view.add_item({"put eibi.bin in /EIBI folder", Color::red(), nullptr, [](KeyEvent) {}});
        return;
    }

    struct __attribute__((packed)) EibiHeader {
        char magic[4];
        uint16_t rec_size;
        uint16_t band_count;
        uint32_t entry_count;
    } header;
    static_assert(sizeof(EibiHeader) == 12, "EIBI header layout changed");

    const auto header_read = file.read(&header, sizeof(header));
    if (!header_read.is_ok() || header_read.value() != sizeof(header) ||
        header.magic[0] != 'E' || header.magic[1] != 'I' ||
        header.magic[2] != 'B' || header.magic[3] != '1' ||
        header.rec_size != sizeof(EibiRecord)) {
        text_info.set("bad eibi.bin format!");
        menu_view.add_item({"rebuild it with make_eibi_bin.py", Color::red(), nullptr, [](KeyEvent) {}});
        return;
    }

    // The file is sorted by frequency; appending in file order keeps the
    // list sorted. Cap the list: each menu item costs ~90 B of the tiny heap.
    constexpr size_t max_items = 32;
    size_t active_count = 0;
    EibiRecord rec;
    for (uint32_t i = 0; i < header.entry_count; i++) {
        const auto rec_read = file.read(&rec, sizeof(rec));
        if (!rec_read.is_ok() || rec_read.value() != sizeof(rec))
            break;
        if (rec.band != band_index)
            continue;
        if (!(rec.days & today_bit))
            continue;
        const bool on_air = (rec.start_min <= rec.stop_min)
                                ? (now_min >= rec.start_min && now_min < rec.stop_min)
                                : (now_min >= rec.start_min || now_min < rec.stop_min);
        if (!on_air)
            continue;

        active_count++;
        if (menu_view.item_count() >= max_items)
            continue;

        char station[21];
        memcpy(station, rec.station, 20);
        station[20] = '\0';

        char line[40];
        char* p = line;
        p = append_uint(p, rec.freq_hz / 1000, 5, ' ');
        *p++ = ' ';
        p = append_uint(p, rec.start_min / 60, 2, '0');
        p = append_uint(p, rec.start_min % 60, 2, '0');
        *p++ = '-';
        p = append_uint(p, rec.stop_min / 60, 2, '0');
        p = append_uint(p, rec.stop_min % 60, 2, '0');
        *p++ = ' ';
        p = append_str(p, station);
        *p = '\0';
        const rf::Frequency freq_hz = rec.freq_hz;
        menu_view.add_item({line, Color::white(), nullptr,
                            [this, freq_hz](KeyEvent) {
                                rx_->on_eibi_station_selected(freq_hz);
                            }});
    }

    char info[40];
    char* p = append_str(info, "UTC ");
    p = append_uint(p, dt.hour(), 2, '0');
    *p++ = ':';
    p = append_uint(p, dt.minute(), 2, '0');
    p = append_str(p, " - ");
    p = append_uint(p, active_count, 1, '0');
    p = append_str(p, " on air");
    if (active_count > max_items)
        p = append_str(p, " (32 max)");
    *p = '\0';
    text_info.set(info);

    if (active_count == 0)
        menu_view.add_item({"no station on air now", Color::grey(), nullptr, [](KeyEvent) {}});
}

void KhanfarEibiListView::focus() {
    menu_view.focus();
}

}  // namespace ui::external_app::khanfarrx
