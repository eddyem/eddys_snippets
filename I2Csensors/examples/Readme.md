# Examples of library usage

### logmany.c

Creates executable `log`. The purpose of this is to show how you can use library wit lots of sensors (even with fully similar)
divided by groups (to prevent same addresses on one bus) switching by PCA9548A.

Usage:
```
  -H, --hlog=arg       humidity logging file
  -P, --plog=arg       pressure logging file
  -T, --tlog=arg       temperature logging file
  -a, --muladdr=arg    multiplexer I2C address
  -d, --device=arg     I2C device path
  -h, --help           show this help
  -i, --interval=arg   logging interval, seconds (default: 10)
  -m, --presmm         pressure in mmHg instead of hPa
```

### single_sensor.c

Creates executable `single`. Open single sensor and show its parameters.

Usage:

```
  -H, --heater=arg    turn on/off heater (if present)
  -a, --address=arg   sensor's address (if not default)
  -d, --device=arg    I2C device path
  -h, --help          show this help
  -l, --list          list all supported sensors
  -m, --presmm        pressure in mmHg instead of hPa
  -s, --sensor=arg    sensor's name

```
