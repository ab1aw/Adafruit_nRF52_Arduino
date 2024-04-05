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
 * This sketch demonstrates how the Spanning Tree Protocol (STP) can be
 * leveraged, along with the dual-role capabilities of BLE, to create
 * low power, low radiation, self-healing mesh networks using low cost
 * devices and without any additional networking protocols or overhead.
 */

#include <bluefruit.h>

// Struct containing peripheral info
typedef struct {
    char name[16 + 1];
    char body_sensor_location[16 + 1];

    uint16_t conn_handle;

    // Each prph need its own BLE HRM client service
    BLEClientService        *hrms;
    BLEClientCharacteristic *hrmc;
    BLEClientCharacteristic *bslc;
} prph_info_t;


/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Heart Rate Measurement Char: 0x2A37 (Mandatory)
 * Body Sensor Location Char:   0x2A38 (Optional)
 */

BLEClientService        hrmsClient (UUID16_SVC_HEART_RATE);
BLEClientCharacteristic hrmcClient (UUID16_CHR_HEART_RATE_MEASUREMENT);
BLEClientCharacteristic bslcClient (UUID16_CHR_BODY_SENSOR_LOCATION);


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


/* Peripheral info array (one per peripheral device)
 *
 * There are 'BLE_MAX_CONNECTION' central connections, but the
 * the connection handle can be numerically larger (for example if
 * the peripheral role is also used, such as connecting to a mobile
 * device). As such, we need to convert connection handles <-> the array
 * index where appropriate to prevent out of array accesses.
 *
 * Note: One can simply declares the array with BLE_MAX_CONNECTION and use connection
 * handle as index directly with the expense of SRAM.
 */
prph_info_t prphs[BLE_MAX_CONNECTION];

uint8_t connection_num = 0;


void setup()
{
    Serial.begin (115200);

    while ( !Serial ) {
        delay (10);    // for nrf52840 with native usb
    }

    Serial.println ("STP Dual-Role BLE Mesh Network");
    Serial.println ("-------------------------------------\n");

    // Initialize Bluefruit with maximum connections as Peripheral = 1, Central = 6
    // SRAM usage required by SoftDevice will increase with number of connections
    Bluefruit.begin (1, 6);
    Bluefruit.setTxPower (4);   // Check bluefruit.h for supported values
    Bluefruit.setName ("STP-Dual-Role");   // Check bluefruit.h for supported values

    // Callbacks for Peripheral
    Bluefruit.Periph.setConnectCallback (periph_connect_callback);
    Bluefruit.Periph.setDisconnectCallback (periph_disconnect_callback);

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
    // Init BLE Central HRM Serivce (client)
    // Initialize HRM client
    setupCentralRole();

    // Callbacks for Central
    Bluefruit.Central.setDisconnectCallback (cent_disconnect_callback);
    Bluefruit.Central.setConnectCallback (cent_connect_callback);

    configureScanning();

    configureAdvertising();
}


void setupCentralRole()
{
    // Init peripheral pool
    for (uint8_t idx = 0; idx < BLE_MAX_CONNECTION; idx++) {
        BLEClientService        *hrms;
        BLEClientCharacteristic *hrmc;
        BLEClientCharacteristic *bslc;
        /* HRM Service Definitions
         * Heart Rate Monitor Service:  0x180D
         * Heart Rate Measurement Char: 0x2A37 (Mandatory)
         * Body Sensor Location Char:   0x2A38 (Optional)
         */
        hrms = prphs[idx].hrms = new BLEClientService (UUID16_SVC_HEART_RATE);
        hrmc = prphs[idx].hrmc = new BLEClientCharacteristic (UUID16_CHR_HEART_RATE_MEASUREMENT);
        bslc = prphs[idx].bslc = new BLEClientCharacteristic (UUID16_CHR_BODY_SENSOR_LOCATION);
        // Invalid all connection handle
        prphs[idx].conn_handle = BLE_CONN_HANDLE_INVALID;

        // Default/invalid name and body sensor location.
        strcpy(prphs[idx].name, "Unknown");
        strcpy(prphs[idx].body_sensor_location, "Other");

        // Initialize HRM client
        hrms->begin();
        // Initialize client characteristics of HRM.
        // Note: Client Char will be added to the last service that is begin()ed.
        bslc->begin();
        // set up callback for receiving measurement
        hrmc->setNotifyCallback (hrm_notify_callback);
        hrmc->begin();
        // Do we have to do this repeatedly for the same uuid value?
        Bluefruit.Scanner.filterUuid (hrms->uuid);
    }
}


void configureScanning(void)
{
    /* Configure Central Scanning
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

    Bluefruit.Scanner.start (0);                  // // 0 = Don't stop scanning after n seconds
}


void configureAdvertising(void)
{
    // Advertising packet
    Bluefruit.Advertising.addFlags (BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();

    // Include HRM Service UUID
    Bluefruit.Advertising.addService (hrms);

    // Include Name
    Bluefruit.Advertising.addName();

    /* Configure Advertising
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
    // changed to relay that peripheral's value to the central client.
    bslc.write8 (0);
}


void loop()
{
    digitalToggle (LED_RED);

    // Only toggle once per second.
    delay (1000);
}


void sendToClient (uint8_t *hrmdata, uint16_t len)
{
    static bool firstTime = true;

    if ( Bluefruit.connected() ) {
        // Note: We use .notify instead of .write!
        // If it is connected but CCCD is not enabled
        // The characteristic's value is still updated although notification is not sent
        if ( hrmc.notify (hrmdata, len) ) {
            Serial.print ("Heart Rate Measurement updated to: ");
            Serial.println (hrmdata[ (len - 1)]);
            firstTime = true;
        }

        else if ( firstTime ) {
            Serial.println ("ERROR: Notify not set in the CCCD or not connected!");
            firstTime = false;
        }
    }
}


void periph_connect_callback (uint16_t conn_handle)
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
void periph_disconnect_callback (uint16_t conn_handle, uint8_t reason)
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
    Serial.printf("scan_callback(): Tx Power %d, RSSI %d\n", report->tx_power, report->rssi);

    // MAC is in little endian --> print reverse
    Serial.printBufferReverse (report->peer_addr.addr, 6, ':');
    Serial.print ("\n\n");

    // Since we configure the scanner with filterUuid()
    // Scan callback only invoked for device with hrm service advertised
    // Connect to device with HRM service in advertising
    Bluefruit.Central.connect (report);
}


/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void cent_connect_callback (uint16_t conn_handle)
{
    // Find an available ID to use
    int id  = findConnHandle (BLE_CONN_HANDLE_INVALID);

    // Eeek: Exceeded the number of connections !!!
    if ( id < 0 ) {
        Serial.println ("ERROR! TOO MANY CONNECTIONS!");
        return;
    }

    prph_info_t *peer = &prphs[id];
    peer->conn_handle = conn_handle;
    Bluefruit.Connection (conn_handle)->getPeerName (peer->name, sizeof (peer->name) - 1);
    Serial.print ("Connected to ");
    Serial.println (peer->name);
    Serial.print ("Discovering HRM Service ... ");

    // If HRM is not found, disconnect and return
    if ( ! peer->hrms->discover (conn_handle) ) {
        Serial.println ("Found NONE");
        // disconnect since we couldn't find HRM service
        Bluefruit.disconnect (conn_handle);
        return;
    }

    // Once HRM service is found, we continue to discover its characteristic
    Serial.println ("Found it");
    Serial.print ("Discovering Measurement characteristic ... ");
#if 0

    if ( ! peer->hrmc->discover() ) {
        // Measurement chr is mandatory, if it is not found (valid), then disconnect
        Serial.println ("ERROR! not found !!!");
        Serial.println ("ERROR! Measurement characteristic is mandatory but not found.");
        Bluefruit.disconnect (conn_handle);
        return;
    }

#endif

    while ( peer->hrmc->discover() ) {
        Serial.println ("FOUND ANOTHER ....");
    }

    Serial.println ("Found it");
    // Measurement is found, continue to look for option Body Sensor Location
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.body_sensor_location.xml
    // Body Sensor Location is optional, print out the location in text if present
    Serial.print ("Discovering Body Sensor Location characteristic ... ");

    if ( peer->bslc->discover() ) {
        Serial.println ("Found it");
        // Body sensor location value is 8 bit
        const char *body_str[] = { "Other", "Chest", "Wrist", "Finger", "Hand", "Ear Lobe", "Foot" };
        // Read 8-bit BSLC value from peripheral
        uint8_t loc_value = peer->bslc->read8();
        Serial.print ("Body Location Sensor: ");
        Serial.println (body_str[loc_value]);
    }

    else {
        Serial.println ("Found NONE");
    }

    // Reaching here means we are ready to go, let's enable notification on measurement chr
    if ( peer->hrmc->enableNotify() ) {
        Serial.println ("Ready to receive HRM Measurement value");
    }

    else {
        Serial.println ("Couldn't enable notify for HRM Measurement. Increase DEBUG LEVEL for troubleshooting");
    }

    connection_num++;
    Serial.println ("Continue scanning for more peripherals");
    Bluefruit.Scanner.start (0);
}


/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void cent_disconnect_callback (uint16_t conn_handle, uint8_t reason)
{
    (void) conn_handle;
    (void) reason;
    // Mark the ID as invalid
    int id  = findConnHandle (conn_handle);

    if ( id < 0 ) {
        Serial.println ("ERROR! CONNECTION NOT FOUND!");
        return;
    }

    connection_num--;
    // Mark conn handle as invalid
    prphs[id].conn_handle = BLE_CONN_HANDLE_INVALID;
    Serial.print (prphs[id].name);
    Serial.print (" is disconnected, reason = 0x");
    Serial.println (reason, HEX);
}


/**
 * Hooked callback that triggered when a measurement value is sent from peripheral
 * @param chr   Pointer client characteristic that even occurred,
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
        Serial.print ("HRM Measurement value: ");
        Serial.println (value);
    }

    else {
        Serial.print ("HRM Measurement data[1]: ");
        Serial.println (data[1]);
    }

    sendToClient (data, 2);
}


/**
 * Find the connection handle in the peripheral array
 * @param conn_handle Connection handle
 * @return array index if found, otherwise -1
 */
int findConnHandle (uint16_t conn_handle)
{
    for (int id = 0; id < BLE_MAX_CONNECTION; id++) {
        if (conn_handle == prphs[id].conn_handle) {
            return id;
        }
    }

    return -1;
}
