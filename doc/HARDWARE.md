# Hardware List – Version 0.1

## 1. Computers and Boards


| # | Component | Qty | Purpose | Notes |
|---|---|---|---|---|
| H1 | Laptop/PC running Linux (native, or VM with bridged networking) | 4 | Controller side, development, Wireshark | Using the Laptops/PCs in our possesion |
| H2 | STM32 NUCLEO-144 with Ethernet  | 2-4 | Target board (Zephyr) |

## 2. I2C Peripherals (two different devices required)

We choose one **read-oriented** and one **write-oriented** device so that both directions are exercised. 

| # | Device | Type | Why |
|---|---|---|---|
| P1 | **Temperature sensor**, e.g. TMP102, or environmental sensor BME280 | read (register read with repeated START)  | Simple, classic "write register, then read" sequence |
| P2 | **Display** e.g OLED display 128×64, SSD1306 | write (many bytes) | Visible result for the live demo, longer write transactions |


## 3. Cabling and Small Parts

| # | Component | Qty | Purpose |
|---|---|---|---|
| K1 | Micro USB cable | 4 - * | For power supply |
| K2 | Ethernet cable (Cat5e or better) | 4 - * | Laptop ↔ board, optionally via a switch |
| K3 | Simple Ethernet switch  | 1 | Multiple participants, or capturing via a third computer |
| K4 | Breadboard + jumper wires (female-female, male-female) + resistors | 4 sets | I2C wiring |
