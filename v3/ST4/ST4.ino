#include <Arduino.h>
#include <M5UnitQRCode.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>


// =====================================================
// QR CODE
// =====================================================

M5UnitQRCodeUART qrcode;


// =====================================================
// ST4 MAC
// =====================================================

uint8_t myMac[] = {
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

const String station = "ST4_";


// =====================================================
// QR DUPLICATE CONTROL
// =====================================================

String lastProcessedCode = "";

unsigned long lastSeenTime = 0;

const unsigned long RESET_INTERVAL = 1500;


// =====================================================
// QR STATUS
// =====================================================

bool qrReady = false;


// =====================================================
// RGB LED
// =====================================================

#define RGB_LED_PIN        27
#define RGB_BRIGHTNESS     100


// =====================================================
// STATION CONFIG
// =====================================================

#define MY_LINE_ID         1
#define MY_STATION_ID      4


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
// EVENT TYPE
// =====================================================

typedef enum {

    EVENT_QR = 0,

    EVENT_ESPNOW = 1

} EventType;


// =====================================================
// EVENT
// =====================================================

typedef struct {

    EventType type;

    uint8_t line_id;

    uint8_t station_id;

    uint32_t packet_id;

    char qr[100];

} Event;


// =====================================================
// QUEUE
// =====================================================

QueueHandle_t eventQueue;


// =====================================================
// KIỂM TRA QR HỢP LỆ
//
// Chỉ nhận:
// A-Z
// a-z
// 0-9
//
// Độ dài:
// 2 -> 90 ký tự
// =====================================================

bool isValidQR(String data)
{

    if (
        data.length() < 2
    ) {
        return false;
    }


    if (
        data.length() > 90
    ) {
        return false;
    }


    for (
        int i = 0;
        i < data.length();
        i++
    ) {

        char c =
            data[i];


        if (!(
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9')
        )) {

            return false;
        }
    }


    return true;
}


// =====================================================
// ESP-NOW RECEIVE CALLBACK
// =====================================================

void onDataReceive(
    const esp_now_recv_info_t *info,
    const uint8_t *incomingData,
    int len
)
{

    // =================================================
    // KIỂM TRA PACKET SIZE
    // =================================================

    if (
        len != sizeof(ESPNowPacket)
    ) {
        return;
    }


    // =================================================
    // COPY PACKET
    // =================================================

    ESPNowPacket packet;


    memcpy(
        &packet,
        incomingData,
        sizeof(packet)
    );


    // =================================================
    // CHỈ NHẬN CÙNG LINE
    // =================================================

    if (
        packet.line_id != MY_LINE_ID
    ) {
        return;
    }


    // =================================================
    // TẠO EVENT
    // =================================================

    Event event;


    event.type =
        EVENT_ESPNOW;


    event.line_id =
        packet.line_id;


    event.station_id =
        packet.station_id;


    event.packet_id =
        packet.packet_id;


    // =================================================
    // COPY QR
    // =================================================

    strncpy(
        event.qr,
        packet.qr,
        sizeof(event.qr) - 1
    );


    event.qr[
        sizeof(event.qr) - 1
    ] = '\0';


    // =================================================
    // ĐƯA EVENT VÀO QUEUE
    // =================================================

    if (
        eventQueue != NULL
    ) {

        xQueueSend(
            eventQueue,
            &event,
            0
        );
    }
}


// =====================================================
// EVENT PROCESSING TASK
//
// Serial CHỈ xuất QR cho Python
// Không xuất log/debug
// =====================================================

void eventProcessingTask(
    void *parameter
)
{

    Event event;


    while (true) {

        if (
            xQueueReceive(
                eventQueue,
                &event,
                portMAX_DELAY
            )
        ) {


            // =========================================
            // LOCAL QR
            // =========================================

            if (
                event.type == EVENT_QR
            ) {

                Serial.println(
                    event.qr
                );
            }


            // =========================================
            // ESP-NOW QR
            // =========================================

            else if (
                event.type == EVENT_ESPNOW
            ) {


                // -------------------------------------
                // Kiểm tra LINE
                // -------------------------------------

                if (
                    event.line_id != MY_LINE_ID
                ) {
                    continue;
                }


                // -------------------------------------
                // Chỉ nhận QR bắt đầu bằng ST
                // -------------------------------------

                if (
                    strncmp(
                        event.qr,
                        "ST",
                        2
                    ) != 0
                ) {
                    continue;
                }


                // -------------------------------------
                // Gửi QR qua UART
                // -------------------------------------

                Serial.println(
                    event.qr
                );
            }
        }
    }
}


// =====================================================
// CREATE QR EVENT
// =====================================================

void createQREvent(
    String qrData
)
{

    Event event;


    // =================================================
    // EVENT TYPE
    // =================================================

    event.type =
        EVENT_QR;


    // =================================================
    // LINE
    // =================================================

    event.line_id =
        MY_LINE_ID;


    // =================================================
    // STATION
    // =================================================

    event.station_id =
        MY_STATION_ID;


    // =================================================
    // LOCAL QR KHÔNG CÓ PACKET ID
    // =================================================

    event.packet_id =
        0;


    // =================================================
    // COPY QR
    // =================================================

    qrData.toCharArray(
        event.qr,
        sizeof(event.qr)
    );


    // =================================================
    // QUEUE
    // =================================================

    if (
        eventQueue != NULL
    ) {

        xQueueSend(
            eventQueue,
            &event,
            0
        );
    }
}


// =====================================================
// SETUP
// =====================================================

void setup()
{

    // =================================================
    // SERIAL
    //
    // Serial dùng để gửi QR cho Python
    // =================================================

    Serial.begin(
        115200
    );


    delay(500);


    // =================================================
    // QUEUE
    // =================================================

    eventQueue =
        xQueueCreate(
            50,
            sizeof(Event)
        );


    if (
        eventQueue == NULL
    ) {

        rgbLedWrite(
            RGB_LED_PIN,
            0,
            0,
            0
        );

        return;
    }


    // =================================================
    // EVENT TASK
    // =================================================

    xTaskCreate(
        eventProcessingTask,
        "EventTask",
        4096,
        NULL,
        2,
        NULL
    );


    // =================================================
    // WIFI STA
    // =================================================

    WiFi.mode(
        WIFI_STA
    );


    delay(100);


    // =================================================
    // SET CUSTOM MAC
    // =================================================

    esp_wifi_set_mac(
        WIFI_IF_STA,
        myMac
    );
    Serial.println( WiFi.macAddress() );


    // =================================================
    // ESP-NOW
    // =================================================

    if (
        esp_now_init() != ESP_OK
    ) {

        rgbLedWrite(
            RGB_LED_PIN,
            0,
            0,
            0
        );

        return;
    }


    // =================================================
    // REGISTER ESP-NOW CALLBACK
    // =================================================

    esp_now_register_recv_cb(
        onDataReceive
    );


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
        qrcode.begin(
            &Serial2,
            UNIT_QRCODE_UART_BAUD,
            4,
            5
        )
    ) {

        qrReady =
            true;


        qrcode.setTriggerMode(
            AUTO_SCAN_MODE
        );
    }

    else {

        qrReady =
            false;


        rgbLedWrite(
            RGB_LED_PIN,
            0,
            0,
            0
        );
    }


    // =================================================
    // STARTUP LED
    // =================================================

    rgbLedWrite(
        RGB_LED_PIN,
        RGB_BRIGHTNESS,
        0,
        0
    );


    delay(300);


    rgbLedWrite(
        RGB_LED_PIN,
        RGB_BRIGHTNESS,
        RGB_BRIGHTNESS,
        0
    );


    delay(400);


    rgbLedWrite(
        RGB_LED_PIN,
        0,
        100,
        100
    );


    delay(500);


    // =================================================
    // LED OFF
    // =================================================

    rgbLedWrite(
        RGB_LED_PIN,
        0,
        0,
        0
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
        qrReady &&
        qrcode.available()
    ) {


        // ---------------------------------------------
        // Lấy QR từ scanner
        // ---------------------------------------------

        String qrData =
            qrcode.getDecodeData();


        // ---------------------------------------------
        // Xóa khoảng trắng đầu/cuối
        // ---------------------------------------------

        qrData.trim();


        // ---------------------------------------------
        // Kiểm tra QR
        // ---------------------------------------------

        if (
            isValidQR(qrData)
        ) {


            // =========================================
            // QR MỚI
            // =========================================

            if (
                qrData != lastProcessedCode
            ) {


                // -------------------------------------
                // Lưu QR đã xử lý
                // -------------------------------------

                lastProcessedCode =
                    qrData;


                // -------------------------------------
                // Cập nhật thời gian
                // -------------------------------------

                lastSeenTime =
                    currentTime;


                // -------------------------------------
                // Thêm ST4_
                // -------------------------------------

                String fullCode =
                    station +
                    qrData;


                // -------------------------------------
                // Đưa vào Queue
                // -------------------------------------

                createQREvent(
                    fullCode
                );
            }


            // =========================================
            // QR CŨ
            // =========================================

            else {

                lastSeenTime =
                    currentTime;
            }
        }
    }


    // =================================================
    // RESET QR
    //
    // Sau 1.5 giây không còn thấy QR
    // cho phép quét lại cùng mã
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


    // =================================================
    // LOOP DELAY
    // =================================================

    delay(5);
}