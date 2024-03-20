/*
 * Example of FreeRTOS configASSERT macro
 * https://www.freertos.org/a00110.html#configASSERT
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

const boolean valueToAssert = true;

// The setup function runs once when you press reset or power the board
void setup()
{
    Serial.begin (115200);

    // Wait for a serial port connection to be established before continuing.
    // Don't want to miss any debug messages.
    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STARTING THE APPLICATION.");
    // Assert value is true, execution doesn't stop.
    configASSERT (valueToAssert == true);
    // Assert value is false, FreeRTOS execution stops and start to blink main led two times with 4 second cycle.
    configASSERT (valueToAssert == false);
}


void loop()
{
    // Empty. Things are done in Tasks.
}
