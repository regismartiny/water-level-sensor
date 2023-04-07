#include "ESPNow.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "ESPLogMacros.h"

ESPNow::ESPNow() {
}

ESPNow::~ESPNow() {
}

void ESPNow::parseBytes(const char* str, char sep, uint8_t* bytes, int maxBytes, int base) {
  for (int i = 0; i < maxBytes; i++) {
    bytes[i] = strtoul(str, NULL, base);
    str = strchr(str, sep); 
    if (str == NULL || *str == '\0') {
      break;
    }
    str++;
  }
}

void ESPNow::configGatewayMacAddress(const char* gatewayMacAddressString) {
  const int baseHexadecimal = 16;
  const char separator = ':';
  parseBytes(gatewayMacAddressString, separator, gatewayMacAddress, 6, baseHexadecimal);
}

int32_t ESPNow::getWiFiChannel(const char *ssid) {
  ESP_LOGI("ESPNOW", "Searching for WiFi with SSID '%s'", ssid);
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (strcmp(ssid, WiFi.SSID(i).c_str()) == 0) {
        int32_t wifiChannel = WiFi.channel(i);
        ESP_LOGI("ESPNOW", "WiFi channel for SSID '%s' found: %d", ssid, wifiChannel);
        return wifiChannel;
      }
    }
  }
  ESP_LOGE("ESPNOW", "WiFi channel for SSID '%s' not found!", ssid);
  return 0;
}

void ESPNow::configEspNowChannel(int wifiChannel) {
  ESP_LOGI("ESPNOW", "Config ESPNow WiFi channel to %d", wifiChannel);
  esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
  uint8_t* chan;
  wifi_second_chan_t* sChan;
  esp_wifi_get_channel(chan, sChan);
  ESP_LOGI("ESPNOW", "ESPNow WiFi channel set to %d", wifiChannel);
}

void ESPNow::configEspNowChannel(const char *wifiSSID) {
  ESP_LOGI("ESPNOW", "Config ESPNow WiFi channel to channel used by SSID %s", wifiSSID);
  int32_t wifiChannel = getWiFiChannel(wifiSSID);
  esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
  uint8_t* chan;
  wifi_second_chan_t* sChan;
  esp_wifi_get_channel(chan, sChan);
  ESP_LOGI("ESPNOW", "ESPNow WiFi channel set to %d", wifiChannel);
}

void ESPNow::init(const char* gatewayMacAddressString) {
  configGatewayMacAddress(gatewayMacAddressString);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE("ESPNOW", "Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(ESPNow_OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, gatewayMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    ESP_LOGE("ESPNOW", "Failed to add peer");
    return;
  }
}

/**
 * 
*/
void ESPNow::init(const char* gatewayMacAddressString, int wifiChannel) {
  WiFi.mode(WIFI_STA);
  configEspNowChannel(wifiChannel);
  init(gatewayMacAddressString);
}

/**
 * 
*/
void ESPNow::init(const char* gatewayMacAddressString, const char* wifiSSIDToGetChannelFrom) {
  ESP_LOGI("ESPNOW", "Initiating with Gateway: %s, WIFI SSID: %s", gatewayMacAddressString, wifiSSIDToGetChannelFrom);
  WiFi.mode(WIFI_STA);
  configEspNowChannel(wifiSSIDToGetChannelFrom);
  init(gatewayMacAddressString);
}

void ESPNow::send() {
  // Send message via ESP-NOW
  
  esp_err_t result = esp_now_send(gatewayMacAddress, (uint8_t *) &myData, sizeof(myData));
  
  if (result == ESP_OK) {
    ESP_LOGI("ESPNOW", "Sent with success");
  }
  else {
    ESP_LOGE("ESPNOW", "Error sending the data");
  }
}

/**
 * 
*/
void ESPNow::sendMessage(std::string message, msgType messageType) {
  int bufferSize = sizeof(myData.content);
  size_t messageLength = message.length(); //without null termination

  ESP_LOGI("ESPNOW", "Content of message being sent has %d bytes", messageLength);

  int numberOfParts = 1;
  if (messageLength >= bufferSize) {
    div_t division = std::div(messageLength, bufferSize - 1);
    numberOfParts = division.quot + (division.rem > 0 ? 1 : 0);
  }
  ESP_LOGI("ESPNOW", "Number of parts: %d", numberOfParts);

  for(int i=0; i < numberOfParts; i++) {
    const char* substring = message.substr(bufferSize*i).c_str();
    strncpy(myData.content, substring, bufferSize);
    myData.content[bufferSize - 1] = 0; //null termination
    myData.type = messageType;
    myData.page = numberOfParts == 1 ? 0 : i + 1;
    send();
  }
}

void ESPNow::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  bool success = status == ESP_NOW_SEND_SUCCESS;
  ESP_LOGI("ESPNOW", "Last Packet Send Confirmation Status: %s", success ? "Success" : "Failed");
}