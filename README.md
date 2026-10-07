# Poko

Zephyr firmware for the ESP32-C6-DevKitC with a 0.96" two-color SSD1306 OLED
and a DHT22 temperature sensor.

- Shows an animation on the OLED (Pong or a scrolling sine wave) with the
  temperature in the top-right corner.
- Starts a Wi-Fi access point named **Poko** with a web page at
  http://192.168.4.1 that shows uptime and temperature and switches the
  animation without a reboot.

## Wiring

| Part           | Pin             |
| -------------- | --------------- |
| OLED SCL       | GPIO0           |
| OLED SDA       | GPIO1           |
| DHT22 data     | GPIO20          |

The OLED is at I2C address `0x3C`. Rows 0-15 are yellow, rows 16-63 blue.

## Build and flash

From the west workspace (`~/zephyrproject`):

```bash
source .venv/bin/activate
west build -p always -b esp32c6_devkitc/esp32c6/hpcore -d workspace/apps/poko/build workspace/apps/poko
west flash -d workspace/apps/poko/build --esp-device /dev/ttyACM0
```

The Espressif Wi-Fi blobs must be present: `west blobs fetch hal_espressif`.

The console and a shell are on the same USB port (`/dev/ttyACM0`, 115200).

## Layout

| Path                  | Contents                                         |
| --------------------- | ------------------------------------------------ |
| `src/main.c`          | Startup and the 30 ms frame loop                 |
| `lib/gfx/`            | Frame buffer, shapes, lines, 3x5 font            |
| `lib/temperature/`    | DHT22 reading and the corner readout             |
| `lib/animation/`      | Animation switching, `pong.c`, `sine.c`          |
| `lib/net/`            | Wi-Fi access point, HTTP server, `www/index.html`|
| `boards/`             | Devicetree overlay for the pins above            |
| `Kconfig`, `prj.conf` | Animation at power-up, drivers, network settings |
