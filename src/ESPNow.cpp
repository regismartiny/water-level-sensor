#include "ESPNow.h"
#include <WiFi.h>
#include "Log.h"

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

void ESPNow::init(char* gatewayMacAddressString) {
  configGatewayMacAddress(gatewayMacAddressString);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    LOGE("Error initializing ESP-NOW");
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
    LOGE("Failed to add peer");
    return;
  }
}

void ESPNow::send() {
  // Send message via ESP-NOW
  
  esp_err_t result = esp_now_send(gatewayMacAddress, (uint8_t *) &myData, sizeof(myData));
  
  if (result == ESP_OK) {
    LOGI("Sent with success");
  }
  else {
    LOGE("Error sending the data");
  }
}

void ESPNow::sendMessage(std::string message, msgType messageType) {
  int bufferSize = sizeof(myData.content);
  size_t messageLength = message.length(); //without null termination

  Serial.printf("Content of message being sent has %d bytes.\n", messageLength);

  int numberOfParts = 1;
  if (messageLength >= bufferSize) {
    div_t division = std::div(messageLength, bufferSize - 1);
    numberOfParts = division.quot + (division.rem > 0 ? 1 : 0);
  }
  Serial.printf("Number of parts: %d\n", numberOfParts);

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
  LOGI("Last Packet Send Confirmation Status: %s", success ? "Success" : "Failed");
}

void ESPNow::sendTestMessages() {

  // Set values to send
  char waterLevelBuff[100];
  snprintf(waterLevelBuff, 100, "{\"idx\": %d, \"nvalue\": %d}", 1, 1);
  sendMessage(std::string(waterLevelBuff), SENSOR_INFO);

  delay(5000);

  char logBuff[500];
  snprintf(logBuff, 500, "log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...log info...\n");
  sendMessage(std::string(logBuff), LOG);

  delay(5000);
}