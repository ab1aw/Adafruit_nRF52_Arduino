/*
 * Example of a Arduino interruption and RTOS Binary Semaphore
 * https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html
 */


#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

// Include semaphore supoport
#include <semphr.h>

/*
 * Declaring a global variable of type SemaphoreHandle_t
 *
 */
SemaphoreHandle_t interruptSemaphore;

void setup()
{
    Serial.begin (115200);

    // Wait for a serial port connection to be established before continuing.
    // Don't want to miss any debug messages.
    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STARTING THE APPLICATION.");
    // Configure pin 4 as an input and enable the internal pull-up resistor
    pinMode (4, INPUT_PULLUP);
// Create task for Arduino led
    xTaskCreate (TaskLed, // Task function
                 "Led", // Task name
                 128, // Stack size
                 NULL,
                 0, // Priority
                 NULL );
    /**
     * Create a binary semaphore.
     * https://www.freertos.org/xSemaphoreCreateBinary.html
     */
    interruptSemaphore = xSemaphoreCreateBinary();

    if (interruptSemaphore != NULL) {
        // Attach interrupt for Arduino digital pin
        attachInterrupt (digitalPinToInterrupt (4), interruptHandler, CHANGE);
    }
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


void interruptHandler()
{
    /**
     * Give semaphore in the interrupt handler
     * https://www.freertos.org/a00124.html
     */
    xSemaphoreGiveFromISR (interruptSemaphore, NULL);
}


/*
 * Led task.
 */
void TaskLed (void *pvParameters)
{
    (void) pvParameters;
    pinMode (LED_BUILTIN, OUTPUT);
    Serial.print ("Starting task ");
    Serial.println (pcTaskGetName (NULL) ); // Get task name
    delay (1000);

    for (;;) {
        /**
         * Take the semaphore.
         * https://www.freertos.org/a00122.html
         */
        if (xSemaphoreTake (interruptSemaphore, portMAX_DELAY) == pdPASS) {
            Serial.println ("Semaphore interrupt occurred.");
            digitalWrite (LED_BUILTIN, !digitalRead (LED_BUILTIN) );
        }

        vTaskDelay (10);
    }
}
