#include <Arduino.h>
#include <M5UnitQRCode.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>




M5UnitQRCodeUART qrcode;


uint8_t station4Mac[] = {
    0x02,
    0xAA,
    0x10,
    0x00,
    0x01,
    0x04
};


// =====================================================
// STATION
// =====================================================

const String station = "ST6_";


// =====================================================

String bufferData = "";

unsigned long lastDataReceivedTime = 0;

const unsigned long GATHER_TIMEOUT = 150;


// =====================================================
// QR DUPLICATE CONTROL
// =====================================================

String lastProcessedCode = "";

unsigned long lastSeenTime = 0;

const unsigned long RESET_INTERVAL = 1500;


// =====================================================
// RGB LED
// =====================================================

#define RGB_LED_PIN       27
#define RGB_BRIGHTNESS    100


// =====================================================
// STATION CONFIG
// =====================================================

#define MY_LINE_ID        1
#define MY_STATION_ID     6


// =====================================================
// PACKET ID
// =====================================================

uint32_t packetID = 0;


// =====================================================
// ESP-NOW PACKET
// =====================================================

typedef struct {

    uint8_t line_id;

    uint8_t station_id;

    uint32_t packet_id;

    char qr[100];

} ESPNowPacket;


// =====================================================
// SEND CALLBACK
// =====================================================

void onDataSent(
    const wifi_tx_info_t *info,
    esp_now_send_status_t status
)
{
    
}


// =====================================================
// SETUP
// =====================================================

void setup()
{

    // =================================================
    // SERIAL
    // =================================================

    Serial.begin(115200);

    delay(500);


    // =================================================
    // WIFI STA
    // =================================================

    WiFi.mode(WIFI_STA);

    delay(100);


    // =================================================
    // ESP-NOW
    // =================================================

    if (
        esp_now_init() != ESP_OK
    ) {

        rgbLedWrite(
            RGB_LED_PIN,
            RGB_BRIGHTNESS,
            RGB_BRIGHTNESS,
            0
        );

        while (1) {
            delay(100);
        }
    }


    // =================================================
    // REGISTER SEND CALLBACK
    // =================================================

    esp_now_register_send_cb(
        onDataSent
    );


    // =================================================
    // ADD ST4 PEER
    // =================================================

    esp_now_peer_info_t peerInfo = {};

    memcpy(
        peerInfo.peer_addr,
        station4Mac,
        6
    );

    peerInfo.channel = 0;

    peerInfo.encrypt = false;


    if (
        !esp_now_is_peer_exist(
            station4Mac
        )
    ) {

        if (
            esp_now_add_peer(
                &peerInfo
            ) != ESP_OK
        ) {

            rgbLedWrite(
                RGB_LED_PIN,
                RGB_BRIGHTNESS,
                RGB_BRIGHTNESS,
                0
            );

            while (1) {
                delay(100);
            }
        }
    }


    // =================================================
    // QR UART
    // =================================================

    Serial2.begin(
        115200,
        SERIAL_8N1,
        16,
        17
    );


    // =================================================
    // QR INIT
    // =================================================

    if (
        !qrcode.begin(
            &Serial2,
            UNIT_QRCODE_UART_BAUD,
            4,
            5
        )
    ) {

        rgbLedWrite(
            RGB_LED_PIN,
            RGB_BRIGHTNESS,
            RGB_BRIGHTNESS,
            0
        );

        while (1) {
            delay(100);
        }
    }


    qrcode.setTriggerMode(
        AUTO_SCAN_MODE
    );


    // =================================================
    // STARTUP LED
    // =================================================

    rgbLedWrite(
        RGB_LED_PIN,
        RGB_BRIGHTNESS,
        0,
        0
    );

    delay(500);


    rgbLedWrite(
        RGB_LED_PIN,
        RGB_BRIGHTNESS,
        RGB_BRIGHTNESS,
        0
    );

    delay(500);


    rgbLedWrite(
        RGB_LED_PIN,
        0,
        RGB_BRIGHTNESS,
        0
    );

    delay(500);


    rgbLedWrite(
        RGB_LED_PIN,
        0,
        0,
        0
    );
}


// =====================================================
// SEND QR TO ST4
// =====================================================

void sendQRToST4(
    String qrCode
)
{

    ESPNowPacket packet;


    // =================================================
    // LINE
    // =================================================

    packet.line_id =
        MY_LINE_ID;


    // =================================================
    // STATION
    // =================================================

    packet.station_id =
        MY_STATION_ID;


    // =================================================
    // PACKET ID
    // =================================================

    packet.packet_id =
        packetID++;


    // =================================================
    // QR
    // =================================================

    qrCode.toCharArray(
        packet.qr,
        sizeof(packet.qr)
    );


    // =================================================
    // SEND
    // =================================================

    esp_now_send(
        station4Mac,
        (uint8_t *)&packet,
        sizeof(packet)
    );
}


// =====================================================
// LOOP
// =====================================================

void loop()
{

    unsigned long currentTime =
        millis();


    // =================================================
    // QR SCANNER
    // =================================================

    if (
        qrcode.available()
    ) {

        String qrPart =
            qrcode.getDecodeData();


        bufferData +=
            qrPart;


        lastDataReceivedTime =
            currentTime;


        lastSeenTime =
            currentTime;
    }


    // =================================================
    // QR GATHER
    // =================================================

    if (
        bufferData != "" &&
        (
            currentTime -
            lastDataReceivedTime
        ) >= GATHER_TIMEOUT
    ) {


        // =================================================
        // CHỐNG QR TRÙNG
        // =================================================

        if (
            bufferData !=
            lastProcessedCode
        ) {

            lastProcessedCode =
                bufferData;


            // =================================================
            // TẠO QR CÓ STATION
            // =================================================

            String fullCode =
                station +
                bufferData;


            // =================================================
            // GỬI ESP-NOW TỚI ST4
            // =================================================

            sendQRToST4(
                fullCode
            );


            // =================================================
            // LED BÁO ĐÃ GỬI
            // =================================================

            rgbLedWrite(
                RGB_LED_PIN,
                0,
                RGB_BRIGHTNESS,
                0
            );

            delay(1000);

            rgbLedWrite(
                RGB_LED_PIN,
                0,
                0,
                0
            );
        }


        bufferData = "";
    }


    // =================================================
    // RESET QR
    // =================================================

    if (
        lastProcessedCode != "" &&
        (
            currentTime -
            lastSeenTime
        ) >= RESET_INTERVAL
    ) {

        lastProcessedCode =
            "";
    }


    delay(10);
}