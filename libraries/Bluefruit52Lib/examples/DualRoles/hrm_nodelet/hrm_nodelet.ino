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

/*
 * This sketch demonstrate how to run both Central and Peripheral roles
 * at the same time. It will act as a relay between an central (mobile)
 * to another peripheral using bleuart service.
 *
 * Mobile <--> DualRole <--> peripheral Ble Uart
 */
#include <bluefruit.h>

/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Heart Rate Measurement Char: 0x2A37 (Mandatory)
 * Body Sensor Location Char:   0x2A38 (Optional)
 */

BLEClientService        hrmsDownstream (UUID16_SVC_HEART_RATE);
BLEClientCharacteristic hrmcDownstream (UUID16_CHR_HEART_RATE_MEASUREMENT);
BLEClientCharacteristic bslcDownstream (UUID16_CHR_BODY_SENSOR_LOCATION);


/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Heart Rate Measurement Char: 0x2A37
 * Body Sensor Location Char:   0x2A38
 */
BLEService        hrms = BLEService (UUID16_SVC_HEART_RATE);
BLECharacteristic hrmc = BLECharacteristic (UUID16_CHR_HEART_RATE_MEASUREMENT);
BLECharacteristic bslc = BLECharacteristic (UUID16_CHR_BODY_SENSOR_LOCATION);

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

uint8_t  bps = 72;


void setup()
{
    Serial.begin (115200);

    // Wait for a serial port monitor connection before proceeding.
    // "We don't wanna miss a thing."
    while ( !Serial ) {
        delay (100);    // for nrf52840 with native usb
    }

    // Configure pin 4 (user button) as an input and enable the internal pull-up resistor.
    pinMode (4, INPUT_PULLUP);

    Serial.println ("HRM Nodelet");
    Serial.println ("-------------------------------------\n");

    // Initialize Bluefruit with max concurrent connections as Peripheral = 2, Central = 6.
    // This allows up to two (2) upstream and up to six (6) downstream connections.
    // The reason that we support two concurrent upstream connections is because
    // we might want to swap the upstream connection if we find one that is better.
    // SRAM usage required by SoftDevice will increase with number of connections
    Bluefruit.begin (2, 6);
    Bluefruit.setTxPower (4);   // Check bluefruit.h for supported values
    Bluefruit.setName ("HRM-Nodelet");   // Check bluefruit.h for supported values

    // Callbacks for upstream connection.
    Bluefruit.Periph.setConnectCallback (upstream_connect_callback);
    Bluefruit.Periph.setDisconnectCallback (upstream_disconnect_callback);

    // Configure and Start the Device Information Service
    Serial.println ("Configuring the Device Information Service");
    bledis.setManufacturer ("Adafruit Industries");
    bledis.setModel ("Bluefruit Feather52");
    bledis.begin();

    // Start the BLE Battery Service and set it to 100%
    Serial.println ("Configuring the Battery Service");
    blebas.begin();
    blebas.write (100);

    ////////////////////////////////////////////////////////////////////////////
    // Configure and Start BLE HRM Service
    // Setup the Heart Rate Monitor service using
    // BLEService and BLECharacteristic classes
    Serial.println ("Configuring the Heart Rate Monitor Service");
    setupHRM();

    ////////////////////////////////////////////////////////////////////////////
    // Init BLE Central HRM Serivce (upstream)
    // Initialize HRM upstream
    hrmsDownstream.begin();

    // Initialize upstream characteristics of HRM.
    // Note: Upstream Char will be added to the last service that is begin()ed.
    bslcDownstream.begin();

    // Set up callback for receiving measurements from downstream nodelets.
    hrmcDownstream.setNotifyCallback (hrm_notify_callback);
    hrmcDownstream.begin();

    // Callbacks for downstream connection.
    Bluefruit.Central.setDisconnectCallback (downstream_disconnect_callback);
    Bluefruit.Central.setConnectCallback (downstream_connect_callback);

    /* Start Central Scanning
     * - Enable auto scan if disconnected
     * - Interval = 100 ms, window = 80 ms
     * - Don't use active scan
     * - Filter only accept HRM service
     * - Start(timeout) with timeout = 0 will scan forever (until connected)
     */
    Bluefruit.Scanner.setRxCallback (scan_callback);
    Bluefruit.Scanner.restartOnDisconnect (true);
    Bluefruit.Scanner.setInterval (160, 80); // in unit of 0.625 ms
    Bluefruit.Scanner.filterUuid (hrms.uuid);
    Bluefruit.Scanner.useActiveScan (false);

    // Do not actually start scanning unless we are the root nodelet or
    // we have an upstream connection to the root nodelet.
    // Bluefruit.Scanner.start (0);                  // // 0 = Don't stop scanning after n seconds

    // Set up Rssi changed callback
    Bluefruit.setRssiCallback(rssi_changed_callback);

    // Set up and start advertising.
    Serial.println ("Setting up the advertising payload(s)");
    startAdv();
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

    Serial.println ("Advertising....");
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
    hrmc.setProperties (CHR_PROPS_NOTIFY);
    hrmc.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    hrmc.setFixedLen (2);
    hrmc.setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
    hrmc.begin();
    uint8_t hrmdata[2] = { 0b00000110, 0x40 }; // Set the characteristic to use 8-bit values, with the sensor connected and detected
    hrmc.write (hrmdata, 2);
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
    bslc.setProperties (CHR_PROPS_READ);
    bslc.setPermission (SECMODE_OPEN, SECMODE_NO_ACCESS);
    bslc.setFixedLen (1);
    bslc.begin();
    // Preset the characteristic to 'Other' (0).
    // When the connection is made to the peripheral, this will be
    // changed to relay that peripheral's value to the central upstream.
    bslc.write8 (0);
}


void loop()
{
    static bool notRoot = true;

    digitalToggle (LED_RED);

    if ( Bluefruit.connected() && (digitalRead (4) == HIGH) ) {
        // Sensor connected, modify BPS value based on body sensor location.
        // BPS value is modified in order to generate different value ranges
        // based on the sensot location. This allow us to process different
        // ranges on the proxy peripheral aggregator.
        uint8_t hrmdata[2] = { 0b00000110, bps };
        bps = (uint8_t) (68 + bslc.read8() +  random (-3, 4) );  // Results in range of +/-3.

        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( hrmc.notify (hrmdata, sizeof (hrmdata) ) ) {
            Serial.print ("Local Heart Rate Measurement is: ");
            Serial.println (bps);
        }

        else {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
        }
    }

    if ( notRoot ) {
        
        int digitalReadValue = digitalRead (4);

        // Use the button to initiate scanning.
        // This is for the case when we want to designate this nodelet as the root.
        if (digitalReadValue == HIGH) {
        }

        else {
            // Enable root nodelet.
            // Scanning for downstream nodelets.
            Serial.println ("ROOT");
            Bluefruit.Scanner.start (0);  // 0 = Don't stop scanning after n seconds
            notRoot = false;
        }
    }

    // Only send update once per second
    delay (1000);
}


void sendToUpstream (uint8_t *hrmdata, uint16_t len)
{
    static bool firstTime = true;

    if ( Bluefruit.connected() ) {
        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( hrmc.notify (hrmdata, len) ) {
            Serial.print ("sendToUpstream(): ");
            Serial.println (hrmdata[ (len - 1)]);
            firstTime = true;
        }

        else if ( firstTime ) {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
            firstTime = false;
        }
    }
}


void upstream_connect_callback (uint16_t conn_handle)
{
    Serial.print ("Upstream connection is to ");
    Serial.println (getPeerNameFromHandle(conn_handle));
    
    // We have an upstream connection.
    // Stop advertising and start scanning.
    Bluefruit.Advertising.stop();
    Bluefruit.Scanner.start (0);  // 0 = Don't stop scanning after n seconds
    
    // Actually, we do want to continue to advertise so that we might find a
    // better connection if a closer (higher RSSI) nodelet appears.

    // Get the reference to current connection
    BLEConnection* connection = Bluefruit.Connection(conn_handle);

    // Start monitoring rssi of this connection
    // This function should be called in connect callback
    // Input argument is value difference (to current rssi) that triggers callback
    connection->monitorRssi(10);
}


/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void upstream_disconnect_callback (uint16_t conn_handle, uint8_t reason)
{
    (void) conn_handle;
    (void) reason;
    Serial.print ("Upstream disconnected, reason = 0x");
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
    if (chr->uuid == hrmc.uuid) {
        if (chr->notifyEnabled (conn_hdl) ) {
            Serial.println ("Heart Rate Measurement 'Notify' enabled");
        }

        else {
            Serial.println ("Heart Rate Measurement 'Notify' disabled");
        }
    }
}


/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback (ble_gap_evt_adv_report_t *report)
{
    // Since we configure the scanner with filterUuid()
    // Scan callback only invoked for device with hrm service advertised
    // Connect to device with HRM service in advertising
    Bluefruit.Central.connect (report);
}


/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void downstream_connect_callback (uint16_t conn_handle)
{
    Serial.print ("Downstream connection is to ");
    Serial.println (getPeerNameFromHandle(conn_handle));

    Serial.print ("Discovering HRM Service ... ");

    // If HRM is not found, disconnect and return
    if ( !hrmsDownstream.discover (conn_handle) ) {
        Serial.println ("Found NONE");
        // disconnect since we couldn't find HRM service
        Bluefruit.disconnect (conn_handle);
        return;
    }

    // Once HRM service is found, we continue to discover its characteristic
    Serial.println ("Found it");
    Serial.print ("Discovering Measurement characteristic ... ");

    if ( !hrmcDownstream.discover() ) {
        // Measurement chr is mandatory, if it is not found (valid), then disconnect
        Serial.println ("not found !!!");
        Serial.println ("Measurement characteristic is mandatory but not found");
        Bluefruit.disconnect (conn_handle);
        return;
    }

    Serial.println ("Found it");
    // Measurement is found, continue to look for option Body Sensor Location
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.body_sensor_location.xml
    // Body Sensor Location is optional, print out the location in text if present
    Serial.print ("Discovering Body Sensor Location characteristic ... ");

    if ( bslcDownstream.discover() ) {
        Serial.println ("Found it");
        // Body sensor location value is 8 bit
        const char *body_str[] = { "Other", "Chest", "Wrist", "Finger", "Hand", "Ear Lobe", "Foot" };
        // Read 8-bit BSLC value from peripheral
        uint8_t loc_value = bslcDownstream.read8();
        // Update what may be read from the proxy by the smart phone app.
        bslc.write8 (loc_value);
        Serial.print ("Body Location Sensor: ");
        Serial.println (body_str[loc_value]);
    }

    else {
        Serial.println ("Found NONE");
    }

    // Reaching here means we are ready to go, let's enable notification on measurement chr
    if ( hrmcDownstream.enableNotify() ) {
        Serial.println ("Ready to receive HRM Measurement value");
    }

    else {
        Serial.println ("Couldn't enable notify for HRM Measurement. Increase DEBUG LEVEL for troubleshooting");
    }

    Serial.println ("Continue scanning for more downstream nodelets.");
    Bluefruit.Scanner.start (0);

    // Start monitoring rssi of this connection
    // This function should be called in connect callback
    // no parameters means we don't use rssi changed callback 
    BLEConnection* conn = Bluefruit.Connection(conn_handle);
    conn->monitorRssi();
}


/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void downstream_disconnect_callback (uint16_t conn_handle, uint8_t reason)
{
    (void) conn_handle;
    (void) reason;
    Serial.print ("Downstream disconnected, reason = 0x");
    Serial.println (reason, HEX);
}


/**
 * Hooked callback that triggered when a measurement value is sent from peripheral
 * @param chr   Pointer upstream characteristic that even occurred,
 *              in this example it should be hrmc
 * @param data  Pointer to received data
 * @param len   Length of received data
 */
void hrm_notify_callback (BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.heart_rate_measurement.xml
    // Measurement contains of control byte0 and measurement (8 or 16 bit) + optional field
    // if byte0's bit0 is 0 --> measurement is 8 bit, otherwise 16 bit.
    if ( data[0] & bit (0) ) {
        uint16_t value;
        memcpy (&value, data + 1, 2);
        Serial.print ("hrm_notify_callback value: ");
        Serial.println (value);
    }

    else {
        Serial.print ("hrm_notify_callback data[1]: ");
        Serial.println (data[1]);
    }

    sendToUpstream (data, 2);
}


char *getPeerNameFromHandle (uint16_t conn_handle)
{
    static char peer_name[32] = { 0 };

    // Get the reference to this handle's connection.
    BLEConnection *connection = Bluefruit.Connection (conn_handle);
    connection->getPeerName (peer_name, sizeof (peer_name) );

    // get the RSSI value of this connection
    // monitorRssi() must be called previously (in connect callback)
    int8_t rssi = connection->getRssi();
    
    Serial.printf("%s RSSI = %d\n", peer_name, rssi);

    return peer_name;
}


void rssi_changed_callback(uint16_t conn_hdl, int8_t rssi)
{
  (void) conn_hdl;
  Serial.printf("New RSSI = %d", rssi);
  Serial.println();
}
