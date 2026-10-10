/*
 * BMSBattery S series motor controllers firmware
 *
 * Copyright (C) Casainho, 2017.
 *
 * Released under the GPL License, Version 3
 */

/* Connections:
 *
 * Motor PHASE_A: yellow wire
 * Motor PHASE_B: green wire
 * Motor PHASE_C: blue wire
 *
 *
 * PIN				  | IN/OUT| Works?|Function
 * ----------------------------------------------------------
 * PA1: NC
 * PA2: cruise
 * PA4: reverse
 * PA5: self study
 * PA6: NC
 *
 * PB0: Phase A low side PWM
 * PB1: Phase B low side PWM
 * PB2: Phase C low side PWM
 * PB3: low/high
 * PB4: low pass filtered current
 * PB5: battery voltage
 * PB6: unfiltered current 
 * PB7: throttle
 *
 * PC1: Phase A high side PWM
 * PC2: Phase B high side PWM
 * PC3: Phase C high side PWM
 * PC5: hall effect position feedback sensor phase B
 * PC6: back EMF phase C
 * PC7: NC
 *
 * PD0: NC
 * PD1: NC
 * PD2: back EMF phase B
 * PD3: hall effect position feedback sensor phase C
 * PD4: back EMF enable? Or voltage low/high switching on back EMF?
 * PD5: TTL serial to display
 * PD6: NC
 * PD7: NC
 *
 * PE0: NC
 * PE1: back EMF phase A
 * PE2: NC
 * PE5: hall effect position feedback sensor phase A
 * PE6: NC
 *
 * PG0: hard boot
 * PG1: 3 speed
 *
 */

#ifndef _GPIO_H_
#define _GPIO_H_

#include "main.h"
#include "stm8s_gpio.h"

// Current sensing pins (updated for new hardware)
#define CURRENT_PHASE_B__PIN      GPIO_PIN_6  // PB6: unfiltered current
#define CURRENT_PHASE_B__PORT     GPIOB

#define CURRENT_TOTAL__PIN        GPIO_PIN_6  // PB6: unfiltered current (same as phase B)
#define CURRENT_TOTAL__PORT       GPIOB

#define CURRENT_MOTOR_TOTAL__PIN  GPIO_PIN_6  // PB6: unfiltered current
#define CURRENT_MOTOR_TOTAL__PORT GPIOB

#define CURRENT_MOTOR_TOTAL_FILTERED__PIN GPIO_PIN_4  // PB4: low pass filtered current
#define CURRENT_MOTOR_TOTAL_FILTERED__PORT GPIOB

#define BATTERY_VOLTAGE__PIN      GPIO_PIN_5  // PB5: battery voltage
#define BATTERY_VOLTAGE__PORT     GPIOB

#define CURRENT_MOTOR_TOTAL_OVER__PIN  GPIO_PIN_7  // PD7: NC (keeping for compatibility)
#define CURRENT_MOTOR_TOTAL_OVER__PORT GPIOD


// Hall sensor pins (updated for new hardware)
#define HALL_SENSOR_A__PIN      GPIO_PIN_5  // PE5: hall effect position feedback sensor phase A
#define HALL_SENSOR_B__PIN      GPIO_PIN_5  // PC5: hall effect position feedback sensor phase B
#define HALL_SENSOR_C__PIN      GPIO_PIN_3  // PD3: hall effect position feedback sensor phase C

#define HALL_SENSOR_A__PORT       GPIOE
#define HALL_SENSOR_B__PORT       GPIOC
#define HALL_SENSOR_C__PORT       GPIOD
#define HALL_SENSORS__PORT        GPIOE  // Primary port for hall sensors
#define HALL_SENSORS_MASK 	      (HALL_SENSOR_A__PIN | HALL_SENSOR_B__PIN | HALL_SENSOR_C__PIN)

// PWM pins (updated for new hardware)
#define PMW_PHASE_A_LOW__PIN      GPIO_PIN_0  // PB0: Phase A low side PWM
#define PMW_PHASE_A_LOW__PORT     GPIOB
#define PMW_PHASE_B_LOW__PIN      GPIO_PIN_1  // PB1: Phase B low side PWM
#define PMW_PHASE_B_LOW__PORT     GPIOB
#define PMW_PHASE_C_LOW__PIN      GPIO_PIN_2  // PB2: Phase C low side PWM
#define PMW_PHASE_C_LOW__PORT     GPIOB
#define PMW_PHASE_A_HIGH__PIN     GPIO_PIN_1  // PC1: Phase A high side PWM
#define PMW_PHASE_A_HIGH__PORT    GPIOC
#define PMW_PHASE_B_HIGH__PIN     GPIO_PIN_2  // PC2: Phase B high side PWM
#define PMW_PHASE_B_HIGH__PORT    GPIOC
#define PMW_PHASE_C_HIGH__PIN     GPIO_PIN_3  // PC3: Phase C high side PWM
#define PMW_PHASE_C_HIGH__PORT    GPIOC

// UART pins (updated for new hardware)
#define UART2_TX__PIN             GPIO_PIN_5  // PD5: TTL serial to display
#define UART2_TX__PORT            GPIOD
#define UART2_RX__PIN             GPIO_PIN_6  // PD6: NC (keeping for compatibility)
#define UART2_RX__PORT            GPIOD

// Control input pins (updated for new hardware)
#define BRAKE__PIN                GPIO_PIN_4  // PA4: reverse (using as brake)
#define BRAKE__PORT               GPIOA

#define THROTTLE__PIN             GPIO_PIN_7  // PB7: throttle
#define THROTTLE__PORT            GPIOB

#define REGEN_THROTTLE__PIN       GPIO_PIN_7  // PB7: throttle (same as throttle)
#define REGEN_THROTTLE__PORT      GPIOB

// New control pins
#define CRUISE__PIN               GPIO_PIN_2  // PA2: cruise
#define CRUISE__PORT              GPIOA

#define REVERSE__PIN              GPIO_PIN_4  // PA4: reverse
#define REVERSE__PORT             GPIOA

#define SELF_STUDY__PIN           GPIO_PIN_5  // PA5: self study
#define SELF_STUDY__PORT          GPIOA

#define LOW_HIGH__PIN             GPIO_PIN_3  // PB3: low/high
#define LOW_HIGH__PORT            GPIOB

#define THREE_SPEED__PIN          GPIO_PIN_1  // PG1: 3 speed
#define THREE_SPEED__PORT         GPIOG

#define HARD_BOOT__PIN            GPIO_PIN_0  // PG0: hard boot
#define HARD_BOOT__PORT           GPIOG

// Back EMF pins
#define BACK_EMF_A__PIN           GPIO_PIN_1  // PE1: back EMF phase A
#define BACK_EMF_A__PORT          GPIOE

#define BACK_EMF_B__PIN           GPIO_PIN_2  // PD2: back EMF phase B
#define BACK_EMF_B__PORT          GPIOD

#define BACK_EMF_C__PIN           GPIO_PIN_6  // PC6: back EMF phase C
#define BACK_EMF_C__PORT          GPIOC

#define BACK_EMF_ENABLE__PIN      GPIO_PIN_4  // PD4: back EMF enable
#define BACK_EMF_ENABLE__PORT     GPIOD

// Legacy pins (keeping for compatibility)
#define PAS__PIN                  GPIO_PIN_0  // PD0: NC (keeping for compatibility)
#define PAS__PORT                 GPIOD

#define SPEED__PIN                GPIO_PIN_5  // PC5: hall effect position feedback sensor phase B (reusing)
#define SPEED__PORT               GPIOC

#define DEBUG__PIN                GPIO_PIN_2  // PD2: back EMF phase B (reusing)
#define DEBUG__PORT               GPIOD

#define LIGHT__PIN                GPIO_PIN_4  // PC4: NC (keeping for compatibility)
#define LIGHT__PORT               GPIOC


void gpio_init (void);
void debug_pin_init (void);
void debug_pin_set (void);
void debug_pin_reset (void);
void PAS_init (void);
void SPEED_init (void);
void light_pin_init (void);
void light_pin_set (void);
void light_pin_reset (void);

// New control input functions for updated hardware
void cruise_pin_init (void);
BitStatus cruise_is_set (void);
void reverse_pin_init (void);
BitStatus reverse_is_set (void);
void self_study_pin_init (void);
BitStatus self_study_is_set (void);
void low_high_pin_init (void);
BitStatus low_high_is_set (void);
void three_speed_pin_init (void);
BitStatus three_speed_is_set (void);
void hard_boot_pin_init (void);
BitStatus hard_boot_is_set (void);

// Back EMF functions
void back_emf_pins_init (void);
BitStatus back_emf_a_read (void);
BitStatus back_emf_b_read (void);
BitStatus back_emf_c_read (void);
void back_emf_enable_set (void);
void back_emf_enable_reset (void);

#endif /* GPIO_H_ */
