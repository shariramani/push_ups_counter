// this example will play a track and then 
// every five seconds play another track
//
// https://archive.org/details/SynthesizedPianoNotes
// https://github.com/Makuna/DFMiniMp3/wiki/API-Reference
// it expects the sd card to contain these three mp3 files
// but doesn't care whats in them
//
// sd:/mp3/0001.mp3
// sd:/mp3/0002.mp3
// sd:/mp3/0003.mp3
// ToDo: Add Random function, Add windchime function, Add webpage to select function. explore Blutooth source, Add sensitivity, How many push ups in 2 min.

#undef abs                        //This will remove arduino macro abs() and the standard library function will be used instead.
#include <FS.h>
//#include <WiFi.h>
#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"

#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

//#include "time.h"
#include <TimeLib.h> 
#include <NTPClient.h> //Use Library: https://github.com/taranais/NTPClient This has getFormattedDate function.
#include <EEPROM.h>

#include <SoftwareSerial.h>
#include <DFMiniMp3.h>
//DFplayer VCC 5V
bool isPlaying=false;
const byte pinDfpBusy = D7;

#include "Adafruit_VL53L0X.h"
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
// D1 SCL ---- SCL VL530X
// D2 SDA ---- SDA VL530X
//VCC 3.3V
int tofDistance=0;
int last_tofDistance=0;
uint8_t sensitivity=10; // Play only if TOF sensor measured deviation is equal or more than this value. 
int minRange = 200;
int maxRange = 1000;

uint16_t trackPlaying=0;
String userInput="Piano"; //default instrument to play
uint8_t volume=24;  //max volume is 30
int waitAfterPlay=150; //wait milli seconds after file play
int pianoCount = 25;  //number of mp3/wav files for Piano
int tablaCount = 20;  //number of mp3/wav files for Tabla
int windchimeCount =38; //number of mp3/wav files for windchime

int counter = 0;
bool up_counter = false;
bool down_counter = true;
bool playcounter = false;
bool changeModeEnter=false;              //hand very near to TOF sensor
bool changeModeExit=false;             //move hand away from TOF sensor

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//debug Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//include ESP8266 SDK C functions to get heap
extern "C" {                                          // ESP8266 SDK C functions
#include "user_interface.h"
} 

// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTLN(x) Serial.println(F(x))
#define PRINTD(x) Serial.println(x, DEC)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTLN(x)
#define PRINTD(x)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//WiFi  Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* ssid     = "Suresh";
const char* password = "mvls$1488";
  IPAddress staticIP(192, 168, 1, 154);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dnsIP1(192, 168, 1, 1);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//OTA Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t content_len;

void handleUpdate(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    PRINTS("\n Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ?  U_PART : U_FLASH;
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
#ifdef ESP8266
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
#endif
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      PRINTS("\n Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

boolean webInit() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {request->redirect("/update");});
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});
  server.begin();
#ifdef ESP32
  Update.onProgress(printProgress);
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//NTP Section

//const char* ntpServer = "pool.ntp.org";
//const long  gmtOffset_sec = 19800;
//const int   daylightOffset_sec = 0;

WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800, 60000);
String myTime;
String myDate;



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//DFMiniMp3 Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool free2play = 1;
bool Piano = false;
bool Tabla = false;
bool WindChime = false;
bool Random = false;
bool Counter = true;
bool Distance = false;

// forward declare the notify class, just the name
//
class Mp3Notify; 

// define a handy type using serial and our notify class
//
//typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3; 

// instance a DfMp3 object, 
//
//DfMp3 dfmp3(Serial1);

// Some arduino boards only have one hardware serial port, so a software serial port is needed instead.
// comment out the above definitions and use these
SoftwareSerial secondarySerial(D5, D6); // RX, TX
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(secondarySerial);

// implement a notification class,
// its member methods will get called 
//
class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    if (source & DfMp3_PlaySources_Sd) 
    {
        PRINTS("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) 
    {
        PRINTS("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) 
    {
        PRINTS("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError(DfMp3& mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
   // Serial.println();                   //uncomment these to print comm errors.
   // Serial.print("Com Error ");
   // Serial.println(errorCode);
  }
  static void OnPlayFinished(DfMp3& mp3, DfMp3_PlaySources source, uint16_t track)
  {
    PRINT("\n Play finished for #", track);
   }
  static void OnPlaySourceOnline(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved(DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};



////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() 
{
  
#if  DEBUG
  Serial.begin(115200);
  delay(100);
#endif

//Get Mac as hostname
  char host[16];
  #ifdef ESP8266
    snprintf(host, 12, "ESP%08X", ESP.getChipId());
  #else
    snprintf(host, 16, "ESP%012llX", ESP.getEfuseMac());
  #endif


// Initialize SPIFFS
  if(!SPIFFS.begin()){
    PRINTS("\n An Error has occurred while mounting SPIFFS");
    return;
  }

// Initialize EEPROM
EEPROM.begin(512);

randomSeed(analogRead(A0)); // used to generate random track number to play within a folder

PRINTS("\n initializing...");
pinMode(pinDfpBusy, INPUT);   // init Busy pin from DFPlayer (lo: file is playing / hi: no file playing)  
dfmp3.begin();


// Connect to Wi-Fi
    WIFI_Connect();
    
    if (WiFi.status() == WL_CONNECTED) {
    PRINT("\nIP number assigned is ", WiFi.localIP());

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Webserver code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  PRINTS("\n webpage request");
//set volume
   if (request->hasParam("volume")) {
        volume=request->getParam("volume")->value().toInt();
        dfmp3.setVolume(volume);
        PRINT("\n Volume set to = ", volume);
    }

//set sensitivity
        if (request->hasParam("sensitivity")) {
        sensitivity=request->getParam("sensitivity")->value().toInt();
        PRINT("\n sensitivity set to = ", sensitivity);
    }

//set minRange
        if (request->hasParam("minRange")) {
        minRange=request->getParam("minRange")->value().toInt();
        PRINT("\n minRange set to = ", minRange);
    }
//set maxRange
        if (request->hasParam("maxRange")) {
        maxRange=request->getParam("maxRange")->value().toInt();
        PRINT("\n maxRange set to = ", maxRange);
    }
//set waitAfterPlay
        if (request->hasParam("waitAfterPlay")) {
        waitAfterPlay=request->getParam("waitAfterPlay")->value().toInt();
        PRINT("\n waitAfterPlay set to = ", waitAfterPlay);
    }

        int paramsNr = request->params();
        PRINT("\n paramsNr:", paramsNr);
        if (paramsNr > 2){
              //set Piano
                      if (request->hasParam("Piano")) {
                      Piano = true;
                      PRINT("\n Piano set to = ", request->getParam("Piano")->value());
                        }
                       else {
                        Piano = false;
                       }
              
              //set Tabla
                      if (request->hasParam("Tabla")) {
                      Tabla = true;
                      PRINT("\n Tabla set to = ", request->getParam("Tabla")->value());
                        }
                       else {
                        Tabla = false;
                       }
              
              //set WindChime
                      if (request->hasParam("WindChime")) {
                      WindChime = true;
                      PRINT("\n WindChime set to = ", request->getParam("WindChime")->value());
                        }
                       else {
                        WindChime = false;
                       }
              
              //set Random
                      if (request->hasParam("Random")) {
                      Random = true;
                      PRINT("\n Random set to = ", request->getParam("Random")->value());
                        }
                       else {
                        Random = false;
                       }
              
              //set Counter
                      if (request->hasParam("Counter")) {
                      Counter = true;
                      PRINT("\n Counter set to = ", request->getParam("Counter")->value());
                        }
                       else {
                        Counter = false;
                       }
              
              
              //set Distance
                      if (request->hasParam("Distance")) {
                      Distance = true;
                      PRINT("\n Distance set to = ", request->getParam("Distance")->value());
                        }
                       else {
                        Distance = false;
                       }
              
              //ResetCounter
                      if (request->hasParam("ResetCounter")) {
                      counter = 0;
                      PRINT("\n counter set to = zero ", request->getParam("ResetCounter")->value());
                      number2speech(0); //Play zero
                        }
    }
        
      //collect all parameters
         if (paramsNr > 2){
              userInput="";         //reset user input
              for(int i=0;i<paramsNr;i++){
                  AsyncWebParameter* p = request->getParam(i);
                  PRINT("\n Param name: ", p->name());
                  PRINT("\n Param value: ", p->value());
                  PRINTS("\n-------------------------");
                  userInput=userInput + p->value() + "##";
                 }
                 PRINT("\n userInput: ", userInput);
          }
  
  request->send(SPIFFS, "/index.html", String(), false, processor);

  saveSettingsToEEPROM();
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to set GPIO to HIGH
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
   // digitalWrite(ledPin, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to set GPIO to LOW
  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
   // digitalWrite(ledPin, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
 
  server.begin();
  PRINTS("\nHTTP server started");
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//mdns
  MDNS.begin(host);
  if(webInit()) MDNS.addService("http", "tcp", 80);
  PRINT("\n Ready! Open http://%s.local in your browser\n", host);
  


}
else { PRINTS("\n WiFi not connected    ");}

////////////////////////////


  uint16_t volume = dfmp3.getVolume();
  PRINT("\n volume ", volume);
  dfmp3.setVolume(15);
  
  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  PRINT("\n files count: ", count);
  
  PRINTS("\n starting...");

  if (!lox.begin()) {
    PRINTS("\n Failed to boot VL53L0X");
 //   while(1);
  }

  loadSettingsFromEEPROM();  //
}

void waitMilliseconds(uint16_t msWait)
{
  uint32_t start = millis();
  
  while ((millis() - start) < msWait)
  {
    // if you have loops with delays, its important to 
    // call dfmp3.loop() periodically so it allows for notifications 
    // to be handled without interrupts
    dfmp3.loop(); 
    delay(1);
  }
}

void loop() 
{
 
    if (WiFi.status() == WL_CONNECTED) {
       timeClient.update();  //NTP
    }
    else 
    {
      WIFI_Connect();
    }
  
//printLocalTime();
//timeDisplay();
//dateDisplay();
   
uint8_t track2play=0;
tofDistance=0;
free2play = digitalRead(pinDfpBusy);
PRINT("\n free2play: ",free2play);

uint16_t stt = dfmp3.getStatus(); 
//PRINT("\n stt: " , stt);
 
  //trackPlaying = dfmp3.getCurrentTrack();
  //PRINT("\nCurrent Track: ",trackPlaying);


if (Random) {

    if (WindChime) {
        free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
        PRINT("\n free2play: ",free2play);
      if ( free2play && stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
      track2play= random(1,windchimeCount+1);
      PRINT("\n WindChime track: " , track2play);
      dfmp3.playFolderTrack(3,track2play);  // sd:/03/001.mp3
      waitMilliseconds(waitAfterPlay);
      }
    }
    
    if (Piano) {
      if ( stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
      track2play=random(1,pianoCount+1);
      PRINT("\n Piano track: " , track2play);
      dfmp3.playMp3FolderTrack(track2play);  // sd:/mp3/0001.mp3
      waitMilliseconds(waitAfterPlay);
      }
    }


    if (Tabla) {
      if ( stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
      track2play=random(1,tablaCount+1);
      PRINT("\n Tabla track: " , track2play);
      dfmp3.playFolderTrack(2,track2play);  // sd:/02/001.mp3
      waitMilliseconds(waitAfterPlay);
      }
    }
  }

if (Random && !(Tabla) && !(WindChime) && !(Piano)) {
     free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
     PRINT("\n Random free2play: ",free2play);
     if (free2play) {
        PRINTS("\nRandom Playing");
        dfmp3.playRandomTrackFromAll();  // play a random track from all tracks on the SD card
        waitMilliseconds(waitAfterPlay);
     }
  }



checkTOFrange();
if (tofDistance>0 && tofDistance<=maxRange) {   // check again to remove false values. remove this line to get random notes like windchime.
  delay(1); 
  checkTOFrange();
  PRINT("\n tofDistance:",tofDistance);
  } // check again to remove false values. remove this line to get random notes like windchime.

if (tofDistance>maxRange){
  PRINTS("\n Leaving Loop tofDistance > maxrange");
  return;
  }


//Counter
if (Counter) {
    if (tofDistance>minRange && tofDistance<maxRange ){
      up_counter=true;
      down_counter=false;
      PRINT("\n tofDistance: ",tofDistance);
      PRINT("\n up_counter: ",up_counter);
      PRINT("\n down_counter: ",down_counter);
      }
    else { down_counter=true;PRINT("\n down_counter: ",down_counter);} 
    
    if (up_counter && down_counter){    //one up/down (or in/out))cycle completed
      counter++;
      up_counter=false;
      playcounter=true;
    }

    if (playcounter) {
    number2speech(counter);
    playcounter=false;
    }
   //waitMilliseconds(2000);
}

//Speak Distance
if (Distance) {
  PRINT("\n speakDistance: ",tofDistance);
  number2speech(tofDistance);
}

/*
//Test abs works or not
int test_abs = abs(tofDistance - last_tofDistance);
PRINT("\n tofDistance: " , tofDistance);
PRINT("\n last_tofDistance: " , last_tofDistance);
PRINT("\n test_abs: " , test_abs);
*/

//
if (tofDistance>minRange && tofDistance<=maxRange && abs(tofDistance - last_tofDistance) >= sensitivity && !(Random)){
  
    if (Piano) {
      if ( stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
        track2play=(tofDistance-minRange)*pianoCount/(maxRange-minRange);
        PRINT("\n Piano track: " , track2play);
        dfmp3.playMp3FolderTrack(track2play);  // sd:/mp3/0001.mp3
        waitMilliseconds(waitAfterPlay);
      }
    }


    if (Tabla) {
      if ( stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
        track2play=(tofDistance-minRange)*tablaCount/(maxRange-minRange);
        PRINT("\n Tabla track: " , track2play);
        dfmp3.playFolderTrack(2,track2play);  // sd:/02/001.mp3
        waitMilliseconds(waitAfterPlay);
      }
    }

    if (WindChime) {
        free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
        PRINT("\n free2play: ",free2play);
      if ( free2play && stt != 529 && stt != 513 && stt != 512 ) {   //if no mp3 is playing -> play next file
        track2play=(tofDistance-minRange)*windchimeCount/(maxRange-minRange);
        PRINT("\n WindChime track: " , track2play);
        dfmp3.playFolderTrack(3,track2play);  // sd:/03/001.mp3
        waitMilliseconds(waitAfterPlay);
      }
    }

        last_tofDistance=tofDistance;     //Save value after file played
}




//Mode Change - toggle the playing instrument by placing hand very near to TOF sensor

    if (tofDistance>0 && tofDistance<minRange/2){
      changeModeEnter=true;              //hand very near to TOF sensor
      changeModeExit=false;             //move hand away from TOF sensor
      //PRINT("\n tofDistance: ",tofDistance);
      PRINT("\n changeModeEnter: ",changeModeEnter);
      PRINT("\n changeModeExit: ",changeModeExit);
      }
    else { changeModeExit=true;
    //PRINT("\n changeModeExit: ",changeModeExit);
    } 
    
    if (changeModeEnter && changeModeExit){    //Activate change mode
      PRINTS("\n changing Mode");
      if (Piano) {
        Piano = false; Tabla = true; WindChime = false; Random = false; Counter = false; Distance = false;
        track2play=random(1,tablaCount+1);
        dfmp3.playFolderTrack(2,track2play);  // sd:/02/001.mp3
        waitMilliseconds(waitAfterPlay);
        }

    else if (Tabla) {
        Piano = false; Tabla = false; WindChime = true; Random = false; Counter = false; Distance = false;
        track2play=random(1,windchimeCount+1);
        dfmp3.playFolderTrack(3,track2play);  // sd:/02/001.mp3
        waitMilliseconds(waitAfterPlay);
        }   

      else if (WindChime) {
            Piano = false; Tabla = false; WindChime = false; Random = true; Counter = false; Distance = false;
            dfmp3.playRandomTrackFromAll();  // play a random track from all tracks on the SD card
            waitMilliseconds(waitAfterPlay);
        }   
    
     else if (Random) {
        Piano = false; Tabla = false; WindChime = false; Random = false; Counter = true; Distance = false;
        number2speech(0); //Play zero
        playcounter=false;
        } 

     else if (Counter) {
        Piano = false; Tabla = false; WindChime = false; Random = false; Counter = false; Distance = true;
        }     
  
      else if (Distance) {
        Piano = true; Tabla = false; WindChime = false; Random = false; Counter = false; Distance = false;
        track2play=random(1,pianoCount+1);
        dfmp3.playMp3FolderTrack(track2play);  // sd:/mp3/0001.mp3
        waitMilliseconds(waitAfterPlay);
        }   
    
      changeModeEnter=false;
      changeModeExit=true;
      saveSettingsToEEPROM();
    }

/*

//DFPlayer
if ((digitalRead(pinDfpBusy) == HIGH)&&(tofDistance>200 && tofDistance<=1000)) {      // if no mp3 is playing -> play next file 
    Serial.print("track ");
    Serial.println((tofDistance-175)/25);
    Serial.print("tof: ");
    Serial.println(tofDistance);
    mp3.playMp3FolderTrack((tofDistance-175)/25);  // sd:/mp3/0001.mp3
    isPlaying=true;
    delay(500);
}
*/

    waitMilliseconds(waitAfterPlay);
  
}




void checkTOFrange(){
   VL53L0X_RangingMeasurementData_t measure;
    
  //Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    tofDistance=measure.RangeMilliMeter;      
    PRINT("\n checkTOFrange_Distance (mm): ",tofDistance); 
  } else {
//    PRINTS("\n out of range ");
    tofDistance=0;
  }
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//function to play numbers using dfplayer. It can play from 1 to 9999.
void number2speech(int number) {
  uint8_t thousands = (number/1000)%10;
  PRINT("\nthousands: ",thousands);

  uint8_t hundreds = (number/100)%10;
  PRINT("\nhundreds: ",hundreds);

  uint8_t tens = number - (thousands * 1000) - (hundreds * 100);
  PRINT("\ntens: ",tens);
  
 //Play Thousands
 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playThousands: ",free2play);
 }
  
  if ( thousands > 0 && free2play ) {   //if no mp3 is playing -> play next file
        PRINT("\n Playing Number: " , thousands);
        dfmp3.playFolderTrack(4,thousands);  // sd:/04/001.mp3
        waitMilliseconds(waitAfterPlay);
    }

    
 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playThousands: ",free2play);
 }
  
  if ( thousands > 0 && free2play ) {   //if no mp3 is playing -> play next file
        dfmp3.playFolderTrack(4,101);  // sd:/04/101_thousand.mp3
        waitMilliseconds(waitAfterPlay);
    }

//Play Hundreds
 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playH: ",free2play);
 }
 
  if ( hundreds > 0 && free2play ) {   //if no mp3 is playing -> play next file
        PRINT("\n Playing Number: " , hundreds);
        dfmp3.playFolderTrack(4,hundreds);  // sd:/04/001.mp3
        waitMilliseconds(waitAfterPlay);
    }
    
 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playH: ",free2play);
 }
 
  if ( hundreds > 0 && free2play ) {   //if no mp3 is playing -> play next file
        dfmp3.playFolderTrack(4,100);  // sd:/04/100.mp3
        waitMilliseconds(waitAfterPlay);
    }


//Play Tens (this plays units too)
 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playTens: ",free2play);
 }
 
  if ( tens > 0 && free2play ) {   //if no mp3 is playing -> play next file
        PRINT("\n Playing Number: " , tens);
        dfmp3.playFolderTrack(4,tens);  // sd:/04/001.mp3
        waitMilliseconds(waitAfterPlay);
    } 

 free2play = digitalRead(pinDfpBusy); //check dfplayer is free to play
 while (!free2play){                  //while busy wait
  waitMilliseconds(waitAfterPlay);
  free2play = digitalRead(pinDfpBusy); 
  PRINT("\n free2playTens: ",free2play);
 }
 
} //End number2speech

/////////////////

//////////////////////////////////



///////////////////////////////////////////////
//WiFi Connect Function
//////////////////////////////////////////////
  void WIFI_Connect() {
    WiFi.disconnect();
    PRINTS("\nConnecting WiFi...");
    //WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);
    WiFi.config(staticIP, gateway, subnet, dnsIP1);
    // Wait for connection
    for (uint8_t i = 0; i < 25; i++) {
      if (WiFi.status() != WL_CONNECTED)  {
         time_t prevEvtTime =millis();
         while ((millis() - prevEvtTime) < 500) {
          //do things that doesn't need WiFi here
          yield();        
          }
         PRINTS ( "." );
      }
    }
  } // end of WIFI_Connect

///////////////////////////////////////////////

// Replaces placeholder with LED state value
String processor(const String& var){

  PRINT("\n var: ", var);
  String outString="";
  
  if(var == "PianoState"){
    if (Piano) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n Piano: ", outString);
    return outString;
  }


  if(var == "TablaState"){
    if (Tabla) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n Tabla: ", outString);
    return outString;
  }


  if(var == "WindChimeState"){
    if (WindChime) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n WindChime: ", outString);
    return outString;
  }


  if(var == "RandomState"){
    if (Random) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n Random: ", outString);
    return outString;
  }



  if(var == "CounterState"){
    if (Counter) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n Counter: ", outString);
    return outString;
  }

  
  if(var == "DistanceState"){
    if (Distance) {
      outString = "checked";
    }
    else{
      outString = "!checked";
    }
   PRINT("\n Distance: ", outString);
    return outString;
  }



    if(var == "volume"){
    //outString=itoa(volume);
    char buf[12];
    return itoa(volume,buf,10);
  }


    if(var == "sensitivity"){
    //outString=itoa(volume);
    char buf[12];
    return itoa(sensitivity,buf,10);
  }


    if(var == "minRange"){
    //outString=itoa(volume);
    char buf[12];
    return itoa(minRange,buf,10);
  }


    if(var == "maxRange"){
    //outString=itoa(volume);
    char buf[12];
    return itoa(maxRange,buf,10);
  }


    if(var == "waitAfterPlay"){
    //outString=itoa(volume);
    char buf[12];
    return itoa(waitAfterPlay,buf,10);
  }


   if(var == "ESP_TIME"){
    return timeClient.getFormattedTime();
  }

  
  return String();
}
