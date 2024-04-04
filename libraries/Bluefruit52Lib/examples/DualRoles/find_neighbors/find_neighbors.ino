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
    //Bluefruit.Periph.setConnectCallback (upstream_connect_callback);
    //Bluefruit.Periph.setDisconnectCallback (upstream_disconnect_callback);
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
    //hrmcDownstream.setNotifyCallback (hrm_notify_callback);
    hrmcDownstream.begin();
    // Callbacks for downstream connection.
    //Bluefruit.Central.setDisconnectCallback (downstream_disconnect_callback);
    //Bluefruit.Central.setConnectCallback (downstream_connect_callback);
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
    Bluefruit.Scanner.useActiveScan (true);
    Bluefruit.Scanner.start (0);                  // // 0 = Don't stop scanning after n seconds
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
    //hrmc.setCccdWriteCallback (cccd_callback); // Optionally capture CCCD updates
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
    digitalToggle (LED_RED);
    // Only send update once per second
    delay (1000);
}


/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback (ble_gap_evt_adv_report_t *report)
{
    PRINT_LOCATION();
    uint8_t len = 0;
    uint8_t buffer[32];
    memset (buffer, 0, sizeof (buffer) );

    /* Display the timestamp and device address */
    if (report->type.scan_response) {
        Serial.printf ("[SR%10d] Packet received from ", millis() );
    }

    else {
        Serial.printf ("[ADV%9d] Packet received from ", millis() );
    }

    // MAC is in little endian --> print reverse
    Serial.printBufferReverse (report->peer_addr.addr, 6, ':');
    Serial.print ("\n");
    /* Raw buffer contents */
    Serial.printf ("%14s %d bytes\n", "PAYLOAD", report->data.len);

    if (report->data.len) {
        Serial.printf ("%15s", " ");
        Serial.printBuffer (report->data.p_data, report->data.len, '-');
        Serial.println();
    }

    /* RSSI value */
    Serial.printf ("%14s %d dBm\n", "RSSI", report->rssi);
    /* Adv Type */
    Serial.printf ("%14s ", "ADV TYPE");

    if ( report->type.connectable ) {
        Serial.print ("Connectable ");
    }

    else {
        Serial.print ("Non-connectable ");
    }

    if ( report->type.directed ) {
        Serial.println ("directed");
    }

    else {
        Serial.println ("undirected");
    }

    /* Shortened Local Name */
    if (Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, buffer, sizeof (buffer) ) ) {
        Serial.printf ("%14s %s\n", "SHORT NAME", buffer);
        memset (buffer, 0, sizeof (buffer) );
    }

    /* Complete Local Name */
    if (Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof (buffer) ) ) {
        Serial.printf ("%14s %s\n", "COMPLETE NAME", buffer);
        memset (buffer, 0, sizeof (buffer) );
    }

    /* TX Power Level */
    if (Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, buffer, sizeof (buffer) ) ) {
        Serial.printf ("%14s %i\n", "TX PWR LEVEL", buffer[0]);
        memset (buffer, 0, sizeof (buffer) );
    }

    /* Check for UUID16 Complete List */
    len = Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, buffer, sizeof (buffer) );

    if ( len ) {
        printUuid16List (buffer, len);
    }

    /* Check for UUID16 More Available List */
    len = Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof (buffer) );

    if ( len ) {
        printUuid16List (buffer, len);
    }

    /* Check for UUID128 Complete List */
    len = Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, buffer, sizeof (buffer) );

    if ( len ) {
        printUuid128List (buffer, len);
    }

    /* Check for UUID128 More Available List */
    len = Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof (buffer) );

    if ( len ) {
        printUuid128List (buffer, len);
    }

    /* Check for BLE UART UUID */
    if ( Bluefruit.Scanner.checkReportForUuid (report, BLEUART_UUID_SERVICE) ) {
        Serial.printf ("%14s %s\n", "BLE UART", "UUID Found!");
    }

    /* Check for DIS UUID */
    if ( Bluefruit.Scanner.checkReportForUuid (report, UUID16_SVC_DEVICE_INFORMATION) ) {
        Serial.printf ("%14s %s\n", "DIS", "UUID Found!");
    }

    /* Check for Manufacturer Specific Data */
    len = Bluefruit.Scanner.parseReportByType (report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, buffer, sizeof (buffer) );

    if (len) {
        Serial.printf ("%14s ", "MAN SPEC DATA");
        Serial.printBuffer (buffer, len, '-');
        Serial.println();
        memset (buffer, 0, sizeof (buffer) );
    }

    Serial.println();
    // For Softdevice v6: after received a report, scanner will be paused
    // We need to call Scanner resume() to continue scanning
    Serial.println ("Keep scanning and advertising.");
    Bluefruit.Scanner.resume();
}

void printUuid16List (uint8_t *buffer, uint8_t len)
{
    Serial.printf ("%14s %s", "16-Bit UUID");

    for (int i = 0; i < len; i += 2) {
        uint16_t uuid16;
        memcpy (&uuid16, buffer + i, 2);
        Serial.printf ("%04X ", uuid16);
    }

    Serial.println();
}

void printUuid128List (uint8_t *buffer, uint8_t len)
{
    (void) len;
    Serial.printf ("%14s %s", "128-Bit UUID");

    // Print reversed order
    for (int i = 0; i < 16; i++) {
        const char *fm = (i == 4 || i == 6 || i == 8 || i == 10) ? "-%02X" : "%02X";
        Serial.printf (fm, buffer[15 - i]);
    }

    Serial.println();
}
