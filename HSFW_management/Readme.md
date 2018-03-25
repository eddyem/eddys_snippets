Edmund Optics high-speed filter wheel management
================================================

This command-line utility allows you to manage with HSFW turrets: check and change their positions,
list connected devices and their property, rename wheels' and filters' names stored in EEPROM of
given turret.

## Command line options:


    -H, --home              move to home position
    -N, --wheel-name=arg    wheel name
    -W, --wheel-id=arg      letter wheel identificator
    -h, --help              show this help
    -i, --filter-id=arg     filter identificator like "A3"
    -n, --filter-name=arg   filter name
    -p, --f-position=arg    filter position number
    -s, --serial=arg        turret serial (with leading zeros)
    --list                  list only present devices' names
    --list-all              list all stored names
    --rename                rename stored wheels/filters names
    --resetnames            reset all names to default values

## Usage examples

#### Short list of connected devices (wheel ID, turret serial and current position):
    HSFW_manage
    'B' '00000563' 1
    'A' '00000532' 1

#### List all devices connected
    HSFW_manage --list

    Connected wheel properties
    Wheel ID 'B' , name 'Sloan', serial '00000563' , 5 filters:
        1: '1'
        2: '2'
        3: '3'
        4: '4'
        5: '5'
    current position: 1

    Connected wheel properties
    Wheel ID 'A' , name 'UBVRI', serial '00000532' , 5 filters:
        1: 'U'
        2: 'B'
        3: 'V'
        4: 'R'
        5: 'I'
    current position: 1

#### Move wheel by turret's serial and position number
    HSFW_manage -s 00000563 -p 3

Will move first turret (wheel 'A' named 'UBVRI') into third position (filter 'V').

#### Move wheel by filter name
    HSFW_manage -nV

Is equivalent of previous.

#### Rename wheel 'B' of first turret
    HSFW_manage --rename -s00000563 -WB -N "New name"

Assigns "New name" to wheel 'B' of first turret in spite of its absence in current moment.
You can control all changes by `HSFV_manage --list` (show only wheels presents) or
`HSFV_manage --list-all` (show all EEPROM information).
