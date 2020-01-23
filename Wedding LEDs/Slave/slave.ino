extern "C" {
  #include <espnow.h>
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <FastLED.h> //really wont need this in the final product, but its good to see it now for testing purposes

#define WIFI_CHANNEL 1

//uint8_t hue = 0; //a byte for the hue color
byte hue[1];

#define NUM_LEDS 16
#define DATA_PIN 2
CRGBArray<NUM_LEDS> leds;

void setup() 
{
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  
  WiFi.mode(WIFI_STA); //we need to start off in wifi station mode to initialize MAC addresses and such
  WiFi.begin();
  Serial.println(WiFi.macAddress()); //print out this devices MAC address so we know what it is for the slaves to listen for. Slave 1 seems to be A4:CF:12:D9:0A:EC. Slave 2 is A4:CF:12:D9:0B:08

  WiFi.disconnect();

  if (esp_now_init() != 0) //we initialize esp now and if it doesnt succeed we print an error
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO); //tell esp now that this device will be both receiving and sending out messages
  //esp_now_add_peer(slaveMACs, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0); //add the "peer" which is really any device with that MAC, set slave (no return data), on wifi channel defined, and no encryption


  esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) //this is the "function" that receives data and does something with it
  {
  memcpy(&hue, data, 1); //copy the data that was received into our hue array, 1 byte long.
  Serial.print("Message received: ");
  Serial.println(hue[0]);
  moveRight(1);
  leds[0] = CHSV(hue[0], 255, 20);
  //fill_solid(leds, NUM_LEDS, CHSV(hue[0], 255, 20));
  FastLED.show();
  });

  Serial.println("done");
}

void moveRight(int pixels)
{
  for (int j = NUM_LEDS-1; j>=pixels; j--)
  {    
    leds[j] = leds[j-pixels];
  }
}

void loop()
{
  
}
