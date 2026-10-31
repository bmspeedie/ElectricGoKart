# Electric Go-Kart Modifications

This document describes the modifications made to adapt the BMSBattery S-series motor controller firmware for electric go-kart applications with direct throttle control.

## Overview

The original firmware was designed for e-bikes and e-scooters with Pedal Assist System (PAS) and complex throttle ramping. For go-kart applications, we've implemented a simplified direct throttle control system that provides immediate response without PAS dependency.

## Key Changes Made

### 1. Direct Throttle Control (`ACAcommons.c`)
- **File**: `ACAcommons.c`
- **Function**: `updateRequestedTorque()`
- **Changes**: 
  - Added `#ifdef GOKART_MODE` conditional compilation
  - Direct mapping of throttle ADC value to motor power (0-255)
  - Removed throttle accumulation and ramping
  - Set PAS as always active to bypass PAS dependency

### 2. Simplified Control Logic (`ACAsetPoint.c`)
- **File**: `ACAsetPoint.c`
- **Function**: `aca_setpoint()`
- **Changes**:
  - Added go-kart mode with direct throttle-to-power mapping
  - Removed speed limiting and complex assist level calculations
  - Maintained basic safety limits (battery current, phase current)
  - Simplified PI controller input

### 3. Configuration Updates (`config.h`)
- **File**: `config.h`
- **Changes**:
  - Added `GOKART_MODE` flag
  - Added `GOKART_MAX_SPEED_KPH` setting
  - Added `GOKART_THROTTLE_DIRECT` flag

### 4. Go-Kart Specific Configuration (`gokart_config.h`)
- **File**: `gokart_config.h` (new file)
- **Purpose**: Centralized configuration for go-kart applications
- **Features**:
  - Throttle settings and deadzone
  - Power and speed limits
  - Safety parameters
  - Feature enable/disable flags

## How to Use

### 1. Enable Go-Kart Mode
Ensure `GOKART_MODE` is defined in your configuration:

```c
#define GOKART_MODE 1
```

### 2. Throttle Wiring
Connect your foot throttle to the throttle ADC input:
- **ADC Channel**: ADC1_CHANNEL_4 (as defined in `adc.h`)
- **Voltage Range**: 0-3.3V (typical for STM8S)
- **ADC Values**: 43-182 (as defined in `config.h`)

### 3. Throttle Calibration
The actuator's full ADC range is 43 (rest) to 182 (fully pressed). The pedal
is one-pedal: `GOKART_THROTTLE_RAIL_COUNTS` (6) are clipped off each end so
the demand reaches its rail just before the mechanical stop and stays there,
and `GOKART_THROTTLE_ZERO_PCT` (30 %) of the remaining travel is the
zero-torque point:

| ADC | position | demand |
|---|---|---|
| 43..49 | below the lower rail | full regen |
| 49..85 | 0..28 % | regen, linear, full at the bottom |
| 85..89 | neutral band (±`GOKART_THROTTLE_NEUTRAL_COUNTS`) | zero torque, bridge off (coast) |
| 89..176 | 32..100 % | drive, duty `GOKART_MIN_DUTY`..`GOKART_MAX_DUTY` |
| 176..182 | above the upper rail | full drive |

With the pedal released the kart brakes (regen). Hold it at ~30 % to coast.

### 4. Safety Features
The go-kart mode maintains these safety features:
- **Undervoltage Protection**: Stops motor if battery voltage drops below minimum
- **Overcurrent Protection**: Limits battery and phase current
- **Brake Override**: Stops motor when brake is applied
- **Overvoltage Protection**: Protects against overcharging

## Configuration Options

### Throttle Settings
```c
#define ADC_THROTTLE_MIN_VALUE 43          // actuator at rest
#define ADC_THROTTLE_MAX_VALUE 182         // actuator fully pressed
#define GOKART_THROTTLE_RAIL_COUNTS 6      // dead spot at each end of travel
#define GOKART_THROTTLE_ZERO_PCT 30        // zero-torque point, % of travel
#define GOKART_THROTTLE_NEUTRAL_COUNTS 2   // coast band each side of the zero point
```

### Regenerative Braking
Regen closes a loop on the PB6 shunt (battery current). The pedal sets a
motor current (torque); each tick that is converted to the battery current
expected at the present duty and the duty is stepped to hold it. The loop is
entered just under the back-EMF duty, estimated from speed, pack voltage and
`GOKART_BEMF_DUTY_X256_PER_ERPS` (refined at run time; `bk` in the telemetry),
and is never allowed below half of that estimate.
```c
#define GOKART_REGEN 1
#define GOKART_REGEN_MAX_MOTOR_COUNTS 60   // motor current at full regen (~40 A)
#define GOKART_REGEN_MAX_BAT_COUNTS 30     // cap on current into the pack (~20 A)
#define GOKART_REGEN_FULL_ERPS 40          // full torque above this speed, fading below
#define GOKART_REGEN_MIN_ERPS 10           // no regen below this speed
#define GOKART_REGEN_GAIN_X256 32          // loop gain, see config.h for tuning
#define GOKART_REGEN_VBAT_TAPER_ADC 980    // regen tapers off above this PB5 reading
#define GOKART_REGEN_VBAT_CUT_ADC 1010     // and is off above this one
```
The brake switch, invalid hall states and speeds below `GOKART_REGEN_MIN_ERPS`
all turn the bridge off (coast) in regen as well as in drive.

### Power Limits
```c
#define GOKART_MAX_CURRENT_A 64       // Maximum battery current
#define GOKART_MAX_PHASE_CURRENT_A 350 // Maximum phase current
```

### Speed Limits
```c
#define GOKART_MAX_SPEED_KPH 80       // Maximum speed (disabled in current implementation)
```

## Compilation

### For Go-Kart Mode
1. Ensure `GOKART_MODE` is defined
2. Include `gokart_config.h` if using the new configuration file
3. Compile with your preferred toolchain

### For Original E-Bike Mode
1. Comment out or remove `GOKART_MODE` definition
2. Use original configuration settings

## Testing

### Throttle Response Test
1. Power on the controller with the pedal released (reads full regen, but
   nothing happens below `GOKART_REGEN_MIN_ERPS`)
2. Press to ~30 % - `dm` in the telemetry reads 0 (coast)
3. Press further - motor should start at once and speed up linearly
4. Ease back to ~30 % - motor coasts down (bridge off)
5. Ease below 30 % while the motor is turning - `rt` goes positive, `cur`
   goes negative (current into the pack) and the motor slows faster. The
   LCD3 shows the circle-with-arrows symbol whenever the pedal is below the zero point.

Regen on the bench: a lab supply cannot sink current, so regen pushes the
bus voltage up. With a bare motor the energy is small (rotor inertia only),
but go into regen briefly and only from moderate speed, watch the supply
voltage, and keep `bv` in the telemetry well below `GOKART_REGEN_VBAT_CUT_ADC`.
Proper regen tests need the battery.

### Safety Test
1. Test brake override - applying brake should stop motor
2. Test undervoltage protection - simulate low battery voltage
3. Test overcurrent protection - monitor current limits

## Troubleshooting

### Motor Doesn't Start
- Check throttle ADC values
- Verify `GOKART_MODE` is enabled
- Check brake input (brake must be released)

### Jerky Throttle Response
- Adjust `GOKART_THROTTLE_NEUTRAL_COUNTS` (coast band) or the filters
  (`GOKART_THROTTLE_MEDIAN3`, `GOKART_THROTTLE_DEADBAND`)
- Check throttle potentiometer quality
- Verify ADC readings are stable

### Regen Too Strong / Too Weak / Pulsing
- Strength: `GOKART_REGEN_MAX_MOTOR_COUNTS` (pedal feel) and
  `GOKART_REGEN_MAX_BAT_COUNTS` (pack protection)
- Too strong at low speed: raise `GOKART_REGEN_FULL_ERPS`
- Current alternating tick to tick in regen: halve `GOKART_REGEN_GAIN_X256`;
  slow to settle: double it
- A kick at the moment regen engages: `bk` (back-EMF constant) is off; it
  self-corrects after a steady run, or set `GOKART_BEMF_DUTY_X256_PER_ERPS`
  to the settled `bk`

### Overcurrent Trips
- Reduce `GOKART_MAX_CURRENT_A`
- Check motor and battery specifications
- Verify phase current sensor calibration

## Safety Warnings

⚠️ **IMPORTANT SAFETY CONSIDERATIONS**:

1. **No Speed Limiting**: The go-kart mode removes speed limiting. Ensure your vehicle can safely handle the maximum speed.

2. **Direct Throttle Response**: There is no ramping or current limiting on the drive side. The motor will respond immediately to throttle input.

6. **One-Pedal Regen**: A released pedal is a braking command. Anyone pushing the kart, or riding it, must know that coasting means holding the pedal at the zero point. Regen into a pack whose BMS has opened (full or faulted) has nowhere to go and raises the bus voltage; the `GOKART_REGEN_VBAT_*` taper is the only firmware mitigation and the PB5 voltage calibration must be checked before relying on it.

3. **Testing**: Always test in a safe, controlled environment before full operation.

4. **Battery Safety**: Monitor battery voltage and temperature during operation.

5. **Mechanical Safety**: Ensure brakes and mechanical systems are properly maintained.

## Technical Details

### Control Loop
- **Frequency**: 50 Hz (20ms period)
- **Throttle Update**: Every control loop cycle
- **PI Controller**: Maintains current control for smooth operation

### ADC Sampling
- **Throttle Channel**: ADC1_CHANNEL_4
- **Resolution**: 10-bit (0-1023)
- **Effective Range**: 43-182 (mapped to 0-255 for motor control)

### PWM Control
- **Frequency**: 15.625 kHz
- **Duty Cycle**: 0-255 (0-100%)
- **Resolution**: 8-bit

## Future Enhancements

Potential improvements for go-kart applications:
1. **Speed Limiting**: Optional speed limiting for safety
2. **Throttle Curves**: Configurable throttle response curves
3. **Temperature Monitoring**: Motor and controller temperature protection
4. **Data Logging**: Enhanced logging for performance analysis
5. **Multiple Throttle Modes**: Different response characteristics

## Support

For questions or issues with the go-kart modifications:
1. Check the original firmware documentation
2. Review the STM8S microcontroller datasheet
3. Verify your hardware connections and specifications
4. Test thoroughly in a safe environment

---

**Disclaimer**: These modifications are provided as-is. Users are responsible for ensuring their implementation meets safety requirements and local regulations for electric vehicles.
