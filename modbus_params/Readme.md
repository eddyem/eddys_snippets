# modbus_params

**modbus_params** is a command-line tool and network server for interacting with Modbus RTU
devices. It uses a human‑readable **dictionary** that maps register addresses to mnemonic codes,
supports periodic dumping of selected parameters to a TSV file, and provides a TCP/UNIX socket
interface for remote access.

The project is hosted at:  
[https://github.com/eddyem/eddys_snippets/tree/master/modbus_params](https://github.com/eddyem/eddys_snippets/tree/master/modbus_params)

## Features

- Read and write Modbus holding registers by **address** or **mnemonic code**.
- Define a dictionary (`code → register`) with optional comments and read‑only flags.
- Dump selected registers periodically to a TSV file.
- Start a **TCP or UNIX socket server** that accepts commands to:
  - Read/write registers (by code or address).
  - List dictionary entries or aliases.
  - Start/stop and rename the dump file.
- Define **aliases** – macros that expand to other commands or expressions.
- Verbose logging with configurable level.

## Dependencies

- [libmodbus](https://libmodbus.org/) (Modbus RTU backend)
- [usefull_macros](https://github.com/eddyem/snippets_library) (argument parsing, socket helpers,
  logging macros)
- POSIX threads (pthread)
- Standard C library

## Building

Clone the repository together with the required submodule:

```bash
git clone --depth=1 https://github.com/eddyem/eddys_snippets.git
cd eddys_snippets/modbus_params
```

Then compile with `make`:

```bash
make
```

The resulting executable is named `modbus_params`. You can copy this file wherever you want, e.g.

```bash
su -c "cp modbus_params /usr/local/bin
```

## Command‑line usage

```
modbus_params [options]
```

### General options

| Option | Argument          | Description |
|--------|-------------------|-------------|
| `-h`, `--help` | – | Show help and exit |
| `-v`, `--verbose` | – | Increase verbosity (can be used multiple times) |
| `-D`, `--dictionary` | *file* | Dictionary file (format: `code register value readonly`). *Required for most operations.* |
| `-a`, `--alias` | *file* | Aliases file (format: `name : command`). Optional. |
| `-s`, `--slave` | *ID* | Modbus slave ID (default: 1) |
| `-d`, `--device` | *path* | Serial device (default: `/dev/ttyUSB0`) |
| `-b`, `--baudrate` | *rate* | Baud rate (default: 9600) |

### Read/write operations

| Option | Description |
|--------|-------------|
| `-r`, `--readr` *addr* | Read register by address (multiply: `-r 100 -r 200`) |
| `-R`, `--readc` *code* | Read register by dictionary code (multiply) |
| `-w`, `--writer` *addr=val* | Write value to register by address (multiply) |
| `-W`, `--writec` *code=val* | Write value to register by dictionary code (multiply) |
| `-O`, `--outdic` *file* | Read **all** registers listed in the dictionary and save them (with current values) into a new dictionary file |

### Dumping (periodic TSV output)

| Option | Description |
|--------|-------------|
| `-k`, `--dumpkey` *code* | Add a dictionary key to the dump list (multiply) |
| `-o`, `--outfile` *file* | TSV file for the dump |
| `-t`, `--dumptime` *seconds* | Dump interval (default: 0.1 s) |

### Server mode

| Option | Description |
|--------|-------------|
| `-N`, `--node` *spec* | Start a server. For TCP: IP address or hostname with port after ':' (e.g. `:1212` or `localhost:1212`). For UNIX socket: a path (e.g. `/tmp/mb_sock`). |
| `-U`, `--unixsock` | Use UNIX domain socket instead of TCP (requires `-N` with a path) |

If `-N` is given, the program runs as a server **after** executing any immediate read/write/dump
operations. The server stays alive until interrupted (Ctrl+C).

## File formats

### Dictionary file

Each line defines one register. Empty lines and text after `#` are ignored.  
Format:

```
<code> <register> <value> <readonly>
```

- `code` – mnemonic string (no spaces).
- `register` – 16‑bit Modbus address (decimal).
- `value` – initial/default value (16‑bit, not used by the tool except for documentation).
- `readonly` – `0` (writable) or `1` (read‑only).

Example (from `dictionary.dic`):

```
F00.00  61440  1  0   # Motor management (V/F)
F00.01  61441  0  0   # Selecting the START command task
...
A02.01  41473  0  1   # Output frequency (read-only)
```

### Aliases file

Aliases provide a way to define shortcuts or composite commands.  
Format:

```
name : expression   # optional comment
```

The `expression` is a string that will be interpreted as a command (register read/write, another
alias, or a built‑in server command). Aliases are resolved recursively.

Example:

```
stop  : F00.00=0            # stop motor
start : F00.00=1            # start motor
freq  : F00.11              # read current frequency
```

When the server receives `stop`, it writes `0` to register mapped to `F00.00`.

### Dump file (TSV)

When dumping is active, the tool writes a tab-separated file with a header line:

```
#   time,s  <code1> <code2> ...
```

Each subsequent line contains a timestamp (seconds since dump start) followed by the current value
of each registered key. Unreadable registers are shown as `----`.

Example:
```
#   time,s F00.00 F00.11
     0.000 1      5000
     0.102 1      5000
```

## Server protocol

The server listens for plain text commands, terminated by newline (`\n`).  
Each command is a key‑value pair: `key[=value]`.

- **Getter** – just the key → returns current value.
- **Setter** – `key=value` → writes the value (if the register is writable).

### Built‑in keys (handlers)

| Key | Description |
|-----|-------------|
| `list` | As a getter: prints the whole dictionary (one line per entry). As a setter (e.g. `list=F00.01`): prints details of the given code or register address. |
| `alias` | Getter: prints all aliases. Setter (e.g. `alias=myalias`): prints that specific alias. |
| `newdump` | Getter: returns current dump file name. Setter (e.g. `newdump=/path/file.dump`): closes current dump file (if any), opens a new one and starts dumping. |
| `clodump` | Stops the dump thread and closes the dump file. No value expected. |

### Default handler (register access)

If the key is **not** a built‑in handler, the program tries to interpret it as:

- A **register address** (decimal integer) → find the corresponding dictionary entry.
- A **dictionary code** → find the entry.
- An **alias** → recursively expand and execute.

Then:

- **Getter** (only key) → reads the register and replies `key=value`.
- **Setter** (`key=value`) → writes the value to the register (if not read‑only) and replies `OK`.

### Examples (using `netcat`, `telnet` or `socat`)

**TCP server** (assuming `-N :5020`):

```
$ nc localhost 5020
F00.00
F00.00=1
F00.00=0
OK
list=F00.01
F00.01 61441 0 0 # Selecting the START command task
newdump=/tmp/motor.dump
OK
clodump
OK
```

**UNIX socket**:

```
$ socat - UNIX-CONNECT:/tmp/mb_sock
F00.11
F00.11=5000
```

## Examples

### 1. Simple read by address

```bash
modbus_params -D dictionary.txt -s 1 -d /dev/ttyUSB0 -b 9600 -r 61440
```

### 2. Write a value using a code

```bash
modbus_params -D dictionary.txt -W "F00.00=1"
```

### 3. Dump two registers every 0.5 seconds to `data.tsv`

```bash
modbus_params -D dictionary.txt -k F00.00 -k F00.11 -o data.tsv -t 0.5
```

(No server started – the program will dump until interrupted.)

### 4. Start a local server with dumping already active

```bash
modbus_params -D dictionary.txt -k F00.00 -o /tmp/dump.csv -t 1 -N localhost:5020
```

Now you can connect to port 5020 and use `newdump` to change the output file, or `clodump` to stop.

### 5. Read all dictionary registers and save current values

```bash
modbus_params -D dictionary.dic -O current_state.dic
```

## Signals

- `SIGINT` (Ctrl+C), `SIGTERM`, `SIGQUIT` – gracefully close Modbus, dump files, and exit.
- `SIGHUP`, `SIGTSTP` – ignored (no action).

## Notes

- The dictionary is **shared** between the main program and the server thread. Do not modify the
  dictionary file while the program is running.
- The dump thread uses a separate timer; if a read operation hangs, the dump may stall.
- When using UNIX sockets, the path can be prefixed with `\0` or `@` (for abstract sockets). 
  The program accepts plain paths for filesystem sockets.
- The `usefull_macros` library provides the `sl_dtime()` high‑resolution timer and thread‑safe
logging macros.

## License

Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
GNU General Public License v3 or later.  

Full text of the GPLv3 is available at <https://www.gnu.org/licenses/gpl-3.0.html>.
