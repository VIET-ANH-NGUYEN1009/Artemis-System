#include <Arduino.h>
#include <Wire.h>
#include "M5UnitQRCode.h"

// ===== QR Modules =====
M5UnitQRCodeI2C qrI2C;
M5UnitQRCodeUART qrUART;

#define QR_QUEUE_SIZE 10
#define MAX_QR_LEN 100
#define StationUART "ST1_"
#define StationI2C "ST2_"

QueueHandle_t qrQueue;

// ===== Struct =====
typedef struct {
  char qr[MAX_QR_LEN];
} QRItem;

// ===== Kiểm tra QR =====
bool isValidQR(const char* qr) {
  return qr[0] != '\0';
}

// ===== Task đọc QR =====
void QRTask(void *pvParameters) {

  char lastI2CQR[MAX_QR_LEN] = "";
  char lastUARTQR[MAX_QR_LEN] = "";
  char qrBuffer[MAX_QR_LEN];

  while (true) {

    // ===== I2C =====
    if (qrI2C.getDecodeReadyStatus() == 1) {

    uint16_t len = qrI2C.getDecodeLength();

    if (len > 0 && len < MAX_QR_LEN) {

        qrI2C.getDecodeData((uint8_t *)qrBuffer, len);
        qrBuffer[len] = '\0';

  
        String qrStr = String(StationI2C) + String(qrBuffer);

        if (strcmp(qrStr.c_str(), lastI2CQR) != 0) {

            QRItem item;
            strncpy(item.qr, qrStr.c_str(), MAX_QR_LEN - 1);
            item.qr[MAX_QR_LEN - 1] = '\0';

            xQueueSend(qrQueue, &item, portMAX_DELAY);

            strcpy(lastI2CQR, qrStr.c_str());
        }
    }
}

    // ===== UART =====
    if (qrUART.available()) {

      String qrStr = String(StationUART) + qrUART.getDecodeData();

      qrStr.toCharArray(qrBuffer, MAX_QR_LEN);

      if (isValidQR(qrBuffer) &&
          strcmp(qrBuffer, lastUARTQR) != 0) {

        QRItem item;
        strncpy(item.qr, qrBuffer, MAX_QR_LEN - 1);
        item.qr[MAX_QR_LEN - 1] = '\0';

        xQueueSend(qrQueue, &item, portMAX_DELAY);

        strcpy(lastUARTQR, qrBuffer);
      }
    }

    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ===== Task xử lý =====
void PrintTask(void *pvParameters) {

  QRItem item;

  while (true) {

    if (xQueueReceive(qrQueue, &item, portMAX_DELAY) == pdTRUE) {

     // Serial.print("QR: ");
      Serial.println(item.qr);
    }
  }
}

void setup() {

  Serial.begin(115200);
  delay(1000);

  //Serial.println("ESP32-C5 QR Reader");

  // ===== I2C =====
  Wire.begin(2, 3);

  if (!qrI2C.begin(&Wire, UNIT_QRCODE_ADDR, 2, 3, 100000U)) {

    Serial.println("I2C Init Fail");

  } else {

    qrI2C.setTriggerMode(AUTO_SCAN_MODE);
    //Serial.println("I2C Ready");
  }

  // ===== UART =====
  if (!qrUART.begin(&Serial2,
                    UNIT_QRCODE_UART_BAUD,
                    4,
                    5)) {

   // Serial.println("UART Init Fail");

  } else {

    qrUART.setTriggerMode(AUTO_SCAN_MODE);
    Serial.println("UART Ready");
  }

  // ===== Queue =====
  qrQueue = xQueueCreate(QR_QUEUE_SIZE, sizeof(QRItem));

  if (qrQueue == NULL) {

    Serial.println("Queue Create Fail");

    while (1);
  }

  // ===== Task =====
  xTaskCreate(QRTask,
              "QRTask",
              4096,
              NULL,
              2,
              NULL);

  xTaskCreate(PrintTask,
              "PrintTask",
              4096,
              NULL,
              1,
              NULL);
}

void loop() {
  
} 