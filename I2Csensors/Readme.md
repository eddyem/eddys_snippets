Reading information from different T/P/H sensors
================================================

Supported sensors: AHT10, AHT15, AHT21b, BMP180, BMP280, BME280, SHT3x, SI7005




| Sensor  | Precision H/T/P | Address       | Max speed | Comment |
|---------|-----------------|---------------|-----------|---------|
| AHT10   | 2%/0.3°/-       | 0x38/0x39     | 400k      | ADDR selects lowest address bit |
| AHT15   | 2%/0.3°/-       | 0x38          | 400k      | what is the difference from AHT10? |
| AHT21b  | 3%/0.5°/-       | 0x38          | 400k      | |
| BMP180  | -/1°/12Pa       | 0x77          | 3.4M      | could also works by SPI |
| BME280  | 3%/1°/0.2Pa     | 0x76/77       | 3.4M      | SDO allows to select lowest I2C address bit; supports SPI |
| SHT30   | 3%/0.3°/-       | 0x44/0x45     | 1M        | SHT31 have higher humidity precision (2%); ADDR selects address lowest bit; hav ALERT pin  |
| SI7005  | 4.5%/1°/-       | 0x40          | 400k      | ~CS can select sensor if several are on bus |
-------------------------------------------------------------------


## Install library

1. Download: `git clone` or other way.
2. Create building directory: `mkdir mk`. Go into it: `cd mk`.
3. Run `cmake`: `cmake ..`.
4. Build and install: `make && su -c "make install"`.

### Cmake options

Marked options are ON by default:

- [ ] DEBUG - compile in debug mode;
- [x] EXAMPLES - build also examples (they won't be installed, you can use them just in build dir).

## How to use

After installing library you can use it including `i2csensorsPTH.h` into your code and linking with `-l i2csensorsPTH`.
Also you can use `pkg-config` after installing library:

```
pkg-config --libs --cflags i2csensorsPTH

``` 


### Base types

#### `sensor_status_t`

Status of given sensor. `SENS_NOTINIT` means that you should init device; also if you get `SENS_ERR` you should try to reinit it.
Receiving error on init function means that there's troubles on the bus or with sensor.  

```
typedef enum{
    SENS_NOTINIT,   // wasn't inited
    SENS_BUSY,      // measurement in progress
    SENS_ERR,       // error occured
    SENS_RELAX,     // do nothing
    SENS_RDY,       // data ready - can get it
} sensor_status_t;
```

#### `sensor_props_t` 

Properties: if the corresponding field sets, the device have this ability. `flags` allows to use all together as bit fields. 

```
typedef union{
    struct{
        uint8_t T   : 1; // can temperature (degC)
        uint8_t H   : 1; // can humidity (percent)
        uint8_t P   : 1; // can pressure (hPa)
        uint8_t htr : 1; // have heater
    };
    uint32_t flags;
} sensor_props_t;
```

#### `sensor_data_t` 

Gathered data. The fields that are zeroed in sensor's properties are undefined.

```
typedef struct{
    double T;    // temperature, degC
    double H;    // humidity, percents
    double P;    // pressure, hPa
} sensor_data_t;
```

### Functions

#### `int sensors_open(const char *dev)`

Open I2C device by path `dev`. Returns `TRUE` if all OK.

#### `void sensors_close()`

Close I2C device.

#### `char *sensors_list()`

Returns allocated string with comma-separated names of all supported sensors. Don't forget to `free` it later.

#### `sensor_t* sensor_new(const char *name)`

Search `name` in list of supported sensors and, if found, returns pointer to sensors structure. Returns `NULL` if sensor not failed or some error oqqured.

#### `void sensor_delete(sensor_t **s)`

Delete all memory, allocated for given sensor.

#### `sensor_props_t sensor_properties(sensor_t *s)`

Allows to check sensor's properties.

#### `int sensor_init(sensor_t *s, uint8_t address)`

Try to find given sensor on the bus and run initial procedures (like calibration and so on). The `address` argument shoul be zero to use default I2C address or non-zero for custom.
Returns `TRUE` if all OK.

#### `int sensor_heater(sensor_t *s, int on)`

Turn on (`on == 1`) or off (`on == 0`) sensor's heater (if sensor supported it). Returns `FALSE` if sensor don't support heater or some error occured during operations.

#### `int sensor_start(sensor_t *s)`

Start measurement process. While measuring, you should poll sensor until data would be ready (or you get timeout error). 

#### `sensor_status_t sensor_process(sensor_t *s)`

Polling sensor and gathering all data in simple finite-state machine. Checks if sensor is still busy and asks for data portion on each measuring stage. 
Returns current sensor's state. If you get `SENS_RDY`, you can ask for data.

#### `int sensor_getdata(sensor_t *s, sensor_data_t *d)`

Get data into your variable `d`. Returns `FALSE` if data isn't ready (e.g. you didn't run `start` or sensor is still measuring).

### I2C functions

Of course, you can wish to work with I2C directly (e.g. to switch multiplexer's channel and so on), so here are some usefull functions.

#### `int sensor_writeI2C(uint8_t addr, uint8_t *data, int len)`

Write `data` array with len of `len` bytes with device address `addr`. Returns `FALSE` if failed.

#### `int sensor_readI2C(uint8_t addr, uint8_t *data, int len)`

Read `len` bytes of `data` from address `addr`. Returns `FALSE` if failed.

#### `int sensor_readI2Cregs(uint8_t addr, uint8_t regaddr, uint16_t N, uint8_t *data)`

Read content of `N` 8-bit registers starting from `regaddr` to array `data`. Returns `FALSE` if failed.

#### `int sensor_writeI2Creg(uint8_t addr, uint8_t regaddr, uint8_t data)`

Write `data` to single register `regaddr`. Returns `FALSE` if failed.

