/* 
 *  It should not be necessary to change anything else than in this file to suit your needs.
 *  If you need to change other things, consider adding to this file and submit a pull request :)
 *  Remember that all password, SSID´s and so on are CASE SENSITIVE !
 */

// Change config to verify that project has been configured
#define CONFIGURED 0 // Change this to "1" when you completed your config and begin upload

// WIFI settings
#define WIFI_SSID     "XX" // Put in your SSID 
#define WIFI_PASSWORD "XX" // Put in your SSID Password 

// LED settings
#define WIFI_LED LED_BUILTIN // Blue led on NodeMCU
#define USE_WIFI_LED true // if 'true', the blue led in a NodeMCU will blink during connection,
                           // and glow solid once connected
// MQTT settings
#define MQTT_SERVER   "XX" // Put in the IP addresses of your MQTT broker
#define MQTT_USERNAME NULL // Username for the MQTT broker (NULL if no username is required)
#define MQTT_PASSWORD NULL // Password for the MQTT broker (NULL if no password is required)
#define MQTT_SEND_INTERVAL 600000 // normally set to 180000 milliseconds = 3 minutes. Define as you like

// Serial port
#define SERIAL_CHOICE SERIAL_HARDWARE // SERIAL_SOFTWARE or SERIAL_HARDWARE
#define SERIAL_SOFTWARE_RX D2 // only needed if SERIAL is SERIAL_SOFTWARE
#define SERIAL_SOFTWARE_TX D1 // only needed if SERIAL is SERIAL_SOFTWARE

// Modbus address of the unit (possible to be changed via config)
#define MODBUS_SLAVE_ADDRESS 30 // Default is 30

#if CONFIGURED == 0
  #error "Default configuration used - won't upload to avoid loosing connection."
#endif
