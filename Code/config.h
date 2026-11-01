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
// Spare controller wires are ignored: brake (PA4), cruise (PA2), self-study
// (PA5), low/high (PB3), 3-speed (PG1). None of them can hold startup, cut
// drive, or change a mode. The hard boot wire (PG0) is the reverse switch,
// see GOKART_REVERSE_SWITCH.
#define GOKART_IGNORE_AUX_INPUTS 1
// Reverse switch on the "hard boot" input (PG0). The pin has the MCU
// pull-up and idles high = forward; pulled to ground = reverse. The pin is
// read at boot and then every 50 Hz tick; a change must hold for
// DEBOUNCE_TICKS (5 = 100 ms) and is only applied with the bridge off and
// the motor stopped (no electrical revolution for ~0.2 s), so flipping the
// switch while rolling does nothing until the kart has stopped and the
// pedal is at zero torque. "dir" in the telemetry is the direction in use.
#define GOKART_REVERSE_SWITCH 1
#define GOKART_REVERSE_DEBOUNCE_TICKS 5
// Commutation offset (ui8_s_motor_angle) used in reverse. Reverse undoes the
// A<->C swap below, which mirrors the stator angle about phase B, and keeps
// the hall table (it is mirror-symmetric: ANGLE_6_60 + ANGLE_1_240 =
// ANGLE_2_120 + ANGLE_3_180 = 198, as it must be for 120 deg spaced
// sensors). The forward offset M fixes the field relative to the hall
// edges; the rotor's own position relative to the hall edges cancels in
// forward but counts double in reverse, so M cannot be mirrored on its own.
// If forward runs the field D units ahead of the rotor flux, the reverse
// offset is 2*D - M - 70 (mod 256). With D = 64 (90 deg) + a that gives
// 112 + 2a for M = 202. The forward sweep put the current minimum at 186
// and the clean window at 190-210, so a is 0..16 and the reverse offset
// 112..144; 128 is the middle. Confirm on the bench with GOKART_ANGLE_SWEEP
// (set START/END around this value, run the sweep with the switch in
// reverse): the FOC correction only absorbs +/-16 units.
#define GOKART_REVERSE_MOTOR_ANGLE 128
#define GOKART_MAX_SPEED_KPH 80  // Maximum speed for go-kart
#define GOKART_THROTTLE_DIRECT 1  // Direct throttle mapping without ramping
// One-pedal throttle geometry. The usable travel is the ADC_THROTTLE_MIN/MAX
// range with RAIL counts clipped off each end, so the demand reaches its rail
// a little before the pedal reaches its mechanical stop and sits there
// despite ADC noise. ZERO_PCT of that travel is the zero-torque point: above
// it the pedal is a drive command (0..full duty), below it a regen command
// (0..full regen current, full at the bottom rail). NEUTRAL counts either
// side of the zero point give zero demand (bridge off, coast) so the pedal
// can be held at zero torque without flipping between drive and regen.
// With 43..182: travel 49..176, zero at 87 (85..89 neutral), regen 49..85,
// drive 89..176.
#define GOKART_THROTTLE_RAIL_COUNTS 6
#define GOKART_THROTTLE_ZERO_PCT 30
#define GOKART_THROTTLE_NEUTRAL_COUNTS 2
// derived, do not edit
#define GOKART_THR_LO (ADC_THROTTLE_MIN_VALUE + GOKART_THROTTLE_RAIL_COUNTS)
#define GOKART_THR_HI (ADC_THROTTLE_MAX_VALUE - GOKART_THROTTLE_RAIL_COUNTS)
#define GOKART_THR_ZERO (GOKART_THR_LO + ((GOKART_THR_HI - GOKART_THR_LO) * GOKART_THROTTLE_ZERO_PCT) / 100)
#define GOKART_THR_DRIVE_START (GOKART_THR_ZERO + GOKART_THROTTLE_NEUTRAL_COUNTS)
#define GOKART_THR_REGEN_START (GOKART_THR_ZERO - GOKART_THROTTLE_NEUTRAL_COUNTS)
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
// Regenerative braking (pedal below the zero point). The bridge is
// complementary PWM, so a duty below the back-EMF-equivalent duty pushes
// current back into the pack; the controller closes a loop on the PB6 shunt
// (battery current) to hold the braking torque the pedal asks for.
// Torque follows the motor current, and motor current = battery current /
// duty fraction. Battery current alone is a poor target: it is zero at the
// BEMF duty, peaks at about half of it, and falls back to zero at duty 0
// where the motor is shorted (maximum torque, nothing into the pack), and
// at low speed the pedal's battery current is simply unreachable, which
// would send the duty to the floor and the torque far past what was asked.
// So the pedal sets a motor current; each tick it is converted to the
// battery current expected at the present duty (motor current x duty/256),
// which is monotonic in duty, and that is what the loop holds. The loop is
// entered from an estimate of the BEMF duty (about zero current) and is
// never allowed below half of that estimate.
#define GOKART_REGEN 1
// Motor (phase-equivalent) current at full regen, in PB6 counts
// (1 count ~ 0.66 A on this board, so 60 ~ 40 A). The pedal is linear in
// this. For reference the drive side applies no current limit at all.
#define GOKART_REGEN_MAX_MOTOR_COUNTS 60
// Cap on the current into the pack whatever the pedal asks, PB6 counts
// (30 ~ 20 A, ~1.4 kW at 72 V). Whichever of the two limits is reached
// first holds.
#define GOKART_REGEN_MAX_BAT_COUNTS 30
// Above FULL_ERPS the full torque is available. Below it the target fades
// linearly with speed so the braking does not end in a step. Below MIN_ERPS
// regen is off (coast): the halls cannot give a usable speed there.
#define GOKART_REGEN_FULL_ERPS 40
#define GOKART_REGEN_MIN_ERPS 10
// Back-EMF constant: duty per erps, x256, at a PB5 pack-voltage reading of
// REF_VBAT_ADC (the duty for a given speed scales with 1/Vbat). No-load
// bench (31 Oct, bv 736): duty 189 at 151 erps, 99 at 78, 90 at 70 -> 1.25.
// The firmware refines this on its own whenever the bridge is on with the
// battery current within 3 counts of zero (that is the BEMF duty); "bk" in
// the telemetry is the value in use.
#define GOKART_BEMF_DUTY_X256_PER_ERPS 320
#define GOKART_BEMF_REF_VBAT_ADC 736
// Regen starts this percent below the BEMF estimate so an estimate on the
// high side does not give a drive kick on entry. Kept small: one duty is
// 0.28 V at 72 V, several amps through a low-resistance motor.
#define GOKART_REGEN_ENTRY_MARGIN_PCT 2
// Loop: each 50 Hz tick the duty moves by (battery current error in counts)
// * GAIN_X256 / 256, at most SLEW duty per tick. One duty step is several
// amps on a low-resistance motor, so the gain is kept small. If the
// current alternates tick to tick in regen, halve GAIN; if it takes more
// than ~0.3 s to settle, double it.
#define GOKART_REGEN_GAIN_X256 32
#define GOKART_REGEN_SLEW 8
// Overvoltage taper on the raw 10 bit PB5 reading ("bv" in the telemetry):
// the regen target falls linearly from full at TAPER to zero at CUT. The
// PB5 calibration for the 72 V pack is not settled (bv 736 was read on the
// bench supply, believed 60 V, ~12.3 counts/V, which would put 84 V past
// the ADC's full scale), so these defaults are set just under full scale:
// a full pack gets no regen until bv has been checked against a meter.
#define GOKART_REGEN_VBAT_TAPER_ADC 980
#define GOKART_REGEN_VBAT_CUT_ADC 1010
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
// 1 or 0. The reverse switch inverts this at run time (motor.h,
// GOKART_AC_SWAPPED).
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
// Wheel circumference, mm, that the LCD3 uses to turn the wheel period into
// km/h; the throttle digits are scaled through it. Measured on the bench
// (31 Oct) from two readings against the monitor: 65 shown for 34 sent with
// a 1277 mm assumption, 72 shown for 60 sent with 2075 mm. Both give the LCD
// about 2460 mm (26" x 79.8 would be 2075). Changing the wheel size in the
// LCD's menu changes this. The code the LCD reports is "wz" in the telemetry.
#define LCD3_WHEEL_CIRC_MM 2460
// B2, the controller's nominal pack voltage. 72 V system (20S li-ion).
#define LCD3_CONTROLLER_VOLTAGE 72
// Bench test: cycle the B7 status symbols 5 s each instead of the brake flag.
// On this LCD3 (31 Oct): bit 4 assist, bit 3 cruise "C", bit 5 cruise with
// assist flashing (no brake symbol exists), bit 0 the circle with arrows,
// which the normal build shows while the pedal is below the zero point.
#define LCD3_SYMBOL_TEST 0
// PB6 counts above the idle zero before the watts field leaves 0
#define LCD3_CURRENT_DEADBAND 1
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
