#include "AppConfig.h"
#include "Log.h"
#include <ArduinoJson.h>

#define USE_LittleFS

#include <FS.h>
#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LITTLEFS.h> 
  #define FORMAT_LITTLEFS_IF_FAILED true
#else
  #include <SPIFFS.h>
#endif

AppConfig::AppConfig() {
  filePath = DEFAULT_CONFIG_FILE_PATH;
  Config myConfig = Config();
  config = &myConfig;
}

AppConfig::AppConfig(Config *config) {
  filePath = DEFAULT_CONFIG_FILE_PATH;
  this->config = config;
}

AppConfig::~AppConfig() {
  ESP_LOGI("APPCONFIG", "AppConfig destructor called");
}

void AppConfig::listFiles() {
  File root = LittleFS.open("/");
  File file_ = root.openNextFile();
  while(file_){
    ESP_LOGI("APPCONFIG", "FILE: ");
    ESP_LOGI("APPCONFIG", file_.name());

    file_ = root.openNextFile();
  }
}

bool AppConfig::loadJsonConfig()
{
  ESP_LOGI("APPCONFIG", "Loading JSON config");

  if(!LittleFS.begin(true)){
    ESP_LOGE("APPCONFIG", "An Error has occurred while mounting LittleFS");
    return false;
  }

  //listFiles();

  ESP_LOGI("APPCONFIG", "Reading file: %s", filePath);
  File file = LittleFS.open(filePath, FILE_READ);
  if(!file){
    ESP_LOGI("APPCONFIG", "There was an error opening the file");
    return false;
  }

  auto err = deserializeJson(json_doc, file);
  if(err) {
    ESP_LOGE("APPCONFIG", "Unable to deserialize JSON to JsonDocument: %s", err.c_str() );
    return false;
  }

  // serializeJson(json_doc, Serial); //print config to serial

  // Copy values from the JsonDocument to the Config
  strlcpy(config->wifiSSID, json_doc["wifi"]["ssid"], sizeof(config->wifiSSID));
  strlcpy(config->wifiPassword, json_doc["wifi"]["password"], sizeof(config->wifiPassword));
  strlcpy(config->mqttClientName, json_doc["mqtt"]["clientName"], sizeof(config->mqttClientName));
  strlcpy(config->mqttServer, json_doc["mqtt"]["server"], sizeof(config->mqttServer));
  config->mqttPort = (int)json_doc["mqtt"]["port"];
  strlcpy(config->mqttUser, json_doc["mqtt"]["user"], sizeof(config->mqttUser));
  strlcpy(config->mqttPassword, json_doc["mqtt"]["password"], sizeof(config->mqttPassword));
  strlcpy(config->espNowGatewayMacAddress, json_doc["espnow"]["gatewayMacAddress"], sizeof(config->espNowGatewayMacAddress));
  
  return true;
}

bool AppConfig::loadConfig() {
  return loadJsonConfig();
}

Config* AppConfig::getConfig() {
  return config;
}