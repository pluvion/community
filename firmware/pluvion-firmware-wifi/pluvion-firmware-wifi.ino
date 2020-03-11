/**
 * Pluvi.On WiFi Firmware
 * 
 *       FILE: pluvion_firwmware_wifi.ino
 *    VERSION: 2.0.0.W
 *    LICENSE: Creative Commons 4
 *    AUTHORS:
 *             Pedro Godoy <pedro@pluvion.com.br>
 *             Hugo Santos <hugo@pluvion.com.br>
 * 
 *       SITE: https://www.pluvion.com.br
 */
#include <FS.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <PluviOn.h>
PluviOn utils;

/////ATTENTION!!!! CHANGES SHOULD BE DONE IN COMPILATION TIME///
///BEGIN
// STATION CONFIG
const char *ssid           = "PG_hotspot";
const char *password       = "0102030405";
const String STATION_ID    = "31";
#define  SLEEP_PERIOD      900000 //15 minutes
#define SERVER_ADDR        "3.87.153.3"
#define SERVER_PORT      10000
///END
/////ATTENTION!!!! CHANGES SHOULD BE DONE IN COMPILATION TIME///

// WIFI
WiFiClient wf_client;

// SYSTEM SLEEP CONTROL
unsigned long sleepCounter = 0; // Sleep time counter


// SYSTEM DEFINES
#define SYSTEM_BAUDRATE 115200
const String DATA_DIR = "/data";
bool fs_is_active = false;

// PINS DEFINES
#define PIN_RESET_MONO D1 // pin reset mono
#define PIN_DHT D5        // pin where the dht22 is connected
#define PIN_SYSTEM_FORMAT D6 // pin full reset
#define PIN_HALL D7       // pin where Hall sensor is connected
#define PIN_RTC_SDA GPIO_ID_PIN(2)
#define PIN_RTC_SCL GPIO_ID_PIN(4)

// TEMPERATURE + HUMIDITY SENSOR
#define DHT_TYPE DHT22 //DHT type
DHT dht(PIN_DHT, DHT_TYPE, 20); // Initialize DHT sensor
#define DHT_SENSOR_READING_DELAY 600000
#define DHT_SENSOR_READING_FAILURE_DELAY 180000
#define DHT_SENSOR_READING_FAILURE_ATTEMPTS_LIMIT 2
unsigned long lastDHTReadingInMillis = 0;    // The last DHT reading time in ms
unsigned long currentDHTReadingInMillis = 0; // Current DHT reading in millis
boolean DHTSensorReadingFailure = false;
int DHTSensorFailedReadingAttempts = 0;
volatile float humidity = 0;          // Read air humidity in %
volatile float temperature = 0;       // Read temperature as Celsius
volatile float computedHeatIndex = 0; // Compute heat index in Celsius

// RAIN SENSOR
#define DELAY_HALL_SENSOR_DEBOUNCING 100 // Delay in ms to debounce hall sensor
volatile int tipCounter = 0;             // Store the bucket tip counter
volatile int realTipCount = 0;           // Checked tip counter
volatile int lastTipCount = 0;           // The last tip count total
unsigned long curBucketReadingInMs = 0;  // Current rain sensor reading in millis
unsigned long lastBucketReadingInMs = 0; // The last rain sensor reading time in ms
const float CONTRIBUTION_AREA = 7797.0; // Contribuition area in mm2
volatile float rainVolume = 0;          // The rain accumulated volume

void init_dht() {
  dht.begin();
}

void count_tip_bucket() {
  tipCounter++;
  Serial.print(F("[count_tip_bucket] - Tip counter: "));
  Serial.println(tipCounter);
  Serial.print(F("[count_tip_bucket] - Real tip count: "));
  Serial.println(realTipCount);
  Serial.print(F("[count_tip_bucket] - Last tip count: "));
  Serial.println(lastTipCount);
  detachInterrupt(PIN_HALL);
  read_hall_sensor();
  attach_interruption();
  }

void attach_interruption() {
  Serial.println(F("[attach_interruption] - Attaching"));
  attachInterrupt(PIN_HALL, count_tip_bucket, FALLING);
  Serial.println(F("[attach_interruption] - Done"));
}

void read_hall_sensor() {
  // Current millis mark
  curBucketReadingInMs = millis();

  // Check if the configured delay has passed
  if (
      ((curBucketReadingInMs - lastBucketReadingInMs) > DELAY_HALL_SENSOR_DEBOUNCING) || // delay
      (lastBucketReadingInMs == 0)                                                       // first time
  ) {
    // Any increment since the last time?
    if (tipCounter > lastTipCount) {

      // Increase the tip bucket count
      realTipCount++;
      Serial.print(F("REAL TIP COUNT: "));
      Serial.println(realTipCount);

      // Update last tip counter value
      lastTipCount = tipCounter;
    }

    // Store the current millis for the next iteration
    lastBucketReadingInMs = curBucketReadingInMs;
  }
}

void read_dht_sensor() {
  // Current millis mark
  currentDHTReadingInMillis = millis();

  if (
    DHTSensorReadingFailure &&                                                                    // Fail
    ((currentDHTReadingInMillis - lastDHTReadingInMillis) >= DHT_SENSOR_READING_FAILURE_DELAY) && // The time hasn' passed yet
    (DHTSensorFailedReadingAttempts <= DHT_SENSOR_READING_FAILURE_ATTEMPTS_LIMIT)                 // Number of attempts
  ) {
    Serial.println(F("[read_dht_sensor] - DHT reading failure"));
    Serial.print(F("[read_dht_sensor] - DHT reading retries: "));
    Serial.println(DHTSensorFailedReadingAttempts);

    DHTSensorFailedReadingAttempts++;

    Serial.println(F("[read_dht_sensor] - Reading DHT..."));
    get_dht_data();
  }

  // Check if the configured delay has passed
  if ((currentDHTReadingInMillis - lastDHTReadingInMillis) >= DHT_SENSOR_READING_DELAY || // delay
    (lastDHTReadingInMillis == 0)                                                         // first time
  ) {
    Serial.println(F("[read_dht_sensor] - Reading DHT..."));
    get_dht_data();
  }
}

void get_dht_data() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();

  // Read temperature as Celsius
  temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("[get_dht_data] - ERROR - Failed to read from DHT sensor!"));

    // Indicates a failure in readings
    DHTSensorReadingFailure = true;

    // Fix humidity, temperature and computed heat index
    humidity = isnan(humidity) ? -999 : humidity;
    temperature = isnan(temperature) ? -999 : temperature;
    computedHeatIndex = -999;

    // Store the current millis for the next iteration
    lastDHTReadingInMillis = currentDHTReadingInMillis;

    return;
  }

  // Reset failure flags
  DHTSensorFailedReadingAttempts = 0;
  DHTSensorReadingFailure = false;

  // Compute heat index in Celsius (isFahreheit = false)
  computedHeatIndex = dht.computeHeatIndex(temperature, humidity, false);
  computedHeatIndex = (computedHeatIndex > 100) ? 0 : computedHeatIndex;

  Serial.print(F("[get_dht_data] - temperature (C)      : "));
  Serial.println(temperature);

  Serial.print(F("[get_dht_data] - humidity (%)         : "));
  Serial.println(humidity);

  Serial.print(F("[get_dht_data] - computedHeatIndex (C): "));
  Serial.println(computedHeatIndex);

  // Store the current millis for the next iteration
  lastDHTReadingInMillis = currentDHTReadingInMillis;
}

void print_fs_info() {
  FSInfo fs_info;
  SPIFFS.info(fs_info);

  float used_space = (float) fs_info.usedBytes/fs_info.totalBytes;
  float free_space = (float) (1 - used_space);

  Serial.print(F("[print_fs_info] - Filesystem Total: "));
  Serial.println(fs_info.totalBytes);

  Serial.print(F("[print_fs_info] - Filesystem Used: "));
  Serial.println(fs_info.usedBytes);

  Serial.print(F("[print_fs_info] - Filesystem Used %: "));
  Serial.println(used_space);

  Serial.print(F("[print_fs_info] - Filesystem Free %: "));
  Serial.println(free_space);
}

void read_sensors() {
  Serial.println(F("[read_sensors] - Begin"));

  // Reads the hall sensor
  read_hall_sensor();

  // Reads the DHT Sensor Info
  read_dht_sensor();

  Serial.println(F("[read_sensors] - Done"));
}

void save_message(String message){
  Serial.print(F("[save_message] - Saving Message: \""));
  Serial.print(message);
  Serial.println(F("\""));

  Serial.print(F("[save_message] - Free Space: "));
}
  
int wifi_quality() {
  long rssi = WiFi.RSSI();

  int percent = 0;
  if(rssi > -50) {
    percent = 100;
  } else if(rssi > -55) {
    percent = 90;
  } else if(rssi > -62) {
    percent = 80;
  } else if(rssi > -65) {
    percent = 70;
  } else if(rssi > -68) {
    percent = 60;
  } else if(rssi > -74) {
    percent = 50;
  } else if(rssi > -79) {
    percent = 40;
  } else if(rssi > -83) {
    percent = 30;
  } else if(rssi > -88) {
    percent = 20;
  } else if(rssi > -93) {
    percent = 10;
  }

  Serial.print("[wifi_quality] - rssi/percent: ");
  Serial.print(rssi);
  Serial.print(F("/"));
  Serial.println(percent);

  return percent;
}

float fs_free_space() {
  FSInfo fs_info;
  SPIFFS.info(fs_info);

  float used_space = (float) fs_info.usedBytes/fs_info.totalBytes;
  float free_space = (float) (1 - used_space);

  Serial.print("[fs_free_space] - Used: ");
  Serial.println(used_space);

  Serial.print("[fs_free_space] - Free: ");
  Serial.println(free_space);

  return free_space;
}

String build_message() {
  String message = "i;";

  message += STATION_ID;
  message += ";";

  message += realTipCount;
  message += ";";

  message += "0;"; // Raining

  message += temperature;
  message += ";";

  message += humidity;
  message += ";";

  message += wifi_quality();
  message += ";";

  message += "0"; // crc

  Serial.print(F("[build_message] - Message (i;id;timestamp;bt;raining;tp;rh;sigs;crc): "));
  Serial.println(message);

  return message;
}

void delete_messages(String send_result){

  if(send_result == "ok") {
    Serial.println(F("[delete_messages] - Deleting files:"));
    utils.FSPrintFileList();
  
    utils.FSDeleteFiles(DATA_DIR);
    Serial.println(F("[delete_messages] - Done."));
  
    Serial.println(F("[delete_messages] - File list:"));
    utils.FSPrintFileList();
  } else {
    Serial.print(F("[delete_messages] - Can't delete, socket response: '"));
    Serial.print(send_result);
    Serial.println(F("'"));
  }
}

String read_messages() {
  Serial.println(F("[read_messages]"));
}

String send_messages() {
  if(fs_is_active){
    // Open directory
    Dir dir = SPIFFS.openDir(DATA_DIR);
    Serial.print(F("[send_messages] - Files in \""));
    Serial.print(DATA_DIR);
    Serial.println(F("\" directory:"));
    Serial.println(F("[send_messages] - ---------"));
    File f;
    String messages;
    while (dir.next()) {
      Serial.print(F("[send_messages] - "));
      Serial.println(dir.fileName());
      // Open file
      f = SPIFFS.open(dir.fileName(), "r");

      if (!f) {
        Serial.print(F("[send_messages] - Unable To Open '"));
        Serial.print(dir.fileName());
        Serial.println(F("' for Reading"));
      } else {

        // Group messages
        while (f.position() < f.size()) {
          messages += f.readStringUntil('\n');
          messages += '\n';
        }

        f.close();
      }
    }

    Serial.println(F("[send_messages] - Message payload:"));
    Serial.print(messages);
    
    // Send all messages
    return send_message(messages);

  } else {
      Serial.println(F("[send_messages] - Filesystem is not mounted"));
      return "not_mounted";
  }
}

String send_message(String message){
  String result = "";

  Serial.print(F("[send_message] - Connecting to "));
  Serial.print(SERVER_ADDR);
  Serial.print(":");
  Serial.println(SERVER_PORT);

  boolean wf_connected = wifi_connected();

  if(wf_connected > 0) {
    Serial.println(F("[send_message] - Connected!"));
    Serial.println(F("[send_message] - Transmitting message: "));
    Serial.print(message);
  
    // Post Data
    wf_client.print(message);

    Serial.println(F("[send_message] - Handling response"));

    // Handle result
    while (wf_client.connected()) {
      if (wf_client.available()) {
        result = wf_client.readStringUntil('\r');
        Serial.print(F("[send_message] - Return: "));
        Serial.println(result);
        break;
      }
    }

    Serial.println(F("[send_message] - Closing connection"));

    // Close connection
    wf_client.stop();
    
  } else {
    Serial.println(F("[send_message] - FAIL - Can't connect, can't transmit, aborted."));
  }

  Serial.println(F("[send_message] - Done"));
  return result;
}

void init_fs(){
  Serial.println(F("[init_fs] - Begin"));

  if (SPIFFS.begin()) {
    fs_is_active = true;
    Serial.println(F("[init_fs] - SPIFFS is active"));
  } else {
    Serial.println(F("[init_fs] - FATAL! Unable to activate SPIFFS"));
    while(1);
  }

  Serial.println(F("[init_fs] - Done"));
}

void wifi_init() {
  wifi_setup();
  wifi_connected();
}

void wifi_setup() {
  Serial.print(F("[wifi_setup] - Connecting to SSID: "));
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

boolean wifi_connected() {
  Serial.print(F("[wifi_connected] - Connecting to SSID: "));
  Serial.println(ssid);

  int conn_counter = 20;
  while (!wf_client.connect(SERVER_ADDR, SERVER_PORT)) {
    Serial.print(F("[wifi_connected] - Connection Failed ("));
    Serial.print(conn_counter);
    Serial.println(F(")"));

    delay(1000);
    
    conn_counter--;

    if(conn_counter < 0) break;
  }
  
  if(conn_counter > 0) {
    Serial.println(F("[wifi_connected] - WiFi Connected"));
    return true;
  } else {
    Serial.println(F("[wifi_connected] - WiFi NOT Connected"));
    return false;
  }
}

 
/**
 * Put the system to sleep
 */
void system_sleep() {
//  wifi_set_sleep_type(LIGHT_SLEEP_T);
  Serial.println(F("[system_sleep] - Modem is sleeping"));

  delay(SLEEP_PERIOD);
//  unsigned long sleepCounter = millis();
//  while ((millis() - sleepCounter) < SLEEP_PERIOD);

  Serial.println(F("[system_sleep] - Modem up and running"));
}

void init_ntp() {
  Serial.println(F("[init_ntp] - Starting"));
  
  Serial.println(F("[init_ntp] - Updating"));
  
  Serial.println(F("[init_ntp] - Done"));
}

void format_fs() {
  Serial.println(F("[format_fs] - Formatting File System, please wait..."));
  utils.FSFormat();
  Serial.println(F("[format_fs] - Done."));
}

void config_pins() {
  Serial.println(F("[config_pins] - Begin"));

  // RAIN
  pinMode(PIN_HALL, INPUT_PULLUP);

  // RESET
  pinMode(PIN_RESET_MONO, OUTPUT);
  digitalWrite(PIN_RESET_MONO, HIGH);
  pinMode(PIN_SYSTEM_FORMAT, INPUT_PULLUP);
  if (digitalRead(PIN_SYSTEM_FORMAT) == 0) {
    Serial.println(F("[config_pins] - Format FS pin active. Formatting..."));
    format_fs();
  }

  Serial.println(F("[config_pins] - Done"));
}

void reset_bucket_tip_counter() {
  Serial.print(F("[reset_bucket_tip_counter] - Tip counter: "));
  Serial.println(tipCounter);

  Serial.print(F("[reset_bucket_tip_counter] - Real tip count: "));
  Serial.println(realTipCount);

  Serial.print(F("[reset_bucket_tip_counter] - Last tip count: "));
  Serial.println(lastTipCount);

  lastTipCount = 0;
  realTipCount = 0;
  tipCounter = 0;
  rainVolume = 0;
}

void setup() {
  Serial.begin(SYSTEM_BAUDRATE);

  Serial.println(F("\n\n[setup] - Init =========================="));
  
  config_pins();

  wifi_init();

  init_fs();

//  format_fs();

  init_dht();
    
  attach_interruption();
}

void loop() {
  Serial.println(F("\n\n[loop] - Begin =========================="));
  read_sensors();
  String message = build_message();
  reset_bucket_tip_counter();
//  save_message(message);
  String send_result = send_messages();
//  delete_messages(send_result);
  attach_interruption();
  system_sleep();
}
