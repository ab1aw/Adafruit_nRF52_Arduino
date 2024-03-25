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

/* This sketch show how to use BLEClientService and BLEClientCharacteristic
 * to implement a custom client that is used to talk with Gatt server on
 * peripheral.
 *
 * Note: you will need another feather52 running peripheral/custom_HRM sketch
 * to test with.
 */

#include <bluefruit.h>

// Struct containing peripheral info
typedef struct {
    char name[16 + 1];

    uint16_t conn_handle;

    // Each prph need its own BLE HRM client service
    BLEClientService        *hrms;
    BLEClientCharacteristic *hrmc;
    BLEClientCharacteristic *bslc;
} prph_info_t;

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

    Serial.println ("Bluefruit52 Multi Central Custom HRM Example");
    Serial.println ("--------------------------------------\n");
    // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 4
    // SRAM usage required by SoftDevice will increase dramatically with number of connections
    Bluefruit.begin (0, 4);
    // Set Name
    Bluefruit.setName ("Bluefruit52 Multi Central HRM Example");

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

    // Increase Blink rate to different from PrPh advertising mode
    Bluefruit.setConnLedInterval (250);
    // Callbacks for Central
    Bluefruit.Central.setDisconnectCallback (disconnect_callback);
    Bluefruit.Central.setConnectCallback (connect_callback);
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
    //Bluefruit.Scanner.filterUuid(hrms.uuid);
    Bluefruit.Scanner.useActiveScan (false);
    Bluefruit.Scanner.start (0);                  // // 0 = Don't stop scanning after n seconds
}

void loop()
{
    // do nothing
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
    Serial.println ("scan_callback");
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback (uint16_t conn_handle)
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

    if ( ! peer->hrmc->discover() ) {
        // Measurement chr is mandatory, if it is not found (valid), then disconnect
        Serial.println ("ERROR! not found !!!");
        Serial.println ("ERROR! Measurement characteristic is mandatory but not found.");
        Bluefruit.disconnect (conn_handle);
        return;
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
void disconnect_callback (uint16_t conn_handle, uint8_t reason)
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
    int id = findConnHandle ( chr->connHandle() );
    Serial.print (id);
    Serial.print (" : ");
    Serial.print (prphs[id].name);
    Serial.print (" : HRM Measurement: ");

    if ( data[0] & bit (0) ) {
        uint16_t value;
        memcpy (&value, data + 1, 2);
        Serial.print (value);
    }

    else {
        Serial.print (data[1]);
    }

    Serial.print (" : BSLC : ");
    Serial.println (prphs[id].bslc->read8() );
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
