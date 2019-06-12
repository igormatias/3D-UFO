//******************************************************* LIBRARIES ******************************************************
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

//************************************************************************************************************************
//**************************************************** CAPTIVE PORTAL ****************************************************
// Set these to your desired credentials. */
const char *ssid = "UFO";
const char *password = "closeencounters";
//************************************************************************************************************************
//**************************************************** CAPTIVE PORTAL ****************************************************
//Configurations
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
ESP8266WebServer server(80);

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
//************************************************************************************************************************
//****************************************************    LED STRIP   ****************************************************
//Configurations
FASTLED_USING_NAMESPACE
#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
  #warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN          5
#define LED_TYPE          WS2811
#define COLOR_ORDER       GRB
#define NUM_LEDS          72+12+12
#define NUM_LEDS_TOP      12
#define NUM_LEDS_BOTTOM   12

CRGB leds[NUM_LEDS];

#define BRIGHTNESS         255
#define FRAMES_PER_SECOND  120

int colourRed = 255;
int colourGreen = 255;
int colourBlue = 255;
bool gReverseDirection = false;
//************************************************************************************************************************
//*****************************************************    FOR OTA   *****************************************************
#define SENSORNAME "ufocontroller"
#define OTApassword "closeencounters" // change this to whatever password you want to use when you upload OTA
int OTAport = 8266;

//************************************************************************************************************************
//*************************************************** GLOBAL VARIABLES ***************************************************
bool changePattern = false;
bool changePatternOnline = false;
int btm_brightness = 255;

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

//************************************************************************************************************************
//*****************************************************   WEB PAGE   *****************************************************
String page ="<!DOCTYPE html> <html><head> <style> html{ text-align:center; font-family: Arial, Helvetica, sans-serif; } a, .button_style { background-color: #008CBA; border: none; color: white; width: 80%; height: 30px; line-height: 30px; float: left; margin-left: 10%; margin-bottom: 15px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; border-radius: 26px; -moz-border-radius: 26px; -webkit-border-radius: 26px; border:none; } </style> </head> <body> <h1>UFO</h1> <p><a href=\"nextMode\">Next Mode</a></p> <form action=\"/colourMode\"> <p> <input type=\"text\" name=\"r\" id=\"r\" value=\"255\"> <input type=\"text\" name=\"g\" id=\"g\" value=\"0\"> <input type=\"text\" name=\"b\" id=\"b\" value=\"0\"> </p> <input type=\"submit\" class=\"button_style\" value=\"Set Colour\" /> </form> <p><a href=\"ledOff\">OFF</a></p> </body></html>";


//************************************************************************************************************************
//*******************************************************  SETUP  ********************************************************
void setup() {
  delay(1000);
  wifiMulti.addAP("Staff", "cef@Richmondsouth88");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("Xoxo", "LollaeCharlotte");
  wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");
  
  Serial.begin(115200);
  Serial.println("Connecting ...");
  
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  } 
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer


  /*
  WiFi.mode(WIFI_AP);
  Serial.println();
  Serial.print("Configuring access point...");
  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid, password);

  dnsServer.start(DNS_PORT, "*", apIP);

  
  
  */
  server.on("/", handleRoot);
  server.on("/nextMode", handleNextMode);
  server.on("/colourMode", handleColourMode);
  server.on("/ledOff", handleLedOff);
  server.onNotFound([]() {
    server.send(200, "text/html", page);
  });  
  
  server.begin();
  Serial.println("HTTP server started");
  
  delay(2000); // 3 second delay for recovery

  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  ArduinoOTA.setPort(OTAport);

  ArduinoOTA.setHostname(SENSORNAME);

  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.println("Starting Node named " + String(SENSORNAME));
  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
}
  
// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { ufo, lights_on, juggle, cylon, LEDFire2012, rainbow, confetti, sinelon, bpm, lights_off };

void loop()
{
  ArduinoOTA.handle();
  dnsServer.processNextRequest();
  server.handleClient();
  
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  //EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))


void handleRoot() {
  server.send(200, "text/html",page);
}
void handleNextMode() {
  server.send(200, "text/html",page);
  changePattern = true;
  nextPattern(); // trigger once 
}
void handleLedOff() {
  server.send(200, "text/html",page);
  gCurrentPatternNumber = (ARRAY_SIZE( gPatterns) - 1);
}
void handleColourMode(){
  colourRed = (server.arg("r") != "") ? server.arg("r").toInt() : 0;
  colourGreen = (server.arg("g") != "") ? server.arg("g").toInt() : 0;
  colourBlue = (server.arg("b") != "") ? server.arg("b").toInt() : 0;
  server.send(200, "text/html",page);
  gCurrentPatternNumber = 1;
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void LEDFire2012() 
{
  Fire2012();
}


void lights_off() 
{
  FastLED.clear ();
}

void lights_on() 
{
  for(int dot = 0; dot < NUM_LEDS; dot++) {
      leds[dot].setRGB( colourRed, colourGreen, colourBlue);
  }
  FastLED.show();
  delay(100);
}

void ufo() 
{
  // Top
  for(int dot = 0; dot < NUM_LEDS_TOP; dot++) { 
    leds[dot].setRGB( 255, 51, 0);
  }
  // Bottom
  for(int dot = NUM_LEDS_TOP; dot < (NUM_LEDS_TOP + NUM_LEDS_BOTTOM); dot++) {
    leds[dot].setRGB( 185, 101, 0);
  }
  // Ring
  for(int dot = (NUM_LEDS_TOP + NUM_LEDS_BOTTOM); dot < NUM_LEDS; dot++) { 
    leds[dot].setRGB( 255, 244, 230);
    FastLED.show();
    // clear this led for the next time around the loop
    leds[dot].setRGB( 185, 101, 0);
    delay(5);
  }
}

void fadeall() { 
  for(int i = 0; i < NUM_LEDS; i++) { 
    leds[i].nscale8(250); 
  } 
}
void cylon()
{
  static uint8_t hue = 0;
  // First slide the led in one direction
  for(int i = 0; i < NUM_LEDS; i++) {
    // Set the i'th led to red 
    leds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show(); 
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }

  // Now go in the other direction.  
  for(int i = (NUM_LEDS)-1; i >= 0; i--) {
    // Set the i'th led to red 
    leds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
  
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100 
#define COOLING  50

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120


void Fire2012()
{
// Array of temperature readings at each simulation cell
  const int num_leds = NUM_LEDS_TOP + NUM_LEDS_BOTTOM;
  static byte heat[num_leds];

  // Step 1.  Cool down every cell a little
  for( int i = 0; i < num_leds; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / num_leds) + 2));
  }
  
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= num_leds - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }
  
  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160,255) );
  }
  
  // Step 4.  Map from heat cells to LED colors
  for( int j = 0; j < num_leds; j++) {
    CRGB color = HeatColor( heat[j]);
    int pixelnumber;
    if( gReverseDirection ) {
      pixelnumber = (num_leds - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
  
  //shuffle top
  for (int i = 0; i < NUM_LEDS_TOP; i++){
    int cell = random(NUM_LEDS_TOP);
    CRGB temp = leds[i];
    leds[i] = leds[cell];
    leds[cell] = temp;
  }
  
  //shuffle bottom
  for (int i = NUM_LEDS_TOP; i < num_leds; i++){
    int cell = random(NUM_LEDS_TOP, num_leds);
    CRGB temp = leds[i];
    leds[i] = leds[cell];
    leds[cell] = temp;
  }
}

