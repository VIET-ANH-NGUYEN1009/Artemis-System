#include <Arduino.h>
#include <M5UnitQRCode.h>

#include <WiFi.h>
#include <esp_wifi.h>


// =====================================================
// QR
// =====================================================

M5UnitQRCodeUART qrcode;


// =====================================================
// ST4 SOFTAP
// =====================================================

const char *AP_SSID =
    "ARTEMIS_L1";

const char *AP_PASSWORD =
    "12345678";


// =====================================================
// ST4 TCP SERVER
// =====================================================

IPAddress ST4_IP(
    192,
    168,
    4,
    1
);

#define TCP_PORT 4210


WiFiClient tcpClient;


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
        "Connecting to ST4 SoftAP..."
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
            ST4_IP,
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
// SEND ONLY QR
// =====================================================

bool sendQRToST4(
    String qrCode
)
{
    // =================================================
    // TCP CHECK
    // =================================================

    if (
        !tcpClient.connected()
    )
    {
        if (
            !connectTCP()
        )
        {
            return false;
        }
    }


    // =================================================
    // GỬI CHỈ QR
    // =================================================

    size_t sent =
        tcpClient.print(
            qrCode
        );


    // =================================================
    // KÝ TỰ KẾT THÚC
    // ST4 dùng readStringUntil('\n')
    // =================================================

    tcpClient.print(
        "\n"
    );


    // =================================================
    // CHECK
    // =================================================

    if (
        sent ==
        qrCode.length()
    )
    {
        Serial.print(
            "QR SEND OK: "
        );

        Serial.println(
            qrCode
        );


        return true;
    }


    Serial.println(
        "QR SEND FAIL"
    );


    tcpClient.stop();


    return false;
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
    // CONNECT ST4 SOFTAP
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

    if (
        qrcode.available()
    )
    {
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
    )
    {
        // =================================================
        // CHỐNG QR TRÙNG
        // =================================================

        if (
            bufferData !=
            lastProcessedCode
        )
        {
            lastProcessedCode =
                bufferData;


            // =================================================
            // TẠO QR
            // =================================================

            String fullCode =
                station +
                bufferData;


            // =================================================
            // GỬI CHỈ QR
            // =================================================

            bool sent =
                sendQRToST4(
                    fullCode
                );


            // =================================================
            // LED
            // =================================================

            if (
                sent
            )
            {
                // XANH = GỬI OK

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
            else
            {
                // ĐỎ = GỬI FAIL

                rgbLedWrite(
                    RGB_LED_PIN,
                    RGB_BRIGHTNESS,
                    0,
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
        }


        // =================================================
        // CLEAR BUFFER
        // =================================================

        bufferData =
            "";
    }


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


    delay(10);
}