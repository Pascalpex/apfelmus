# Quickstart

Laptop (`controller/`) reads a temperature sensor on the Nucleo board (`target/`) via ACF_I2C over UDP.
By default the board uses an **emulated** I2C temperature sensor (ADT7420 @ `0x48`, fed with the CPU temperature).

| Folder | Content |
|---|---|
| `common/` | Packing/unpacking ACF_I2C into IEEE 1722 frames (Open1722), shared by both sides |
| `target/` | Zephyr app for the Nucleo-F429ZI/F439ZI |
| `target/emul/` | Everything emulated (only built with `I2C1722_EMUL=ON`, the default) |
| `controller/` | Linux command line tool |

## Requirements
- Board connected via USB (ST-LINK) and Ethernet. Network: DHCP by default, static IP see `target/prj.conf`

## 1. Board
```bash
cd src/target
west build -b nucleo_f429zi
west flash
picocom -b 115200 /dev/ttyACM0
```


## 2. Laptop
```bash
cd src/controller
make
./controller apfelmus.local        # or the board's IP address
```
Expected: `Temperature: xx.xx °C`

## Using a real sensor
1. Connect the sensor to `i2c1`: **SDA = PB9 (D14)**, **SCL = PB8 (D15)**
2. Build without the emulator (`-p` is needed when switching):
   ```bash
   west build -p -b nucleo_f429zi -- -DI2C1722_EMUL=OFF
   west flash
   ```
3. In `controller/main.c` set `TEMP_SENSOR_ADDR` (and `TEMP_SENSOR_REG_TEMP` and the °C conversion) for sensor, then `make`.

Switch back to the emulator: `west build -p -b nucleo_f429zi`.


