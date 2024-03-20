/**
   Example of a Arduino interruption and RTOS Task Notification.
   https://www.freertos.org/RTOS_Task_Notification_As_Binary_Semaphore.html
*/

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

/**
   Declaring a global TaskHandle for the led task.
*/
TaskHandle_t taskNotificationHandler;

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
    // Create task for FreeRTOS notification
    xTaskCreate (TaskNotification, // Task function
                 "Notification", // Task name
                 128, // Stack size
                 NULL,
                 3, // Priority
                 &taskNotificationHandler ); // TaskHandle
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

/*
   Notification task.
*/
void TaskNotification (void *pvParameters)
{
    (void) pvParameters;
    int digitalPin = 4;
    int rv = attachInterrupt (digitalPinToInterrupt (digitalPin), digitalPinInterruptHandler, CHANGE);
    Serial.print ("Starting task ");
    Serial.print (pcTaskGetName (NULL) ); // Get task name
    Serial.print (" with rv =  ");
    Serial.println (rv);
    delay (1000);

    for (;;) {
        if (ulTaskNotifyTake (pdTRUE, portMAX_DELAY) ) {
            Serial.println ("Notification received");
        }
    }
}


void digitalPinInterruptHandler()
{
    Serial.println ("digitalPinInterruptHandler()");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR (taskNotificationHandler, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        Serial.println ("Calling taskYIELD()");
        taskYIELD();
    }
}
