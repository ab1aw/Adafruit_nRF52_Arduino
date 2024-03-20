/*
 * Example of a basic FreeRTOS queue
 * https://www.freertos.org/Embedded-RTOS-Queues.html
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

// Include queue support
#include <queue.h>

// Define a struct
struct pinRead {
    int pin;
    int value;
};

/*
 * Declaring a global variable of type QueueHandle_t
 *
 */
QueueHandle_t structQueue;

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
    structQueue = xQueueCreate (10, // Queue length
                                sizeof (struct pinRead) // Queue item size
                               );

    if (structQueue != NULL) {
        // Create task that consumes the queue if it was created.
        xTaskCreate (TaskSerial, // Task function
                     "Serial", // A name just for humans
                     128,  // This stack size can be checked & adjusted by reading the Stack Highwater
                     NULL,
                     2, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                     NULL);
        // Create task that publish data in the queue if it was created.
        xTaskCreate (TaskAnalogReadPin0, // Task function
                     "AnalogReadPin0", // Task name
                     128,  // Stack size
                     NULL,
                     1, // Priority
                     NULL);
        // Create other task that publish data in the queue if it was created.
        xTaskCreate (TaskAnalogReadPin1, // Task function
                     "AnalogReadPin1", // Task name
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
 * Analog read task for Pin A0
 * Reads an analog input on pin 0 and send the readed value through the queue.
 * See Blink_AnalogRead example.
 */
void TaskAnalogReadPin0 (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name

    for (;;) {
        // Read the input on analog pin 0:
        struct pinRead currentPinRead;
        currentPinRead.pin = 0;
        currentPinRead.value = analogRead (A0);
        /**
         * Post an item on a queue.
         * https://www.freertos.org/a00117.html
         */
        xQueueSend (structQueue, &currentPinRead, portMAX_DELAY);
        // One tick delay (15ms) in between reads for stability
        vTaskDelay (1);
    }
}


/**
 * Analog read task for Pin A1
 * Reads an analog input on pin 1 and send the readed value through the queue.
 * See Blink_AnalogRead example.
 */
void TaskAnalogReadPin1 (void *pvParameters)
{
    (void) pvParameters;
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name

    for (;;) {
        // Read the input on analog pin 1:
        struct pinRead currentPinRead;
        currentPinRead.pin = 1;
        currentPinRead.value = analogRead (A1);
        /**
         * Post an item on a queue.
         * https://www.freertos.org/a00117.html
         */
        xQueueSend (structQueue, &currentPinRead, portMAX_DELAY);
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

    for (;;) {
        struct pinRead currentPinRead;

        /**
         * Read an item from a queue.
         * https://www.freertos.org/a00118.html
         */
        if (xQueueReceive (structQueue, &currentPinRead, portMAX_DELAY) == pdPASS) {
            Serial.print ("Pin: ");
            Serial.print (currentPinRead.pin);
            Serial.print (" Value: ");
            Serial.println (currentPinRead.value);
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
    pinMode (LED_BUILTIN, OUTPUT);
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name

    for (;;) {
        digitalWrite (LED_BUILTIN, HIGH);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
        digitalWrite (LED_BUILTIN, LOW);
        vTaskDelay ( 250 / (1 + portTICK_PERIOD_MS) );
    }
}
