/*
Generic display init and update functions
Written by jenkie and Thomas Jarosch
Functions for the Nokia graphical screen mainly by m--k
King-Meter library and support written by Michael Fabry

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <stdio.h>
#include "stm8s.h"
#include "display.h"
#include "main.h"
#include "config.h"
#include "stm8s_itc.h"
#include "uart.h"
#include "adc.h"
#include "brake.h"
#include "ACAeeprom.h"
#include "interrupts.h"
#include "ACAcontrollerState.h"
#include "ACAcommons.h"

#ifdef DISPLAY_TYPE_KT_LCD3

display_view_type display_view;
display_mode_type display_mode; //currently display mode
float current_display;
uint8_t battery_percent_fromcapacity = 11; //hier nur als Konstante um Batterie normal zu senden....


uint8_t ui8_tx_buffer[12];
uint8_t ui8_j;
uint8_t ui8_crc;
uint16_t ui16_wheel_period_ms = 4500;
uint8_t ui8_throttle_percent = 0;
uint8_t ui8_battery_soc = 0;
uint8_t ui16_error;

/* Battery icon. B1 codes from the KT-LCD3 protocol: 1 = border flashing
 * (under voltage), 3 = empty box, 4/8/12/16 = 1..4 bars. Thresholds are
 * in tenths of a volt across the LCD3_BATTERY_EMPTY_DV..FULL_DV span. */
#define BATTERY_LEVELS 6
#define BATTERY_SPAN_DV (LCD3_BATTERY_FULL_DV - LCD3_BATTERY_EMPTY_DV)
static const uint8_t ui8_battery_codes[BATTERY_LEVELS] = {1, 3, 4, 8, 12, 16};
static const uint16_t ui16_battery_min_dv[BATTERY_LEVELS] = {
	0,
	LCD3_BATTERY_EMPTY_DV,                              // empty box
	LCD3_BATTERY_EMPTY_DV + BATTERY_SPAN_DV / 10,       // 1 bar  at 10 %
	LCD3_BATTERY_EMPTY_DV + BATTERY_SPAN_DV / 4,        // 2 bars at 25 %
	LCD3_BATTERY_EMPTY_DV + BATTERY_SPAN_DV / 2,        // 3 bars at 50 %
	LCD3_BATTERY_EMPTY_DV + (BATTERY_SPAN_DV * 3) / 4,  // 4 bars at 75 %
};
static uint8_t ui8_battery_level = 0;
static uint16_t ui16_battery_adc_acc = 0;
static uint8_t ui8_send_tick = 0;

/* Pack voltage in tenths of a volt from the filtered 10 bit ADC reading,
 * using the same calibration constant the rest of the firmware uses. */
static uint16_t battery_decivolts(void) {
	uint32_t acc = (uint32_t) (ui16_battery_adc_acc >> 4) * ui8_s_battery_voltage_calibration * 10;
	return (uint16_t) (acc >> 10);
}

static void update_battery_level(void) {
	uint16_t dv;
	uint8_t level;

	// 16 sample running average, updated at 50 Hz
	ui16_battery_adc_acc -= ui16_battery_adc_acc >> 4;
	ui16_battery_adc_acc += ui16_adc_read_battery_voltage();
	dv = battery_decivolts();

	level = 0;
	while (level + 1 < BATTERY_LEVELS && dv >= ui16_battery_min_dv[level + 1]) {
		level++;
	}
	// hysteresis: only step up once clear of the threshold
	if (level > ui8_battery_level && dv < ui16_battery_min_dv[level] + LCD3_BATTERY_HYST_DV) {
		level = ui8_battery_level;
	}
	ui8_battery_level = level;
	ui8_battery_soc = ui8_battery_codes[level];
}
uint8_t ui8_rx_buffer[13];
uint8_t ui8_rx_buffer_counter = 0;
uint8_t ui8_byte_received;
uint8_t ui8_moving_indication = 0;
uint8_t ui8_UARTCounter = 0;

volatile struc_lcd_configuration_variables lcd_configuration_variables;

void display_init(){
	// 0x00 is a valid wheel code (16"), so mark it unset until the LCD talks
	lcd_configuration_variables.ui8_wheel_size = 0xff;
}

static void send_message(void);

/* Wheel circumference in mm for the wheel size code the LCD3 sends us
 * (see LCD3_to_S12SN-1.txt). 25.4 * pi = 79.8 mm per inch of diameter. */
static uint16_t wheel_circumference_mm(void) {
	uint8_t inches;
	switch (lcd_configuration_variables.ui8_wheel_size) {
		case 0x0e: inches = 10; break;
		case 0x02: inches = 12; break;
		case 0x06: inches = 14; break;
		case 0x00: inches = 16; break;
		case 0x04: inches = 18; break;
		case 0x08: inches = 20; break;
		case 0x0c: inches = 22; break;
		case 0x10: inches = 24; break;
		case 0x14: inches = 26; break;
		case 0x18: inches = 28; break; // 700c
		default:   inches = LCD3_DEFAULT_WHEEL_INCH; break;
	}
	return (uint16_t) (((uint16_t) inches * 798 + 5) / 10);
}

/* Battery current in 0.25 A steps for B8, clamped to 0..255. */
static uint8_t battery_current_quarter_amps(void) {
	uint16_t adc = ui16_adc_read_x4_value();
	uint32_t qa;
	// a few counts of noise around the boot offset would otherwise show as watts at idle
	if (adc <= ui16_x4_cal_b + LCD3_CURRENT_DEADBAND) {
		return 0;
	}
	// amps = counts * 10 / current_cal_a, quarter amps = counts * 40 / cal_a
	qa = ((uint32_t) (adc - ui16_x4_cal_b) * 40 + (ui8_current_cal_a >> 1)) / ui8_current_cal_a;
	return qa > 255 ? 255 : (uint8_t) qa;
}

static void send_message(void) {

	// prepare moving indication info
	ui8_moving_indication = 0;
	if (brake_is_set()) {
		ui8_moving_indication |= (1 << 5);
	}
	//if (ebike_app_cruise_control_is_set ()) { ui8_moving_indication |= (1 << 3); }
	//if (throttle_is_set ()) { ui8_moving_indication |= (1 << 1); }
	//if (pas_is_set ()) { ui8_moving_indication |= (1 << 4); }


	// Throttle position, 0..100 over the calibrated ADC range, read raw
	// so there is no filtering on the controller side.
	ui8_throttle_percent = (uint8_t) map(ui8_adc_read_throttle(), ADC_THROTTLE_MIN_VALUE, ADC_THROTTLE_MAX_VALUE, 0, 100);

	// Show throttle percent in the speed digits. The LCD turns the wheel
	// period into km/h with the wheel size set in its own menu, so use the
	// size it reports back to us: period_ms = circumference_mm * 3.6 / kmh.
	if (ui8_throttle_percent == 0) {
		ui16_wheel_period_ms = 4500; // reads as 0
	} else {
		uint32_t period = ((uint32_t) wheel_circumference_mm() * 36) / ((uint16_t) ui8_throttle_percent * 10);
		ui16_wheel_period_ms = period > 4500 ? 4500 : (uint16_t) period;
	}

	ui8_tx_buffer [0] = 65;
	// B1: battery level
	ui8_tx_buffer [1] = ui8_battery_soc;
	// B2: 24V controller
	ui8_tx_buffer [2] = ui8_battery_voltage_nominal;
	// B3: speed, wheel rotation period, ms; period(ms)=B3*256+B4;
	ui8_tx_buffer [3] = (ui16_wheel_period_ms >> 8) & 0xff;
	ui8_tx_buffer [4] = ui16_wheel_period_ms & 0xff;

	//Send confirming signal for activating offroad mode
	if (ui8_offroad_state == 4) { //quitting signal for offroad mode enabled. Shows about 80 km/h for three seconds

		ui8_tx_buffer [3] = (100 >> 8) & 0xff; //100ms are about 80 km/h @ 28" 2200mm wheel circumference
		ui8_tx_buffer [4] = 100 & 0xff;
	}

	// B5: error info display
	ui8_tx_buffer [5] = ui16_error;
	// B6: CRC: xor B1,B2,B3,B4,B5,B7,B8,B9,B10,B11
	// 0 value so no effect on xor operation for now
	ui8_tx_buffer [6] = 0;
	// B7: moving mode indication, bit
	// throttle: 2
	ui8_tx_buffer [7] = ui8_moving_indication;
	// B8: battery current in 0.25 A units; the LCD multiplies by its own
	// voltage reading to show watts. Taken from the low-pass filtered
	// current channel against the zero offset measured at boot, with
	// current_cal_a counts per 10 A as elsewhere in the firmware.
	ui8_tx_buffer [8] = battery_current_quarter_amps();

	// B9: motor temperature, degrees = B9 + 15
	ui8_tx_buffer [9] = i8_motor_temperature - 15;
	// B10 and B11: 0
	ui8_tx_buffer [10] = 0;
	ui8_tx_buffer [11] = 0;

	// calculate CRC xor
	ui8_crc = 0;
	for (ui8_j = 1; ui8_j <= 11; ui8_j++) {
		ui8_crc ^= ui8_tx_buffer[ui8_j];
	}
	ui8_tx_buffer [6] = ui8_crc;

	// send the package over UART
	for (ui8_j = 0; ui8_j <= 11; ui8_j++) {
		uart_put_buffered(ui8_tx_buffer [ui8_j]);
	}
}

/* Called from the 50 Hz loop. The battery filter runs every tick; a frame
 * goes out every 40 ms (a 12 byte frame takes 12.5 ms at 9600 baud) so the
 * throttle readout tracks as fast as the LCD will follow. */
void display_tick(void) {
	update_battery_level();

	if (++ui8_send_tick < 2) {
		return;
	}
	ui8_send_tick = 0;
	send_message();
}

/********************************************************************************************/
// Process received package from the LCD
//

void digestLcdValues(void) {

	ui8_assistlevel_global = lcd_configuration_variables.ui8_assist_level + 80; // always add max regen 
	ui8_walk_assist = lcd_configuration_variables.ui8_WalkModus_On;
	// added by DerBastler Light On/Off
	light_stat = (light_stat&~128) | lcd_configuration_variables.ui8_light_On; // only update 7th bit, 1st bit is current status
	
	if (lcd_configuration_variables.ui8_max_speed != ui8_speedlimit_kph) {
		ui8_speedlimit_kph = lcd_configuration_variables.ui8_max_speed;
		eeprom_write(OFFSET_MAX_SPEED_DEFAULT, lcd_configuration_variables.ui8_max_speed);
	}
}

// see if we have a received package to be processed

void display_update() {

	//printf("display_update\r\n");

	// fill local buffer from uart ringbuffer
	uart_fill_rx_packet_buffer(ui8_rx_buffer, 13, &ui8_UARTCounter);
	
	// Check for reception of complete message
	if ((ui8_UARTCounter > 12) || (ui8_rx_buffer[ui8_UARTCounter - 1] == 0x0E)) {
		ui8_UARTCounter = 0;
		

		// validation of the package data
		ui8_crc = 0;
		for (ui8_j = 0; ui8_j <= 12; ui8_j++) {
			
			if (ui8_j == 5) continue; // don't xor B5 
			ui8_crc ^= ui8_rx_buffer[ui8_j];
		}

		// see if CRC is ok
		if (((ui8_crc ^ 10) == ui8_rx_buffer [5]) || // some versions of CRC LCD5 (??)
				((ui8_crc ^ 1) == ui8_rx_buffer [5]) || // CRC LCD3 (tested with KT36/48SVPR, from PSWpower)
				((ui8_crc ^ 2) == ui8_rx_buffer [5]) || // CRC LCD5
				((ui8_crc ^ 3) == ui8_rx_buffer [5]) || // CRC LCD5 Added display 5 Romanta
				((ui8_crc ^ 4) == ui8_rx_buffer [5]) ||
		    		((ui8_crc ^ 5) == ui8_rx_buffer [5]) ||
		    		((ui8_crc ^ 6) == ui8_rx_buffer [5]) ||
		    		((ui8_crc ^ 7) == ui8_rx_buffer [5]) ||
		    		((ui8_crc ^ 8) == ui8_rx_buffer [5]) ||
		    		((ui8_crc ^ 14) == ui8_rx_buffer [5]) ||
				((ui8_crc ^ 9) == ui8_rx_buffer [5])) // CRC LCD3
		{
			
			// added by DerBastler Light On/Off 
			lcd_configuration_variables.ui8_light_On = ui8_rx_buffer [1] & 128;
			
			lcd_configuration_variables.ui8_assist_level = ui8_rx_buffer [1] & 7;
			
			// walk assist, see https://endless-sphere.com/forums/viewtopic.php?f=2&t=73471&p=1324745&hilit=kunteng+protocol+hacked#p1109048 
			if(lcd_configuration_variables.ui8_assist_level == 6)lcd_configuration_variables.ui8_WalkModus_On = 1;
			else lcd_configuration_variables.ui8_WalkModus_On = 0;
			
			lcd_configuration_variables.ui8_max_speed = 10 + ((ui8_rx_buffer [2] & 248) >> 3) | (ui8_rx_buffer [4] & 32);
			lcd_configuration_variables.ui8_wheel_size = ((ui8_rx_buffer [4] & 192) >> 6) | ((ui8_rx_buffer [2] & 7) << 2);

			lcd_configuration_variables.ui8_p1 = ui8_rx_buffer[3];
			lcd_configuration_variables.ui8_p2 = ui8_rx_buffer[4] & 0x07;
			lcd_configuration_variables.ui8_p3 = ui8_rx_buffer[4] & 0x08;
			lcd_configuration_variables.ui8_p4 = ui8_rx_buffer[4] & 0x10;
			lcd_configuration_variables.ui8_p5 = ui8_rx_buffer[0];

			lcd_configuration_variables.ui8_c1 = (ui8_rx_buffer[6] & 0x38) >> 3;
			lcd_configuration_variables.ui8_c2 = (ui8_rx_buffer[6] & 0x37);
			lcd_configuration_variables.ui8_c4 = (ui8_rx_buffer[8] & 0xE0) >> 5;
			lcd_configuration_variables.ui8_c5 = (ui8_rx_buffer[7] & 0x0F);
			lcd_configuration_variables.ui8_c12 = (ui8_rx_buffer[9] & 0x0F);
			lcd_configuration_variables.ui8_c13 = (ui8_rx_buffer[10] & 0x1C) >> 2;
			lcd_configuration_variables.ui8_c14 = (ui8_rx_buffer[7] & 0x60) >> 5;

			digestLcdValues();
			//send_message();
		}
	}
}

#endif
