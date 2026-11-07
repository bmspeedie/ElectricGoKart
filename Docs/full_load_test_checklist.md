# Pending items and full-load test checklist

Running list of everything still open. Tick items off as they are done and  
add new ones at the end of the relevant section.

## Wiring

`Code/main.c`'s header comment has an older version of the display and
programmer colours (programmer blue = SWIM, purple = RESET; display
yellow = tx, green = rx): bring the two into line.

- [ ] Display connector
  - red = +14V
  - green = tx (data into the display)
  - yellow = rx (data from the display)
  - black = gnd
  - blue = unused (on/off/reset)
- [ ] Programmer (ST-LINK/V2)
  - purple = clk
  - blue = io (swim)
  - yellow = 5V
  - green = gnd
- [ ] Cat6 cable
  - orange = tx (to display)
  - orange/white = rx
  - green = +14V
  - green/white = gnd
  - blue = sw clk
  - brown = 5V
  - brown/white = swim
  - blue/white = unused
- [ ] Display to Cat6 (if the display stays blank, swap green and yellow
      first)
  - red (+14V) = green
  - black (gnd) = green/white
  - green (data into display) = orange
  - yellow (data from display) = orange/white
  - blue (on/off/reset) = not connected
- [ ] Programmer to Cat6 (optional). No separate programmer ground in the
      Cat6; it shares green/white with the display.
  - yellow (5V) = brown
  - green (gnd) = green/white
  - purple (clk) = blue
  - blue (swim/io) = brown/white
- [ ] Motor phases: large green/yellow/blue to green/yellow/blue on the
      controller, colour to colour.
- [ ] Motor halls: small red/black/yellow/green/blue to the same colours on
      the controller's "HALL" loom. Original connectors are poor; solder or
      use a better terminal strip.
- [ ] Throttle: controller red/black/green. Hand twist throttle for
      initial testing, draw wire encoder on the kart. Mapping: 0 % = full
      regen, 30 % = zero torque, 100 % = full torque. Calibration: item 5.
- [ ] Hard boot: two wires. Open = anticlockwise (firmware forward,
      `dir 0`), closed = clockwise (`dir 1`, reverse offset not yet tuned,
      item 7).
- [ ] Battery: big red = battery +, big black = battery -. "E-lock" powers
      the controller; switch it to battery + when ready to run.

## STM8S105S LQFP44

Top view, package rotated so pin 1 (NRST) and the dot are at the lower
right. Numbers run counterclockwise. This is the 44-pin 10×10 mm part,
which has PG0 and PG1. Probe the STM8 pins, not the motor phase wires or
battery positive.

Top edge, left to right:

| Pin | Port | Signal |
| --- | ---- | ------ |
| 22 | PE6 | unused |
| 21 | PB0 | phase A low-side PWM |
| 20 | PB1 | phase B low-side PWM |
| 19 | PB2 | phase C low-side PWM |
| 18 | PB3 | low/high |
| 17 | PB4 | filtered current |
| 16 | PB5 | battery voltage |
| 15 | PB6 | unfiltered current |
| 14 | PB7 | throttle |
| 13 | VSSA | analog ground |
| 12 | VDDA | analog supply |

Left edge, top to bottom:

| Pin | Port | Signal |
| --- | ---- | ------ |
| 23 | PE5 | hall A |
| 24 | PC1 | phase A high-side PWM |
| 25 | PC2 | phase B high-side PWM |
| 26 | PC3 | phase C high-side PWM |
| 27 | PC5 | hall B |
| 28 | VSSIO | I/O ground |
| 29 | VDDIO | I/O supply |
| 30 | PC6 | back EMF phase C |
| 31 | PC7 | unused |
| 32 | PG0 | hard boot |
| 33 | PG1 | 3 speed |

Bottom edge, left to right. Pin 38 is also SWIM.

| Pin | Port | Signal |
| --- | ---- | ------ |
| 34 | PE2 | unused |
| 35 | PE1 | back EMF phase A |
| 36 | PE0 | unused |
| 37 | PD0 | unused |
| 38 | PD1 | unused (SWIM) |
| 39 | PD2 | back EMF phase B |
| 40 | PD3 | hall C |
| 41 | PD4 | back EMF enable |
| 42 | PD5 | display serial TX |
| 43 | PD6 | unused |
| 44 | PD7 | unused |

Right edge, top to bottom. Pin 1 is the bottom pin. Pin 6 is the 1.8 V
regulator capacitor, not a GPIO.

| Pin | Port | Signal |
| --- | ---- | ------ |
| 11 | PA6 | unused |
| 10 | PA5 | self study |
| 9 | PA4 | reverse |
| 8 | VDDIO | I/O supply |
| 7 | VDD | digital supply |
| 6 | VCAP | 1.8 V capacitor |
| 5 | VSS | digital ground |
| 4 | VSSIO | I/O ground |
| 3 | PA2 | cruise |
| 2 | PA1 | unused (oscillator in) |
| 1 | NRST | reset |

Analog Discovery 2, all digital pins as inputs. Ground is pin 5 (VSS) or
pin 13 (VSSA). Leave the waveform outputs and V+/V− unconnected. Scope
range ±5 V; logic sample rate 1 MS/s is enough for the 15.6 kHz PWM.

| Discovery | Package pin | Signal |
| --------- | ----------- | ------ |
| GND, scope 1− and 2− | 5 or 13 | logic ground |
| Scope 1+ (orange) | 14 PB7 | throttle |
| Scope 2+ (blue) | 15 PB6 | unfiltered current |
| DIO 0 | 21 PB0 | phase A low-side PWM |
| DIO 1 | 20 PB1 | phase B low-side PWM |
| DIO 2 | 19 PB2 | phase C low-side PWM |
| DIO 3 | 24 PC1 | phase A high-side PWM |
| DIO 4 | 25 PC2 | phase B high-side PWM |
| DIO 5 | 26 PC3 | phase C high-side PWM |
| DIO 6 | 23 PE5 | hall A |
| DIO 7 | 27 PC5 | hall B |
| DIO 8 | 40 PD3 | hall C |

## Full-load test checklist

Tuned aggressively for maximum torque and throttle response, but only ever
run free spinning on the bench. Expect further tuning once it is loaded and
on an axle with its own inertia, resonance and backlash.

Bench tuning (31 Oct 2026) got the motor smooth at no load on a 60 V / 5 A
supply, up to 164 erps. Everything below needs real current and can only be
settled on the battery. Firmware under test: `main` at `8efa1d4` or later
(rotor offset 202, hall lockout 9 %, throttle median+deadband, FOC
correction inert).

## Before the first run

- [ ] Controller flashed from current `main`; `GOKART_ANGLE_SWEEP`,
  ```
  `GOKART_THROTTLE_HOLD_TICKS`, `GOKART_HALL_REJECT_BACKSTEP` all 0 in
  `Code/config.h`.
  ```
- [ ] Telemetry available if at all possible: laptop + Analog Discovery on the
  ```
  display UART line, `python3 Code/test/pin_readback/throttle_monitor.py`,
  page at http://127.0.0.1:8765. Without it the checks below reduce to
  feel and the LCD3 watts/speed digits.
  ```
- [ ] After each run, save the data before it scrolls off (the monitor keeps
  ```
  the last 2000 reports, about 4 min):
  `curl -s 127.0.0.1:8765/history > runN_history.json`
  `curl -s 127.0.0.1:8765/ht > runN_ht.json`
  ```
- [ ] Battery full, LCD3 reads 84.0 V-ish; note the pack voltage at the start.



## Telemetry fields

One line every ~240 ms (TX is queued and interrupt driven; it never stalls the control loop):
`thr duty hall hc hi erps cur tmin tmax rst up rev hb hx hm p6 pc bv wz dm rt bk dir`


| field    | meaning                                                    | healthy                                       |
| -------- | ---------------------------------------------------------- | --------------------------------------------- |
| `hc`     | hall changes accepted                                      | `hc / (6 * rev)` = 1.00                       |
| `hi`     | invalid hall reads (0 or 7)                                | 0                                             |
| `rev`    | electrical revolutions                                     |                                               |
| `hb`     | backward hall steps                                        | 0 once rolling; a few on spin-up is normal    |
| `hx`     | non-neighbour hall jumps                                   | 0                                             |
| `hm`     | raw flips swallowed by the lockout                         | `hm / (6 * rev)` < 0.1 at no load; see item 1 |
| `cur`    | shunt amp, counts above boot zero                          | ~1 count per amp on this board                |
| `pc`     | FOC angle correction                                       | 127 (inert; see item 3)                       |
| `hd ...` | 16 hall changes as `state:dwell`, every ~1 s               | see item 2                                    |
| `rst`    | reset flags                                                | should not change mid-run (8 = normal)        |
| `bv`     | raw PB5 pack-voltage reading, 10 bit                       | see item 8                                    |
| `dm`     | pedal demand, -255 full regen .. 0 coast .. 255 full drive |                                               |
| `rt`     | regen motor-current target, PB6 counts                     | 0 outside regen                               |
| `bk`     | back-EMF constant in use, duty per erps x256               | settles, no drift                             |
| `dir`    | direction in use, 0 forward / 1 reverse                    | matches the switch                            |




## 1. Hall sensors under load (highest priority)

Why: the hall edges dither when the stator field at the sensors is strong
enough. At no load that only happened at offset >= 214; at 202 the sensors
were clean. Load multiplies the stator field.

- [ ] Hard acceleration from rest, several times; hold full throttle up a
  ```
  grade if there is one.
  ```
- [ ] `hm / (6 * rev)` over the run. Below ~0.5: fine. Above: the field is
  ```
  reaching the sensors at load; try offset 198 (`MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT`),
  not higher.
  ```
- [ ] `hb` stays flat once rolling; `hx` 0; `hi` 0.
- [ ] Listen for the hunting/growl that the bench had at 214. If it appears
  ```
  only under load, same action: offset down a few units.
  ```
- [ ] Starting torque from rest acceptable with a rider on board. If weak,
  ```
  the offset may be too far retarded for high current; try 206 and
  re-check `hm`.
  ```



## 2. Hall sector widths vs current

Why: no-load traces showed the sector widths move with current
(4/6/2/3/1/5 = 40/78/43/79/41/80 deg at 1 count, 45/72/49/71/49/73 deg at
7 counts). The `ANGLE_*` table in `config.h` is right for one load only.

- [ ] Collect `hd` traces at steady speed under light, medium and heavy
  ```
  load (`/ht` endpoint). Widths per state = dwell / sum(dwell) * 360.
  ```
- [ ] Confirm the short sectors keep growing with current and that the
  ```
  asymmetry does not invert at kart currents.
  ```
- [ ] If widths at typical cruise differ from the table by more than ~5 deg,
  ```
  set the table to the cruise values (keep the mean of the six entries
  constant so the sweep-calibrated offset still applies).
  ```



## 3. FOC angle correction

Why: `pc` has been 127 in every run. `ui16_BatteryCurrent` is never
assigned in this fork, so `updateCorrection()` in `Code/motor.c` always
takes the regen branch. All tuning so far was with the correction
effectively off.

- [ ] Decide after items 1 and 2 whether it is wanted. The load-induced
  ```
  edge shift already retards commutation under current, which is the
  safe direction; the FOC loop would advance it.
  ```
- [ ] If enabling: assign `ui16_BatteryCurrent` from
  ```
  `ui16_adc_read_motor_total_current()` in the slow loop, and tighten the
  clamp in `updateCorrection()` from 111..143 to something like 119..135
  first. The dither cliff is only +12 counts above offset 202.
  ```
- [ ] Watch `pc` on the bench before the road: it should settle a few counts
  ```
  from 127 and not oscillate.
  ```



## 4. Top end

Why: lockout margin at >160 erps is verified at no load only (164 erps,
`hm/edge` 0.001, `hb` 0).

- [ ] Full throttle on the flat; note max erps and `hm`, `hb` in the top
  ```
  duty band. Clean = same as bench.
  ```
- [ ] Battery sag at full throttle; LCD3 voltage digits.



## 5. Throttle

Why: median-of-3 + 2-count deadband was tuned for bench noise. The range
(43..182) was measured on the old actuator.

- [ ] Draw wire encoder fitted: read `thr` at rest and fully pulled (over
  ```
  the programmer/monitor), set `ADC_THROTTLE_MIN_VALUE` /
  `ADC_THROTTLE_MAX_VALUE`, reflash. Wheels off the ground until done:
  with the old range an unknown resting reading can mean drive or full
  regen.
  ```

- [ ] No hunting at steady throttle on the road (vibration adds throttle
  ```
  noise the bench did not have). `tmin`/`tmax` per report show the raw
  spread; if it exceeds ~4 counts at steady hand, raise
  `GOKART_THROTTLE_DEADBAND` to 3.
  ```
- [ ] Response still feels immediate; if not, do not add averaging, look
  ```
  at the deadband first.
  ```
- [ ] Full travel reaches `thr` ~180 (`ADC_THROTTLE_MAX_VALUE` 182) and
  ```
  duty 255.
  ```



## 6. Thermal and current

- [ ] Controller and motor temperature by hand after a hard run.
- [ ] Peak `cur` seen; compare against `PHASE_CURRENT_MAX_VALUE` and
  ```
  `BATTERY_CURRENT_MAX_VALUE` in `config.h` (both inert in GOKART mode;
  decide if a limiter is wanted).
  ```



## 7. Reverse switch

Why: reverse commutation offset `GOKART_REVERSE_MOTOR_ANGLE` (128) is the
middle of a calculated range of 112..144, not measured.

- [ ] Wire PG0 to a switch to ground. With the motor stopped and the pedal
  ```
  in the coast band, flip it: `dir` goes 1 after ~0.2 s. Flipping it
  while rolling must do nothing until the kart stops.
  ```
- [ ] Bench sweep in reverse: `GOKART_ANGLE_SWEEP 1`, START/END ~100..160,
  ```
  switch pulled low. Pick the middle of the clean window (lowest `cur`,
  `hm` 0) and set `GOKART_REVERSE_MOTOR_ANGLE`. Sweep back to 0 after.
  ```
- [ ] Note: the sweep currently writes `ui8_s_motor_angle` directly, so
  ```
  flipping the switch during a sweep resets it. Sweep in one direction
  per boot.
  ```



## 8. Regen and pack voltage

Why: regen has only been tried on the bench supply, which cannot sink
current. The PB5 calibration for the 72 V pack is not settled.

- [ ] Read `bv` with the pack connected and compare against a meter; set
  ```
  `ADC_BATTERY_VOLTAGE_K` and move `BATTERY_VOLTAGE_MIN/MAX_VALUE` to the
  72 V range.
  ```
- [ ] Then set `GOKART_REGEN_VBAT_TAPER_ADC` / `_CUT_ADC` to just under a
  ```
  full pack (now 980/1010, near full scale, so a full pack gets no regen).
  ```
- [ ] On the battery: regen from moderate speed. `rt` follows the pedal,
  ```
  `cur` goes negative, no pulsing. If current alternates tick to tick,
  halve `GOKART_REGEN_GAIN_X256`; if it takes >0.3 s to settle, double it.
  ```
- [ ] Strength feels right at full regen (`GOKART_REGEN_MAX_MOTOR_COUNTS` 60,
  ```
  pack cap `GOKART_REGEN_MAX_BAT_COUNTS` 30).
  ```
- [ ] `bk` settles to a stable value during steady cruising.



## Record per run

date, pack V start/end, rider weight, surface/grade, firmware commit,
offset, max erps, `hc/(6*rev)`, `hb`, `hx`, `hm/edge`, peak `cur`, notes on
feel, history/ht json filenames.