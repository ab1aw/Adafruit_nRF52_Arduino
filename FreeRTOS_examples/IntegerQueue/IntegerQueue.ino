/*
 * Example of a basic FreeRTOS queue
 * https://www.freertos.org/Embedded-RTOS-Queues.html
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

// Include queue support
#include <queue.h>

/*
 * Declaring a global variable of type QueueHandle_t
 *
 */
QueueHandle_t integerQueue;

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
     * Create a queue.
     * https://www.freertos.org/a00116.html
     */
    integerQueue = xQueueCreate (10, // Queue length
                                 sizeof (int) // Queue item size
                                );

    if (integerQueue != NULL) {
        // Create task that consumes the queue if it was created.
        xTaskCreate (TaskSerial, // Task function
                     "Serial", // A name just for humans
                     128,  // This stack size can be checked & adjusted by reading the Stack Highwater
                     NULL,
                     2, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                     NULL);
        // Create task that publish data in the queue if it was created.
        xTaskCreate (TaskAnalogRead, // Task function
                     "AnalogRead", // Task name
                     128,  // Stack size
                     NULL,
                     1, // Priority
                     NULL);
    }

    xTaskCreate (TaskBlink, // Task function
                 "Blink", // Task name
                 128, // Stack size
                 NULL,
                 0, // Priority
                 NULL );
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
 * Analog read task
 * Reads an analog input on pin 0 and send the readed value through the queue.
 * See Blink_AnalogRead example.
 */
void TaskAnalogRead (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name

    for (;;) {
        // Read the input on analog pin 0:
        int sensorValue = analogRead (A0);
        /**
         * Post an item on a queue.
         * https://www.freertos.org/a00117.html
         */
        xQueueSend (integerQueue, &sensorValue, portMAX_DELAY);
        // One tick delay (15ms) in between reads for stability
        vTaskDelay (1);
    }
}

/**
 * Serial task.
 * Prints the received items from the queue to the serial monitor.
 */
void TaskSerial (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    int valueFromQueue = 0;

    for (;;) {
        /**
         * Read an item from a queue.
         * https://www.freertos.org/a00118.html
         */
        if (xQueueReceive (integerQueue, &valueFromQueue, portMAX_DELAY) == pdPASS) {
            Serial.println (valueFromQueue);
        }
    }
}

/*
 * Blink task.
 * See Blink_AnalogRead example.
 */
void TaskBlink (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    pinMode (LED_BUILTIN, OUTPUT);

    for (;;) {
        digitalWrite (LED_BUILTIN, HIGH);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
        digitalWrite (LED_BUILTIN, LOW);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
    }
}
