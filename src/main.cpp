/**
   AWS S3 OTA Update
   Date: 14th June 2017
   Author: Arvind Ravulavaru <https://github.com/arvindr21>
   Purpose: Perform an OTA update from a bin located in Amazon S3 (HTTP Only)

   Upload:
   Step 1 : Download the sample bin file from the examples folder
   Step 2 : Upload it to your Amazon S3 account, in a bucket of your choice
   Step 3 : Once uploaded, inside S3, select the bin file >> More (button on top of the file list) >> Make Public
   Step 4 : You S3 URL => http://bucket-name.s3.ap-south-1.amazonaws.com/sketch-name.ino.bin
   Step 5 : Build the above URL and fire it either in your browser or curl it `curl -I -v http://bucket-name.ap-south-1.amazonaws.com/sketch-name.ino.bin` to validate the same
   Step 6:  Plug in your WIFI_SSID, Password, S3 Host and Bin file below

   Build & upload
   Step 1 : Menu > Sketch > Export Compiled Library. The bin file will be saved in the sketch folder (Menu > Sketch > Show Sketch folder)
   Step 2 : Upload bin to S3 and continue the above process

   // Check the bottom of this sketch for sample serial monitor log, during and after successful OTA Update
*/

#define BLYNK_TEMPLATE_ID "TMPLbMbSLqQv"
#define BLYNK_TEMPLATE_NAME "Norika Water Meter and Pressure Sensor"
#define BLYNK_AUTH_TOKEN "WJPexy_64oJzZqY9dcOe-nQKiEtQDSz6"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "AR_model.h"
#include "BlynkSimpleEsp32.h"
// #include "SparkFun_LIS2DH12.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include <secrets.h>
#include <vector>
#include <Update.h>

WiFiClient client;

// Blynk variables
#define OTA_VPIN V0
volatile bool OTAAvailable = false;

// Variables to validate response from S3
long contentLength = 0;
bool isValidContentType = false;

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(OTA_VPIN);
}

BLYNK_WRITE(OTA_VPIN) {
  OTAAvailable = param.asInt();
}

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

// OTA Logic 
void execOTA() {
  Serial.printf("Connecting to: %s\n", S3_HOST);
  // Connect to S3
  if (client.connect(S3_HOST, S3_PORT)) {
    // Connection Succeed.
    // Fecthing the bin
    Serial.printf("Fetching Bin: %s\n", String(S3_BIN));

    // Get the contents of the bin file
    String request = String("GET ");
    request += S3_BIN;
    request += " HTTP/1.1\r\nHost: ";
    request += S3_HOST;
    request += "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";

    client.print(request);

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        client.stop();
        return;
      }
    }

    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();

      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      if (line.startsWith("Content-Length: ")) {
        contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.printf("Got %s bytes from server\n", String(contentLength));
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.printf("Got %s payload\n", contentType.c_str());
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to S3 failed
    Serial.printf("Connection to %s failed. Please check your setup\n",String(S3_HOST));
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.printf("contentLength : %s, isValidContentType : %s\n", String(contentLength), String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      Blynk.virtualWrite(OTA_VPIN, 0); // Set OTA_VPIN to 0
      Serial.println("OTA in progress...");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        Serial.printf("Written : %s successfully\n", String(written));
      } else {
        Serial.printf("Written only : %s/%s. Retry?\n", String(written), String(contentLength));
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.printf("Error Occurred. Error #: %s\n", String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA
      Serial.println("Not enough space to begin OTA");
      client.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    client.flush();
  }
}

void setup() {
  //Begin Serial
  Serial.begin(115200);
  delay(10);

  Serial.printf("Connecting to %s\n", String(WIFI_SSID));

  // Connect to provided WIFI_SSID and WIFI_PASSWORD
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait for connection to establish
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); // Keep the serial monitor lit!
    delay(500);
  }

  // Connection Succeed
  Serial.println("");
  Serial.printf("Connected to %s\n", String(WIFI_SSID));

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD, "blynk.cloud", 8080);
  if (Blynk.connected()) {
    Serial.println("Blynk Connected");
    Blynk.syncVirtual(OTA_VPIN);
  } else {
    Serial.println("Blynk Not Connected");
  }

  // Execute OTA Update
  if (OTAAvailable) {
    Serial.println("OTA is available");
    execOTA();
  } else {
    Serial.println("OTA is not available");
  }
}

void loop() {
  Blynk.run();
}
