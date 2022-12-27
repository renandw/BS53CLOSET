#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include <button.h>
#include <toggle.h>
#include "ota-api.h"

// The GPIO pin that is connected to RELAY#1 on the board.
const int relay_gpio_1 = 0;

// The GPIO pin that is connected to RELAY#2 on the board.
const int relay_gpio_2 = 2;

// The GPIO pin that is connected to the button1 on the Board.
#define TOGGLE_PIN_1 4
#ifndef TOGGLE_PIN_1
#error TOGGLE_PIN_1 is not specified
#endif

// The GPIO pin that is connected to the button2 on the Board.
#define MOTION_PIN 5
#ifndef MOTION_PIN
#error MOTION_PIN is not specified
#endif

// The GPIO pin that is connected to the button2 on the Board.
#define MOTION_PIN_2 12
#ifndef MOTION_PIN_2
#error MOTION_PIN_2 is not specified
#endif

// The GPIO pin that is connected to the button2 on the Board.
#define MOTION_PIN_3 14
#ifndef MOTION_PIN_3
#error MOTION_PIN_3 is not specified
#endif

// The GPIO pin that is connected to the button2 on the Board.
#define MOTION_PIN_4 16
#ifndef MOTION_PIN_4
#error MOTION_PIN_4 is not specified
#endif

// The GPIO pin that is connected to the button2 on the Board.
#define CONTACT_PIN 13
#ifndef CONTACT_PIN
#error CONTACT_PIN is not specified
#endif

#define ALLOWED_FACTORY_RESET_TIME 60000

void lightbulb_on_1_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void lightbulb_on_2_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

homekit_characteristic_t occupancy_detected = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);
homekit_characteristic_t occupancy_detected_2 = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);
homekit_characteristic_t occupancy_detected_3 = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);
homekit_characteristic_t occupancy_detected_4 = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);
homekit_characteristic_t door_open_detected = HOMEKIT_CHARACTERISTIC_(CONTACT_SENSOR_STATE, 0);
homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "BS53CLOSET");
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

void relay_write_1(bool on) {
        gpio_write(relay_gpio_1, on ? 0 : 1);
}

void relay_write_2(bool on) {
        gpio_write(relay_gpio_2, on ? 0 : 1);
}

void reset_configuration_task() {
    printf("Resetting Wifi");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Restarting\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {
    if (xTaskGetTickCountFromISR() < ALLOWED_FACTORY_RESET_TIME / portTICK_PERIOD_MS) {
    xTaskCreate(reset_configuration_task, "Reset configuration", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
} else {
    printf("Factory reset not allowed after %ims since boot. Repower device and try again\n", ALLOWED_FACTORY_RESET_TIME);
}
}

homekit_characteristic_t lightbulb_on_1 = HOMEKIT_CHARACTERISTIC_(
        ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(lightbulb_on_1_callback)
        );

homekit_characteristic_t lightbulb_on_2 = HOMEKIT_CHARACTERISTIC_(
        ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(lightbulb_on_2_callback)
        );




void gpio_init() {
  gpio_enable(relay_gpio_1, GPIO_OUTPUT);
  relay_write_1(lightbulb_on_1.value.bool_value);

  gpio_enable(relay_gpio_2, GPIO_OUTPUT);
  relay_write_2(lightbulb_on_2.value.bool_value);
    gpio_enable(MOTION_PIN,  GPIO_INPUT);
    gpio_enable(MOTION_PIN_2,  GPIO_INPUT);
    gpio_enable(MOTION_PIN_3,  GPIO_INPUT);
    gpio_enable(MOTION_PIN_4,  GPIO_INPUT);
    gpio_enable(TOGGLE_PIN_1,  GPIO_INPUT);
    gpio_enable(CONTACT_PIN, GPIO_INPUT);
}

void lightbulb_on_1_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
        relay_write_1(lightbulb_on_1.value.bool_value);
}

void lightbulb_on_2_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
        relay_write_2(lightbulb_on_2.value.bool_value);
}

void toggle_callback_1(bool high, void *context) {
        printf("toggle is %s\n", high ? "high" : "low");
        lightbulb_on_1.value.bool_value = !lightbulb_on_1.value.bool_value;
        relay_write_1(lightbulb_on_1.value.bool_value);
        homekit_characteristic_notify(&lightbulb_on_1, lightbulb_on_1.value);
        reset_configuration();
}

//ContactSensor task
void sensor_callback(bool high, void *context) {
        door_open_detected.value = HOMEKIT_UINT8(high ? 1 : 0);
        homekit_characteristic_notify(&door_open_detected, door_open_detected.value);
}

//OCCUPANCY SENSOR SECTION

void motion_callback(bool high, void *context) {
    occupancy_detected.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected, occupancy_detected.value);
}

void motion_callback_2(bool high, void *context) {
    occupancy_detected_2.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected_2, occupancy_detected_2.value);
}

void motion_callback_3(bool high, void *context) {
    occupancy_detected_3.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected_3, occupancy_detected_3.value);
}

void motion_callback_4(bool high, void *context) {
    occupancy_detected_4.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected_4, occupancy_detected_4.value);
}

//ACCESSORIES IDENTIFY
void BS53CLOSET_identify(homekit_value_t _value) {
    printf("Multiple Acessory Identify\n");
}


//HOMEKIT ACCESSORIES SECTION
homekit_accessory_t *accessories[] = {
          HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_bridge, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
            NULL
        }),
        NULL
      }),
      HOMEKIT_ACCESSORY(.id=2, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
              HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                      HOMEKIT_CHARACTERISTIC(NAME, "Lampada 1"),
                      HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
                      NULL
              }),
              HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                      HOMEKIT_CHARACTERISTIC(NAME, "Lampada 1"),
                      &lightbulb_on_1,
                      &ota_trigger,
                      NULL
              }),
              NULL
      }),

      HOMEKIT_ACCESSORY(.id=3, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
              HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                      HOMEKIT_CHARACTERISTIC(NAME, "Lampada 2"),
                      HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
                      NULL
              }),
              HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                      HOMEKIT_CHARACTERISTIC(NAME, "Lampada 2"),
                      &lightbulb_on_2,
                      NULL
              }),
              NULL
      }),

      HOMEKIT_ACCESSORY(.id=4, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
          HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Closet"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
              NULL
          }),
          HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
              HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Closet"),
              &occupancy_detected,
              NULL
          }),
          NULL
      }),
      HOMEKIT_ACCESSORY(.id=5, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
          HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy ClosetS"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
              NULL
          }),
          HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
              HOMEKIT_CHARACTERISTIC(NAME, "Occupancy ClosetS"),
              &occupancy_detected_2,
              NULL
          }),
          NULL
      }),
      HOMEKIT_ACCESSORY(.id=6, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
          HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Banheiro"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
              NULL
          }),
          HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
              HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Banheiro"),
              &occupancy_detected_3,
              NULL
          }),
          NULL
      }),
      HOMEKIT_ACCESSORY(.id=7, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
          HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Chuveiro"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
              NULL
          }),
          HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
              HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Chuveiro"),
              &occupancy_detected_4,
              NULL
          }),
          NULL
      }),
      HOMEKIT_ACCESSORY(.id=8, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
              HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Porta Closet"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, BS53CLOSET_identify),
                      NULL
              }),
              HOMEKIT_SERVICE(CONTACT_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
                      HOMEKIT_CHARACTERISTIC(NAME, "Porta Closet"),
                      &door_open_detected,
                      NULL
              }),
              NULL
      }),
      NULL
  };

homekit_server_config_t config = {
        .accessories = accessories,
        .password = "736-24-212",
        .setupId="7EN2", //ci=8
};

void on_wifi_ready() {
}


void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "BS53CLOSET-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "BS53CLOSET-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);

    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
}


void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();
    gpio_init();


    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;

    homekit_server_init(&config);

    button_config_t config = BUTTON_CONFIG(
            button_active_low,
            .long_press_time = 3000,
            .max_repeat_presses = 3,
            );


    if (toggle_create(TOGGLE_PIN_1, toggle_callback_1, NULL)) {
            printf("Failed to initialize toggle 1 \n");
    }

    if (toggle_create(CONTACT_PIN, sensor_callback, NULL)) {
            printf("Failed to initialize sensor\n");
    }

    if (toggle_create(MOTION_PIN, motion_callback, NULL)) {
            printf("Failed to initialize sensor\n");
    }
    if (toggle_create(MOTION_PIN_2, motion_callback_2, NULL)) {
            printf("Failed to initialize sensor\n");
    }
    if (toggle_create(MOTION_PIN_3, motion_callback_3, NULL)) {
            printf("Failed to initialize sensor\n");
    }
    if (toggle_create(MOTION_PIN_4, motion_callback_4, NULL)) {
            printf("Failed to initialize sensor\n");
    }
}
