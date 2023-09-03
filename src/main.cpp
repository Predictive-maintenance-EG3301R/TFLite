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
#include "AR_model.h"
#include <Arduino.h>
#include <WiFi.h>

// Create a memory pool for the nodes in the network
constexpr int FFT_tensor_pool_size = 2 * 1024;
alignas(16) uint8_t FFT_tensor_pool[FFT_tensor_pool_size];

constexpr int AR_tensor_pool_size = 10 * 1024;
alignas(16) uint8_t AR_tensor_pool[AR_tensor_pool_size];

// Define the model to be used
const tflite::Model* FFT_model;
const tflite::Model* AR_model;

// Define the interpreter
tflite::MicroInterpreter* FFT_interpreter;
tflite::MicroInterpreter* AR_interpreter;

// Input/Output nodes for the network
TfLiteTensor* FFT_input;
TfLiteTensor* FFT_output;
TfLiteTensor* AR_input;
TfLiteTensor* AR_output;

// Set up the ESP32's environment.
void setup() { 
	// Start serial at 115200 baud
	Serial.begin(115200);

	// FFT portion
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


	// AR portion
	// Load the FFT model
	Serial.println("Loading Tensorflow model....");
	AR_model = tflite::GetModel(AR_model_fullint_quantized_tflite);
	Serial.println("AR model loaded!");

	// Define ops resolver and error reporting
	static tflite::AllOpsResolver AR_resolver;
	Serial.println("Resolver loaded!");

	static tflite::ErrorReporter* AR_error_reporter;
	static tflite::MicroErrorReporter AR_micro_error;
	AR_error_reporter = &AR_micro_error;
	Serial.println("Error reporter loaded!");

	// Instantiate the interpreter 
	static tflite::MicroInterpreter AR_static_interpreter(
		AR_model, AR_resolver, AR_tensor_pool, AR_tensor_pool_size, AR_error_reporter
	);

	AR_interpreter = &AR_static_interpreter;
	Serial.println("Interpreter loaded!");

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if(AR_interpreter->AllocateTensors() != kTfLiteOk) {
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
                             AR_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	AR_input = AR_interpreter->input(0);
	AR_output = AR_interpreter->output(0);
	Serial.println("Input and output nodes loaded!");

	Serial.println("Starting inferences... Input a number! ");
}

// Wait for 5 serial inputs to be made available and parse them as floats
float user_input[5];
#define INPUT_SIZE 5

// Logic loop for taking user input and outputting the FFT
void loop() { 
	// Wait for serial input to be made available and parse it as a float
	for (int i = 0; i < INPUT_SIZE; i++) {
		while (Serial.available() == 0) {
			// Wait for serial input to be made available
			delay(10);
		}
		char buffer[10];
		Serial.readBytesUntil('\n', buffer, 10);
		user_input[i] = atof(buffer);
		Serial.printf("Input %d: %f\n", i, user_input[i]);
		Serial.flush();
	}

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

	// Print the output of the model.
	Serial.print("Output 1: ");
	Serial.println(FFT_output->data.f[0]);
	Serial.print("Output 2: ");
	Serial.println(FFT_output->data.f[1]);
	Serial.println("Input another 5 numbers!");
	Serial.println("");
} 