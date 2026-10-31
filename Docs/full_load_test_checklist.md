# Full-load test checklist

Bench tuning (31 Oct 2026) got the motor smooth at no load on a 60 V / 5 A
supply, up to 164 erps. Everything below needs real current and can only be
settled on the battery. Firmware under test: `main` at `8efa1d4` or later
(rotor offset 202, hall lockout 9 %, throttle median+deadband, FOC
correction inert).

## Before the first run

- [ ] Controller flashed from current `main`; `GOKART_ANGLE_SWEEP`,
      `GOKART_THROTTLE_HOLD_TICKS`, `GOKART_HALL_REJECT_BACKSTEP` all 0 in
      `Code/config.h`.
- [ ] Telemetry available if at all possible: laptop + Analog Discovery on the
      display UART line, `python3 Code/test/pin_readback/throttle_monitor.py`,
      page at http://127.0.0.1:8765. Without it the checks below reduce to
      feel and the LCD3 watts/speed digits.
- [ ] After each run, save the data before it scrolls off (the monitor keeps
      the last 2000 reports, about 4 min):
      `curl -s 127.0.0.1:8765/history > runN_history.json`
      `curl -s 127.0.0.1:8765/ht > runN_ht.json`
- [ ] Battery full, LCD3 reads 84.0 V-ish; note the pack voltage at the start.

## Telemetry fields

One line every ~240 ms (TX is queued and interrupt driven; it never stalls the control loop):
`thr duty hall hc hi erps cur tmin tmax rst up rev hb hx hm p6 p4 pc`

| field | meaning | healthy |
|---|---|---|
| `hc` | hall changes accepted | `hc / (6 * rev)` = 1.00 |
| `hi` | invalid hall reads (0 or 7) | 0 |
| `rev` | electrical revolutions | |
| `hb` | backward hall steps | 0 once rolling; a few on spin-up is normal |
| `hx` | non-neighbour hall jumps | 0 |
| `hm` | raw flips swallowed by the lockout | `hm / (6 * rev)` < 0.1 at no load; see item 1 |
| `cur` | shunt amp, counts above boot zero | ~1 count per amp on this board |
| `pc` | FOC angle correction | 127 (inert; see item 3) |
| `hd ...` | 16 hall changes as `state:dwell`, every ~1 s | see item 2 |
| `rst` | reset flags | should not change mid-run (8 = normal) |

## 1. Hall sensors under load (highest priority)

Why: the hall edges dither when the stator field at the sensors is strong
enough. At no load that only happened at offset >= 214; at 202 the sensors
were clean. Load multiplies the stator field.

- [ ] Hard acceleration from rest, several times; hold full throttle up a
      grade if there is one.
- [ ] `hm / (6 * rev)` over the run. Below ~0.5: fine. Above: the field is
      reaching the sensors at load; try offset 198 (`MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT`),
      not higher.
- [ ] `hb` stays flat once rolling; `hx` 0; `hi` 0.
- [ ] Listen for the hunting/growl that the bench had at 214. If it appears
      only under load, same action: offset down a few units.
- [ ] Starting torque from rest acceptable with a rider on board. If weak,
      the offset may be too far retarded for high current; try 206 and
      re-check `hm`.

## 2. Hall sector widths vs current

Why: no-load traces showed the sector widths move with current
(4/6/2/3/1/5 = 40/78/43/79/41/80 deg at 1 count, 45/72/49/71/49/73 deg at
7 counts). The `ANGLE_*` table in `config.h` is right for one load only.

- [ ] Collect `hd` traces at steady speed under light, medium and heavy
      load (`/ht` endpoint). Widths per state = dwell / sum(dwell) * 360.
- [ ] Confirm the short sectors keep growing with current and that the
      asymmetry does not invert at kart currents.
- [ ] If widths at typical cruise differ from the table by more than ~5 deg,
      set the table to the cruise values (keep the mean of the six entries
      constant so the sweep-calibrated offset still applies).

## 3. FOC angle correction

Why: `pc` has been 127 in every run. `ui16_BatteryCurrent` is never
assigned in this fork, so `updateCorrection()` in `Code/motor.c` always
takes the regen branch. All tuning so far was with the correction
effectively off.

- [ ] Decide after items 1 and 2 whether it is wanted. The load-induced
      edge shift already retards commutation under current, which is the
      safe direction; the FOC loop would advance it.
- [ ] If enabling: assign `ui16_BatteryCurrent` from
      `ui16_adc_read_motor_total_current()` in the slow loop, and tighten the
      clamp in `updateCorrection()` from 111..143 to something like 119..135
      first. The dither cliff is only +12 counts above offset 202.
- [ ] Watch `pc` on the bench before the road: it should settle a few counts
      from 127 and not oscillate.

## 4. Top end

Why: lockout margin at >160 erps is verified at no load only (164 erps,
`hm/edge` 0.001, `hb` 0).

- [ ] Full throttle on the flat; note max erps and `hm`, `hb` in the top
      duty band. Clean = same as bench.
- [ ] Battery sag at full throttle; LCD3 voltage digits.

## 5. Throttle

Why: median-of-3 + 2-count deadband was tuned for bench noise.

- [ ] No hunting at steady throttle on the road (vibration adds throttle
      noise the bench did not have). `tmin`/`tmax` per report show the raw
      spread; if it exceeds ~4 counts at steady hand, raise
      `GOKART_THROTTLE_DEADBAND` to 3.
- [ ] Response still feels immediate; if not, do not add averaging, look
      at the deadband first.
- [ ] Full travel reaches `thr` ~180 (`ADC_THROTTLE_MAX_VALUE` 182) and
      duty 255.

## 6. Thermal and current

- [ ] Controller and motor temperature by hand after a hard run.
- [ ] Peak `cur` seen; compare against `PHASE_CURRENT_MAX_VALUE` and
      `BATTERY_CURRENT_MAX_VALUE` in `config.h` (both inert in GOKART mode;
      decide if a limiter is wanted).

## Record per run

date, pack V start/end, rider weight, surface/grade, firmware commit,
offset, max erps, `hc/(6*rev)`, `hb`, `hx`, `hm/edge`, peak `cur`, notes on
feel, history/ht json filenames.
