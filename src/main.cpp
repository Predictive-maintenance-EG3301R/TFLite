/*=============================================================================
TensorFlow Lite Platformio Example

Author: Wezley Sherman
Referenced Authors: The TensorFlow Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// #include "tensorflow/lite/micro/all_ops_resolver.h"
// #include "tensorflow/lite/micro/micro_error_reporter.h"
// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/schema/schema_generated.h"
// #include "CNN_1D_model.h"

#include <WiFi.h>
#include <Wire.h>
// #include <DFRobot_LIS2DH12.h>
#include <AWS_IOT.h>
#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>

const char *ssid = "router-wifi-2098"; // Write your SSID
const char *password = "govtech123";   // Write your password

AWS_IOT aws;

#define DIM_RGB_BRIGHTNESS 10

// Create vector to store value read in
float accelXVecVert[100];
float accelYVecVert[100];
float accelZVecVert[100];
float accelXVecHori[100];
float accelYVecHori[100];
float accelZVecHori[100];
int num_healthy = 0;
int num_cavitation = 0;
int num_loosebase = 0;
int currSamples = 0;

// Set up the ESP32's environment.
void setup()
{
	Wire.begin();
	Serial.begin(115200);
	Serial.println("Initializing....");
	delay(100);

	// Use to easily indicate if WiFi is connected
	pinMode(RGB_BUILTIN, OUTPUT);
	digitalWrite(RGB_BUILTIN, LOW);

	// while (LIS.init(LIS2DH12_RANGE_16GA) == -1)
	// { // Equipment connection exception or I2C address error
	// 	Serial.println("No I2C devices found");
	// 	delay(1000);
	// }

	int num_attempts = 0;
	WiFi.begin(ssid, password);
	Serial.println("Connecting to WiFi...");
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(300);

		if (num_attempts >= 15)
		{
			Serial.print("Connection Failed! Restarting...");
			ESP.restart();
		}

		num_attempts++;
	}

	// Turn on Blue LED to indicate WiFi successfully connected
	neopixelWrite(RGB_BUILTIN, 0, 0, DIM_RGB_BRIGHTNESS);

	Serial.println("Starting connection with AWS");
	if (aws.connect(AWS_IOT_HOST, AWS_IOT_CLIENT_ID) == 0)
	{
		Serial.println("Connected to AWS!");
		neopixelWrite(RGB_BUILTIN, 0, DIM_RGB_BRIGHTNESS, 0);
	}
	else
	{
		Serial.println("Connection Failed! Check AWS HOST and Client ID");
		delay(1000);
	}
	Serial.println("Setup Complete!");
}

void loop()
{
	Serial.println("Collecting Data...");
	// Set LED to green to indicate data collection
	neopixelWrite(RGB_BUILTIN, 0, DIM_RGB_BRIGHTNESS, 0);

	int16_t x, y, z;
	x = esp_random() % esp_random();
	y = esp_random() % esp_random();
	z = esp_random() % esp_random();

	String message = "Acceleration: ";
	message += "x: " + String(x) + " mg, ";
	message += "y: " + String(y) + " mg, ";
	message += "z: " + String(z) + " mg";

	char payload[100]; // Adjust the size as needed
	message.toCharArray(payload, sizeof(payload));

	// Publish the text message to AWS IoT Core
	Serial.print("Publishing Acceleration Data: ");
	Serial.println(payload);

	int num_attempts = 0;
	while (aws.publish(AWS_IOT_MQTT_TOPIC, payload) != 0)
	{
		Serial.print(".");
		delay(300);

		if (num_attempts >= 15)
		{
			Serial.print("Publish Failed! Restarting...");
			ESP.restart();
		}

		num_attempts++;
	}

	Serial.println("Publish Success!");
	// Turn on White LED to indicate message successfully sent
	neopixelWrite(RGB_BUILTIN, DIM_RGB_BRIGHTNESS, DIM_RGB_BRIGHTNESS, DIM_RGB_BRIGHTNESS);
	Serial.println("Waiting 30 seconds before sending next message");
	delay(1000 * 30);
}