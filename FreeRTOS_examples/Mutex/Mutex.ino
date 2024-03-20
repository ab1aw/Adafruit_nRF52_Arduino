/*
   Example of a FreeRTOS mutex
   https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html
*/

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial


// Include mutex support
#include <semphr.h>

/*
   Declaring a global variable of type SemaphoreHandle_t

*/
SemaphoreHandle_t mutex;

int globalCount = 0;

void setup()
{
    Serial.begin (115200);

    // Wait for a serial port connection to be established before continuing.
    // Don't want to miss any debug messages.
    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STARTING THE APPLICATION.");
    /**
        Create a mutex.
        https://www.freertos.org/CreateMutex.html
    */
    mutex = xSemaphoreCreateMutex();

    if (mutex != NULL) {
        Serial.println ("Mutex created");
    }

    else {
        Serial.println ("ERROR! Mutex creation failed!");
    }

    /**
        Create tasks.
        Relative task periodicity is offset by 200 mSec.
    */
    xTaskCreate (TaskMutex, // Task function
                 "Task1", // Task name for humans
                 128,
                 (void *) 900, // Task parameter
                 1, // Task priority
                 NULL);
    xTaskCreate (TaskMutex, "Task2", 128, (void *) 1100, 1, NULL);
}

void loop()
{
    Serial.println ("...looping...");
    delay (1000);
}

void TaskMutex (void *pvParameters)
{
    TickType_t delayTime = ( (TickType_t) pvParameters); // Use task parameters to define delay
    Serial.print ("Starting task ");
    Serial.print (pcTaskGetName (NULL) ); // Get task name
    Serial.print (" with delay ");
    Serial.println (delayTime);
    delay (1000);

    for (;;) {
        /**
        Take mutex
        https://www.freertos.org/a00122.html
        */
        if (xSemaphoreTake (mutex, 10) == pdTRUE) {
            Serial.print (pcTaskGetName (NULL) ); // Get task name
            Serial.print (", Count read value: ");
            Serial.print (globalCount);
            globalCount++;
            Serial.print (", Updated value: ");
            Serial.print (globalCount);
            Serial.println();
            /**
            Give mutex
            https://www.freertos.org/a00123.html
            */
            xSemaphoreGive (mutex);
        }

        // Add '1' to prevent divide by zero compilation error.
        vTaskDelay (delayTime / (1 + portTICK_PERIOD_MS) );
    }
}
