#include "Sol16TinyGsmSim7670.h"
#include <TinyGsmClient.h>
#include <numeric>
#include <time.h>

void Sol16TinyGsmSim7670::light_sleep(uint32_t sec)
{
  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
  DBG("Lilygo going into light sleep for", sec, "seconds");
  esp_light_sleep_start();
}

void Sol16TinyGsmSim7670::setup(bool led)
{
  // if BAT_EN is LOW, battery won't power GSM
  pinMode(BAT_EN, OUTPUT);
  digitalWrite(BAT_EN, HIGH);

  //A7670 Reset
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, LOW);
  delay(100);
  digitalWrite(RESET, HIGH);
  delay(3000);
  digitalWrite(RESET, LOW);

  pinMode(MODEM_PWR, OUTPUT);
  digitalWrite(MODEM_PWR, LOW);
  delay(100); 
  digitalWrite(MODEM_PWR, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWR, LOW);

  delay(3000);
    
  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  int connect_attempts = 0;
  while (!init_modem())
  {
    connect_attempts++;
    if (connect_attempts >= 15)
    {
      // 15 attempts failed, enter deep sleep for 15 minutes
      DBG("15 attempts failed, entering deep sleep for 15 minutes");
      esp_sleep_enable_timer_wakeup(15 * 60 * 1000000);
      esp_deep_sleep_start();
    }
    else
    {
      DBG("Unable to init modem, going to light sleep for 2 secs");
      light_sleep(2);
    }
  }
  
  init_lte();
}

bool Sol16TinyGsmSim7670::init_modem()
{
  DBG("Initializing modem...");
  if (!this->init())
  {
    DBG("Unable to init modem");
    DBG("Failed to restart modem, delaying 2s and retrying");
    // return false;
  }
  return true;
}

void Sol16TinyGsmSim7670::init_lte(const char apn[], const char gprs_user[], const char gprs_pass[])
{
  /*  Preferred mode selection : AT+CNMP
      2 – Automatic
      13 – GSM Only
      14 – WCDMA Only
      38 – LTE Only
      59 – TDS-CDMA Only
      9 – CDMA Only
      10 – EVDO Only
      19 – GSM+WCDMA Only
      22 – CDMA+EVDO Only
      48 – Any but LTE
      60 – GSM+TDSCDMA Only
      63 – GSM+WCDMA+TDSCDMA Only
      67 – CDMA+EVDO+GSM+WCDMA+TDSCDMA Only
      39 – GSM+WCDMA+LTE Only
      51 – GSM+LTE Only
      54 – WCDMA+LTE Only
  */
  String ret;
  do
  {
    ret = this->setNetworkMode(2);
    delay(500);
  } while (!ret);
  // ret = modem.setNetworkMode(2);
  DBG("setNetworkMode:", ret);

  String name = this->getModemName();
  DBG("Modem Name:", name);

  String modemInfo = this->getModemInfo();
  DBG("Modem Info:", modemInfo);

  // Detect network before moving on
  DBG("Waiting for network...");
  while (!this->waitForNetwork(600000L))
  {
    light_sleep(1);
  }

  bool res;
  do
  {
    res = this->isNetworkConnected();
    DBG("Network status:", res ? "connected" : "not connected");
  } while (!res);

  DBG("Connecting to", apn);
  while (!this->gprsConnect(apn, gprs_user, gprs_pass))
  {
    light_sleep(1);
  }

  do
  {
    res = this->isGprsConnected();
    DBG("GPRS status:", res ? "connected" : "not connected");
  } while (!res);

  String ccid = this->getSimCCID();
  DBG("CCID:", ccid);

  String imei = this->getIMEI();
  DBG("IMEI:", imei);

  String imsi = this->getIMSI();
  DBG("IMSI:", imsi);

  String cop = this->getOperator();
  DBG("Operator:", cop);

  IPAddress local = this->localIP();
  DBG("Local IP:", local);

  int csq = this->getSignalQuality();
  DBG("Signal quality:", csq);
}

// void Sol16TinyGsmSim7670::init_gps()
// {
//   /**
//       CGNSSMODE: <gnss_mode>,<dpo_mode>
//       This command is used to configure GPS, GLONASS, BEIDOU and QZSS support mode.
//       gnss_mode:
//           0 : GLONASS
//           1 : BEIDOU
//           2 : GALILEO
//           3 : QZSS
//       dpo_mode :
//           0 disable
//           1 enable
//   */
//   Serial.println("GNSS mode set");
//   this->setGNSSMode(0, 1);
//   light_sleep(1);

//   // https://github.com/vshymanskyy/TinyGSM/pull/405
//   uint8_t mode = this->getGNSSMode();
//   DBG("GNSS Mode:", mode);

//   DBG("Enabling GPS/GNSS/GLONASS");
//   bool res;
//   do
//   {
//     res = this->enableGPS();
//     DBG("GPS status:", res ? "enabled" : "not enabled");
//   } while (!res);
// }

// bool Sol16TinyGsmSim7670::update_gps()
// {
//   bool updated = false;
//   DBG("Requesting current GPS/GNSS/GLONASS location");
//   for (int i = 0; i < 5; i++)
//   {
//     if (this->getGPS(&_gps_info.lat, &_gps_info.lon, &_gps_info.spd, &_gps_info.alt, &_gps_info.vsat,
//                      &_gps_info.usat, &_gps_info.acc, &_gps_info.gps_year, &_gps_info.gps_month,
//                      &_gps_info.gps_day, &_gps_info.gps_hour, &_gps_info.gps_min, &_gps_info.gps_sec))
//     {
//       if (_gps_info.vsat >= 4)
//       {
//         DBG("Successfully updated GPS data");
//         DBG("Number of sats detected:", _gps_info.vsat);
//         DBG("Current Longitude:", _gps_info.lon);
//         DBG("Current Latitude:", _gps_info.lat);
//         updated = true;
//       }
//       else
//       {
//         DBG("Unable to get accurate GPS info");
//         DBG("Number of sats detected:", _gps_info.vsat);
//       }
//       break;
//     }
//   }
//   if (!updated)
//   {
//     DBG("Unable to obtain GPS information");
//   }
//   return updated;
// }

// gps Sol16TinyGsmSim7670::get_latest_gps()
// {
//   return _gps_info;
// }

void Sol16TinyGsmSim7670::update_curr_datetime()
{
  for (int8_t i = 5; i; i--)
  {
    DBG("Requesting current network time");
    if (this->getNetworkTime(&_curr_datetime.tm_year, &_curr_datetime.tm_mon, &_curr_datetime.tm_mday,
                             &_curr_datetime.tm_hour, &_curr_datetime.tm_min, &_curr_datetime.tm_sec, &_timezone))
    {
      // DBG("Year:", _curr_datetime.tm_year, "\tMonth:", _curr_datetime.tm_mon, "\tDay:", _curr_datetime.tm_mday);
      // DBG("Hour:", _curr_datetime.tm_hour, "\tMinute:", _curr_datetime.tm_min, "\tSecond:", _curr_datetime.tm_sec);
      // DBG("Timezone:", _timezone);
      DBG("Successfully updated current datetime");
      break;
    }
    else
    {
      DBG("Couldn't get network time, retrying in 5s.");
      light_sleep(5);
    }
  }
}

String Sol16TinyGsmSim7670::get_str_datetime()
{
  update_curr_datetime();
  tm str_dt = _curr_datetime;
  str_dt.tm_year -= 1900;
  str_dt.tm_mon -= 1;
  strftime(_datetimeinfo, 100, "%d %b %Y\t%r", &str_dt);
  DBG("Latest datetime in str format is", _datetimeinfo);
  return String(_datetimeinfo);
}

tm Sol16TinyGsmSim7670::get_tm_datetime()
{
  update_curr_datetime();
  DBG("Returning tm struct with updated datetime info");
  return _curr_datetime;
}

void Sol16TinyGsmSim7670::update_batt()
{
  int vref = 1.1;
  uint16_t volt = analogRead(BATT_ADC);
  double battery_voltage = ((double)volt / 4095.0) * 2.0 * 3.3 * (vref);
  DBG("Remaining batt voltage:", battery_voltage);
  _batt_vec.push_back(battery_voltage);
}

double Sol16TinyGsmSim7670::get_remaining_batt()
{
  if (_batt_vec.empty())
  {
    DBG("Battery vector is empty!");
    return 0.0;
  }
  else
  {
    double avg_volt = std::accumulate(_batt_vec.begin(), _batt_vec.end(), 0.0) / _batt_vec.size();
    DBG("Average remaining voltage is", avg_volt);
    _batt_vec.clear();
    return avg_volt;
  }
}

// To minimise power consumption
void Sol16TinyGsmSim7670::hibernate(unsigned long long duration)
{
  this->poweroff(); // Off modem to conserve battery

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
  esp_sleep_enable_timer_wakeup(duration * uS_TO_S_FACTOR);
  DBG("Lilygo entering deep sleep now, waking up in", duration, "seconds");
  esp_deep_sleep_start();
}