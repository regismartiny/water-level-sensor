#include <esp_now.h>
#include <string>

#define MAX_MESSAGE_LENGTH 240

void ESPNow_OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status); // MUST be implemented in your sketch. Called after data is sent.

const uint8_t espNow_broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum msgType : uint {
  SENSOR_INFO = 1,
  LOG = 2,
  COMMAND = 3
};

// Structure example to send data
// Must match the receiver structure
// total struct size has to be max ESP_NOW_MAX_DATA_LEN bytes
typedef struct struct_message {
  char content[MAX_MESSAGE_LENGTH];
  msgType type;
  int page; //0 = single page message, -1 = last page, 1-n = current page of multipage message
} struct_message;

class ESPNow {
    public:
        ESPNow();
        ~ESPNow();
        void init(char* gatewayMacAddressString);
        void sendMessage(std::string message, msgType messageType);
        void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    private:
        uint8_t gatewayMacAddress[6];
        struct_message myData;
        esp_now_peer_info_t peerInfo;
        void parseBytes(const char* str, char sep, uint8_t* bytes, int maxBytes, int base);
        void configGatewayMacAddress(const char* macAddressString);
        void send();
        void sendTestMessages();
};