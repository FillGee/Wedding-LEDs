extern "C" {
  #include <espnow.h>
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <FastLED.h>

#define WIFI_CHANNEL 1

//uint8_t hue = 0; //a byte for the hue color
//byte hue[1];

#define NUM_LEDS 16
#define DATA_PIN 2
CRGBArray<NUM_LEDS> leds;

uint8_t oldHue;
uint8_t oldBrightness;
uint8_t oldRed;
uint8_t oldGreen;
uint8_t oldBlue;
uint8_t rainbowHue = 0;
uint8_t delayCounter = 0;
uint8_t randomBrightness = random8();
uint8_t usableBrightness = 0;

struct data 
{
  byte displayMode;
  byte hue;
  byte saturation;
  byte brightness;
  byte red;
  byte green;
  byte blue;
} recv_data;

void setup() 
{
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(150);
  
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
    //oldHue = recv_data.hue; //copy the previous hue into oldHue before it is overwritten with the new one
    oldBrightness = recv_data.brightness;
    oldRed = recv_data.red;
    oldGreen = recv_data.green;
    oldBlue = recv_data.blue;
    
    memcpy(&recv_data, data, 7); //copy the data that was received into our recv_data struct, 7 bytes long

    if (recv_data.displayMode == 0 )
    {
      EVERY_N_MILLISECONDS(20)
      {
        fadeToBlackBy(leds, NUM_LEDS, 2); 
        FastLED.show();
      }
      //Serial.print("Message received: ");
      //Serial.print(recv_data.hue);
      //Serial.print(". Old hue: ");
      //Serial.print(oldHue);
      //Serial.print(". Blend: ");
      //Serial.println((recv_data.hue + oldHue) / 2);
      moveRight(1);
      if (recv_data.brightness > 85) //it will sometimes be 15 or lower during random noise when there is no actual music playing
      {
        //FastLED.setBrightness(recv_data.brightness); //raw
        FastLED.setBrightness((recv_data.brightness + oldBrightness ) / 2); //average blend between last pixel and now
        //Serial.println(recv_data.brightness);
        //leds[0] = CRGB(recv_data.red, recv_data.green, recv_data.blue); //raw
        leds[0] = blend(CRGB(recv_data.red, recv_data.green, recv_data.blue), CRGB(oldRed, oldGreen, oldBlue), 128);// half blend
      }
      else
      {
        leds[0] = CRGB(0,0,0);
      }
      FastLED.show();
    }
    else if (recv_data.displayMode == 1)
    {
      rainbowFade();
      //delayMicroseconds(100); 
    }
  });

  Serial.println("done");
}

void moveRight(int pixels)
{
  for (int j = NUM_LEDS-1; j>=pixels; j--)
  {    
    leds[j] = leds[j-pixels];
  }
  //fadeToBlackBy(leds, NUM_LEDS, 4);
}

void rainbowFade()
{
  if (delayCounter > 9)
  {
    if (sin8(randomBrightness) > 200)
    {
      usableBrightness = 200;
    }
    else if (sin8(randomBrightness) < 100)
    {
      usableBrightness = 100;
    }
    else
    {
      usableBrightness = sin8(randomBrightness);
    }
    randomBrightness++;
    delayCounter = 0;
    leds[0] = CHSV(rainbowHue++, 255, usableBrightness);
    moveRight(1);
    FastLED.show();
  }
  else
  {
    delayCounter++;
  }
  Serial.println(delayCounter);
}

void loop()
{
  EVERY_N_MILLISECONDS(20)
  {
    //fadeToBlackBy(leds, NUM_LEDS, 2); 
    //FastLED.show();
  }
}
