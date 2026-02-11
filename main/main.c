#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <hd44780.h>
#include <esp_idf_lib_helpers.h>
#include <inttypes.h>
#include <stdio.h>
#include "driver/gpio.h"
#include <sdkconfig.h>
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"
#include "math.h"

//Pin number declaration
#define redLED_PIN          4  
#define greenLED_PIN        5   
#define driveSeatBelt       16
#define passengerSeatBelt   17 
#define Alarm               2
#define ignitionButton      1
#define driveSeatSense      37
#define passengerSeatSense  36
#define LCD_bright          18

//ADC constants
#define CHANNEL_Mode    ADC_CHANNEL_6
#define CHANNEL_Timer   ADC_CHANNEL_5
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define BITWIDTH        ADC_BITWIDTH_12
#define DELAY_MS        20                  // Loop delay (ms)
adc_oneshot_unit_handle_t adc1_handle;      // ADC for Mode
adc_oneshot_unit_handle_t adc2_handle;      // ADC for Timer

//Global boolean values
bool initial_message = true;
bool dSense = false;
bool dsbelt = false;
bool pSense = false;
bool psbelt = false;
bool engine = false;
bool hold = false;

//Function prototypes
static void pinConfig(void);
static void ADC_Config(void);
static void LCD_Config(void);
static bool enable(void);
static bool ignitionPressed(void);


static uint32_t get_time_sec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

void lcd_test(void *pvParameters)
{
    hd44780_t lcd =
    {
        .write_cb = NULL,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = GPIO_NUM_8,
            .e  = GPIO_NUM_3,
            .d4 = GPIO_NUM_9,
            .d5 = GPIO_NUM_10,
            .d6 = GPIO_NUM_11,
            .d7 = GPIO_NUM_12,
            .bl = HD44780_NOT_USED
        }
    };

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "\x08 Hello, World!");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "\x09 ");

    char time[16];

    while (1)
    {
        hd44780_gotoxy(&lcd, 2, 1);

        snprintf(time, 7, "%" PRIu32 "  ", get_time_sec());
        time[sizeof(time) - 1] = 0;

        hd44780_puts(&lcd, time);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main()
{
    pinConfig();
    ADC_Config();
    LCD_Config();
    while(1){
        bool ignitEn = ignitionPressed();
        if (!engine) {
            //Check if the engine is not started.
            bool ready = enable();

            if (dSense && initial_message){
                //Prints out the welcome message when the driver is seated.
                printf("Welcome to enhanced Alarm system model 218 -W25\n");
                initial_message = false;

            }

            if(ready){
                //Turn on greenlight if the engine is ready
                gpio_set_level(greenLED_PIN, 1);
            }
            else {
                gpio_set_level(greenLED_PIN, 0);
            }

            if(ignitEn){
                //Turn on the engine if ready and ignite pressed.
                if (ready) {
                    printf("Starting the engine.\n");
                    gpio_set_level(greenLED_PIN, 0);
                    gpio_set_level(redLED_PIN, 1);
                    engine = true;
                }

                else {
                    //Prints error and raise alarm otherwise.
                    gpio_set_level (Alarm, 1);

                    if (!dSense){
                    printf("Driver seat not occupied\n");
                    }
                
                    if (!dsbelt){
                    printf("Driver seatbelt not fastened\n");
                    }

                    if (!pSense){
                    printf("Passenger seat not occupied\n");
                    }

                    if (!psbelt){
                    printf("Passenger seatbelt not fastened\n");
                    }
                    vTaskDelay (3000/ portTICK_PERIOD_MS);
                }
            }

            else {
                gpio_set_level (Alarm, 0);
            }
        }

        else {
            //Check if the engine is pressed
            if (ignitEn) {
                //Turn off engine is ignite is pressed again.
                gpio_set_level (redLED_PIN, 0);
                printf("Stopping the engine.\n");
                engine = false;
            }
            else {
                xTaskCreate(lcd_test, "lcd_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
            }
        }
    }
}

//Configure the GPIO
void pinConfig(void){
    gpio_reset_pin(greenLED_PIN);
    gpio_reset_pin(redLED_PIN);
    gpio_reset_pin(ignitionButton);
    gpio_reset_pin(driveSeatBelt);
    gpio_reset_pin(passengerSeatBelt);
    gpio_reset_pin(driveSeatSense);
    gpio_reset_pin(passengerSeatSense);
    gpio_reset_pin(Alarm);
    gpio_reset_pin(LCD_bright);

    gpio_set_direction(greenLED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(redLED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(Alarm, GPIO_MODE_OUTPUT);
    gpio_set_direction(ignitionButton, GPIO_MODE_INPUT);
    gpio_set_direction(driveSeatBelt, GPIO_MODE_INPUT);
    gpio_set_direction(passengerSeatBelt, GPIO_MODE_INPUT);
    gpio_set_direction(driveSeatSense, GPIO_MODE_INPUT);
    gpio_set_direction(passengerSeatSense, GPIO_MODE_INPUT);

    gpio_pullup_en(ignitionButton);
    gpio_pullup_en(driveSeatBelt);
    gpio_pullup_en(driveSeatSense);
    gpio_pullup_en(passengerSeatBelt);
    gpio_pullup_en(passengerSeatSense);

    gpio_set_level(greenLED_PIN, 0);
    gpio_set_level(redLED_PIN, 0);
    gpio_set_level(Alarm, 0);

}

//Configure the ADC
void ADC_Config (void) {
    // Unit configuration
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };                                                  
    adc_oneshot_new_unit(&init_config1, &adc1_handle);  
    adc_oneshot_new_unit(&init_config1, &adc2_handle);  

    // Channel configuration
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = BITWIDTH
    };                                                  
    adc_oneshot_config_channel (adc1_handle, CHANNEL_Mode, &config);
    adc_oneshot_config_channel (adc2_handle, CHANNEL_Timer, &config);
}

//Configure the LCD
void LCD_Config (void) {
    hd44780_t lcd =
    {
        .write_cb = NULL,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = GPIO_NUM_8,
            .e  = GPIO_NUM_3,
            .d4 = GPIO_NUM_9,
            .d5 = GPIO_NUM_10,
            .d6 = GPIO_NUM_11,
            .d7 = GPIO_NUM_12,
            .bl = HD44780_NOT_USED
        }
    };

    ESP_ERROR_CHECK(hd44780_init(&lcd));
}

/*Check the seat and seatbelt sensor
to see if the engine is ready.*/
bool enable(void){
    bool dslvl = gpio_get_level(driveSeatSense);
    bool dsbeltlvl = gpio_get_level(driveSeatBelt);
    bool pslvl = gpio_get_level(passengerSeatSense);
    bool psbltlvl = gpio_get_level(passengerSeatBelt);

    if (!dslvl){
        dSense = true; //Driver sensor
    }
    else{dSense = false;}

    if (!dsbeltlvl){
        dsbelt = true; // driver seatbelt sensor
    }
    else{dsbelt = false;}

    if (!pslvl){
        pSense = true; // passenger seat level
    }
    else{pSense = false;}

    if (!psbltlvl){
        psbelt = true; // passenger seatbelt level
    }
    else{psbelt = false;}

    bool IgnitReady = dSense && dsbelt && pSense && psbelt;
    return IgnitReady;
}


/*Check if the Ignition button is pressed 
(only return true after a hold and release)*/
bool ignitionPressed (void) {
    bool igniteHold = gpio_get_level(ignitionButton) == 0;
    if (igniteHold) {
        hold = true;
    }
    if (hold && !igniteHold)
    {
        hold = false;
        return true;
    }
    return false;
}