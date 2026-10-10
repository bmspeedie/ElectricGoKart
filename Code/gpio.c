/*
 * BMSBattery S series motor controllers firmware
 *
 * Copyright (C) Casainho, 2017.
 *
 * Released under the GPL License, Version 3
 */

#include "stm8s.h"
#include "stm8s_gpio.h"
#include "gpio.h"

void gpio_init (void)
{
  ;
}

void debug_pin_init (void)
{
  GPIO_Init(DEBUG__PORT,
	    DEBUG__PIN,
	    GPIO_MODE_OUT_PP_HIGH_FAST);
}

void debug_pin_set (void)
{
  GPIO_WriteHigh(DEBUG__PORT, DEBUG__PIN);
}

void debug_pin_reset (void)
{
  GPIO_WriteLow(DEBUG__PORT, DEBUG__PIN);
}

// added by DerBastler - Light
void light_pin_init (void)
{
  GPIO_Init(LIGHT__PORT,
	    LIGHT__PIN,
	    GPIO_MODE_OUT_PP_HIGH_FAST);
}
// added by DerBastler - Light
void light_pin_set (void)
{
  GPIO_WriteHigh(LIGHT__PORT, LIGHT__PIN);
}
// added by DerBastler - Light
void light_pin_reset (void)
{
  GPIO_WriteLow(LIGHT__PORT, LIGHT__PIN);
}

// New control input functions for updated hardware
void cruise_pin_init (void)
{
  GPIO_Init(CRUISE__PORT,
	    CRUISE__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus cruise_is_set (void)
{
  if (GPIO_ReadInputPin(CRUISE__PORT, CRUISE__PIN) == 0)
    return 1;
  else
    return 0;
}

void reverse_pin_init (void)
{
  GPIO_Init(REVERSE__PORT,
	    REVERSE__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus reverse_is_set (void)
{
  if (GPIO_ReadInputPin(REVERSE__PORT, REVERSE__PIN) == 0)
    return 1;
  else
    return 0;
}

void self_study_pin_init (void)
{
  GPIO_Init(SELF_STUDY__PORT,
	    SELF_STUDY__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus self_study_is_set (void)
{
  if (GPIO_ReadInputPin(SELF_STUDY__PORT, SELF_STUDY__PIN) == 0)
    return 1;
  else
    return 0;
}

void low_high_pin_init (void)
{
  GPIO_Init(LOW_HIGH__PORT,
	    LOW_HIGH__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus low_high_is_set (void)
{
  if (GPIO_ReadInputPin(LOW_HIGH__PORT, LOW_HIGH__PIN) == 0)
    return 1;
  else
    return 0;
}

void three_speed_pin_init (void)
{
  GPIO_Init(THREE_SPEED__PORT,
	    THREE_SPEED__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus three_speed_is_set (void)
{
  if (GPIO_ReadInputPin(THREE_SPEED__PORT, THREE_SPEED__PIN) == 0)
    return 1;
  else
    return 0;
}

void hard_boot_pin_init (void)
{
  GPIO_Init(HARD_BOOT__PORT,
	    HARD_BOOT__PIN,
	    GPIO_MODE_IN_PU_NO_IT);
}

BitStatus hard_boot_is_set (void)
{
  if (GPIO_ReadInputPin(HARD_BOOT__PORT, HARD_BOOT__PIN) == 0)
    return 1;
  else
    return 0;
}

// Back EMF functions
void back_emf_pins_init (void)
{
  GPIO_Init(BACK_EMF_A__PORT, BACK_EMF_A__PIN, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(BACK_EMF_B__PORT, BACK_EMF_B__PIN, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(BACK_EMF_C__PORT, BACK_EMF_C__PIN, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(BACK_EMF_ENABLE__PORT, BACK_EMF_ENABLE__PIN, GPIO_MODE_OUT_PP_LOW_FAST);
}

BitStatus back_emf_a_read (void)
{
  return GPIO_ReadInputPin(BACK_EMF_A__PORT, BACK_EMF_A__PIN);
}

BitStatus back_emf_b_read (void)
{
  return GPIO_ReadInputPin(BACK_EMF_B__PORT, BACK_EMF_B__PIN);
}

BitStatus back_emf_c_read (void)
{
  return GPIO_ReadInputPin(BACK_EMF_C__PORT, BACK_EMF_C__PIN);
}

void back_emf_enable_set (void)
{
  GPIO_WriteHigh(BACK_EMF_ENABLE__PORT, BACK_EMF_ENABLE__PIN);
}

void back_emf_enable_reset (void)
{
  GPIO_WriteLow(BACK_EMF_ENABLE__PORT, BACK_EMF_ENABLE__PIN);
}


