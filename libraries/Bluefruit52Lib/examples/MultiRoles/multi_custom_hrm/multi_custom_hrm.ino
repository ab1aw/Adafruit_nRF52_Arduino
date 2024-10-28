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
BLEService        hrms = BLEService (UUID16_SVC_HEART_RATE);

BLECharacteristic *hrmc1 = NULL;
BLECharacteristic *bslc1 = NULL;

BLECharacteristic *hrmc2 = NULL;
BLECharacteristic *bslc2 = NULL;

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

// Initial value: heart rate beats per second.
uint8_t  bps = 72;

static char *names[4] = {"George", "Martha", "Fred", "Ethel"};

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
    Serial.println ("Bluefruit52 Multi Custom HRM Example");
    Serial.println ("-----------------------\n");
    // Initialise the Bluefruit module
    Serial.println ("Initialise the Bluefruit nRF52 module");
    Bluefruit.configServiceChanged(true);
    Bluefruit.begin (2, 0);
    Bluefruit.setName ("MultiCustomHrm");   // Check bluefruit.h for supported values
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

    Bluefruit.configServiceChanged(true);

    // Setup the Heart Rate Monitor service using
    // BLEService and BLECharacteristic classes
    Serial.println ("Configuring the Heart Rate Monitor Service #1");
    hrmc1 = new BLECharacteristic (UUID16_CHR_HEART_RATE_MEASUREMENT);
    bslc1 = new BLECharacteristic (UUID16_CHR_BODY_SENSOR_LOCATION);
    setupHRM (hrmc1, bslc1, (char *) "Unit 1", 2);
#if 1
    Serial.println ("Configuring the Heart Rate Monitor Service #2");
    hrmc2 = new BLECharacteristic (UUID16_CHR_HEART_RATE_MEASUREMENT);
    bslc2 = new BLECharacteristic (UUID16_CHR_BODY_SENSOR_LOCATION);
    setupHRM (hrmc2, bslc2, (char *) "Unit 2", 3);
#endif

    // Setup the advertising packet(s)
    Serial.println ("Setting up the advertising payload(s)");
    startAdv();
    Serial.println ("Ready Player One!!!");
    Serial.println ("\nAdvertising");
}

void startAdv (void)
{
    // Advertising packet
    Bluefruit.Advertising.addFlags (BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    // Include HRM Service UUID
    Bluefruit.Advertising.addService (hrms);
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

void setupHRM (BLECharacteristic *hrmc, BLECharacteristic *bslc, char *name, uint8_t location)
{
    // Configure the Heart Rate Monitor service
    // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.heart_rate.xml
    // Supported Characteristics:
    // Name                         UUID    Requirement Properties
    // ---------------------------- ------  ----------- ----------
    // Heart Rate Measurement       0x2A37  Mandatory   Notify
    // Body Sensor Location         0x2A38  Optional    Read
    // Heart Rate Control Point     0x2A39  Conditional Write       <-- Not used here
    hrms.begin();
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
    hrmc->setProperties (CHR_PROPS_NOTIFY);
    hrmc->setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    hrmc->setFixedLen (2);
    hrmc->setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
    hrmc->setUserDescriptor (name); // aka user descriptor
    hrmc->begin();
    uint8_t hrmdata[2] = { 0b00000110, 0x40 }; // Set the characteristic to use 8-bit values, with the sensor connected and detected
    hrmc->write (hrmdata, 2);
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
    bslc->setProperties (CHR_PROPS_READ);
    bslc->setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    bslc->setFixedLen (1);
    bslc->setUserDescriptor (name); // aka user descriptor
    bslc->begin();
    bslc->write8 (location);
}

void connect_callback (uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection (conn_handle);

    char central_name[32] = { 0 };
    ble_gap_addr_t peerAddr = connection->getPeerAddr();

    connection->getPeerName (central_name, sizeof (central_name) );
    Serial.printf ("Connected to %s %02X:%02X:%02X:%02X:%02X:%02X\n", central_name, peerAddr.addr[5], peerAddr.addr[4], peerAddr.addr[3], peerAddr.addr[2], peerAddr.addr[1], peerAddr.addr[0]);
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
    uint16_t _uuid;

#if 1
    // Display the raw request packet
    Serial.printf ("CCCD Updated: connection handle: 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, %s, %s, \n",
        conn_hdl,
        cccd_value,
        chr->getCccd(conn_hdl),
        hrmc1->getCccd(conn_hdl),
        hrmc2->getCccd(conn_hdl),
        (((chr == hrmc1) ? "hrmc1" : "hrmc2")),
        chr->uuid.toString().c_str()
        );
#else
    Serial.print ("CCCD Updated: connection handle: ");
    //Serial.printBuffer(request->data, request->len);
    Serial.print (conn_hdl);
    Serial.print (", CCCD values: ");
    Serial.print (cccd_value);
    Serial.print (" : ");
    Serial.print (chr->getCccd(conn_hdl));
    Serial.print (" : ");
    Serial.print (hrmc1->getCccd(conn_hdl));
    Serial.print (" : ");
    Serial.print (hrmc2->getCccd(conn_hdl));
    Serial.print (" : ");
    Serial.print (((chr == hrmc1) ? "TRUE" : "FALSE"));
    Serial.print (" : ");
    Serial.print (((chr == hrmc2) ? "TRUE" : "FALSE"));
    Serial.print (", UUID value: ");
    (void) chr->uuid.get (&_uuid);
    Serial.println (_uuid);
#endif

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if ( (hrmc1 != NULL) && (chr->uuid == hrmc1->uuid) && (cccd_value == hrmc1->getCccd(conn_hdl)) ) {
        if (hrmc1->notifyEnabled (conn_hdl) ) {
            Serial.println ("Heart Rate Measurement #1 'Notify' enabled");
        }

        else {
            Serial.println ("Heart Rate Measurement #1 'Notify' disabled");
        }
    }

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if ( (hrmc2 != NULL) && (chr->uuid == hrmc2->uuid) && (cccd_value == hrmc2->getCccd(conn_hdl)) ) {
        if (hrmc2->notifyEnabled (conn_hdl) ) {
            Serial.println ("Heart Rate Measurement #2 'Notify' enabled");
        }

        else {
            Serial.println ("Heart Rate Measurement #2 'Notify' disabled");
        }
    }
}

void loop()
{
    static int count = 30;

    static bool hrm1_not_connected_announced = false;
    static bool hrm2_not_connected_announced = false;

    digitalToggle (LED_RED);

    if ( Bluefruit.connected() ) {
        if ( hrmc1 != NULL ) {
            uint8_t hrmdata[2] = { 0b00000110, bps };           // Sensor connected, modify BPS value
            bps = (uint8_t) (73 + random (-3, 3) );

            int i = random (0, 4);
            hrmc1->setUserDescriptor(names[i]);

            // Note: We use .notify instead of .write!
            // If it is connected but CCCD is not enabled
            // The characteristic's value is still updated although notification is not sent
            if ( hrmc1->notify (hrmdata, sizeof (hrmdata) ) ) {
                Serial.printf ("Heart Rate 1 Measurement updated to: %d, count %d, %s\n", bps, count, names[i]);
                hrm1_not_connected_announced = false;
            }

            else if ( hrm1_not_connected_announced == false ) {
                Serial.printf ("ERROR: HRM 1 notify not set in the CCCD or not connected! count = %d\n", count);
                hrm1_not_connected_announced = true;
            }
        }

        if ( hrmc2 != NULL ) {
            uint8_t hrmdata[2] = { 0b00000110, bps };           // Sensor connected, modify BPS value
            bps = (uint8_t) (63 + random (-3, 3) );

            int i = random (0, 4);
            hrmc2->setUserDescriptor(names[i]);

            // Note: We use .notify instead of .write!
            // If it is connected but CCCD is not enabled
            // The characteristic's value is still updated although notification is not sent
            if ( hrmc2->notify (hrmdata, sizeof (hrmdata) ) ) {
                Serial.printf ("Heart Rate 2 Measurement updated to: %d, count %d, %s\n", bps, count, names[i]);
                hrm2_not_connected_announced = false;
            }

            else if ( hrm2_not_connected_announced == false ) {
                Serial.printf ("ERROR: HRM 2 notify not set in the CCCD or not connected! count = %d\n", count);
                hrm2_not_connected_announced = true;
            }
        }

#if 0
        if ( (hrmc2 == NULL) && (--count == 0) ) {
            Serial.println ("Configuring the Heart Rate Monitor Service #2");
            hrmc2 = new BLECharacteristic (UUID16_CHR_HEART_RATE_MEASUREMENT);
            bslc2 = new BLECharacteristic (UUID16_CHR_BODY_SENSOR_LOCATION);
            setupHRM (hrmc2, bslc2, (char *) "Unit 2", 4);
            // Don't need this to make new service characteristic discoverable by central device.
            //   Bluefruit.Advertising.start (0);               // 0 = Don't stop advertising after n seconds
        }
#endif
#if 0
        if ( count-- == 0 ) {
            delete bslc2;
            bslc2 = NULL;
            delete hrmc2;
            hrmc2 = NULL;
        }
#endif
    }

    // Only send update once per second
    delay (1000);
}
