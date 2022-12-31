/**
 * Copyright (C) 2021, Mate Kukri <km@mkukri.xyz>
 * Based on "pico-serprog" by Thomas Roth <code@stacksmashing.net>
 * 
 * Licensed under GPLv3
 *
 * Also based on stm32-vserprog:
 *  https://github.com/dword1511/stm32-vserprog
 * 
 */

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "tusb.h"
#include "serprog.h"

#include "ice_fpga_flash.h"

#define CDC_ITF_0     0           // USB CDC interface 0

/*** from pico-ice***/
#define ICE_FPGA_CDONE_PIN 26
#define ICE_FPGA_CRESET_PIN 27
/*** from pico-ice***/

static inline void cs_select(uint cs_pin)
{
    soft_spi_chip_select();
}

static inline void cs_deselect(uint cs_pin)
{
    soft_spi_chip_deselect();
}

static void enable_spi(uint baud)
{
    gpio_put(ICE_FPGA_CRESET_PIN, false);
    sleep_ms(10);
    ice_fpga_flash_init();
}

static void disable_spi()
{
    ice_fpga_flash_deinit();
    sleep_ms(10);
    gpio_put(ICE_FPGA_CRESET_PIN, true);
    sleep_ms(10);
    gpio_put(ICE_FPGA_CRESET_PIN, false);
    sleep_ms(10);
    gpio_put(ICE_FPGA_CRESET_PIN, true);
}

void wait_for_read(void)
{
    do
    {
        tud_task();
    }
    while (!tud_cdc_n_available(CDC_ITF_0));
}

void readbytes_blocking(void *b, uint32_t len)
{
    while (len) {
        wait_for_read();
        uint32_t r = tud_cdc_n_read(CDC_ITF_0, b, len);
        b += r;
        len -= r;
    }
}

uint8_t readbyte_blocking(void)
{
    wait_for_read();
    uint8_t b;
    tud_cdc_n_read(CDC_ITF_0, &b, 1);
    return b;
}

void wait_for_write(void)
{
    do {
        tud_task();
    } while (!tud_cdc_n_write_available(CDC_ITF_0));
}

void sendbytes_blocking(const void *b, uint32_t len)
{
    while (len) {
        wait_for_write();
        uint32_t w = tud_cdc_n_write(CDC_ITF_0, b, len);
        b += w;
        len -= w;
    }
}

void sendbyte_blocking(uint8_t b)
{
    wait_for_write();
    tud_cdc_n_write(CDC_ITF_0, &b, 1);
}

void command_loop(void)
{
    uint baud = 12000000;

    for (;;) {
        switch (readbyte_blocking()) {
        case S_CMD_NOP:
            sendbyte_blocking(S_ACK);
            break;
        case S_CMD_Q_IFACE:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0x01);
            sendbyte_blocking(0x00);
            break;
        case S_CMD_Q_CMDMAP:
            {
                const uint32_t cmdmap[8] = {
                    (1 << S_CMD_NOP)       |
                    (1 << S_CMD_Q_IFACE)   |
                    (1 << S_CMD_Q_CMDMAP)  |
                    (1 << S_CMD_Q_PGMNAME) |
                    (1 << S_CMD_Q_SERBUF)  |
                    (1 << S_CMD_Q_BUSTYPE) |
                    (1 << S_CMD_SYNCNOP)   |
                    (1 << S_CMD_O_SPIOP)   |
                    (1 << S_CMD_S_BUSTYPE) |
                    (1 << S_CMD_S_SPI_FREQ)|
                    (1 << S_CMD_S_PIN_STATE)
                };

                sendbyte_blocking(S_ACK);
                sendbytes_blocking((uint8_t *) cmdmap, sizeof cmdmap);
                break;
            }
        case S_CMD_S_PIN_STATE:
            {
                uint8_t pinstate;
                readbytes_blocking(&pinstate, 1);
                if (pinstate)
                {
                    enable_spi(baud);
                }
                else
                {
                    disable_spi();
                }
                sendbyte_blocking(S_ACK);
            }
            break;
        case S_CMD_Q_PGMNAME:
            {
                static const char progname[16] = "pico-ice-serprog";

                sendbyte_blocking(S_ACK);
                sendbytes_blocking(progname, sizeof progname);
                break;
            }
        case S_CMD_Q_SERBUF:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0xFF);
            sendbyte_blocking(0xFF);
            break;
        case S_CMD_Q_BUSTYPE:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking((1 << 3));
            break;
        case S_CMD_SYNCNOP:
            sendbyte_blocking(S_NAK);
            sendbyte_blocking(S_ACK);
            break;
        case S_CMD_S_BUSTYPE:
            if((uint8_t) readbyte_blocking() & (1 << 3))
                sendbyte_blocking(S_ACK);
            else
                sendbyte_blocking(S_NAK);
            break;
        case S_CMD_O_SPIOP:
            {
                uint8_t buf[4096];

                uint32_t wlen = 0;
                readbytes_blocking(&wlen, 3);
                uint32_t rlen = 0;
                readbytes_blocking(&rlen, 3);

                soft_spi_chip_select();
                while (wlen) {
                    uint32_t cur = MIN(wlen, sizeof buf);
                    readbytes_blocking(buf, cur);
                    soft_spi_write_blocking(buf, cur);
                    wlen -= cur;
                }

                sendbyte_blocking(S_ACK);

                while (rlen) {
                    uint32_t cur = MIN(rlen, sizeof buf);
                    soft_spi_read_blocking(0x00,buf,cur);
                    sendbytes_blocking(buf, cur);
                    rlen -= cur;
                }
               
                soft_spi_chip_deselect();

            }
            break;
        case S_CMD_S_SPI_FREQ:
            {
                uint32_t want_baud;
                readbytes_blocking(&want_baud, 4);
                if (want_baud) {
                    //Set frequency
                    //baud = spi_set_baudrate(SPI_IF, want_baud);
                    //Send back actual value
                    sendbyte_blocking(S_ACK);
                    sendbytes_blocking(&baud, 4);
                } else {
                    //0 Hz is reserved
                    sendbyte_blocking(S_NAK);
                }
                break;
            }
        case 0x9F:
            sendbyte_blocking(S_ACK);
            break;
        default:
            sendbyte_blocking(S_NAK);
            break;
        }

        tud_cdc_n_write_flush(CDC_ITF_0);
    }
}

int main()
{
   
    // Setup USB
    tusb_init();

    /****INIT ***/
    ice_fpga_flash_deinit();

    /***fpga init*/
    // Keep the FPGA in reset until ice_fpga_reset() is called.
    gpio_init(ICE_FPGA_CRESET_PIN);
    gpio_put(ICE_FPGA_CRESET_PIN, false);
    gpio_set_dir(ICE_FPGA_CRESET_PIN, GPIO_OUT);

    // Input pin for sensing configuration status.
    gpio_init(ICE_FPGA_CDONE_PIN);
    gpio_set_dir(ICE_FPGA_CDONE_PIN, GPIO_IN);

    ice_fpga_flash_init();


    /*** start running fgpa bitstream***/
    ice_fpga_flash_deinit();
    gpio_put(ICE_FPGA_CRESET_PIN, true);
    sleep_ms(10);
    gpio_put(ICE_FPGA_CRESET_PIN, false);
    sleep_ms(10);
    gpio_put(ICE_FPGA_CRESET_PIN, true);

    command_loop();

    return 0;
}

