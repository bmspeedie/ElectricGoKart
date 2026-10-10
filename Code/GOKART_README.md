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
The system uses these ADC values:
- **Minimum**: 43 (throttle at rest)
- **Maximum**: 182 (full throttle)
- **Mapping**: Direct linear mapping to motor power (0-255)

### 4. Safety Features
The go-kart mode maintains these safety features:
- **Undervoltage Protection**: Stops motor if battery voltage drops below minimum
- **Overcurrent Protection**: Limits battery and phase current
- **Brake Override**: Stops motor when brake is applied
- **Overvoltage Protection**: Protects against overcharging

## Configuration Options

### Throttle Settings
```c
#define GOKART_THROTTLE_MIN_ADC 43    // Minimum throttle position
#define GOKART_THROTTLE_MAX_ADC 182   // Maximum throttle position
#define GOKART_THROTTLE_DEADZONE 5    // Deadzone to prevent creep
```

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
1. Power on the controller
2. Apply minimal throttle - motor should start immediately
3. Gradually increase throttle - power should increase linearly
4. Release throttle - motor should stop immediately

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
- Adjust `GOKART_THROTTLE_DEADZONE`
- Check throttle potentiometer quality
- Verify ADC readings are stable

### Overcurrent Trips
- Reduce `GOKART_MAX_CURRENT_A`
- Check motor and battery specifications
- Verify phase current sensor calibration

## Safety Warnings

⚠️ **IMPORTANT SAFETY CONSIDERATIONS**:

1. **No Speed Limiting**: The go-kart mode removes speed limiting. Ensure your vehicle can safely handle the maximum speed.

2. **Direct Throttle Response**: There is no ramping or current limiting. The motor will respond immediately to throttle input.

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
