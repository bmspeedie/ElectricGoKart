/*
 * Go-Kart Configuration File
 * 
 * This file contains optimized settings for electric go-kart applications
 * with direct throttle control and no PAS dependency.
 */

#ifndef GOKART_CONFIG_H_
#define GOKART_CONFIG_H_

// Enable go-kart mode
#define GOKART_MODE 1

// Throttle settings for direct control
#define GOKART_THROTTLE_DIRECT 1
#define GOKART_THROTTLE_MIN_ADC 43    // Minimum throttle ADC value
#define GOKART_THROTTLE_MAX_ADC 182   // Maximum throttle ADC value
#define GOKART_THROTTLE_DEADZONE 5    // Deadzone to prevent accidental movement

// Speed and power limits
#define GOKART_MAX_SPEED_KPH 80       // Maximum speed in km/h
#define GOKART_MAX_CURRENT_A 64       // Maximum battery current in amps
#define GOKART_MAX_PHASE_CURRENT_A 350 // Maximum phase current in amps

// Safety settings
#define GOKART_MIN_BATTERY_V 148      // Minimum battery voltage (14.8V per cell for 10S)
#define GOKART_MAX_BATTERY_V 208      // Maximum battery voltage (20.8V per cell for 10S)
#define GOKART_UNDERVOLTAGE_HYSTERESIS 2 // Voltage hysteresis for undervoltage protection

// Motor control settings
#define GOKART_PWM_FREQUENCY 15625    // PWM frequency in Hz
#define GOKART_PI_P_GAIN 0.5          // PI controller proportional gain
#define GOKART_PI_I_GAIN 0.2          // PI controller integral gain

// Timing settings
#define GOKART_CONTROL_LOOP_HZ 50     // Control loop frequency
#define GOKART_ADC_UPDATE_HZ 1000     // ADC update frequency

// Features to disable for go-kart
#define GOKART_DISABLE_PAS 1          // Disable PAS system
#define GOKART_DISABLE_SPEED_LIMIT 1  // Disable speed limiting
#define GOKART_DISABLE_CRUISE_CONTROL 1 // Disable cruise control
#define GOKART_DISABLE_REGEN 1        // Disable regenerative braking

// Features to enable for go-kart
#define GOKART_ENABLE_DIRECT_THROTTLE 1 // Enable direct throttle control
#define GOKART_ENABLE_BRAKE_OVERRIDE 1  // Enable brake override
#define GOKART_ENABLE_OVERVOLTAGE_PROTECTION 1 // Enable overvoltage protection
#define GOKART_ENABLE_OVERCURRENT_PROTECTION 1 // Enable overcurrent protection

// Debug and monitoring
#define GOKART_ENABLE_DEBUG_OUTPUT 1  // Enable debug output via UART
#define GOKART_LOG_THROTTLE_VALUES 1  // Log throttle values for debugging
#define GOKART_LOG_CURRENT_VALUES 1   // Log current values for debugging
#define GOKART_LOG_VOLTAGE_VALUES 1   // Log voltage values for debugging

#endif /* GOKART_CONFIG_H_ */
