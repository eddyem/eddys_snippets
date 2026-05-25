# Multi-Client Serial Port Proxy

`sersock` is a lightweight TCP/UNIX socket‑to‑serial proxy that enables **multiple simultaneous
clients** to read from and write to a single serial (TTY) device. It runs as a robust daemon that
automatically respawns its worker child, supports logging, and works with both local and remote
socket connections.

---

## Table of Contents
- [Features](#features)
- [How It Works](#how-it-works)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Command-Line Arguments](#command-line-arguments)
- [Usage Examples](#usage-examples)
- [Socket Node Format](#socket-node-format)
- [Signal Handling](#signal-handling)
- [Logging](#logging)
- [Building from Source](#building-from-source)
- [License](#license)

---

## Features
- **Multi‑client support** – Up to 30 TCP clients can connect simultaneously.
- **Serial device management** – Configurable baud rate, path, and exclusive access.
- **Daemon mode** – Runs in background with automatic child process respawn.
- **Command‑line configuration** – All options available via CLI arguments.
- **Verbose logging** – Adjustable log level, optional file logging with flock().
- **Signal‑safe** – Proper cleanup of PID files, serial ports, and terminal settings on exit.

---

## How It Works
The program operates in two modes, selected by the `--client` flag:

| Mode | Description |
|------|-------------|
| **Server** (default) | Opens the serial device, creates a listening socket, and forks a worker child. The child manages all client connections (up to 30). If the child dies, the parent automatically respawns it after a short delay. |
| **Client** | Connects to the server socket and forwards terminal input to the server. Data received from the server is printed on the client’s terminal. |

The server daemonises itself, writes its PID to a file (default: `/tmp/usbsock.pid`), and changes
the working directory to `/` to avoid keeping any directory busy.

---

## Prerequisites
- Linux operating system
- [`libusefull_macros`](https://github.com/eddyem/snippets_library) – the core utility library used by sersock.  
  This library provides:
  - Command‑line parsing with `sl_parseargs`
  - Serial port (TTY) handling via `sl_tty_t`
  - TCP socket management (`sl_sock_open`)
  - Daemonisation, PID file handling, and logging macros (`LOGMSG`, `LOGWARN`, `LOGERR`, etc.)

---

## Installation

### 1. Install `libusefull_macros`
```bash
git clone --depth=1 https://github.com/eddyem/snippets_library.git
cd snippets_library
mkdir build && cd build
cmake ..
make
su -c "make install"
```

### 2. Build `sersock`
```bash
git clone --depth=1 https://github.com/eddyem/eddys_snippets
cd serialsock
make

```

The resulting binary is `serialsock`. You can copy it wherever you want, e.g. 
```bash
su -c "cp serialsock /usr/local/bin
```


---

## Command-Line Arguments

| Option | Argument | Default | Description |
|--------|----------|---------|-------------|
| `-h, --help` | – | – | Show help message and exit. |
| `-d, --devpath` | *path* | – | Path to the serial device (e.g., `/dev/ttyUSB0`). **Required in server mode.** |
| `-s, --speed` | *baud* | `9600` | Serial device baud rate. |
| `-l, --logfile` | *file* | – | File to write logs (uses `flock(LOCK_EX)`). |
| `-p, --pidfile` | *file* | `/tmp/usbsock.pid` | PID file for the server. |
| `-c, --client` | – | – | Run as a client (connect to server). |
| `-n, --node` | *node* | – | Socket node (see [format](#socket-node-format)). |
| `-v, --verbose` | – | – | Increase log verbosity (can be used multiple times). |

> **Note:** In server mode, `--devpath` must be provided. `--node` must be provided in any mode.

---

## Usage Examples

### Server – Share a serial device on TCP port 12345
```bash
sersock -d /dev/ttyACM0 -s 115200 -n :12345 -v -l /var/log/sersock.log
```
- Listens on TCP port 12345.
- Uses baud rate 115200.
- Logs to `/var/log/sersock.log` with increased verbosity.

### Client – Connect to the TCP server
```bash
sersock -c -n localhost:12345
```
Opens an interactive terminal where every line entered is sent to the serial port; data received
from the serial port is displayed.

You also can connect to this socket using my [tty_term](https://github.com/eddyem/tty_term),
netcat or any other same terminal socket client. 

---

## Socket Node Format

The `-n` parameter accepts only TCP-sockets.

- **Server**: `":port"` – listens on all interfaces (e.g., `":12345"`).  
- **Client**: `"host:port"` – connects to the specified host (e.g., `"localhost:12345"` or `"192.168.1.10:8080"`).

If you want to open server listening only local clients, point node as `-n localhost:port`,
e.g. `-n localhost:12345`.

---

## Signal Handling

| Signal | Effect |
|--------|--------|
| `SIGTERM`, `SIGINT`, `SIGQUIT` | Gracefully close serial port, remove PID file, restore terminal settings (client), and exit. |
| `SIGTSTP` (Ctrl+Z), `SIGHUP` | Ignored in server mode. |

If the worker child dies unexpectedly, the master process waits 1–10 seconds and respawns it
automatically.

---

## Logging

- **Console logging** – Messages appear on stderr unless daemonised.  
- **File logging** – Use `-l` to write logs to a file.  
  File writes are guarded with `flock(LOCK_EX)` to be safe across processes.  
- **Verbosity** – Each `-v` increases the log level (up to `LOGLEVEL_ANY`).  
  Default level is `LOGLEVEL_WARN`.

---

## License

Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Full text of the GPLv3 is available at <https://www.gnu.org/licenses/gpl-3.0.html>.
