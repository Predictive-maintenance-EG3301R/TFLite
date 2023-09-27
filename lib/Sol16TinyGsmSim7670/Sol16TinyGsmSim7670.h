#ifndef SOL16TINYGSMSIM7600_H
#define SOL16TINYGSMSIM7600_H

// Lilygo default setup
#define TINY_GSM_MODEM_SIM7600   // Define which TinyGsm library to use
#define TINY_GSM_DEBUG SerialMon // Define the serial console for debug prints
#define SerialMon Serial         // Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialAT Serial1         // Set serial for AT commands (to the module)
#define UART_BAUD 115200

// Lilygo default pins
#define MODEM_TX 26
#define MODEM_RX 27
#define MODEM_PWR 4
#define MODEM_DTR 25
#define MODEM_RI 33
#define BATT_ADC 35

#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

#define BAT_EN       12
#define RESET        5

// #define LED_PIN 12

// Time constant
#define uS_TO_S_FACTOR 1000000ULL // Conversion factor for micro seconds to seconds

#include <TinyGsmClient.h>
#include <time.h>
#include <vector>

struct gps
{
  float lat = 0;
  float lon = 0;
  float spd = 0;
  float alt = 0;
  int vsat = 0;
  int usat = 0;
  float acc = 0;
  int gps_year;
  int gps_month;
  int gps_day;
  int gps_hour;
  int gps_min;
  int gps_sec;
};

class Sol16TinyGsmSim7670 : public TinyGsmSim7600
{
public:
  // Inherit constructors from the base lib
  using TinyGsmSim7600::TinyGsmSim7600;

  /**
   * @brief Puts the device to light sleep. Only ULP Coprocessor and RTC kept active.
   * ESP32 core is paused.
   * WiFi, Bluetooth, Radio and other peripherals are all inactive.
   * Device would wake up once set number of seconds has passed.
   *
   * @param sec Number of seconds to put device to light sleep
   */
  void light_sleep(uint32_t sec);

  /**
   * @brief Modem setup. Pulls required pins high to ensure modulator works properly.
   * Turn off LED to save battery.
   * Automatically calls init_modem() and init_lte() and init_lte()
   *
   * @param led Set to true to on LED. Default set to false to save battery
   */
  void setup(bool led = false);

  /**
   * @brief Initialise the modem to allow data to be sent. 
   *
   * @return True if modem able to initialise successfullly, false otherwise
   */
  bool init_modem();

  /**
   * @brief Initialise the 4G capability of the SIM7600
   *
   * @param apn APN of network. Default is empty
   * @param gprs_user GPRS username. Default is empty
   * @param gprs_pass  GPRS password. Deault is empty
   */
  void init_lte(const char apn[] = "", const char gprs_user[] = "", const char gprs_pass[] = "");

  // /**
  //  * @brief Initiliase GPS capability of SIM7600.
  //  * Required only if GPS is needed later on.
  //  */
  // void init_gps();

  // /**
  //  * @brief Sends AT command to update GPS related information.
  //  */
  // bool update_gps();

  // /**
  //  * @brief Get the latest updated gps object
  //  *
  //  * @return gps Object which holds all the latest gps information
  //  */
  // gps get_latest_gps();

  /**
   * @brief Retrieve the latest datetime information in str format.
   * Formatted in "day of month, abbreviated month name, year, 12-hour clock time"
   * e.g 09 Sep 2022 12:26:29 PM
   * @return String containing latest datetime
   */
  String get_str_datetime();

  /**
   * @brief Retrieve the latest datetime information in tm struct.
   *
   * @return tm struct containing latest datetime
   */
  tm get_tm_datetime();

  /**
   * @brief Reads the latest battery voltage remaining and stores in vector for averaging.
   */
  void update_batt();

  /**
   * @brief Get the remaining voltage of the battery attached to the back of the lilygo.
   * Average is taken from all the data points stored in vector.
   * Vector containing the datapoints is cleared whenever this function is called.
   *
   * @return Remaining battery voltage (V)
   */
  double get_remaining_batt();

  /**
   * @brief Puts lilygo into deepsleep. Automatically wakes up once it passes set duration.
   * 
   * @param duration Number of seconds lilygo goes into deep sleep
   */
  void hibernate(unsigned long long duration);

private:
  /**
   * @brief Sends AT command to get latest time.
   * Updated datetime information is stored in private tm struct.
   * This function is called at the start of get_str_datetime() and get_tm_datetime().
   * Use either get_str_datetime() or get_tm_datetime() to retrieve the latest datetime.
   */
  void update_curr_datetime();

  char _datetimeinfo[100];       // array to store datetime
  tm _curr_datetime;             // tm struct to hold current time
  float _timezone;               // holds timezone information received
  std::vector<double> _batt_vec; // vector to store battery level
  gps _gps_info;                 // struct to hold all the GPS data
};

#endif