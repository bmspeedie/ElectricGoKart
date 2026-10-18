/*
 * Connectivity test for the STM8S105 on the go-kart controller.
 *
 * PC1 double-blinks and PC2 pulses once per second so the two pins can be
 * told apart. UART2 (PD5 TX, PD6 RX) prints a line each cycle at 9600 8N1
 * and echoes any byte it receives.
 *
 * PC1 and PC2 are the phase high-side gate pins. Run this only with the
 * motor power stage unpowered.
 */

#include <stdint.h>

#define REG(addr) (*(volatile uint8_t *)(addr))

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

#define PC1 (1u << 1)
#define PC2 (1u << 2)
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

/* Echo a received byte, if one is waiting. */
static void uart_poll_echo(void) {
    if (UART2_SR & 0x20) {
        uint8_t b = UART2_DR;
        uart_putc(b);
    }
}

static void pins(uint8_t pc1, uint8_t pc2, uint16_t ms) {
    if (pc1) {
        PC_ODR |= PC1;
    } else {
        PC_ODR &= (uint8_t)~PC1;
    }
    if (pc2) {
        PC_ODR |= PC2;
    } else {
        PC_ODR &= (uint8_t)~PC2;
    }
    while (ms--) {
        uart_poll_echo();
        delay_ms(1);
    }
}

void main(void) {
    /* HSI 16 MHz, no prescaler. */
    CLK_CKDIVR = 0x00;

    /* Stretch the independent watchdog in case the option byte starts it. */
    IWDG_KR = 0x55;
    IWDG_PR = 0x06;
    IWDG_RLR = 0xFF;
    kick();

    /* 1 ms tick: 16 MHz / 128 = 125 kHz, ARR 124. */
    TIM4_PSCR = 7;
    TIM4_ARR = 124;
    TIM4_EGR = 0x01;
    TIM4_SR1 = 0;
    TIM4_CR1 = 0x01;

    PC_ODR &= (uint8_t)~(PC1 | PC2);
    PC_DDR |= (uint8_t)(PC1 | PC2);
    PC_CR1 |= (uint8_t)(PC1 | PC2);
    PC_CR2 &= (uint8_t)~(PC1 | PC2);

    /* PD5 UART2 TX push-pull, PD6 UART2 RX pull-up. */
    PD_DDR = (uint8_t)((PD_DDR | PD5) & (uint8_t)~PD6);
    PD_CR1 |= (uint8_t)(PD5 | PD6);
    PD_CR2 &= (uint8_t)~(PD5 | PD6);
    (void)PD_ODR;

    /* 9600 8N1 at 16 MHz: BRR2 then BRR1, then enable TX and RX. */
    UART2_CR1 = 0x00;
    UART2_CR3 = 0x00;
    UART2_BRR2 = 0x02;
    UART2_BRR1 = 0x68;
    UART2_CR2 = 0x0C;

    /* Three pulses on both pins marks the start of the test firmware. */
    for (uint8_t n = 0; n < 3; n++) {
        pins(1, 1, 80);
        pins(0, 0, 80);
    }

    uart_puts("PINTEST\r\n");

    for (;;) {
        /*
         * 1 second cycle:
         *   150 ms PC1 high, PC2 low
         *   150 ms both low
         *   150 ms PC1 high, PC2 low
         *   550 ms PC1 low,  PC2 high
         * PC1 double-blinks. PC2 is a single later pulse.
         */
        uart_puts("PC1 blink PC2 pulse\r\n");
        pins(1, 0, 150);
        pins(0, 0, 150);
        pins(1, 0, 150);
        pins(0, 1, 550);
    }
}
