/*
 * config.h
 *
 *  Automatically created by OSEC Parameter Configurator
 *  Author: stancecoke
 *  Modified for Electric Go-Kart - Direct Throttle Control
 */

#ifndef CONFIG_H_
#define CONFIG_H_

//#define DIAGNOSTICS 1

#define NUMBER_OF_PAS_MAGS 12
#define limit 28
#define timeout 3125
#define wheel_circumference 2230L
#define limit_without_pas 6
#define ADC_THROTTLE_MIN_VALUE 43
#define ADC_THROTTLE_MAX_VALUE 182
#define BATTERY_VOLTAGE_MIN_VALUE 148
#define BATTERY_CURRENT_MAX_VALUE 64L
#define PHASE_CURRENT_MAX_VALUE 350L
#define REGEN_CURRENT_MAX_VALUE 50L
#define MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT 214
#define current_cal_a 46
#define LEVEL_1 26
#define LEVEL_2 38
#define LEVEL_3 55
#define LEVEL_4 80
#define LEVEL_5 100
#define MORSE_TIME_1 50
#define MORSE_TIME_2 50
#define MORSE_TIME_3 50
#define RAMP_END 1500
#define P_FACTOR 0.5
#define I_FACTOR 0.2
#define GEAR_RATIO 69L
#define PAS_THRESHOLD 1.9
#define RAMP_START 64000
#define limit_with_throttle_override 35
#define CORRECTION_AT_ANGLE 127
// Go-kart motor hall placement, measured on the bench (hall dwell at constant
// speed): states 4, 2, 1 last ~37 deg electrical and 6, 3, 5 last ~83 deg,
// i.e. each hall sensor is high for ~203 deg. 256 units = 360 deg.
#define ANGLE_4_0 1
#define ANGLE_6_60 27
#define ANGLE_2_120 86
#define ANGLE_3_180 112
#define DISPLAY_TYPE_KT_LCD3
#define ANGLE_1_240 171
#define ANGLE_5_300 197
#define TQS_CALIB 0.0
#define ACA 4764
#define EEPROM_NOINIT // eeprom will not be cleared
#define EEPROM_INIT_MAGIC_BYTE 93 // makes sure (chance of fail 1/255) eeprom is invalidated after flashing new config
#define ADC_BATTERY_VOLTAGE_K 67
#define ACA_EXPERIMENTAL 128
#define BATTERY_VOLTAGE_MAX_VALUE 208

// Go-Kart specific modifications
#define GOKART_MODE 1  // Enable go-kart mode
#define GOKART_MAX_SPEED_KPH 80  // Maximum speed for go-kart
#define GOKART_THROTTLE_DIRECT 1  // Direct throttle mapping without ramping
#define GOKART_THROTTLE_DEADZONE 10  // counts above ADC_THROTTLE_MIN_VALUE before power starts
#define GOKART_MAX_DUTY 100
// Throttle ADC smoothing: exponential average over ~2^N of the 50 Hz samples
// (2 -> ~80 ms). 0 disables. Added after the raw reading was seen jumping
// +/-15 counts between consecutive loops with the motor running.
#define GOKART_THROTTLE_FILTER_SHIFT 2  // throttle span maps to 0..this, out of 255
// The go-kart motor is wired so it turns in this firmware's "reverse" sense:
// hall base angles step backwards while the 60-degree interpolation steps
// forwards. Swapping phase A<->C and hall A<->C in software (same as
// swapping two motor wires and two hall wires) makes the sequence forward.
#define GOKART_SWAP_PHASE_AC 1
// Bench calibration: step the motor angle offset (ui8_s_motor_angle) by
// GOKART_ANGLE_SWEEP_STEP every ~3 s while running and report it as "ang"
// in the telemetry line, so the best offset can be read off speed vs angle.
// Disable once MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT has been set for this motor.
#define GOKART_ANGLE_SWEEP 0
#define GOKART_ANGLE_SWEEP_STEP 16
#define GOKART_ANGLE_SWEEP_START 166
#define GOKART_ANGLE_SWEEP_END 246 // inclusive, then wraps to START
#define LCD3_DEFAULT_WHEEL_INCH 26  // wheel size assumed for the speed digits until the LCD3 reports its own setting
#define LCD3_CURRENT_DEADBAND 4  // ADC counts above the boot offset before the watts field shows anything
// LCD3 battery icon, tenths of a volt. 20S li-ion: 84.0 V full, 60.0 V empty.
// Bars at 10/25/50/75 % of the span; below EMPTY the icon border flashes.
#define LCD3_BATTERY_FULL_DV 840
#define LCD3_BATTERY_EMPTY_DV 600
#define LCD3_BATTERY_HYST_DV 5  // 0.5 V hysteresis stepping up a bar

#endif /* CONFIG_H_ */
