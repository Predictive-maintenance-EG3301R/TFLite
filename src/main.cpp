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

#define ROW_SIZE 24 // 20 for 1d array, 24 for 2d array
#define DIM_RGB_BRIGHTNESS 10

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "cnn_model_fullint_vibeonly.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include <secrets.h>

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info);

// Create a memory pool for the nodes in the network
constexpr int CNN_tensor_pool_size = 60 * 1024;
alignas(16) uint8_t CNN_tensor_pool[CNN_tensor_pool_size];

// Define the model to be used
const tflite::Model *CNN_model;

// Define the interpreter
tflite::MicroInterpreter *CNN_interpreter;

// For editing Google sheets
volatile unsigned long rowNumber = 1;
int lastRow = 620000;
int numConnection = 0;

// Input/Output nodes for the network
TfLiteTensor *CNN_input;
TfLiteTensor *CNN_output;

// Create vector to store value read in
// For 1D
// std::vector<float> accelXVecVert;
// std::vector<float> accelYVecVert;
// std::vector<float> accelZVecVert;
// std::vector<float> accelXVecHori;
// std::vector<float> accelYVecHori;
// std::vector<float> accelZVecHori;

// For 2D
// std::vector<std::vector<float>> accelXVecVert(4, std::vector<float>(24));
// std::vector<std::vector<float>> accelYVecVert(4, std::vector<float>(24));
// std::vector<std::vector<float>> accelZVecVert(4, std::vector<float>(24));
// std::vector<std::vector<float>> accelXVecHori(4, std::vector<float>(24));
// std::vector<std::vector<float>> accelYVecHori(4, std::vector<float>(24));
// std::vector<std::vector<float>> accelZVecHori(4, std::vector<float>(24));

float accelXVecVert[1000];
float accelYVecVert[1000];
float accelZVecVert[1000];
float accelXVecHori[1000];
float accelYVecHori[1000];
float accelZVecHori[1000];

// For normalizing the accelerometer data
float maxAccelXVert = -10e9;
float minAccelXVert = 10e9;
float maxAccelYVert = -10e9;
float minAccelYVert = 10e9;
float maxAccelZVert = -10e9;
float minAccelZVert = 10e9;
float maxAccelXHori = -10e9;
float minAccelXHori = 10e9;
float maxAccelYHori = -10e9;
float minAccelYHori = 10e9;
float maxAccelZHori = -10e9;
float minAccelZHori = 10e9;

int num_healthy = 0;
int num_cavitation = 0;
int num_loosebase = 0;

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info)
{
	if (info.status == token_status_error)
	{
		GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
		GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
		Serial.println("Restarting ESP32...");
		ESP.restart();
	}
	else
	{
		GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
	}
}

// Set up the ESP32's environment.
void setup()
{
	// Start serial at 115200 baud
	Serial.begin(115200);

	// Use to easily indicate if WiFi is connected
	pinMode(RGB_BUILTIN, OUTPUT);
	digitalWrite(RGB_BUILTIN, LOW);

	// ********************** Loading of CNN Model ************************
	// Load the AR model
	Serial.println("Loading Tensorflow model....");
	CNN_model = tflite::GetModel(cnn_model_fullint_vibeonly_tflite);
	Serial.println("CNN model loaded!");

	// Define ops resolver and error reporting
	static tflite::AllOpsResolver CNN_resolver;
	Serial.println("Resolver loaded!");

	static tflite::ErrorReporter *CNN_error_reporter;
	static tflite::MicroErrorReporter CNN_micro_error;
	CNN_error_reporter = &CNN_micro_error;
	Serial.println("Error reporter loaded!");

	// Instantiate the interpreter
	static tflite::MicroInterpreter CNN_static_interpreter(
		CNN_model, CNN_resolver, CNN_tensor_pool, CNN_tensor_pool_size, CNN_error_reporter);

	CNN_interpreter = &CNN_static_interpreter;
	Serial.println("Interpreter loaded!");

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if (CNN_interpreter->AllocateTensors() != kTfLiteOk)
	{
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
					  CNN_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	CNN_input = CNN_interpreter->input(0);
	CNN_output = CNN_interpreter->output(0);
	Serial.println("Input and output nodes loaded!");

	// ********************** Google Sheets portion ************************
	GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);
	WiFi.setAutoReconnect(true);

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	Serial.print("Connecting to Wi-Fi");
	unsigned long ms = millis();
	while (WiFi.status() != WL_CONNECTED)
	{
		if (numConnection >= 15)
		{
			Serial.println("Failed to connect to Wi-Fi");
			ESP.restart();
		}
		Serial.print(".");
		numConnection++;
		delay(300);
	}

	// Change RGB LED to blue to indicate WiFi connection
	neopixelWrite(RGB_BUILTIN, 0, 0, DIM_RGB_BRIGHTNESS);

	Serial.println();
	Serial.print("Connected with IP: ");
	Serial.println(WiFi.localIP());
	Serial.println();

	// Set the callback for Google API access token generation status (for debug only)
	GSheet.setTokenCallback(tokenStatusCallback);

	// Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
	GSheet.setPrerefreshSeconds(10 * 60);

	// Begin the access token generation for Google API authentication
	GSheet.begin(TESTING_CLIENT_EMAIL, TESTING_PROJECT_ID, GOOGLESHEETS_TESTING_PRIVATE_KEY);
}

void loop()
{
	// Call ready() repeatedly in loop for authentication checking and processing
	bool ready = GSheet.ready();

	// bool ready = 1;
	if (ready && rowNumber < lastRow)
	{

		for (int a = 0; a < 40; a++) // 5 for 1d array, 4 for 2d array
		{
			FirebaseJson response;
			FirebaseJsonData data;
			FirebaseJsonArray dataArr;
			// Instead of using FirebaseJson for response, you can use String for response to the functions
			// especially in low memory device that deserializing large JSON response may be failed as in ESP8266
			Serial.printf("Row number: %d\n", rowNumber);
			String startCol = "Sheet1!B";
			String endCol = ":G";
			startCol.concat(String(rowNumber));
			endCol.concat(String(rowNumber + ROW_SIZE - 1)); // +19 for 1d array, +23 for 2d array
			startCol.concat(endCol);

			rowNumber += ROW_SIZE;
			String rangeToRead = startCol;

			Serial.println("\nGet spreadsheet values from range...");
			Serial.println("---------------------------------------------------------------");

			// For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/get

			bool success = GSheet.values.get(&response /* returned response */, TESTSPREADSHEET_ID /* spreadsheet Id to read */, rangeToRead /* range to read */);

			// response.toString(Serial, true);
			Serial.println();
			Serial.println("Received data, processing now...");

			for (int j = 0; j < ROW_SIZE; j++)
			{
				String valueRange = "values/[";
				valueRange.concat(String(j));
				valueRange.concat("]");

				response.get(data, valueRange);
				data.get<FirebaseJsonArray>(dataArr);

				// For 1D
				// dataArr.get(data, 0);
				// accelXVecVert.push_back(data.to<String>().toFloat());
				// dataArr.get(data, 1);
				// accelYVecVert.push_back(data.to<String>().toFloat());
				// dataArr.get(data, 2);
				// accelZVecVert.push_back(data.to<String>().toFloat());
				// dataArr.get(data, 3);
				// accelXVecHori.push_back(data.to<String>().toFloat());
				// dataArr.get(data, 4);
				// accelYVecHori.push_back(data.to<String>().toFloat());
				// dataArr.get(data, 5);
				// accelZVecHori.push_back(data.to<String>().toFloat());

				// For 2D
				dataArr.get(data, 0);
				accelXVecVert[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				// Get max and min values for XVert
				if (accelXVecVert[(a * ROW_SIZE) + j] > maxAccelXVert)
				{
					maxAccelXVert = accelXVecVert[(a * ROW_SIZE) + j];
				}
				if (accelXVecVert[(a * ROW_SIZE) + j] < minAccelXVert)
				{
					minAccelXVert = accelXVecVert[(a * ROW_SIZE) + j];
				}

				// Get max and min values for YVert
				dataArr.get(data, 1);
				accelYVecVert[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				if (accelYVecVert[(a * ROW_SIZE) + j] > maxAccelYVert)
				{
					maxAccelYVert = accelYVecVert[(a * ROW_SIZE) + j];
				}
				if (accelYVecVert[(a * ROW_SIZE) + j] < minAccelYVert)
				{
					minAccelYVert = accelYVecVert[(a * ROW_SIZE) + j];
				}

				// Get max and min values for ZVert
				dataArr.get(data, 2);
				accelZVecVert[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				if (accelZVecVert[(a * ROW_SIZE) + j] > maxAccelZVert)
				{
					maxAccelZVert = accelZVecVert[(a * ROW_SIZE) + j];
				}
				if (accelZVecVert[(a * ROW_SIZE) + j] < minAccelZVert)
				{
					minAccelZVert = accelZVecVert[(a * ROW_SIZE) + j];
				}

				// Get max and min values for XHori
				dataArr.get(data, 3);
				accelXVecHori[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				if (accelXVecHori[(a * ROW_SIZE) + j] > maxAccelXHori)
				{
					maxAccelXHori = accelXVecHori[(a * ROW_SIZE) + j];
				}
				if (accelXVecHori[(a * ROW_SIZE) + j] < minAccelXHori)
				{
					minAccelXHori = accelXVecHori[(a * ROW_SIZE) + j];
				}

				// Get max and min values for YHori
				dataArr.get(data, 4);
				accelYVecHori[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				if (accelYVecHori[(a * ROW_SIZE) + j] > maxAccelYHori)
				{
					maxAccelYHori = accelYVecHori[(a * ROW_SIZE) + j];
				}
				if (accelYVecHori[(a * ROW_SIZE) + j] < minAccelYHori)
				{
					minAccelYHori = accelYVecHori[(a * ROW_SIZE) + j];
				}

				// Get max and min values for ZHori
				dataArr.get(data, 5);
				accelZVecHori[(a * ROW_SIZE) + j] = data.to<String>().toFloat();
				if (accelZVecHori[(a * ROW_SIZE) + j] > maxAccelZHori)
				{
					maxAccelZHori = accelZVecHori[(a * ROW_SIZE) + j];
				}
				if (accelZVecHori[(a * ROW_SIZE) + j] < minAccelZHori)
				{
					minAccelZHori = accelZVecHori[(a * ROW_SIZE) + j];
				}
			}
		}

		Serial.println("Done processing data");
		// To check if user input is set correctly
		// for (int i = 0; i < INPUT_SIZE; i++)
		// {
		// 	Serial.printf("Input %d: %f\n", i, user_input[i]);
		// }
		Serial.println();
		Serial.println("---------------------------------------------------------------");
	}

	Serial.print("Free heap: ");
	Serial.println(ESP.getFreeHeap());

	// Normalize the data for 2D between -1 and 1
	for (int i = 0; i < 24 * 40; i++)
	{
		accelXVecVert[i] = (accelXVecVert[i] - minAccelXVert) / (maxAccelXVert - minAccelXVert) * 2 - 1;
		accelYVecVert[i] = (accelYVecVert[i] - minAccelYVert) / (maxAccelYVert - minAccelYVert) * 2 - 1;
		accelZVecVert[i] = (accelZVecVert[i] - minAccelZVert) / (maxAccelZVert - minAccelZVert) * 2 - 1;
		accelXVecHori[i] = (accelXVecHori[i] - minAccelXHori) / (maxAccelXHori - minAccelXHori) * 2 - 1;
		accelYVecHori[i] = (accelYVecHori[i] - minAccelYHori) / (maxAccelYHori - minAccelYHori) * 2 - 1;
		accelZVecHori[i] = (accelZVecHori[i] - minAccelZHori) / (maxAccelZHori - minAccelZHori) * 2 - 1;
	}

	// Reset max and min values
	maxAccelXVert = -10e9;
	minAccelXVert = 10e9;
	maxAccelYVert = -10e9;
	minAccelYVert = 10e9;
	maxAccelZVert = -10e9;
	minAccelZVert = 10e9;
	maxAccelXHori = -10e9;
	minAccelXHori = 10e9;
	maxAccelYHori = -10e9;
	minAccelYHori = 10e9;
	maxAccelZHori = -10e9;
	minAccelZHori = 10e9;

	Serial.print("Combining all vector into 1");
	Serial.println();

	// For 1D
	// for (int i = 0; i < accelXVecVert.size(); i++)
	// {
	// 	accelData.push_back(accelXVecVert[i]);
	// }
	// for (int i = 0; i < accelYVecVert.size(); i++)
	// {
	// 	accelData.push_back(accelYVecVert[i]);
	// }
	// for (int i = 0; i < accelZVecVert.size(); i++)
	// {
	// 	accelData.push_back(accelZVecVert[i]);
	// }
	// for (int i = 0; i < accelXVecHori.size(); i++)
	// {
	// 	accelData.push_back(accelXVecHori[i]);
	// }
	// for (int i = 0; i < accelYVecHori.size(); i++)
	// {
	// 	accelData.push_back(accelYVecHori[i]);
	// }
	// for (int i = 0; i < accelZVecHori.size(); i++)
	// {
	// 	accelData.push_back(accelZVecHori[i]);
	// }

	// Serial.print("Size of combined vector: ");
	// Serial.println(accelData.size());

	// accelXVecHori.clear();
	// accelYVecHori.clear();
	// accelZVecHori.clear();
	// accelXVecVert.clear();
	// accelYVecVert.clear();
	// accelZVecVert.clear();

	// // Set input into interpreter
	// for (int i = 0; i < accelData.size(); i++)
	// {
	// 	CNN_input->data.f[i] = accelData[i];
	// }

	// For 2D
	for (int a = 0; a < 10; a++)
	{

		// Only for 1D
		// accelXVecHori.clear();
		// accelYVecHori.clear();
		// accelZVecHori.clear();
		// accelXVecVert.clear();
		// accelYVecVert.clear();
		// accelZVecVert.clear();

		// Set input into interpreter
		for (int i = 0; i < 96; i++)
		{
			CNN_input->data.f[i] = accelXVecVert[(a * 96) + i];
			CNN_input->data.f[i+96] = accelYVecVert[(a * 96) + i];
			CNN_input->data.f[i+96*2] = accelZVecVert[(a * 96) + i];
			CNN_input->data.f[i+96*3] = accelXVecHori[(a * 96) + i];
			CNN_input->data.f[i+96*4] = accelYVecHori[(a * 96) + i];
			CNN_input->data.f[i+96*5] = accelZVecHori[(a * 96) + i];
		}

		Serial.println("Running inference on inputted data...");

		// Run inference on the input data
		if (CNN_interpreter->Invoke() != kTfLiteOk)
		{
			Serial.println("There was an error invoking the interpreter!");
			return;
		}

		Serial.println("Inference complete!");

		// Print the output of the model.
		Serial.print("Output 1: ");
		Serial.println(CNN_output->data.f[0]);
		Serial.print("Output 2: ");
		Serial.println(CNN_output->data.f[1]);
		Serial.print("Output 3: ");
		Serial.println(CNN_output->data.f[2]);

		int predicted;
		if (CNN_output->data.f[0] > CNN_output->data.f[1] && CNN_output->data.f[0] > CNN_output->data.f[2])
		{
			predicted = 0;
			num_cavitation++;
			// Set RGB LED to red to indicate cavitation
			neopixelWrite(RGB_BUILTIN, DIM_RGB_BRIGHTNESS, 0, 0);
		}
		else if (CNN_output->data.f[1] > CNN_output->data.f[0] && CNN_output->data.f[1] > CNN_output->data.f[2])
		{
			predicted = 1;
			num_healthy++;
			// Set RGB LED to green to indicate healthy
			neopixelWrite(RGB_BUILTIN, 0, DIM_RGB_BRIGHTNESS, 0);
		}
		else
		{
			predicted = 2;
			num_loosebase++;
			// Set RGB LED to yellow to indicate loose base
			neopixelWrite(RGB_BUILTIN, DIM_RGB_BRIGHTNESS, DIM_RGB_BRIGHTNESS, 0);
		}
	}

	Serial.print("Free heap: ");
	Serial.println(ESP.getFreeHeap());
	// Serial.println("Delaying for 3 seconds...");
	// delay(3000);

	// For sending result to Google sheets
	if (ready)
	{
		FirebaseJson toUpdate1;
		FirebaseJson toUpdate2;
		FirebaseJson toUpdate3;
		FirebaseJson updateResponse;

		toUpdate1.add("range", "Sheet1!A5");
		toUpdate1.add("majorDimension", "ROWS");
		toUpdate1.set("values/[0]/[0]", num_healthy);

		toUpdate2.add("range", "Sheet1!A8");
		toUpdate2.add("majorDimension", "ROWS");
		toUpdate2.set("values/[0]/[0]", num_cavitation);

		toUpdate3.add("range", "Sheet1!A11");
		toUpdate3.add("majorDimension", "ROWS");
		toUpdate3.set("values/[0]/[0]", num_loosebase);

		FirebaseJsonArray toUpdateArr;
		toUpdateArr.add(toUpdate1);
		toUpdateArr.add(toUpdate2);
		toUpdateArr.add(toUpdate3);

		bool successUpdate = GSheet.values.batchUpdate(&updateResponse,	   /* returned response */
													   TESTSPREADSHEET_ID, /* spreadsheet Id to update */
													   &toUpdateArr		   /* range to update */
		);

		// updateResponse.toString(Serial, true);
		Serial.println();

		Serial.print("Free heap: ");
		Serial.println(ESP.getFreeHeap());
		delay(100);
	}
	else
	{
		esp_deep_sleep_start();
	}

	// // Set the input node to the user input
	// for (int i = 0; i < INPUT_SIZE; i++)
	// {
	// 	FFT_input->data.f[i] = user_input[i];
	// }

	// Serial.println("Running inference on inputted data...");

	// // Run inference on the input data
	// if (CNN_interpreter->Invoke() != kTfLiteOk)
	// {
	// 	Serial.println("There was an error invoking the interpreter!");
	// 	return;
	// }

	// Serial.println("Inference complete!");

	// // Print the output of the model.
	// Serial.print("Output 1: ");
	// Serial.println(FFT_output->data.f[0]);
	// Serial.print("Output 2: ");
	// Serial.println(FFT_output->data.f[1]);

	// int predicted;
	// if (FFT_output->data.f[0] > FFT_output->data.f[1])
	// {
	// 	predicted = 0;
	// }
	// else
	// {
	// 	predicted = 1;
	// }

	// // For sending result to Google sheets
	// if (ready)
	// {
	// 	FirebaseJson toUpdate;
	// 	FirebaseJson updateResponse;

	// 	String updateCol = "Sheet1!G";
	// 	updateCol.concat(String(rowNumber));

	// 	String updateRange = updateCol;

	// 	toUpdate.add("range", updateCol);
	// 	toUpdate.add("majorDimension", "ROWS");
	// 	toUpdate.set("values/[0]/[0]", predicted);

	// 	bool success0 = GSheet.values.update(&updateResponse /* returned response */, SPREADSHEET_ID /* spreadsheet Id to update */, updateRange /* range to update */, &toUpdate /* data to update */);
	// 	updateResponse.toString(Serial, true);
	// 	Serial.println();

	// 	Serial.print("Free heap: ");
	// 	Serial.println(ESP.getFreeHeap());
	// }

	// Serial.println("Waiting 1 seconds before next inference...");
	// delay(1000);

	// if (rowNumber >= lastRow)
	// {
	// 	Serial.println("Done with testing, putting ESP32 to deepsleep...");

	// 	ESP.deepSleep(UINT32_MAX);
	// }
}