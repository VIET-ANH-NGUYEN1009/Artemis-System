#include <M5UnitQRCode.h>

M5UnitQRCodeUART qrcode;

String bufferData = "";
unsigned long lastDataReceivedTime = 0;
const unsigned long GATHER_TIMEOUT = 150; 
const String station = "ST2_"; 
String lastProcessedCode = "";           
unsigned long lastSeenTime = 0;         
const unsigned long RESET_INTERVAL = 1500; 
#define RGB_LED_PIN 27   // Chân GPIO của LED RGB mặc định trên ESP32-C5 DevKit
#define RGB_BRIGHTNESS 100 // Độ sáng tối đa của đèn (Giá trị từ 0 đến 255)
void setup() {
    Serial.begin(115200);

   
    Serial2.begin(115200, SERIAL_8N1, 16, 17);

    if (!qrcode.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 4, 5)) {
        //Serial.println("QRCode Init Fail");
        rgbLedWrite(RGB_LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, 0); 

        while (1);
    }

    qrcode.setTriggerMode(AUTO_SCAN_MODE); 
    //Serial.println("Auto Scan Mode Ready");
    rgbLedWrite(RGB_LED_PIN, RGB_BRIGHTNESS, 0, 0); 
    delay(500);
    rgbLedWrite(RGB_LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, 0); 
    delay(500);
    Serial.println("LED: MÀU XANH LÁ");
    rgbLedWrite(RGB_LED_PIN, 0, RGB_BRIGHTNESS, 0); 
    delay(500);
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0); 
}

void loop() {
    unsigned long currentTime = millis();

    if (qrcode.available()) {
        bufferData += qrcode.getDecodeData();
        lastDataReceivedTime = currentTime; 
        lastSeenTime = currentTime;         
    }

    if (bufferData != "" && (currentTime - lastDataReceivedTime >= GATHER_TIMEOUT)) {
        
        
        if (bufferData != lastProcessedCode) {
            lastProcessedCode = bufferData; 

            
            //Serial.print("QR Data: ");
            Serial.println(station + bufferData);
            rgbLedWrite(RGB_LED_PIN, 0, RGB_BRIGHTNESS, 0); 
            delay(1000);
            rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
        }
        bufferData = ""; 
    }

    
    if (lastProcessedCode != "" && (currentTime - lastSeenTime >= RESET_INTERVAL)) {
        lastProcessedCode = ""; 
        delay(10); 
        
    }

    delay(10); 
}
