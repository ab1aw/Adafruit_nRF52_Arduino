/*
 * Example of a basic FreeRTOS queue
 * https://www.freertos.org/Embedded-RTOS-Queues.html
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial

// Include queue support
#include <queue.h>

// Define a Array
int pinReadArray[4] = {0, 0, 0, 0};

//Function Declaration
void TaskBlink (void *pvParameters);
void TaskAnalogReadPin0 (void *pvParameters);
void TaskAnalogReadPin1 (void *pvParameters);
void TaskSerial (void *pvParameters);

/*
 * Declaring a global variable of type QueueHandle_t
 *
 */
QueueHandle_t arrayQueue;

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
    arrayQueue = xQueueCreate (10, //Queue length
                               sizeof (int) ); //Queue item size

    if (arrayQueue != NULL) {
        // Create task that consumes the queue if it was created.
        xTaskCreate (TaskSerial, // Task function
                     "PrintSerial",// Task name
                     128,// Stack size
                     NULL,
                     2,// Priority
                     NULL);
        // Create task that publish data in the queue if it was created.
        xTaskCreate (TaskAnalogReadPin0, // Task function
                     "AnalogRead1",// Task name
                     128,// Stack size
                     NULL,
                     1,// Priority
                     NULL);
        // Create other task that publish data in the queue if it was created.
        xTaskCreate (TaskAnalogReadPin1, // Task function
                     "AnalogRead2",// Task name
                     128,// Stack size
                     NULL,
                     1,// Priority
                     NULL);
        xTaskCreate (TaskBlink, // Task function
                     "Blink", // Task name
                     128,// Stack size
                     NULL,
                     0,// Priority
                     NULL);
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
        pinReadArray[0] = 0;
        pinReadArray[1] = analogRead (A0);
        /**
           * Post an item on a queue.
           * https://www.freertos.org/a00117.html
           */
        xQueueSend (arrayQueue, &pinReadArray, portMAX_DELAY);
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
        pinReadArray[2] = 1;
        pinReadArray[3] = analogRead (A1);
        /**
           * Post an item on a queue.
           * https://www.freertos.org/a00117.html
           */
        xQueueSend (arrayQueue, &pinReadArray, portMAX_DELAY);
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
        if (xQueueReceive (arrayQueue, &pinReadArray, portMAX_DELAY) == pdPASS ) {
            Serial.print ("PIN:");
            Serial.println (pinReadArray[0]);
            Serial.print ("value:");
            Serial.println (pinReadArray[1]);
            Serial.print ("PIN:");
            Serial.println (pinReadArray[2]);
            Serial.print ("value:");
            Serial.println (pinReadArray[3]);
            vTaskDelay (500 / (1 + portTICK_PERIOD_MS) );
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
    digitalWrite (LED_BUILTIN, LOW);

    for (;;) {
        digitalWrite (LED_BUILTIN, HIGH);
        vTaskDelay (250 / (1 + portTICK_PERIOD_MS) );
        digitalWrite (LED_BUILTIN, LOW);
        vTaskDelay (250 / (1 + portTICK_PERIOD_MS) );
    }
}
