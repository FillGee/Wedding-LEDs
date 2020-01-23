//This is the file that should run on the one "master" node-mcu amica. It will be taking in data from the MSGEQ7s and calculating a hue to send out to the other node-mcus
//Because these are ESP8266 they dont directly support multicast, and a lot of the stuff you find online doesnt work because its meant for the esp-32s

extern "C" {
  #include <espnow.h>
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <FastLED.h> //really wont need this in the final product, but its good to see it now for testing purposes

#define WIFI_CHANNEL 1
//static uint8_t slave1MAC[] = { 0xA4, 0xCF, 0x12, 0xD9, 0x0A, 0xEC }; slave 2
//static uint8_t slave2MAC[] = { 0xA4, 0xCF, 0x12, 0xD9, 0x0B, 0x08 }; slave 2
static uint8_t slaveMACs[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //setup the MAC address for the slaves allowing pseudo multicast. 
//the esp8266 is not supposed to have multicast, but setting the slaveMacs as this seems to work just fine and broadcast to multiple at the same time.

byte hue[1]; //a 1 byte array to store hue color. Seems to need to be an array due to how the send function wants a pointer and byte length, not an actual value

//FastLED neopixel ring info
#define NUM_LEDS 16
#define DATA_PIN 2
CRGBArray<NUM_LEDS> leds; //make the CRGBArray and call it "leds" this time

//pins for the MSGEQ7
#define STROBE 4
#define RESET 5
#define AUDIO_DATA A0

int freq;
int audioAmplitudes[7];

void setup() 
{
  //initialize the MSGEQ7 pins. Dont need to initialize the LED pins because FastLED handles that for us
  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);
  digitalWrite(STROBE, HIGH);
  digitalWrite(RESET, HIGH);

  //Get the MSGEQ7 ready for its first reading
  digitalWrite(STROBE, LOW);
  delay(1);
  digitalWrite(RESET, HIGH);
  delay(1);
  digitalWrite(STROBE, HIGH);
  delay(1);
  digitalWrite(STROBE, LOW);
  delay(1);
  digitalWrite(RESET, LOW);
    
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS); //neopixels are just WS2182s, these ones have the GRB color order
  
  WiFi.mode(WIFI_STA); //we need to start off in wifi station mode to initialize MAC addresses and such
  WiFi.begin();
  Serial.println(WiFi.macAddress()); //print out this devices MAC address so we know what it is for the slaves to listen for. Master seems to be A4:CF:12:D9:02:68
  WiFi.disconnect();

  if (esp_now_init() != 0) //we initialize esp now and if it doesnt succeed we print an error
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER); //tell esp now that this device will be sending out messages
  esp_now_add_peer(slaveMACs, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0); //add the "peer" which is really any device with that MAC, set slave (no return data), on wifi channel defined, and no encryption

  FastLED.clear(); //clear out the LEDs because a lot of time initializing wifi stuff sends data accidentally to the LEDs
  FastLED.show();
  
  Serial.println("done");
}

void readAudio(int freq)
{
    //Read amplitudes for each frequency band
    audioAmplitudes[freq] = analogRead(AUDIO_DATA);
    digitalWrite(STROBE, HIGH);
    delayMicroseconds(75);                    // Delay necessary due to timing diagram 
    digitalWrite(STROBE, LOW);
    delayMicroseconds(100);                    // Delay necessary due to timing diagram 
}

byte averageFrequency()
{
  float averageFreq = 0.0;
  float totalLevel = 0.000001;
  for(int i = 0; i < 8; i++)
  {
    if (audioAmplitudes[i] > 40) //only count it if its above base noise level
    {
      averageFreq += ((i+1) * audioAmplitudes[i]); //add up the amplitudes, multiplied by the frequency band
      totalLevel += audioAmplitudes[i]; 
    }
  }
  averageFreq = averageFreq / totalLevel;
  Serial.print("Average Frequency band: ");
  Serial.print(averageFreq);
  byte resultHue = map(averageFreq, 1, 8, 0, 255);
  Serial.print(" Result hue: ");
  Serial.println(resultHue);
  return resultHue;
}
void loop() 
{
  //EVERY_N_MILLISECONDS(100)
  //{
    //hue[0] = random8(); //generate a random number between 0-255
    //Serial.print("Hue gen: ");
    //Serial.print(hue[0]);
    //byte sendResult = esp_now_send(NULL, hue, 1); //send the random hue, which is one byte in size, to all peers
    //Serial.print(". Sent ");
    //Serial.print(*hue);
    //Serial.print(" with result: ");
    //Serial.println(sendResult); //print the results of the send attempt. 0 is success
    //fill_solid(leds, NUM_LEDS, CHSV(hue[0], 255, 20));
    //FastLED.show();
  //}
  EVERY_N_MILLISECONDS(75)
  {
    for (int i = 0; i < 8; i++)
    {
      readAudio(i);
      Serial.print(i);
      Serial.print(": ");
      Serial.print(audioAmplitudes[i]);
      Serial.print(" | ");
    }
    Serial.println();
    hue[0] = averageFrequency();
    byte sendResult = esp_now_send(NULL, hue, 1); //send the random hue, which is one byte in size, to all peers
    Serial.println();
  }  
}
