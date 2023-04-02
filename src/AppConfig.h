#include <ArduinoJson.h>

#define DEFAULT_CONFIG_FILE_PATH "/config.json"

struct Config {
  char wifiSSID[64];
  char wifiPassword[64];
  char mqttClientName[64];
  char mqttServer[64];
  int mqttPort;
  char mqttUser[64];
  char mqttPassword[64];
  char espNowGatewayMacAddress[18];
};

class AppConfig {
   public:
      AppConfig();
      AppConfig(Config *config);
      ~AppConfig();
      bool loadConfig();
      Config* getConfig();
   private:
      const char* filePath;
      StaticJsonDocument<2048> json_doc;
      Config* config;
      bool loadJsonConfig();
      void listFiles();
};