# Poko memory analysis

Static analysis of the build for `esp32c6_devkitc/esp32c6/hpcore`
(ESP32-C6, 512 KB SRAM, 8 MB flash), from the linker output, `zephyr.map`
and the symbol table of `build/zephyr/zephyr.elf`. Measured on 2026-10-08
with the debugging options in `prj.conf` enabled (logging, shell, stack
sentinel).

## Summary

| Memory | Used | Size | Used % | Free |
| ------ | ---: | ---: | -----: | ---: |
| Flash (firmware image) | 744,852 B (727 KB) | 8 MB | 8.9 % | ~7.3 MB |
| SRAM (`sram0_0_seg`) | 320,608 B (313 KB) | 509,456 B (497.5 KB) | 62.9 % | 188,848 B (184 KB) |
| LP RAM (`lp_ram_seg`) | 0 B | 15,584 B | 0 % | all |

The SRAM figure counts the whole 128 KB heap as used. How much of the heap
the Wi-Fi driver really needs is only visible at runtime (see
[Runtime measurements](#runtime-measurements)).

## Flash

The firmware image is 744,852 bytes. With `CONFIG_ESP_SIMPLE_BOOT` there is
no MCUboot: the single image is written at flash address `0x0` and the
ROM bootloader starts it directly.

| Part of the image | Size | What it is |
| ----------------- | ---: | ---------- |
| `.text` (irom) | 478,448 B | Code executed from flash |
| `.flash.rodata` (drom) | 87,856 B | Constants, strings, tables, the web page |
| RAM image | ~177 KB | Code and data copied into SRAM at boot (see below) |

### Flash by component

Code and constants that stay in flash (590,151 B):

| Component | Size | Share | Main contents |
| --------- | ---: | ----: | ------------- |
| Espressif Wi-Fi libraries (binary blobs) | 325,127 B | 55 % | `libnet80211` 207,586 · `libpp` 91,153 · `libphy` 25,966 |
| `libzephyr.a` | 103,329 B | 18 % | Espressif HAL and Wi-Fi adapter, WPA2 code (`wpa`, `wpa_auth`, SHA-1, AES ≈ 30 KB), shell (≈ 15 KB), logging, utilities |
| Networking | 91,717 B | 16 % | IPv4/TCP/UDP 43,467 · HTTP server 27,275 · net core 8,389 · Ethernet L2 6,041 · DHCP server 5,985 |
| Poko app | 31,616 B | 5 % | See below |
| Drivers (I2C, GPIO, display, sensor, serial) | 14,025 B | 2 % | |
| C library | 9,264 B | 2 % | |
| Zephyr Wi-Fi driver | 7,402 B | 1 % | |
| Other (file descriptors, net_buf, arch, …) | 7,671 B | 1 % | |

### Poko app code

| File | Flash | RAM |
| ---- | ----: | --: |
| `web_server.c` (includes the 3 KB gzipped `index.html`) | 3,513 B | 250 B |
| `gfx.c` | 982 B | 1,024 B (frame buffer `fb`) |
| `pong.c` | 939 B | 45 B |
| `wifi_ap.c` | 686 B | 24 B |
| `sine.c` | 569 B | 68 B |
| `temperature.c` | 469 B | 33 B |
| `animation.c` | 308 B | 8 B |
| **Total app code** | **≈ 7.5 KB** | **≈ 1.45 KB** |

The linker map also lists 23,816 B of `.rodata.main.str1.4` under
`main.c`. That is not code from `main.c`: it is the merged string pool
(log messages, shell help texts, …) of many objects, which the linker
names after the first file that contributes to it.

### Flash partitions

The devicetree uses Espressif's default 4 MB partition table:

| Partition | Offset | Size | Used now |
| --------- | -----: | ---: | -------- |
| `mcuboot` | 0x000000 | 64 KB | No (simple boot: image starts at `0x0`) |
| `sys` | 0x010000 | 64 KB | No |
| `image-0` | 0x020000 | 1,792 KB | No (only with MCUboot) |
| `image-1` | 0x1E0000 | 1,792 KB | No (only with MCUboot) |
| `image-0-lpcore` / `image-1-lpcore` | 0x3A0000 | 32 KB each | No |
| `storage` | 0x3B0000 | 192 KB | No, free for settings/NVS |
| `coredump` | 0x3FF000 | 4 KB | No |

If MCUboot is added later (e.g. for over-the-air updates), the image must
fit a 1,792 KB slot; at 727 KB it uses 41 % of one.

## SRAM

| Section | Size | Contents |
| ------- | ---: | -------- |
| `.dram0.noinit` | 181,708 B | Heap, thread stacks, network buffers (not zeroed at boot) |
| `.iram0.text` | 84,148 B | Code that must run from RAM: interrupt handlers, kernel, Wi-Fi hot paths, flash driver |
| `.dram0.bss` | 35,264 B | Zero-initialized variables |
| `.dram0.data` | 14,912 B | Initialized variables (mostly Wi-Fi/PHY tables) |
| `.loader.*` | 3,184 B | Boot loader code and data |
| **Total** | **≈ 313 KB** | |

### Heap: 131,172 B (40 % of used SRAM)

`CONFIG_HEAP_MEM_POOL_SIZE=131072`. Raised from the 55 KB default because
the Wi-Fi driver ran out of memory and halted the chip when a phone
connected. The Wi-Fi driver allocates from it at runtime: its own task
stacks (`CONFIG_DYNAMIC_THREAD_ALLOC`), per-station state, WPA2 handshake
buffers and up to 32 dynamic RX and 32 dynamic TX buffers.

### Thread stacks: 29,696 B (29 KB)

| Thread | Stack | Set by |
| ------ | ----: | ------ |
| `main` (display loop, Wi-Fi/HTTP setup) | 4,096 B | `CONFIG_MAIN_STACK_SIZE` |
| Network management events (`mgmt_stack`) | 4,096 B | `CONFIG_NET_MGMT_EVENT_STACK_SIZE` |
| Socket service, runs the DHCP server (`service_thread_stack`) | 4,096 B | `CONFIG_NET_SOCKETS_SERVICE_STACK_SIZE` |
| HTTP server | 3,072 B | `CONFIG_HTTP_SERVER_STACK_SIZE` (default) |
| Network RX (`rx_stack`) | 2,048 B | `CONFIG_NET_RX_STACK_SIZE` (Wi-Fi driver default) |
| TCP work queue (`work_q_stack`) | 2,048 B | `CONFIG_NET_TCP_WORKQ_STACK_SIZE` |
| System work queue | 2,048 B | `CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE` |
| Wi-Fi event thread | 2,048 B | Zephyr ESP32 Wi-Fi driver |
| Shell | 2,048 B | Shell default |
| Interrupts | 2,048 B | `CONFIG_ISR_STACK_SIZE` |
| Wi-Fi timer task | 1,536 B | Espressif Wi-Fi adapter |
| Idle | 512 B | `CONFIG_IDLE_STACK_SIZE` |

Not included: the Espressif Wi-Fi tasks, whose stacks come from the heap.

### Largest other RAM users

| Symbol | Size | Owner |
| ------ | ---: | ----- |
| `_k_mem_slab_buf_tcp_conns_slab` | 6,800 B | TCP connection pool (`CONFIG_NET_MAX_CONN=10`) |
| `g_cnxMgr` | 5,256 B | Espressif Wi-Fi connection manager |
| `net_buf_data_rx_bufs` / `net_buf_data_tx_bufs` | 4,608 B each | Network packet data buffers |
| `server_ctx` | 4,040 B | HTTP server state (clients, buffers) |
| `esp32_data` | 1,684 B | Zephyr ESP32 Wi-Fi driver state |
| `contexts` | 1,520 B | Network contexts (`CONFIG_NET_MAX_CONTEXTS=10`) |
| `s_wifi_nvs` | 1,316 B | Wi-Fi settings cache |
| `s_retention` | 1,176 B | Power-management retention |
| `fb` | 1,024 B | OLED frame buffer (128 × 64 / 8) |

## Runtime measurements

Static analysis cannot show how much of the heap and of each stack is
actually used. The firmware has a shell on the USB console
(`/dev/ttyACM0`, 115200 baud) for this:

- `kernel thread stacks`: size, used and unused bytes per thread.
  Needs `CONFIG_INIT_STACKS` and `CONFIG_THREAD_STACK_INFO` (enabled).
- `kernel heap`: free, allocated and maximum allocated heap bytes since
  boot. Needs `CONFIG_SYS_HEAP_RUNTIME_STATS=y`.

Measure after a phone has connected and loaded the web page a few times,
because the peak heap use happens during the WPA2 handshake and while
serving pages.

## Where to save memory if needed

1. **Heap:** once `kernel heap` shows the peak, lower
   `CONFIG_HEAP_MEM_POOL_SIZE` to the peak plus a safety margin (25-30 %).
2. **Stacks:** shrink stacks that `kernel thread stacks` reports as
   mostly unused, keeping at least 25 % headroom.
3. **Debugging options:** shell, logging, thread names and the stack
   sentinel cost RAM (shell stack, buffers, history) and flash (shell ≈
   15 KB, log strings). Turn them off for a release build.
4. **Network limits:** `CONFIG_NET_MAX_CONN` and
   `CONFIG_NET_MAX_CONTEXTS` (10 each) are sized for the HTTP server with
   3 clients; lowering them shrinks the TCP slab and context table.

There is no pressure to do any of this yet: 184 KB of SRAM and over 7 MB
of flash are still free.
