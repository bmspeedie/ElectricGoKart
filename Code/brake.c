/*
 * EGG OpenSource EBike firmware
 *
 * Copyright (C) Casainho, 2015, 2106, 2017.
 *
 * Released under the GPL License, Version 3
 */

#include <stdint.h>
#include "stm8s.h"
#include "stm8s_it.h"
#include "gpio.h"
#include "main.h"
#include "interrupts.h"
#include "brake.h"
#include "cruise_control.h"
#include "motor.h"
#include "pwm.h"
#include "config.h"

// Brake signal
void EXTI_PORTA_IRQHandler(void) __interrupt(EXTI_PORTA_IRQHANDLER)
{
  if (brake_is_set())
  {
    //brake_coast_enable ();
    //stop_cruise_control ();
  }
  else
  {
    //brake_coast_disable ();
    //pwm_set_duty_cycle (0);
    //stop_cruise_control ();
  }
}

void brake_init (void)
{
#if GOKART_IGNORE_AUX_INPUTS
  // PA4 is left as a reset input. No pull-up, no port-A edge interrupt.
  return;
#else
  //hall sensors pins as external input pin interrupt
#ifdef GOKART_MODE
  // pull-up so an unconnected brake input reads "not braking" instead of floating
  GPIO_Init(BRAKE__PORT,
	    BRAKE__PIN,
	    GPIO_MODE_IN_PU_IT);
#else
  GPIO_Init(BRAKE__PORT,
	    BRAKE__PIN,
	    GPIO_MODE_IN_FL_IT); // with external interrupt
#endif

  //initialize the Interrupt sensitivity
  EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOA,
			    EXTI_SENSITIVITY_RISE_FALL);
#endif
}

BitStatus brake_is_set (void)
{
#if GOKART_IGNORE_AUX_INPUTS
  return 0;
#else
  if (GPIO_ReadInputPin(BRAKE__PORT, BRAKE__PIN) == 0)
    return 1;
  else
    return 0;
#endif
}

void brake_coast_enable (void)
{
  TIM1->BKR &= (uint8_t) ~(TIM1_BKR_MOE);
}

void brake_coast_disable (void)
{
  TIM1->BKR |= (uint8_t) (TIM1_BKR_MOE);
}
