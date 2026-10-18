/*
 * Copyright (c) 2018 Björn Schmidt
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

/* 
 * File:   ACAsetPoint.c
 * Author: Björn Schmidt
 *
 * Created on September 7, 2018, 6:47 PM
 */

#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "stm8s.h"
#include "stm8s_it.h"
#include "ACAsetPoint.h"
#include "ACAcontrollerState.h"
#include "ACAcommons.h"
// FIXME ugly cross references
// why? cause this blackbox is just for calculating,
// it's not supposed to read values on its own
// all values should be read by a slowloop_"readall/updatesensors, whatever" before calling it
#include "brake.h"
#include "adc.h" // FIXME ugly cross reference
#include "motor.h"
#include "stm8s_tim1.h"

static uint32_t ui32_dutycycle; // local version of setpoint
#ifdef GOKART_MODE
uint8_t ui8_gk_thr_min = 0, ui8_gk_thr_max = 0; // raw throttle ADC spread since last telemetry report
static uint16_t ui16_gk_thr_filt = 0;
#endif

static int8_t uint_PWM_Enable = 0; //flag for PWM state
static uint16_t ui16_BatteryCurrent_accumulated = 2496L; //8x current offset, for filtering or Battery Current
static uint16_t ui16_BatteryVoltage_accumulated;
static uint16_t ui16_assist_percent_smoothed;
static uint32_t ui32_time_ticks_between_pas_interrupt_accumulated = 0; // for filtering of PAS value 
static uint32_t ui32_erps_accumulated; //for filtering of erps
//static uint32_t ui32_speedlimit_actual_accumulated;
static uint32_t ui32_sumthrottle_accumulated; //it is already smoothed b4 we get it, we want to smooth it even more though for dynamic assist levels

static float float_temp = 0; //for float calculations

static uint32_t uint32_temp = 0;
static uint16_t uint16_temp = 0;
static uint16_t controll_state_temp = 0;
static uint8_t ui8_temp = 0;

uint16_t cutoffSetpoint(uint32_t ui32_dutycycle) {
	if (ui32_dutycycle < 5) {
		ui32_dutycycle = 0;
	}
	if (ui32_dutycycle > 255) {
		ui32_dutycycle = 255;
	}
	return ui32_dutycycle;
}

BitStatus checkMaxErpsOverride(){
	if (ui32_erps_filtered > ui16_erps_max) {
		ui32_dutycycle = PI_control(ui32_erps_filtered, ui16_erps_max,uint_PWM_Enable); //limit the erps to maximum value to have minimum 30 points of sine table for proper commutation
		controll_state_temp +=1024;
		return 1;
	}
	return 0;
}

BitStatus checkUnderVoltageOverride(){
	//check for undervoltage --> ramp down power starting 6.25% above min
	ui8_temp = ui8_s_battery_voltage_min + (ui8_s_battery_voltage_min>>4);
	if (ui8_BatteryVoltage < ui8_temp) {

		uint32_current_target = map(ui8_BatteryVoltage, ui8_s_battery_voltage_min, ui8_temp, ui16_current_cal_b, uint32_current_target );
		ui32_dutycycle = PI_control(ui16_BatteryCurrent, uint32_current_target,uint_PWM_Enable);
		controll_state_temp +=2048;
		return 1;
    }
	return 0;
}

BitStatus checkOverVoltageOverride(){
	//check for overvoltage --> ramp down regen starting 3.125% below max
	ui8_temp = ui8_s_battery_voltage_max - (ui8_s_battery_voltage_max>>5);
	if (ui8_BatteryVoltage > ui8_temp) {

		uint32_current_target = map(ui8_BatteryVoltage, ui8_temp, ui8_s_battery_voltage_max, uint32_current_target, ui16_current_cal_b );
		ui32_dutycycle = PI_control(ui16_BatteryCurrent, uint32_current_target,uint_PWM_Enable);
		controll_state_temp +=4096;
		return 1;
    }
	return 0;
}

void aca_setpoint_init(void) {
	ui32_time_ticks_between_pas_interrupt_accumulated = ((uint32_t)ui16_s_ramp_start)<<3;
}

uint16_t aca_setpoint(uint16_t ui16_time_ticks_between_pas_interrupt, uint16_t setpoint_old) {
#ifdef GOKART_MODE
    /* Throttle sets duty directly. The bridge stays off until the throttle
       leaves its rest deadzone, the brake is released, and the halls are valid. */
    {
        uint8_t ui8_throttle_raw = ui8_adc_read_throttle();
        uint8_t ui8_throttle_adc;
        int16_t i16_throttle_start = (int16_t) ADC_THROTTLE_MIN_VALUE + (int16_t) GOKART_THROTTLE_DEADZONE;
        uint16_t ui16_duty = 0;

        // raw sample spread between telemetry reports (bench diagnostic)
        if (ui8_gk_thr_min == 0 && ui8_gk_thr_max == 0) {
            ui8_gk_thr_min = ui8_gk_thr_max = ui8_throttle_raw;
        } else {
            if (ui8_throttle_raw < ui8_gk_thr_min) ui8_gk_thr_min = ui8_throttle_raw;
            if (ui8_throttle_raw > ui8_gk_thr_max) ui8_gk_thr_max = ui8_throttle_raw;
        }
#if GOKART_THROTTLE_FILTER_SHIFT > 0
        // exponential average of the 50 Hz samples: time constant ~ 2^shift samples
        if (ui16_gk_thr_filt == 0) ui16_gk_thr_filt = (uint16_t) ui8_throttle_raw << GOKART_THROTTLE_FILTER_SHIFT;
        ui16_gk_thr_filt += ui8_throttle_raw - (ui16_gk_thr_filt >> GOKART_THROTTLE_FILTER_SHIFT);
        ui8_throttle_adc = (uint8_t) (ui16_gk_thr_filt >> GOKART_THROTTLE_FILTER_SHIFT);
#else
        ui8_throttle_adc = ui8_throttle_raw;
#endif

        if (!brake_is_set() && hall_sensors > 0 && hall_sensors < 7 && ui8_throttle_adc > i16_throttle_start) {
            ui16_duty = (uint16_t) map(ui8_throttle_adc, i16_throttle_start, ADC_THROTTLE_MAX_VALUE, 0, GOKART_MAX_DUTY);
        }

        ui16_sum_throttle = ui16_duty;
        if (ui16_duty > 0) {
            TIM1_CtrlPWMOutputs(ENABLE);
            uint_PWM_Enable = 1;
        } else {
            TIM1_CtrlPWMOutputs(DISABLE);
            uint_PWM_Enable = 0;
        }
        return ui16_duty;
    }
#else
    // Original e-bike mode code
    // select virtual erps speed based on speedsensor type
    if (((ui16_aca_flags & EXTERNAL_SPEED_SENSOR) == EXTERNAL_SPEED_SENSOR)) {
        ui16_virtual_erps_speed = (uint16_t) ((((uint32_t)ui8_gear_ratio) * ui32_speed_sensor_rpks) /1000); 
    }else{
        ui16_virtual_erps_speed = (uint16_t) ui32_erps_filtered;
    }

    // first select current speed limit
    if (ui8_offroad_state == 255) {
        ui8_speedlimit_actual_kph = 80;
    } else if (ui8_offroad_state > 15 && ui16_sum_throttle <= 2) { // allow a slight increase based on ui8_offroad_state
        ui8_speedlimit_actual_kph = ui8_speedlimit_kph + (ui8_offroad_state - 16);
    } else if (ui8_offroad_state > 15 && ui16_sum_throttle > 2) {
        ui8_speedlimit_actual_kph = ui8_speedlimit_with_throttle_override_kph + (ui8_offroad_state - 16);
    } else if (ui16_time_ticks_for_pas_calculation > timeout || !PAS_is_active) {
        ui8_speedlimit_actual_kph = ui8_speedlimit_without_pas_kph;
    } else {
        ui8_speedlimit_actual_kph = ui8_speedlimit_kph;
    }

    // >=8 means levels are switched of, use wanted percentage directly instead
    ui16_assist_percent_smoothed -= ui16_assist_percent_smoothed >> 4;
    if ((ui8_assistlevel_global & 15) < 8) {
        ui16_assist_percent_smoothed += ui8_a_s_assistlevels[ui8_assistlevel_global & 15];
    } else {
        ui16_assist_percent_smoothed += ui8_assist_percent_wanted;
    }
    ui8_assist_percent_actual = ui16_assist_percent_smoothed >> 4;


    // average throttle over a longer time period (for dynamic assist level) 
    ui32_sumthrottle_accumulated -= ui32_sumthrottle_accumulated >> 10;
    ui32_sumthrottle_accumulated += ui16_sum_throttle;
    ui8_assist_dynamic_percent_addon = ui32_sumthrottle_accumulated >> 10;
    if ((ui8_assist_dynamic_percent_addon + ui8_assist_percent_actual) > 100) {
        ui8_assist_dynamic_percent_addon = 100 - ui8_assist_percent_actual;
    }

    ui16_BatteryCurrent_accumulated -= ui16_BatteryCurrent_accumulated >> 3;
    ui16_BatteryCurrent_accumulated += ui16_BatteryCurrent;
    ui16_BatteryCurrent_filtered = ui16_BatteryCurrent_accumulated >> 3;

    ui16_time_ticks_between_pas_interrupt_smoothed -= ui16_time_ticks_between_pas_interrupt_smoothed >> 4;
    ui16_time_ticks_between_pas_interrupt_smoothed += ui16_time_ticks_between_pas_interrupt;

    uint8_t controll_state_temp = 0;
    uint32_t uint32_temp;
    float float_temp;

    // check for undervoltage override
    if (checkUnderVoltageOverride()) {
        uint32_current_target = ui16_current_cal_b;
        controll_state_temp = 1;
    }
    // check for max erps override
    else if (checkMaxErpsOverride()) {
        uint32_current_target = ui16_current_cal_b;
        controll_state_temp = 2;
    }
    // check for brake override
    else if (brake_is_set()) {
        uint32_current_target = ui16_current_cal_b;
        controll_state_temp = 4;
    }
    // check for walk assist override
    else if (ui8_walk_assist) {
        uint32_current_target = 10 + ui16_current_cal_b;
        controll_state_temp = 8;
    }
    // check for regen override
    else if (ui8_regen_throttle > 0) {
        uint32_current_target = ui16_current_cal_b - (ui8_regen_throttle * ui16_regen_current_max_value / 255);
        if (uint32_current_target < ui16_current_cal_b - ui16_regen_current_max_value) {
            uint32_current_target = ui16_current_cal_b - ui16_regen_current_max_value;
        }
        controll_state_temp = 16;
    }
    // check for battery current limit override
    else if (ui16_BatteryCurrent_filtered > ui16_battery_current_max_value) {
        uint32_current_target = ui16_current_cal_b;
        controll_state_temp = 32;
    }
    // check for phase current limit override
    else if (setpoint_old > 0 && (ui16_ADC_iq_current >> 2) > PHASE_CURRENT_MAX_VALUE) {
        uint32_current_target = ui16_current_cal_b;
        controll_state_temp = 64;
    }
    
    } else {

        uint32_current_target = ui16_current_cal_b; // reset target to zero
        controll_state_temp = 0;
        //if none of the overruling boundaries are concerned, calculate new setpoint

        // if torque sim is requested. We could check if we could solve this function with just one line with map function...
        if (((ui16_aca_flags & TQ_SENSOR_MODE) != TQ_SENSOR_MODE)) {

            // add dynamic assist level based on past throttle input
            ui8_temp = ui8_assist_percent_actual;

            if ((ui16_aca_flags & DYNAMIC_ASSIST_LEVEL) == DYNAMIC_ASSIST_LEVEL) {
                ui8_temp += ui8_assist_dynamic_percent_addon;
            }

            if (ui16_time_ticks_between_pas_interrupt_smoothed > ui16_s_ramp_end) {
                //if you are pedaling slower than defined ramp end
                //or not pedalling at all
                //current is proportional to cadence
                uint32_current_target = (ui8_temp * (ui16_battery_current_max_value) / 100);
                float_temp = 1.0 - (((float) (ui16_time_ticks_between_pas_interrupt_smoothed - ui16_s_ramp_end)) / ((float) (ui16_s_ramp_start - ui16_s_ramp_end)));
                uint32_current_target = ((uint16_t) (uint32_current_target)*(uint16_t) (float_temp * 100.0)) / 100 + ui16_current_cal_b;
                controll_state_temp += 1;

                //in you are pedaling faster than in ramp end defined, desired battery current level is set,
            } else {
                uint32_current_target = (ui8_temp * (ui16_battery_current_max_value) / 100 + ui16_current_cal_b);
                controll_state_temp += 2;
            }
        } else { // torque sensor mode


                         //erst mal alles aufmultiplizieren, damit beim Teilen was über 1 übrig bleibt. Bitte mal überschlagen, ob die int32 da nicht überlaufen kann...
                         uint32_temp = ui16_sum_torque;
                         uint32_temp *= ui8_assist_percent_actual;
                         uint32_temp *= ui16_battery_current_max_value;
                         uint32_temp *= uint32_torquesensorCalibration;

                         uint32_temp /= ui16_time_ticks_between_pas_interrupt_smoothed; // hier lässt sich die geteilt-Operation nicht vermeiden :-(

                         if(PAS_is_active)
                         uint32_current_target = (uint32_temp >>8) +  (uint32_t) ui16_current_cal_b; //right shift 15 fasst die Operationen /100 (annähernd >>7) aus der assist_percent und /255 ( >>8) aus dem battery_current max zusammen, ist nicht ganz korrekt, ggf. nur >>14 nehmen -->/(256*128) vs. /(256*64)
                         else uint32_current_target =(uint32_t) ui16_current_cal_b;


            controll_state_temp += 4;

        }


        float_temp = 0.0;
        // throttle / torquesensor override following
        if (((ui16_aca_flags & TQ_SENSOR_MODE) != TQ_SENSOR_MODE)) {
            if (ui8_speedlimit_kph > 1){
                // do not apply throttle at very low speed limits (technical restriction, speelimit can and should never be lover than 1)
                float_temp = (float) ui16_sum_throttle;
            }
        } else {
            float_temp = (float) ui16_momentary_throttle; // or ui16_sum_throttle
            //float_temp *= (1 - (float) ui16_virtual_erps_speed / 2 / (float) (ui16_speed_kph_to_erps_ratio * ((float) ui8_speedlimit_kph))); //ramp down linear with speed. Risk: Value is getting negative if speed>2*speedlimit

        }

        // map curret target to assist level, not to maximum value
        if ((ui16_aca_flags & ASSIST_LVL_AFFECTS_THROTTLE) == ASSIST_LVL_AFFECTS_THROTTLE) {
            float_temp *= ((float) ui8_assist_percent_actual / 100.0);
            controll_state_temp += 8;
        }

        float_temp = float_temp * (float) (ui16_battery_current_max_value) / 255.0 + (float) ui16_current_cal_b; //calculate current target


        if ((uint32_t) float_temp > uint32_current_target) {

                //override current target with throttle
                uint32_current_target = (uint32_t) float_temp; 

            controll_state_temp += 16;
        }

        // check for overspeed
        uint32_temp = uint32_current_target;
        uint32_current_target = CheckSpeed((uint16_t) uint32_current_target, (uint16_t) ui16_virtual_erps_speed, (ui16_speed_kph_to_erps_ratio * ((uint16_t) ui8_speedlimit_actual_kph)) / 100, (ui16_speed_kph_to_erps_ratio * ((uint16_t) (ui8_speedlimit_actual_kph + 2))) / 100); //limit speed
        if (uint32_temp != uint32_current_target) {
            controll_state_temp += 32;
        }

        if (uint32_current_target > ui16_battery_current_max_value + ui16_current_cal_b) {
            uint32_current_target = ui16_battery_current_max_value + ui16_current_cal_b;
            controll_state_temp += 64;
        }
        //phase current limiting
        if (setpoint_old > 0 && (uint32_current_target - ui16_current_cal_b)*255 / setpoint_old > PHASE_CURRENT_MAX_VALUE) { // limit phase current according to Phase Current = battery current/duty cycle
            uint32_current_target = (PHASE_CURRENT_MAX_VALUE) * setpoint_old / 255 + ui16_current_cal_b;
            controll_state_temp += 128;
        }
        
        // control power instead of current
        if ((ui16_aca_flags & POWER_BASED_CONTROL) == POWER_BASED_CONTROL) {
            // nominal voltage based on limits
            ui8_temp = ((ui8_s_battery_voltage_max - ui8_s_battery_voltage_min)>>1)+ui8_s_battery_voltage_min;
            uint32_current_target*=ui8_temp/ui8_BatteryVoltage;
        }
        
        if ((ui16_aca_experimental_flags & DC_STATIC_ZERO) == DC_STATIC_ZERO) {
            ui32_dutycycle = 0;
            controll_state_temp += 256;
        }else if (!checkUnderVoltageOverride() && !checkMaxErpsOverride()){

            if (ui8_walk_assist) uint32_current_target = 10 + ui16_current_cal_b;
            //send current target to PI-controller
            ui32_dutycycle = PI_control(ui16_BatteryCurrent, uint32_current_target,uint_PWM_Enable);
        }
        
        if ((ui16_aca_experimental_flags & PWM_AUTO_OFF) == PWM_AUTO_OFF) {
            controll_state_temp += 512;
            //disable PWM if enabled and no power is wanted
            if (uint_PWM_Enable && ui32_erps_filtered == 0 && uint32_current_target == ui16_current_cal_b) {
                TIM1_CtrlPWMOutputs(DISABLE);
                uint_PWM_Enable = 0;
            }
            //enable PWM if disabled and voltage is 6.25% higher than min, some hysteresis and power is wanted
            if (!uint_PWM_Enable && ui8_BatteryVoltage > (ui8_s_battery_voltage_min +  (ui8_s_battery_voltage_min >>4)) && (uint32_current_target != ui16_current_cal_b)){
                TIM1_CtrlPWMOutputs(ENABLE);
                uint_PWM_Enable = 1;
            }
        }else{

            //enable PWM if disabled and voltage is 6.25% higher than min, some hysteresis
            if (!uint_PWM_Enable && ui8_BatteryVoltage > (ui8_s_battery_voltage_min + (ui8_s_battery_voltage_min >>4))) { 
                TIM1_CtrlPWMOutputs(ENABLE);
                uint_PWM_Enable = 1;
            }
        }
    }
    ui16_control_state = controll_state_temp;
    return cutoffSetpoint(ui32_dutycycle);
#endif
}
