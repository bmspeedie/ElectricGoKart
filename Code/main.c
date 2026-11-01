/*
 * BMSBattery S series motor controllers firmware
 *
 * Copyright (C) Casainho, 2017.
 *
 * Released under the GPL License, Version 3

@Brandon
programmer
Blue = SWIM
Purple = RESET

display
yellow = display tx
green = display rx


 */

#include <stdint.h>
#include <stdio.h>
#include "stm8s.h"
#include "gpio.h"
#include "stm8s_itc.h"
#include "stm8s_gpio.h"
#include "interrupts.h"
#include "stm8s_tim2.h"
#include "motor.h"
#include "main.h"
#include "uart.h"
#include "adc.h"
#include "brake.h"
#include "cruise_control.h"
#include "timers.h"
#include "pwm.h"
#include "PAS.h"
#include "SPEED.h"
//#include "update_setpoint.h"
#include "ACAsetPoint.h"
#include "config.h"
#include "display.h"
#include "display_kingmeter.h"
#include "ACAcontrollerState.h"
#include "BOdisplay.h"
#include "ACAeeprom.h"
#include "ACAcommons.h"

//uint16_t ui16_LPF_angle_adjust = 0;
//uint16_t ui16_LPF_angle_adjust_temp = 0;

uint16_t ui16_log1 = 0;
uint8_t ui8_slowloop_flag = 0;
uint8_t ui8_veryslowloop_counter = 0;
#ifdef GOKART_MODE
static uint32_t ui32_gk_cur_sum = 0;
static uint32_t ui32_gk_x4_sum = 0;
static uint16_t ui16_gk_cur_n = 0;
static uint8_t ui8_gk_reset_flags = 0;
#endif
uint8_t ui8_ultraslowloop_counter = 0;
uint16_t ui16_log2 = 0;
uint8_t ui8_log = 0;
uint8_t ui8_i = 0; //counter for ... next loop

float float_kv = 0;
float float_R = 0;
uint8_t a = 0; //loop counter

static int16_t i16_deziAmps;


/////////////////////////////////////////////////////////////////////////////////////////////
//// Functions prototypes

// main -- start of firmware and main loop
int main(void);

//With SDCC, interrupt service routine function prototypes must be placed in the file that contains main ()
//in order for an vector for the interrupt to be placed in the the interrupt vector space.  It's acceptable
//to place the function prototype in a header file as long as the header file is included in the file that
//contains main ().  SDCC will not generate any warnings or errors if this is not done, but the vector will
//not be in place so the ISR will not be executed when the interrupt occurs.

//Calling a function from interrupt not always works, SDCC manual says to avoid it. Maybe the best is to put
//all the code inside the interrupt

// Local VS global variables
// Sometimes I got the following error when compiling the firmware: motor.asm:750: Error: <r> relocation error
// and the solution was to avoid using local variables and define them as global instead

// Brake signal interrupt
void EXTI_PORTA_IRQHandler(void) __interrupt(EXTI_PORTA_IRQHANDLER);
// Speed signal interrupt
void EXTI_PORTC_IRQHandler(void) __interrupt(EXTI_PORTC_IRQHANDLER);
// PAS signal interrupt
void EXTI_PORTD_IRQHandler(void) __interrupt(EXTI_PORTD_IRQHANDLER);

// Timer1/PWM period interrupt
void TIM1_UPD_OVF_TRG_BRK_IRQHandler(void) __interrupt(TIM1_UPD_OVF_TRG_BRK_IRQHANDLER);

// Timer2/slow control loop
void TIM2_UPD_OVF_TRG_BRK_IRQHandler(void) __interrupt(TIM2_UPD_OVF_TRG_BRK_IRQHANDLER);

// UART2 receivce handler
void UART2_IRQHandler(void) __interrupt(UART2_IRQHANDLER);
void UART2_TX_IRQHandler(void) __interrupt(UART2_TX_IRQHANDLER);


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

int main(void) {
#ifdef GOKART_MODE
	// remember why we booted (RST_SR: 1 WWDG, 2 IWDG, 4 ILLOP, 8 SWIM, 16 EMC; 0 = power-on/NRST)
	ui8_gk_reset_flags = *(volatile uint8_t *) 0x50B3;
	*(volatile uint8_t *) 0x50B3 = 0x1F; // clear
#endif
	//set clock at the max 16MHz
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);

	gpio_init();
	brake_init();
#if !GOKART_IGNORE_AUX_INPUTS
	while (brake_is_set()); // hold here while brake is pressed -- this is a protection for development
#endif
	debug_pin_init();
	light_pin_init();

#if !GOKART_IGNORE_AUX_INPUTS
	// Initialize new control pins
	cruise_pin_init();
	reverse_pin_init();
	self_study_pin_init();
	low_high_pin_init();
	three_speed_pin_init();
	hard_boot_pin_init();
#endif
#if GOKART_REVERSE_SWITCH
	hard_boot_pin_init(); // PG0 with pull-up: the reverse switch
#endif
	back_emf_pins_init();
	timer2_init();
	uart_init();
	eeprom_init();
	controllerstate_init();
#if GOKART_REVERSE_SWITCH
	motor_direction_init(); // after the EEPROM has set ui8_s_motor_angle, before the first hall read
#endif
	initErpsRatio();
	pwm_init();
	hall_sensor_init();
	adc_init();
	PAS_init();
	SPEED_init();
	aca_setpoint_init();
#if (defined (DISPLAY_TYPE) && defined (DISPLAY_TYPE_KINGMETER)) || defined DISPLAY_TYPE_KT_LCD3 || defined BLUOSEC
	display_init();
#endif

	//  ITC_SetSoftwarePriority (ITC_IRQ_TIM1_OVF, ITC_PRIORITYLEVEL_2);

	enableInterrupts();

	watchdog_init(); //init watchdog after enabling interrupt to have fast loop running already

#if (SVM_TABLE == SVM)
	TIM1_SetCompare1(126 << 1);
	TIM1_SetCompare2(126 << 1);
	TIM1_SetCompare3(126 << 1);
#elif (SVM_TABLE == SINE) || (SVM_TABLE == SINE_SVM)
	TIM1_SetCompare1(126 << 2);
	TIM1_SetCompare2(126 << 2);
	TIM1_SetCompare3(126 << 2);
#endif

	hall_sensors_read_and_action(); // needed to start the motor
	//printf("Back in Main.c\n");

	for (a = 0; a < NUMBER_OF_PAS_MAGS; a++) {// array init
		ui16_torque[a] = 0;
	}
#ifdef DIAGNOSTICS
	printf("System initialized\r\n");
#endif
	while (1) {

		uart_send_if_avail();
#ifdef GOKART_MODE
		// average both current ADC pins between telemetry reports
		// p6 is PB6, the shunt amplifier (the sample from the PWM update
		// interrupt; ~7 counts at 4.6 A). p4 is PB4, not on the shunt.
		{
			uint16_t ui16_p6_now = ui16_adc_read_motor_total_current();
			ui32_gk_cur_sum += ui16_p6_now;
			ui32_gk_x4_sum += ui16_adc_read_x4_value();
			ui16_gk_cur_n++;
			// same samples, averaged per 50 Hz tick for the regen current loop (consumed by aca_setpoint)
			if (ui16_gk_ibat_n < 65535) {
				ui32_gk_ibat_sum += ui16_p6_now;
				ui16_gk_ibat_n++;
			}
		}
#endif

		updateSpeeds();
		updatePasStatus();

#if (defined (DISPLAY_TYPE) && defined (DISPLAY_TYPE_KINGMETER)) || defined DISPLAY_TYPE_KT_LCD3 || defined BLUOSEC
		display_update();
#endif

		// scheduled update of setpoint and duty cycle (slow loop, 50 Hz)
		if (ui8_slowloop_flag) {
#ifdef DISPLAY_TYPE_KT_LCD3
			display_tick();
#endif
			//printf("MainSlowLoop\r\n");
			//send_message();
		    debug_pin_set();
			ui8_slowloop_flag = 0; //reset flag for slow loop
			ui8_veryslowloop_counter++; // increase counter for very slow loop

			checkPasInActivity();
			updateRequestedTorque(); //now calculates tq for sensor as well
			updateSlowLoopStates();
			updateX4();
			updateLight();
			ui16_setpoint = (uint16_t) aca_setpoint(ui16_time_ticks_between_pas_interrupt, ui16_setpoint); //update setpoint

			//#define DO_CRUISE_CONTROL 1
#if DO_CRUISE_CONTROL == 1
			ui16_setpoint = cruise_control(ui16_setpoint);
#endif
			
			pwm_set_duty_cycle((uint8_t) ui16_setpoint);

			//pwm_set_duty_cycle ((uint8_t)ui16_sum_throttle);


			
			
			/****************************************************************************/
			//very slow loop for communication
			if (ui8_veryslowloop_counter > 5) {
				
				ui8_ultraslowloop_counter++;
				ui8_veryslowloop_counter = 0;

				if (ui8_ultraslowloop_counter > 2) {
					ui8_ultraslowloop_counter = 0;
					ui8_uptime++;
				}

#ifdef GOKART_MODE
#if GOKART_ANGLE_SWEEP
				// step the motor angle offset every ~3 s while the motor is being driven
				{
					static uint8_t ui8_sweep_tick = 0;
					ui16_aca_flags &= ~ANGLE_CORRECTION_ENABLED; // hold the FOC correction neutral during the sweep
					if (ui16_setpoint > 0 && ++ui8_sweep_tick >= 25) {
						ui8_sweep_tick = 0;
						ui8_s_motor_angle += GOKART_ANGLE_SWEEP_STEP;
					}
					if (ui8_s_motor_angle < GOKART_ANGLE_SWEEP_START || ui8_s_motor_angle > GOKART_ANGLE_SWEEP_END) {
						ui8_s_motor_angle = GOKART_ANGLE_SWEEP_START;
					}
				}
#endif
				// bench telemetry for the throttle monitor, every other very-slow loop (~240 ms).
				// TX is queued (uart_put_buffered, drained by the UART TX interrupt) so this no longer
				// stalls the 50 Hz slow loop, but 9600 baud only carries ~960 chars/s and the
				// LCD3 frames (300 chars/s) share the line. A running line is ~155 chars (~180 worst case), so the
				// budget is tight: a line is only started when the previous one has drained
				// (queue below 64 of 256 bytes, enough room for the line plus a frame).
				// Otherwise the line overran the queue while the motor turned and every line
				// came out shredded after "rst 8 u...".
				// "cur" is PB6 minus its boot zero. "p6" is the raw 10-bit average
				// (0 = 0 V, 1023 = ADC reference, 5 V on this board).
				static uint8_t ui8_gk_tele_div = 0;
				if ((++ui8_gk_tele_div & 1) && uart_tx_pending() < 64) {
					uint16_t ui16_n = ui16_gk_cur_n;
					uint32_t ui32_gk_cur_sum_copy = ui32_gk_cur_sum;
					uint16_t ui16_p6 = ui16_n ? (uint16_t) (ui32_gk_cur_sum / ui16_n) : 0;
					uint16_t ui16_p4 = ui16_n ? (uint16_t) (ui32_gk_x4_sum / ui16_n) : 0;
					int16_t i16_cur = ui16_n ? (int16_t) ui16_p6 - (int16_t) ui16_current_cal_b : 0;
					ui32_gk_cur_sum = 0;
					ui32_gk_x4_sum = 0;
					ui16_gk_cur_n = 0;
					// "hold" is the throttle-hold test: slow-loop ticks left before the duty is re-sampled (0 = live throttle)
					// "rev" electrical revolutions, "hb" backward hall steps, "hx" non-neighbour hall jumps (all wrap at 65536)
					// (a line that does not fit in the 256-byte TX queue is truncated, never blocks)
#if GOKART_ANGLE_SWEEP
					// sweep build: report the angle offset under test instead of p6/p4 (line must stay < ~130 chars),
					// and "cur" in 1/16 ADC counts (the window average has sub-count resolution) so the current
					// minimum vs angle is resolvable
					(void) ui16_p4;
					(void) ui16_p6;
					i16_cur = ui16_n ? (int16_t) ((uint32_t) (ui32_gk_cur_sum_copy * 16UL) / ui16_n) - (int16_t) (ui16_current_cal_b * 16) : 0;
					printf("thr %u duty %u hall %d hc %u hi %u erps %u ang %u cur %d tmin %u tmax %u rst %u up %u hold %u rev %u hb %u hx %u hm %u\r\n", ui8_adc_read_throttle(), ui16_setpoint, (int) hall_sensors, ui16_hall_changes, ui16_hall_invalid, ui16_motor_speed_erps, ui8_s_motor_angle, i16_cur, ui8_gk_thr_min, ui8_gk_thr_max, ui8_gk_reset_flags, ui8_uptime, ui16_gk_hold_ticks, ui16_hall_erevs, ui16_hall_back, ui16_hall_seq_err, ui16_hall_masked);
#else
#if GOKART_THROTTLE_HOLD_TICKS > 0
					printf("thr %u duty %u hall %d hc %u hi %u erps %u cur %d tmin %u tmax %u rst %u up %u hold %u rev %u hb %u hx %u hm %u p6 %u p4 %u\r\n", ui8_adc_read_throttle(), ui16_setpoint, (int) hall_sensors, ui16_hall_changes, ui16_hall_invalid, ui16_motor_speed_erps, i16_cur, ui8_gk_thr_min, ui8_gk_thr_max, ui8_gk_reset_flags, ui8_uptime, ui16_gk_hold_ticks, ui16_hall_erevs, ui16_hall_back, ui16_hall_seq_err, ui16_hall_masked, ui16_p6, ui16_p4);
#else
					// "pc" is the FOC angle correction (127 = neutral, 360/256 deg per count)
					// "bv" is the raw PB5 pack-voltage reading (10 bit). "wz" is the wheel
					// size code the LCD3 sent (255 = none yet); the speed digits use the
					// measured LCD3_WHEEL_CIRC_MM regardless.
					// p4 (PB4) is not reported: it is not on the shunt and reads a constant 301.
					// "dm" is the pedal demand (-255 full regen .. 0 coast .. 255 full drive),
					// "rt" the regen motor-current (torque) target in PB6 counts after the speed
					// and voltage derating (0 outside regen), "bk" the back-EMF constant in use
					// (duty per erps x256 at bv GOKART_BEMF_REF_VBAT_ADC), "dir" the
					// direction in use (0 forward, 1 reverse; PG0 switch).
					(void) ui16_p4;
#if GOKART_REVERSE_SWITCH
#define GK_DIR_IN_USE ui8_gk_reverse
#else
#define GK_DIR_IN_USE 0
#endif
					printf("thr %u duty %u hall %d hc %u hi %u erps %u cur %d tmin %u tmax %u rst %u up %u rev %u hb %u hx %u hm %u p6 %u pc %u bv %u wz %u dm %d rt %u bk %u dir %u\r\n", ui8_adc_read_throttle(), ui16_setpoint, (int) hall_sensors, ui16_hall_changes, ui16_hall_invalid, ui16_motor_speed_erps, i16_cur, ui8_gk_thr_min, ui8_gk_thr_max, ui8_gk_reset_flags, ui8_uptime, ui16_hall_erevs, ui16_hall_back, ui16_hall_seq_err, ui16_hall_masked, ui16_p6, ui8_position_correction_value, ui16_adc_read_battery_voltage(), lcd_configuration_variables.ui8_wheel_size, i16_gk_demand, ui8_gk_regen_target, ui16_gk_bemf_k_x256, GK_DIR_IN_USE);
#endif
#endif
					ui8_gk_thr_min = ui8_gk_thr_max = 0;
#if GOKART_HALL_TRACE
					// hall changes as state:dwell (PWM cycles of 64 us), oldest first; the backward step is 8 entries before the end
					{
						// besides the backward-step trigger in motor.c, capture 16 fresh changes every ~1 s while
						// turning so the sector dwells (hall placement) can be read at steady speed
						static uint8_t ui8_trace_arm_tick = 0;
						if (++ui8_trace_arm_tick >= 8) {
							ui8_trace_arm_tick = 0;
							if (!ui8_hall_trace_frozen && ui8_hall_trace_post == 0 && ui16_motor_speed_erps > 3) ui8_hall_trace_post = 16;
						}
					}
					if (ui8_hall_trace_frozen) {
						uint8_t ui8_t;
						printf("hd");
						for (ui8_t = 0; ui8_t < 16; ui8_t++) {
							uint8_t ui8_i = (ui8_hall_trace_pos + ui8_t) & 15;
							printf(" %u:%u", ui8_hall_trace_state[ui8_i], ui8_hall_trace_dwell[ui8_i]);
						}
						printf("\r\n");
						ui8_hall_trace_frozen = 0; // re-arm
					}
#endif
				}
#endif


#ifdef DIAGNOSTICS
				//uint32_torquesensorCalibration=80;
				printf("%u,%u, %u, %u, %u, %u\r\n", ui16_control_state, (uint16_t) uint32_current_target, PAS_is_active, ui16_BatteryCurrent, ui16_sum_torque, (uint16_t)uint32_torquesensorCalibration);

				//printf("erps %d, motorstate %d, cyclecountertotal %d\r\n", ui16_motor_speed_erps, ui8_possible_motor_state|ui8_dynamic_motor_state, ui16_PWM_cycles_counter_total);

				//printf("cheatstate, %d, km/h %lu, Voltage, %d, setpoint %d, erps %d, current %d, correction_value, %d\n", ui8_offroad_state, ui32_speed_sensor_rpks, ui8_BatteryVoltage, ui16_setpoint, ui16_motor_speed_erps, ui16_BatteryCurrent, ui8_position_correction_value);

				//printf("kv %d, erps %d, R %d\n", (uint16_t)(float_kv*10.0) , ui16_motor_speed_erps, (uint16_t)(float_R*1000.0));

				/*for(a = 0; a < 6; a++) {			// sum up array content
						 putchar(uint8_t_hall_case[a]);
						 }
				putchar(ui16_ADC_iq_current>>2);
				putchar(ui8_position_correction_value);
				putchar(255);*/
				// printf("%d, %d, %d, %d, %d, %d\r\n", (uint16_t) uint8_t_hall_case[0], (uint16_t)uint8_t_hall_case[1],(uint16_t) uint8_t_hall_case[2],(uint16_t) uint8_t_hall_case[3], (uint16_t)uint8_t_hall_case[4], (uint16_t)uint8_t_hall_case[5]);
				//printf("%d, %d, %d, %d, %d, %d, %d,\r\n", ui8_position_correction_value, ui16_BatteryCurrent, ui16_setpoint, ui8_regen_throttle, ui16_motor_speed_erps, ui16_ADC_iq_current>>2,ui16_adc_read_battery_voltage());


				//printf("correction angle %d, Current %d, Voltage %d, sumtorque %d, setpoint %d, km/h %lu\n",ui8_position_correction_value, i16_deziAmps, ui8_BatteryVoltage, ui16_sum_throttle, ui16_setpoint, ui32_speed_sensor_rpks);
#endif
			}//end of very slow loop

			debug_pin_reset();
		}// end of slow loop
	}// end of while(1) loop
}

