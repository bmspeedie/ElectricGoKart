/*
 * EGG OpenSource EBike firmware
 *
 * Copyright (C) Casainho, 2015, 2106, 2017.
 *
 * Released under the GPL License, Version 3
 */

#ifndef _MOTOR_H
#define _MOTOR_H

#include "main.h"
#include "interrupts.h"
#include "config.h"

// motor states
#define MOTOR_STATE_COAST 0
#define MOTOR_STATE_RUNNING_NO_INTERPOLATION 1
#define MOTOR_STATE_RUNNING_INTERPOLATION_60 2
#define MOTOR_STATE_RUNNING_INTERPOLATION_360 3

extern int8_t hall_sensors;
extern uint16_t ui16_hall_changes;
extern uint16_t ui16_hall_invalid;
extern uint16_t ui16_hall_erevs, ui16_hall_back, ui16_hall_seq_err, ui16_hall_masked;
#if GOKART_HALL_TRACE
extern uint8_t ui8_hall_trace_state[16], ui8_hall_trace_dwell[16], ui8_hall_trace_pos, ui8_hall_trace_frozen, ui8_hall_trace_post;
#endif
extern uint8_t ui8_sinetable_position;
extern uint16_t ui16_speed_inverse;
extern uint16_t ui16_PWM_cycles_counter_total;
extern uint16_t ui16_iq_current_ma;
extern uint16_t ui16_log;

// Phase/hall A<->C exchange in use (see GOKART_SWAP_PHASE_AC in config.h).
// Read by the hall sampling and the PWM output every PWM cycle.
#ifndef GOKART_SWAP_PHASE_AC
#define GOKART_SWAP_PHASE_AC 0
#endif
#if GOKART_REVERSE_SWITCH
extern uint8_t ui8_gk_reverse; // 0 forward, 1 reverse (PG0 pulled low)
#define GOKART_AC_SWAPPED() ((GOKART_SWAP_PHASE_AC) ^ ui8_gk_reverse)
void motor_direction_init (void);
void motor_set_reverse (uint8_t ui8_rev);
#else
#define GOKART_AC_SWAPPED() (GOKART_SWAP_PHASE_AC)
#endif

void hall_sensor_init (void);
void hall_sensors_read_and_action (void);
void motor_fast_loop (void);
void watchdog_init (void);

#endif /* _MOTOR_H_ */
