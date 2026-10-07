# Display drawing: frame buffer, shapes, 3x5 font
target_sources(app PRIVATE gfx/gfx.c)
target_include_directories(app PRIVATE gfx)

# DHT22 temperature reading and corner readout
target_sources(app PRIVATE temperature/temperature.c)
target_include_directories(app PRIVATE temperature)

# Animations: all built in, switchable at runtime from the web page
target_sources(app PRIVATE animation/animation.c animation/pong.c animation/sine.c)
target_include_directories(app PRIVATE animation)

# Wi-Fi access point and HTTP server
target_sources(app PRIVATE net/wifi_ap.c net/web_server.c)
target_include_directories(app PRIVATE net)
zephyr_linker_sources(SECTIONS net/sections-rom.ld)

# Web page, gzipped into the firmware as index.html.gz.inc
generate_inc_file_for_target(app
  net/www/index.html
  ${ZEPHYR_BINARY_DIR}/include/generated/index.html.gz.inc
  --gzip
)
