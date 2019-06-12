//******************************************************* LIBRARIES ******************************************************
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <FS.h>   // Include the SPIFFS library


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

#define BRIGHTNESS         92
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
#define INDEX     "/index.html"
bool changePattern = false;
bool changePatternOnline = false;
int btm_brightness = 255;

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

File fsUploadFile;              // a File object to temporarily store the received file
ESP8266WebServer server(80);
ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'


//************************************************************************************************************************
//*****************************************************   WEB PAGE   *****************************************************
String uploadPage ="<html><body><form method=\"post\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form></body></html>";


//************************************************************************************************************************
//****************************************************  PROTOTYPES  ******************************************************
//Function Prototypes
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload(void);            // Save the Uploaded file into the FS memory

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


  if (MDNS.begin("esp8266")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  SPIFFS.begin();                           // Start the SPI Flash Files System
  
  
  /*
  WiFi.mode(WIFI_AP);
  Serial.println();
  Serial.print("Configuring access point...");
  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid, password);

  dnsServer.start(DNS_PORT, "*", apIP);

  
  
  */
  //server.on("/", handleRoot);
  
  server.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    server.send(200, "text/html",uploadPage);
  });

  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );
  
  server.on("/nextMode", handleNextMode);
  server.on("/colourMode", handleColourMode);
  server.on("/ledOff", handleLedOff);
  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
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


//************************************************************************************************************
//************************************************************************************************************
//Main Loop
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

//************************************************************************************************************************
//********************************************  handleFileUpload FUNCTION  ***********************************************
//Handle the file read from the server

void handleFileUploadPage() {
  server.send(200, "text/html",uploadPage);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void handleNextMode() {
  handleFileRead(INDEX);
  changePattern = true;
  nextPattern(); // trigger once 
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void handleLedOff() {
  FastLED.clear();
  handleFileRead(INDEX);
  gCurrentPatternNumber = (ARRAY_SIZE( gPatterns) - 1);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void handleColourMode(){
  colourRed = (server.arg("r") != "") ? server.arg("r").toInt() : 0;
  colourGreen = (server.arg("g") != "") ? server.arg("g").toInt() : 0;
  colourBlue = (server.arg("b") != "") ? server.arg("b").toInt() : 0;
  handleFileRead(INDEX);
//  server.send(200, "text/html",mainPage);
  gCurrentPatternNumber = 1;
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void LEDFire2012() 
{
  Fire2012();
}

//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void lights_off() 
{
  FastLED.clear();
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void lights_on() 
{
  for(int dot = 0; dot < NUM_LEDS; dot++) {
      leds[dot].setRGB( colourRed, colourGreen, colourBlue);
  }
  FastLED.show();
  delay(100);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

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
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void fadeall() { 
  for(int i = 0; i < NUM_LEDS; i++) { 
    leds[i].nscale8(250); 
  } 
}

//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

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
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

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
//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}


//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server
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


//************************************************************************************************************************
//*********************************************  getContentType FUNCTION  ************************************************
//return the content type to the server
String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".svg")) return "image/svg+xml";  
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

//************************************************************************************************************************
//*********************************************  handleFileRead FUNCTION  ************************************************
//Handle the file read from the server
bool handleFileRead(String path) { // send the right file to the client (if it exists)
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  #ifdef DEBUG
  Serial.println("handleFileRead: " + path);
  #endif
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  #ifdef DEBUG
  Serial.println("\tFile Not Found");
  #endif
  return false;                                         // If the file doesn't exist, return false
}

//************************************************************************************************************
//**************************************  handleFileUpload FUNCTION  *****************************************
//save the new file in the file memory
void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.send(303, "text/plain", "File upload Successful"); // otherwise, respond with a 404 (Not Found) error
      //server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      //server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

