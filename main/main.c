#include <stdio.h>
#include <assert.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"

#define PD_PIN 4
#define CS_PIN 15

#define FB_SIZE             800 * 2 * 50
#define MAX_TRANSFER_SIZE   FB_SIZE

static uint32_t tick_cb(void)
{
    int64_t micros = esp_timer_get_time();
    int64_t millis = micros / 1000;
    return millis;
}

static void spi_cb(lv_display_t * disp, lv_ft81x_spi_operation operation, void * data, uint32_t length)
{
    spi_device_handle_t spi = lv_ft81x_get_user_data(disp);
    switch(operation) {
        case LV_FT81X_SPI_OPERATION_CS_ASSERT:
            gpio_set_level(CS_PIN, 0);
            break;
        case LV_FT81X_SPI_OPERATION_CS_DEASSERT:
            gpio_set_level(CS_PIN, 1);
            esp_rom_delay_us(10); /* tiny delay in case a CS_ASSERT immediately follows */
            break;
        case LV_FT81X_SPI_OPERATION_SEND: {
            spi_transaction_t trans = {0};
            while(length) {
                uint32_t sz = length < MAX_TRANSFER_SIZE ? length : MAX_TRANSFER_SIZE;
                trans.length = sz * 8;
                trans.rxlength = 0;
                trans.tx_buffer = data;
                spi_device_polling_transmit(spi, &trans);
                length -= sz;
                data += sz;
            }
            break;
        }
        case LV_FT81X_SPI_OPERATION_RECEIVE: {
            spi_transaction_t trans = {0};
            trans.length = length * 8;
            trans.rxlength = length * 8;
            trans.rx_buffer = data;
            spi_device_polling_transmit(spi, &trans);
            break;
        }
    }
}

void app_main(void)
{
    lv_init();

    lv_tick_set_cb(tick_cb);


    int err;


    gpio_config_t io_conf = {};


    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<PD_PIN);
    gpio_config(&io_conf);

    // reset the ft81x
    gpio_set_level(PD_PIN, 0);
    vTaskDelay(6 / portTICK_PERIOD_MS);
    gpio_set_level(PD_PIN, 1);
    vTaskDelay(21 / portTICK_PERIOD_MS);


    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<CS_PIN);

    gpio_config(&io_conf);
    gpio_set_level(CS_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    spi_bus_config_t buscfg = {
        .miso_io_num = 12,
        .mosi_io_num = 13,
        .sclk_io_num = 14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX_TRANSFER_SIZE,
    };
    //Initialize the SPI bus
    err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(err == 0);

    spi_device_interface_config_t devcfg = {
        // .command_bits = 10,
        .clock_speed_hz = 32*1000*1000,
        .mode = 0,          //SPI mode 0
        /*
         * The timing requirements to read the busy signal from the EEPROM cannot be easily emulated
         * by SPI transactions. We need to control CS pin by SW to check the busy signal manually.
         */
        .spics_io_num = -1,
        .queue_size = 1,
        .flags = 0,
        // .pre_cb = cs_high,
        // .post_cb = cs_low,
        // .input_delay_ns = EEPROM_INPUT_DELAY_NS,  //the EEPROM output the data half a SPI clock behind.
    };
    //Attach the EEPROM to the SPI bus
    spi_device_handle_t spi;
    err = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    assert(err == 0);

    // taken from https://github.com/lvgl/lvgl_esp32_drivers/blob/9fed1cc47b5a45fec6bae08b55d2147d3b50260c/lvgl_tft/EVE_config.h
    // NHD-5.0-800480FT-CxXx-xxx 800x480 5.0" Newhaven, resistive or capacitive, FT81x
    // EVE_NHD_50
    #define EVE_VSYNC0	(0L)
    #define EVE_VSYNC1	(3L)
    #define EVE_VOFFSET	(32L)
    #define EVE_VCYCLE	(525L)
    #define EVE_HSYNC0	(0L)
    #define EVE_HSYNC1	(48L)
    #define EVE_HOFFSET	(88L)
    #define EVE_HCYCLE 	(928L)
    #define EVE_PCLKPOL	(0L)
    #define EVE_SWIZZLE	(0L)
    #define EVE_PCLK	(2L)
    #define EVE_CSPREAD	(1L)

    lv_ft81x_parameters_t params = {
        .hor_res = 800,
        .ver_res = 480,

        .hcycle = EVE_HCYCLE,
        .hoffset = EVE_HOFFSET,
        .hsync0 = EVE_HSYNC0,
        .hsync1 = EVE_HSYNC1,
        .vcycle = EVE_VCYCLE,
        .voffset = EVE_VOFFSET,
        .vsync0 = EVE_VSYNC0,
        .vsync1 = EVE_VSYNC1,
        .swizzle = EVE_SWIZZLE,
        .pclkpol = EVE_PCLKPOL,
        .cspread = EVE_CSPREAD,
        .pclk = EVE_PCLK,

        .has_crystal = true,
        .is_bt81x = false
    };

    static uint8_t fb[FB_SIZE] __attribute__((aligned(4)));
    lv_display_t * disp = lv_ft81x_create(&params, fb, FB_SIZE, spi_cb, spi);
    LV_UNUSED(disp);

    // lv_obj_t * slider = lv_slider_create(lv_screen_active());
    // lv_obj_set_x(slider, 30);
    // lv_obj_set_y(slider, 10);
    // lv_slider_set_value(slider, 60, LV_ANIM_OFF);

    // lv_example_anim_2();

    lv_demo_widgets();
    lv_demo_widgets_start_slideshow();

    while(1) {
        uint32_t millis_to_delay = lv_timer_handler();
        if(millis_to_delay == LV_NO_TIMER_READY) {
            return;
        }
        vTaskDelay(millis_to_delay / portTICK_PERIOD_MS);
    }
}
