/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/tree/esp8266/hardware/esp8266com/esp8266/libraries/DNSServer/examples/CaptivePortalAdvanced
   Built by AlexT https://github.com/tzapu
   Licensed under MIT license
 **************************************************************/

#include <FS.h> // FS must be the first
#include "WiFiManager.h"

// RESET TIME FORMAT:
//  - hours,minutes
//  - No leading zeros (wrong: 10,06, right: 10,6)
//  - MUST HAVE AND MUST BE SEPARATED BY COMMA
//  - GMT: Brasilia
const String RESET_TIME                 = "7,0";

const String DIR_STATION_BUCKET_VOL     = "/stt/bucketvol";
const String DIR_STATION_LATITUDE       = "/stt/lat";
const String DIR_STATION_LONGITUDE      = "/stt/lon";
const String DIR_STATION_TIME_TO_RESET  = "/stt/ttr";
const String DIR_STATION_NAME           = "/stt/name";
const String DIR_FIRMWARE_VERSION       = "/fmwver";

String fmwver = "";
String stationID = "PluviOn_";
String macAddr = "";

WiFiManagerParameter::WiFiManagerParameter(const char *custom) {
  _id = NULL;
  _placeholder = NULL;
  _length = 0;
  _value = NULL;

  _customHTML = custom;
}

WiFiManagerParameter::WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length) {
  init(id, placeholder, defaultValue, length, "");
}

WiFiManagerParameter::WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom) {
  init(id, placeholder, defaultValue, length, custom);
}

void WiFiManagerParameter::init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom) {
  _id = id;
  _placeholder = placeholder;
  _length = length;
  _value = new char[length + 1];
  for (int i = 0; i < length; i++) {
    _value[i] = 0;
  }
  if (defaultValue != NULL) {
    strncpy(_value, defaultValue, length);
  }

  _customHTML = custom;
}

const char* WiFiManagerParameter::getValue() {
  return _value;
}
const char* WiFiManagerParameter::getID() {
  return _id;
}
const char* WiFiManagerParameter::getPlaceholder() {
  return _placeholder;
}
int WiFiManagerParameter::getValueLength() {
  return _length;
}
const char* WiFiManagerParameter::getCustomHTML() {
  return _customHTML;
}

WiFiManager::WiFiManager() {
}

void WiFiManager::addParameter(WiFiManagerParameter *p) {
  _params[_paramsCount] = p;
  _paramsCount++;
  DEBUG_WM("Adding parameter");
  DEBUG_WM(p->getID());
}

void WiFiManager::setupConfigPortal() {
  dnsServer.reset(new DNSServer());
  server.reset(new ESP8266WebServer(80));

  DEBUG_WM(F(""));
  _configPortalStart = millis();

  DEBUG_WM(F("Configuring access point... "));
  DEBUG_WM(_apName);
  if (_apPassword != NULL) {
    if (strlen(_apPassword) < 8 || strlen(_apPassword) > 63) {
      // fail passphrase to short or long!
      DEBUG_WM(F("Invalid AccessPoint password. Ignoring"));
      _apPassword = NULL;
    }
    DEBUG_WM(_apPassword);
  }

  //optional soft ip config
  if (_ap_static_ip) {
    DEBUG_WM(F("Custom AP IP/GW/Subnet"));
    WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn);
  }

  if (_apPassword != NULL) {
    WiFi.softAP(_apName, _apPassword);//password option
  } else {
    WiFi.softAP(_apName);
  }

  delay(500); // Without delay I've seen the IP address blank
  DEBUG_WM(F("AP IP address: "));
  DEBUG_WM(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  //server->on("/generate_204", std::bind(&WiFiManager::handle204, this));  //Android/Chrome OS captive portal check.
  server->on("/", std::bind(&WiFiManager::handleRoot, this));
  server->on("/wifi", std::bind(&WiFiManager::handleWifi, this, true));
  server->on("/0wifi", std::bind(&WiFiManager::handleWifi, this, false));
  server->on("/wifisave", std::bind(&WiFiManager::handleWifiSave, this));
  server->on("/i", std::bind(&WiFiManager::handleInfo, this));
  server->on("/r", std::bind(&WiFiManager::handleReset, this));
  server->on("/fwlink", std::bind(&WiFiManager::handleRoot, this));  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.

  // Pluvi.On
  server->on("/savePluvionConfig", std::bind(&WiFiManager::handleSavePluviOnConfig, this));

  server->onNotFound (std::bind(&WiFiManager::handleNotFound, this));
  server->begin(); // Web server start
  DEBUG_WM(F("HTTP server started"));

  // Get firmware version
  fmwver = getFirmwareVersion();

  // Set station Id
  stationID += ESP.getChipId();

  // Get Station MAC Address
  macAddr = WiFi.softAPmacAddress();
}

boolean WiFiManager::autoConnect() {
  String ssid = "ESP" + String(ESP.getChipId());
  return autoConnect(ssid.c_str(), NULL);
}

boolean WiFiManager::autoConnect(char const *apName, char const *apPassword) {
  DEBUG_WM(F(""));
  DEBUG_WM(F("AutoConnect"));

  // read eeprom for ssid and pass
  //String ssid = getSSID();
  //String pass = getPassword();

  // attempt to connect; should it fail, fall back to AP
  WiFi.mode(WIFI_STA);

  if (connectWifi("", "") == WL_CONNECTED)   {
    DEBUG_WM(F("IP Address:"));
    DEBUG_WM(WiFi.localIP());
    //connected
    return true;
  }

  return startConfigPortal(apName, apPassword);
}

boolean  WiFiManager::startConfigPortal(char const *apName, char const *apPassword) {
  //setup AP
  WiFi.mode(WIFI_AP_STA);
  DEBUG_WM("SET AP STA");

  _apName = apName;
  _apPassword = apPassword;

  //notify we entered AP mode
  if ( _apcallback != NULL) {
    _apcallback(this);
  }

  connect = false;
  setupConfigPortal();

  while (_configPortalTimeout == 0 || millis() < _configPortalStart + _configPortalTimeout) {
    //DNS
    dnsServer->processNextRequest();
    //HTTP
    server->handleClient();


    if (connect) {
      connect = false;
      delay(2000);
      DEBUG_WM(F("Connecting to new AP"));

      // using user-provided  _ssid, _pass in place of system-stored ssid and pass
      if (connectWifi(_ssid, _pass) != WL_CONNECTED) {
        DEBUG_WM(F("Failed to connect."));
      } else {
        //connected
        WiFi.mode(WIFI_STA);
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        break;
      }

      if (_shouldBreakAfterConfig) {
        //flag set to exit after config after trying to connect
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        break;
      }
    }
    yield();
  }

  server.reset();
  dnsServer.reset();

  return  WiFi.status() == WL_CONNECTED;
}


int WiFiManager::connectWifi(String ssid, String pass) {
  DEBUG_WM(F("Connecting as wifi client..."));

  // check if we've got static_ip settings, if we do, use those.
  if (_sta_static_ip) {
    DEBUG_WM(F("Custom STA IP/GW/Subnet"));
    WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
    DEBUG_WM(WiFi.localIP());
  }
  //fix for auto connect racing issue
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_WM("Already connected. Bailing out.");
    return WL_CONNECTED;
  }
  //check if we have ssid and pass and force those, if not, try with last saved values
  if (ssid != "") {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    if (WiFi.SSID()) {
      DEBUG_WM("Using last saved values, should be faster");
      //trying to fix connection in progress hanging
      ETS_UART_INTR_DISABLE();
      wifi_station_disconnect();
      ETS_UART_INTR_ENABLE();

      WiFi.begin();
    } else {
      DEBUG_WM("No saved credentials");
    }
  }

  int connRes = waitForConnectResult();
  DEBUG_WM ("Connection result: ");
  DEBUG_WM ( connRes );
  //not connected, WPS enabled, no pass - first attempt
  if (_tryWPS && connRes != WL_CONNECTED && pass == "") {
    startWPS();
    //should be connected at the end of WPS
    connRes = waitForConnectResult();
  }
  return connRes;
}

uint8_t WiFiManager::waitForConnectResult() {
  if (_connectTimeout == 0) {
    return WiFi.waitForConnectResult();
  } else {
    DEBUG_WM (F("Waiting for connection result with time out"));
    unsigned long start = millis();
    boolean keepConnecting = true;
    uint8_t status;
    while (keepConnecting) {
      status = WiFi.status();
      if (millis() > start + _connectTimeout) {
        keepConnecting = false;
        DEBUG_WM (F("Connection timed out"));
      }
      if (status == WL_CONNECTED || status == WL_CONNECT_FAILED) {
        keepConnecting = false;
      }
      delay(100);
    }
    return status;
  }
}

void WiFiManager::startWPS() {
  DEBUG_WM("START WPS");
  WiFi.beginWPSConfig();
  DEBUG_WM("END WPS");
}
/*
  String WiFiManager::getSSID() {
  if (_ssid == "") {
    DEBUG_WM(F("Reading SSID"));
    _ssid = WiFi.SSID();
    DEBUG_WM(F("SSID: "));
    DEBUG_WM(_ssid);
  }
  return _ssid;
  }

  String WiFiManager::getPassword() {
  if (_pass == "") {
    DEBUG_WM(F("Reading Password"));
    _pass = WiFi.psk();
    DEBUG_WM("Password: " + _pass);
    //DEBUG_WM(_pass);
  }
  return _pass;
  }
*/
String WiFiManager::getConfigPortalSSID() {
  return _apName;
}

void WiFiManager::resetSettings() {
  DEBUG_WM(F("settings invalidated"));
  DEBUG_WM(F("THIS MAY CAUSE AP NOT TO START UP PROPERLY. YOU NEED TO COMMENT IT OUT AFTER ERASING THE DATA."));
  WiFi.disconnect(true);
  //delay(200);
}
void WiFiManager::setTimeout(unsigned long seconds) {
  setConfigPortalTimeout(seconds);
}

void WiFiManager::setConfigPortalTimeout(unsigned long seconds) {
  _configPortalTimeout = seconds * 1000;
}

void WiFiManager::setConnectTimeout(unsigned long seconds) {
  _connectTimeout = seconds * 1000;
}

void WiFiManager::setDebugOutput(boolean debug) {
  _debug = debug;
}

void WiFiManager::setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn) {
  _ap_static_ip = ip;
  _ap_static_gw = gw;
  _ap_static_sn = sn;
}

void WiFiManager::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn) {
  _sta_static_ip = ip;
  _sta_static_gw = gw;
  _sta_static_sn = sn;
}

void WiFiManager::setMinimumSignalQuality(int quality) {
  _minimumQuality = quality;
}

void WiFiManager::setBreakAfterConfig(boolean shouldBreak) {
  _shouldBreakAfterConfig = shouldBreak;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PLUVION CODES - BEGIN
//////////////////////////////////////////////////////////////////////////////////////////////////////

/** Handle root or redirect to captive portal */
void WiFiManager::handleRoot() {

  DEBUG_WM(F("Handle root"));

  if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
    return;
  }

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Home | Pluvi.On");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);

  // Get the previous bucket configuration
  String vol = getBucketVolume();

  // Get the previous Station Name
  String name = getStationName();

  // Setup the form
  String form = FPSTR(HTTP_FORM_CONFIG);
  form.replace("{vol}", vol);
  form.replace("{name}", name);
  form.replace("{sttid}", stationID);

  page += form;

  // Setup the script
  String script = FPSTR(HTTP_SCRIPT_FORM_CONFIG);
  script.replace("{RESET_TIME}", RESET_TIME);
  page += script;

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;

  DEBUG_WM("\n\nPAGE:");
  DEBUG_WM(page);
  DEBUG_WM("\n\n");

  server->send(200, "text/html", page);

}

/**
 * Save Pluvi.On configuration
 */
void WiFiManager::handleSavePluviOnConfig() {

  DEBUG_WM(F("\n\nSAVE PLUVION CONFIG"));
  DEBUG_WM(F("===========================================\n"));

  // Save location on file system
  saveCoordinates();

  // Save bucket volume on file system
  saveBucketVolume();

  // Save time to next reset
  saveTimeToReset();

  // Save station name
  saveStationName();

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Configurações Salvas | Pluvi.On");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_PORTAL_OPTIONS);

  page.replace("{lat}",  server->arg("lat"));
  page.replace("{lon}",  server->arg("lon"));
  page.replace("{vol}",  server->arg("vol"));
  page.replace("{ttr}",  server->arg("ttr"));
  page.replace("{name}", server->arg("name"));

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;

  server->send(200, "text/html", page);
}

/**
 * Get Firmware Version
 */
String WiFiManager::getFirmwareVersion() {
  
  DEBUG_WM(F("\n\nGET FIRMWARE VERSION"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ngetFirmwareVersion() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String fmwver = "";

  // Open the firmware version directory
  Dir dirFmw = SPIFFS.openDir(DIR_FIRMWARE_VERSION);

  DEBUG_WM(F("Getting Firmware Version..."));

  if(dirFmw.next()){

    fmwver = dirFmw.fileName().substring(DIR_FIRMWARE_VERSION.length() + 1);
    DEBUG_WM(F("Firmware Version: "));
    DEBUG_WM(fmwver);

  } else {

    DEBUG_WM(F("ERROR during reading Firmware Version!"));

  }

  return fmwver;
}

/**
 * Get Latitude
 */
String WiFiManager::getLatitude() {
  
  DEBUG_WM(F("\n\nGET LATITUDE"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ngetLatitude() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String lat = "";

  // Open the latitude directory
  Dir dirLat = SPIFFS.openDir(DIR_STATION_LATITUDE);

  DEBUG_WM(F("Getting Latitude..."));

  if(dirLat.next()){

    lat = dirLat.fileName().substring(DIR_STATION_LATITUDE.length() + 1);
    lat.replace(",",".");
    DEBUG_WM(F("Latitude: "));
    DEBUG_WM(lat);

  } else {

    DEBUG_WM(F("ERROR during reading latitude!"));

  }

  return lat;
}

/**
 * Get Longitude
 */
String WiFiManager::getLongitude() {

  DEBUG_WM(F("\n\nGET LONGITUDE"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ngetLongitude() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String lon = "";

  // Open the longitude directory
  Dir dirLon = SPIFFS.openDir(DIR_STATION_LONGITUDE);

  DEBUG_WM(F("Getting Longitude..."));

  if(dirLon.next()){

    lon = dirLon.fileName().substring(DIR_STATION_LONGITUDE.length() + 1);
    lon.replace(",",".");
    DEBUG_WM(F("Longitude: "));
    DEBUG_WM(lon);

  } else {

    DEBUG_WM(F("ERROR during reading longitude!"));

  }
  return lon;
}

/**
 * Get Bucket Volume
 */
String WiFiManager::getBucketVolume() {

  DEBUG_WM(F("\n\nGET BUCKET VOLUME"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ngetBucketVolume() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String vol = "";

  // Open the longitude directory
  Dir dirVol = SPIFFS.openDir(DIR_STATION_BUCKET_VOL);

  DEBUG_WM(F("Getting Bucket Volume..."));

  if(dirVol.next()){

    vol = dirVol.fileName().substring(DIR_STATION_BUCKET_VOL.length() + 1);
    vol.replace(",",".");
    DEBUG_WM(F("Bucket Volume: "));
    DEBUG_WM(vol);

  } else {

    DEBUG_WM(F("ERROR during reading Bucket Volume!"));

  }

  return vol;  
}

/**
 * Get Bucket Volume
 */
String WiFiManager::getTimeToReset() {

  DEBUG_WM(F("\n\nGET TIME TO RESET"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ngetTimeToReset() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String rt = "";

  // Open the longitude directory
  Dir dirRT = SPIFFS.openDir(DIR_STATION_TIME_TO_RESET);

  DEBUG_WM(F("Getting Reset Time..."));

  if(dirRT.next()){

    rt = dirRT.fileName().substring(DIR_STATION_TIME_TO_RESET.length() + 1);
    rt.replace(",",".");
    DEBUG_WM(F("Reset Time: "));
    DEBUG_WM(rt);

  } else {

    DEBUG_WM(F("ERROR during reading Reset Time!"));

  }

  return rt;  
}

/**
 * Save Station Name
 */
void WiFiManager::saveStationName(){

    deleteStationName();

    DEBUG_WM(F("\n\nSAVING STATION NAME"));
    DEBUG_WM(F("===========================================\n"));

    String stationName = server->arg("name");

    DEBUG_WM(F("Station Name:"));
    DEBUG_WM(stationName);

    DEBUG_WM(F("Saving Station Name..."));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        DEBUG_WM(F("\n\nsaveStationName() - FATAL! Error mounting SPIFFS file system!\n\n"));
    }

    String stationNameFile = DIR_STATION_NAME;
      stationNameFile += "/";
      stationNameFile += stationName;

    File fsn = SPIFFS.open(stationNameFile, "w");
    if (!fsn) {
        DEBUG_WM(F("ERROR! Fail saving Station Name file."));
    } else {
        DEBUG_WM(F("SUCCESS! Station Name file:"));
        DEBUG_WM(fsn.name());
        fsn.close();
    }

}

/**
 * Remove Station Name
 */
void WiFiManager::deleteStationName(){
    DEBUG_WM(F("\n\nDELETING STATION NAME"));
    DEBUG_WM(F("===========================================\n"));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        DEBUG_WM(F("\n\ndeleteStationName() - FATAL! Error mounting SPIFFS file system!\n\n"));
    }

    // Open the station name directory
    Dir dir = SPIFFS.openDir(DIR_STATION_NAME);

    DEBUG_WM(F("Deleting Station Name..."));
    while(dir.next()){

        DEBUG_WM(F("Deleting file: "));
        DEBUG_WM(dir.fileName());

        // Remove file
        if (SPIFFS.remove(dir.fileName())) {
            DEBUG_WM(F("File removed successfully."));
        } else {
            DEBUG_WM(F("ERROR during station name file deletion!"));
        }
    }
}

/**
 * Get Station Name
 */
String WiFiManager::getStationName(){

    DEBUG_WM(F("\n\nGET STATION NAME"));
    DEBUG_WM(F("===========================================\n"));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
      DEBUG_WM(F("\n\ngetStationName() - FATAL! Error mounting SPIFFS file system!\n\n"));
    }

    String sn = "";

    // Open the longitude directory
    Dir dirSN = SPIFFS.openDir(DIR_STATION_NAME);

    DEBUG_WM(F("Getting Station Name ..."));

    if(dirSN.next()){

      sn = dirSN.fileName().substring(DIR_STATION_NAME.length() + 1);
      DEBUG_WM(F("Station Name: "));
      DEBUG_WM(sn);

    } else {

      DEBUG_WM(F("ERROR during reading Station Name!"));

    }

    return sn;  
}

/**
 * Remove the Locations
 */
void WiFiManager::deleteCoordinates() {

  DEBUG_WM(F("\n\nDELETING COORDINATES"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ndeleteCoordinates() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  // Open the latitude and longitude directory
  Dir dirLat = SPIFFS.openDir(DIR_STATION_LATITUDE);
  Dir dirLon = SPIFFS.openDir(DIR_STATION_LONGITUDE);

  DEBUG_WM(F("Deleting Latitudes..."));
  while(dirLat.next()){

    DEBUG_WM(F("Deleting file: "));
    DEBUG_WM(dirLat.fileName());

    // Remove file
    if (SPIFFS.remove(dirLat.fileName())) {
      DEBUG_WM(F("File removed successfully."));
    } else {
      DEBUG_WM(F("ERROR during weather file deletion!"));
    }
  }

  DEBUG_WM(F("Deleting Longitudes..."));
  while(dirLon.next()){

    DEBUG_WM(F("Deleting file: "));
    DEBUG_WM(dirLon.fileName());

    // Remove file
    if (SPIFFS.remove(dirLon.fileName())) {
      DEBUG_WM(F("File removed successfully."));
    } else {
      DEBUG_WM(F("ERROR during weather file deletion!"));
    }

  }
}

/**
 * Delete Bucket Volume
 */
void WiFiManager::deleteBucketVolume() {


  DEBUG_WM(F("\n\nDELETING BUCKET VOLUMES"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ndeleteBucketVolume() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  // Open the Bucket Volume directory
  Dir dir = SPIFFS.openDir(DIR_STATION_BUCKET_VOL);

  DEBUG_WM(F("Deleting Bucket Volume..."));
  while(dir.next()){

    DEBUG_WM(F("Deleting file: "));
    DEBUG_WM(dir.fileName());

    // Remove file
    if (SPIFFS.remove(dir.fileName())) {
      DEBUG_WM(F("File removed successfully."));
    } else {
      DEBUG_WM(F("ERROR during weather file deletion!"));
    }
  }

}

/**
 * Delete Time to Reset
 */
void WiFiManager::deleteTimeToReset() {


  DEBUG_WM(F("\n\nDELETING TIME TO RESET"));
  DEBUG_WM(F("===========================================\n"));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\ndeleteTimeToReset() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  // Open the Bucket Volume directory
  Dir dir = SPIFFS.openDir(DIR_STATION_TIME_TO_RESET);

  DEBUG_WM(F("Deleting Timet to Reset..."));
  while(dir.next()){

    DEBUG_WM(F("Deleting file: "));
    DEBUG_WM(dir.fileName());

    // Remove file
    if (SPIFFS.remove(dir.fileName())) {
      DEBUG_WM(F("File removed successfully."));
    } else {
      DEBUG_WM(F("ERROR during time to reset file deletion!"));
    }
  }

}

/**
 * Save coordinates to file system
 */
void WiFiManager::saveCoordinates() {
  
  deleteCoordinates();

  DEBUG_WM(F("\n\nSAVING COORDINATES"));
  DEBUG_WM(F("===========================================\n"));


  String lat = server->arg("lat");
  String lon = server->arg("lon");

  // Replace comma by period
  lat.replace(",",".");
  lon.replace(",",".");

  DEBUG_WM(F("Latitude: "));
  DEBUG_WM(lat);
  DEBUG_WM(F("Longitude: "));
  DEBUG_WM(lon);

  DEBUG_WM(F("Saving location..."));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\nsaveCoordinates() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String latFile = DIR_STATION_LATITUDE;
    latFile += "/";
    latFile += lat;

  String lonFile = DIR_STATION_LONGITUDE;
    lonFile += "/";
    lonFile += lon;

  File fLat = SPIFFS.open(latFile, "w");
  if (!fLat) {
    DEBUG_WM(F("ERROR! Fail saving Latitude file."));
  } else {
    DEBUG_WM(F("SUCCESS! Latitude file:"));
    DEBUG_WM(fLat.name());
    fLat.close();
  }

  File fLon = SPIFFS.open(lonFile, "w");
  if (!fLon) {
    DEBUG_WM(F("ERROR! Fail saving Longitude file."));
  } else {
    DEBUG_WM(F("SUCCESS! Longitude file:"));
    DEBUG_WM(fLon.name());
    fLon.close();
  }

}

/** Save calibrated Bucket Volume to File System */
void WiFiManager::saveBucketVolume() {
  
  deleteBucketVolume();

  DEBUG_WM(F("\n\nSAVING BUCKET VOLUME"));
  DEBUG_WM(F("===========================================\n"));

  String bucketVolume = server->arg("vol");

  // Replace comma by period
  bucketVolume.replace(",",".");

  DEBUG_WM(F("Bucket Volume:"));
  DEBUG_WM(bucketVolume);

  DEBUG_WM(F("Saving Bucket Volume..."));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\nsaveBucketVolume() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String bucketVolFile = DIR_STATION_BUCKET_VOL;
    bucketVolFile += "/";
    bucketVolFile += bucketVolume;

  File fBV = SPIFFS.open(bucketVolFile, "w");
  if (!fBV) {
    DEBUG_WM(F("ERROR! Fail saving Bucket Volume file."));
  } else {
    DEBUG_WM(F("SUCCESS! Bucket Volume file:"));
    DEBUG_WM(fBV.name());
    fBV.close();
  }
}

/** Save next time to reset to File System */
void WiFiManager::saveTimeToReset() {
  
  deleteTimeToReset();

  DEBUG_WM(F("\n\nSAVING TIME TO RESET"));
  DEBUG_WM(F("===========================================\n"));

  String ttr = server->arg("ttr");

  DEBUG_WM(F("Reset time:"));
  DEBUG_WM(ttr);

  DEBUG_WM(F("Saving Time to Reset..."));

  // Mounts SPIFFS file system
  if (!SPIFFS.begin()) {
    DEBUG_WM(F("\n\nsaveTimeToReset() - FATAL! Error mounting SPIFFS file system!\n\n"));
  }

  String ttrFile = DIR_STATION_TIME_TO_RESET;
    ttrFile += "/";
    ttrFile += ttr;

  File ttrF = SPIFFS.open(ttrFile, "w");
  if (!ttrF) {
    DEBUG_WM(F("ERROR! Fail saving Reset Time file."));
  } else {
    DEBUG_WM(F("SUCCESS! Time to Reset file:"));
    DEBUG_WM(ttrF.name());
    ttrF.close();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
// PLUVION CODES - END
//////////////////////////////////////////////////////////////////////////////////////////////////////

/** Wifi config page handler */
void WiFiManager::handleWifi(boolean scan) {

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Configurar WiFi | Pluvi.On");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h3>Pluvi.On | WiFi</h3>");
  page += F("<h5>Redes localizadas:</h5>");

  if (scan) {
    int n = WiFi.scanNetworks();
    DEBUG_WM(F("Scan done"));
    if (n == 0) {
      DEBUG_WM(F("Nenhuma rede encontrada"));
      page += F("Nenhuma rede encontrada. Atualize para procurar novamente.");
    } else {

      //sort networks
      int indices[n];
      for (int i = 0; i < n; i++) {
        indices[i] = i;
      }

      // RSSI SORT

      // old sort
      for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
          if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            std::swap(indices[i], indices[j]);
          }
        }
      }

      /*std::sort(indices, indices + n, [](const int & a, const int & b) -> bool
        {
        return WiFi.RSSI(a) > WiFi.RSSI(b);
        });*/

      // remove duplicates ( must be RSSI sorted )
      if (_removeDuplicateAPs) {
        String cssid;
        for (int i = 0; i < n; i++) {
          if (indices[i] == -1) continue;
          cssid = WiFi.SSID(indices[i]);
          for (int j = i + 1; j < n; j++) {
            if (cssid == WiFi.SSID(indices[j])) {
              DEBUG_WM("DUP AP: " + WiFi.SSID(indices[j]));
              indices[j] = -1; // set dup aps to index -1
            }
          }
        }
      }

      //display networks in page
      for (int i = 0; i < n; i++) {
        if (indices[i] == -1) continue; // skip dups
        DEBUG_WM(WiFi.SSID(indices[i]));
        DEBUG_WM(WiFi.RSSI(indices[i]));
        int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));

        if (_minimumQuality == -1 || _minimumQuality < quality) {
          String item = FPSTR(HTTP_ITEM);
          String rssiQ;
          rssiQ += quality;
          item.replace("{v}", WiFi.SSID(indices[i]));
          item.replace("{r}", rssiQ);
          if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) {
            item.replace("{i}", "l");
          } else {
            item.replace("{i}", "");
          }
          //DEBUG_WM(item);
          page += item;
          delay(0);
        } else {
          DEBUG_WM(F("Skipping due to quality"));
        }

      }
      page += "<br/>";
    }
  }

  page += FPSTR(HTTP_FORM_START);
  char parLength[2];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }

    String pitem = FPSTR(HTTP_FORM_PARAM);
    if (_params[i]->getID() != NULL) {
      pitem.replace("{i}", _params[i]->getID());
      pitem.replace("{n}", _params[i]->getID());
      pitem.replace("{p}", _params[i]->getPlaceholder());
      snprintf(parLength, 2, "%d", _params[i]->getValueLength());
      pitem.replace("{l}", parLength);
      pitem.replace("{v}", _params[i]->getValue());
      pitem.replace("{c}", _params[i]->getCustomHTML());
    } else {
      pitem = _params[i]->getCustomHTML();
    }

    page += pitem;
  }
  if (_params[0] != NULL) {
    page += "<br/>";
  }

  if (_sta_static_ip) {

    String item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "ip");
    item.replace("{n}", "ip");
    item.replace("{p}", "Static IP");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_ip.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "gw");
    item.replace("{n}", "gw");
    item.replace("{p}", "Static Gateway");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_gw.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "sn");
    item.replace("{n}", "sn");
    item.replace("{p}", "Subnet");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_sn.toString());

    page += item;

    page += "<br/>";
  }

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_SCAN_LINK);

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;

  server->send(200, "text/html", page);


  DEBUG_WM(F("Sent config page"));
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WiFiManager::handleWifiSave() {
  DEBUG_WM(F("WiFi save"));

  //SAVE/connect here
  _ssid = server->arg("s").c_str();
  _pass = server->arg("p").c_str();

  //parameters
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }
    //read parameter
    String value = server->arg(_params[i]->getID()).c_str();
    //store it in array
    value.toCharArray(_params[i]->_value, _params[i]->_length);
    DEBUG_WM(F("Parameter"));
    DEBUG_WM(_params[i]->getID());
    DEBUG_WM(value);
  }

  if (server->arg("ip") != "") {
    DEBUG_WM(F("static ip"));
    DEBUG_WM(server->arg("ip"));
    //_sta_static_ip.fromString(server->arg("ip"));
    String ip = server->arg("ip");
    optionalIPFromString(&_sta_static_ip, ip.c_str());
  }
  if (server->arg("gw") != "") {
    DEBUG_WM(F("static gateway"));
    DEBUG_WM(server->arg("gw"));
    String gw = server->arg("gw");
    optionalIPFromString(&_sta_static_gw, gw.c_str());
  }
  if (server->arg("sn") != "") {
    DEBUG_WM(F("static netmask"));
    DEBUG_WM(server->arg("sn"));
    String sn = server->arg("sn");
    optionalIPFromString(&_sta_static_sn, sn.c_str());
  }

  String lat = getLatitude();
  String lon = getLongitude();
  String vol = getBucketVolume();
  String ttr = getTimeToReset();
  String stationName = getStationName();

  String summary = FPSTR(HTTP_SAVED);
  summary.replace("{sttid}", stationID);
  summary.replace("{name}", stationName);
  summary.replace("{lat}", lat);
  summary.replace("{lon}", lon);
  summary.replace("{vol}", vol);
  summary.replace("{ttr}", ttr);
  summary.replace("{wifi}", _ssid);
  summary.replace("{fmwver}", fmwver);
  summary.replace("{wifimac}", macAddr);

  DEBUG_WM(F("\n\nSUMMARY"));
  DEBUG_WM(F("===========================================\n"));
  DEBUG_WM(F("Station ID:"));
  DEBUG_WM(stationID);
  DEBUG_WM(F("Station Name:"));
  DEBUG_WM(stationName);
  DEBUG_WM(F("Latitude:"));
  DEBUG_WM(lat);
  DEBUG_WM(F("Longitude:"));
  DEBUG_WM(lon);
  DEBUG_WM(F("Bucket Volume:"));
  DEBUG_WM(vol);
  DEBUG_WM(F("WiFi Connected:"));
  DEBUG_WM(_ssid);

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Credenciais Salvas | Pluvi.On");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += summary;  

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent wifi save page"));

  connect = true; //signal ready to connect/reset
}

/** Handle the info page */
void WiFiManager::handleInfo() {
  DEBUG_WM(F("Info"));

  String lat = getLatitude();
  String lon = getLongitude();
  String vol = getBucketVolume();
  String name = getStationName();

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Informações do sistema | Pluvi.On");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h3>Pluvi.On | Info. do sistema</h3>");
  page += F("<dl>");

  // PLUVION - BEGIN
  page += F("<dt>Station Name</dt><dd>");
  page += name;
  page += F("</dd>");
  page += F("<dt>Latitude</dt><dd>");
  page += lat;
  page += F("</dd>");
  page += F("<dt>Longitude</dt><dd>");
  page += lon;
  page += F("</dd>");
  page += F("<dt>Bucket Volume</dt><dd>");
  page += vol;
  page += F("</dd>");
  // PLUVION - END

  page += F("<dt>Chip ID</dt><dd>");
  page += ESP.getChipId();
  page += F("</dd>");
  page += F("<dt>Flash Chip ID</dt><dd>");
  page += ESP.getFlashChipId();
  page += F("</dd>");
  page += F("<dt>IDE Flash Size</dt><dd>");
  page += ESP.getFlashChipSize();
  page += F(" bytes</dd>");
  page += F("<dt>Real Flash Size</dt><dd>");
  page += ESP.getFlashChipRealSize();
  page += F(" bytes</dd>");
  page += F("<dt>Soft AP IP</dt><dd>");
  page += WiFi.softAPIP().toString();
  page += F("</dd>");
  page += F("<dt>Soft AP MAC</dt><dd>");
  page += WiFi.softAPmacAddress();
  page += F("</dd>");
  page += F("<dt>Station MAC</dt><dd>");
  page += WiFi.macAddress();
  page += F("</dd>");
  page += F("</dl>");

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent info page"));
}

/** Handle the reset page */
void WiFiManager::handleReset() {
  DEBUG_WM(F("Reset"));

  String page = FPSTR(HTTP_HEAD);
  page.replace("{t}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("Sistema irá reiniciar em alguns segundos.");

  String footer = FPSTR(HTTP_END);
  footer.replace("{fmwver}", fmwver);
  footer.replace("{sttid}", stationID);
  footer.replace("{wifimac}", macAddr);
  page += footer;
  
  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent reset page"));
  delay(5000);
  ESP.reset();
  delay(2000);
}



//removed as mentioned here https://github.com/tzapu/WiFiManager/issues/114
/*void WiFiManager::handle204() {
  DEBUG_WM(F("204 No Response"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  server->send ( 204, "text/plain", "");
}*/

void WiFiManager::handleNotFound() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the error page.
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += ( server->method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";

  for ( uint8_t i = 0; i < server->args(); i++ ) {
    message += " " + server->argName ( i ) + ": " + server->arg ( i ) + "\n";
  }
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  server->send ( 404, "text/plain", message );
}


/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean WiFiManager::captivePortal() {
  if (!isIp(server->hostHeader()) ) {
    DEBUG_WM(F("Request redirected to captive portal"));
    server->sendHeader("Location", String("http://") + toStringIp(server->client().localIP()), true);
    server->send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//start up config portal callback
void WiFiManager::setAPCallback( void (*func)(WiFiManager* myWiFiManager) ) {
  _apcallback = func;
}

//start up save config callback
void WiFiManager::setSaveConfigCallback( void (*func)(void) ) {
  _savecallback = func;
}

//sets a custom element to add to head, like a new style tag
void WiFiManager::setCustomHeadElement(const char* element) {
  _customHeadElement = element;
}

//if this is true, remove duplicated Access Points - defaut true
void WiFiManager::setRemoveDuplicateAPs(boolean removeDuplicates) {
  _removeDuplicateAPs = removeDuplicates;
}



template <typename Generic>
void WiFiManager::DEBUG_WM(Generic text) {
  if (_debug) {
    Serial.print("*WM: ");
    Serial.println(text);
  }
}

int WiFiManager::getRSSIasQuality(int RSSI) {
  int quality = 0;

  if (RSSI <= -100) {
    quality = 0;
  } else if (RSSI >= -50) {
    quality = 100;
  } else {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

/** Is this an IP? */
boolean WiFiManager::isIp(String str) {
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String WiFiManager::toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}
