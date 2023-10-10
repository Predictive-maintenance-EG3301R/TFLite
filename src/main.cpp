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
// #include "AR_model.h"
#include "cnn_fullint_quantized.h"
#include <Arduino.h>
#include <WiFi.h>

constexpr int CNN_tensor_pool_size = 100 * 1024;
alignas(16) uint8_t CNN_tensor_pool[CNN_tensor_pool_size];

// Define the model to be used
const tflite::Model* CNN_model;

// Define the interpreter
tflite::MicroInterpreter* CNN_interpreter;

// Input/Output nodes for the network
TfLiteTensor* CNN_input;
TfLiteTensor* CNN_output;

// Set up the ESP32's environment.
void setup() { 
	// Start serial at 115200 baud
	Serial.begin(115200);

	// CNN portion
	// Load the FFT model
	Serial.println("Loading Tensorflow model....");
	CNN_model = tflite::GetModel(cnn_fullint_quantized_tflite);
	Serial.println("CNN model loaded!");

	// Define ops resolver and error reporting
	static tflite::AllOpsResolver CNN_resolver;
	Serial.println("Resolver loaded!");

	static tflite::ErrorReporter* CNN_error_reporter;
	static tflite::MicroErrorReporter CNN_micro_error;
	CNN_error_reporter = &CNN_micro_error;
	Serial.println("Error reporter loaded!");

	// Instantiate the interpreter 
	static tflite::MicroInterpreter CNN_static_interpreter(
		CNN_model, CNN_resolver, CNN_tensor_pool, CNN_tensor_pool_size, CNN_error_reporter
	);

	CNN_interpreter = &CNN_static_interpreter;
	Serial.println("Interpreter loaded!");

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if(CNN_interpreter->AllocateTensors() != kTfLiteOk) {
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
                             CNN_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	CNN_input = CNN_interpreter->input(0);
	CNN_output = CNN_interpreter->output(0);
	Serial.println("Input and output nodes loaded!");

	Serial.println("Starting inferences... Input a number! ");
}

// Wait for 5 serial inputs to be made available and parse them as floats
float user_input[5];
#define INPUT_SIZE 5

// Logic loop for taking user input and outputting the FFT
void loop() { 
	// Wait for serial input to be made available and parse it as a float
	Serial.println("Delaying for 10sec...");
	Serial.println("Free heap: " + String(ESP.getFreeHeap()));
	delay(10000);
} 