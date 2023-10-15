#include <Wire.h>
#include <SPI.h>
#include "SparkFun_LIS2DH12.h"
#include "DFRobot_MLX90614.h"
// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Google_Sheet_Client.h>
#include <secrets.h>
#include <vector>
#include "time.h"

using namespace std;

#define NUM_PER_SEND 50		  // Number of rows to update at once, needs to be a factor of 1000
#define NUM_PER_SAMPLE 1000	  // Number of data points per sample
#define DELAY_PER_SEND 100	  // Delay between each batch of data sent to Google Sheets
#define DELAY_PER_SAMPLE 1000 // Delay between each data point collected

// NTP server details
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;
char str_time[100];

// Accelerometer variables
int acc_counter = 0;
int acc_timechecker = millis();
volatile bool isAccTimerTriggered = false; // For checking if timer triggered
int accSamplingRate = 1000;				   // Sampling rate in Hz, max is 1000Hz
hw_timer_t *acc_timer = NULL;			   // Timer object
int currSample = 0;						   // Current sample number
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

// *********** Accelerometers variables ***********
SPARKFUN_LIS2DH12 accelVert; // Create instance
SPARKFUN_LIS2DH12 accelHori; // Create instance
// Adafruit_MPU6050 mpu;

TwoWire I2Cone = TwoWire(0);
// TwoWire I2Ctwo = TwoWire(1);

// DFRobot_MLX90614_I2C sensor(0x5A, &I2Ctwo); // instantiate an object to drive the temp sensor

// For current sensor
const int ACPin = 35;	   // set arduino signal read pin
#define ACTectionRange 20; // set Non-invasive AC Current Sensor tection range (5A,10A,20A)
#define VREF 3.3

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

// For Google sheets debugging
void tokenStatusCallback(TokenInfo info);

#define GOOGLE_SHEETS_DELAY 0.25 // Number of seconds between each API call to Google Sheets
								 // API limit is 300 requests per minute per project for read and write separately
String SHEET_NAME = "Sheet1";	 // Name of the sheet to be used

// For editing Google sheets
volatile unsigned long currRowNumber = 11;
int lastRow = 900000;
int numConnection = 0;

/**
 * @brief Timer used to collect data for FFT.
 */
void IRAM_ATTR onTimer()
{
	isAccTimerTriggered = true; // Indicates that the interrupt has been entered since the last time its value was changed to false
}

// To update the latest datetime
void updateLatestTime()
{
	int num_attempts = 0;
	if (!getLocalTime(&timeinfo) && num_attempts < 5)
	{
		Serial.println("Failed to obtain time, attempting to resync with NTP server...");
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
		num_attempts++;
		delay(1000);
	}
	strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", &timeinfo);
}

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

// **************** Google Sheets functions ****************
// To update new offset values obtained from calibration to Google Sheets
void sendNewOffsets()
{
	FirebaseJson updateOffset;
	FirebaseJson updateResponse;

	// Updating of offset values
	String updateOffsetCol = SHEET_NAME;
	updateOffsetCol.concat("!C4:H4");
	updateOffset.add("range", updateOffsetCol);
	updateOffset.add("majorDimension", "ROWS");
	updateOffset.set("values/[0]/[0]", offsetXVert);
	updateOffset.set("values/[0]/[1]", offsetYVert);
	updateOffset.set("values/[0]/[2]", offsetZVert);
	updateOffset.set("values/[0]/[3]", offsetXHori);
	updateOffset.set("values/[0]/[4]", offsetYHori);
	updateOffset.set("values/[0]/[5]", offsetZHori);

	bool successOffset = GSheet.values.update(&updateResponse, // Returned response
											  SPREADSHEET_ID,  // Spreadsheet ID to update
											  updateOffsetCol, // Range of data to update
											  &updateOffset);  // Data to update

	if (successOffset)
	{
		Serial.println("New offsets sent to Google Sheets!");
	}
	else
	{
		Serial.println("Unable to send new offsets to Google Sheets, restarting ESP32...");
		ESP.restart();
	}

	Serial.println();
	updateResponse.toString(Serial, true);
	Serial.print("Free heap: ");
	Serial.println(ESP.getFreeHeap());
	Serial.println("New offsets sent to Google Sheets!");
	Serial.println();
	delay(1000);
}

void googleSheetsSetup()
{
	// *********** Setting up of Google Sheets ***********
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
	GSheet.begin(CLIENT_EMAIL, PROJECT_ID, GOOGLESHEETS_PRIVATE_KEY);

	// Send new offsets to Google Sheets before reading old offsets
	sendNewOffsets();

	/*
	 * Get config details from row 2
	 * Col A: Current row number
	 * Col B: Threshold
	 * Col C: Old offsetXVert
	 * Col D: Old offsetYVert
	 * Col E: Old offsetZVert
	 * Col F: Old offsetXHori
	 * Col G: Old offsetYHori
	 * Col H: Old offsetZHori
	 */
	FirebaseJson responseConfig;
	FirebaseJsonData dataConfig;
	String configRange = SHEET_NAME;
	configRange.concat("!A2:H2");

	bool successConfig = GSheet.values.get(&responseConfig, /* returned response */
										   SPREADSHEET_ID,	/* spreadsheet Id to read */
										   configRange		/* range to read */
	);
	// responseConfig.toString(Serial, true); // To view the entire response
	Serial.println();

	if (!successConfig)
	{
		Serial.println("Unable to read in config values, restarting ESP32...");
		ESP.restart();
	}

	// Get the current row number
	responseConfig.get(dataConfig, "values/[0]/[0]");
	currRowNumber = dataConfig.to<long>();

	// Restart if unable to get row number properly
	Serial.printf("Row number to read in: %d\n", currRowNumber);
	currRowNumber = (((int)currRowNumber / (int)1000) * 1000) + 11; // Set to nearest 1000 + 11
	Serial.printf("Row number to start: %d\n", currRowNumber);

	// Get the model threshold value
	responseConfig.get(dataConfig, "values/[0]/[1]");
	double THRESHOLD = dataConfig.to<double>();
	Serial.printf("Threshold: %f\n", THRESHOLD);

	// Get the old offset values
	responseConfig.get(dataConfig, "values/[0]/[2]");
	offsetXVert = dataConfig.to<double>();
	responseConfig.get(dataConfig, "values/[0]/[3]");
	offsetYVert = dataConfig.to<double>();
	responseConfig.get(dataConfig, "values/[0]/[4]");
	offsetZVert = dataConfig.to<double>();
	responseConfig.get(dataConfig, "values/[0]/[5]");
	offsetXHori = dataConfig.to<double>();
	responseConfig.get(dataConfig, "values/[0]/[6]");
	offsetYHori = dataConfig.to<double>();
	responseConfig.get(dataConfig, "values/[0]/[7]");
	offsetZHori = dataConfig.to<double>();

	// Use hardcoded values if unable to read in old config values
	if (offsetXVert < 0.1 && offsetXVert > -0.1)
	{
		Serial.println("Unable to read in old offsetXVert, using hardcoded value instead...");
		offsetXVert = 68.172;
	}
	if (offsetYVert < 0.1 && offsetYVert > -0.1)
	{
		Serial.println("Unable to read in old offsetYVert, using hardcoded value instead...");
		offsetYVert = 3.213;
	}
	if (offsetZVert < 0.1 && offsetZVert > -0.1)
	{
		Serial.println("Unable to read in old offsetZVert, using hardcoded value instead...");
		offsetZVert = 1013.33801;
	}
	if (offsetXHori < 0.1 && offsetXHori > -0.1)
	{
		Serial.println("Unable to read in old offsetXHori, using hardcoded value instead...");
		offsetXHori = 24.436;
	}
	if (offsetYHori < 0.1 && offsetYHori > -0.1)
	{
		Serial.println("Unable to read in old offsetYHori, using hardcoded value instead...");
		offsetYHori = -1064.93298;
	}
	if (offsetZHori < 0.1 && offsetZHori > -0.1)
	{
		Serial.println("Unable to read in old offsetZHori, using hardcoded value instead...");
		offsetZHori = 28.983;
	}

	Serial.printf("Old Vertical calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXVert), String(offsetYVert), String(offsetZVert));
	Serial.printf("Old Horizontal calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXHori), String(offsetYHori), String(offsetZHori));
	delay(1000);
}

// For sending data to Google sheets
void sendAllDataGoogleSheets()
{
	FirebaseJson updateTime;
	FirebaseJson updateResponse;
	/*
	 * Update the Google Sheets with the 9 columns of new data, from A to I
	 * Col A: Time
	 * Col B: accelXVecVert
	 * Col C: accelYVecVert
	 * Col D: accelZVecVert
	 * Col E: accelXVecHori
	 * Col F: accelYVecHori
	 * Col G: accelZVecHori
	 * Col H: ambientTemp
	 * Col I: objectTemp
	 * Col J: current
	 */

	// Updating of time first
	updateLatestTime();
	String updateTimeCol = SHEET_NAME;
	updateTimeCol.concat("!A");
	updateTimeCol.concat(String(currRowNumber));
	updateTime.add("range", updateTimeCol);
	updateTime.add("majorDimension", "ROWS");
	updateTime.set("values/[0]/[0]", str_time);
	bool successTime = GSheet.values.update(&updateResponse, // Returned response
											SPREADSHEET_ID,	 // Spreadsheet ID to update
											updateTimeCol,	 // Range of data to update
											&updateTime);	 // Data to update

	// Updating of sensor values
	// float ambientTemp = sensor.getAmbientTempCelsius();
	// float objectTemp = sensor.getObjectTempCelsius();
	float current = readACCurrentValue();

	float ambientTemp = 0.0;
	float objectTemp = 0.0;
	// float current = 0.0;

	int numRowWritten = 0;
	int num_looped = 0;

	while (numRowWritten < NUM_PER_SAMPLE)
	{ // To write to Google Sheets in batches of 40 rows
		FirebaseJson updateOutput;
		FirebaseJson updateCurrentRow;
		FirebaseJson updateMSE;

		String dataSheetRange = SHEET_NAME;
		dataSheetRange.concat("!B"); // Start from column B
		dataSheetRange.concat(String(currRowNumber));
		dataSheetRange.concat(":J"); // End at column J
		dataSheetRange.concat(String(currRowNumber + NUM_PER_SEND - 1));
		updateOutput.add("range", dataSheetRange);
		updateOutput.add("majorDimension", "ROWS");

		for (int i = 0; i < NUM_PER_SEND; i++)
		{
			String updateXVert = "values/[";
			updateXVert.concat(String(i));
			updateXVert.concat("]/[0]");
			updateOutput.set(updateXVert, accelXVecVert[i + (num_looped * NUM_PER_SEND)]);

			String updateYVert = "values/[";
			updateYVert.concat(String(i));
			updateYVert.concat("]/[1]");
			updateOutput.set(updateYVert, accelYVecVert[i + (num_looped * NUM_PER_SEND)]);

			String updateZVert = "values/[";
			updateZVert.concat(String(i));
			updateZVert.concat("]/[2]");
			updateOutput.set(updateZVert, accelZVecVert[i + (num_looped * NUM_PER_SEND)]);

			String updateXHori = "values/[";
			updateXHori.concat(String(i));
			updateXHori.concat("]/[3]");
			updateOutput.set(updateXHori, accelXVecHori[i + (num_looped * NUM_PER_SEND)]);

			String updateYHori = "values/[";
			updateYHori.concat(String(i));
			updateYHori.concat("]/[4]");
			updateOutput.set(updateYHori, accelYVecHori[i + (num_looped * NUM_PER_SEND)]);

			String updateZHori = "values/[";
			updateZHori.concat(String(i));
			updateZHori.concat("]/[5]");
			updateOutput.set(updateZHori, accelZVecHori[i + (num_looped * NUM_PER_SEND)]);

			String updateAmbientTemp = "values/[";
			updateAmbientTemp.concat(String(i));
			updateAmbientTemp.concat("]/[6]");
			updateOutput.set(updateAmbientTemp, ambientTemp);

			String updateObjectTemp = "values/[";
			updateObjectTemp.concat(String(i));
			updateObjectTemp.concat("]/[7]");
			updateOutput.set(updateObjectTemp, objectTemp);

			String updateCurrent = "values/[";
			updateCurrent.concat(String(i));
			updateCurrent.concat("]/[8]");
			updateOutput.set(updateCurrent, current);
		}

		// To update latest row number
		currRowNumber += NUM_PER_SEND;
		numRowWritten += NUM_PER_SEND;
		num_looped++;
		String updateCurrentRowRange = SHEET_NAME;
		updateCurrentRowRange.concat("!A2");
		updateCurrentRow.add("range", updateCurrentRowRange);
		updateCurrentRow.add("majorDimension", "ROWS");
		updateCurrentRow.set("values/[0]/[0]", currRowNumber);

		// Appending data + row number to update all at once
		FirebaseJsonArray updateArr;
		updateArr.add(updateCurrentRow);
		updateArr.add(updateOutput);

		bool success = GSheet.values.batchUpdate(&updateResponse, // Returned response
												 SPREADSHEET_ID,  // Spreadsheet ID to update
												 &updateArr);	  // Array of data to update

		if (!success)
		{
			Serial.println("Unable to send data to Google Sheets, restarting ESP32...");
			ESP.restart();
		}

		// updateResponse.toString(Serial, true);
		Serial.print("Free heap: ");
		Serial.println(ESP.getFreeHeap());
		Serial.print("Number of rows written: ");
		Serial.println(numRowWritten);
		Serial.println();

		delay(DELAY_PER_SEND);
	}

	Serial.printf("Current row number: %d\n", currRowNumber);
	Serial.printf("Delay for %d seconds to prevent calling API too frequently...\n", DELAY_PER_SAMPLE);
	delay(DELAY_PER_SAMPLE);
	isAccTimerTriggered = false;
}

// Set up the ESP32's environment.
void setup()
{
	// Start serial at 115200 baud
	Serial.begin(115200);

	// Use to easily indicate if WiFi is connected
	pinMode(LED_BUILTIN, OUTPUT);

	// **************** Accelerometer setup ****************
	I2Cone.begin(21, 22);
	Serial.println("I2Cone begin");
	I2Cone.setClock(1000000);
	delay(200);

	// **************** Temperature sensor setup ****************
	// I2Ctwo.begin(16, 17);
	// I2Ctwo.setClock(1000000);
	// delay(200);

	// **************** MPU6050 setup ****************
	// // Try to initialize!
	// if (!mpu.begin()) {
	// 	Serial.println("Failed to find MPU6050 chip");
	// 	while (1) {
	// 	delay(10);
	// 	}
	// }
	// Serial.println("MPU6050 Found!");

	// mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
	// Serial.print("Accelerometer range set to: ");
	// switch (mpu.getAccelerometerRange()) {
	// case MPU6050_RANGE_2_G:
	// 	Serial.println("+-2G");
	// 	break;
	// case MPU6050_RANGE_4_G:
	// 	Serial.println("+-4G");
	// 	break;
	// case MPU6050_RANGE_8_G:
	// 	Serial.println("+-8G");
	// 	break;
	// case MPU6050_RANGE_16_G:
	// 	Serial.println("+-16G");
	// 	break;
	// }
	// mpu.setGyroRange(MPU6050_RANGE_500_DEG);
	// Serial.print("Gyro range set to: ");
	// switch (mpu.getGyroRange()) {
	// case MPU6050_RANGE_250_DEG:
	// 	Serial.println("+- 250 deg/s");
	// 	break;
	// case MPU6050_RANGE_500_DEG:
	// 	Serial.println("+- 500 deg/s");
	// 	break;
	// case MPU6050_RANGE_1000_DEG:
	// 	Serial.println("+- 1000 deg/s");
	// 	break;
	// case MPU6050_RANGE_2000_DEG:
	// 	Serial.println("+- 2000 deg/s");
	// 	break;
	// }

	// mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
	// Serial.print("Filter bandwidth set to: ");
	// switch (mpu.getFilterBandwidth()) {
	// case MPU6050_BAND_260_HZ:
	// 	Serial.println("260 Hz");
	// 	break;
	// case MPU6050_BAND_184_HZ:
	// 	Serial.println("184 Hz");
	// 	break;
	// case MPU6050_BAND_94_HZ:
	// 	Serial.println("94 Hz");
	// 	break;
	// case MPU6050_BAND_44_HZ:
	// 	Serial.println("44 Hz");
	// 	break;
	// case MPU6050_BAND_21_HZ:
	// 	Serial.println("21 Hz");
	// 	break;
	// case MPU6050_BAND_10_HZ:
	// 	Serial.println("10 Hz");
	// 	break;
	// case MPU6050_BAND_5_HZ:
	// 	Serial.println("5 Hz");
	// 	break;
	// }

	// Serial.println("");
	// delay(100);
	// **************** End of MPU6050 setup ****************

	// **************** Sparkfun LIS2DH12 setup ****************
	if (accelVert.begin(0x18, I2Cone) == false)
	{
		Serial.println("Accelerometer 1 not detected. Check address jumper and wiring. Freezing...");
		while (1)
			;
	}

	if (accelHori.begin(0x19, I2Cone) == false)
	{
		Serial.println("Accelerometer 2 not detected. Check address jumper and wiring. Freezing...");
		while (1)
			;
	};

	// // Init the temperature sensor
	// while (NO_ERR != sensor.begin())
	// {
	// 	Serial.println("Communication with temperature sensor failed, please check connection");
	// 	delay(3000);
	// }
	// Serial.println("Temperature sensor init successful!");

	accelVert.setScale(LIS2DH12_2g);							  // Set full-scale range to 2g
	accelVert.setMode(LIS2DH12_HR_12bit);						  // Set operating mode to low power
	accelVert.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz

	accelHori.setScale(LIS2DH12_2g);							  // Set full-scale range to 2g
	accelHori.setMode(LIS2DH12_HR_12bit);						  // Set operating mode to low power
	accelHori.setDataRate(LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP); // Set data rate to 1Khz Hz

	Serial.println("Calibrating accelerometer 1. Do not move the board.");
	for (int i = 0; i < 1000; i++)
	{
		offsetXVert += accelVert.getX();
		offsetYVert += accelVert.getY();
		offsetZVert += accelVert.getZ();
		delay(2);
	}

	Serial.println("Calibrating accelerometer 2. Do not move the board.");
	for (int i = 0; i < 1000; i++)
	{
		offsetXHori += accelHori.getX();
		offsetYHori += accelHori.getY();
		offsetZHori += accelHori.getZ();
		delay(2);
	}

	offsetXVert /= 1000.0;
	offsetYVert /= 1000.0;
	offsetZVert /= 1000.0;

	offsetXHori /= 1000.0;
	offsetYHori /= 1000.0;
	offsetZHori /= 1000.0;

	Serial.printf("Vertical calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXVert), String(offsetYVert), String(offsetZVert));
	Serial.printf("Horizontal calibration = \"%s\", \"%s\", \"%s\"\n", String(offsetXHori), String(offsetYHori), String(offsetZHori));
	delay(2000);
	// ********** End of Sparkfun LIS2DH12 setup **********

	// ********** Timer Interrupt setup **********
	// accelVert Timer setup
	acc_timer = timerBegin(0, 80, true);				  // Begin timer with 1 MHz frequency (80MHz/80)
	timerAttachInterrupt(acc_timer, &onTimer, true);	  // Attach the interrupt to Timer1
	unsigned int timerFactor = 1000000 / accSamplingRate; // Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
	timerAlarmWrite(acc_timer, timerFactor, true);		  // Initialize the timer
	timerAlarmEnable(acc_timer);

	// ********************** Google Sheets portion ************************
	googleSheetsSetup();

	// Init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	delay(1000);
}

// Logic loop for taking user input and outputting the FFT
void loop()
{
	// ********************** Accelerometer portion ************************
	// **** MPU setup ****
	// sensors_event_t a, g, temp;
	// mpu.getEvent(&a, &g, &temp);
	// **** End of MPU setup ****

	if (currRowNumber >= lastRow)
	{
		// When hit limit, copy over entire sheet to another copy, then restart ESP and write from first row again
		Serial.println("Copying over entire sheet to another copy...");
		FirebaseJson copyResponse;
		bool copysuccess = GSheet.sheets.copyTo(&copyResponse,		// returned response
												SPREADSHEET_ID,		// Spreadsheet ID to copy from
												0,					// Sheet ID
												COPY_SPREADSHEET_ID // Spreadsheet ID to copy to
		);

		delay(2000);
		// copyResponse.toString(Serial, true);

		// Write current row number = 11 to Google Sheets
		FirebaseJson updateCurrentRow;
		FirebaseJson updateResponse;
		String updateCurrentRowRange = SHEET_NAME;
		updateCurrentRowRange.concat("!A2");
		updateCurrentRow.add("range", updateCurrentRowRange);
		updateCurrentRow.add("majorDimension", "ROWS");
		updateCurrentRow.set("values/[0]/[0]", 11);

		bool success = false;
		do
		{
			Serial.println("Updating of current row number to 11...");
			success = GSheet.values.update(&updateResponse,		  // Returned response
										   SPREADSHEET_ID,		  // Spreadsheet ID to update
										   updateCurrentRowRange, // Range of data to update
										   &updateCurrentRow);	  // Array of data to update
			delay(2000); // Wait for 2 seconds before trying again
		} while (!success);

		// updateResponse.toString(Serial, true);

		Serial.println("Updating of current row number to 11 successful!");
		Serial.println("Restarting ESP32...");
		ESP.restart();
	}

	// Print accelVert values only if new data is available
	if (millis() - acc_timechecker >= 1000 && currSample == NUM_PER_SAMPLE)
	{
		Serial.printf("Number of samples collected: %d\n", currSample);
		// double MSE = getARInference(accelYVecVert);
		sendAllDataGoogleSheets();

		delay(1000);
		acc_timechecker = millis();
		currSample = 0; // Reset currSample
	}

	if (isAccTimerTriggered && (currSample < NUM_PER_SAMPLE))
	{
		isAccTimerTriggered = false;

		// ********** MPU6050 **********
		// // float y_value = a.acceleration.y;
		// accelXVecVert.push_back(a.acceleration.x);
		// accelXVecHori.push_back(a.acceleration.x);

		// accelYVecVert.push_back(a.acceleration.y);
		// accelYVecHori.push_back(a.acceleration.y);

		// accelZVecVert.push_back(a.acceleration.z);
		// accelZVecHori.push_back(a.acceleration.z);

		// // // For normalizing the accelerometer data
		// // if (y_value > maxAccelYVert) {
		// // 	maxAccelYVert = y_value;
		// // }
		// // if (y_value < minAccelYVert) {
		// // 	minAccelYVert = y_value;
		// // }

		// ********** Sparkfun LIS2DH12 **********
		accelXVecVert[currSample] = accelVert.getX() - offsetXVert;
		accelXVecHori[currSample] = accelHori.getX() - offsetXHori;

		accelYVecVert[currSample] = accelVert.getY() - offsetYVert;
		accelYVecHori[currSample] = accelHori.getY() - offsetYHori;

		accelZVecVert[currSample] = accelVert.getZ() - offsetZVert;
		accelZVecHori[currSample] = accelHori.getZ() - offsetZHori;

		// ********** Dummy data **********
		// accelXVecVert[currSample] = esp_random() % esp_random();
		// accelXVecHori[currSample] = esp_random() % esp_random();

		// accelYVecVert[currSample] = esp_random() % esp_random();
		// accelYVecHori[currSample] = esp_random() % esp_random();

		// accelZVecVert[currSample] = esp_random() % esp_random();
		// accelZVecHori[currSample] = esp_random() % esp_random();

		currSample++;
	}
}