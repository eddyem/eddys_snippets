# Eddy's Snippets

A collection of my small (and sometimes not-so-small) C utilities for Linux and embedded systems —
a personal toolbox built from real‑world projects in astronomy, laboratory automation, and embedded
development. Some are battle‑tested, others are experimental or deprecated.

---

## Overview

This repository contains a wide variety of C code snippets, full utilities, and helper libraries
that I have developed over the years for different tasks. The common thread is **pragmatic C on
Linux** — tools for serial communication, Modbus control, sensor interfacing, network proxies,
embedded optimization, and more.

The repository is intentionally modular. Each subdirectory typically contains an independent
utility or library with its own build system (plain `Makefile` or `CMakeLists.txt`). Some modules
rely on my common helper library [`snippets_library`](https://github.com/eddyem/snippets_library)
(also known as `usefull_macros`).

### 🔍 Quick Navigation
- **Active / Maintained** → [`ESP8266`](#esp8266), [`I2Csensors`](#i2csensors), [`modbus_params`](#modbus_params), [`serialsock`](#serialsock), [`stringHash4MCU_`](#stringhash4mcu_)
- **Mature / Stable** → [`HSFW_management`](#hsfw_management), [`thread_array_management`](#thread_array_management), [`USBrelay`](#usbrelay)
- **Deprecated / Archive** → [`Trinamic`](#trinamic), [`Socket_CAN`](#socket_can), [`NES_webiface`](#nes_webiface)
- **Algorithmic / Embedded Helpers** → [`_snippets`](#_snippets), [`table_sincos`](#table_sincos), [`stringHash4MCU_`](#stringhash4mcu_), [`Zernike`](#zernike)

---

## Modules

### `ESP8266` - **Description:** A simple TCP server based on AT‑commands for the ESP8266 Wi‑Fi
module. The code is written to be as close to the MCU as possible, making it lightweight and easy
to port.
- **Status:** Active
- **Key Files:** `esp8266.c`, `esp8266.h`, `main.c`, `serial.c`, `CMakeLists.txt`

### `I2Csensors`
- **Description:** A complete library for reading data from various temperature, humidity, and
    pressure sensors over I2C.
- **Supported Sensors:** AHT10, AHT15, AHT21b, BMP180, BMP280, BME280, SHT3x, SI7005.
- **Features:** Unified API with sensor status tracking, support for both single‑shot and periodic
    measurements, and example programs included.
- **Build:** CMake (with `DEBUG` and `EXAMPLES` options). After installation, use `#include
    <i2csensorsPTH.h>` and link with `-li2csensorsPTH`.
- **Status:** Active
- **Documentation:** See [`I2Csensors/README`](I2Csensors/Readme) for full API reference and usage
    examples.

### `HSFW_management`
- **Description:** A command‑line utility to manage Edmund Optics high‑speed filter wheels (HSFW).
    It allows you to check/change turret positions, list connected devices, and rename wheels or
    filters stored in EEPROM.
- **Features:** Supports operation by filter name, wheel ID, or serial number; includes a
    “list‑all” mode to dump all EEPROM information.
- **Status:** Mature (marked for possible code review)
- **Documentation:** See [`HSFW_management/README`](HSFW_management/Readme) and the usage examples
in the module’s description.

### `MLX90640_test`
- **Description:** A test implementation for the MLX90640 thermal camera sensor. Currently
    processes example data from Melexis and is used to debug calculation errors in the temperature
    conversion algorithm.
- **Status:** Experimental / Almost deprecated
- **Note:** The sensor driver is not yet fully functional; the main purpose is to test and verify
    the mathematical model offline using CSV data.

### `NES_webiface`
- **Description:** A web interface for managing the Nesmith Echelle Spectrograph (NES). Built on
    top of `pusirobot` and `liboninon`.
- **Status:** Deprecated (almost deprecated)

### `Raspberry`
- **Description:** Raspberry Pi–specific code.
- **Contents:** `MPU-6050-log/` — a utility to log data from an MPU‑6050 accelerometer/gyroscope
    sensor.
- **Status:** Archive

### `SSL_Socket_snippet`
- **Description:** A client/server example demonstrating an SSL‑protected TCP socket connection
    with mutual certificate verification (server checks client certificates).
- **Features:** Complete example with CA and client/server certificate files, command‑line option
    parsing, and separate client/server implementations.
- **Status:** Stable (reference implementation)

### `Socket_CAN`
- **Description:** A CANbus message retranslation utility between two different CAN networks (BTA
    bus ↔ Dome bus). Includes SSL support for secure communication and example udev rules.
- **Components:** `soccanserver` and `soccanclient` with mutual TLS authentication.
- **Status:** Deprecated (marked as almost deprecated)

### `TeAmanagement`
- **Description:** Client/server management utility for "Telescope Analyzer" (TeA) stages.
- **Status:** Archive / Unmaintained

### `Tetris`
- **Description:** A classic CLI Tetris implementation using terminal graphics.
- **Features:** Full game logic with keyboard controls, score tracking, and a terminal display
    buffer.
- **Status:** Stable / Complete

### `Trinamic`
- **Description:** Older code for managing Trinamic stepper motor drivers.
- **Submodules:** `SHA_client/`, `SHA_client_cmdline/` (Shack‑Hartmann sensor clients), `client/`,
    `websock_simple/`.
- **Status:** Deprecated (almost deprecated)

### `USBrelay`
- **Description:** A utility to control Chinese USB relay modules (HID‑based). Includes device
    autodetection and udev rules for permission setup.
- **Status:** Mature
- **Documentation:** See the `Makefile` and the module’s README (if present).

### `Zernike`
- **Description:** A implementation of wavefront decomposition and reconstruction using Zernike
    polynomials (up to 100th order). Supports both standard (n, m) and Noll indexing notations.
- **Functions:** `zernfunR()`, `zernfunNR()`, `Zdecompose()`, `Zcompose()`, `set_prec()`.
- **Status:** Archive / Algorithmic reference

### `_snippets`
- **Description:** A collection of miscellaneous small C code snippets, including:
    - AVX examples
    - B‑trees
    - Bresenham circle drawing
    - C‑style function overloading tricks
    - USB reset
    - CLI color output
    - Bit counting
- **Status:** Archive / Personal reference

### `cmakelists_`
- **Description:** A set of reusable `CMakeLists.txt` and `Makefile` templates for different
    project configurations (e.g., debug/release, library/executable, cross‑compilation).
- **Contents:** Several `CMakeLists_regular_XX.txt` variants, `Makefile`, `Makefile_dbg_rls`, and
    helper scripts.
- **Status:** Reference / Active

### `modbus_params`
- **Description:** A command‑line tool and network server for interacting with Modbus RTU devices.
    It uses a human‑readable dictionary that maps register addresses to mnemonic codes.
- **Features:**
    - Read/write holding registers by address or code.
    - Periodic dumping of selected parameters to a TSV file.
    - TCP/UNIX socket server for remote control.
    - Aliases (macros) and verbose logging.
- **Dependencies:** `libmodbus`, `usefull_macros`, pthread.
- **Status:** Active
- **Documentation:** See [`modbus_params/README.md`](modbus_params/Readme) for command‑line options
    and server protocol.

### `modbus_relay`
- **Description:** A simple utility to manage Chinese Modbus relay boards. Includes a basic web
    interface (`web_management/` subdirectory) for remote control.
- **Features:** Supports multiple relay board types (including a non‑standard 4‑relay board based
    on STM8S003) and command‑line operation.
- **Status:** Active

### `serialsock`
- **Description:** A lightweight TCP/UNIX socket‑to‑serial proxy that allows **multiple
    simultaneous clients** to read from and write to a single serial (TTY) device. It runs as a robust
    daemon with automatic child respawning.
- **Features:**
    - Up to 30 simultaneous TCP clients.
    - Configurable baud rate, device path, and exclusive access.
    - Daemon mode with PID file and logging.
    - Signal‑safe cleanup.
- **Dependencies:** `usefull_macros` (provides command‑line parsing, serial port handling, socket
    management, and logging).
- **Status:** Active
- **Documentation:** See [`serialsock/Readme.md`](serialsock/Readme.md) for full usage.

### `stellarium_emul`
- **Description:** An emulator for the Stellarium telescope control protocol, designed to control a
    10‑micron mount.
- **Features:** Daemon mode, command‑line options, and socket‑based communication.
- **Status:** Archive

### `stringHash4MCU_`
- **Description:** A tool for testing and generating hash functions suitable for string command
    parsing on microcontrollers.
- **Components:** `hashtest.c` (test different hash functions on a dictionary), `hashgen.c`
    (generate source/header files from a dictionary), and helper scripts (`runtest`, `mktestdic`).
- **Use Case:** Convert string commands to integer hashes for efficient parsing on
    resource‑constrained MCUs.
- **Status:** Active

### `table_sincos`
- **Description:** A generator for lookup tables to compute `sin()` and `cos()` using integer
    arithmetic on MCUs. Includes a fast integer square root implementation.
- **Usage:** Run `gentab <precision>` to generate a table; compile with `-DMAX_LEN` and
    `-DDATATYPE` to customize.
- **Status:** Stable / Reference

### `thread_array_management`
- **Description:** A library for managing named thread lists with FIFO message passing between
    threads.
- **Status:** Mature
- **Example:** Includes a test program (`test.c`) demonstrating usage.

### `usb_reset`
- **Description:** A small utility to reset a USB bus (useful for recovering stuck USB devices).
- **Status:** Stable

---

## Common Dependencies

Many modules rely on my common helper library:
[`snippets_library`](https://github.com/eddyem/snippets_library) (also known as `usefull_macros`).
This library provides:
- Command‑line parsing (`sl_parseargs`)
- Serial port (TTY) handling (`sl_tty_t`)
- TCP socket management (`sl_sock_open`)
- Daemonisation, PID file handling
- Logging macros (`LOGMSG`, `LOGWARN`, `LOGERR`, etc.)

To build any module that depends on it, first install `libusefull_macros`:

```bash
git clone --depth=1 https://github.com/eddyem/snippets_library.git
cd snippets_library
mkdir build && cd build
cmake ..
make
sudo make install
```

Then return to the module directory and build (usually with `make`).

---

## Building a Module

Most modules can be built by simply running `make` in their directory. For CMake‑based modules
(like `I2Csensors`):

```bash
mkdir build && cd build
cmake ..
make
su -c "make install"   # optional
```

Some modules have additional configuration options (e.g., `DEBUG`, `EXAMPLES`) that can be passed
to CMake.

---

## License

This repository is provided under the GNU General Public License v3 or later.
Individual modules may contain their own license headers; the GPL license applies unless otherwise
stated.

---

## Contributing

This is primarily a personal collection, but issues and pull requests are welcome. If you find a
bug or have an improvement, please open an issue first to discuss it.

---

## Disclaimer

**Use at your own risk.** Some modules are marked as deprecated or “almost deprecated” — they may
contain bugs, rely on outdated interfaces, or lack proper documentation. Always review the code
before using it in a production environment.

