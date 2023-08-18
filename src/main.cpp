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

#include <Arduino.h>
#include <math.h>
#include "tensorflow/lite/experimental/micro/kernels/all_ops_resolver.h"
#include "tensorflow/lite/experimental/micro/micro_error_reporter.h"
#include "tensorflow/lite/experimental/micro/micro_interpreter.h"
#include "multi_model_data.h"

// Create a memory pool for the nodes in the network
constexpr int tensor_pool_size = 2 * 1024;
uint8_t tensor_pool[tensor_pool_size];

// Define the model to be used
const tflite::Model* multi_model;

// Define the interpreter
tflite::MicroInterpreter* interpreter;

// Input/Output nodes for the network
TfLiteTensor* input;
TfLiteTensor* output;

// Set up the ESP32's environment.
void setup() { 
	// Start serial at 115200 baud
	Serial.begin(115200);

	// Load the sample multi model
	Serial.println("Loading Tensorflow model....");
	multi_model = tflite::GetModel(g_multi_model_data);
	Serial.println("multi model loaded!");

	// Define ops resolver and error reporting
	static tflite::ops::micro::AllOpsResolver resolver;

	static tflite::ErrorReporter* error_reporter;
	static tflite::MicroErrorReporter micro_error;
	error_reporter = &micro_error;

	// Instantiate the interpreter 
	static tflite::MicroInterpreter static_interpreter(
		multi_model, resolver, tensor_pool, tensor_pool_size, error_reporter
	);

	interpreter = &static_interpreter;

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if(interpreter->AllocateTensors() != kTfLiteOk) {
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	input = interpreter->input(0);
	output = interpreter->output(0);

	Serial.println("Starting inferences... Input a number! ");
}

// Wait for 2 serial inputs to be made available and parse them as floats
float user_input[2];

// Logic loop for taking user input and outputting the multi
void loop() { 
	// Wait for serial input to be made available and parse it as a float
	for (int i = 0; i < 2; i++) {
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

	/* The sample model is only trained for values between 0 and 50
	* This will keep the user from inputting bad numbers. 
	*/
	if (user_input[0] < 1 || user_input[0] > 50 ||
		user_input[1] < 1 || user_input[1] > 50) {
		Serial.println("Your numbers must be greater than 0 and less than 50");
		return;
	}

	// Set the input node to the user input
	input->data.f[0] = user_input[0];
	input->data.f[1] = user_input[1];

	Serial.println("Running inference on inputted data...");

	// Run inference on the input data
	if(interpreter->Invoke() != kTfLiteOk) {
		Serial.println("There was an error invoking the interpreter!");
		return;
	}

	// Print the output of the model.
	Serial.print("Output: ");
	Serial.println(output->data.f[0]);
	Serial.println("");
	Serial.println("Input another 2 numbers!");
} 