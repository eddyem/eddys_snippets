# poweroff_daemon

`poweroff_daemon` is a small Linux daemon that monitors a serial-attached [power/fan control
module](https://github.com/eddyem/stm32samples/tree/master/F0%3AF030%2CF042%2CF072/Socket_fans) of a
single-board computer. The board is powered by four 18650 cells and an external 16.8 V supply. The daemon
polls the module for button state and conditional battery voltage and calls `/sbin/poweroff` when a shutdown
condition is detected.

The daemon is intended for headless systems where the power module must shut the computer down before the
batteries are deeply discharged, or when a physical `BUTTON0` is pressed.

## Features

- Polls a serial device for:
  - `BUTTON0` state;
  - `V12` voltage value (in arbitrary units, not hundreds of Volts!).
- Triggers `/sbin/poweroff` when:
  - `BUTTON0` is pressed;
  - `V12` falls below the configured low-voltage threshold.
- Runs as a daemon.
- Supervises a child process and restarts it if it exits unexpectedly.
- Provides a UNIX-domain socket for sending custom commands to the serial device.
- Supports optional logging to a file.
- Uses newline-terminated serial protocol (`\n`, no `\r`).

## Project files

| File | Purpose |
|------|---------|
| `main.c` | Program entry point, daemonization, child supervision, main polling loop, poweroff trigger. |
| `cmdlnopts.c` / `cmdlnopts.h` | Command-line option parsing and default configuration values. |
| `serial.c` / `serial.h` | Serial port handling, command/response parsing, button and voltage checks. |
| `server.c` / `server.h` | UNIX-domain socket server and request handlers. |

## Requirements

- Linux.
- C compiler and build tools.
- [`usefull_macros`](https://github.com/eddyem/snippets_library) library.
- `pthread` support.
- A serial device connected to the power/fan control module, usually `/dev/ttyUSB0`.
- Root privileges or suitable permissions for:
  - opening the serial device;
  - executing `/sbin/poweroff`;
  - creating the PID file and socket, depending on paths.

Build with `make` commands, supporting several targets:  

- `release` (default) - release build;
- `debug` - build with debugging on;
- `clean` - clear temporarily build files;
- `xclean` - also clean binary file.

There's no option `install`, so you should manually copy binary file into some destination.


## Usage

```sh
./poweroff_daemon [options]
```

Example:

```sh
./poweroff_daemon \
    --device /dev/ttyUSB0 \
    --baudrate 115200 \
    --interval 1.0 \
    --sockpath @poweroffdaemon \
    --log /var/log/poweroff_daemon.log \
    --verbose
```

The daemon normally needs to run as root because it calls `/sbin/poweroff`. 

## Command-line options

| Short | Long | Argument | Default | Description |
|-------|------|----------|---------|-------------|
| `-p` | `--pidfile` | string | `/tmp/powerdaemon.pid` | PID file path. |
| `-l` | `--log` | string | none | Log file path. If not set, no logging. |
| `-i` | `--interval` | double | `1.0` | Poll interval in seconds. |
| `-t` | `--timeout` | double | `100000` | Serial read timeout in microseconds. |
| `-v` | `--verbose` | none | warning level | Increases verbosity. Each `-v` adds a level. |
| `-d` | `--device` | string | `/dev/ttyUSB0` | Serial device path. |
| `-b` | `--baudrate` | integer | `115200` | Serial baudrate. For USB CDC devices this is often irrelevant. |
| `-s` | `--sockpath` | string | `@poweroffdaemon` | UNIX-domain socket path. The leading `@` means the Linux abstract socket namespace. |
| `-h` | `--help` | none | | Show help and exit. |

## Serial protocol

The daemon communicates with the power/fan module using newline-terminated text commands. Only `\n` is used
as a line separator; `\r` is not expected.

Built-in commands:

| Command | Expected key | Meaning |
|---------|--------------|---------|
| `B\n` | `BUTTON0` | Read button state. A non-zero value triggers poweroff. |
| `V\n` | `V12` | Read voltage value. |

For more information read documentation for [power/fan control
module](https://github.com/eddyem/stm32samples/tree/master/F0%3AF030%2CF042%2CF072/Socket_fans).

Responses are parsed line by line. Each relevant line is expected to contain a key/value pair, for example:

```text
BUTTON0 = 0
V12 = 1200
```

For USB CDC serial devices, port settings such as baudrate and data format are usually ignored by the device.
The `--baudrate` option is retained for compatibility or connection over USB-UART adapter.

## Shutdown conditions

The current code triggers poweroff when either condition is true:

1. `BUTTON0` value is non-zero.
2. `V12` value is below `LOWEST_V12`.

Relevant constants are defined in `serial.h`:

```c
#define CMD_BTNCHECK        "B\n"
#define ANS_BTN0STATE       "BUTTON0"

#define CMD_GETVOLTAGE      "V\n"
#define ANS_V12             "V12"
#define LOG_V12             1150
#define LOG_VOLTAGE_DT      60
#define LOWEST_V12          970

#define MAX_WERR_CNT        5
#define MAX_RERR_CNT        15
```

The code logs voltage (in arbitrary units depending on voltage divider configuration) as:

```c
(double)voltage / 100.
```

For current device configuration value of 11.73 V means 16.8 V.

So with the current constants, `LOWEST_V12 == 970` is logged as `9.70 V` (near 13.9 V for my configuration)
and `LOG_V12 == 1150` is logged as `11.50 V` (near 16.5 V - started working after power lost). Verify that
these values match the actual units and safe thresholds of your battery pack and power module before
deploying the daemon.

Additional behavior:

- If `V12` is below `LOG_V12` but not below `LOWEST_V12`, the voltage is logged at most once per
`LOG_VOLTAGE_DT` seconds.
- When voltage returns above `LOG_V12`, a "power restored" message is logged.
- The current implementation does not implement multi-sample confirmation, debounce, or hysteresis. A single
reading can trigger poweroff.

## UNIX socket interface

The daemon starts a UNIX-domain socket server. By default it uses the Linux abstract socket name:

```text
@poweroffdaemon
```

Clients can connect and send text commands.

Default behavior:

- Any message received by the default handler is forwarded to the serial device.
- The serial response is sent back to the client.
- The special request `_help` sends the literal command `help` to the serial device and returns its response.

Important: the socket interface currently forwards arbitrary client input to the serial device. It does not
implement authentication or command filtering. On a multi-user system, use a filesystem socket with
restrictive permissions or add peer credential checks and a command whitelist.

## Runtime behavior

1. The program initializes logging and parses command-line options.
2. It checks whether another instance is already running using the PID file.
3. If a log file is configured, it opens the log.
4. The process daemonizes.
5. The parent process supervises a child process:
   - if the child exits, the parent waits 5 seconds and starts a new child;
   - the parent logs child deaths.
6. The child process:
   - sets `PR_SET_PDEATHSIG` so that it receives `SIGTERM` if the parent dies;
   - opens the serial device;
   - starts the UNIX socket server;
   - enters the polling loop.
7. On each poll, the child checks buttons and voltage.
8. If a shutdown condition is met, it logs the event and executes:

   ```c
   system("/sbin/poweroff");
   ```

9. On normal loop exit, it stops the socket server and closes the serial port.

## Signals

The daemon handles `SIGTERM`, `SIGINT`, and `SIGQUIT`; `SIGTSTP` is ignored. `SIGHUP` ignored by `sl_daemonize()`.

The current signal handler is simple and calls logging and cleanup functions directly. For production use,
consider replacing it with an async-signal-safe handler that only sets a flag or writes to a self-pipe, and
perform cleanup in the main loop.

## Logging

Logging is implemented through `usefull_macros`.

- By default, the log level is warning.
- Each `--verbose` option increases the log level.
- If `--log` is specified, the daemon opens the given log file before daemonizing.
- If the log file cannot be created, a warning is emitted.

## Safety and deployment notes

This daemon can power off the machine. Before deploying it, verify the following:

- The `V12` units and thresholds match your hardware.
- The low-voltage threshold is safe for your four-cell battery pack.
- A single noisy reading cannot cause an unwanted shutdown, or add filtering/debounce if needed.
- Loss of serial communication has the desired behavior. In the current code, repeated serial errors cause
the child process to exit and be restarted by the parent; they do not directly call `poweroff`.
- The socket interface is appropriately protected.
- The PID file path is safe. The default `/tmp/powerdaemon.pid` may be unsuitable for a root daemon on a
multi-user system.
- Only one process can access the serial device. The current code does not set an exclusive serial lock.
- `/sbin/poweroff` exists and works on the target system.

## Usage example

Because the program daemonizes itself, you can run it over OpenRC's `local` service, creating bash-script
`powerdaemon.start` in `/etc/local.d`:

```bash
#!/bin/bash

/usr/local/sbin/powerdaemon -vvv -l /RAID/powerdaemon.log -p /tmp/powerdaemon.pid &
```

The main process doesn't run `fork()`, so you can start it even with `supervise-daemon` or other services.

Adjust paths, user, and permissions to match your system.

## License

This project is licensed under the GNU General Public License, version 3 or later. See the source file
headers for details.
