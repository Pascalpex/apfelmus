# Hardware List – Version 0.1

Status values: **available** = already in our possession or provided by the university · **open** = still to be procured.

## 1. Computers and Boards

| # | Component | Qty | Purpose | Status |
|---|---|---|---|---|
| H1 | Laptop/PC running Linux (native, or VM with bridged networking) | 4 | Controller side, development, Wireshark | available (own laptops/PCs) |
| H2 | STM32 NUCLEO-144 with Ethernet | 2–4 | Target board (Zephyr) | available (university) |

## 2. I2C Peripherals (two different devices required)

We choose one **read-oriented** and one **write-oriented** device so that both directions are exercised.

| # | Device | Type | Why | Status |
|---|---|---|---|---|
| P1 | **Temperature sensor**, e.g. TMP102, or environmental sensor BME280 | read (register read with repeated START) | Simple, classic "write register, then read" sequence | open |
| P2 | **Display**, e.g. OLED display 128×64, SSD1306 | write (many bytes) | Visible result for the live demo, longer write transactions | open |

## 3. Cabling and Small Parts

| # | Component | Qty | Purpose | Status |
|---|---|---|---|---|
| K1 | Micro USB cable | 4+ | Power supply | open |
| K2 | Ethernet cable (Cat5e or better) | 4+ | Laptop ↔ board, optionally via a switch | open |
| K3 | Simple Ethernet switch | 1 | Multiple participants, or capturing via a third computer | open |
| K4 | Breadboard + jumper wires (female-female, male-female) + resistors | 4 sets | I2C wiring | open |
