#include <Arduino.h>
#include <M5UnitQRCode.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>


// =====================================================
// QR
// =====================================================

M5UnitQRCodeUART qrcode;


// =====================================================
// ST6 SOFTAP
// =====================================================

const char *AP_SSID =
    "ARTEMIS_L1";

const char *AP_PASSWORD =
    "12345678";


// =====================================================
// ST6 TCP SERVER
// =====================================================

IPAddress ST6_IP(
    192,
    168,
    4,
    1
);

#define TCP_PORT 4210


WiFiClient tcpClient;


// =====================================================
// TCP PACKET STRUCT (ĐỒNG BỘ VỚI ST6)
// =====================================================

typedef struct {

    uint8_t line_id;

    uint8_t station_id;

    uint32_t packet_id;

    char qr[100];

} TCP_Packet;


// =====================================================
// STATION
// =====================================================

const String station =
    "ST6_";


// =====================================================
// QR BUFFER
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
// CONNECT WIFI
// =====================================================

bool connectWiFi()
{
    Serial.println();
    Serial.println(
        "Connecting to ST6 SoftAP..."
    );

    Serial.print(
        "SSID: "
    );

    Serial.println(
        AP_SSID
    );


    WiFi.begin(
        AP_SSID,
        AP_PASSWORD
    );


    unsigned long startTime =
        millis();


    while (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        delay(500);

        Serial.print(
            "."
        );


        if (
            millis() -
            startTime >
            15000
        )
        {
            Serial.println();

            Serial.println(
                "WIFI CONNECT FAIL"
            );

            return false;
        }
    }


    Serial.println();

    Serial.println(
        "WIFI CONNECT OK"
    );


    // =================================================
    // WIFI INFO
    // =================================================

    Serial.print(
        "ST6 IP: "
    );

    Serial.println(
        WiFi.localIP()
    );


    Serial.print(
        "Gateway: "
    );

    Serial.println(
        WiFi.gatewayIP()
    );


    Serial.print(
        "Channel: "
    );

    Serial.println(
        WiFi.channel()
    );


    Serial.print(
        "MAC: "
    );

    Serial.println(
        WiFi.macAddress()
    );


    return true;
}


// =====================================================
// CONNECT TCP
// =====================================================

bool connectTCP()
{
    if (
        tcpClient.connected()
    )
    {
        return true;
    }


    Serial.println(
        "Connecting TCP..."
    );


    tcpClient.stop();


    if (
        tcpClient.connect(
            ST6_IP,
            TCP_PORT
        )
    )
    {
        tcpClient.setNoDelay(
            true
        );


        Serial.println(
            "TCP CONNECT OK"
        );


        return true;
    }


    Serial.println(
        "TCP CONNECT FAIL"
    );


    return false;
}


// =====================================================
// SEND QR BẰNG STRUCT TCP_PACKET
// =====================================================

bool sendQRToST6(const String& qrCode)
{
    if (!tcpClient.connected())
    {
        if (!connectTCP())
        {
            return false;
        }
    }

    TCP_Packet packet;

    memset(&packet, 0, sizeof(packet));

    packet.line_id    = 1;
    packet.station_id = 6;
    packet.packet_id  = millis();

    snprintf(
        packet.qr,
        sizeof(packet.qr),
        "%s",
        qrCode.c_str()
    );

    size_t sent = tcpClient.write(
        (const uint8_t*)&packet,
        sizeof(packet)
    );

    tcpClient.flush();

    if (sent != sizeof(packet))
    {
        Serial.println("TCP SEND FAIL");
        tcpClient.stop();
        return false;
    }

    Serial.print("SEND: ");
    Serial.println(packet.qr);

    return true;
}



// =====================================================
// SETUP
// =====================================================

void setup()
{
    // =================================================
    // SERIAL
    // =================================================

    Serial.begin(
        115200
    );


    delay(500);


    Serial.println();
    Serial.println(
        "================================"
    );
    Serial.println(
        " ST6 - SOFTAP 5GHz + TCP"
    );
    Serial.println(
        "================================"
    );


    // =================================================
    // QR UART
    // =================================================

    // qrcode.begin() sẽ tự cấu hình Serial2 lại theo
    // đúng chân RX/TX được truyền vào bên dưới.
    Serial2.begin(
        115200,
        SERIAL_8N1,
        16,
        17
    );


    // =================================================
    // QR INIT - THỰC HIỆN TRƯỚC WIFI/TCP
    // =====================================================

    if (
        !qrcode.begin(
            &Serial2,
            UNIT_QRCODE_UART_BAUD,
            4,
            5
        )
    )
    {
        Serial.println(
            "QR INIT FAIL"
        );


        rgbLedWrite(
            RGB_LED_PIN,
            RGB_BRIGHTNESS,
            RGB_BRIGHTNESS,
            0
        );


        while (1)
        {
            delay(100);
        }
    }


    qrcode.setTriggerMode(
        AUTO_SCAN_MODE
    );


    Serial.println(
        "QR READY"
    );


    // =================================================
    // WIFI STA
    // =================================================

    WiFi.mode(
        WIFI_STA
    );


    delay(100);


    // =================================================
    // 5 GHz ONLY
    // =================================================

    esp_err_t bandResult =
        esp_wifi_set_band_mode(
            WIFI_BAND_MODE_5G_ONLY
        );


    if (
        bandResult != ESP_OK
    )
    {
        Serial.print(
            "5GHz MODE FAIL: "
        );

        Serial.println(
            esp_err_to_name(
                bandResult
            )
        );


        rgbLedWrite(
            RGB_LED_PIN,
            RGB_BRIGHTNESS,
            RGB_BRIGHTNESS,
            0
        );


        while (1)
        {
            delay(100);
        }
    }


    Serial.println(
        "5GHz MODE OK"
    );


    // =================================================
    // CONNECT ST6 SOFTAP
    // =================================================

    if (
        !connectWiFi()
    )
    {
        rgbLedWrite(
            RGB_LED_PIN,
            RGB_BRIGHTNESS,
            RGB_BRIGHTNESS,
            0
        );


        while (1)
        {
            delay(100);
        }
    }


    // =================================================
    // TCP
    // =================================================

    if (
        !connectTCP()
    )
    {
        Serial.println(
            "TCP CONNECT FAIL"
        );

        Serial.println(
            "Will retry later..."
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


    // =================================================
    // READY
    // =================================================

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "ST6 READY"
    );

    Serial.println(
        "LINE    : 1"
    );

    Serial.println(
        "STATION : 6"
    );

    Serial.println(
        "BAND    : 5 GHz"
    );

    Serial.println(
        "TCP     : 192.168.4.1:4210"
    );

    Serial.println(
        "QR MODE : ONLY QR"
    );

    Serial.println(
        "================================"
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
    // WIFI CHECK
    // =================================================

    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        Serial.println(
            "WiFi disconnected!"
        );


        tcpClient.stop();


        if (
            connectWiFi()
        )
        {
            connectTCP();
        }


        delay(1000);

        return;
    }


    // =================================================
    // TCP CHECK
    // =================================================

    if (
        !tcpClient.connected()
    )
    {
        connectTCP();
    }


    // =================================================
    // QR SCANNER
    // =================================================

    if (qrcode.available())
{
    String qrCode = qrcode.getDecodeData();
    qrCode.trim();

    if (qrCode.length() > 0)
    {
        if (qrCode != lastProcessedCode)
        {
            lastProcessedCode = qrCode;
            lastSeenTime = currentTime;

            String fullCode = station + qrCode;

            sendQRToST6(fullCode);
        }
    }
}


    // =================================================
    // QR GATHER
    // =================================================

    // if (
    //     bufferData != "" &&
    //     (
    //         currentTime -
    //         lastDataReceivedTime
    //     ) >= GATHER_TIMEOUT
    // )
    // {
    //     // =================================================
    //     // CHỐNG QR TRÙNG
    //     // =================================================

    //     if (
    //         bufferData !=
    //         lastProcessedCode
    //     )
    //     {
    //         lastProcessedCode =
    //             bufferData;


    //         // =================================================
    //         // TẠO QR
    //         // =================================================

    //         String fullCode =
    // station +
    // bufferData +
    // "\r\n";

    //         // =================================================
    //         // GỬI DATA QUA TCP CHO ST6
    //         // =================================================

    //         bool sent =
    //             sendQRToST6(
    //                 fullCode
    //             );


            
    //         if (
    //             sent
    //         )
    //         {
    //             // XANH = GỬI OK

    //             rgbLedWrite(
    //                 RGB_LED_PIN,
    //                 0,
    //                 RGB_BRIGHTNESS,
    //                 0
    //             );


    //             delay(1000);


    //             rgbLedWrite(
    //                 RGB_LED_PIN,
    //                 0,
    //                 0,
    //                 0
    //             );
    //         }
    //         else
    //         {
    //             // ĐỎ = GỬI FAIL

    //             rgbLedWrite(
    //                 RGB_LED_PIN,
    //                 RGB_BRIGHTNESS,
    //                 0,
    //                 0
    //             );


    //             delay(1000);


    //             rgbLedWrite(
    //                 RGB_LED_PIN,
    //                 0,
    //                 0,
    //                 0
    //             );
    //         }
    //     }


    //     // =================================================
    //     // CLEAR BUFFER
    //     // =================================================

    //     bufferData =
    //         "";
    // }


    // =================================================
    // RESET QR DUPLICATE
    // =================================================

    if (
        lastProcessedCode != "" &&
        (
            currentTime -
            lastSeenTime
        ) >= RESET_INTERVAL
    )
    {
        lastProcessedCode =
            "";
    }


    delay(50);
}