// For WiFi communication
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

// Temperature Sensor
#include <OneWire.h> 
#include <DallasTemperature.h>

// For display
#include <Adafruit_SSD1306.h>

// multicolour LED control. 
#include <FastLED.h>
//WiFi on/off pin
#define   WiFiPin 14
// Variables for LED display
#define LEDPIN     16
#define NUMOFLEDS    1      // only one LED now                                                                                         
CRGB leds[NUMOFLEDS]; // LED variable

// For sensing powercut. (Currently unused, though defined in Setup function)
#define powerSensor 13 //(D7 pin)Button pin for mains sensing - here actually, instead of a button, 
                  

// Setup For temperature sensing
#define TA_SENSOR_PIN 0 // ESP8266 pin D3 connected to DS18B20 sensor's DQ pin
#define TFridge_SENSOR_PIN 2   // ESP8266 pin D4 connected to 2nd DS18B20 sensor's DQ pin
#define TFreezer_SENSOR_PIN 12   // ESP8266 pin  D6 connected to 3rd DS18B20 sensor's DQ pin

//Battery Voltage
const int analogInPin = A0;
//#define analogInPin A0

// T Sensor variables
OneWire oneWire_TA(TA_SENSOR_PIN);
OneWire oneWire_TFridge(TFridge_SENSOR_PIN);
OneWire oneWire_TFreezer(TFreezer_SENSOR_PIN);
DallasTemperature DS_TA(&oneWire_TA), DS_TFridge(&oneWire_TFridge), DS_TFreezer(&oneWire_TFreezer); 



//---------------------------------------------------------OLED part (Display)--------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//-----------------------------------------------------------------------------------------

// Variable declarations

// output variables
String D1; 
String D2 ;
int P_int;
int P;
int T;
char t;
int T_count = 0;
int Ver= 200005; //Device 2
String Status;
String P_s;
int testcount = 0;
int resend_count = 0;
int reinit_count = 0;
int reterm_count = 0;
bool post_again = 0;
bool do_init;
bool break_flag;
bool post_flag;
int dispflag = 0;      //alternating fridge and freezer temp on display
bool powerlow_flag = 0;

// Others
float tempAmb; // temperature in Celsius
float tempFridge; // temperature in Celsius inside FRIDGE
float tempFreezer; // temperature in Celsius inside FREEZER
float T_orange = 7, T_red = 8, T_cyan = 2, T_blue = 0.5; // ideal temperature range 2 to 7 deg C
float Ta, Tf, TF;  
int sensorValue = 0;
float volt = 0;

const char* ssid = "4GWIFI_09684";
const char* password = "12345678";
//const char* ssid = "swetha";
//const char* password = "swe95123";
//
//const char* ssid = "pc_DUK_mi2n";
//const char* password = "connectifyouCAN";
//
//const char* ssid = "DanS PC";
//const char* password = "123*56789";

//For Google Sheet data logging
String host = "script.google.com";
const int httpsPort = 443;
String GAS_ID = "AKfycbz60f0sha2SK4J0AKJ10zNbqAd5BRnVy3M3J8bmA45G4xsrNHdfwHzh6o3pgd0JJqNI";
// Sample JSON data to send
char jsonData[200];

bool useWiFi = 1;
bool use4G = 0;

unsigned long int wifi_start;
unsigned long int t_lastPOST;
unsigned long int t_lastREPEAT;
unsigned long int t_lastOTAcheck;
int POST_interval = 600000;//-30000; // post interval in ms
int repeat_POST_interval = 120000; //failed message retry posting in 2 mins
int OTA_check_interval = 86400000;

//OTA server
String OTA_url = "https://raw.githubusercontent.com/swetha-p95/esp8266ota/main/dev2_v6.bin";


// ################################ SETUP ################################ // 

void setup()
{
  
 
  D2 = '0';
 
  WiFi.forceSleepWake();
  Status = "S0P00T00";
  
 // initialize the DS18B20 sensor
  DS_TA.begin();    
  DS_TFridge.begin();
  DS_TFreezer.begin();
  
  FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUMOFLEDS); // ---- WS LED part
  
  pinMode(powerSensor, INPUT); //The MAins button is always on HIGH level, when pressed it goes LOW (powercut sensor) 
  pinMode(WiFiPin, OUTPUT); //Pin for turning dongle ON/OFF
//  pinMode(analogInPin, INPUT);
    
  Serial.begin(115200);         //Serial monitor baud rate for data output from Arduino to the serial monitor
  Serial.flush();
  Serial.print( "Set up " );
  //---------------------------------------------------------------------------------------------------
  //-- this is the section for OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
  }
  // Clear the buffer
  display.clearDisplay();
  display.display();
  digitalWrite(WiFiPin,LOW);
  delay(500);
  Serial.println("WiFi Pin LOW");
  
  //-----------------------------------------------------------------------------------------------------
  //-- setting the POST timer
  t_lastPOST = millis();
  t_lastREPEAT = millis();
  t_lastOTAcheck = millis();
}





// ################################ LOOP ################################ // 
 
void loop()
{
   D1 = D2;  
   if ((millis() - t_lastOTAcheck) > OTA_check_interval){
    OTA_check();
   }
   
  Serial.print( "Loop: " );
  //Collecting Data
  displayTemp(); // Collect the temperatures and display in OLED and serial monitor
  setLED(); // Set the LED colour based on the temperature 
  powerlow_flag = 0; 
  getPowerStatus();
  getAlertData();//(P,T); // Check previous and current state and update Status
  
  // Loading the data to a JSON variable 
 
  sprintf( jsonData, "{\"Ta\":%.2f, \"Ts\":%.2f, \"P\":%c, \"D1\":%s, \"D2\":%.2f, \"Ver\":%d }", Ta, Tf, P, Status, TF, Ver  );
  
  
  // Send POST request (Broadcast)
  if ( (millis() - t_lastPOST) > POST_interval ) {
      // Serial printing the data to send
      Serial.print( "Data to send: " );
      Serial.println( jsonData );
     //------------------------------------------------------
      t_lastPOST = millis();   // if sending is successful next data sent after 10 mins
//      t_lastREPEAT = millis(); // if sending fails next attempt should be after 2 mins
      if ((useWiFi) && powerlow_flag == 0) {
        
        POST_WiFi();
      }
       
  }
  
  if ((post_again == 1) && (millis() - t_lastPOST) > repeat_POST_interval ){
      Serial.print( "Data to send: " );
      Serial.println( jsonData );
       //sendGSData("StartingBroadcast");
      t_lastPOST = millis(); // if sending fails next attempt should be after 2 mins
      
    if (useWiFi && powerlow_flag == 0) {
      Serial.println("post again");
      POST_WiFi();
    }
    
  }
  Status[0]='A';
  // This delay will control the interval at which the temparature is read
  delay(10000); 
           
}





//--------------------------------- To get alert data --------------------------------------------
void getAlertData(){//(char pc,char tc) {

  Status[3] = Status[4];
  Status[6] = Status[7];
//  Status[9] = Status[10];
  
  // char pc = char(p);
  // char tc = char(t);
  Status[4] = P;
  Status[7] = T;
//  Status[10] =t;

  if ((Status[3] != Status[4])||(Status[6] != Status[7]) || Ta == -127 || Tf == -127 || TF ==-127){
    Status[1] = '1';
    
    // Serial.println(Status);
  // Loading the data to a JSON variable 
    // String P_s = String(P);
    if (String(P)!=String(P_int)){
      Serial. println(P_int);
      Serial.println(P);
      Serial.println(String(P_int));
      Serial.println("P_warning");
    }
    sprintf( jsonData, "{\"Ta\":%.2f, \"Ts\":%.2f, \"P\":%c, \"D1\":%s, \"D2\":%.2f, \"Ver\":%d }", Ta, Tf, P, Status, TF, Ver  );
    t_lastPOST = millis(); // if sending fails next attempt should be after 2 mins
    Serial.print( "Sending Alert: " );
    Serial.println( jsonData );
//    sendGSData("StartingAlert");
    
   
    if (useWiFi && powerlow_flag == 0) {
      POST_WiFi();
    }
     
    
    Status[1] = '0';
    

  }
}


void getPowerStatus(){
  P_int = digitalRead(powerSensor);
  // Serial.println(P_int);
  sensorValue = analogRead(analogInPin);//  
  Serial.print(sensorValue);
//  float volt = map(sensorValue,0,666,0,4.2);//(1024*2);
  volt = 2*3.3*float(sensorValue)/1023;
//  Serial.println(volt);
  if (volt <3.5){
    P = '3';
    powerlow_flag = 1;
  }
  else if (volt < 3.6){
    P = '2';
    
  }
  else if (P_int ==0){
    P = '0';
  }
  else{
    P = '1';
  }
  P_s = String(P);

}

//--------------------------------------------------------------------------------------------
//---------------------------FUNCTION for OLED DISPLAY ---------------------------------------
//--------------------------------------------------------------------------------------------
void displayTemp() {

  //-----------------------------------------------------SECTION FOR GETTING TEMP FROM 2 DS18B20 sensors-------------------------
  
  DS_TA.requestTemperatures();       // send the command to get temperatures
  tempAmb = DS_TA.getTempCByIndex(0);  // read temperature in °C
  
  DS_TFridge.requestTemperatures();       // send the command to get temperatures
  tempFridge = DS_TFridge.getTempCByIndex(0);  // read temperature in °C
  
  DS_TFreezer.requestTemperatures();       // send the command to get temperatures
  tempFreezer = DS_TFreezer.getTempCByIndex(0);  // read temperature in °C

  
  
  Ta =  tempAmb;
  Tf =  tempFridge;
  TF =  tempFreezer;
  Serial.println(TF);
//  char serialOut[100];
//  sprintf( serialOut, "Ta = %d\tTf = %d\tTF = %d", (int)Ta, (int)Tf, (int)TF);
//  Serial.println(serialOut);

  if ((Ta>50 || Ta<-10 || Tf>50 || Tf<-10 || TF>50 || TF<-10) && (T_count < 10) ){
    T_count = T_count+1;
    delay(500);
    Serial.println("Warning####################");
    displayTemp();
  }
  T_count = 0;  
//  Tf = 0.5; //cyan check
  display.clearDisplay();
  display.display();

  // Setting the text colour
  display.setTextColor(SSD1306_WHITE);

  
 if (dispflag==0){
  dispflag = 1;
  // Ambiant temperature display
  display.setCursor(5,3);
  display.setTextSize(2);
  display.print("Ambient:");
  
  display.setCursor(5,32);
  display.setTextSize(2);
  display.print(Ta, 1);


  // Showing degree celsius 
  display.setTextSize(2);
  display.setCursor(75,32);
  display.print("C");
  display.setTextSize(1);
  display.setCursor(70,32);
  display.print((char)247);
 }
 
  else if (dispflag==1){
  dispflag = 2;
    // Fridge temperature display
  display.setCursor(5,3);
  display.setTextSize(2);
  display.print("Fridge:");
  
  display.setCursor(5,32);
  display.setTextSize(2);
  display.print(Tf, 1);


  // Showing degree celsius 
  display.setTextSize(2);
  display.setCursor(75,32);
  display.print("C");
  display.setTextSize(1);
  display.setCursor(70,32);
  display.print((char)247);
    }       
  else {
  dispflag = 0;
  
  // Freezer temperature display
  display.setCursor(5,3);
  display.setTextSize(2);
  display.print("Freezer:");
  
  display.setCursor(5,32);
  display.setTextSize(2);
  display.print(TF, 1);
  
  
  // Showing degree celsius 
  display.setTextSize(2);
  display.setCursor(75,32);
  display.print("C");
  display.setTextSize(1);
  display.setCursor(70,32);
  display.print((char)247);
  }                                                                                            
  display.display();
  
}


//-------------------------------- POWER LED Control -------------------------------------------------------------

void setLED() {
 if (Tf <= 0.5) {
      leds[0] = CRGB ( 20, 0, 0); // Red
      FastLED.show();
      T = '4';
      delay(30);
  } else if (Tf <= 2) {
      leds[0] = CRGB (30, 15, 0); // Orange
      FastLED.show();
      T = '3';
      delay(30);
  } else if (Tf<= 7) {
      leds[0] = CRGB ( 0, 20, 0); // Green
      FastLED.show();
      T = '0';
      delay(30);
  } else if (Tf <= 8) {
      leds[0] = CRGB ( 30, 15, 0); // Orange
      FastLED.show();
      T = '1';
      delay(30);
  } else if (Tf > 8) {
      leds[0] = CRGB ( 20, 0, 0); // Red
      FastLED.show();
      T = '2';
      delay(30);
  }
  if (TF >= -0.5){
      leds[0] = CRGB ( 20, 0, 0); // Red
      FastLED.show();
      T = '5';
      delay(30);
  }else if (TF >= -1.0){
      if(T!='2'||T!='4'){
      leds[0] = CRGB ( 30, 15, 0); // Orange
      T='6';
      FastLED.show();
      }
      
      delay(30);
  }
  else if (TF < -50.0){
      
      leds[0] = CRGB ( 20, 0, 0); // Red
      T='7';
      FastLED.show();     
      
      delay(30);
  }
  
}




//-------------------------------- SEND DATA TO SERVER Over WiFi--------------------------------------------------

void POST_WiFi() {
 
  //------------------SET UP WIFI IF POSSIBLE-----------------------------------
    //Switch on dongle
    digitalWrite(WiFiPin,HIGH);
    Serial.println("WiFi Pin HIGH");
//    delay(500);
    delay(15000);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi");
    wifi_start = millis();
    while (WiFi.status() != WL_CONNECTED && ((millis() - wifi_start) < 90000)) {
     
      delay(500);
      Serial.print(".");   
    
    }
    Serial.println("##");
    delay(20000);
    
    
  //-----------------------------------------------------------------------------------------------------
  
  
  // Server URL  
  String serverURL = "https://storagemonitoring.cdipd.in/Services?param=update-monitoring-status";

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();  // Disable SSL certificate verification
    Serial.println("Sending POST request through WiFi...");

    // Start HTTP POST request
    http.begin(client, serverURL);

    // Set content type
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Content-Length", String(strlen(jsonData)));


    // Send the actual POST request
     int httpResponseCode = http.POST(jsonData);
     D2 = String(httpResponseCode);
     // If successful response received
     if (httpResponseCode > 0) {
       post_again = 0;
       Serial.print("HTTP Response code: ");
       Serial.println(httpResponseCode);
       // Read the response message
       String response = http.getString();
       Serial.println("Response message: " + response);
     } else {
       post_again = 1;
       Serial.print("Error code: ");
       Serial.println(httpResponseCode);
       Serial.println(http.errorToString(httpResponseCode));       
     }
    client.stop();   
    http.end();
  } else {
    Serial.println("Issue with wifi connectivity");
    D2 = "NoConn";
    post_again = 1;
  }

  // Disconnect WiFi after sending
//  WiFi.disconnect();
  delay(500);
  
  sendGSData(D1,D2);
}



void sendGSData(String data1, String data2) {
//  WiFi.begin(ssid, password); //--> Connect to your WiFi router 
//   
//  while (WiFi.status() != WL_CONNECTED) {
//      delay(500);
//      Serial.print(".");
//    }
//  Serial.println("");
//  Serial.println("WiFi connected");
  
  if (WiFi.status() == WL_CONNECTED){
//    WiFiClient client1;
//   client.setInsecure(); 
   WiFiClientSecure client1;
   client1.setInsecure();
   Serial.println("Sending GS data");
  
  //----------------------------------------Connect to Google host---------------------------------------------------//
  
  if (!client1.connect(host, httpsPort)) {
    Serial.println("connection failed");
    client1.stop();
    WiFi.disconnect();
  //Switching off 
 
    digitalWrite(WiFiPin,LOW);
    delay(500);
    Serial.println("WiFi Pin LOW");
    return;
  }
    
  //----------------------------------------Processing data and sending data------------------------------------------//
// Serial.print(client.connect(host,443));
 String url ="/macros/s/" + GAS_ID + "/exec?logdataprev="+data1+"&logdatacurr="+data2+"&status="+Status+"&vbat="+volt;

 Serial.println(url);
 
 client1.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +         
         "Connection: close\r\n\r\n");
  
  Serial.println("request sent");
  
  client1.stop();
  
  
  
 }
 WiFi.disconnect();
 digitalWrite(WiFiPin,LOW);
 delay(500);
 Serial.println("WiFi Pin LOW");
}

void OTA_check(){
    FastLED.clear(true);

  //Switch on dongle
    digitalWrite(WiFiPin,HIGH);
    Serial.println("WiFi Pin HIGH");
    delay(500);
    
    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi");
    wifi_start = millis();
    while (WiFi.status() != WL_CONNECTED && ((millis() - wifi_start) < 90000)) {
     
      delay(500);
      Serial.print(".");   
    
    }
    Serial.println("##");
    delay(20000);
   if(WiFi.status() == WL_CONNECTED){
   WiFiClientSecure client2;
   client2.setInsecure();
      // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    //    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client2,OTA_url);
    // Or:
    // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED: if (ESPhttpUpdate.getLastError() != -102) {
                                  Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                               } else {
                                  Serial.println("HTTP_UPDATE_NO_UPDATES");
                                  t_lastOTAcheck = millis();
                               }
                               break;

      case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); t_lastOTAcheck = millis(); break;

      case HTTP_UPDATE_OK: 
            Serial.println("HTTP_UPDATE_OK");    
            t_lastOTAcheck = millis();  
            ESP.restart();       
            break;
    }
    client2.stop();
    
   }  
   WiFi.disconnect();
   digitalWrite(WiFiPin,LOW);
   delay(500);
   Serial.println("WiFi Pin LOW"); 
   
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  if (err != -102 ) {
    Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
  }
}
