/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <bluefruit.h>

/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Heart Rate Measurement Char: 0x2A37
 * Body Sensor Location Char:   0x2A38
 */

BLEService customService("59668690-8d7d-11eb-8dcd-0242ac130003");


BLECharacteristic CharacteristicX("59668691-8d7d-11eb-8dcd-0242ac130003", BLERead | BLENotify);
BLECharacteristic CharacteristicY("59668692-8d7d-11eb-8dcd-0242ac130003", BLERead | BLENotify);
BLECharacteristic CharacteristicZ("59668693-8d7d-11eb-8dcd-0242ac130003", BLERead | BLENotify);
BLECharacteristic CharacteristicW("59668694-8d7d-11eb-8dcd-0242ac130003", BLERead | BLENotify);

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

uint8_t  bps = 72;


void setup()
{
    Serial.begin (115200);

    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    /* Intializes random number generator */
    /*Set random number generator*/
    uint32_t r = millis();
    randomSeed (r);
    Serial.println ("Bluefruit52 HRM Example");
    Serial.println ("-----------------------\n");
    // Initialise the Bluefruit module
    Serial.println ("Initialise the Bluefruit nRF52 module");
    Bluefruit.begin();
    Bluefruit.setName ("ItsyBitsyHrm");   // Check bluefruit.h for supported values
    // Set the connect/disconnect callback handlers
    Bluefruit.Periph.setConnectCallback (connect_callback);
    Bluefruit.Periph.setDisconnectCallback (disconnect_callback);
    // Configure and Start the Device Information Service
    Serial.println ("Configuring the Device Information Service");
    bledis.setManufacturer ("Adafruit Industries");
    bledis.setModel ("Bluefruit Feather52");
    bledis.begin();
    // Start the BLE Battery Service and set it to 100%
    Serial.println ("Configuring the Battery Service");
    blebas.begin();
    blebas.write (100);
    // Setup the Heart Rate Monitor service using
    // BLEService and BLECharacteristic classes
    Serial.println ("Configuring the Heart Rate Monitor Service");
    setupHRM();
    // Setup the advertising packet(s)
    Serial.println ("Setting up the advertising payload(s)");
    startAdv();

    const char *body_str[] = { "Other", "Chest", "Wrist", "Finger", "Hand", "Ear Lobe", "Foot" };

    Serial.printf ("Advertising HRM body sensor location: %s.\n", body_str[CharacteristicW.read8()]);
}

void startAdv (void)
{
    // Advertising packet
    Bluefruit.Advertising.addFlags (BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    // Include HRM Service UUID
    Bluefruit.Advertising.addService (customService);
    // Include Name
    Bluefruit.Advertising.addName();
    /* Start Advertising
     * - Enable auto advertising if disconnected
     * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     * - Timeout for fast mode is 30 seconds
     * - Start(timeout) with timeout = 0 will advertise forever (until connected)
     *
     * For recommended advertising interval
     * https://developer.apple.com/library/content/qa/qa1931/_index.html
     */
    Bluefruit.Advertising.restartOnDisconnect (true);
    Bluefruit.Advertising.setInterval (32, 244);   // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout (30);     // number of seconds in fast mode
    Bluefruit.Advertising.start (0);               // 0 = Don't stop advertising after n seconds
}

void setupHRM (void)
{
    // Configure the Heart Rate Monitor service
    // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.heart_rate.xml
    // Supported Characteristics:
    // Name                         UUID    Requirement Properties
    // ---------------------------- ------  ----------- ----------
    // Heart Rate Measurement       0x2A37  Mandatory   Notify
    // Body Sensor Location         0x2A38  Optional    Read
    // Heart Rate Control Point     0x2A39  Conditional Write       <-- Not used here
    customService.begin();
    // Note: You must call .begin() on the BLEService before calling .begin() on
    // any characteristic(s) within that service definition.. Calling .begin() on
    // a BLECharacteristic will cause it to be added to the last BLEService that
    // was 'begin()'ed!
    // Configure the Heart Rate Measurement characteristic
    // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.heart_rate_measurement.xml
    // Properties = Notify
    // Min Len    = 1
    // Max Len    = 8
    //    B0      = UINT8  - Flag (MANDATORY)
    //      b5:7  = Reserved
    //      b4    = RR-Internal (0 = Not present, 1 = Present)
    //      b3    = Energy expended status (0 = Not present, 1 = Present)
    //      b1:2  = Sensor contact status (0+1 = Not supported, 2 = Supported but contact not detected, 3 = Supported and detected)
    //      b0    = Value format (0 = UINT8, 1 = UINT16)
    //    B1      = UINT8  - 8-bit heart rate measurement value in BPM
    //    B2:3    = UINT16 - 16-bit heart rate measurement value in BPM
    //    B4:5    = UINT16 - Energy expended in joules
    //    B6:7    = UINT16 - RR Internal (1/1024 second resolution)
    CharacteristicX.setProperties (CHR_PROPS_NOTIFY);
    CharacteristicX.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    CharacteristicX.setFixedLen (2);
    CharacteristicX.setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
    CharacteristicX.setUserDescriptor ("HRM User Descriptor: CharacteristicX"); // aka user descriptor
    CharacteristicX.begin();
    uint8_t hrmdataX[2] = { 0b00000110, 0x40 }; // Set the characteristic to use 8-bit values, with the sensor connected and detected
    CharacteristicX.write (hrmdataX, 2);

    CharacteristicY.setProperties (CHR_PROPS_NOTIFY);
    CharacteristicY.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    CharacteristicY.setFixedLen (2);
    CharacteristicY.setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
    CharacteristicY.setUserDescriptor ("HRM User Descriptor: CharacteristicY"); // aka user descriptor
    CharacteristicY.begin();
    uint8_t hrmdataY[2] = { 0b00000110, 0x40 }; // Set the characteristic to use 8-bit values, with the sensor connected and detected
    CharacteristicY.write (hrmdataY, 2);

    CharacteristicZ.setProperties (CHR_PROPS_NOTIFY);
    CharacteristicZ.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    CharacteristicZ.setFixedLen (2);
    CharacteristicZ.setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
    CharacteristicZ.setUserDescriptor ("HRM User Descriptor: CharacteristicZ"); // aka user descriptor
    CharacteristicZ.begin();
    uint8_t hrmdataZ[2] = { 0b00000110, 0x40 }; // Set the characteristic to use 8-bit values, with the sensor connected and detected
    CharacteristicZ.write (hrmdataZ, 2);

    // Configure the Body Sensor Location characteristic
    // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.body_sensor_location.xml
    // Properties = Read
    // Min Len    = 1
    // Max Len    = 1
    //    B0      = UINT8 - Body Sensor Location
    //      0     = Other
    //      1     = Chest
    //      2     = Wrist
    //      3     = Finger
    //      4     = Hand
    //      5     = Ear Lobe
    //      6     = Foot
    //      7:255 = Reserved

    // Randomly choose one of the possible locations.
    // Loop on the random number generator for a bit to "prime the pump."
    int count_down = 10;
    while ( count_down-- ) {
        (void)random (1, 7);
    }
    uint8_t location = (uint8_t) (random (1, 7) );  // Results in range [1..6].

    CharacteristicW.setProperties (CHR_PROPS_READ);
    CharacteristicW.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    CharacteristicW.setFixedLen (1);
    CharacteristicW.setUserDescriptor ("HRM User Descriptor: CharacteristicW"); // aka user descriptor
    CharacteristicW.begin();

    // Set the characteristic to one of the possible body sensor location values.
    CharacteristicW.write8 (location);
}

void connect_callback (uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection (conn_handle);
    char central_name[32] = { 0 };
    connection->getPeerName (central_name, sizeof (central_name) );
    Serial.print ("Connected to ");
    Serial.println (central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback (uint16_t conn_handle, uint8_t reason)
{
    (void) conn_handle;
    (void) reason;
    Serial.print ("Disconnected, reason = 0x");
    Serial.println (reason, HEX);
    Serial.println ("Advertising!");
}

void cccd_callback (uint16_t conn_hdl, BLECharacteristic *chr, uint16_t cccd_value)
{
    // Display the raw request packet
    Serial.print ("CCCD Updated: ");
    //Serial.printBuffer(request->data, request->len);
    Serial.print (cccd_value);
    Serial.println ("");

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if (chr->uuid == CharacteristicX.uuid) {
        if (chr->notifyEnabled (conn_hdl) ) {
            Serial.println ("CharacteristicX 'Notify' enabled");
        }

        else {
            Serial.println ("CharacteristicX 'Notify' disabled");
        }
    }

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if (chr->uuid == CharacteristicY.uuid) {
        if (chr->notifyEnabled (conn_hdl) ) {
            Serial.println ("CharacteristicY 'Notify' enabled");
        }

        else {
            Serial.println ("CharacteristicY 'Notify' disabled");
        }
    }

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if (chr->uuid == CharacteristicZ.uuid) {
        if (chr->notifyEnabled (conn_hdl) ) {
            Serial.println ("CharacteristicZ 'Notify' enabled");
        }

        else {
            Serial.println ("CharacteristicZ 'Notify' disabled");
        }
    }
}

void loop()
{
    digitalToggle (LED_RED);

    if ( Bluefruit.connected() ) {
        // Sensor connected, modify BPS value based on body sensor location.
        // BPS value is modified in order to generate different value ranges
        // based on the sensot location. This allow us to process different
        // ranges on the proxy peripheral aggregator.
        uint8_t hrmdataX[2] = { 0b00000110, bps };
        bps = (uint8_t) (68 + CharacteristicW.read8() +  random (-3, 4) );  // Results in range of +/-3.

        uint8_t hrmdataY[2] = { 0b00000110, bps };
        bps = (uint8_t) (68 + CharacteristicW.read8() +  random (-3, 4) );  // Results in range of +/-3.

        uint8_t hrmdataZ[2] = { 0b00000110, bps };
        bps = (uint8_t) (68 + CharacteristicW.read8() +  random (-3, 4) );  // Results in range of +/-3.

        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( CharacteristicX.notify (hrmdataX, sizeof (hrmdataX) ) ) {
            Serial.print ("CharacteristicX updated to: ");
            Serial.println (bps);
        }

        else {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
        }

        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( CharacteristicY.notify (hrmdataY, sizeof (hrmdataY) ) ) {
            Serial.print ("CharacteristicX updated to: ");
            Serial.println (bps);
        }

        else {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
        }

        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( CharacteristicZ.notify (hrmdataZ, sizeof (hrmdataZ) ) ) {
            Serial.print ("CharacteristicX updated to: ");
            Serial.println (bps);
        }

        else {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
        }
    }

    // Only send update once per second
    delay (1000);
}
