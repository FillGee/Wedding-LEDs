//This is the file that should run on the one "master" node-mcu amica. It will be taking in data from the MSGEQ7s and calculating a hue to send out to the other node-mcus
//Because these are ESP8266 they dont directly support multicast, and a lot of the stuff you find online doesnt work because its meant for the esp-32s

//board should be setup as the node-mcu 1.0

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

struct data 
{
  byte displayMode;
  byte hue;
  byte saturation;
  byte brightness;
  byte red;
  byte green;
  byte blue;
} send_data;

uint8_t bs[7]; //define/make a new "bullshit" array that will essentially just house the send_data above for use in broadcast

//FastLED neopixel ring info
#define NUM_LEDS 16
#define DATA_PIN 2
CRGBArray<NUM_LEDS> leds; //make the CRGBArray and call it "leds" this time

//pins for the MSGEQ7
#define STROBE 14 //D5
#define RESET 12 //D6
#define AUDIO_DATA A0

#define BUTTON 5 //D1

int freq;
int audioAmplitudes[7];
uint8_t dMode = 1;

CRGB avgColor;
CRGB oldAvgColor;

void setup() 
{
  //initialize the MSGEQ7 pins. Dont need to initialize the LED pins because FastLED handles that for us
  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);
  digitalWrite(STROBE, HIGH);
  digitalWrite(RESET, HIGH);

  pinMode(BUTTON, INPUT);

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
    if (audioAmplitudes[i] > 100) //only count it if its above base noise level, 50 is background noise
    {
      averageFreq += ((i+1) * audioAmplitudes[i]); //add up the amplitudes, multiplied by the frequency band
      totalLevel += audioAmplitudes[i]; 
    }
  }
  averageFreq = averageFreq / totalLevel;
  Serial.print("Average Frequency band: ");
  Serial.print(averageFreq);
  Serial.print(" Total Loudness: ");
  Serial.print(totalLevel);
  send_data.hue =  map(averageFreq, 1, 7, 0, 255);
  send_data.saturation = 255;
  send_data.brightness = map(totalLevel, 0, 5000, 0, 255);
  Serial.print(" Result hue: ");
  Serial.print(send_data.hue);
  Serial.print(" Brightness: ");
  Serial.println(send_data.brightness);
  
}

void addSingleColorPixel()
{
   //find the frequencies with the highest amplitude and just calculate the color based on that?
  int maximum = 0;
  int secondMax = 0;
  int thirdMax = 0;
  int maxIndex = 0;
  int secondMaxIndex = 0;
  int thirdMaxIndex = 0;
  float totalLevel = 0.000001;

  for (int i=0; i<7; i++)
  {
    if (audioAmplitudes[i] > maximum)
    {
      thirdMax = secondMax;
      thirdMaxIndex = secondMaxIndex;
      //put the old max value in secondMax so we can get the second highest value
      secondMax = maximum;
      secondMaxIndex = maxIndex;
      //store this new max value
      maximum = audioAmplitudes[i];
      maxIndex = i;
    }
    if (audioAmplitudes[i] > 100) //50-80 is basically background noise
    {
    totalLevel += audioAmplitudes[i]; //calculate the total "loudness" of each reading
    }
  }


  oldAvgColor = avgColor; //take the most recent average color and store it here so we can use it to blend
  avgColor = CRGB(0,0,0); //reset the average color to nothing so we can add stuff from scratch

  calculateColor(maxIndex, maximum, 0.7); //the maximum freqeuncy gets to set the color with .70 effectiveness
  calculateColor(secondMaxIndex, secondMax, 0.35); //the second to the max gets to add their color,but only .35 effectiveness
  calculateColor(thirdMaxIndex, thirdMax, 0.25); //lastly the third to max adds theirs with .25 effectiveness

  avgColor = blend(oldAvgColor, avgColor, 128); //we can take the color between the last color and the current one to get more of a smooth gradient between colors instead of jumps from red > green etc
  //moveRight(1);
  //stripLEDs[0] = CRGB(avgColor.red, avgColor.green, avgColor.blue); //sets the first pixel to the "average" color of the song at that moment. Then use move right to move it down the chain
  //fill_solid(stripLEDs,NUM_LEDS, CRGB(avgColor.red, avgColor.green, avgColor.blue)); //sets the whole strip to the "average" color, which will then flash to the next when its calculated.
  //fill_gradient_RGB(stripLEDs, 0, avgColor, NUM_LEDS-1, oldAvgColor); //should make a gradient from the new color on the start to the old color on the end? Doesnt seem to work
  //FastLED.show();
  //delayMicroseconds(75);


  send_data.red = avgColor.red;
  send_data.green = avgColor.green;
  send_data.blue = avgColor.blue;
  //send_data.hue =  avgColor.hue;
  send_data.saturation = 255;
  send_data.brightness = map(totalLevel, 0, 5000, 75, 255);
  Serial.print(" Brightness: ");
  Serial.println(send_data.brightness);
}

void calculateColor(int band, int value, float multiplier)
{
  int topValue = 220;
  int twoThirds = 150;
  int oneThirds = 90;
  float blueShift = 0.70; //literally just turn the blue values to this percent so its not so overpowering
  float greenShift = 1.8; //increase the heck out of greens because theres not enough of them usually
  float redShift = 0.70; //red multiplier, could maybe turn it down a tad but I like it

  switch(band)
  {
    case 0:
      avgColor.red = qadd8(avgColor.red , (map(value, 0, 1024, 0, (topValue * redShift)) * multiplier));
      break;
    case 1:
      avgColor.red = qadd8(avgColor.red , (map(value, 0, 1024, 0, (twoThirds * redShift)) * multiplier));
      avgColor.blue = qadd8(avgColor.blue , (map(value, 0, 1024, 0, (oneThirds * blueShift)) * multiplier));
      break;
    case 2:
      avgColor.red = qadd8(avgColor.red , (map(value, 0, 1024, 0, (oneThirds * redShift)) * multiplier));
      avgColor.blue = qadd8(avgColor.blue , (map(value, 0, 1024, 0, (twoThirds * blueShift)) * multiplier));
      break;
    case 3:
      avgColor.blue = qadd8(avgColor.blue , (map(value, 0, 1024, 0, (topValue * blueShift)) * multiplier));
      avgColor.green = qadd8(avgColor.green , (map(value, 0, 1024, 0, (oneThirds * greenShift)) * multiplier)); //even though this should only be blue, add some green otherwise the green end is very under-utilized
      break;
    case 4:
      avgColor.blue = qadd8(avgColor.blue, (map(value, 0, 1024, 0, (twoThirds * blueShift)) * multiplier));
      avgColor.green = qadd8(avgColor.green , (map(value, 0, 1024, 0, (oneThirds * greenShift)) * multiplier));
      break;
    case 5:
      avgColor.blue = qadd8(avgColor.blue , (map(value, 0, 1024, 0, (oneThirds * blueShift)) * multiplier));
      avgColor.green = qadd8(avgColor.green , (map(value, 0, 1024, 0, (twoThirds * greenShift)) * multiplier));
      break;
    case 6:
      avgColor.green = qadd8(avgColor.green , (map(value, 0, 1024, 0, (topValue * greenShift)) * multiplier));
      break;
  }
}

void loop() 
{
  byte val = digitalRead(BUTTON);
  if (val == HIGH)
  {
    if (dMode == 0)
    {
      dMode = 1;
    }
    else
    {
      dMode = 0;
    }
  }
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
  EVERY_N_MILLISECONDS(15) //10 is pretty flashy, might want to be a bit slower for less seizures?
  {
    for (int i = 0; i < 7; i++)
    {
      readAudio(i);
      Serial.print(i);
      Serial.print(": ");
      Serial.print(audioAmplitudes[i]);
      Serial.print(" | ");
    }
    Serial.println();
    //averageFrequency();
    addSingleColorPixel();

    send_data.displayMode = dMode; //display mode 0 is the music reactive
    //send_data.displayMode = 1; //display mode 1 should be rainbow fade
  
    memcpy(bs, &send_data, 7); //copy the send data into a bullshit array in order to be able to send successfully?
    byte sendResult = esp_now_send(NULL, bs, 7); //send the send_data struct variable, which is 7 bytes in size, to all peers
    Serial.println();
  }  
} 
