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
 * File:   ACAsetPoint.h
 * Author: Björn Schmidt
 *
 * Created on September 7, 2018, 6:47 PM
 */


#ifndef ACASETPOINT_H
#define ACASETPOINT_H

#include "config.h"

uint16_t aca_setpoint (uint16_t ui16_time_ticks_between_pas_interrupt, uint16_t setpoint_old );
void aca_setpoint_init(void);
extern uint8_t ui8_gk_thr_min, ui8_gk_thr_max;
extern uint16_t ui16_gk_hold_ticks;
// PB6 (battery current) samples accumulated by the main loop between slow-loop ticks
extern uint32_t ui32_gk_ibat_sum;
extern uint16_t ui16_gk_ibat_n;
extern int16_t i16_gk_demand; // pedal demand, -255 (full regen) .. 0 (coast) .. 255 (full drive)
extern uint8_t ui8_gk_regen_target; // regen motor current (torque) target in use after derating, PB6 counts; 0 outside regen
extern uint16_t ui16_gk_bemf_k_x256; // back-EMF constant in use, duty per erps x256 at GOKART_BEMF_REF_VBAT_ADC
extern int16_t i16_gk_ibat16; // last tick's mean battery current, 1/16 PB6 counts, + out of the pack

#endif /* ACASETPOINT_H */

