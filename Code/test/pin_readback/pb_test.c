/*
 * Low-side gate pin test for the STM8S105 go-kart controller.
 *
 * PC1..PC3 (high-side gates) are held low the whole time, so no half bridge
 * can shoot through. PB0..PB2 (low-side gates) alternate between:
 *   FLOAT  (2 s): inputs, no pull. A healthy pin follows the Analog
 *                 Discovery's weak pull in a pull test.
 *   TOGGLE (3 s): push-pull outputs, PB0 125 Hz, PB1 250 Hz, PB2 500 Hz,
 *                 so each pin can be identified by frequency.
 * UART2 (PD5) prints the phase name at 9600 8N1.
 *
 * Run only with the motor stationary.
 */

#include <stdint.h>

#define REG(addr) (*(volatile uint8_t *)(addr))

#define PB_ODR REG(0x5005)
#define PB_DDR REG(0x5007)
#define PB_CR1 REG(0x5008)
#define PB_CR2 REG(0x5009)

#define PC_ODR REG(0x500A)
#define PC_DDR REG(0x500C)
#define PC_CR1 REG(0x500D)
#define PC_CR2 REG(0x500E)

#define PD_ODR REG(0x500F)
#define PD_DDR REG(0x5011)
#define PD_CR1 REG(0x5012)
#define PD_CR2 REG(0x5013)

#define CLK_CKDIVR REG(0x50C6)

#define IWDG_KR  REG(0x50E0)
#define IWDG_PR  REG(0x50E1)
#define IWDG_RLR REG(0x50E2)

#define UART2_SR   REG(0x5240)
#define UART2_DR   REG(0x5241)
#define UART2_BRR1 REG(0x5242)
#define UART2_BRR2 REG(0x5243)
#define UART2_CR1  REG(0x5244)
#define UART2_CR2  REG(0x5245)
#define UART2_CR3  REG(0x5246)

#define TIM4_CR1  REG(0x5340)
#define TIM4_SR1  REG(0x5342)
#define TIM4_EGR  REG(0x5343)
#define TIM4_PSCR REG(0x5345)
#define TIM4_ARR  REG(0x5346)

#define PB0 (1u << 0)
#define PB1 (1u << 1)
#define PB2 (1u << 2)
#define PB_ALL (PB0 | PB1 | PB2)
#define PC_HIGH_SIDES ((1u << 1) | (1u << 2) | (1u << 3))
#define PD5 (1u << 5)
#define PD6 (1u << 6)

static void kick(void) {
    IWDG_KR = 0xAA;
}

static void delay_ms(uint16_t ms) {
    while (ms--) {
        kick();
        TIM4_SR1 = 0;
        while ((TIM4_SR1 & 0x01) == 0) {
            kick();
        }
    }
}

static void uart_putc(uint8_t c) {
    while ((UART2_SR & 0x80) == 0) {
        kick();
    }
    UART2_DR = c;
}

static void uart_puts(const char *s) {
    while (*s) {
        uart_putc((uint8_t)*s++);
    }
}

void main(void) {
    CLK_CKDIVR = 0x00;

    IWDG_KR = 0x55;
    IWDG_PR = 0x06;
    IWDG_RLR = 0xFF;
    kick();

    TIM4_PSCR = 7;
    TIM4_ARR = 124;
    TIM4_EGR = 0x01;
    TIM4_SR1 = 0;
    TIM4_CR1 = 0x01;

    /* High-side gates: push-pull low, and stay there. */
    PC_ODR &= (uint8_t)~PC_HIGH_SIDES;
    PC_DDR |= (uint8_t)PC_HIGH_SIDES;
    PC_CR1 |= (uint8_t)PC_HIGH_SIDES;
    PC_CR2 &= (uint8_t)~PC_HIGH_SIDES;

    PD_DDR = (uint8_t)((PD_DDR | PD5) & (uint8_t)~PD6);
    PD_CR1 |= (uint8_t)(PD5 | PD6);
    PD_CR2 &= (uint8_t)~(PD5 | PD6);

    UART2_CR1 = 0x00;
    UART2_CR3 = 0x00;
    UART2_BRR2 = 0x02;
    UART2_BRR1 = 0x68;
    UART2_CR2 = 0x0C;

    uart_puts("PBTEST\r\n");

    for (;;) {
        /* FLOAT: PB0..PB2 inputs, no pull-up. */
        PB_DDR &= (uint8_t)~PB_ALL;
        PB_CR1 &= (uint8_t)~PB_ALL;
        PB_CR2 &= (uint8_t)~PB_ALL;
        uart_puts("FLOAT\r\n");
        delay_ms(2000);

        /* TOGGLE: push-pull, distinct rates. */
        PB_ODR &= (uint8_t)~PB_ALL;
        PB_DDR |= (uint8_t)PB_ALL;
        PB_CR1 |= (uint8_t)PB_ALL;
        uart_puts("TOGGLE\r\n");
        for (uint16_t t = 0; t < 3000; t++) {
            uint8_t v = 0;
            if (t & 4) v |= PB0;   /* period 8 ms  -> 125 Hz */
            if (t & 2) v |= PB1;   /* period 4 ms  -> 250 Hz */
            if (t & 1) v |= PB2;   /* period 2 ms  -> 500 Hz */
            PB_ODR = (uint8_t)((PB_ODR & (uint8_t)~PB_ALL) | v);
            delay_ms(1);
        }
    }
}
