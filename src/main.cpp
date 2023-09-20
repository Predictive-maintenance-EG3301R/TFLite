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
#include "AR_model.h"
// #include "SparkFun_LIS2DH12.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include <secrets.h>
#include <vector>

using namespace std;

// Accelerometer variables
int acc_counter = 0;
int acc_timechecker = millis();
volatile bool isAccTimerTriggered = false; 	// For checking if timer triggered
int accSamplingRate = 1000;          		// Sampling rate in Hz, max is 1000Hz
hw_timer_t *acc_timer = NULL;         		// Timer object

// Offset values for the accelerometers
float offsetXVert = 0.0;
float offsetYVert = 0.0;
float offsetZVert = 0.0;
float offsetXHori = 0.0;
float offsetYHori = 0.0;
float offsetZHori = 0.0;
vector<float> accelXVecVert;
vector<float> accelYVecVert;
vector<float> accelZVecVert;
vector<float> accelXVecHori;
vector<float> accelYVecHori;
vector<float> accelZVecHori;

// For normalizing the accelerometer data
float maxAccelXVert = -10e9;
float minAccelXVert = 10e9;
float maxAccelYVert = -10e9;
float minAccelYVert = 10e9;
float maxAccelZVert = -10e9;
float minAccelZVert = 10e9;

// SPARKFUN_LIS2DH12 accelVert; // Create instance
// SPARKFUN_LIS2DH12 accelHori; // Create instance
Adafruit_MPU6050 mpu;

TwoWire I2Cone = TwoWire(0);

// Details for model to be tested
#define AR_INPUT_SIZE 		100

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info);

// Create a memory pool for the nodes in the network
constexpr int AR_tensor_pool_size = 10 * 1024;
alignas(16) uint8_t AR_tensor_pool[AR_tensor_pool_size];

// Define the model to be used
const tflite::Model* AR_model;

// Define the interpreter
tflite::MicroInterpreter* AR_interpreter;

#define GOOGLE_SHEETS_DELAY 0.25 	// Number of seconds between each API call to Google Sheets
									// API limit is 300 requests per minute per project for read and write separately
#define SHEET_NAME "Uploaded"	// Name of the sheet to be used

// Variables for AR model
float AR_user_input[AR_INPUT_SIZE];
float recon_data[AR_INPUT_SIZE];
double THRESHOLD = 0.005262835091655835;

// For editing Google sheets
volatile unsigned long currRowNumber = 0;
int lastRow = 10000;
int numConnection = 0;

// Input/Output nodes for the network
TfLiteTensor* AR_input;
TfLiteTensor* AR_output;

/**
 * @brief Timer used to collect data for FFT.
 */
void IRAM_ATTR onTimer() {
  isAccTimerTriggered = true; // Indicates that the interrupt has been entered since the last time its value was changed to false
}

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info) {
    if (info.status == token_status_error) {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
		Serial.println("Restarting ESP32...");
        ESP.restart();
    } else {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}

// For sending data to Google sheets
void sendGoogleSheets(vector<float> accelYVecVert, unsigned long currRowNumber, int lastRow, String sheetName) {
	FirebaseJson updateOutput;
	FirebaseJson updateCurrentRow;
	FirebaseJson updateResponse;

	String sheetRange = sheetName;
	sheetRange.concat("!C");
	sheetRange.concat(String(currRowNumber));
	sheetRange.concat(":CX");
	sheetRange.concat(String(currRowNumber));
	updateOutput.add("range", sheetRange);
 	updateOutput.add("majorDimension", "ROWS");

	for (int i = 0; i < 100; i++) {
		String updateRowCol = "values/[0]/[";
		updateRowCol.concat(String(i));
		updateRowCol.concat("]");

		updateOutput.set(updateRowCol, accelYVecVert[i]);
	}

	bool success = GSheet.values.update(&updateResponse /* returned response */, SPREADSHEET_ID /* spreadsheet Id to update */, sheetRange /* range to update */, &updateOutput /* data to update */);
	updateResponse.toString(Serial, true);
	Serial.println();

}
	


// Set up the ESP32's environment.
void setup() { 
	// Start serial at 115200 baud
	Serial.begin(115200);

	// Use to easily indicate if WiFi is connected
	pinMode(LED_BUILTIN, OUTPUT);

	// **************** Accelerometer setup ****************
	I2Cone.begin(21, 22);
  	Serial.println("I2Cone begin");
	I2Cone.setClock(1000000);
	delay(200);

	// Try to initialize!
	if (!mpu.begin()) {
		Serial.println("Failed to find MPU6050 chip");
		while (1) {
		delay(10);
		}
	}
	Serial.println("MPU6050 Found!");

	mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
	Serial.print("Accelerometer range set to: ");
	switch (mpu.getAccelerometerRange()) {
	case MPU6050_RANGE_2_G:
		Serial.println("+-2G");
		break;
	case MPU6050_RANGE_4_G:
		Serial.println("+-4G");
		break;
	case MPU6050_RANGE_8_G:
		Serial.println("+-8G");
		break;
	case MPU6050_RANGE_16_G:
		Serial.println("+-16G");
		break;
	}
	mpu.setGyroRange(MPU6050_RANGE_500_DEG);
	Serial.print("Gyro range set to: ");
	switch (mpu.getGyroRange()) {
	case MPU6050_RANGE_250_DEG:
		Serial.println("+- 250 deg/s");
		break;
	case MPU6050_RANGE_500_DEG:
		Serial.println("+- 500 deg/s");
		break;
	case MPU6050_RANGE_1000_DEG:
		Serial.println("+- 1000 deg/s");
		break;
	case MPU6050_RANGE_2000_DEG:
		Serial.println("+- 2000 deg/s");
		break;
	}

	mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
	Serial.print("Filter bandwidth set to: ");
	switch (mpu.getFilterBandwidth()) {
	case MPU6050_BAND_260_HZ:
		Serial.println("260 Hz");
		break;
	case MPU6050_BAND_184_HZ:
		Serial.println("184 Hz");
		break;
	case MPU6050_BAND_94_HZ:
		Serial.println("94 Hz");
		break;
	case MPU6050_BAND_44_HZ:
		Serial.println("44 Hz");
		break;
	case MPU6050_BAND_21_HZ:
		Serial.println("21 Hz");
		break;
	case MPU6050_BAND_10_HZ:
		Serial.println("10 Hz");
		break;
	case MPU6050_BAND_5_HZ:
		Serial.println("5 Hz");
		break;
	}

	Serial.println("");
	delay(100);

	// if (accelVert.begin(0x18, I2Cone) == false) {
	// 	Serial.println("Accelerometer 1 not detected. Check address jumper and wiring. Freezing...");
	// 	while (1)
	// 	;
	// }

	// if (accelHori.begin(0x19, I2Cone) == false) {
	// 	Serial.println("Accelerometer 2 not detected. Check address jumper and wiring. Freezing...");
	// 	while (1)
	// 	;
	// };

	// accelVert.setScale(LIS2DH12_2g);                              // Set full-scale range to 2g
	// accelVert.setMode(LIS2DH12_HR_12bit);                         // Set operating mode to low power
	// accelVert.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz

	// accelHori.setScale(LIS2DH12_2g);                              // Set full-scale range to 2g
	// accelHori.setMode(LIS2DH12_HR_12bit);                         // Set operating mode to low power
	// accelHori.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz

	// Serial.println("Calibrating accelerometer 1. Do not move the board.");
	// for (int i = 0; i < 1000; i++) {
	// 	offsetXVert += accelVert.getX();
	// 	offsetYVert += accelVert.getY();
	// 	offsetZVert += accelVert.getZ();
	// 	delay(2);
	// }

	// Serial.println("Calibrating accelerometer 2. Do not move the board.");
	// for (int i = 0; i < 1000; i++) {
	// 	offsetXHori += accelHori.getX();
	// 	offsetYHori += accelHori.getY();
	// 	offsetZHori += accelHori.getZ();
	// 	delay(2);
	// }

	// offsetXVert /= 1000.0;
	// offsetYVert /= 1000.0;
	// offsetZVert /= 1000.0;

	// offsetXHori /= 1000.0;
	// offsetYHori /= 1000.0;
	// offsetZHori /= 1000.0;

	// Serial.printf("Vertical calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXVert), String(offsetYVert), String(offsetZVert));
	// Serial.printf("Horizontal calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXHori), String(offsetYHori), String(offsetZHori));
	delay(2000);

	// accelVert Timer setup
	acc_timer = timerBegin(0, 80, true);                   // Begin timer with 1 MHz frequency (80MHz/80)
	timerAttachInterrupt(acc_timer, &onTimer, true);       // Attach the interrupt to Timer1
	unsigned int timerFactor = 1000000 / accSamplingRate; 	   // Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
	timerAlarmWrite(acc_timer, timerFactor, true);             // Initialize the timer
	timerAlarmEnable(acc_timer);

	// ********************** Loading of AR Model ************************
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
		if (numConnection >= 15) {
			Serial.println("Failed to connect to Wi-Fi");
			ESP.restart();
		}
        Serial.print(".");
		numConnection++;
        delay(300);
    }

	digitalWrite(LED_BUILTIN, HIGH);
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

	// Get the current row number
	FirebaseJson response;
	FirebaseJsonData data;
	String rowNumberCol = SHEET_NAME;
	rowNumberCol.concat("!A2");

	bool success = GSheet.values.get(&response /* returned response */, SPREADSHEET_ID /* spreadsheet Id to read */, rowNumberCol /* range to read */);
	response.toString(Serial, true); // To view the entire response
	Serial.println();
	response.get(data, "values/[0]/[0]");
	currRowNumber = data.to<long>();
	Serial.printf("Previous row number: %d\n", currRowNumber);
}


// Logic loop for taking user input and outputting the FFT
void loop() {
	// ********************** Accelerometer portion ************************
	sensors_event_t a, g, temp;
	mpu.getEvent(&a, &g, &temp);

	// Print accelVert values only if new data is available
	if (millis() - acc_timechecker >= 1000 && accelYVecVert.size() == 100) {

		// Normalise the vector values
		for (int i = 0; i < accelXVecVert.size(); i++) {
			// accelXVecVert[i] = (accelXVecVert[i] - minAccelXVert) / (maxAccelXVert - minAccelXVert);
			accelYVecVert[i] = (accelYVecVert[i] - minAccelYVert) / (maxAccelYVert - minAccelYVert);
			// accelZVecVert[i] = (accelZVecVert[i] - minAccelZVert) / (maxAccelZVert - minAccelZVert);
		}
		Serial.println("Done normalising vector");
		Serial.print("Max Y: ");
		Serial.println(maxAccelYVert);
		Serial.print("Min Y: ");
		Serial.println(minAccelYVert);

		// for (int i = 0; i < accelXVecVert.size(); i++)
		// {
		// // Serial.print(accelXVecVert[i], 1);
		// // Serial.print(", ");
		// // Serial.print(accelYVecVert[i], 1);
		// // Serial.print(", ");
		// // Serial.print(accelZVecVert[i], 1);
		// // Serial.print(", ");
		// // Serial.print(accelXVecHori[i], 1);
		// // Serial.print(", ");
		// // Serial.print(accelYVecHori[i], 1);
		// // Serial.print(", ");
		// // Serial.println(accelZVecHori[i], 1);
		// // delay(5);
		// }

		Serial.printf("Number of samples Vertical: %s\n", String(accelYVecVert.size()));
		// Serial.printf("Number of samples Horizontal: %s\n", String(accelXVecHori.size()));
		sendGoogleSheets(accelYVecVert, currRowNumber, lastRow, SHEET_NAME);
		currRowNumber++;

		accelXVecVert.clear();
		accelYVecVert.clear();
		accelZVecVert.clear();

		// accelXVecHori.clear();
		// accelYVecHori.clear();
		// accelZVecHori.clear();

		// Reset normalizing values
		maxAccelXVert = -10e9;
		minAccelXVert = 10e9;
		maxAccelYVert = -10e9;
		minAccelYVert = 10e9;
		maxAccelZVert = -10e9;
		minAccelZVert = 10e9;

		delay(1000);
		acc_timechecker = millis();
	}

	if (isAccTimerTriggered && (accelYVecVert.size() < 100)) {
		isAccTimerTriggered = false;
		float y_value = a.acceleration.y;
		accelXVecVert.push_back(a.acceleration.x);
		// accelXVecVert.push_back(accelVert.getX() - offsetXVert);
		// accelXVecHori.push_back(accelHori.getX() - offsetXHori);

		accelYVecVert.push_back(y_value);
		// accelYVecVert.push_back(accelVert.getY() - offsetYVert);
		// accelYVecHori.push_back(accelHori.getY() - offsetYHori);

		accelZVecVert.push_back(a.acceleration.z);
		// accelZVecVert.push_back(accelVert.getZ() - offsetZVert);
		// accelZVecHori.push_back(accelHori.getZ() - offsetZHori);

		// For normalizing the accelerometer data
		if (y_value > maxAccelYVert) {
			maxAccelYVert = y_value;
		}
		if (y_value < minAccelYVert) {
			minAccelYVert = y_value;
		}
	}

	// Call ready() repeatedly in loop for authentication checking and processing
    bool ready = GSheet.ready();
	if (ready && currRowNumber < lastRow) {
        // FirebaseJson response;
        // FirebaseJsonData data;
        // FirebaseJsonArray dataArr;
        // // Instead of using FirebaseJson for response, you can use String for response to the functions
        // // especially in low memory device that deserializing large JSON response may be failed as in ESP8266
        // currRowNumber++;
        // Serial.printf("Current row number: %d\n", currRowNumber);

        // String startCol = SHEET_NAME;
		// startCol.concat("!C");

        // String endCol = ":CX";
        // startCol.concat(String(currRowNumber));
        // endCol.concat(String(currRowNumber));
        // startCol.concat(endCol);
        // String rangeToRead = startCol;

        // Serial.println("---------------------------------------------------------------");

        // // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/get

        // bool success = GSheet.values.get(&response /* returned response */, SPREADSHEET_ID /* spreadsheet Id to read */, rangeToRead /* range to read */);

        // // response.toString(Serial, true); // To view the entire response
        // Serial.println();
        // response.get(data, "values/[0]");
        // data.get<FirebaseJsonArray>(dataArr);

        // for (int i = 0; i < dataArr.size(); i++) {
        //     dataArr.get(data, i);
		// 	if (i < AR_INPUT_SIZE) {
		// 		AR_user_input[i] = data.to<String>().toFloat();
		// 	}
        // }

		// // To check if user input is set correctly
		// for (int i = 0; i < AR_INPUT_SIZE; i++) {
		// 	Serial.printf("Input %d: %f\n", i, AR_user_input[i]);
		// }
        
        // Serial.println();
        // Serial.println("---------------------------------------------------------------");
    }
	
	// Serial.print("Free heap: ");
	// Serial.println(ESP.getFreeHeap());

	// // Set the input node to the user input
	// for (int i = 0; i < AR_INPUT_SIZE; i++) {
	// 	// FFT_input->data.f[i] = user_input[i];
	// 	AR_input->data.f[i] = AR_user_input[i];
	// }

	// Serial.println("Running inference on inputted data...");

	// // Run infernece on the AR input data
	// if(AR_interpreter->Invoke() != kTfLiteOk) {
	// 	Serial.println("There was an error invoking the AR interpreter!");
	// 	return;
	// }

	// Serial.println("Inference complete!");

	// // Save the output of the AR inference
	// for (int i = 0; i < AR_INPUT_SIZE; i++) {
	// 	recon_data[i] = AR_output->data.f[i];
	// }

	// for (int i = 0; i < AR_INPUT_SIZE; i++) {
	// 	Serial.printf("AR Output %d: %f\n", i, recon_data[i]);
	// }

	// Calculate MSE
	// double MSE = 0.0;
	// for (int i = 0; i < AR_INPUT_SIZE; i++) {
	// 	double diff = AR_user_input[i] - recon_data[i];
	// 	MSE += diff * diff;
	// }
	// MSE /= AR_INPUT_SIZE;

	// Serial.printf("MSE: %f\n", MSE);
	// if (MSE > THRESHOLD) {
	// 	Serial.println("MSE > Threshold -> Unhealthy!");
	// } else {
	// 	Serial.println("MSE <= Threshold -> Healthy!");
	// }

	// For sending AR result to Google sheets
	// if (ready) {
	// 	FirebaseJson updateOutput;
	// 	FirebaseJson updateCurrentRow;
	// 	FirebaseJson updateResponse;

	// 	// To update latest MSE calculated
	// 	String updateResultCol = "UnhealthyAR!CX";
	// 	updateResultCol.concat(String(currRowNumber));
	// 	String updateRange = updateResultCol;

	// 	updateOutput.add("range", updateResultCol);
	// 	updateOutput.add("majorDimension", "ROWS");
	// 	updateOutput.set("values/[0]/[0]", MSE);

	// 	// To update latest row number
	// 	String updateCurrentRowRange = "UnhealthyAR!CZ2";
	// 	updateCurrentRow.add("range", updateCurrentRowRange);
	// 	updateCurrentRow.add("majorDimension", "ROWS");
	// 	updateCurrentRow.set("values/[0]/[0]", currRowNumber);

	// 	// Appending data + row number to update all at once
	// 	FirebaseJsonArray updateArr;
	// 	updateArr.add(updateCurrentRow);
	// 	updateArr.add(updateOutput);

	// 	bool success = GSheet.values.batchUpdate(&updateResponse, 		// Returned response
	// 											SPREADSHEET_ID, 		// Spreadsheet ID to update
	// 											&updateArr); 			// Array of data to update
		
	// 	// updateResponse.toString(Serial, true);
	// 	Serial.println();
	// }

	// Serial.printf("Waiting %.2f seconds before next inference...\n", GOOGLE_SHEETS_DELAY);
	// delay(1000 * GOOGLE_SHEETS_DELAY);

	if (currRowNumber >= lastRow) {
		Serial.println("Done with testing, putting ESP32 to deepsleep...");

		ESP.deepSleep(UINT32_MAX);
	}
}