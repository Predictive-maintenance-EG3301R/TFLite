
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
#include "SparkFun_LIS2DH12.h"
#include "DFRobot_MLX90614.h"
// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>
#include <Update.h>
#include "time.h"

using namespace std;

//***************** Generic variables *****************
// #define RESET_TIMER 1000ULL * 60 * 60 * 24 // Reset ESP32 every 24 hours
unsigned long long deep_sleep_time = 1000000ULL * 10 * 1 * 1; // Time to sleep in microseconds (10seconds)
Preferences preferences;
int mode; // 0 for autoencoder, 1 for classifier

//***************** WiFi/OTA variables *****************
WiFiClient client;

// Variables to validate response from S3
long contentLength = 0;
bool isValidContentType = false;

//***************** Blynk variables *****************
volatile bool OTAAvailable = false;

// **************** Accelerometer variables ****************
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
void IRAM_ATTR onTimer()
{
	isAccTimerTriggered = true; // Indicates that the interrupt has been entered since the last time its value was changed to false
}

// **************** TF Lite variables ****************
// Details for model to be tested
#define AUTOENCODER_INPUT_SIZE 100
#define CLASSIFICATION_INPUT_SIZE 576
#define NUM_SAMPLES 200
#define NUM_CATEGORIES 3

// For both
vector<float> output_data;

// For classification
int num_healthy = 0;
int num_loose = 0;
int num_cavitation = 0;

// For autoencoder
float mse_threshold = 0.02774564472135648; // Value obtained from training
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

//***************** Blynk Functions *****************
BLYNK_CONNECTED()
{
	Blynk.syncVirtual(OTA_VPIN);
}

BLYNK_WRITE(OTA_VPIN) // To read in whether OTA is available from Blynk
{
	OTAAvailable = param.asInt();
}

//***************** OTA Functions *****************
String getHeaderValue(String header, String headerName);
void execOTA();

// Set up the ESP32's environment.
void setup()
{
	// Start serial at 115200 baud
	Serial.begin(115200);

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

	// Check for OTA first
	// ********** Blynk setup **********
	Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD, "blynk.cloud", 8080);
	if (Blynk.connected())
	{
		Serial.println("Blynk Connected");
		delay(100);
		Blynk.syncVirtual(OTA_VPIN);
	}
	else
	{
		Serial.println("Blynk Not Connected");
	}

	// ************ Execute OTA Update if available ************
	if (OTAAvailable)
	{
		Serial.println("OTA is available");
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
	Serial.println("Starting inferences");
}

// Logic loop for taking user input and outputting the FFT
void loop()
{
	Blynk.run();
	// Check amount of free heap space initially
	Serial.print("Free heap before running model: ");
	Serial.println(String(ESP.getFreeHeap()));

	// Test the model with some dummy data
	Serial.println("Testing model with dummy data...");

	// Run the model for NUM_SAMPLES times
	int curr_sample = 0;
	while (curr_sample < NUM_SAMPLES)
	{
		if (mode == 0)
		{
			// Load the dummy data into the interpreter
			for (int i = 0; i < AUTOENCODER_INPUT_SIZE; i++)
			{
				model_input->data.f[i] = 0.0;
			}
		}
		else
		{
			// Load the dummy data into the interpreter
			for (int i = 0; i < CLASSIFICATION_INPUT_SIZE; i++)
			{
				model_input->data.f[i] = 0.0;
			}
		}

		// Run inference on the model
		TfLiteStatus invoke_status = model_interpreter->Invoke();
		if (invoke_status != kTfLiteOk)
		{
			Serial.println("There was an error invoking the interpreter...ooof");
			esp_deep_sleep_start();
		}

		// Get the output from the model
		if (mode == 0)
		{
			for (int i = 0; i < AUTOENCODER_INPUT_SIZE; i++)
			{
				output_data.push_back(model_output->data.f[i]);
			}
		}
		else
		{
			for (int i = 0; i < CLASSIFICATION_INPUT_SIZE; i++)
			{
				output_data.push_back(model_output->data.f[i]);
			}
		}

		if (mode == 0)
		{
			// Calculate the mse
			float output_mse = 0.0;
			for (int i = 0; i < AUTOENCODER_INPUT_SIZE; i++)
			{
				output_mse += pow((1.0 - output_data[i]), 2);
			}
			output_mse /= AUTOENCODER_INPUT_SIZE;

			// Check if the mse is above the threshold
			if (output_mse > mse_threshold)
			{
				num_anomaly++;
			}
		}
		else
		{
			// Check if the output is healthy, loose or cavitation
			float max_output = -10e9;
			int max_index = 0;
			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				if (output_data[i] > max_output)
				{
					max_output = output_data[i];
					max_index = i;
				}
			}

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

		output_data.clear();
		curr_sample++;
		// Serial.println("Inference count: " + String(curr_sample));
		delay(10);
	}

	Serial.println("Inferences done, processing results now");
	// Process the results
	if (mode == 0)
	{
		float percentage_anomaly = (float)num_anomaly / (float)NUM_SAMPLES;
		if (percentage_anomaly > 0.5)
		{
			Serial.println("Anomaly detected!");
			anomaly_detected = true;
		}
		else
		{
			Serial.println("No anomaly detected!");
		}
	}
	else
	{
		float percentage_healthy = (float)num_healthy / (float)NUM_SAMPLES;
		float percentage_loose = (float)num_loose / (float)NUM_SAMPLES;
		float percentage_cavitation = (float)num_cavitation / (float)NUM_SAMPLES;
		Serial.printf("Percentage healthy: %f\n", percentage_healthy);
		Serial.printf("Percentage loose: %f\n", percentage_loose);
		Serial.printf("Percentage cavitation: %f\n", percentage_cavitation);
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

	// Check amount of free heap space after running model
	Serial.print("Free heap after running model: ");
	Serial.println(String(ESP.getFreeHeap()));
	Serial.println("Going into deep sleep...");
	esp_sleep_enable_timer_wakeup(deep_sleep_time);
	esp_deep_sleep_start();
}

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName)
{
	return header.substring(strlen(headerName.c_str()));
}

// **************** OTA Utility Functions ****************
// Function to get flash ESP32 with new firmware
void execOTA()
{
	Serial.printf("Connecting to: %s\n", S3_HOST);
	// Connect to S3
	if (client.connect(S3_HOST, S3_PORT))
	{
		// Connection Succeed.
		// Fecthing the bin
		Serial.printf("Fetching Bin: %s\n", String(S3_BIN));

		// Get the contents of the bin file
		String request = String("GET ");
		request += S3_BIN;
		request += " HTTP/1.1\r\nHost: ";
		request += S3_HOST;
		request += "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";

		client.print(request);

		unsigned long timeout = millis();
		while (client.available() == 0)
		{
			if (millis() - timeout > 5000)
			{
				Serial.println("Client Timeout !");
				client.stop();
				return;
			}
		}

		while (client.available())
		{
			String line = client.readStringUntil('\n');
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
		Serial.printf("Connection to %s failed. Please check your setup\n", String(S3_HOST));
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
			size_t written = Update.writeStream(client);

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
			client.flush();
		}
	}
	else
	{
		Serial.println("There was no content in the response");
		client.flush();
	}
}
