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

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "FFT_model_data.h"
// #include "AR_model.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include <secrets.h>

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info);

// Create a memory pool for the nodes in the network
constexpr int FFT_tensor_pool_size = 2 * 1024;
alignas(16) uint8_t FFT_tensor_pool[FFT_tensor_pool_size];

// constexpr int AR_tensor_pool_size = 10 * 1024;
// alignas(16) uint8_t AR_tensor_pool[AR_tensor_pool_size];

// Define the model to be used
const tflite::Model* FFT_model;
// const tflite::Model* AR_model;

// Define the interpreter
tflite::MicroInterpreter* FFT_interpreter;
// tflite::MicroInterpreter* AR_interpreter;

// To load data to test FFT model
#define INPUT_SIZE 5
float user_input[INPUT_SIZE];

// For editing Google sheets
volatile unsigned long rowNumber = 0;

// Input/Output nodes for the network
TfLiteTensor* FFT_input;
TfLiteTensor* FFT_output;
// TfLiteTensor* AR_input;
// TfLiteTensor* AR_output;

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info) {
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
void setup() { 
	// Start serial at 115200 baud
	Serial.begin(115200);

	// ********************** Loading of FFT Model ************************
	// Load the FFT model
	Serial.println("Loading FFT Tensorflow model....");
	FFT_model = tflite::GetModel(FFT_model_quantized_tflite);
	Serial.println("FFT model loaded!");

	// Define ops resolver and error reporting
	static tflite::AllOpsResolver FFT_resolver;

	static tflite::ErrorReporter* FFT_error_reporter;
	static tflite::MicroErrorReporter FFT_micro_error;
	FFT_error_reporter = &FFT_micro_error;

	// Instantiate the interpreter 
	static tflite::MicroInterpreter FFT_static_interpreter(
		FFT_model, FFT_resolver, FFT_tensor_pool, FFT_tensor_pool_size, FFT_error_reporter
	);

	FFT_interpreter = &FFT_static_interpreter;

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if(FFT_interpreter->AllocateTensors() != kTfLiteOk) {
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
                             FFT_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	FFT_input = FFT_interpreter->input(0);
	FFT_output = FFT_interpreter->output(0);
	Serial.println("Input and output nodes loaded!");


	// ********************** Loading of AR Model ************************
	// Load the AR model
	// Serial.println("Loading Tensorflow model....");
	// AR_model = tflite::GetModel(AR_model_fullint_quantized_tflite);
	// Serial.println("AR model loaded!");

	// // Define ops resolver and error reporting
	// static tflite::AllOpsResolver AR_resolver;
	// Serial.println("Resolver loaded!");

	// static tflite::ErrorReporter* AR_error_reporter;
	// static tflite::MicroErrorReporter AR_micro_error;
	// AR_error_reporter = &AR_micro_error;
	// Serial.println("Error reporter loaded!");

	// // Instantiate the interpreter 
	// static tflite::MicroInterpreter AR_static_interpreter(
	// 	AR_model, AR_resolver, AR_tensor_pool, AR_tensor_pool_size, AR_error_reporter
	// );

	// AR_interpreter = &AR_static_interpreter;
	// Serial.println("Interpreter loaded!");

	// // Allocate the the model's tensors in the memory pool that was created.
	// Serial.println("Allocating tensors to memory pool");
	// if(AR_interpreter->AllocateTensors() != kTfLiteOk) {
	// 	Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
    //                          AR_model->version(), TFLITE_SCHEMA_VERSION);
	// 	Serial.println("There was an error allocating the memory...ooof");
	// 	return;
	// }

	// // Define input and output nodes
	// AR_input = AR_interpreter->input(0);
	// AR_output = AR_interpreter->output(0);
	// Serial.println("Input and output nodes loaded!");


	// ********************** Google Sheets portion ************************
	GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);
	WiFi.setAutoReconnect(true);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    unsigned long ms = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }

    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Set the callback for Google API access token generation status (for debug only)
    GSheet.setTokenCallback(tokenStatusCallback);

    // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
    GSheet.setPrerefreshSeconds(10 * 60);

    // Begin the access token generation for Google API authentication
    GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
}

// Wait for 5 serial inputs to be made available and parse them as floats


// Logic loop for taking user input and outputting the FFT
void loop() { 
	// Call ready() repeatedly in loop for authentication checking and processing
    bool ready = GSheet.ready();

	// bool ready = 1;
	if (ready) {
        FirebaseJson response;
        FirebaseJsonData data;
        FirebaseJsonArray dataArr;
        // Instead of using FirebaseJson for response, you can use String for response to the functions
        // especially in low memory device that deserializing large JSON response may be failed as in ESP8266
        rowNumber++;
        Serial.printf("Row number: %d\n", rowNumber);
        String startCol = "Sheet1!A";
        String endCol = ":F";
        startCol.concat(String(rowNumber));
        endCol.concat(String(rowNumber));
        startCol.concat(endCol);

        String rangeToRead = startCol;

        Serial.println("\nGet spreadsheet values from range...");
        Serial.println("---------------------------------------------------------------");

        // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/get

        bool success = GSheet.values.get(&response /* returned response */, SPREADSHEET_ID /* spreadsheet Id to read */, rangeToRead /* range to read */);

        response.toString(Serial, true);
        Serial.println();
        response.get(data, "values/[0]");
        data.get<FirebaseJsonArray>(dataArr);

        for (int i = 0; i < dataArr.size(); i++) {
            dataArr.get(data, i);

            //Print its value
            Serial.print("Array index: ");
            Serial.print(i);
            Serial.print(", type: ");
            Serial.print(data.type);
            Serial.print(", value: ");
            Serial.println(data.to<String>());

			if (i < INPUT_SIZE) {
				user_input[i] = data.to<String>().toFloat();
			}
        }

		// To check if user input is set correctly
		for (int i = 0; i < INPUT_SIZE; i++) {
			Serial.printf("Input %d: %f\n", i, user_input[i]);
		}

        
        Serial.println();
        Serial.println("---------------------------------------------------------------");
    }
	
	Serial.print("Free heap: ");
	Serial.println(ESP.getFreeHeap());

	// Wait for serial input to be made available and parse it as a float ----- Portion below is for mauual input
	// for (int i = 0; i < INPUT_SIZE; i++) {
	// 	while (Serial.available() == 0) {
	// 		// Wait for serial input to be made available
	// 		delay(10);
	// 	}
	// 	char buffer[10];
	// 	Serial.readBytesUntil('\n', buffer, 10);
	// 	user_input[i] = atof(buffer);
	// 	Serial.printf("Input %d: %f\n", i, user_input[i]);
	// 	Serial.flush();
	// }

	// Set the input node to the user input
	for (int i = 0; i < INPUT_SIZE; i++) {
		FFT_input->data.f[i] = user_input[i];
	}

	Serial.println("Running inference on inputted data...");

	// Run inference on the input data
	if(FFT_interpreter->Invoke() != kTfLiteOk) {
		Serial.println("There was an error invoking the interpreter!");
		return;
	}

	Serial.println("Inference complete!");

	// Print the output of the model.
	Serial.print("Output 1: ");
	Serial.println(FFT_output->data.f[0]);
	Serial.print("Output 2: ");
	Serial.println(FFT_output->data.f[1]);

	int predicted;
	if (FFT_output->data.f[0] > FFT_output->data.f[1]) {
		predicted = 0;
	} else {
		predicted = 1;
	}

	// For sending result to Google sheets
	if (ready) {
		FirebaseJson toUpdate;
		FirebaseJson updateResponse;


		String updateCol = "Sheet1!G";
		updateCol.concat(String(rowNumber));

		String updateRange = updateCol;

		toUpdate.add("range", updateCol);
		toUpdate.add("majorDimension", "ROWS");
		toUpdate.set("values/[0]/[0]", predicted);

		bool success0 = GSheet.values.update(&updateResponse /* returned response */, SPREADSHEET_ID /* spreadsheet Id to update */, updateRange /* range to update */, &toUpdate /* data to update */);
		updateResponse.toString(Serial, true);
		Serial.println();

		Serial.print("Free heap: ");
		Serial.println(ESP.getFreeHeap());
	}

	Serial.println("Waiting 1 seconds before next inference...");
	delay(1000);
}