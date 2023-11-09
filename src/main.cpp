
#include <secrets.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "cnn_model_fullint_vibeonly.h"
#include "autoencoder_2dcnn_model_varying.h"
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
unsigned long long deep_sleep_time = 1000000ULL * 3 * 1 * 1; // Time to sleep in microseconds (10seconds)
int mode;													 // 0 for autoencoder, 1 for classifier

Preferences preferences; // To store the model to be used

// **************** RTC variables ****************
// NTP server details
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;
char str_time[100];

//***************** WiFi variables *****************
WiFiClient otaclient;

//***************** OTA variables *****************
// Variables to validate response from S3
long contentLength = 0;
bool isValidContentType = false;
volatile bool OTAAvailable = false;

//***************** AWS IoT variables *****************
AWS_IOT aws_iot;
char payload[100];
volatile bool sendToAWS = false;

// **************** Accelerometer variables ****************
#define ACCEL_SDA 5
#define ACCEL_SCL 4
#define NUM_PER_SAMPLE 1000				   // Number of data points per sample
int numSamples = 0;						   // To keep track of number of samples taken so far
int acc_timechecker = millis();			   // For checking if 1 second has passed
int accSamplingRate = 1000;				   // Sampling rate in Hz, max is 1000Hz
hw_timer_t *acc_timer = NULL;			   // Timer object
volatile bool isAccTimerTriggered = false; // For checking if timer triggered

// Offset values for the accelerometers
float offsetXVert = 65.058;
float offsetYVert = 10.662;
float offsetZVert = 1017.08398;
float offsetXHori = 5.171;
float offsetYHori = -1058.53503;
float offsetZHori = 38.316;

// Arrays to store the accelerometer data
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

// Set up the I2C bus for the accelerometers
TwoWire I2Cone = TwoWire(0);

// Instantiate the accelerometers objects
SPARKFUN_LIS2DH12 accelVert;
SPARKFUN_LIS2DH12 accelHori;

//****************** Temperature variables ******************
// #define TEMP_SDA 7
// #define TEMP_SCL 6

// TwoWire I2Ctwo = TwoWire(1);				// Set up the I2C bus for the temperature sensor
// DFRobot_MLX90614_I2C sensor(0x5A, &I2Ctwo); // Instantiate the temperature sensor object

float ambientTemp = 0.0;
float objectTemp = 0.0;

//***************** AC Current Sensor variables *****************
// #define ACPin 9			   // define the AC Current Sensor input analog pin
// #define ACTectionRange 20; // set Non-invasive AC Current Sensor tection range (5A,10A,20A)
// #define VREF 3.3

float currACValue = 0.0;

// **************** TF Lite variables ****************
// Details for model to be tested
#define AUTOENCODER_INPUT_SIZE 96	 // Per axis
#define CLASSIFICATION_INPUT_SIZE 96 // Per axis
#define NUM_INFERENCE_SAMPLES 100	 // Number of samples to run inference on before sending data
#define NUM_AXIS 6
#define NUM_CATEGORIES 3

// For both models
int total_inference_count = 0; // To keep track of total number of inferences run so far

// For classification model
int curr_num_healthy = 0;
int curr_num_loose = 0;
int curr_num_cavitation = 0;
int total_num_healthy = 0;
int total_num_loose = 0;
int total_num_cavitation = 0;
int max_index = 0;
float max_output = 0.0;

// For autoencoder model
float percentage_anomaly = 0.0;
float average_anomaly = 0.0;
float anomaly_score = 0.0;
float anomaly_threshold = 0.10; // Threshold for anomaly detection
int num_anomaly = 0;
bool anomaly_detected = false;

// Initialize memory pool for the TFLite model
constexpr int model_tensor_pool_size = 100 * 1024; // Allocate space for tensors to be loaded
alignas(16) uint8_t model_tensor_pool[model_tensor_pool_size];

const tflite::Model *ML_model;				 // Instantiate the model object
tflite::MicroInterpreter *model_interpreter; // Define the interpreter object
TfLiteTensor *model_input;					 // Set up input tensors
TfLiteTensor *model_output;					 // Set up output tensors

//****************** Generic Functions ******************
void ledSetup();
void wifiSetup();
void rtcSetup();
void updateRTCTime();
void blynkSetup();
bool checkBlynkConnection();
void loadPreferences();
void updatePreferences();
void resetCounters();
void interruptSetup();

//***************** RTC Functions *****************
void updateRTCTime();

//***************** RGB Functions *****************
void setRed();
void setGreen();
void setBlue();
void setPurple();
void setOrange();
void setYellow();
void setLightBlue();
void setWhite();
void offLED();

//***************** Blynk Functions *****************
BLYNK_CONNECTED()
{
	Blynk.syncVirtual(OTA_VPIN, SEND_AWS_VPIN, CAVITATION_COUNT_VPIN, HEALTHY_COUNT_VPIN, LOOSE_COUNT_VPIN, RESET_COUNTER_VPIN);
}

BLYNK_WRITE(OTA_VPIN) // To read in whether OTA is available from Blynk
{
	OTAAvailable = param.asInt();
}

BLYNK_WRITE(SEND_AWS_VPIN) // To read in whether to send data to AWS
{
	sendToAWS = param.asInt();
}

BLYNK_WRITE(CAVITATION_COUNT_VPIN) // To read in the number of cavitation events
{
	total_num_cavitation = param.asInt();
}

BLYNK_WRITE(HEALTHY_COUNT_VPIN) // To read in the number of healing events
{
	total_num_healthy = param.asInt();
}

BLYNK_WRITE(LOOSE_COUNT_VPIN) // To read in the number of loose events
{
	total_num_loose = param.asInt();
}

BLYNK_WRITE(RESET_COUNTER_VPIN)
{
	bool toReset = param.asInt();
	if (toReset)
	{
		resetCounters();
	}
}

void sendLatestTime(int VPIN) // To send the latest time data sent to Blynk
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}
	Blynk.virtualWrite(VPIN, str_time);
}

void sendFirmwareVersion()
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}
	Blynk.virtualWrite(FIRMWARE_VERSION_VPIN, FIRMWARE_VERSION);
}

void resetCounters() // To reset the counters on Blynk
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}

	num_anomaly = 0;
	total_num_healthy = 0;
	total_num_loose = 0;
	total_num_cavitation = 0;

	Blynk.virtualWrite(CAVITATION_COUNT_VPIN, 0);
	Blynk.virtualWrite(HEALTHY_COUNT_VPIN, 0);
	Blynk.virtualWrite(LOOSE_COUNT_VPIN, 0);
	Blynk.virtualWrite(RESET_COUNTER_VPIN, 0); // Set to 0 to indicate that it has been reset
}

void indicateOTASuccess()
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}
	Blynk.virtualWrite(OTA_VPIN, 0);
	resetCounters();
}

void sendInferenceResults()
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}

	if (mode == 0)
	{
		Blynk.virtualWrite(ANOMALY_PERCENTAGE_VPIN, average_anomaly * 100);
		Blynk.virtualWrite(NUM_ANOMALY_VPIN, num_anomaly);

		// For setting LED color on Blynk
		if (average_anomaly <= anomaly_threshold)
		{
			Blynk.virtualWrite(PUMP2_ANOMALY_VPIN, "Healthy");
		}
	}
	else
	{
		Blynk.virtualWrite(LATEST_CAVITATION_COUNT_VPIN, curr_num_cavitation);
		delay(10);
		Blynk.virtualWrite(LATEST_HEALTHY_COUNT_VPIN, curr_num_healthy);
		delay(10);
		Blynk.virtualWrite(LATEST_LOOSE_COUNT_VPIN, curr_num_loose);
		delay(10);
		Blynk.virtualWrite(HEALTHY_COUNT_VPIN, total_num_healthy);
		delay(10);
		Blynk.virtualWrite(LOOSE_COUNT_VPIN, total_num_loose);
		delay(10);
		Blynk.virtualWrite(CAVITATION_COUNT_VPIN, total_num_cavitation);
		delay(10);

		if (curr_num_healthy >= curr_num_loose && curr_num_healthy >= curr_num_cavitation)
		{
			Blynk.virtualWrite(PUMP2_ANOMALY_VPIN, "Healthy");
			delay(10);
		}
		else if (curr_num_loose >= curr_num_healthy && curr_num_loose >= curr_num_cavitation)
		{
			Blynk.virtualWrite(PUMP2_ANOMALY_VPIN, "Loose");
			delay(10);
		}
		else
		{
			Blynk.virtualWrite(PUMP2_ANOMALY_VPIN, "Cavitation");
			delay(10);
		}
	}
}

void sendTempReadings()
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}

	Blynk.virtualWrite(TEMP_AMBIENT_VPIN, ambientTemp);
	Blynk.virtualWrite(TEMP_OBJECT_VPIN, objectTemp);
}

void sendACReading()
{
	if (!checkBlynkConnection())
	{
		ESP.restart();
	}

	Blynk.virtualWrite(AC_READING_VPIN, currACValue);
}

//***************** OTA Functions *****************
bool isOTAAvail();
String getHeaderValue(String header, String headerName);
void execOTA();

//***************** Accelerometer Functions *****************
void accelSetup();
void updateAccelData(int updateIndex);
void normalizeAccelData();

//***************** Temperature Sensor Functions *****************
void tempSetup();
float getAmbientTemp();
float getObjectTemp();

//***************** AC Current Sensor Functions *****************
float getACCurrentValue();

//***************** AWS Functions *****************
void publishToAWS();

//***************** TFLite Functions *****************
void evaluateResults();
void loadMLModel();
void setModelInput(int curr_set);
void runModel();
void readModelOutput(int curr_set);

// *************** Interrupt handler ***************
void IRAM_ATTR onTimer() // To collect accelerometer data per 1ms
{
	isAccTimerTriggered = true; // Set flag to read accelerometer data
}

void setup()
{
	Serial.begin(115200); // Start serial at 115200 baud

	ledSetup(); // Setup of LED to show program status
	delay(100);

	wifiSetup(); // Connect to network
	delay(100);

	rtcSetup(); // Setup of RTC to get local time
	delay(100);

	setBlue(); // Set RGB to blue to indicate successful network setup

	blynkSetup(); // Connect to Blynk
	delay(100);

	sendLatestTime(LATEST_CONNECT_TIME_VPIN);
	delay(100);

	if (isOTAAvail()) // OTA if new firmware is available
	{
		Serial.println("OTA is available");
		setPurple(); // To indicate OTA is available
		execOTA();
	}
	else
	{
		Serial.println("OTA is not available");
	}

	sendFirmwareVersion();

	// Sensors setup // !! UNCOMMENT WHEN SENSORS ARE CONNECTED !!
	accelSetup();
	// tempSetup();

	loadPreferences(); // Load the preferences for the ML model

	loadMLModel();

	interruptSetup(); // Setup of interrupt to collect accelerometer data

	delay(100);
}

void loop()
{
	Blynk.run();					  // To keep Blynk connection alive
	if (numSamples == NUM_PER_SAMPLE) // Running of inference once data collected
	{
		if (mode == 0) // For autoencoder model
		{
			setLightBlue();
		}
		else // For classification model
		{
			setWhite();
		}

		// Test the model with some dummy data
		// Serial.println("Testing model with dummy data...");

		// Normalizing the accelerometer data first
		normalizeAccelData();

		// Each sample of 1000ms can be split into 10 samples of 100ms for inference
		int curr_set = 0;
		while (curr_set < 10)
		{
			Blynk.run(); // To keep Blynk connection alive

			setModelInput(curr_set);

			runModel();

			readModelOutput(curr_set);

			curr_set++;
			total_inference_count++;
			Serial.println("Current inference count: " + String(curr_set));
			delay(10);
		}

		numSamples = 0; // Reset the number of samples taken
		Serial.println("Total inference count: " + String(total_inference_count));
		isAccTimerTriggered = false; // Reset the timer trigger
		setBlue();					 // Set LED to blue to show inference done
	}

	if (isAccTimerTriggered && (numSamples < NUM_PER_SAMPLE)) // Collecting of data per 1ms
	{
		isAccTimerTriggered = false;

		updateAccelData(numSamples);

		// ********** Dummy data **********
		// accelXVecVert[numSamples] = 0.0;
		// accelXVecHori[numSamples] = 0.0;
		// accelYVecVert[numSamples] = 0.0;
		// accelYVecHori[numSamples] = 0.0;
		// accelZVecVert[numSamples] = 0.0;
		// accelZVecHori[numSamples] = 0.0;

		numSamples++; // Increment the number of samples taken
	}

	if (total_inference_count >= NUM_INFERENCE_SAMPLES) // Processing of results after running inference
	{
		Serial.println("Processing results");
		evaluateResults();
		sendInferenceResults();
		updatePreferences();

		// Update latest time on Blynk
		updateRTCTime();
		delay(10);
		sendLatestTime(LATEST_UPDATE_TIME_VPIN);

		// !! UNCOMMENT WHEN SENSORS ARE CONNECTED !!
		// float currACValue = getACCurrentValue();
		// float ambientTemp = getAmbientTemp();
		// float objectTemp = getObjectTemp();

		// // !! FAKE RANDOMIZED DATA !!
		currACValue = 3.104 + (double)esp_random() / (UINT32_MAX) * (3.296 - 3.104);		 // RNG for AC current
		ambientTemp = 35.0455 + (double)esp_random() / (UINT32_MAX) * (38.7345 - 35.0455); // RNG for ambient temp
		objectTemp = 44.0515 + (double)esp_random() / (UINT32_MAX) * (48.6885 - 44.0515);	 // RNG for object temp

		// Serial.println("Preparing to send temp and current data to Blynk...");
		// Sending of current and temp values to Blynk
		sendACReading();
		sendTempReadings();
		// Serial.println("Done sending temp and current data to Blynk");

		if (sendToAWS) // If user wants to send data to AWS
		{
			Serial.println("Sending to AWS...");
			publishToAWS();
		}
		else
		{
			Serial.println("Not sending to AWS");
		}

		// Set to deep sleep to save power
		Serial.println("Going into deep sleep...");
		esp_sleep_enable_timer_wakeup(deep_sleep_time);
		delay(3000);
		offLED();
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

void setWhite()
{
	neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS);
}

void setLightBlue()
{
	neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, RGB_BRIGHTNESS);
}

void offLED()
{
	neopixelWrite(RGB_BUILTIN, 0, 0, 0);
}
// **************** Accelerometer Utility Functions ****************
void accelSetup()
{
	I2Cone.begin(ACCEL_SDA, ACCEL_SCL);
	Serial.println("I2Cone begin");
	I2Cone.setClock(1000000);
	delay(500);
	Serial.println("I2Cone setup done");

	if (accelVert.begin(0x18, I2Cone) == false)
	{
		Serial.println("Accelerometer 1 not detected. Check address jumper and wiring. Restarting...");
		Blynk.virtualWrite(ACCEL1_CONNECTION_VPIN, 0); // Indicate on Blynk
		delay(100);
		ESP.restart();
	}

	Blynk.virtualWrite(ACCEL1_CONNECTION_VPIN, 1); // Update accel 1 connection status on Blynk

	if (accelHori.begin(0x19, I2Cone) == false)
	{
		Serial.println("Accelerometer 2 not detected. Check address jumper and wiring. Restarting...");
		Blynk.virtualWrite(ACCEL2_CONNECTION_VPIN, 0); // Indicate on Blynk
		delay(100);
		ESP.restart();
	};

	Blynk.virtualWrite(ACCEL2_CONNECTION_VPIN, 1); // Update accel 2 connection status on Blynk

	accelVert.setScale(LIS2DH12_2g);							  // Set full-scale range to 2g
	accelVert.setMode(LIS2DH12_HR_12bit);						  // Set operating mode to low power
	accelVert.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz

	accelHori.setScale(LIS2DH12_2g);							  // Set full-scale range to 2g
	accelHori.setMode(LIS2DH12_HR_12bit);						  // Set operating mode to low power
	accelHori.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz
}

// To get the readings from the accelerometer & update min and max values for normalizing
void updateAccelData(int updateIndex)
{
	// Get accelerometer data and update min and max for normalizing
	float currXVert = accelVert.getX() - offsetXVert;
	float currYVert = accelVert.getY() - offsetYVert;
	float currZVert = accelVert.getZ() - offsetZVert;
	float currXHori = accelHori.getX() - offsetXHori;
	float currYHori = accelHori.getY() - offsetYHori;
	float currZHori = accelHori.getZ() - offsetZHori;

	// Update the min and max values for each axis (Vertical)
	maxAccelXVert = (currXVert > maxAccelXVert) ? currXVert : maxAccelXVert;
	minAccelXVert = (currXVert < minAccelXVert) ? currXVert : minAccelXVert;
	maxAccelYVert = (currYVert > maxAccelYVert) ? currYVert : maxAccelYVert;
	minAccelYVert = (currYVert < minAccelYVert) ? currYVert : minAccelYVert;
	maxAccelZVert = (currZVert > maxAccelZVert) ? currZVert : maxAccelZVert;
	minAccelZVert = (currZVert < minAccelZVert) ? currZVert : minAccelZVert;

	// Update the min and max values for each axis (Horizontal)
	maxAccelXHori = (currXHori > maxAccelXHori) ? currXHori : maxAccelXHori;
	minAccelXHori = (currXHori < minAccelXHori) ? currXHori : minAccelXHori;
	maxAccelYHori = (currYHori > maxAccelYHori) ? currYHori : maxAccelYHori;
	minAccelYHori = (currYHori < minAccelYHori) ? currYHori : minAccelYHori;
	maxAccelZHori = (currZHori > maxAccelZHori) ? currZHori : maxAccelZHori;
	minAccelZHori = (currZHori < minAccelZHori) ? currZHori : minAccelZHori;

	// Update the accelerometer data arrays
	accelXVecVert[updateIndex] = currXVert;
	accelXVecHori[updateIndex] = currXHori;
	accelYVecVert[updateIndex] = currYVert;
	accelYVecHori[updateIndex] = currYHori;
	accelZVecVert[updateIndex] = currZVert;
	accelZVecHori[updateIndex] = currZHori;
}

// Normalize the accelerometer data and reset the min and max values for each axis
void normalizeAccelData()
{
	// Normalize the accelerometer data between -1 and 1
	for (int i = 0; i < NUM_PER_SAMPLE; i++)
	{
		accelXVecVert[i] = ((accelXVecVert[i] - minAccelXVert) / (maxAccelXVert - minAccelXVert)) * 2 - 1;
		accelYVecVert[i] = ((accelYVecVert[i] - minAccelYVert) / (maxAccelYVert - minAccelYVert)) * 2 - 1;
		accelZVecVert[i] = ((accelZVecVert[i] - minAccelZVert) / (maxAccelZVert - minAccelZVert)) * 2 - 1;
		accelXVecHori[i] = ((accelXVecHori[i] - minAccelXHori) / (maxAccelXHori - minAccelXHori)) * 2 - 1;
		accelYVecHori[i] = ((accelYVecHori[i] - minAccelYHori) / (maxAccelYHori - minAccelYHori)) * 2 - 1;
		accelZVecHori[i] = ((accelZVecHori[i] - minAccelZHori) / (maxAccelZHori - minAccelZHori)) * 2 - 1;
	}

	// Reset the min and max values
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
};

// **************** Temp sensor Utility Functions ****************
// void tempSetup()
// {
// 	I2Ctwo.begin(TEMP_SDA, TEMP_SCL);
// 	I2Ctwo.setClock(1000000);
// 	delay(500);
// 	// Init the temperature sensor
// 	if (NO_ERR != sensor.begin())
// 	{
// 		Serial.println("Communication with temperature sensor failed, please check connection");
// 		Blynk.virtualWrite(TEMP_CONNECTION_VPIN, 0);
// 		delay(100);
// 		ESP.restart();
// 	}

// 	Blynk.virtualWrite(TEMP_CONNECTION_VPIN, 1); // Update temp sensor connection status on Blynk
// 	Serial.println("Temperature sensor init successful!");
// }

// float getAmbientTemp()
// {
// 	return sensor.getAmbientTempCelsius();
// }

// float getObjectTemp()
// {
// 	return sensor.getObjectTempCelsius();
// }

// **************** TFLite Utility Functions ****************
void loadMLModel()
{
	if (mode == 0) // Load autoencoder model if mode == 0
	{
		Serial.println("Loading autoencoder model....");
		ML_model = tflite::GetModel(autoencoder_2dcnn_model_varying_tflite); // !! TESTING 2D CNN MODEL
	}
	else
	{ // Load classifier model if mode == 1
		Serial.println("Loading classifier model....");
		ML_model = tflite::GetModel(cnn_model_fullint_vibeonly_tflite); // Correct final model to be used
	}

	// Load the model into the interpreter
	static tflite::AllOpsResolver model_resolver;
	static tflite::ErrorReporter *model_error_reporter;
	static tflite::MicroErrorReporter model_micro_error;
	model_error_reporter = &model_micro_error;

	// Instantiate the interpreter
	static tflite::MicroInterpreter model_static_interpreter(
		ML_model, model_resolver, model_tensor_pool, model_tensor_pool_size, model_error_reporter);
	model_interpreter = &model_static_interpreter;

	// Allocate the the model's tensors in the memory pool that was created
	if (model_interpreter->AllocateTensors() != kTfLiteOk)
	{
		Serial.printf("Model provided is schema version %d not equal to supported version %d.\n",
					  ML_model->version(), TFLITE_SCHEMA_VERSION);
		Serial.println("There was an error allocating the memory...ooof");
		delay(10000);
		ESP.restart();
	}

	// Define input and output nodes
	model_input = model_interpreter->input(0);
	model_output = model_interpreter->output(0);

	if (mode == 0)
	{
		Serial.println("Autoencoder model loaded and ready to go!");
	}
	else
	{
		Serial.println("Classifier model loaded and ready to go!");
	}
}

void setModelInput(int curr_set)
{
	if (mode == 0) // For autoencoder model
	{
		for (int i = 0; i < AUTOENCODER_INPUT_SIZE; i++)
		{
			model_input->data.f[i] = accelXVecVert[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
			model_input->data.f[i + AUTOENCODER_INPUT_SIZE] = accelYVecVert[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
			model_input->data.f[i + (2 * AUTOENCODER_INPUT_SIZE)] = accelZVecVert[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
			model_input->data.f[i + (3 * AUTOENCODER_INPUT_SIZE)] = accelXVecHori[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
			model_input->data.f[i + (4 * AUTOENCODER_INPUT_SIZE)] = accelYVecHori[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
			model_input->data.f[i + (5 * AUTOENCODER_INPUT_SIZE)] = accelZVecHori[i + (curr_set * AUTOENCODER_INPUT_SIZE)];
		}
	}
	else // For classification model
	{
		for (int i = 0; i < CLASSIFICATION_INPUT_SIZE; i++)
		{
			model_input->data.f[i] = accelXVecVert[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
			model_input->data.f[i + CLASSIFICATION_INPUT_SIZE] = accelYVecVert[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
			model_input->data.f[i + (2 * CLASSIFICATION_INPUT_SIZE)] = accelZVecVert[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
			model_input->data.f[i + (3 * CLASSIFICATION_INPUT_SIZE)] = accelXVecHori[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
			model_input->data.f[i + (4 * CLASSIFICATION_INPUT_SIZE)] = accelYVecHori[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
			model_input->data.f[i + (5 * CLASSIFICATION_INPUT_SIZE)] = accelZVecHori[i + (curr_set * CLASSIFICATION_INPUT_SIZE)];
		}
	}
}

void runModel() // Run inference on the model
{
	TfLiteStatus invoke_status = model_interpreter->Invoke();
	if (invoke_status != kTfLiteOk)
	{
		Serial.println("There was an error invoking the interpreter...ooof");
		delay(3000);
		esp_restart();
	}
}

void readModelOutput(int curr_set)
{
	if (mode == 0) // For autoencoder model
	{
		anomaly_score = model_output->data.f[0];
		Serial.print("Anomaly score: ");
		Serial.println(anomaly_score);
		average_anomaly += anomaly_score;
	}
	else
	{
		for (int i = 0; i < NUM_CATEGORIES; i++)
		{
			float curr_data = model_output->data.f[i];
			// For debugging
			Serial.print("Output data: ");
			Serial.println(curr_data);

			if (curr_data > max_output)
			{
				max_output = curr_data;
				max_index = i;
			}
		}

		max_output = 0.0; // Reset the max_output
	}

	// Processing output data based on model
	if (mode == 0)
	{
		// Check if the score is above the threshold
		if (anomaly_score > anomaly_threshold)
		{
			num_anomaly++;
		}

		anomaly_score = 0.0; // Reset the anomaly score
	}
	else
	{
		// Check if the output is healthy, loose or cavitation
		if (max_index == 1)
		{
			curr_num_healthy++;
			total_num_healthy++;
			Serial.println("Healthy");
		}
		else if (max_index == 2)
		{
			curr_num_loose++;
			total_num_loose++;
			Serial.println("Loose");
		}
		else
		{
			curr_num_cavitation++;
			total_num_cavitation++;
			Serial.println("Cavitation");
		}
	}
}

// Evaluate the results of the inference and update EEPROM to change mode based on result
// Mode will always be changed back to autoencoder after running the classifier once
void evaluateResults()
{
	if (mode == 0) // Processing of results for autoencoder model
	{
		average_anomaly = (float)average_anomaly / (float)total_inference_count;
		Serial.print("Average anomaly: ");
		Serial.println(average_anomaly);
		if (average_anomaly > anomaly_threshold)
		{
			Serial.println("Anomaly detected!");
			anomaly_detected = true;
			setRed(); // To indicate anomaly detected
		}
		else
		{
			Serial.println("No anomaly detected!");
			anomaly_detected = false;
			setGreen(); // To indicate no anomaly detected
		}
	}
	else // Processing of results for classifier model
	{
		Serial.println("Healthy count: " + String(curr_num_healthy));
		Serial.println("Loose count: " + String(curr_num_loose));
		Serial.println("Cavitation count: " + String(curr_num_cavitation));

		if (curr_num_healthy >= curr_num_loose && curr_num_healthy >= curr_num_cavitation)
		{
			Serial.println("Pump is healthy!");
			setGreen();
		}
		else if (curr_num_loose >= curr_num_healthy && curr_num_loose >= curr_num_cavitation)
		{
			Serial.println("Pump is loose!");
			setOrange();
		}
		else
		{
			Serial.println("Pump is cavitation!");
			setRed();
		}
	}
};

// **************** OTA Utility Functions ****************
bool isOTAAvail()
{
	Blynk.syncVirtual(OTA_VPIN);
	delay(100);
	return (OTAAvailable) ? true : false;
}

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
				indicateOTASuccess();
				setGreen(); // To indicate that OTA was successful
			}
			else
			{
				Serial.printf("Written only : %s/%s. Retry?\n", String(written), String(contentLength));
				// Set to red when unsuccessful
				neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);
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
					setRed(); // Set to red when unsuccessful
				}
			}
			else
			{
				Serial.printf("Error Occurred. Error #: %s\n", String(Update.getError()));
				setRed(); // Set to red when unsuccessful
			}
		}
		else
		{
			// not enough space to begin OTA
			Serial.println("Not enough space to begin OTA");
			otaclient.flush();
			setRed(); // Set to red when unsuccessful
		}
	}
	else
	{
		Serial.println("There was no content in the response");
		otaclient.flush();
		setRed(); // Set to red when unsuccessful
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
// float getACCurrentValue()
// {
// 	float ACCurrentValue = 0;
// 	float peakVoltage = 0;
// 	float voltageVirtualValue = 0; // Vrms
// 	for (int i = 0; i < 20; i++)
// 	{
// 		peakVoltage += analogRead(ACPin); // read peak voltage
// 		delay(1);
// 	}
// 	peakVoltage = peakVoltage / 20.0;
// 	voltageVirtualValue = peakVoltage * 0.707; // change the peak voltage to the Virtual Value of voltage

// 	/*The circuit is amplified by 2 times, so it is divided by 2.*/
// 	voltageVirtualValue = (voltageVirtualValue / 1024 * VREF) / 2.0;

// 	ACCurrentValue = voltageVirtualValue * ACTectionRange;

// 	return analogRead(ACPin);
// }

// **************** Generic Functions ****************
void updateRTCTime()
{
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time, attempting to resync with NTP server...");
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
		return;
	}
	strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", &timeinfo);
}

bool checkBlynkConnection()
{
	int num_attempts = 0;
	while (!Blynk.connected() && num_attempts < 15)
	{
		Serial.println("Blynk not connected, reconnecting...");
		Blynk.connect();
		num_attempts++;
		delay(100);
	}

	return (num_attempts < 15) ? true : false;
}

void ledSetup()
{
	// Setup RGB LED
	pinMode(RGB_BUILTIN, OUTPUT);
	neopixelWrite(RGB_BUILTIN, 0, 0, 0);
}

void wifiSetup()
{
	int numConnection = 0;
	WiFi.setAutoReconnect(true);
	WiFi.mode(WIFI_STA);
	delay(100);
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
	Serial.println();
}

void rtcSetup()
{
	int numAttempts = 0;
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	delay(100);
	while (!getLocalTime(&timeinfo) && numAttempts < 15)
	{
		Serial.println("Failed to obtain time, attempting to resync with NTP server...");
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
		numAttempts++;
		delay(100);
	}

	if (numAttempts >= 15) // Check if time was obtained successfully
	{
		Serial.println("Failed to obtain time");
	}

	strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", &timeinfo);
	Serial.println(str_time);
}

void blynkSetup()
{
	Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD, "blynk.cloud", 8080);
	delay(500);
	if (Blynk.connected())
	{
		Serial.println("Blynk Connected");
	}
	else
	{
		Serial.println("Blynk not connected, restarting ESP32");
		ESP.restart();
	}
}

void loadPreferences()
{
	preferences.begin("mode", false);
	mode = preferences.getInt("mode", 0);
	preferences.end();
}

void updatePreferences()
{
	// Setting the mode for the next run
	preferences.begin("mode", false); // Open preferences with write access
	// !! ACTUAL METHOD TO CHANGE MODE !!
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

	// !! TOGGLING MODE FOR TESTING !!
	// if (mode == 0)
	// {
	// 	preferences.putInt("mode", 1);
	// }
	// else
	// {
	// 	preferences.putInt("mode", 0);
	// }
	preferences.end();
}

void interruptSetup()
{
	acc_timer = timerBegin(0, 80, true);				  // Begin timer with 1 MHz frequency (80MHz/80)
	timerAttachInterrupt(acc_timer, &onTimer, true);	  // Attach the interrupt to Timer1
	unsigned int timerFactor = 1000000 / accSamplingRate; // Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
	timerAlarmWrite(acc_timer, timerFactor, true);		  // Initialize the timer
	timerAlarmEnable(acc_timer);
	Serial.println("Timer interrupt setup done!");
}
