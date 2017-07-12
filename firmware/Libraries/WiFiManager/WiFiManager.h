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

#ifndef WiFiManager_h
#define WiFiManager_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <memory>

extern "C" {
  #include "user_interface.h"
}

const char HTTP_HEAD[] PROGMEM                = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{t}</title>";
const char HTTP_STYLE[] PROGMEM               = "<style>.c{text-align:center}.q{float:right;width:64px;text-align:right}div,input{padding:5px;font-size:1em}input{width:95%}body{text-align:center;font-family:verdana; background-color: #E9E9E9;}button{border:0;border-radius:.3rem;background-color:#34383F;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;margin:3px 0 3px 0}p{text-align:left}</style>";
const char HTTP_SCRIPT[] PROGMEM              = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_END[] PROGMEM            = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char HTTP_PORTAL_OPTIONS[] PROGMEM      = "<h3>Pluvi.On | Config. Salvas!</h3><p><b>Station Name:</b> {name}</p><p><b>Latitude:</b> {lat}</p><p><b>Longitude:</b> {lon}</p><p><b>Volume do Bucket:</b> {vol}</p><p><b>Time to Reset: </b><span id='ttr'></span></p><form action=\"/wifi\" method=\"get\"><button>Configurar WiFi</button></form><form action=\"/0wifi\" method=\"get\"><button>Conf. WiFi (Manualmente)</button></form><form action=\"/i\" method=\"get\"><button>Info. do sistema</button></form><form action=\"/r\" method=\"post\"><button>Reiniciar WiFi</button></form><script>var ms={ttr};x=ms/1000;var ss=x%60;x/=60;var mm=x%60;x/=60;var hh=x%24;x/=24;document.getElementById('ttr').innerHTML=parseInt(hh)+'h '+parseInt(mm)+'m '+parseInt(ss)+'s ';</script>";
const char HTTP_ITEM[] PROGMEM                = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char HTTP_FORM_START[] PROGMEM          = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='SSID'><br/><input id='p' name='p' length=64 type='password' placeholder='password'><br/>";
const char HTTP_FORM_PARAM[] PROGMEM          = "<br/><PIDinput id='{i}' name='{n}' length={l} placeholder='{p}' value='{v}' {c}>";
const char HTTP_FORM_END[] PROGMEM            = "<br/><button type='submit'>Salvar</button></form>";
const char HTTP_SCAN_LINK[] PROGMEM           = "<br/><div class=\"c\"><a href=\"/wifi\">Scan</a></div>";
const char HTTP_FORM_CONFIG[] PROGMEM         = "<form name='frmConfig' method='post' action='/savePluvionConfig' onsubmit='return validate();'><h3>Pluvi.On | Configuração</h3><p id='msg' style='color: red'></p><p>Station Name</p><input type='text' name='name' value='{name}' maxlength='15'/><p>Latitude</p><input type='text' name='lat' /><p>Longitute</p><input type='text' name='lon' /><p>Calibragem do Bucket</p><input type='text' name='vol' value='{vol}' /><br><input type='hidden' name='ttr' /><button type='submit'>Salvar</button></form>";
const char HTTP_SCRIPT_FORM_CONFIG[] PROGMEM  = "<script type='text/javascript'>var msg=document.getElementById('msg');function validate(){msg.innerHTML='';var result=true;var f=document.forms['frmConfig'];var vol=parseFloat(f['vol'].value);var lat=parseFloat(f['lat'].value);var lon=parseFloat(f['lon'].value);var ttr=f['ttr'];var name=f['name'].value;if (!lat || isNaN(lat)) {msg.innerHTML += '- Latitude é obrigatória<br>';result=false;} if (!lon || isNaN(lon)) {msg.innerHTML += '- Longitude é obrigatória<br>';result=false;}if (!vol || isNaN(vol) || (vol < 2.00 || vol > 5.00)) {msg.innerHTML += '- O valor da calibragem deve estar entre 2.00 e 5.00 (ex.: 2.47)<br>';result=false;}if(!name || !(/^([a-z0-9\-\_]+)$/ig.test(name))){msg.innerHTML += ' - Nome Inválido. Deve conter apenas (aA-zZ), (0-9), (-) ou (_)';result=false;} var n=new Date();var nR=new Date();nR.setHours({RESET_TIME},0);if(n>nR){nR.setDate(nR.getDate()+1);}var nRm=Date.parse(nR.toUTCString()); ttr.value=nRm-n; return result;}</script>";
const char HTTP_SAVED[] PROGMEM               = "<h1>Pluvi.On | Sucesso!</h1><h6>Tudo pronto, seu Pluvi.On foi configurado com sucesso!</h6><p><b>Station ID:</b> {id}</p><p><b>Station Name:</b> {name}</p><p><b>Latitude:</b> {lat}</p><p><b>Longitude:</b> {lon}</p><p><b>Volume do Bucket:</b> {vol}</p><p><b>Time to Reset: </b><span id='ttr'></span></p><p><b>Rede Conectada:</b> {wifi}</p><p><b>Mac Address:</b> {wifimac}</p><b>Firmware Version:</b> {fmwver}</p><p>É isso! Qualquer coisa só chamar :)</p><p>E-mail:<a href='mailto:community@pluvion.com.br'>community@pluvion.com.br</a></p><p>Site: <a href='http://www.pluvion.com.br'>pluvion.com.br</a></p><script>var ms={ttr};x=ms/1000;var ss=x%60;x/=60;var mm=x%60;x/=60;var hh=x%24;x/=24;document.getElementById('ttr').innerHTML=parseInt(hh)+'h '+parseInt(mm)+'m '+parseInt(ss)+'s ';</script>";
const char HTTP_END[] PROGMEM                 = "</div><br/><br/><i style='text-align:right; font-size:.6em; display:block'>Pluvi.On &copy; 2017. All Rights Reserved.</i></body></html>";

#define WIFI_MANAGER_MAX_PARAMS 10

class WiFiManagerParameter {
  public:
    WiFiManagerParameter(const char *custom);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    const char *getID();
    const char *getValue();
    const char *getPlaceholder();
    int         getValueLength();
    const char *getCustomHTML();
  private:
    const char *_id;
    const char *_placeholder;
    char       *_value;
    int         _length;
    const char *_customHTML;

    void init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    friend class WiFiManager;
};


class WiFiManager
{
  public:
    WiFiManager();

    boolean       autoConnect();
    boolean       autoConnect(char const *apName, char const *apPassword = NULL);

    //if you want to always start the config portal, without trying to connect first
    boolean       startConfigPortal(char const *apName, char const *apPassword = NULL);

    // get the AP name of the config portal, so it can be used in the callback
    String        getConfigPortalSSID();

    void          resetSettings();

    //sets timeout before webserver loop ends and exits even if there has been no setup.
    //usefully for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout
    void          setConfigPortalTimeout(unsigned long seconds);
    void          setTimeout(unsigned long seconds);

    //sets timeout for which to attempt connecting, usefull if you get a lot of failed connects
    void          setConnectTimeout(unsigned long seconds);


    void          setDebugOutput(boolean debug);
    //defaults to not showing anything under 8% signal quality if called
    void          setMinimumSignalQuality(int quality = 8);
    //sets a custom ip /gateway /subnet configuration
    void          setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //sets config for a static IP
    void          setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //called when AP mode and config portal is started
    void          setAPCallback( void (*func)(WiFiManager*) );
    //called when settings have been changed and connection was successful
    void          setSaveConfigCallback( void (*func)(void) );
    //adds a custom parameter
    void          addParameter(WiFiManagerParameter *p);
    //if this is set, it will exit after config, even if connection is unsucessful.
    void          setBreakAfterConfig(boolean shouldBreak);
    //if this is set, try WPS setup when starting (this will delay config portal for up to 2 mins)
    //TODO
    //if this is set, customise style
    void          setCustomHeadElement(const char* element);
    //if this is true, remove duplicated Access Points - defaut true
    void          setRemoveDuplicateAPs(boolean removeDuplicates);

  private:
    std::unique_ptr<DNSServer>        dnsServer;
    std::unique_ptr<ESP8266WebServer> server;

    //const int     WM_DONE                 = 0;
    //const int     WM_WAIT                 = 10;

    //const String  HTTP_HEAD = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>{t}</title>";

    void          setupConfigPortal();
    void          startWPS();

    const char*   _apName                 = "no-net";
    const char*   _apPassword             = NULL;
    String        _ssid                   = "";
    String        _pass                   = "";
    unsigned long _configPortalTimeout    = 0;
    unsigned long _connectTimeout         = 0;
    unsigned long _configPortalStart      = 0;

    IPAddress     _ap_static_ip;
    IPAddress     _ap_static_gw;
    IPAddress     _ap_static_sn;
    IPAddress     _sta_static_ip;
    IPAddress     _sta_static_gw;
    IPAddress     _sta_static_sn;

    int           _paramsCount            = 0;
    int           _minimumQuality         = -1;
    boolean       _removeDuplicateAPs     = true;
    boolean       _shouldBreakAfterConfig = false;
    boolean       _tryWPS                 = false;

    const char*   _customHeadElement      = "";

    //String        getEEPROMString(int start, int len);
    //void          setEEPROMString(int start, int len, String string);

    int           status = WL_IDLE_STATUS;
    int           connectWifi(String ssid, String pass);
    uint8_t       waitForConnectResult();

    void          handleRoot();
    void          handleWifi(boolean scan);
    void          handleWifiSave();
    void          handleInfo();
    void          handleReset();
    void          handleNotFound();
    void          handle204();
    boolean       captivePortal();

    // Pluvi.On Functions - Begin

    void          handleSavePluviOnConfig();
    
    void          saveCoordinates();
    void          saveBucketVolume();
    void          saveTimeToReset();
    void          saveStationName();

    void          deleteCoordinates();
    void          deleteBucketVolume();
    void          deleteTimeToReset();
    void          deleteStationName();

    String        getLatitude();
    String        getLongitude();
    String        getBucketVolume();
    String        getTimeToReset();
    String        getStationName();
    String        getFirmwareVersion();

    // Pluvi.On Functions - End

    // DNS server
    const byte    DNS_PORT = 53;

    //helpers
    int           getRSSIasQuality(int RSSI);
    boolean       isIp(String str);
    String        toStringIp(IPAddress ip);

    boolean       connect;
    boolean       _debug = true;

    void (*_apcallback)(WiFiManager*) = NULL;
    void (*_savecallback)(void) = NULL;

    WiFiManagerParameter* _params[WIFI_MANAGER_MAX_PARAMS];

    template <typename Generic>
    void          DEBUG_WM(Generic text);

    template <class T>
    auto optionalIPFromString(T *obj, const char *s) -> decltype(  obj->fromString(s)  ) {
      return  obj->fromString(s);
    }
    auto optionalIPFromString(...) -> bool {
      DEBUG_WM("NO fromString METHOD ON IPAddress, you need ESP8266 core 2.1.0 or newer for Custom IP configuration to work.");
      return false;
    }
};

#endif
