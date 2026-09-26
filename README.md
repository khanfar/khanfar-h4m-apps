# Khanfar H4M Apps for PortaPack H4M

Free external apps for the HackRF PortaPack H4M (Mayhem firmware):

- **KhanfarRX** — HF band plan with in-band tuning lock, and a live EIBI "on air now" shortwave station browser
- **FT8-geo** — FT8 decoder with a live world map: every decoded station is plotted from its Maidenhead locator in real time

## FT8-geo

FT8-geo decodes FT8 like the stock FT8 RX app (same decoder, same band buttons, both can stay installed), and adds a **Map** view:

- **Live station plotting**: the Maidenhead grid locator is parsed from each decoded message and the station pops up on the world map as it is received
- **Colored by message type**: CQ = green, QSO exchange = yellow, 73 = red — dots and lines
- **Home QTH**: 4-character locator entry, saved on the SD card (defaults to KM72)
- **Touch-drag panning** and 7 zoom presets: 400 / 800 / 2000 / 4000 / 8000 / 16000 km / whole world, with a live scale label
- **Maidenhead field grid** (A–R letters and 20°×10° lines) baked into the light 8 MB map
- **Last-heard list**: total count + 5 rows of `callsign | grid | country | distance | bearing`; rotary scrolls, touch selects, the selected station is highlighted on the map with a white double line
- **RST** button clears the spots
- Country names from a compact callsign-prefix table (~150 prefixes, in flash)
- Memory-safe for the H4M: fixed 24-entry spot store, no heap allocation
- Uses its own map file (`/ADSB/ft8geo_map.bin`), so the hi-res ADS-B map is untouched

## KhanfarRX

- **Easy Band Plan**: Broadcast, amateur (HAM HF/VHF), airband and marine bands in one clean menu
- **Stay in the Band**: Smart band lock with wrap-around tuning inside band edges
- **EIBI "On Air Now"**: Live EIBI shortwave schedule browser for 10 SW bands, with paging
- **Correct Mode Automatically**: Every band sets the right mode and step
- **Full Receiver Power**: Live waterfall, RSSI, recorder, gain and squelch controls
- **Always Fresh Data**: Convert newest EIBI season with the online converter

## Requirements

- PortaPack H4M with HackRF One hardware
- The custom Mayhem firmware from [GitHub Releases](https://github.com/khanfar/khanfar-h4m-apps/releases) (nightly 2026-09-25 base — the apps and firmware are a matched pair, do not mix with other builds)
- USB cable for firmware flashing
- PC with Chrome or Edge browser

## Installation

1. Download the latest release from [GitHub Releases](https://github.com/khanfar/khanfar-h4m-apps/releases)
2. Flash the custom firmware using [hackrf.app](https://hackrf.app/)
3. Copy all app files to your SD card (download them from https://khanfar-h4m-apps.web.app or from [GitHub Releases](https://github.com/khanfar/khanfar-h4m-apps/releases)), then:
   - create an `EIBI` folder in the root of the card — download the EIBI database from eibispace.de, convert it with the online EIBI converter, and put the file at `/EIBI/eibi.bin` (needed by KhanfarRX)
   - copy the map file to `/ADSB/ft8geo_map.bin` (needed by FT8-geo; the stock `/ADSB/world_map.bin` stays as it is for ADS-B)
4. Set the clock to UTC on your H4M
5. Launch **KhanfarRX** or **FT8-geo** from the Receive menu

Detailed installation instructions are available on the [project website](https://khanfar-h4m-apps.web.app)

## Source Code

- `ft8geo/` — FT8-geo app source (`main.cpp`, `ui_ft8geo.hpp/.cpp`)

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).
FT8-geo is derived from the Mayhem `ft8_rx` app (GPL-2.0-or-later) and is distributed under compatible GPL terms.

### Attribution

This application is built for the PortaPack Mayhem firmware environment and depends on:

- **PortaPack Mayhem Firmware** - https://github.com/portapack-mayhem/mayhem-firmware
- **HackRF** - https://greatscottgadgets.com/hackrf/
- **PortaPack** - https://store.sharebrained.com/products/portapack-for-hackrf-one-kit

### Third-Party Data

- **EIBI Schedule Data** © eibispace.de - Please support their work at https://www.eibispace.de/
- **World map imagery**: NASA Blue Marble (public domain)
- **Country borders**: Natural Earth (public domain)

### GPL-3.0 Compliance

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

## Support

- **Developer**: M. Khanfar
- **Email**: E4MWAK@gmail.com
- **Issues**: [GitHub Issues](https://github.com/khanfar/khanfar-h4m-apps/issues)
- **Website**: https://khanfar-h4m-apps.web.app

## Acknowledgments

- PortaPack Mayhem firmware team for the excellent firmware platform
- The HF radio community for feedback and testing
- eibispace.de for maintaining the EIBI schedule database

## Disclaimer

This software is provided as-is for educational and hobbyist purposes. Users are responsible for ensuring compliance with local laws and regulations regarding radio frequency usage.
