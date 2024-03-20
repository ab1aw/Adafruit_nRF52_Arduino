/*
 * Example of FreeRTOS task utilities
 * https://www.freertos.org/a00021.html
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

/**
 * Task handlers
 * https://www.freertos.org/a00019.html#xTaskHandle
 */
TaskHandle_t taskBlinkHandle;

TaskHandle_t taskDeletedHandle;

TaskHandle_t taskBlockedHandle;

void setup()
{
    Serial.begin (115200);

    // Wait for a serial port connection to be established before continuing.
    // Don't want to miss any debug messages.
    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STARTING THE APPLICATION.");
    // Configure pin 4 as an input and enable the internal pull-up resistor.
    pinMode (4, INPUT_PULLUP);
    /**
     * Task creation
     */
    xTaskCreate (TaskBlink, // Task function
                 "Blink", // Task name
                 128, // Stack size
                 NULL,
                 0, // Priority
                 &taskBlinkHandle); // Task handler
    xTaskCreate (TaskSerial,
                 "Serial",
                 128,
                 NULL,
                 2,
                 NULL);
    xTaskCreate (TaskDeleted,
                 "Deleted",
                 64,
                 NULL,
                 1,
                 &taskDeletedHandle);
    xTaskCreate (TaskBlocked,
                 "Blocked",
                 64,
                 NULL,
                 1,
                 &taskBlockedHandle);
}

void loop()
{
    static bool firstTime = true;
    static int previousDigitalReadValue = -1;

    if ( firstTime ) {
        Serial.println ("Starting loop....");
        delay (1000);
        firstTime = false;
    }

    int digitalReadValue = digitalRead (4);

    if (digitalReadValue != previousDigitalReadValue) {
        if (digitalReadValue == HIGH) {
            Serial.println ("HIGH");
        }

        else {
            Serial.println ("LOW");
        }

        previousDigitalReadValue = digitalReadValue;
    }

    delay (1000);
}

/**
 * Example of utilities usage
 */
void TaskSerial (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    delay (1000);

    for (;;) {
        Serial.println ("======== Tasks status ========");
        Serial.print ("Tick count: ");
        Serial.print (xTaskGetTickCount() );
        Serial.print (", Task count: ");
        Serial.print (uxTaskGetNumberOfTasks() );
        Serial.println();
        Serial.println();
        // Serial task status
        Serial.print ("- TASK ");
        Serial.print (pcTaskGetName (NULL) ); // Get task name without handler https://www.freertos.org/a00021.html#pcTaskGetName
        Serial.print (", High Watermark: ");
        Serial.print (uxTaskGetStackHighWaterMark (NULL) ); // https://www.freertos.org/uxTaskGetStackHighWaterMark.html
        TaskHandle_t taskSerialHandle = xTaskGetCurrentTaskHandle(); // Get current task handle. https://www.freertos.org/a00021.html#xTaskGetCurrentTaskHandle
        Serial.println();
        Serial.print ("- TASK ");
        Serial.print (pcTaskGetName (taskBlinkHandle) ); // Get task name with handler
        Serial.print (", High Watermark: ");
        Serial.print (uxTaskGetStackHighWaterMark (taskBlinkHandle) );
        Serial.println();
        Serial.print ("- TASK ");
        Serial.print (pcTaskGetName (taskDeletedHandle) );
        Serial.print (", High Watermark: ");
        Serial.print (uxTaskGetStackHighWaterMark (taskDeletedHandle) );
        Serial.println();
        Serial.print ("- TASK ");
        Serial.print (pcTaskGetName (taskBlockedHandle) );
        Serial.print (", High Watermark: ");
        Serial.print (uxTaskGetStackHighWaterMark (taskBlockedHandle) );
        Serial.println();
        Serial.println();
        vTaskDelay ( 5000 / (1 + portTICK_PERIOD_MS) );
    }
}

/**
 * Blocked tasks when run
 */
void TaskBlocked (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    delay (1000);

    for (;;) {
        vTaskDelay ( 900000 / (1 + portTICK_PERIOD_MS) );
    }
}

/**
 * Deleted tasks when run
 */
void TaskDeleted (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    delay (1000);
    vTaskDelete (NULL);
}

/*
 * Blink task.
 * See Blink_AnalogRead example.
 */
void TaskBlink (void *pvParameters)
{
    (void) pvParameters;
    pinMode (LED_BUILTIN, OUTPUT);
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    delay (1000);

    for (;;) {
        digitalWrite (LED_BUILTIN, HIGH);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
        digitalWrite (LED_BUILTIN, LOW);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
    }
}
