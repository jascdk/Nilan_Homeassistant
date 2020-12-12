/* 
 *  It should not be necessary to change anything else than in this file to suit your needs.
 *  If you need to change other things, consider adding to this file and submit a pull request :)
 */

// Change config to "1" verify that project has been configured
#define CONFIGURED 1

// WIFI settings
#define WIFI_SSID     "XXXXXX"            // Put in your SSID
#define WIFI_PASSWORD "XXXXXX"            // Put in you SSID Password

// MQTT settings
#define MQTT_SERVER   "XXXXXX"            // Put in the IP adresse of your MQTT broker
#define MQTT_PORT     "1883"              // Put in your MQTT broker PORT number - defaults to 1883  
#define MQTT_USERNAME "XXXXXX"            // Username for the MQTT broker (NULL if no username is required)
#define MQTT_PASSWORD "XXXXXX"            // Passowrd for the MQTT broker (NULL if no password is required)

const String   MQTT_CLIENT = "NILAN-AIR-GATEWAY-COMFORT-300-";       // mqtt client_id prefix. Will be suffixed with Esp32 mac to make it unique

char* usersetTopic1 = "ventilation/userset"; 
 
// Comment out below to disable the OTA feature, especially if you have
// stability problems.
//#define ENABLE_ARDUINO_OTA

/* Serial */
#define SERIAL_BAUD_RATE  115200               // Speed for USB serial console

#if CONFIGURED == 0
  #error "Default configuration used - won't upload to avoid loosing connection."
#endif
