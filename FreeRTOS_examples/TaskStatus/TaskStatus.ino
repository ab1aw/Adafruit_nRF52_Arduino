#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

//define task handles
TaskHandle_t TaskBlink_Handler;
TaskHandle_t TaskSerial_Handler;

// define two tasks for Blink & Serial
void TaskBlink ( void *pvParameters );
void TaskSerial (void *pvParameters);

// the setup function runs once when you press reset or power the board
void setup()
{
    Serial.begin (115200);

    // Wait for a serial port connection to be established before continuing.
    // Don't want to miss any debug messages.
    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STARTING THE APPLICATION.");
    // Now set up two tasks to run independently.
    xTaskCreate (
        TaskBlink
        ,  "Blink"   // A name just for humans
        ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
        ,  NULL //Parameters passed to the task function
        ,  2  // Priority, with 2 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
        ,  &TaskBlink_Handler );//Task handle
    xTaskCreate (
        TaskSerial
        ,  "Serial"
        ,  128  // Stack size
        ,  NULL //Parameters passed to the task function
        ,  1  // Priority
        ,  &TaskSerial_Handler );  //Task handle
}


void loop()
{
    // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskSerial (void *pvParameters)
{
    /*
     Serial
     Send "s" or "r" through the serial port to control the suspend and resume of the LED light task.
     This example code is in the public domain.
    */
    (void) pvParameters;

    for (;;) { // A Task shall never return or exit.
        while (Serial.available() > 0) {
            switch (Serial.read() ) {
                case 's':
                    vTaskSuspend (TaskBlink_Handler);
                    Serial.println ("Suspend!");
                    break;

                case 'r':
                    vTaskResume (TaskBlink_Handler);
                    Serial.println ("Resume!");
                    break;
            }

            vTaskDelay (1);
        }
    }
}

void TaskBlink (void *pvParameters) // This is a task.
{
    (void) pvParameters;
    pinMode (LED_BUILTIN, OUTPUT);

    for (;;) { // A Task shall never return or exit.
        Serial.println ("LED on.");
        digitalWrite (LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
        vTaskDelay ( 1000 / (1 + portTICK_PERIOD_MS) ); // wait for one second
        Serial.println ("LED off.");
        digitalWrite (LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
        vTaskDelay ( 1000 / (1 + portTICK_PERIOD_MS) ); // wait for one second
    }
}
