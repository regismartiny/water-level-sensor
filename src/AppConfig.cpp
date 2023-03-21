#include "AppConfig.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
}

void AppConfig::listFiles() {
  File root = SPIFFS.open("/");
  File file_ = root.openNextFile();
  while(file_){
    Serial.print("FILE: ");
    Serial.println(file_.name());

    file_ = root.openNextFile();
  }
}

bool AppConfig::loadJsonConfig()
{
  Serial.println("Loading JSON config");

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  //listFiles();

  Serial.printf("Reading file: %s\n", filePath);
  File file = SPIFFS.open(filePath, FILE_READ);
  if(!file){
    Serial.println("There was an error opening the file");
    return false;
  }

  auto err = deserializeJson(json_doc, file);
  SPIFFS.end();
  if(err) {
    Serial.printf("Unable to deserialize JSON to JsonDocument: %s\n", err.c_str() );
    return false;
  }

  serializeJson(json_doc, Serial); //print config to serial

  // Copy values from the JsonDocument to the Config
  strlcpy(config->wifiSSID, json_doc["wifi"]["ssid"], sizeof(config->wifiSSID));
  strlcpy(config->wifiPassword, json_doc["wifi"]["password"], sizeof(config->wifiPassword));
  strlcpy(config->mqttClientName, json_doc["mqtt"]["clientName"], sizeof(config->mqttClientName));
  strlcpy(config->mqttServer, json_doc["mqtt"]["server"], sizeof(config->mqttServer));
  config->mqttPort = (int)json_doc["mqtt"]["port"];
  strlcpy(config->mqttUser, json_doc["mqtt"]["user"], sizeof(config->mqttUser));
  strlcpy(config->mqttPassword, json_doc["mqtt"]["password"], sizeof(config->mqttPassword));
  
  return true;
}

bool AppConfig::loadConfig() {
  return loadJsonConfig();
}

Config* AppConfig::getConfig() {
  return config;
}