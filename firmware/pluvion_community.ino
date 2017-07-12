/**
 * Pluvi.On WiFi Community Firmware
 * 
 *       FILE: pluvion_community_firwmware.ino
 *    VERSION: 0.0.1.C
 *    LICENSE: Creative Commons 4
 *    AUTHORS:
 *             Pedro Godoy <pedro@pluvion.com.br>
 *             Hugo Santos <hugo@pluvion.com.br>
 * 
 *       SITE: https://github.com/pluvion/firmware
 *             https://www.pluvion.com.br
 */

// Libraries    
#include <FS.h> // FS must be the first
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

// System Config (must come before #include <PluviOn.h>)
#define PLV_DEBUG_ENABLED true
#define PLV_SYSTEM_BAUDRATE 115200

#include <PluviOn.h>
PluviOn pluvion;

// Debug voltage in (enable ESP.getVcc())
ADC_MODE(ADC_VCC);

// PINS
#define PIN_DHT D5       // pin where the dht22 is connected
#define PIN_POWER_OFF D6 // pin where toggle is connected
#define PIN_HALL D7      // pin where Hall sensor is connected

// |------------------------------------------|
// |     DHT Sensor Reading Interval in ms    |
// |------------------------------------------|
// |   INTERVAL  |   FREQ.  |  # MSGS/DAY     |
// |-------------|----------|-----------------|
// |  1.800.000  |  30 min  |     48 msg/day  |
// |  1.200.000  |  20 min  |     72 msg/day  |
// |    900.000  |  15 min  |     96 msg/day  |
// |    600.000  |  10 min  |    144 msg/day  |
// |    300.000  |   5 min  |    288 msg/day  |
// |     60.000  |   1 min  |  1.440 msg/day  |
// |     30.000  |  30 sec  |  2.880 msg/day  |
// |-------------|----------|-----------------|
//
// ATTENTION!
// You must consider the available storage size and the message size
// More messages, less space in case of lost connection

#define DHT_SENSOR_READING_DELAY 600000
#define DHT_SENSOR_READING_FAILURE_DELAY 180000
#define SEND_OFFLINE_MESSAGES_DELAY 120000

// READING PERIODS AND DEBOUNCE TIMERS
#define DELAY_HALL_SENSOR_DEBOUNCING 1000 // Delay in ms to debounce hall sensor

/**
 * Station Information
 */

// Configured at setup
float  PLV_STATION_BUCKET_VOLUME              = 3.22; // Bucket Calibrated Volume in ml
String STATION_LONGITUDE                      = "";
String STATION_LATITUDE                       = "";
String STATION_NAME                           = "PluviOn";
String STATION_ID                             = "";

const String STATION_ID_PREFIX                = "PluviOn_";
const String FIRMWARE_VERSION                 = "0.0.1.C";

/**
 * File system directories and variables
 */
const String DIR_SYSTEM_DATA                  = "/sys";
const String DIR_WEATHER_DATA                 = "/weather";
const String DIR_MESSAGE_COUNTER              = "/msgcnt";
const String DIR_FIRMWARE_VERSION             = "/fmwver";
const String DIR_STATION_ID                   = "/stt/id";
const String DIR_STATION_NAME                 = "/stt/name";
const String DIR_STATION_BUCKET_VOL           = "/stt/bucketvol";
const String DIR_STATION_TIME_TO_RESET        = "/stt/ttr";
const String DIR_STATION_TIME                 = "/stt/time";
const String DIR_STATION_LATITUDE             = "/stt/lat";
const String DIR_STATION_LONGITUDE            = "/stt/lon";
const String FIELD_SEPARATOR                  = "|";

// Station time and reset timer
const unsigned long DEFAULT_RESET_PERIOD      = 86400000; // Default reset period 24 hours in ms = 86400000
unsigned long resetCicleCounter               = 0;
unsigned long resetStartTime                  = 0;
unsigned long TIME_TO_RESET                   = 0; // Time left until the next reset
unsigned long STATION_TIME                    = 0; // Station time
boolean resetFlag                             = false; //


// Tipping Bucket Configurations
const float CONTRIBUTION_AREA                 = 7797.0; // Contribuition area in mm2
volatile int tipCounter                       = 0; // Store the bucket tip counter
volatile float rainVolume                     = 0; // The rain accumulated volume

// State Control Variables
unsigned long lastTipBucketReadingInMillis    = 0; // The last tip bucket (hall sensor) reading time in ms
unsigned long currentTipBucketReadingInMillis = 0; // Current tip bucket reading in millis
volatile float lastTipCount                   = 0; // The last tip count total
volatile int realTipCount                     = 0; // Checked tip counter

// DHT22 Sensor Variables and initialization
#define DHT_TYPE DHT22 // DHT 22  (AM2302)
volatile float humidity                       = 0; // Read air humidity in %
volatile float temperature                    = 0; // Read temperature as Celsius
volatile float computedHeatIndex              = 0; // Compute heat index in Celsius

// State Control Variables
unsigned long lastDHTReadingInMillis          = 0; // The last DHT reading time in ms
unsigned long currentDHTReadingInMillis       = 0; // Current DHT reading in millis

// Initialize DHT sensor
DHT dht(PIN_DHT, DHT_TYPE, 20);

// In case of DHT Sensor reading failure
#define DHT_SENSOR_READING_FAILURE_ATTEMPTS_LIMIT 2
boolean DHTSensorReadingFailure = false;
int DHTSensorFailedReadingAttempts = 0;

/**
 * Local Storage
 */
int messageID = 0;

/**
 * System Information
 */
float SYSAvailableDiskSpaceInPercent = 0.0;
int SYSAvailableDiskSpaceInBytes = 0;
int SYSAvailableHeapInBytes = 0;
String SYSLastResetReason = "";
String SYSWiFiHostname = "";
String SYSWiFiLocalIp = "";
String SYSWiFiMACAddr = "";
int SYSWiFiRSSIindBm = 0;
String SYSWiFiSSID = "";
int SYSVCCInV = 0;

// Initialize WiFiClient
WiFiClientSecure WIFISecureClient;

// Initialize WiFiManager
WiFiManager wifiManager;

// Time to try
#define WIFI_CONN_COUNTER 4
// Delay to connect to WiFi (WIFI_CONN_DELAY X WIFI_CONN_COUNTER = time to access point mode)
#define WIFI_CONN_DELAY 30000

/**
 * Pluvi.On API
 */
const char* PLUVION_API_SERVER_ADDR        = "community.pluvion.com.br"; // Prod
const char* PLUVION_API_RESOURCE_WEATHER   = "/weather";
const int   PLUVION_API_SERVER_PORT        = 443;
const int   PLUVION_API_SERVER_REQ_TIMEOUT = 5000; // Request Timeout (ms)
const char* PLUVION_API_CERT_FINGERPRINT   = "d51b47c0781990536dd9cfa55de5c2f2932301ad";

// Pluvi.On Media Types Standard
const char* HTTP_HEADER_ACCEPT             = "Accept: application/vnd.community.pluvion.com.br.v1.text";
const char* HTTP_HEADER_CONTENT_TYPE       = "Content-Type: application/vnd.community.pluvion.com.br.v1.text";
const char* HTTP_HEADER_EOL                = "\r\n";
const char* HTTP_HEADER_USER_AGENT         = "User-Agent: Pluvi.On Community Station/1.0";

// Send offline messages
unsigned long currentBatchReadingInMillis = 0;
unsigned long lastBatchReadingInMillis = 0;

/**
 * Print system environment status to serial
 */
void printSystemEnvironmentStatus() {

    PLV_DEBUG_HEADER(F("SYSTEM ENVIRONMENT STATUS"));

    PLV_DEBUG_(F("Free Heap size: "));
    PLV_DEBUG_(ESP.getFreeHeap());
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(ESP.getFreeHeap(), 'K'));
    PLV_DEBUG(F(" KB)"));

    float v = ESP.getVcc();
    v /= 1000;
    PLV_DEBUG_(F("VCC: "));
    PLV_DEBUG_(ESP.getVcc());
    PLV_DEBUG_(F(" ("));
    PLV_DEBUG_(v);
    PLV_DEBUG(F(" V)"));

    PLV_DEBUG_(F("Last Reset Reason: "));
    PLV_DEBUG_(ESP.getResetReason());
    PLV_DEBUG(F(""));

    PLV_DEBUG_(F("Chip Id: "));
    PLV_DEBUG(ESP.getChipId());

    PLV_DEBUG_(F("Flash Chip Id: "));
    PLV_DEBUG(ESP.getFlashChipId());

    PLV_DEBUG_(F("Flash Chip Size: "));
    PLV_DEBUG_(ESP.getFlashChipSize());
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(ESP.getFlashChipSize(), 'M'));
    PLV_DEBUG(F(" MB)"));

    PLV_DEBUG_(F("Flash Chip Real Size: "));
    PLV_DEBUG_(ESP.getFlashChipRealSize());
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(ESP.getFlashChipRealSize(), 'M'));
    PLV_DEBUG(F(" MB)"));

    PLV_DEBUG_(F("Flash Chip Speed: "));
    PLV_DEBUG_(ESP.getFlashChipSpeed());
    PLV_DEBUG(F(" Hz"));

    PLV_DEBUG_(F("Cycle Count: "));
    PLV_DEBUG(ESP.getCycleCount());

    PLV_DEBUG_(F("Reset Flag: "));
    PLV_DEBUG(resetFlag);

    PLV_DEBUG_(F("Time to next reset:"));
    PLV_DEBUG(TIME_TO_RESET);
}

/**
 * Print system configurations to serial monitor
 */
void printSystemConfigurations() {

    PLV_DEBUG_HEADER(F("SYSTEM CONFIGURATIONS"));

    PLV_DEBUG(F("// Station General Configuration"));
    PLV_DEBUG_(F("ID:                                      ")); PLV_DEBUG(STATION_ID);
    PLV_DEBUG_(F("Name:                                    ")); PLV_DEBUG(STATION_NAME);
    PLV_DEBUG_(F("Latitude:                                ")); PLV_DEBUG(STATION_LATITUDE);
    PLV_DEBUG_(F("Longitude:                               ")); PLV_DEBUG(STATION_LONGITUDE);
    PLV_DEBUG_(F("Bucket Volume (ml):                      ")); PLV_DEBUG(PLV_STATION_BUCKET_VOLUME);
    PLV_DEBUG_(F("Contribution Area (mm2):                 ")); PLV_DEBUG(CONTRIBUTION_AREA);
    PLV_DEBUG_(F("Firmware Version:                        ")); PLV_DEBUG(FIRMWARE_VERSION);

    PLV_DEBUG(F("// Pluvi.On API"));
    PLV_DEBUG_(F("API Server Address:                      ")); PLV_DEBUG(PLUVION_API_SERVER_ADDR);
    PLV_DEBUG_(F("API Server Port:                         ")); PLV_DEBUG(PLUVION_API_SERVER_PORT);
    PLV_DEBUG_(F("API Server SSL Certificate Fingerprint:  ")); PLV_DEBUG(PLUVION_API_CERT_FINGERPRINT);
    PLV_DEBUG_(F("API Server Request Timeout Limit (ms):   ")); PLV_DEBUG(PLUVION_API_SERVER_REQ_TIMEOUT);
    PLV_DEBUG(F("API Server Resources in use:              "));
    PLV_DEBUG_(F(" - ")); PLV_DEBUG(PLUVION_API_RESOURCE_WEATHER);

    PLV_DEBUG(F("// HTTP Client Configurations"));
    PLV_DEBUG_(F("HTTP Header                              ")); PLV_DEBUG(HTTP_HEADER_USER_AGENT);
    PLV_DEBUG_(F("HTTP Header                              ")); PLV_DEBUG(HTTP_HEADER_ACCEPT);
    PLV_DEBUG_(F("HTTP Header                              ")); PLV_DEBUG(HTTP_HEADER_CONTENT_TYPE);

    PLV_DEBUG(F("// File System Directories"));
    PLV_DEBUG_(F("System Data Directory:                   ")); PLV_DEBUG(DIR_SYSTEM_DATA);
    PLV_DEBUG_(F("Weather Data Directory :                 ")); PLV_DEBUG(DIR_WEATHER_DATA);
    PLV_DEBUG_(F("Message Counter Directory:               ")); PLV_DEBUG(DIR_MESSAGE_COUNTER);
    PLV_DEBUG_(F("Station ID Directory:                    ")); PLV_DEBUG(DIR_STATION_ID);
    PLV_DEBUG_(F("Station Name Directory:                  ")); PLV_DEBUG(DIR_STATION_NAME);
    PLV_DEBUG_(F("Station Bucket Volume Directory:         ")); PLV_DEBUG(DIR_STATION_BUCKET_VOL);
    PLV_DEBUG_(F("Station Latitude Directory:              ")); PLV_DEBUG(DIR_STATION_LATITUDE);
    PLV_DEBUG_(F("Station Longitude Directory:             ")); PLV_DEBUG(DIR_STATION_LONGITUDE);
    PLV_DEBUG_(F("Message Field Separator:                 ")); PLV_DEBUG(FIELD_SEPARATOR);

    PLV_DEBUG(F("// DHT Sensor Configuration"));
    PLV_DEBUG_(F("DHT Sensor PIN:                          ")); PLV_DEBUG(PIN_DHT);
    PLV_DEBUG_(F("DHT Sensor Type:                         ")); PLV_DEBUG(DHT_TYPE);
    PLV_DEBUG_(F("DHT Sensor Reading Delay (ms):           ")); PLV_DEBUG(DHT_SENSOR_READING_DELAY);
    PLV_DEBUG_(F("DHT Sensor Reading Retry Attempts Limit: ")); PLV_DEBUG(DHT_SENSOR_READING_FAILURE_ATTEMPTS_LIMIT);
    PLV_DEBUG_(F("DHT Sensor Reading Retry Interval (ms):  ")); PLV_DEBUG(DHT_SENSOR_READING_FAILURE_DELAY);

    PLV_DEBUG(F("// Hall Sensor Configuration"));
    PLV_DEBUG_(F("Hall Sensor PIN:                         ")); PLV_DEBUG(PIN_HALL);
    PLV_DEBUG_(F("Hall Sensor Debouncing Delay (ms):       ")); PLV_DEBUG(DELAY_HALL_SENSOR_DEBOUNCING);
    PLV_DEBUG_(F("Power Off PIN:                           ")); PLV_DEBUG(PIN_POWER_OFF);
}

/**
 * Print the latest system weather info to serial
 */
void printCurrentWeatherData() {

    PLV_DEBUG_HEADER(F("WEATHER DATA"));

    PLV_DEBUG(F("// Rain Info"));
    PLV_DEBUG_(F("Tip count:                               ")); PLV_DEBUG(realTipCount);
    PLV_DEBUG_(F("Rain volume:                             ")); PLV_DEBUG_(rainVolume); PLV_DEBUG(F(" mm"));
    PLV_DEBUG(F("// Weather Info"));
    PLV_DEBUG_(F("Humidity:                                ")); PLV_DEBUG_(humidity); PLV_DEBUG(F(" %"));
    PLV_DEBUG_(F("Temperature:                             ")); PLV_DEBUG_(temperature); PLV_DEBUG(F(" C"));
    PLV_DEBUG_(F(" Heat index:                             ")); PLV_DEBUG_(computedHeatIndex); PLV_DEBUG(F(" C"));
}

/**
 * Print system environment status to serial
 */
void printSystemWiFiStatus() {

    PLV_DEBUG_HEADER(F("SYSTEM WIFI STATUS"));

    PLV_DEBUG_(F("Status:                                "));
    if (WiFi.status() == WL_CONNECTED) {
        PLV_DEBUG(F("CONNECTED - Wi-Fi connection successful established."));

    } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
        PLV_DEBUG(F("NO SSID AVAILABLE - Configured SSID cannot be reached."));

    } else if (WiFi.status() == WL_CONNECT_FAILED) {
        PLV_DEBUG(F("CONNECT FAILED - Password is incorrect."));

    } else if (WiFi.status() == WL_IDLE_STATUS) {
        PLV_DEBUG(F("IDLE STATUS - Wi-Fi is in process of changing between statuses."));

    } else if (WiFi.status() == WL_DISCONNECTED) {
        PLV_DEBUG(F("DISCONNECTED - Wi-Fi module is not configured in station mode."));

    }

    PLV_DEBUG_(F("SSID:                                  "));
    PLV_DEBUG(WiFi.SSID().c_str());

    PLV_DEBUG_(F("MAC address:                           "));
    PLV_DEBUG(WiFi.macAddress().c_str());

    PLV_DEBUG_(F("Local Ip:                              "));
    PLV_DEBUG(WiFi.localIP());

    PLV_DEBUG_(F("RSSI (dBm):                            "));
    PLV_DEBUG_(WiFi.RSSI());
    PLV_DEBUG(F(" (Signal Strength)"));

    PLV_DEBUG_(F("Subnet Mask:                           "));
    PLV_DEBUG(WiFi.subnetMask());

    PLV_DEBUG_(F("Gateway IP:                            "));
    PLV_DEBUG(WiFi.gatewayIP());

    PLV_DEBUG_(F("Hostname:                              "));
    PLV_DEBUG(WiFi.hostname());

    PLV_DEBUG_(F(""));
}

/**
 * Send all offline messages
 */
void sendOfflineMessages() {

    currentBatchReadingInMillis = millis();

    // Check if the configured delay has passed
    if ((currentBatchReadingInMillis - lastBatchReadingInMillis) >= SEND_OFFLINE_MESSAGES_DELAY) {

        // Mounts SPIFFS file system
        if (!SPIFFS.begin()) {
            PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
        }

        Dir dir = SPIFFS.openDir(DIR_WEATHER_DATA);
        File f;

        if (dir.next()) {

            PLV_DEBUG_HEADER(F("SENDING OFFLINE MESSAGES"));

            PLV_DEBUG_(F("FILE NAME: "));
            PLV_DEBUG(dir.fileName());
    
            f = dir.openFile("r");

            // Reads the file content and print to Serial monitor
            if(f.available()) {
                String line = f.readStringUntil('\n');
                int messageID = dir.fileName().substring(DIR_WEATHER_DATA.length() + 1).toInt();
                String message  = line;
                       message += FIELD_SEPARATOR + "off";

                PLV_DEBUG_(F("Line:       ")); PLV_DEBUG(line);
                PLV_DEBUG_(F("messageID:  ")); PLV_DEBUG(messageID);
                PLV_DEBUG_(F("message:    ")); PLV_DEBUG(message);

                sendMessage(messageID, message);
            }
            
            // Close file
            f.close();
            
            lastBatchReadingInMillis = currentBatchReadingInMillis;
        }
    }
}

/**
 * Print the current file system status
 */
void printFileSystemStatus() {

    PLV_DEBUG_HEADER(F("FILE SYSTEM STATUS"));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("printFileSystemStatus() - FATAL! Error mounting SPIFFS file system!"));
    }

    FSInfo fs_info;
    SPIFFS.info(fs_info);

    float totalBytes = fs_info.totalBytes;
    float usedBytes = fs_info.usedBytes;
    float freeBytes = totalBytes - usedBytes;

    float utilizationFactor = usedBytes / totalBytes;
    float availablePercent = (1 - utilizationFactor) * 100;
    float usedPercent = utilizationFactor * 100;

    int blockSize = fs_info.blockSize;
    int pageSize = fs_info.pageSize;
    int maxOpenFiles = fs_info.maxOpenFiles;
    int maxPathLength = fs_info.maxPathLength;

    PLV_DEBUG_(F("Available:                             "));
    PLV_DEBUG_(freeBytes);
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(freeBytes, 'K')); // KB
    PLV_DEBUG_(F(" KB or "));
    PLV_DEBUG_(pluvion.bytesConverter(freeBytes, 'M')); // MB
    PLV_DEBUG_(F(" MB, "));
    PLV_DEBUG_(availablePercent);
    PLV_DEBUG(F("%)"));

    PLV_DEBUG_(F("Used:                                  "));
    PLV_DEBUG_(usedBytes);
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(usedBytes, 'K')); // KB
    PLV_DEBUG_(F(" KB, "));
    PLV_DEBUG_(usedPercent);
    PLV_DEBUG(F("%)"));

    PLV_DEBUG_(F("Capacity:                              "));
    PLV_DEBUG_(pluvion.bytesConverter(totalBytes, 'K')); // KB
    PLV_DEBUG_(F(" KB ("));
    PLV_DEBUG_(pluvion.bytesConverter(totalBytes, 'M')); // MB
    PLV_DEBUG(F(" MB)"));

    PLV_DEBUG_(F("Block Size:                            "));
    PLV_DEBUG_(blockSize);
    PLV_DEBUG_(F(" bytes ("));
    PLV_DEBUG_(pluvion.bytesConverter(blockSize, 'K')); // KB
    PLV_DEBUG(F(" KB)"));

    PLV_DEBUG_(F("Page Size:                             "));
    PLV_DEBUG_(pageSize); // KB
    PLV_DEBUG(F(" bytes"));

    PLV_DEBUG_(F("Max Open Files:                        "));
    PLV_DEBUG(maxOpenFiles);

    PLV_DEBUG_(F("Max Path Length:                       "));
    PLV_DEBUG_(maxPathLength);
    PLV_DEBUG(F(" bytes"));

    PLV_DEBUG_(F(""));
}

/**
 * Print the system status
 */
void printSystemStatus() {

    // Print System Configuration
    printSystemConfigurations();

    // Print System WiFi Status
    printSystemWiFiStatus();

    // Print System Environment Status
    printSystemEnvironmentStatus();

    // Print the file system status
    printFileSystemStatus();

    // Print the system's list of files and directories
    pluvion.FSPrintFileList();

    // Print Current Weather Data Information
    printCurrentWeatherData();
}

/**
 * Print the serial available commands
 */
void printCommands() {

    PLV_DEBUG_HEADER(F("SYSTEM AVALIABLE COMMANDS"));

    PLV_DEBUG(F("help              Show all available commands"));

    PLV_DEBUG(F("\nSystem commands:"));
    PLV_DEBUG(F("status             Print the current system status to serial monitor including, file system, internet connectivity, etc."));
    PLV_DEBUG(F("reset              Reset system completely, format file system, reset message counter, delete weather data (WARNING: cannot be undone)"));
    PLV_DEBUG(F("resetwifi          Reset WiFi appliance settings, SSID and password"));

    PLV_DEBUG(F("\nFile system commands:"));
    PLV_DEBUG(F("filelist           Print the current system file list."));
    PLV_DEBUG(F("fsformat           Format File System. (WARNING: cannot be undone)"));
    PLV_DEBUG(F("fsstatus           Print the current file system status."));
    PLV_DEBUG(F("cleardatadir       Remove all data files."));

    PLV_DEBUG(F("\nWeather data commands:"));
    PLV_DEBUG(F("dump               Dump all data into serial monitor"));
    PLV_DEBUG(F("deletedata         Delete weather data files from local filesystem (WARNING: cannot be undone)"));

    PLV_DEBUG(F(""));
}

/**
 * Get the current system information
 */
void getSystemInformation() {

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("getSystemInformation() - FATAL! Error mounting SPIFFS file system!"));
    }

    FSInfo fs_info;
    SPIFFS.info(fs_info);

    float totalBytes = fs_info.totalBytes;
    float usedBytes = fs_info.usedBytes;
    float freeBytes = totalBytes - usedBytes;

    float utilizationFactor = usedBytes / totalBytes;
    float availablePercent = (1 - utilizationFactor) * 100;
    float usedPercent = utilizationFactor * 100;

    SYSAvailableHeapInBytes = ESP.getFreeHeap();
    SYSAvailableDiskSpaceInBytes = freeBytes;
    SYSAvailableDiskSpaceInPercent = availablePercent;
    SYSAvailableHeapInBytes = ESP.getFreeHeap();
    SYSVCCInV = ESP.getVcc();
    SYSLastResetReason = ESP.getResetReason();

    SYSWiFiSSID = WiFi.SSID().c_str();
    SYSWiFiLocalIp = WiFi.localIP().toString().c_str();
    SYSWiFiRSSIindBm = WiFi.RSSI(); // Signal Strength
    SYSWiFiHostname = WiFi.hostname();
    SYSWiFiMACAddr = WiFi.macAddress().c_str();

    PLV_DEBUG_HEADER(F("CURRENT SYSTEM INFO"));

    PLV_DEBUG_(F("Free Heap size: "));
    PLV_DEBUG_(SYSAvailableHeapInBytes);
    PLV_DEBUG(F(" bytes"));

    PLV_DEBUG_(F("Available Disk Space: "));
    PLV_DEBUG_(SYSAvailableDiskSpaceInBytes);
    PLV_DEBUG(F(" bytes"));

    PLV_DEBUG_(F("Available Disk Space: "));
    PLV_DEBUG_(SYSAvailableDiskSpaceInPercent);
    PLV_DEBUG(F(" %"));

    PLV_DEBUG_(F("VCC: "));
    PLV_DEBUG(SYSVCCInV);

    PLV_DEBUG_(F("WiFi SSID: "));
    PLV_DEBUG(SYSWiFiSSID);

    PLV_DEBUG_(F("WiFi Local Ip: "));
    PLV_DEBUG(SYSWiFiLocalIp);

    PLV_DEBUG_(F("WiFi MAC Address: "));
    PLV_DEBUG(SYSWiFiLocalIp);

    PLV_DEBUG_(F("WiFi RSSI (dBm): "));
    PLV_DEBUG_(SYSWiFiRSSIindBm);
    PLV_DEBUG(F(" (Signal Strength)"));

    PLV_DEBUG_(F("WiFi Hostname: "));
    PLV_DEBUG_(SYSWiFiHostname);

    PLV_DEBUG_(F(""));
}

/**
 * Dump all data in the local file to serial
 */
void dump() {

    PLV_DEBUG_HEADER(F("DUMP WEATHER DATA TO SERIAL"));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL! Error mounting SPIFFS file system."));
    }

    // Open data directory
    Dir dir = SPIFFS.openDir(DIR_WEATHER_DATA);

    File f;

    // Read all files
    while (dir.next()) {

        PLV_DEBUG_(F("File name: "));
        PLV_DEBUG(dir.fileName());

        // Open file for reading
        f = dir.openFile("r");

        // Reads the file content and print to Serial monitor
        while (f.available()) {
            String line = f.readStringUntil('\n');
            PLV_DEBUG(line);
        }

        // Close file
        f.close();
    }
}

/**
 * Perform a full system reset
 */
void fullReset() {

    PLV_DEBUG_HEADER(F("\n\n\nFULL SYSTEM RESET"));

    // Reset Message ID
    resetMessageID();

    // Reset Rain Indicators
    resetRainIndicators();

    // Format file system
    pluvion.FSFormat();

    // Reset WiFi Settings
    resetWiFiSettings();

    // Init all system configuration
    initSystem();
}

/**
 * Init all system configurations
 */
void initSystem(){

    // Get Station ID from file system or create the file with
    initStationID();

    // Get Station Name
    initStationName();

    // Register Firmware Version
    initFirmwareVersion();

    // Get Station Latitude and Longitude
    initStationCoordinates();

    // Get Station Calibrated Bucket Volume
    initBucketVolume();

    // Initialize Message ID
    initMessageID();

    // Initialize Time To Reset
    initTimeToReset();

    // Initialize DHT Sensor
    initDHTSensor();
}

/**
 * Delete weather data from local filesystem
 */
void deleteWeatherData() {

    PLV_DEBUG_HEADER(F("DELETE WEATHER DATA"));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
    PLV_DEBUG(F("deleteWeatherData() - FATAL! Error mounting SPIFFS file system!"));
    }

    PLV_DEBUG_(F("Removing files from ("));
    PLV_DEBUG_(DIR_WEATHER_DATA);
    PLV_DEBUG(F(") directory..."));

    // Delete weather data dir
    if (SPIFFS.remove(DIR_WEATHER_DATA)) {

        PLV_DEBUG(F("Weather data removed successfully."));

    } else {

        PLV_DEBUG(F("ERROR! Weather data NOT removed."));
    }
}

/**
 * Read Serial Commands
 */
void readSerialCommands() {

    // Holds Serial Monitor commands
    String serialCommand = "";

    // Serial availability
    if (Serial.available()) {

    // Read the incoming command
    serialCommand = Serial.readString();
    serialCommand.trim();
    Serial.flush();

    PLV_DEBUG_(F("\nCommand received: "));
    PLV_DEBUG(serialCommand);

    if (serialCommand == "help") {
        printCommands();

    } else if (serialCommand == "reset") {
        fullReset();

    } else if (serialCommand == "status") {
        printSystemStatus();

    } else if (serialCommand == "resetwifi") {
        resetWiFiSettings();

    } else if (serialCommand == "filelist") {
        pluvion.FSPrintFileList();

    } else if (serialCommand == "fsformat") {
        pluvion.FSFormat();

    } else if (serialCommand == "cleardatadir") {

        PLV_DEBUG_HEADER(F("DELETING WEATHER DATA"));
        pluvion.FSDeleteFiles(DIR_WEATHER_DATA);
        

    } else if (serialCommand == "fsstatus") {
        printFileSystemStatus();

    } else if (serialCommand == "dump") {
        dump();

    } else if (serialCommand == "deletedata") {
        deleteWeatherData();

    } else {

        PLV_DEBUG_(F("ERROR! Command not recognized: "));
        PLV_DEBUG(serialCommand);
    
        printCommands();

    }
    }
}

/**
 * Initialize DHT Sensor
 */
void initDHTSensor() {
    dht.begin();
}

/**
 * Reads the hall sensor (tip bucket)
 */
void countTipBucket() {

    tipCounter++;

    PLV_DEBUG_HEADER(F("TIP BUCKET INCREASED"));

    PLV_DEBUG_(F("Current value: "));
    PLV_DEBUG(tipCounter);

    PLV_DEBUG_(F("Real tip count: "));
    PLV_DEBUG(realTipCount);

    PLV_DEBUG_(F("Rain volume (mm): "));
    PLV_DEBUG_(rainVolume);
    PLV_DEBUG(F(" mm"));
}

/**
 * Reset all rain indicators
 */
void resetRainIndicators() {

    PLV_DEBUG_HEADER(F("RESET RAIN INDICATORS"));

    resetFlag    = true;
    tipCounter   = 0;
    lastTipCount = 0;
    realTipCount = 0;
    rainVolume   = 0;
}

/**
 * Initialize Firmware Version
 */
void initFirmwareVersion() {

    PLV_DEBUG_HEADER(F("INIT FIRMWARE VERSION"));

    pluvion.FSDeleteFiles(DIR_FIRMWARE_VERSION);
    pluvion.FSCreateFile(DIR_FIRMWARE_VERSION, FIRMWARE_VERSION);

    PLV_DEBUG_(F("FIRMWARE VERSION: ")); PLV_DEBUG(FIRMWARE_VERSION);
}

/**
 * Initialize Station ID
 */
void initStationID() {

    PLV_DEBUG_HEADER(F("INIT STATION ID"));

    STATION_ID = pluvion.FSReadString(DIR_STATION_ID);

    if(!STATION_ID.length()){

        PLV_DEBUG(F("STATION ID not found.\nSetting STATION ID ..."));

        STATION_ID  = STATION_ID_PREFIX;
        STATION_ID += ESP.getChipId();

        pluvion.FSCreateFile(DIR_STATION_ID, STATION_ID);
    }

    PLV_DEBUG_(F("STATION ID: "));
    PLV_DEBUG(STATION_ID);
}

/**
 * Initialize Station Name
 */
void initStationName(){

    PLV_DEBUG_HEADER(F("INIT STATION NAME"));

    String sttName = pluvion.FSReadString(DIR_STATION_NAME);

    // If STATION NAME not found, set default
    if(!sttName.length()) {

        PLV_DEBUG(F("STATION NAME not found.\nSetting default STATION NAME ..."));
        pluvion.FSCreateFile(DIR_STATION_NAME, STATION_NAME);

    } else {

        STATION_NAME = sttName;
    }

    PLV_DEBUG_(F("STATION NAME: "));
    PLV_DEBUG(STATION_NAME);
}

/**
 * Initialize Station Coordinates
 */
void initStationCoordinates() {

    PLV_DEBUG_HEADER(F("INIT STATION COORDINATES"));

    STATION_LATITUDE  = pluvion.FSReadString(DIR_STATION_LATITUDE);
    STATION_LONGITUDE = pluvion.FSReadString(DIR_STATION_LONGITUDE);

    PLV_DEBUG_(F("STATION LATITUDE: "));
    PLV_DEBUG(STATION_LATITUDE);
    PLV_DEBUG_(F("STATION LONGITUDE: "));
    PLV_DEBUG(STATION_LONGITUDE);
}

/**
 * Initialize Station Bucket Volume
 */
void initBucketVolume() {

    PLV_DEBUG_HEADER(F("INIT BUCKET VOLUME"));

    float vol  = pluvion.FSReadFloat(DIR_STATION_BUCKET_VOL);

    if(vol) {
        PLV_STATION_BUCKET_VOLUME = vol;
    } else {
        PLV_DEBUG(F("STATION BUCKET VOLUME not found.\nUsing default value..."));
    }

    PLV_DEBUG_(F("STATION BUCKET VOLUME: "));
    PLV_DEBUG(PLV_STATION_BUCKET_VOLUME);
}

/**
 * Initialize the system message counter with the value in disk or create it as 1
 */
void initMessageID() {

    PLV_DEBUG_HEADER(F("INITIALIZING SYSTEM MESSAGE COUNTER"));

    // Read message counter
    int counter = pluvion.FSReadInt(DIR_MESSAGE_COUNTER);

    if (counter) {

        // Set the message counter
        messageID = counter;
    }

    // No message counter directory (first system boot)
    else {

        PLV_DEBUG(F("Message Counter not found.\nInit counter ..."));

        // Init counter
        messageID = 0;

        incrementMessageID();
    }

    PLV_DEBUG_(F("MESSAGE COUNTER: "));
    PLV_DEBUG(messageID);
}

/**
 * Initialize System Time (clock)
 */
void initStationTime(){

    PLV_DEBUG_HEADER(F("INIT STATION TIME"));

    // Read System Time
    unsigned long sttTime = pluvion.FSReadULong(DIR_STATION_TIME);

    if(sttTime){

        STATION_TIME = sttTime;

    } else {

        PLV_DEBUG(F("CRITICAL! STATION TIME not found.\nUsing millis() instead (bad)."));
        STATION_TIME = millis();
    }
    
    PLV_DEBUG_(F("STATION TIME: "));
    PLV_DEBUG(STATION_TIME);
}

/**
 * Initialize System Time To Reset (count down)
 */
void initTimeToReset(){

    PLV_DEBUG_HEADER(F("INIT TIME TO RESET"));

    // Read System Time To Reset
    unsigned long ttr = pluvion.FSReadULong(DIR_STATION_TIME_TO_RESET);

    if(ttr){

        TIME_TO_RESET = ttr;

    } else {

        PLV_DEBUG(F("CRITICAL! STATION TIME TO RESET not found.\nUsing default time frame (24hrs) instead (bad)."));
        TIME_TO_RESET = DEFAULT_RESET_PERIOD;
    }

    PLV_DEBUG_(F("TIME TO RESET: "));
    PLV_DEBUG(TIME_TO_RESET);
}

/**
 * Increment Station Time
 */
void incrementStationTime() {

    PLV_DEBUG_HEADER(F("INCREMENTING STATION TIME"));

    // Add millis to current Station Time
    STATION_TIME += millis();

    // Remove old Station Time
    pluvion.FSDeleteFiles(DIR_STATION_TIME);

    // Save new Station Time
    pluvion.FSCreateFile(DIR_STATION_TIME, STATION_TIME);
}

/**
 * Update Station Time
 */
void updateStationTime(unsigned long sttTime) {

    PLV_DEBUG_HEADER(F("UPDATING STATION TIME"));

    PLV_DEBUG_(F("STATION TIME RECEIVED: "));
    PLV_DEBUG(sttTime);

    if(sttTime) {

        STATION_TIME = sttTime;

        // Remove system time
        pluvion.FSDeleteFiles(DIR_STATION_TIME);

        // Save new station time
        pluvion.FSCreateFile(DIR_STATION_TIME, STATION_TIME);
    }

    PLV_DEBUG_(F("STATION TIME: "));
    PLV_DEBUG(STATION_TIME);
}

/**
 * Update Station Time
 */
void updateTimeToReset(unsigned long ttr) {

    PLV_DEBUG_HEADER(F("UPDATING TIME TO RESET"));

    if(ttr){

        TIME_TO_RESET = ttr;

        // Remove system counter
        pluvion.FSDeleteFiles(DIR_STATION_TIME_TO_RESET);

        // Save message id
        pluvion.FSCreateFile(DIR_STATION_TIME_TO_RESET, TIME_TO_RESET);
    }

    PLV_DEBUG_(F("TIME TO RESET: "));
    PLV_DEBUG(TIME_TO_RESET);
}

/**
 * Check if it' time to reset the system
 */
void resetSystem() {

    // How does the reset system works...
    //  __________________________________________________> (time)
    //  | start time          |||| millis()              |TIME_TO_RESET
    //  | init during setup

    if((millis() - resetStartTime) >= TIME_TO_RESET) {

        PLV_DEBUG(F("RESTARTING!"));

        // Set the reset target to 24 hours from now
        resetStartTime = millis(); // counting from now
        resetCicleCounter = 0;
        TIME_TO_RESET = DEFAULT_RESET_PERIOD; // until 24 hours from now
        resetRainIndicators();
        
    } else {
        // Persist TIME TO RESET (in case of boot)
        if((++resetCicleCounter % 2400000000) == 0){
            TIME_TO_RESET = (TIME_TO_RESET - ((millis() - resetStartTime)));
            resetCicleCounter=0;
            updateTimeToReset(TIME_TO_RESET);
        }
    }
}

/**
 * thin Reset the system messsage counter file
 */
void resetMessageID() {

    PLV_DEBUG_HEADER(F("RESETTING MESSAGE ID"));

    messageID = 0;

    // Remove system counter
    pluvion.FSDeleteFiles(DIR_MESSAGE_COUNTER);
}

/**
 * Remove the system messsage counter file
 */
void removeMessageByID(int messageID) {

    PLV_DEBUG_HEADER(F("REMOVE WEATHER MESSAGE LOCALLY BY ID"));

    // Remove message from file system
    pluvion.FSDeleteFile(DIR_WEATHER_DATA, messageID);
}

/**
 * Increment Message Counter
 */
void incrementMessageID() {

    PLV_DEBUG_HEADER(F("INCREMENTING MESSAGE COUNTER"));

    // Increment message id
    messageID++;

    // Remove message id
    pluvion.FSDeleteFiles(DIR_MESSAGE_COUNTER);

    // Save message id
    pluvion.FSCreateFile(DIR_MESSAGE_COUNTER, messageID);
}

/**
 * Build message
 * 
 * @return Message
 */
String buildMessage() {

    PLV_DEBUG(F("Building Message..."));

    String message = "";

    // Message ID
    message += messageID; message += FIELD_SEPARATOR;

    // System Timestamp
    message += STATION_TIME; message += FIELD_SEPARATOR;

    // WEATHER INFORMATION ====================================
    // Rain Volume
    message += rainVolume; message += FIELD_SEPARATOR;

    // Hall Sensor - Tip Count
    message += realTipCount; message += FIELD_SEPARATOR;

    // DHT Sensor - Temperature in C
    message += temperature; message += FIELD_SEPARATOR;

    // DHT Sensor - Humidity in Percent
    message += humidity; message += FIELD_SEPARATOR;

    // DHT Sensor - Apparent Temperature (Computed Heat Index)
    message += computedHeatIndex; message += FIELD_SEPARATOR;

    // CONFIGURATION INFORMATION ====================================
    // Station Id
    message += STATION_ID; message += FIELD_SEPARATOR;

    // Station Name
    message += STATION_NAME; message += FIELD_SEPARATOR;

    // Latitude
    message += STATION_LATITUDE; message += FIELD_SEPARATOR;

    // Longitude
    message += STATION_LONGITUDE; message += FIELD_SEPARATOR;

    // Bucket Volume in ml
    message += PLV_STATION_BUCKET_VOLUME; message += FIELD_SEPARATOR;

    // DHT Sensor - Reading Delay
    message += DHT_SENSOR_READING_DELAY; message += FIELD_SEPARATOR;

    // Hall Sensor - Reading Delay
    message += DELAY_HALL_SENSOR_DEBOUNCING; message += FIELD_SEPARATOR;

    // RUNTIME INFORMATION ====================================
    // Available Heap Space (bytes)
    message += SYSAvailableHeapInBytes; message += FIELD_SEPARATOR;

    // Last Reset Reason
    message += SYSLastResetReason; message += FIELD_SEPARATOR;

    // Reset Flag
    message += (resetFlag?"true":"false"); message += FIELD_SEPARATOR;
    resetFlag=false;

    // Time to next reset
    message += TIME_TO_RESET; message += FIELD_SEPARATOR;

    // Firmware Version
    message += FIRMWARE_VERSION; message += FIELD_SEPARATOR;

    // FILE SYSTEM INFORMATION ====================================
    // Available Disk Space (bytes)
    message += SYSAvailableDiskSpaceInBytes; message += FIELD_SEPARATOR;

    // Available Disk Space (%)
    message += SYSAvailableDiskSpaceInPercent; message += FIELD_SEPARATOR;

    // INFRASTRUCTURE - CONNECTIVITY ====================================
    // WiFi SSID
    message += SYSWiFiSSID; message += FIELD_SEPARATOR;

    // WiFi LocalIp
    message += SYSWiFiLocalIp; message += FIELD_SEPARATOR;

    // WiFi Hostname
    message += SYSWiFiHostname; message += FIELD_SEPARATOR;

    // WiFi MACAddr
    message += SYSWiFiMACAddr; message += FIELD_SEPARATOR;

    // WiFi RSSIindBm
    message += SYSWiFiRSSIindBm; message += FIELD_SEPARATOR;

    // INFRASTRUCTURE - POWER SUPPLY ====================================
    // VCC (volts)
    message += SYSVCCInV;

    PLV_DEBUG_(F("\nMESSAGE ("));
    PLV_DEBUG_(message.length());
    PLV_DEBUG(F(" bytes): "));
    PLV_DEBUG(message);

    return message;
}

/**
 * Build the weather message and send to Pluvi.On API
 */
void buildSaveAndSendMessage() {

    PLV_DEBUG_HEADER(F("BUILD WEATHER MESSAGE AND SAVE"));

    // Get System Environmental Information
    getSystemInformation();

    // Build Weather Message
    String message = buildMessage();

    // Save message locally
    int id = messageID;

    // Save message
    saveMessage(id, message);

    // Increment MessageID
    incrementMessageID();

    // Post
    sendMessage(id, message);
}

/**
 * Post weather data to Pluvi.On API
 */
void sendMessage(int messageID, String message) {

    PLV_DEBUG_HEADER(F("POST WEATHER TO PLUVION API"));

    PLV_DEBUG_(F("MessageID:           "));
    PLV_DEBUG(messageID);

    PLV_DEBUG_(F("Message ("));
    PLV_DEBUG_(message.length());
    PLV_DEBUG_(F(" bytes): "));
    PLV_DEBUG(message);

    // Check Connection
    while (!WIFISecureClient.connect(PLUVION_API_SERVER_ADDR, PLUVION_API_SERVER_PORT)) {
        PLV_DEBUG(F("CONNECTION FAILED!"));
        return;
    }

    // Check Certificate Fingerprint
    if (WIFISecureClient.verify(PLUVION_API_CERT_FINGERPRINT, PLUVION_API_SERVER_ADDR)) {
        PLV_DEBUG(F("SSL Certificate Fingerprint Match: PASSED"));
    } else {
        PLV_DEBUG(F("SSL Certificate Fingerprint Match: ERROR!"));
    }

    String requestHeader =
    String("POST ") + PLUVION_API_RESOURCE_WEATHER + " HTTP/1.1" + HTTP_HEADER_EOL +
    "Host: " + PLUVION_API_SERVER_ADDR + HTTP_HEADER_EOL +
    HTTP_HEADER_USER_AGENT + HTTP_HEADER_EOL +
    HTTP_HEADER_ACCEPT + HTTP_HEADER_EOL +
    HTTP_HEADER_CONTENT_TYPE + HTTP_HEADER_EOL +
    "Content-Length: " + message.length() + HTTP_HEADER_EOL +
    "Connection: close" + HTTP_HEADER_EOL + HTTP_HEADER_EOL;

    PLV_DEBUG(F("Request Header:"));
    PLV_DEBUG(requestHeader);

    // Post Data
    WIFISecureClient.print(requestHeader);
    WIFISecureClient.print(message);

    PLV_DEBUG(F("Response:"));

    // Read server response
    // SECTIONS:
    // h - Headers
    // r - Response
    char section = 'x';
    String line = "";
    String response = "";

    while (WIFISecureClient.connected()) {
        if (WIFISecureClient.available()) {    
            line = WIFISecureClient.readStringUntil('\r');
            PLV_DEBUG_(line);
            
            // Body
            if (line == "\n") {
                section = 'r';
            }
            
            if (section == 'r' && line.length() > 1) {
                response += line.substring(1);
            }
        }
    }

    PLV_DEBUG(F("\n\nClosing connection."));
    WIFISecureClient.stop();

    // Process the response and/or takes an action
    processResponse(messageID, response);
}

/**
 * Process server response
 */
void processResponse(int messageID, String response) {

    PLV_DEBUG_HEADER(F("PROCESS RESPONSE"));

    PLV_DEBUG_(F("MessageID: "));
    PLV_DEBUG(messageID);

    PLV_DEBUG_(F("Response:  "));
    PLV_DEBUG(response);

    if (response.length() < 1 || response.indexOf('|') == -1) {
        PLV_DEBUG(F("Response empty! Nothing to do, keep walking :)"));
        return;
    }

    // Get response as char array
    char* res = (char*)response.c_str();

    // Read each command pair 
    char* msg = strtok(res, "|");
    int index=0;

    do
    {
        PLV_DEBUG_(F(" ["));
        PLV_DEBUG_(index);
        PLV_DEBUG_(F("] "));
        PLV_DEBUG(msg);
    
        // 0 - Result
        if(index == 0) {

            PLV_DEBUG_(F("Request Result: [")); PLV_DEBUG_(msg); PLV_DEBUG(F("]"));

            if (String(msg) == "ok") {
                // Remove local file
                removeMessageByID(messageID);
            }
            continue;
        }

        // 1 - System Time in millis
        if(index == 1){

            PLV_DEBUG_(F("Server time: ")); PLV_DEBUG(msg);
            updateStationTime(strtoul(msg, NULL, 10));

            continue;
        }
    
        // 2 - Time to Reset System
        if(index == 2){

            PLV_DEBUG_(F("Time to reset: ")); PLV_DEBUG(msg);
            resetStartTime = millis();
            updateTimeToReset(strtoul(msg, NULL, 10));

            continue;
        }
    
        // 3 - Server Command
        if(index == 3){

            PLV_DEBUG_(F("Server Command: ")); PLV_DEBUG(msg);
            
            if (msg == "reset") {
                // Reset Appliance
                PLV_DEBUG(F("RESET !!!!"));
                resetRainIndicators();
            }
            
            continue;
        }

    } while (
        (msg = strtok(0, "|")) // Find the next command in input string
        && ++index // Increment the command index
    );
}

/**
 * Write weather data to file system
 */
boolean saveMessage(int messageID, String message) {

    PLV_DEBUG_HEADER(F("SAVE WEATHER DATA LOCALLY"));

    // Save message
    return pluvion.FSWriteToFile(DIR_WEATHER_DATA, messageID, message);
}

/**
 * Read DHT Sensor
 */
void readDHTSensor() {

    // Current millis mark
    currentDHTReadingInMillis = millis();

    // Check if the past reading has failed and the time
    if (
        DHTSensorReadingFailure && // Fail
        ((currentDHTReadingInMillis - lastDHTReadingInMillis) >= DHT_SENSOR_READING_FAILURE_DELAY) && // The time hasn' passed yet
        (DHTSensorFailedReadingAttempts <= DHT_SENSOR_READING_FAILURE_ATTEMPTS_LIMIT) // Number of attempts
    ) {
    
        PLV_DEBUG(F("DHT SENSOR READING ON FAILURE!"));
        PLV_DEBUG_(F("DHTSensorFailedReadingAttempts: "));
        PLV_DEBUG(DHTSensorFailedReadingAttempts);
    
        DHTSensorFailedReadingAttempts++;
        getDHTData();
    
        // Build the weather message and send to Pluvi.On API
        buildSaveAndSendMessage();
    }

    // Check if the configured delay has passed
    if ((currentDHTReadingInMillis - lastDHTReadingInMillis) >= DHT_SENSOR_READING_DELAY) {

        getDHTData();

        // Build the weather message and send to Pluvi.On API
        buildSaveAndSendMessage();
    }
}

/**
 * Get DHT Data
 */
void getDHTData() {

    PLV_DEBUG_HEADER(F("DHT SENSOR READINGS"));

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();

    // Read temperature as Celsius
    temperature = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {

        PLV_DEBUG(F("ERROR! Failed to read from DHT sensor!"));

        // Indicates a failure in readings
        DHTSensorReadingFailure = true;

        // Fix humidity, temperature and computed heat index
        humidity = isnan(humidity)? -999:humidity;
        temperature = isnan(temperature)? -999:temperature;
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

    PLV_DEBUG_(F("computeHeatIndex(temperature, humidity, false)("));
    PLV_DEBUG_(temperature);
    PLV_DEBUG_(F(","));
    PLV_DEBUG_(humidity);
    PLV_DEBUG_(F(",false): "));
    PLV_DEBUG(computedHeatIndex);

    computedHeatIndex = (computedHeatIndex > 100) ? 0 : computedHeatIndex;

    PLV_DEBUG_(F("Temperature: "));
    PLV_DEBUG_(temperature);
    PLV_DEBUG(F(" C"));

    PLV_DEBUG_(F("   Humidity: "));
    PLV_DEBUG_(humidity);
    PLV_DEBUG(F(" %"));

    PLV_DEBUG_(F(" Heat index: "));
    PLV_DEBUG_(computedHeatIndex);
    PLV_DEBUG(F(" C"));

    // Store the current millis for the next iteration
    lastDHTReadingInMillis = currentDHTReadingInMillis;
}

/**
 * Reads the hall sensor (tip bucket)
 */
void readHallSensor() {

    // Current millis mark
    currentTipBucketReadingInMillis = millis();

    // Check if the configured delay has passed
    if (
        ((currentTipBucketReadingInMillis - lastTipBucketReadingInMillis) >= DELAY_HALL_SENSOR_DEBOUNCING) || // delay
        (lastTipBucketReadingInMillis == 0) // first time
    ) {
        // Disable interrupt when calculating
        detachInterrupt(PIN_HALL);

        // Any increment since the last time?
        if (tipCounter > lastTipCount) {

            // Increase the tip bucket count
            realTipCount++;
    
            // Update last tip counter value
            lastTipCount = tipCounter;
    
            // Calculate the rain volume, converting tips into mm
            rainVolume = realTipCount * PLV_STATION_BUCKET_VOLUME * 1000 / CONTRIBUTION_AREA;
    
            // Get DHT Sensor Data
            getDHTData();
    
            // Build the weather message and send to Pluvi.On API
            buildSaveAndSendMessage();
        }

        // Store the current millis for the next iteration
        lastTipBucketReadingInMillis = currentTipBucketReadingInMillis;

        // Enable interrupt again, CHANGE = trigger the interrupt whenever the pin changes value
        attachInterrupt(PIN_HALL, countTipBucket, CHANGE);
    }
}

/**
 * Reset module WiFi Settings
 */
void resetWiFiSettings() {

    PLV_DEBUG_HEADER(F("RESETTING WIFI SETTINGS"));

    // Reset WiFi Settings
    PLV_DEBUG(F("Resetting WiFi Manager..."));
    wifiManager.resetSettings();
}

/**
 * Setup WiFi module
 */
void setupWiFiModule() {

    PLV_DEBUG_HEADER(F("SETUP WIFI MODULE"));

    int count=0;
    while(true){

        PLV_DEBUG(F("Connecting... "));

        ETS_UART_INTR_DISABLE();
        wifi_station_disconnect();
        ETS_UART_INTR_ENABLE();

        // WiFi.begin();
        WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());

        PLV_DEBUG_(F("Status: "));
        if (WiFi.status() == WL_CONNECTED) {
            PLV_DEBUG(F("CONNECTED - Wi-Fi connection successful established."));
            break;
        } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
            PLV_DEBUG(F("NO SSID AVAILABLE - Configured SSID cannot be reached."));
            wifiManager.autoConnect(STATION_ID.c_str());

        } else if (WiFi.status() == WL_CONNECT_FAILED) {
            PLV_DEBUG(F("CONNECT FAILED - Password is incorrect."));
            wifiManager.autoConnect(STATION_ID.c_str());

        } else if (WiFi.status() == WL_IDLE_STATUS) {
            PLV_DEBUG(F("IDLE STATUS - Wi-Fi is in process of changing between statuses."));

        } else if (WiFi.status() == WL_DISCONNECTED) {
            PLV_DEBUG(F("DISCONNECTED - Wi-Fi module is not configured in station mode."));
        }

        delay(WIFI_CONN_DELAY);

        if(count++ > WIFI_CONN_COUNTER){
            // Connect to WiFi
            wifiManager.autoConnect(STATION_ID.c_str());
            break;
        }
    }
}


/**
 * Connect to WiFi Network
 */
void connectWiFi(){

    PLV_DEBUG_HEADER(F("CONNECTING TO WIFI"));

    printSystemWiFiStatus();

    // Connect to WiFi
    wifiManager.autoConnect(STATION_ID.c_str());
}

/**
 * Disconnect from WiFi Network
 */
void disconnectWiFi(){

    PLV_DEBUG_HEADER(F("DISCONNECTING FROM WIFI"));

    // Disconnect WiFi
    
    wifi_station_disconnect();

    printSystemWiFiStatus();
}

/**
 * System SETUP
 */
void setup() {

    // Init the Serial
    PLV_DEBUG_SETUP(PLV_SYSTEM_BAUDRATE);

    PLV_DEBUG_(F("\n\nPLUVI.ON WIFI FIRMWARE (Version "));
    PLV_DEBUG_(FIRMWARE_VERSION);
    PLV_DEBUG(F(")"));

    // Initialize Station ID
    initStationID();

    // Register Firmware Version
    initFirmwareVersion();

    // Print the file list
    pluvion.FSPrintFileList();

    // Setup WiFi Module
    setupWiFiModule();

    // Init all system configurations
    initSystem();

    // Print System Current Status
    printSystemStatus();

    // Print System Usage
    printCommands();

    PLV_DEBUG(F("STARTING READINGS..."));

    // Read DHT Data
    getDHTData();

    // Build the weather message and send to Pluvi.On API
    buildSaveAndSendMessage();
}

/**
 * System main loop function
 */
void loop() {

    // Enables command on serial
    readSerialCommands();

    // Reads the DHT Sensor Info
    readDHTSensor();

    // Reads the hall sensor (tip bucket)
    readHallSensor();

    // Reset System
    resetSystem();

    // Send Offline messages
    sendOfflineMessages();
}
