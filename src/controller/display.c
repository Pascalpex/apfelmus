#include <errno.h>
#include <unistd.h>

#include "display.h"

/* Pin-Belegung des PCF8574 am Display */
#define LCD_RS (1 << 0) // Register Select (0 = Command, 1 = Data)
#define LCD_RW (1 << 1) // Read/Write (0 = Write)
#define LCD_EN (1 << 2) // Enable
#define LCD_BL (1 << 3) // Backlight (1 = An, 0 = Aus)

/* Helper: Sendet 1 Byte direkt über I2C an den PCF8574 */
static int lcd_write_pcf(struct i2c1722_link *link, uint8_t data)
{
	return i2c1722_write(link, LCD_BUS, LCD_I2C_ADDR, &data, 1);
}

/* Helper: Überträgt ein 4-Bit-Nibble mit gepulstem Enable-Pin */
static int lcd_send_nibble(struct i2c1722_link *link, uint8_t nibble, uint8_t mode)
{
	uint8_t data = (nibble & 0xF0) | mode | LCD_BL;
	int ret;

	// Enable HIGH
	ret = lcd_write_pcf(link, data | LCD_EN);
	if (ret != 0)
	{
		return ret;
	}

	usleep(1); // Min. 450 ns für Enable-Puls halten

	// Enable LOW (Übernahme der Daten durch das Display)
	ret = lcd_write_pcf(link, data & ~LCD_EN);
	if (ret != 0)
	{
		return ret;
	}

	usleep(50); // Kurze Ausführungszeit abwarten
	return 0;
}

/* Helper: Sendet ein komplettes Byte (split in High- und Low-Nibble) */
static int lcd_send_byte(struct i2c1722_link *link, uint8_t val, uint8_t mode)
{
	/*
	 * Both nibbles with their Enable pulses in one I2C write (one network
	 * round trip): the PCF8574 outputs the bytes one after another, each
	 * byte takes ~90 us on the bus (> 450 ns Enable pulse, > 37 us execution).
	 */
	uint8_t hi = (val & 0xF0) | mode | LCD_BL;
	uint8_t lo = ((val << 4) & 0xF0) | mode | LCD_BL;
	uint8_t seq[4] = {hi | LCD_EN, hi, lo | LCD_EN, lo};

	return i2c1722_write(link, LCD_BUS, LCD_I2C_ADDR, seq, sizeof(seq));
}

/* Sendet einen Steuerbefehl (RS = 0) */
static int lcd_send_cmd(struct i2c1722_link *link, uint8_t cmd)
{
	return lcd_send_byte(link, cmd, 0);
}

/* Sendet ein einzelnes Textzeichen (RS = LCD_RS) */
static int lcd_send_data(struct i2c1722_link *link, uint8_t data)
{
	return lcd_send_byte(link, data, LCD_RS);
}

/* Initialisiert das LCD-Display im 4-Bit-Modus */
int lcd_init(struct i2c1722_link *link)
{
	usleep(50 * 1000); // Warten nach dem Einschalten (>40ms)

	// Hardware-Reset/Synchronisationssequenz (3x 0x30 senden)
	lcd_send_nibble(link, 0x30, 0);
	usleep(5 * 1000);
	lcd_send_nibble(link, 0x30, 0);
	usleep(150);
	lcd_send_nibble(link, 0x30, 0);

	// Umschalten auf 4-Bit-Interface
	lcd_send_nibble(link, 0x20, 0);

	// Display-Einstellungen vornehmen
	lcd_send_cmd(link, 0x28); // 4-Bit-Modus, 2 Zeilen, 5x8 Font
	lcd_send_cmd(link, 0x0C); // Display EIN, Cursor AUS, Blinken AUS
	lcd_send_cmd(link, 0x01); // Display löschen
	usleep(2 * 1000);				 // Clear Display benötigt ca. 1.52 ms
	lcd_send_cmd(link, 0x06); // Auto-Increment Schreibzeiger nach rechts

	return 0;
}

/* Löscht den Inhalt des Displays */
int lcd_clear(struct i2c1722_link *link)
{
	int ret = lcd_send_cmd(link, 0x01);
	usleep(2 * 1000);
	return ret;
}

/* Setzt den Schreibcursor auf Zeile (0 oder 1) und Spalte (0 bis 15) */
int lcd_set_cursor(struct i2c1722_link *link, uint8_t row, uint8_t col)
{
	static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
	if (row >= 2)
	{
		row = 0;
	}
	return lcd_send_cmd(link, 0x80 | (col + row_offsets[row]));
}

/* Schreibt einen String zeichenweise auf das Display */
int lcd_print(struct i2c1722_link *link, const char *str)
{
	if (link == NULL || str == NULL)
	{
		return -EINVAL;
	}

	while (*str != '\0')
	{
		int ret = lcd_send_data(link, (uint8_t)*str);
		if (ret != 0)
		{
			return ret;
		}
		str++;
	}

	return 0;
}