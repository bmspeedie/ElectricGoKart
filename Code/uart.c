/*
 * EGG OpenSource EBike firmware
 *
 * Copyright (C) Casainho, 2015, 2106, 2017.
 *
 * Released under the GPL License, Version 3
 */

#include <stdint.h>
#include <stdio.h>
#include "uart.h"
#include "stm8s.h"
#include "stm8s_uart2.h"
#include "interrupts.h"
#include "ACAcontrollerState.h"

static uint8_t ui8_uarx_buffer[UART_RINGBUFFER_SIZE];
static uint8_t ui8_uatx_buffer[UART_TX_RINGBUFFER_SIZE];
static uint8_t ui8_rx_fillpointer = 0;
static uint8_t ui8_tx_fillpointer = 0;
static uint8_t ui8_rx_digestpointer = 0;
static uint8_t ui8_tx_digestpointer = 0;

static uint8_t ui8_rx_packet_start_pos = 0;

// Queue a byte for uart_send_if_avail() (main loop). Never blocks: a byte
// that does not fit is dropped, so a long printf can truncate a telemetry
// line but cannot stall the 50 Hz control loop (a blocking 125-char line at
// 9600 baud held the slow loop off for 130 ms at a time).
// The queue is drained by the UART2 TX-empty interrupt, not by the main
// loop: polling once per main-loop pass fell below the 1.04 ms byte time
// and the queue overflowed (truncated lines, 2-byte LCD3 frames).
void uart_put_buffered(uint8_t c) {
    uint8_t ui8_next = ui8_tx_fillpointer + 1; // wraps at UART_TX_RINGBUFFER_SIZE (256)
    if (ui8_next == ui8_tx_digestpointer) {
        return; // full
    }
    ui8_uatx_buffer[ui8_tx_fillpointer] = c;
    ui8_tx_fillpointer = ui8_next;
    UART2_ITConfig(UART2_IT_TXE, ENABLE);
}

// Bytes queued and not yet handed to the UART. Callers that produce long
// bursts (the telemetry line) check this first so a burst never arrives at
// a nearly full queue: bytes dropped mid-line come out as a sparse scatter
// of characters (one per byte time as slots free up), which looks like
// garbage on the wire.
uint8_t uart_tx_pending(void) {
    return (uint8_t) (ui8_tx_fillpointer - ui8_tx_digestpointer);
}

void UART2_TX_IRQHandler(void) __interrupt(UART2_TX_IRQHANDLER) {
    if (ui8_tx_fillpointer != ui8_tx_digestpointer) {
        UART2_SendData8(ui8_uatx_buffer[ui8_tx_digestpointer++]); // pointer wraps at 256
    } else {
        UART2_ITConfig(UART2_IT_TXE, DISABLE); // nothing left; TXE stays set until the next byte
    }
}

uint8_t uart_get_packet_start_pos(void) {
    return ui8_rx_packet_start_pos;
}

uint8_t byte_avail_at_position(void) {
    if (ui8_rx_fillpointer != ui8_rx_digestpointer) {
        return ui8_rx_digestpointer;
    }
    return UART_EMPTY_INDICATOR;
}

uint8_t uart_get_buffered(void) {
    uint8_t c = ui8_uarx_buffer[ui8_rx_digestpointer++];
    if (ui8_rx_digestpointer >= UART_RINGBUFFER_SIZE) {
        ui8_rx_digestpointer = 0;
    }
    return c;
}
// FIXME only needed as long as the other displays do not use new functions
// and instead have their own irq handler

void UART2_IRQHandler(void) __interrupt(UART2_IRQHANDLER) {

    // if there is something to read, read it into cyclic buffer
    if (UART2_GetFlagStatus(UART2_FLAG_RXNE) == SET) {
        // assume data packet timeout after 20ms / new packet starting
        if (ui16_time_ticks_for_uart_timeout > UART_PACKET_TIMEOUT_TICKS) {
            ui8_rx_packet_start_pos = ui8_rx_fillpointer;
        }
        ui16_time_ticks_for_uart_timeout=0;
        
        ui8_uarx_buffer[ui8_rx_fillpointer++] = UART2_ReceiveData8();
        if (ui8_rx_fillpointer >= UART_RINGBUFFER_SIZE) {
            ui8_rx_fillpointer = 0;
        }
        
    } else {

        // catch errors and process them in a reasonable manner which is always to ignore them :)
        if (UART2_GetITStatus(UART2_IT_IDLE) == SET) {
            UART2_ReceiveData8(); // -> clear!
        }
        if (UART2_GetITStatus(UART2_IT_LBDF) == SET) {
            UART2_ReceiveData8(); // -> clear!
        }
        if (UART2_GetITStatus(UART2_IT_OR) == SET) {
            UART2_ReceiveData8(); // -> clear!
        }
        if (UART2_GetITStatus(UART2_IT_PE) == SET) {
            UART2_ReceiveData8(); // -> clear!
        }
    }
}

void uart_send_if_avail() {
    // transmission is interrupt driven; just make sure the interrupt is armed if anything is queued
    if (ui8_tx_fillpointer != ui8_tx_digestpointer) {
        UART2_ITConfig(UART2_IT_TXE, ENABLE);
    }
}

void uart_fill_rx_packet_buffer(uint8_t *buffer, uint8_t bufferSize, uint8_t *bufferPos) {
    while (byte_avail_at_position() != UART_EMPTY_INDICATOR) {

        uint8_t uart_packet_start_pos = uart_get_packet_start_pos();
        uint8_t byte_avail_at_pos = byte_avail_at_position();

        if ((*bufferPos == 0) && (uart_packet_start_pos != byte_avail_at_pos)) {
            uart_get_buffered(); //discard incomplete chunks
        } else if (*bufferPos < bufferSize) {
            *(buffer + (*bufferPos)) = uart_get_buffered();
            (*bufferPos)++;
        }
    }
}

void uart_init(void) {
    UART2_DeInit();
    UART2_Init((uint32_t) 9600,
            UART2_WORDLENGTH_8D,
            UART2_STOPBITS_1,
            UART2_PARITY_NO,
            UART2_SYNCMODE_CLOCK_DISABLE,
            UART2_MODE_TXRX_ENABLE);
    UART2_ITConfig(UART2_IT_RXNE_OR, ENABLE);
}

#if __SDCC_REVISION < 9624

void putchar(char c) {
    uart_put_buffered((uint8_t) c); // non-blocking, see uart_put_buffered()
}

void putbyte(uint8_t c) {
    uart_put_buffered(c);
}
#else

int putchar(int c) {
    uart_put_buffered((uint8_t) c); // non-blocking, see uart_put_buffered()
    return ((unsigned char) c);
}
#endif

#if __SDCC_REVISION < 9989
char getchar(void)
#else

int getchar(void)
#endif
{
    uint8_t c = 0;



    /* Loop until the Read data register flag is SET */
    while (UART2_GetFlagStatus(UART2_FLAG_RXNE) == RESET);

    c = UART2_ReceiveData8();

    return (c);
}
