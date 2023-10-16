
#include <secrets.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "cnn_model_fullint_vibeonly.h"
#include "AR_model.h"
#include "BlynkSimpleEsp32.h"
#include <Wire.h>
#include <SPI.h>
#include <AWS_IOT.h>
#include "SparkFun_LIS2DH12.h"
#include "DFRobot_MLX90614.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Update.h>
#include "time.h"

using namespace std;

//***************** Generic variables *****************
unsigned long long deep_sleep_time = 1000000ULL * 10 * 1 * 1; // Time to sleep in microseconds (10seconds)
Preferences preferences;
int mode;								   // 0 for autoencoder, 1 for classifier
volatile bool isLEDTimerTriggered = false; // For checking if timer triggered
int pulseBrightness = RGB_BRIGHTNESS;
bool pulseDown = true;

// NTP server details
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;
char str_time[100];

//***************** WiFi/OTA variables *****************
WiFiClient otaclient;

// Variables to validate response from S3
long contentLength = 0;
bool isValidContentType = false;

//***************** AWS variables *****************
AWS_IOT aws_iot;
char payload[100];
volatile bool sendToAWS = false;

//***************** Blynk variables *****************
volatile bool OTAAvailable = false;

// **************** Accelerometer variables ****************
#define NUM_PER_SAMPLE 1000 // Number of data points per sample
int numSamples = 0;
int acc_counter = 0;
int acc_timechecker = millis();
volatile bool isAccTimerTriggered = false; // For checking if timer triggered
int accSamplingRate = 1000;				   // Sampling rate in Hz, max is 1000Hz
hw_timer_t *acc_timer = NULL;			   // Timer object

// Offset values for the accelerometers
float offsetXVert = 0.0;
float offsetYVert = 0.0;
float offsetZVert = 0.0;
float offsetXHori = 0.0;
float offsetYHori = 0.0;
float offsetZHori = 0.0;
float accelXVecVert[1000];
float accelYVecVert[1000];
float accelZVecVert[1000];
float accelXVecHori[1000];
float accelYVecHori[1000];
float accelZVecHori[1000];

// For normalizing the accelerometer data
float maxAccelXVert[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelXVert[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};
float maxAccelYVert[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelYVert[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};
float maxAccelZVert[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelZVert[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};
float maxAccelXHori[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelXHori[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};
float maxAccelYHori[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelYHori[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};
float maxAccelZHori[10] = {-10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9, -10e9};
float minAccelZHori[10] = {10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9, 10e9};

SPARKFUN_LIS2DH12 accelVert; // Create instance
SPARKFUN_LIS2DH12 accelHori; // Create instance

TwoWire I2Cone = TwoWire(0); // For acelerometers

//****************** Temperature variables ******************
// TwoWire I2Ctwo = TwoWire(1);
// DFRobot_MLX90614_I2C sensor(0x5A, &I2Ctwo); // instantiate an object to drive the temp sensor

//***************** AC Current Sensor variables *****************
#define ACPin 9
#define ACTectionRange 20; // set Non-invasive AC Current Sensor tection range (5A,10A,20A)
#define VREF 3.3

// **************** TF Lite variables ****************
// Details for model to be tested
#define AUTOENCODER_INPUT_SIZE 100	 // Per axis
#define CLASSIFICATION_INPUT_SIZE 96 // Per axis
#define NUM_INFERENCE_SAMPLES 200
#define NUM_AXIS 6
#define NUM_CATEGORIES 3

// For both
int total_inference_count = 0;

// For classification
int num_healthy = 0;
int num_loose = 0;
int num_cavitation = 0;
int max_index = 0;
float max_output = 0.0;

// For autoencoder
float mse_threshold = 0.02774564472135648; // Value obtained from training
float output_mse = 0;
int num_anomaly = 0;
bool anomaly_detected = false;

// Allocate space for tensors to be loaded
constexpr int model_tensor_pool_size = 100 * 1024;
alignas(16) uint8_t model_tensor_pool[model_tensor_pool_size];

// Define the model to be used
const tflite::Model *ML_model;

// Define the interpreter
tflite::MicroInterpreter *model_interpreter;

// Input/Output nodes for the network
TfLiteTensor *model_input;
TfLiteTensor *model_output;

//****************** Generic Functions ******************
void updateLatestTime();

//***************** RGB Functions *****************
void setRed();
void setGreen();
void setBlue();
void setPurple();
void setOrange();
void setYellow();
void setPulsingBlue();

//***************** Blynk Functions *****************
BLYNK_CONNECTED()
{
	Blynk.syncVirtual(OTA_VPIN, SEND_AWS_VPIN);
}

BLYNK_WRITE(OTA_VPIN) // To read in whether OTA is available from Blynk
{
	OTAAvailable = param.asInt();
}

BLYNK_WRITE(SEND_AWS_VPIN) // To read in whether to send data to AWS
{
	sendToAWS = param.asInt();
}

//***************** OTA Functions *****************
String getHeaderValue(String header, String headerName);
void execOTA();

//***************** Accelerometer Functions *****************
void getAccelData();
void normalizeAccelData();

//***************** AC Current Sensor Functions *****************
float readACCurrentValue();

//***************** AWS Functions *****************
void publishToAWS();

//***************** TFLite Functions *****************
void evaluateResults();

// Interrupt handler to collect accelerometer data per 1ms
void IRAM_ATTR onTimer()
{
	isAccTimerTriggered = true; // Indicates that the interrupt has been entered since the last time its value was changed to false
	isLEDTimerTriggered = true;
}

// Set up the ESP32's environment.
void setup()
{
	// Start serial at 115200 baud
	Serial.begin(115200);

	// Set up LED
	pinMode(RGB_BUILTIN, OUTPUT);
	digitalWrite(RGB_BUILTIN, LOW);

	// **************** Accelerometer setup ****************
	I2Cone.begin(4, 5);
	Serial.println("I2Cone begin");
	I2Cone.setClock(1000000);
	delay(200);

	// **************** Temperature sensor setup ****************
	// I2Ctwo.begin(6, 7);
	// I2Ctwo.setClock(1000000);
	// delay(200);

	// **************** WiFi & RTC setup ****************
	// Connect to Wifi
	int numConnection = 0;
	WiFi.setAutoReconnect(true);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.printf("Connecting to %s\n", WIFI_SSID);
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

	// Init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

	// Set RGB to blue to indicate successful connection
	setBlue();

	// Check for OTA first
	// ********** Blynk setup **********
	Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD, "blynk.cloud", 8080);
	if (Blynk.connected())
	{
		Serial.println("Blynk Connected");
		delay(100);
		Blynk.syncVirtual(OTA_VPIN);
		delay(500);
		Blynk.syncVirtual(OTA_VPIN); // Second time to ensure value read in is correct
	}
	else
	{
		Serial.println("Blynk Not Connected");
	}

	// ************ Execute OTA Update if available ************
	if (OTAAvailable)
	{
		Serial.println("OTA is available");
		setPurple();
		execOTA();
	}
	else
	{
		Serial.println("OTA is not available");
	}

	Blynk.virtualWrite(FIRMWARE_VERSION_VPIN, FIRMWARE_VERSION);

	// Check which mode is supposed to be in
	preferences.begin("mode", false);	  // Open preferences with write access
	mode = preferences.getInt("mode", 0); // Get the value of the key; if not present, return default value of 0
	preferences.end();

	// Load the correct model based on the mode
	if (mode == 0)
	{ // Load autoencoder model if mode == 0
		Serial.println("Loading autoencoder model....");
		ML_model = tflite::GetModel(AR_model_fullint_quantized_tflite); // Model not yet confirmed
		Serial.println("Autoencoder model loaded!");
	}
	else
	{ // Load classifier model if mode == 1
		Serial.println("Loading classifier model....");
		ML_model = tflite::GetModel(cnn_model_fullint_vibeonly_tflite); // Correct final model to be used
		Serial.println("Classifier model loaded!");
	}

	// Load the model into the interpreter
	static tflite::AllOpsResolver model_resolver;
	Serial.println("Resolver loaded!");

	static tflite::ErrorReporter *model_error_reporter;
	static tflite::MicroErrorReporter model_micro_error;
	model_error_reporter = &model_micro_error;
	Serial.println("Error reporter loaded!");

	// Instantiate the interpreter
	static tflite::MicroInterpreter model_static_interpreter(
		ML_model, model_resolver, model_tensor_pool, model_tensor_pool_size, model_error_reporter);

	model_interpreter = &model_static_interpreter;
	Serial.println("Interpreter loaded!");

	// Allocate the the model's tensors in the memory pool that was created.
	Serial.println("Allocating tensors to memory pool");
	if (model_interpreter->AllocateTensors() != kTfLiteOk)
	{
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
					  ML_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		return;
	}

	// Define input and output nodes
	model_input = model_interpreter->input(0);
	model_output = model_interpreter->output(0);
	Serial.println("Input and output nodes loaded!");

	// ********** Timer Interrupt setup **********
	acc_timer = timerBegin(0, 80, true);				  // Begin timer with 1 MHz frequency (80MHz/80)
	timerAttachInterrupt(acc_timer, &onTimer, true);	  // Attach the interrupt to Timer1
	unsigned int timerFactor = 1000000 / accSamplingRate; // Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
	timerAlarmWrite(acc_timer, timerFactor, true);		  // Initialize the timer
	timerAlarmEnable(acc_timer);
}

// Logic loop for taking user input and outputting the FFT
void loop()
{
	if (numSamples == NUM_PER_SAMPLE)
	{
		// Check amount of free heap space initially
		Serial.print("Free heap before running model: ");
		Serial.println(String(ESP.getFreeHeap()));

		// Test the model with some dummy data
		Serial.println("Testing model with dummy data...");

		// Normalizing the accelerometer data first
		normalizeAccelData();

		// Each sample of 1000ms can be split into 10 samples of 100ms for inference
		int curr_inference_count = 0;
		while (curr_inference_count < 10)
		{
			if (mode == 0)
			{
				// Load the data into the interpreter
				for (int i = 0; i < AUTOENCODER_INPUT_SIZE; i++)
				{
					model_input->data.f[i] = accelXVecVert[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
					model_input->data.f[i + AUTOENCODER_INPUT_SIZE] = accelYVecVert[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
					model_input->data.f[i + (2 * AUTOENCODER_INPUT_SIZE)] = accelZVecVert[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
					model_input->data.f[i + (3 * AUTOENCODER_INPUT_SIZE)] = accelXVecHori[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
					model_input->data.f[i + (4 * AUTOENCODER_INPUT_SIZE)] = accelYVecHori[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
					model_input->data.f[i + (5 * AUTOENCODER_INPUT_SIZE)] = accelZVecHori[i + (curr_inference_count * AUTOENCODER_INPUT_SIZE)];
				}
			}
			else
			{
				// Load the data into the interpreter
				for (int i = 0; i < CLASSIFICATION_INPUT_SIZE; i++)
				{
					model_input->data.f[i] = accelXVecVert[i];
					model_input->data.f[i + CLASSIFICATION_INPUT_SIZE] = accelYVecVert[i + (curr_inference_count * 100)];
					model_input->data.f[i + (2 * CLASSIFICATION_INPUT_SIZE)] = accelZVecVert[i + (curr_inference_count * 100)];
					model_input->data.f[i + (3 * CLASSIFICATION_INPUT_SIZE)] = accelXVecHori[i + (curr_inference_count * 100)];
					model_input->data.f[i + (4 * CLASSIFICATION_INPUT_SIZE)] = accelYVecHori[i + (curr_inference_count * 100)];
					model_input->data.f[i + (5 * CLASSIFICATION_INPUT_SIZE)] = accelZVecHori[i + (curr_inference_count * 100)];
				}
			}

			// Run inference on the model
			TfLiteStatus invoke_status = model_interpreter->Invoke();
			if (invoke_status != kTfLiteOk)
			{
				Serial.println("There was an error invoking the interpreter...ooof");
				delay(5000);
				esp_restart();
			}

			// Get the output from the model
			if (mode == 0)
			{
				// Calculate the MSE
				int num_rounds = 0;
				for (int i = 0; i < (AUTOENCODER_INPUT_SIZE * NUM_AXIS); i++)
				{
					float curr_data = model_output->data.f[i];
					if (i % 6 == 0)
					{
						output_mse += pow((accelXVecVert[num_rounds] - curr_data), 2);
					}
					else if (i % 6 == 1)
					{
						output_mse += pow((accelYVecVert[num_rounds] - curr_data), 2);
					}
					else if (i % 6 == 2)
					{
						output_mse += pow((accelZVecVert[num_rounds] - curr_data), 2);
					}
					else if (i % 6 == 3)
					{
						output_mse += pow((accelXVecHori[num_rounds] - curr_data), 2);
					}
					else if (i % 6 == 4)
					{
						output_mse += pow((accelYVecHori[num_rounds] - curr_data), 2);
					}
					else if (i % 6 == 5)
					{
						output_mse += pow((accelZVecHori[num_rounds] - curr_data), 2);
						num_rounds++;
					}
				}

				output_mse /= (AUTOENCODER_INPUT_SIZE * NUM_AXIS);
			}
			else
			{
				for (int i = 0; i < NUM_CATEGORIES; i++)
				{
					float curr_data = model_output->data.f[i];
					if (curr_data > max_output)
					{
						max_output = curr_data;
						max_index = i;
					}
				}
			}

			// Processing output data based on model
			if (mode == 0)
			{
				// Check if the mse is above the threshold
				if (output_mse > mse_threshold)
				{
					num_anomaly++;
				}

				output_mse = 0.0; // Reset the output_mse
			}
			else
			{
				// Check if the output is healthy, loose or cavitation
				if (max_index == 1)
				{
					num_healthy++;
				}
				else if (max_index == 2)
				{
					num_loose++;
				}
				else
				{
					num_cavitation++;
				}
			}

			curr_inference_count++;
			total_inference_count++;
			// Serial.println("Inference count: " + String(curr_inference_count));
			delay(10);
		}

		numSamples = 0; // Reset the number of samples taken
		Serial.println("Inference count: " + String(total_inference_count));
		// Check amount of free heap space after running model
		Serial.print("Free heap after running model: ");
		Serial.println(String(ESP.getFreeHeap()));
	}

	if (isAccTimerTriggered && (numSamples < NUM_PER_SAMPLE))
	{
		isAccTimerTriggered = false;

		// ********** Sparkfun LIS2DH12 **********
		// getAccelData();

		// ********** Dummy data **********
		accelXVecVert[numSamples] = 0.0;
		accelXVecHori[numSamples] = 0.0;
		accelYVecVert[numSamples] = 0.0;
		accelYVecHori[numSamples] = 0.0;
		accelZVecVert[numSamples] = 0.0;
		accelZVecHori[numSamples] = 0.0;

		numSamples++; // Increment the number of samples taken
	}

	if (total_inference_count >= NUM_INFERENCE_SAMPLES)
	{
		Serial.println("Inferences done, processing results now");
		evaluateResults();
		delay(5000); // Delay to allow user to see whether anomaly was detected from RGB LED

		if (sendToAWS) // If user wants to send data to AWS
		{
			Serial.println("Sending to AWS...");
			publishToAWS();
		}
		else
		{
			Serial.println("Not sending to AWS");
		}

		Serial.println("Going into deep sleep...");
		neopixelWrite(RGB_BUILTIN, 0, 0, 0);
		esp_sleep_enable_timer_wakeup(deep_sleep_time);
		esp_deep_sleep_start();
	}
}

// **************** RGB Utility Functions ****************
void setRed()
{
	neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);
}

void setGreen()
{
	neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);
}

void setBlue()
{
	neopixelWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);
}

void setPurple()
{
	neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, RGB_BRIGHTNESS);
}

void setOrange()
{
	neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS / 2, 0);
}

void setYellow()
{
	neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, 0);
}

void setPulsingBlue()
{
	if (isLEDTimerTriggered)
	{
		isLEDTimerTriggered = false;
		if (pulseDown)
		{
			pulseBrightness--;
			if (pulseBrightness == 0)
			{
				pulseDown = false;
			}
		}
		else
		{
			pulseBrightness++;
			if (pulseBrightness == RGB_BRIGHTNESS)
			{
				pulseDown = true;
			}
		}

		neopixelWrite(RGB_BUILTIN, 0, 0, pulseBrightness);
	}
}

// **************** Accelerometer Utility Functions ****************
// To get the readings from the accelerometer & update min and max values for normalizing
void getAccelData()
{
	// Get XVert data and update min and max for normalizing
	float currXVert = accelVert.getX() - offsetXVert;
	int currIdx = numSamples / 100;

	if (currXVert > maxAccelXVert[currIdx])
	{
		maxAccelXVert[currIdx] = currXVert;
	}
	if (currXVert < minAccelXVert[currIdx])
	{
		minAccelXVert[currIdx] = currXVert;
	}

	// Get YVert data and update min and max for normalizing
	float currYVert = accelVert.getY() - offsetYVert;
	if (currYVert > maxAccelYVert[currIdx])
	{
		maxAccelYVert[currIdx] = currYVert;
	}
	if (currYVert < minAccelYVert[currIdx])
	{
		minAccelYVert[currIdx] = currYVert;
	}

	// Get ZVert data and update min and max for normalizing
	float currZVert = accelVert.getZ() - offsetZVert;
	if (currZVert > maxAccelZVert[currIdx])
	{
		maxAccelZVert[currIdx] = currZVert;
	}
	if (currZVert < minAccelZVert[currIdx])
	{
		minAccelZVert[currIdx] = currZVert;
	}

	// Get XHori data and update min and max for normalizing
	float currXHori = accelHori.getX() - offsetXHori;
	if (currXHori > maxAccelXHori[currIdx])
	{
		maxAccelXHori[currIdx] = currXHori;
	}
	if (currXHori < minAccelXHori[currIdx])
	{
		minAccelXHori[currIdx] = currXHori;
	}

	// Get YHori data and update min and max for normalizing
	float currYHori = accelHori.getY() - offsetYHori;
	if (currYHori > maxAccelYHori[currIdx])
	{
		maxAccelYHori[currIdx] = currYHori;
	}
	if (currYHori < minAccelYHori[currIdx])
	{
		minAccelYHori[currIdx] = currYHori;
	}

	// Get ZHori data and update min and max for normalizing
	float currZHori = accelHori.getZ() - offsetZHori;
	if (currZHori > maxAccelZHori[currIdx])
	{
		maxAccelZHori[currIdx] = currZHori;
	}
	if (currZHori < minAccelZHori[currIdx])
	{
		minAccelZHori[currIdx] = currZHori;
	}

	accelXVecVert[numSamples] = currXVert;
	accelXVecHori[numSamples] = currXHori;
	accelYVecVert[numSamples] = currYVert;
	accelYVecHori[numSamples] = currYHori;
	accelZVecVert[numSamples] = currZVert;
	accelZVecHori[numSamples] = currZHori;
	// numSamples is added in the main loop, no need to add here
}

// Normalize the accelerometer data and reset the min and max values for each axis
// Data is normalized in segments of 100, between 0 and 1 for autoencoder and -1 and 1 for classifier
void normalizeAccelData()
{
	if (mode == 0)
	{
		for (int i = 0; i < 10; i++)
		{
			for (int j = 0; j < 100; j++)
			{
				accelXVecVert[(i * 100) + j] = (accelXVecVert[(i * 100) + j] - minAccelXVert[i]) / (maxAccelXVert[i] - minAccelXVert[i]);
				accelYVecVert[(i * 100) + j] = (accelYVecVert[(i * 100) + j] - minAccelYVert[i]) / (maxAccelYVert[i] - minAccelYVert[i]);
				accelZVecVert[(i * 100) + j] = (accelZVecVert[(i * 100) + j] - minAccelZVert[i]) / (maxAccelZVert[i] - minAccelZVert[i]);
				accelXVecHori[(i * 100) + j] = (accelXVecHori[(i * 100) + j] - minAccelXHori[i]) / (maxAccelXHori[i] - minAccelXHori[i]);
				accelYVecHori[(i * 100) + j] = (accelYVecHori[(i * 100) + j] - minAccelYHori[i]) / (maxAccelYHori[i] - minAccelYHori[i]);
				accelZVecHori[(i * 100) + j] = (accelZVecHori[(i * 100) + j] - minAccelZHori[i]) / (maxAccelZHori[i] - minAccelZHori[i]);
			}
		}
	}
	if (mode == 1)
	{
		for (int i = 0; i < 10; i++)
		{
			for (int j = 0; j < 100; j++)
			{
				accelXVecVert[(i * 100) + j] = ((accelXVecVert[(i * 100) + j] - minAccelXVert[i]) / (maxAccelXVert[i] - minAccelXVert[i])) * 2 - 1;
				accelYVecVert[(i * 100) + j] = ((accelYVecVert[(i * 100) + j] - minAccelYVert[i]) / (maxAccelYVert[i] - minAccelYVert[i])) * 2 - 1;
				accelZVecVert[(i * 100) + j] = ((accelZVecVert[(i * 100) + j] - minAccelZVert[i]) / (maxAccelZVert[i] - minAccelZVert[i])) * 2 - 1;
				accelXVecHori[(i * 100) + j] = ((accelXVecHori[(i * 100) + j] - minAccelXHori[i]) / (maxAccelXHori[i] - minAccelXHori[i])) * 2 - 1;
				accelYVecHori[(i * 100) + j] = ((accelYVecHori[(i * 100) + j] - minAccelYHori[i]) / (maxAccelYHori[i] - minAccelYHori[i])) * 2 - 1;
				accelZVecHori[(i * 100) + j] = ((accelZVecHori[(i * 100) + j] - minAccelZHori[i]) / (maxAccelZHori[i] - minAccelZHori[i])) * 2 - 1;
			}
		}
	}

	// Reset the min and max values
	for (int i = 0; i < 10; i++)
	{
		maxAccelXVert[i] = -10e9;
		minAccelXVert[i] = 10e9;
		maxAccelYVert[i] = -10e9;
		minAccelYVert[i] = 10e9;
		maxAccelZVert[i] = -10e9;
		minAccelZVert[i] = 10e9;
		maxAccelXHori[i] = -10e9;
		minAccelXHori[i] = 10e9;
		maxAccelYHori[i] = -10e9;
		minAccelYHori[i] = 10e9;
		maxAccelZHori[i] = -10e9;
		minAccelZHori[i] = 10e9;
	}
};

// **************** TFLite Utility Functions ****************
// Evaluate the results of the inference and update EEPROM to change mode based on result
// Mode will always be changed back to autoencoder after running the classifier once
void evaluateResults()
{
	// Process the results
	if (mode == 0)
	{
		float percentage_anomaly = (float)num_anomaly / (float)total_inference_count;
		Serial.print("Percentage anomaly: ");
		Serial.println(percentage_anomaly);
		if (percentage_anomaly > 0.5)
		{
			Serial.println("Anomaly detected!");
			anomaly_detected = true;
			setRed();
		}
		else
		{
			Serial.println("No anomaly detected!");
		}
	}
	else
	{
		float percentage_healthy = (float)num_healthy / (float)total_inference_count;
		float percentage_loose = (float)num_loose / (float)total_inference_count;
		float percentage_cavitation = (float)num_cavitation / (float)total_inference_count;

		Serial.printf("Percentage healthy: %f\n", percentage_healthy);
		Serial.printf("Percentage loose: %f\n", percentage_loose);
		Serial.printf("Percentage cavitation: %f\n", percentage_cavitation);

		// Set RGB to indicate the health of the pump
		if (percentage_healthy >= percentage_loose && percentage_healthy >= percentage_cavitation)
		{
			Serial.println("Pump is healthy!");
			setGreen();
		}
		else if (percentage_loose >= percentage_healthy && percentage_loose >= percentage_cavitation)
		{
			Serial.println("Pump is loose!");
			setYellow();
		}
		else
		{
			Serial.println("Pump is cavitation!");
			setOrange();
		}
	}

	// Setting the mode for the next run
	preferences.begin("mode", false); // Open preferences with write access
	if (anomaly_detected)
	{
		// Switch over to classifier model to check
		preferences.putInt("mode", 1);
	}
	else
	{
		// Switch back to autoencoder model
		preferences.putInt("mode", 0);
	}
	preferences.end();
};

// **************** OTA Utility Functions ****************

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName)
{
	return header.substring(strlen(headerName.c_str()));
}

// Function to get flash ESP32 with new firmware
void execOTA()
{
	Serial.printf("Connecting to: %s\n", AWS_OTA_S3_HOST);
	// Connect to S3
	if (otaclient.connect(AWS_OTA_S3_HOST, AWS_OTA_S3_PORT))
	{
		// Connection Succeed.
		// Fecthing the bin
		Serial.printf("Fetching Bin: %s\n", String(AWS_OTA_S3_BIN));

		// Get the contents of the bin file
		String request = String("GET ");
		request += AWS_OTA_S3_BIN;
		request += " HTTP/1.1\r\nHost: ";
		request += AWS_OTA_S3_HOST;
		request += "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";

		otaclient.print(request);

		unsigned long timeout = millis();
		while (otaclient.available() == 0)
		{
			if (millis() - timeout > 5000)
			{
				Serial.println("Client Timeout !");
				otaclient.stop();
				return;
			}
		}

		while (otaclient.available())
		{
			String line = otaclient.readStringUntil('\n');
			line.trim();

			if (!line.length())
			{
				// headers ended
				break; // and get the OTA started
			}

			if (line.startsWith("HTTP/1.1"))
			{
				if (line.indexOf("200") < 0)
				{
					Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
					break;
				}
			}

			if (line.startsWith("Content-Length: "))
			{
				contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
				Serial.printf("Got %s bytes from server\n", String(contentLength));
			}

			// Next, the content type
			if (line.startsWith("Content-Type: "))
			{
				String contentType = getHeaderValue(line, "Content-Type: ");
				Serial.printf("Got %s payload\n", contentType.c_str());
				if (contentType == "application/octet-stream")
				{
					isValidContentType = true;
				}
			}
		}
	}
	else
	{
		Serial.printf("Connection to %s failed. Please check your setup\n", String(AWS_OTA_S3_HOST));
	}

	// Check what is the contentLength and if content type is `application/octet-stream`
	Serial.printf("contentLength : %s, isValidContentType : %s\n", String(contentLength), String(isValidContentType));

	// check contentLength and content type
	if (contentLength && isValidContentType)
	{
		// Check if there is enough to OTA Update
		bool canBegin = Update.begin(contentLength);

		// If yes, begin
		if (canBegin)
		{
			Serial.println("OTA in progress...");
			// No activity would appear on the Serial monitor
			// So be patient. This may take 2 - 5mins to complete
			size_t written = Update.writeStream(otaclient);

			if (written == contentLength)
			{
				Serial.printf("Written : %s successfully\n", String(written));
				Blynk.virtualWrite(OTA_VPIN, 0); // Set OTA_VPIN to 0
				Serial.printf("Setting OTA_VPIN to 0 to indicate OTA successful\n");
			}
			else
			{
				Serial.printf("Written only : %s/%s. Retry?\n", String(written), String(contentLength));
			}

			if (Update.end())
			{
				Serial.println("OTA done!");
				if (Update.isFinished())
				{
					Serial.println("Update successfully completed. Rebooting.");
					ESP.restart();
				}
				else
				{
					Serial.println("Update not finished? Something went wrong!");
				}
			}
			else
			{
				Serial.printf("Error Occurred. Error #: %s\n", String(Update.getError()));
			}
		}
		else
		{
			// not enough space to begin OTA
			Serial.println("Not enough space to begin OTA");
			otaclient.flush();
		}
	}
	else
	{
		Serial.println("There was no content in the response");
		otaclient.flush();
	}
}

// **************** AWS Utility Functions ****************
void publishToAWS()
{
	// Connect to AWS
	int numConnection = 0;
	Serial.println("Starting connection with AWS");
	if (aws_iot.connect(AWS_IOT_HOST, AWS_IOT_CLIENT_ID) != 0 && numConnection < 15)
	{
		Serial.println("Not connected to AWS, retrying...");
		numConnection++;
		delay(300);
	}
	else
	{
		Serial.println("Connected to AWS!");
	}

	String message = "";
	int num_success = 0;
	for (int i = 0; i < NUM_PER_SAMPLE; i++)
	{
		message += String(accelXVecVert[i]) + "," + String(accelYVecVert[i]) + "," + String(accelZVecVert[i]) + "," + String(accelXVecHori[i]) + "," + String(accelYVecHori[i]) + "," + String(accelZVecHori[i]) + "\n";
		message.toCharArray(payload, sizeof(payload));
		if (aws_iot.publish(AWS_IOT_MQTT_TOPIC, payload) == 0)
		{
			num_success++;
		}
		delay(100);
	}
	Serial.println("Number of successful publishes: " + String(num_success));
}

// **************** AC Current Functions ****************
float readACCurrentValue()
{
	float ACCurrentValue = 0;
	float peakVoltage = 0;
	float voltageVirtualValue = 0; // Vrms
	for (int i = 0; i < 20; i++)
	{
		peakVoltage += analogRead(ACPin); // read peak voltage
		delay(1);
	}
	peakVoltage = peakVoltage / 20;
	voltageVirtualValue = peakVoltage * 0.707; // change the peak voltage to the Virtual Value of voltage

	/*The circuit is amplified by 2 times, so it is divided by 2.*/
	voltageVirtualValue = (voltageVirtualValue / 1024 * VREF) / 2;

	ACCurrentValue = voltageVirtualValue * ACTectionRange;

	return ACCurrentValue;
}

// **************** Generic Functions ****************
void updateLatestTime()
{
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time, attempting to resync with NTP server...");
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
		return;
	}
	strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", &timeinfo);
}
