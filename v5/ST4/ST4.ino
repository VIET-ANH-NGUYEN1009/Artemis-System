#include <Arduino.h>
#include <M5UnitQRCode.h>

#include <WiFi.h>
#include <esp_wifi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>


// =====================================================
// QR CODE
// =====================================================

M5UnitQRCodeUART qrcode;


// =====================================================
// SOFTAP CONFIG
// =====================================================

const char *AP_SSID     = "ARTEMIS_L1";
const char *AP_PASSWORD = "12345678";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);


// =====================================================
// TCP SERVER
// =====================================================

#define TCP_PORT 4210

WiFiServer tcpServer(TCP_PORT);

#define MAX_TCP_CLIENTS 8

WiFiClient tcpClients[MAX_TCP_CLIENTS];


// =====================================================
// STATION
// =====================================================

const String station = "ST4_";

#define MY_LINE_ID     1
#define MY_STATION_ID  4


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

#define RGB_LED_PIN    27
#define RGB_BRIGHTNESS 100


// =====================================================
// TCP PACKET
// =====================================================

typedef struct {

    uint8_t line_id;

    uint8_t station_id;

    uint32_t packet_id;

    char qr[100];

} TCP_Packet;


// =====================================================
// EVENT TYPE
// =====================================================

typedef enum {

    EVENT_QR = 0,

    EVENT_TCP = 1

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
// TCP RECEIVE BUFFER
// =====================================================
//
// TCP là stream.
// 1 packet có thể bị chia thành nhiều lần read().
//
// Vì TCP_Packet có kích thước cố định nên mỗi client
// có một buffer riêng.
// =====================================================

uint8_t tcpRxBuffer[
    MAX_TCP_CLIENTS
][
    sizeof(TCP_Packet)
];

size_t tcpRxLength[
    MAX_TCP_CLIENTS
];


// =====================================================
// QR VALIDATION
// =====================================================
//

// =====================================================

bool isValidQR(String data)
{
    if (data.length() < 2) {
        return false;
    }

    if (data.length() > 90) {
        return false;
    }

    for (int i = 0; i < data.length(); i++) {

        char c = data[i];

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
// CREATE TCP EVENT
// =====================================================

void createTCPEvent(
    TCP_Packet &packet
)
{
    // =================================================
    // KIỂM TRA LINE
    // =================================================

    if (
        packet.line_id !=
        MY_LINE_ID
    ) {

        return;
    }


    // =================================================
    // TẠO EVENT
    // =================================================

    Event event;

    event.type =
        EVENT_TCP;

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
// PROCESS TCP CLIENT
// =====================================================

void processTCPClient(
    WiFiClient &client,
    int clientIndex
)
{
    if (
        !client ||
        !client.connected()
    ) {

        return;
    }


    // =================================================
    // ĐỌC TCP DATA
    // =================================================

    while (
        client.available()
    ) {

        uint8_t temp[64];


        int availableBytes =
            client.available();


        int readSize =
            availableBytes;


        if (
            readSize >
            sizeof(temp)
        ) {

            readSize =
                sizeof(temp);
        }


        int received =
            client.read(
                temp,
                readSize
            );


        if (
            received <= 0
        ) {

            break;
        }


        // =================================================
        // KIỂM TRA OVERFLOW
        // =================================================

        if (
            tcpRxLength[clientIndex] +
            received >
            sizeof(TCP_Packet)
        ) {

            tcpRxLength[clientIndex] =
                0;

            continue;
        }


        // =================================================
        // COPY DATA
        // =================================================

        memcpy(

            tcpRxBuffer[clientIndex] +
                tcpRxLength[clientIndex],

            temp,

            received
        );


        tcpRxLength[clientIndex] +=
            received;


        // =================================================
        // ĐỦ 1 PACKET
        // =================================================

        if (
            tcpRxLength[clientIndex] ==
            sizeof(TCP_Packet)
        ) {

            TCP_Packet packet;


            memcpy(

                &packet,

                tcpRxBuffer[clientIndex],

                sizeof(TCP_Packet)
            );


            // =============================================
            // RESET BUFFER
            // =============================================

            tcpRxLength[clientIndex] =
                0;


            // =============================================
            // ĐẢM BẢO STRING KẾT THÚC
            // =============================================

            packet.qr[
                sizeof(packet.qr) - 1
            ] = '\0';


            // =============================================
            // TẠO EVENT
            // =============================================

            createTCPEvent(
                packet
            );
        }
    }
}


// =====================================================
// CHECK TCP CONNECTIONS
// =====================================================

void checkTCPConnections()
{
    // =================================================
    // NHẬN CLIENT MỚI
    // =================================================

    WiFiClient newClient =
        tcpServer.accept();


    if (
        newClient
    ) {

        bool accepted =
            false;


        for (
            int i = 0;
            i < MAX_TCP_CLIENTS;
            i++
        ) {

            if (
                !tcpClients[i] ||
                !tcpClients[i].connected()
            ) {

                if (
                    tcpClients[i]
                ) {

                    tcpClients[i].stop();
                }


                tcpClients[i] =
                    newClient;


                tcpRxLength[i] =
                    0;


                accepted =
                    true;


                break;
            }
        }


        // =================================================
        // ĐÃ ĐỦ CLIENT
        // =================================================

        if (
            !accepted
        ) {

            newClient.stop();
        }
    }


    // =================================================
    // XỬ LÝ CLIENT
    // =================================================

    for (
        int i = 0;
        i < MAX_TCP_CLIENTS;
        i++
    ) {

        if (
            tcpClients[i] &&
            tcpClients[i].connected()
        ) {

            processTCPClient(
                tcpClients[i],
                i
            );
        }

        else {

            if (
                tcpClients[i]
            ) {

                tcpClients[i].stop();
            }


            tcpRxLength[i] =
                0;
        }
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
                event.type ==
                EVENT_QR
            ) {

                Serial.println(
                    event.qr
                );
            }


            // =========================================
            // TCP QR
            // =========================================

            else if (
                event.type ==
                EVENT_TCP
            ) {

                // -------------------------------------
                // KIỂM TRA LINE
                // -------------------------------------

                if (
                    event.line_id !=
                    MY_LINE_ID
                ) {

                    continue;
                }


                // -------------------------------------
                // CHỈ NHẬN QR BẮT ĐẦU BẰNG ST
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
                // GỬI QR QUA UART
                // -------------------------------------.
                

                Serial.println(
                    event.qr
                );
            }
        }
    }
}


// =====================================================
// CREATE LOCAL QR EVENT
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
    // LOCAL QR
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
// START SOFTAP 5 GHz
// =====================================================

bool startSoftAP5GHz()
{
    // =================================================
    // WIFI AP MODE
    // =================================================

    WiFi.mode(
        WIFI_AP
    );


    // =================================================
    // ÉP 5 GHz ONLY
    // =================================================

    esp_err_t result =
        esp_wifi_set_band_mode(
            WIFI_BAND_MODE_5G_ONLY
        );


    if (
        result != ESP_OK
    ) {

        return false;
    }


    // =================================================
    // CONFIG IP
    // =================================================

    if (
        !WiFi.softAPConfig(
            AP_IP,
            AP_GATEWAY,
            AP_SUBNET
        )
    ) {

        return false;
    }


    // =================================================
    // START SOFTAP
    // =================================================

    bool started =
        WiFi.softAP(
            AP_SSID,
            AP_PASSWORD,
            36
        );


    if (
        !started
    ) {

        return false;
    }


    return true;
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
    // =================================================
    // SERIAL
    //
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
    // START 5 GHz SOFTAP
    // =================================================

    if (
        !startSoftAP5GHz()
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
    // TCP SERVER
    // =================================================

    tcpServer.begin();

    tcpServer.setNoDelay(
        true
    );


    // =================================================
    // CLEAR TCP BUFFER
    // =================================================

    for (
        int i = 0;
        i < MAX_TCP_CLIENTS;
        i++
    ) {

        tcpRxLength[i] =
            0;
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
    // TCP
    // =================================================

    checkTCPConnections();


    // =================================================
    // QR SCANNER
    // =================================================

    if (
        qrReady &&
        qrcode.available()
    ) {

        // ---------------------------------------------
        // LẤY QR
        // ---------------------------------------------

        String qrData =
            qrcode.getDecodeData();


        // ---------------------------------------------
        // XÓA SPACE
        // ---------------------------------------------

        qrData.trim();




        if (
            isValidQR(
                qrData
            )
        ) {

            // =========================================
            // QR MỚI
            // =========================================

            if (
                qrData !=
                lastProcessedCode
            ) {

                // -------------------------------------
                // LƯU QR
                // -------------------------------------

                lastProcessedCode =
                    qrData;


                // -------------------------------------
                // CẬP NHẬT TIME
                // -------------------------------------

                lastSeenTime =
                    currentTime;


                String fullCode =
                    station +
                    qrData;


                // -------------------------------------
                // QUEUE
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

    delay(50);
}