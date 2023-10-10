#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "cnn_fullint_quantized.h"
// #include "BlynkSimpleEsp32.h"
#include <Wire.h>
#include <SPI.h>
#include "SparkFun_LIS2DH12.h"
#include "DFRobot_MLX90614.h"
// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <WiFi.h>
// #include <ESP_Google_Sheet_Client.h>
#include <secrets.h>
#include <vector>
#include "time.h"

using namespace std;

//***************** Generic variables *****************
#define RESET_TIMER 1000ULL * 60 * 60 * 24 // Reset ESP32 every 24 hours

// **************** Accelerometer variables ****************
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

SPARKFUN_LIS2DH12 accelVert; // Create instance
SPARKFUN_LIS2DH12 accelHori; // Create instance
// Adafruit_MPU6050 mpu;

TwoWire I2Cone = TwoWire(0);

// Interrupt handler to collect accelerometer data per 1ms
void IRAM_ATTR onTimer() {
  isAccTimerTriggered = true; // Indicates that the interrupt has been entered since the last time its value was changed to false
}

// **************** TF Lite variables ****************
// Details for model to be tested
#define AR_INPUT_SIZE 		100

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
	Serial.print("Free heap: ");
	Serial.println(String(ESP.getFreeHeap()));
	delay(10000);
} 