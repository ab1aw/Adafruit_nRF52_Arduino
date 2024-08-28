#include <SoftwareSerial.h>


// Demo Code for SerialCommand Library
// Steven Cogswell
// May 2011

#include "SerialCommand.h"

#define arduinoLED LED_RED   // Arduino LED on board

SerialCommand sCmd;     // The demo SerialCommand object

static int flashRate = 500;

void setup() {
  pinMode(arduinoLED, OUTPUT);      // Configure the onboard LED for output
  digitalWrite(arduinoLED, LOW);    // default to LED off

  Serial.begin(115200);


    while ( !Serial ) {
        delay (50);    // for nrf52840 with native usb
    }

  
  // Create loop2() using Scheduler to run in 'parallel' with loop()
  Scheduler.startLoop(loop2);

  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("ON",    LED_on);          // Turns LED on
  sCmd.addCommand("OFF",   LED_off);         // Turns LED off
  sCmd.addCommand("HELLO", sayHello);        // Echos the string argument back
  sCmd.addCommand("m",     multiArgs);        // Echos the string argument back
  sCmd.addCommand("fr",     LED_change);        // Echos the string argument back
  sCmd.addCommand("P",     processCommand);  // Converts two arguments to integers and echos them back
  sCmd.setDefaultHandler(unrecognized);      // Handler for command that isn't matched  (says "What?")
  Serial.println("Ready");
}

void loop() {
  sCmd.readSerial();     // We don't do much, just process serial commands
}


/**
 * Toggle led1 every 0.5 second
 */
void loop2()
{
  digitalToggle(arduinoLED); // Toggle LED 
  delay(flashRate);              // wait for a half second  
}


void LED_on() {
  Serial.println("LED on");
  digitalWrite(arduinoLED, HIGH);
}

void LED_off() {
  Serial.println("LED off");
  digitalWrite(arduinoLED, LOW);
}


void LED_change() {
  char *arg;
  arg = sCmd.next();    // Get the next argument from the SerialCommand object buffer
  if (arg != NULL) {    // As long as it existed, take it
    Serial.print("New flash rate is ");
    Serial.println(arg);
    flashRate = atoi(arg);    // Converts a char string to an integer
  }
  else {
    Serial.println("ERROR!");
  }
}

void sayHello() {
  char *arg;
  arg = sCmd.next();    // Get the next argument from the SerialCommand object buffer
  if (arg != NULL) {    // As long as it existed, take it
    Serial.print("Hello ");
    Serial.println(arg);
  }
  else {
    Serial.println("Hello, whoever you are");
  }
}


void multiArgs() {
  char *arg;
  arg = sCmd.next();    // Get the next argument from the SerialCommand object buffer
  Serial.print("Hello");
  while (arg != NULL) {    // As long as it existed, take it
    Serial.print(" ");
    Serial.print(arg);
    arg = sCmd.next();    // Get the next argument from the SerialCommand object buffer
  }

  Serial.println("");
}



void processCommand() {
  int aNumber;
  char *arg;

  Serial.println("We're in processCommand");
  arg = sCmd.next();
  if (arg != NULL) {
    aNumber = atoi(arg);    // Converts a char string to an integer
    Serial.print("First argument was: ");
    Serial.println(aNumber);
  }
  else {
    Serial.println("No arguments");
  }

  arg = sCmd.next();
  if (arg != NULL) {
    aNumber = atol(arg);
    Serial.print("Second argument was: ");
    Serial.println(aNumber);
  }
  else {
    Serial.println("No second argument");
  }
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *command) {
  Serial.println("What?");
}
