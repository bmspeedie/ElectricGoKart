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
#define MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT 202
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
// Re-measured 31 Oct with clean (locked-out) edges at offset 202, no load:
//   51-117 erps, cur 1-4:  4/6/2/3/1/5 = 40/78/43/79/41/80 deg  (table within 3 deg)
//   159 erps, cur 7:                    45/72/49/71/49/73 deg
// The widths move with stator current (short sectors grow), so the table is
// only right for one load; left as is.
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
#define GOKART_MAX_DUTY 255
// Duty applied as soon as the throttle leaves the deadzone. Below ~22 the
// motor only cogs/hunts at the edge of stall (dead time eats most of the
// tiny applied voltage), so start the usable range there.
#define GOKART_MIN_DUTY 22
// Throttle ADC noise rejection. Bench data (hold test, 31 Oct): with the foot
// steady the raw reading sits within 0-2 counts most of the time but throws
// isolated single-sample spikes of 5-25 counts (one of 46). Averaging smears
// each spike into an 80 ms bump that the motor follows; a median drops it.
// Filters are applied in this order:
// 1. Median of the last 3 samples: removes any single-sample outlier, costs at
//    most one 20 ms tick of delay on a real step. 0 disables.
#define GOKART_THROTTLE_MEDIAN3 1
// 2. Backlash deadband: the accepted value trails the median by up to this
//    many counts and only moves when the median pulls it, so +/-1 count
//    dither is ignored and a real move is followed at full resolution
//    (~1.8 duty steps per ADC count with MAX_DUTY 255). 0 disables.
#define GOKART_THROTTLE_DEADBAND 1
// 3. Exponential average over ~2^N samples (2 -> ~80 ms). Off: it slows the
//    response and only attenuates spikes rather than rejecting them.
#define GOKART_THROTTLE_FILTER_SHIFT 0
// Bench test for hunting: when the throttle leaves the deadzone, sample it
// once and hold that duty for this many 50 Hz ticks (250 = 5 s) no matter
// what the throttle reading does, then re-sample and hold again. Releasing
// the throttle, braking or an invalid hall state still stops immediately.
// Motor still hunts with duty frozen -> motor side (commutation/angle/halls).
// Motor runs smooth with duty frozen -> analog side (throttle reading).
// 0 disables and restores live throttle control.
// Result 31 Oct: speed was solid with the duty frozen -> the hunting was the
// analog reading. Filters above added in response; hold test off since.
#define GOKART_THROTTLE_HOLD_TICKS 0
// The go-kart motor is wired so it turns in this firmware's "reverse" sense:
// hall base angles step backwards while the 60-degree interpolation steps
// forwards. Swapping phase A<->C and hall A<->C in software (same as
// swapping two motor wires and two hall wires) makes the sequence forward.
#define GOKART_SWAP_PHASE_AC 1
// Hall edge bounce rejection. Bench, 31 Oct: the MCU saw 1.91 hall changes
// per expected change (21019 over 1837 electrical revs), 4996 of them steps
// backwards through the cycle, 0 invalid states, 0 two-bit jumps: 45 % of
// edges read A-B-A-B. While the motor is turning (erps > 3) a backward step
// is ignored and the previous state kept. "hb" in the telemetry counts them.
// 0 disables.
// Tried 31 Oct: hunting got worse. hb went to 63526 in one run (~2.8 rejected
// reads per edge, so the "back" state lasts ~180 us, not one sample) and the
// rejection also fought the rotor genuinely rocking at start-up. Off.
#define GOKART_HALL_REJECT_BACKSTEP 0
// Hall edge lockout. Dwell traces (31 Oct) showed the dither window around a
// bad edge is a fixed angle, up to ~6 % of an electrical revolution, and the
// true edge sits inside it (dwell before the bounce matches a clean sector
// measured to the early flip, dwell after matches measured from the final
// flip). The shortest real sector is ~37 deg = 10 %. So: accept the first
// change, then ignore the halls for this percentage of the last measured
// revolution. No latency, and no following the oscillation. Active only once
// erps > 3 (a measured period exists); at crawl every read is accepted.
// "hm" in the telemetry counts the raw changes that were masked. 0 disables.
// 7 % verified to 75 erps (hc/(6*rev) 1.91 -> 1.001, hb 4089 -> 8, motor
// smooth). At 161 erps 7 % rounds to 7 cycles against a 6-cycle dither, so
// 9 % for margin; the short sector at that current is ~13 %.
#define GOKART_HALL_LOCKOUT_PCT 9
// Hall edge trace: every raw hall change is logged as state:dwell (PWM cycles
// of 64 us) in a 16-entry ring; on the first backward step while turning,
// keep 8 more changes and freeze. The main loop prints "hd s:d s:d ..."
// (oldest first) and re-arms. Raw-sample traces (31 Oct) showed each bad
// edge as early flip ~4 samples, back ~3, then final: a fixed ~18 deg
// electrical window at every speed, i.e. the sensor output itself dithers.
#define GOKART_HALL_TRACE 1
// Bench calibration: step the motor angle offset (ui8_s_motor_angle) by
// GOKART_ANGLE_SWEEP_STEP every ~3 s while running and report it as "ang"
// in the telemetry line, so the best offset can be read off speed vs angle.
// Disable once MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT has been set for this motor.
// First pass (step 16, 166..246, bouncing hall edges) gave 214. Fine passes
// (step 4, 166..238, duty 59-65, no load, 31 Oct) after the hall lockout:
//   170-178  slow, high current (over-retarded)
//   186      current minimum
//   190-210  speed flat, current flat, hall sensors completely clean (hm 0)
//   214      speed +15 %, hall dither begins (the "noise" the lockout hides)
//   218+     rocking, stall by 230
// i.e. 214 sat on the advance-side cliff, and the hall dither is the stator
// field disturbing the sensors there. 202 = middle of the clean window.
#define GOKART_ANGLE_SWEEP 0
#define GOKART_ANGLE_SWEEP_STEP 4
#define GOKART_ANGLE_SWEEP_START 170
#define GOKART_ANGLE_SWEEP_END 214 // inclusive, then wraps to START
#define LCD3_DEFAULT_WHEEL_INCH 26  // wheel size assumed for the speed digits until the LCD3 reports its own setting
#define LCD3_CURRENT_DEADBAND 1  // PB6 counts above the boot zero before the watts field leaves 0
// The LCD multiplies the current byte by the voltage on its own supply wires,
// in tenths of a volt (the VOL page). That feed is the bench supply, not the
// pack, so the byte is scaled by pack_voltage / this value.
#define LCD3_SUPPLY_DV 309
// LCD3 battery icon, tenths of a volt. 20S li-ion: 84.0 V full, 60.0 V empty.
// Bars at 10/25/50/75 % of the span; below EMPTY the icon border flashes.
#define LCD3_BATTERY_FULL_DV 840
#define LCD3_BATTERY_EMPTY_DV 600
#define LCD3_BATTERY_HYST_DV 5  // 0.5 V hysteresis stepping up a bar

#endif /* CONFIG_H_ */
