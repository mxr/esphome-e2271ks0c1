# ESPHome E2271KS0C1 E-Paper Display Driver

ESPHome external component for the [Pervasive Displays E2271KS0C1 2.7" e-paper display](https://www.pervasivedisplays.com/product/2-71-e-ink-displays/#wide-temperature-fast-refresh) (176x264 pixels), as used in the [Bitclock](https://github.com/goat-hill/bitclock) project.

## Features

- Full and fast (partial) update modes
- Configurable update interval and full refresh frequency
- Dark mode support (inverted colors)
- 12h/24h time format
- Celsius/Fahrenheit temperature display

## Installation

1. Copy the external component to your ESPHome configuration directory:

```bash
scp -r external_components your-homeassistant.local:/config/esphome/
```

Replace `your-homeassistant.local` with your Home Assistant hostname or IP address.

2. Create a new ESPHome configuration based on `bitclock.example.yaml`

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `cs_pin` | required | SPI chip select pin |
| `dc_pin` | required | Data/command pin |
| `reset_pin` | required | Hardware reset pin |
| `busy_pin` | required | Busy status pin (set `inverted: true`) |
| `data_rate` | 4MHz | SPI clock speed |
| `update_interval` | 10s | How often to refresh the display |
| `full_update_every` | 30 | Full refresh every N updates (clears ghosting) |
| `temperature_c` | 25 | Ambient temperature for waveform timing |

## Bitclock Example Configuration

See [bitclock.example.yaml](https://github.com/gxlabs/esphome-e2271ks0c1/blob/main/bitclock.example.yaml) for a complete example with:
- Time display (12h/24h configurable)
- WiFi status icon
- CO2, temperature, humidity, and VOC sensor readings
- Material Design Icons
- Dark mode toggle

<img src="https://github.com/user-attachments/assets/4aace8f3-e9bc-49de-a069-bb6bac813f6b" width=450 />

### Substitutions

The example uses substitutions for easy configuration:

```yaml
substitutions:
  dark_mode: "true"       # White text on black background
  time_24h: "false"       # 12-hour time format
  temp_fahrenheit: "true" # Temperature in Fahrenheit
```

## Hardware

This driver is designed for the Pervasive Displays E2271KS0C1 panel with the following pinout (Bitclock PCB):

| Signal | GPIO |
|--------|------|
| SPI CLK | GPIO6 |
| SPI MOSI | GPIO1 |
| CS | GPIO10 |
| DC | GPIO7 |
| RESET | GPIO5 |
| BUSY | GPIO4 |
| I2C SDA | GPIO8 |
| I2C SCL | GPIO9 |

## Credits

The example configuration ([bitclock.example.yaml](https://github.com/gxlabs/esphome-e2271ks0c1/blob/main/bitclock.example.yaml)) extends from [manedfolf/bitclock](https://github.com/manedfolf/bitclock/blob/esphome/bitclock-esphome/bitclock-esphome.yaml) ESPHome configuration.

## License

MIT
