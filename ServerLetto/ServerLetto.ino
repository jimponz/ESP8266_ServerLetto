//prova
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#ifdef ESP32
#include "SPIFFS.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <FS.h>
#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>

#endif
#include <ESPAsyncWebServer.h>

#define relayLuceLetto 5  //D1

#define THRESHOLD_LUMINOSITY 700
#define JSON_SIZE 300
#define DEBUG true

#define UTC_OFFSET_IN_SECONDS 7200

#define IP_SERVER_PRINCIPALE "192.168.1.251"
#define IP_SERVER_SCRIVANIA "192.168.1.170"
#define IP_SERVER_NAS "192.168.1.150"

// NETWORK CREDENTIALS
IPAddress local_IP(192, 168, 1, 180); 
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0);   
IPAddress dns(192, 168, 1, 1);        

IPAddress primaryDNS(8, 8, 8, 8);  
IPAddress secondaryDNS(8, 8, 4, 4); 

const char* ssid = ""; // Insert Wi-Fi SSID (The wifi's name)
const char* password = ""; // Insert Wi-Fi password


// SERVER LOGIN CREDENTIALS
const char* http_username = ""; // Insert Username for WebServer Login
const char* http_password = ""; // Insert password for WebServer Login


String daysOfTheWeek[] = {"Domenica", "Luned&igrave;", "Marted&igrave;",
                          "Mercoled&igrave;", "Gioved&igrave;", "Venerd&igrave;", "Sabato"};
                         
String months[12] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};

WiFiUDP ntpUDP;

const long utcOffsetInSeconds = UTC_OFFSET_IN_SECONDS;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

WiFiClient client;
HTTPClient http;

String IPPub = "";
//String IPLocal = "";
String MAC = "";
String oraServerOnline = ""; 
String ultMovLetto = "";

String IPServerPrincipale = IP_SERVER_PRINCIPALE;
String IPServerScrivania = IP_SERVER_SCRIVANIA;
String IPServerNas = IP_SERVER_NAS;

int threshold_luminosity = THRESHOLD_LUMINOSITY;
int luminosita = 0;

unsigned long timestampLuceLetto = 0;
int timerLuceLetto = 3;

int intervalCheckLuminosita = 0;
unsigned long previousCheckLuminosita = 0;

long duration = 0;
int distanza = 0;
int distanzaImpostata = 85;
int timer = 0;
int timestamp = 0;

//########## SERVER STATE ########

bool controlloAutomatico = true;
bool controlloAutomaticoLuminosita = true;
bool controlloSincronoConScrivania = true;

bool stateLuceLetto = false;

//---------- end of SERVER STATE ----------

bool stateLuceScrivania = false;

// ##################### ESP GENERAL PURPOSES FUNCTIONS #####################

String makeHttpRequest(String url ){
  String response = "";
  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode == 200) {
    response = http.getString();
    Serial.println("Response for HTTP request (" + url + ")" + response);
  }
  else {
    Serial.println("Error on HTTP request (" + url + ")"  + String(httpCode) );
  }
  http.end();
  return response;
}

//String convertFromMillisToDate(unsigned long mil){
//  struct tm *ptm = gmtime ((time_t *)&mil); 
//  
//  String ret = daysOfTheWeek[timeClient.getDay()];
//  ret+= ", ";  
//  ret+= String(timeClient.getHours());
//  ret+= ":";
//  ret+= String(timeClient.getMinutes());
//  ret+= ":";
//  ret+= String(timeClient.getSeconds());
//  return ret;
//}

String getGiornoAndOra(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  
  String ret = daysOfTheWeek[timeClient.getDay()];
  ret+= ", ";  
  ret+= String(timeClient.getHours());
  ret+= ":";
  ret+= String(timeClient.getMinutes());
  ret+= ":";
  ret+= String(timeClient.getSeconds());
  return ret;
}

void getIpPubFromInternet(){
  String url = "http://api.ipify.org";
  
  IPPub = makeHttpRequest(url);
  while (IPPub == "") {
      IPPub = makeHttpRequest(url); 
    }
}

// -------------------------- end of ESP GENERAL PURPOSES FUNCTIONS --------------------------

void getLuminosita(){

  String url = "http://" + IPServerPrincipale+ "/getLuminosita";

  String response = makeHttpRequest(url);
  
  if (response != "") luminosita = response.toInt();
}

void addJsonObject( DynamicJsonDocument jsonDocument, String tag, float value) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj[tag] = value;
}

String getJsonServerLetto(){
  String buffer1 = "";
  DynamicJsonDocument jsonDocument(JSON_SIZE);
  
  jsonDocument["ORARIO"] = getGiornoAndOra();
  //jsonDocument["MEMORIA_DISPONIBILE"] = memoria;
  jsonDocument["ORA_SERVER_ONLINE"] = oraServerOnline;
  jsonDocument["LUMINOSITA"] = luminosita;  
  jsonDocument["STATE_LUCE_LETTO"] = stateLuceLetto;
  jsonDocument["CONTROLLO_AUTOMATICO"] = controlloAutomatico;
  jsonDocument["CONTROLLO_AUTOMATICO_LUMINOSITA"] = controlloAutomaticoLuminosita;
  jsonDocument["CONTROLLO_SINCRONO_CON_SCRIVANIA"] = controlloSincronoConScrivania;
  jsonDocument["TIMER_LUCE_LETTO"] = timerLuceLetto;
  jsonDocument["DISTANZA"] = distanza;
  jsonDocument["DISTANZA_IMPOSTATA"] = distanzaImpostata;
  jsonDocument["ULTIMO_MOVIMENTO"] = ultMovLetto;
    
  serializeJson(jsonDocument, buffer1);
  return buffer1;
}

//void readJsonServerPrincipale(String inputBuffer){
//  Serial.println("Deserializing");
//    
//  //StaticJsonDocument<JSON_SIZE> jsonDocument;
//  jsonDocument.clear();
//  deserializeJson(jsonDocument, inputBuffer);
//
//  stateLuceCamera = jsonDocument["STATO_LUCE_CAMERA"];
//  stateLuceLetto = jsonDocument["STATO_LUCE_LETTO"];
//  stateStufetta = jsonDocument["STATO_STUFETTA"];
//  temperatura = jsonDocument["TEMPERATURA"];
//  umidita = jsonDocument["UMIDITA"];
//  luminosita = jsonDocument["LUMINOSITA"];
//  controlloAutomatico = jsonDocument["CONTROLLO_AUTOMATICO"];
//    
//}


void readJsonServerScrivania(){
  String url = "http://" + IPServerScrivania + "/getJsonServerScrivania";
  String inputBuffer = makeHttpRequest(url);

  if (inputBuffer != "") {
    if (DEBUG) Serial.println("Deserializing jsonServerScrivania");
    DynamicJsonDocument jsonDocument(JSON_SIZE);
    deserializeJson(jsonDocument, inputBuffer);
    stateLuceScrivania = jsonDocument["STATE_LUCE_SCRIVANIA"];
  }
  
}

AsyncWebServer server(80);

bool stateLuceCamera = false;

String ultmov = "";
String fineultmov = "";
String lastAction = "";


const int relayLettoPin = 5;  //D1
const int pirLetto = 13;  //D7

const int trigPin = 12;  //D6
const int echoPin = 14;  //D5




void checkLuceLetto(){
 
  if (checkPir(pirLetto) || checkRadar() ){
    timestampLuceLetto = millis();
    if (!stateLuceLetto && controlloAutomatico) {
    accendiLuceLetto();
    }
  }

  if(controlloSincronoConScrivania){
    if(stateLuceScrivania){
      timestampLuceLetto = millis();
      if (!stateLuceLetto && controlloAutomatico) {
        accendiLuceLetto();
      }
    }
  }
  
  if (millis() - timestampLuceLetto > timerLuceLetto *60000) {
    if (stateLuceLetto && controlloAutomatico) {
      spegniLuceLetto();
    }
  }
  
}

void setupPins(){
  pinMode(relayLettoPin, OUTPUT); 
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  pinMode(pirLetto, INPUT);
    
  digitalWrite(relayLettoPin, LOW);
}

void setupRouting(){
  
   server.on("/principale", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: principale");
    request->redirect("http://192.168.1.251/");
   });

  server.on("/scrivania", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: letto");
    request->redirect("http://192.168.1.170/");
   });
  
  server.on("/bagno", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: bagno");
    request->redirect("http://192.168.1.90/");
   });
   
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: Pagina principale");
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });
    
  
  server.on("/accendiLuceLetto", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: accendiLuceLetto");
    accendiLuceLetto();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/spegniLuceLetto", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: spegniLuceLetto");
    spegniLuceLetto();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/abilitaControlloAutomatico", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: abilitaControlloAutomatico");
    abilitaControlloAutomatico();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/disabilitaControlloAutomatico", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: disabilitaControlloAutomatico");
    disabilitaControlloAutomatico();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/abilitaControlloAutomaticoLuminosita", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: abilitaControlloAutomaticoLuminosita");
    abilitaControlloAutomaticoLuminosita();
     request->send(200, "text/plain", "OK");
  });
  
  server.on("/disabilitaControlloAutomaticoLuminosita", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: disabilitaControlloAutomaticoLuminosita");
    disabilitaControlloAutomaticoLuminosita();
    request->send(200, "text/plain", "OK");
  });

  server.on("/abilitaControlloSincronoConScrivania", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: abilitaControlloSincronoConScrivania");
    abilitaControlloSincronoConScrivania();
    request->send(200, "text/plain", "OK");
  });

  server.on("/disabilitaControlloSincronoConScrivania", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: disabilitaControlloSincronoConScrivania");
    disabilitaControlloSincronoConScrivania();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/impostaTimer", HTTP_GET, [](AsyncWebServerRequest *request){
    
    impostaTimer(request -> arg("timer").toInt());
    request->send(200, "text/plain", "OK");
  });
 
 server.on("/impostaDistanza", HTTP_GET, [](AsyncWebServerRequest *request){
    impostaDistanza(request -> arg("distanza").toInt());
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/getJsonServerLetto", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: getJsonServerLetto");
    request->send(200, "application/json", getJsonServerLetto());
   });

  server.on("/alive_getJsonServerLetto", HTTP_GET, [](AsyncWebServerRequest *request){
    if (DEBUG) Serial.println("Servendo: alive_getJsonServerLetto");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getJsonServerLetto() );
    response->addHeader("Keep-Alive", "100");
    response->addHeader("Connection", "keep-alive");
    request->send(response);
   });

  server.begin();
}

void setup() {
  Serial.begin(115200);
   
  setupSpiffs(); 
  setupOta(); 
  setupPins();
  setupWifi();
  setupRouting();
  
  timeClient.begin();
}

void setupWifi(){

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  
  //IPLocal = String(WiFi.localIP());
  MAC = WiFi.macAddress();

  Serial.println(WiFi.localIP());
  oraServerOnline = "Server online da: " + getGiornoAndOra();
  
}

void loop() {
  ArduinoOTA.handle();
  //timeClient.update();
     
  // OGNI SECONDO
  if (millis() - previousCheckLuminosita >= intervalCheckLuminosita) {
    previousCheckLuminosita = millis();
    getLuminosita();
    
    //CHECK controllo automatico LUMINOSITA
    if (controlloAutomaticoLuminosita){
      if(stateLuceLetto || stateLuceCamera){
        threshold_luminosity = THRESHOLD_LUMINOSITY + 100;
      }
      else{
        threshold_luminosity = THRESHOLD_LUMINOSITY;
      }
        
      if(luminosita <= threshold_luminosity ){
        if(!controlloAutomatico){
          abilitaControlloAutomatico();
        }
      }
   
      else if(luminosita > threshold_luminosity ){
        if (stateLuceLetto){
           spegniLuceLetto();
        }
        if(controlloAutomatico){
          disabilitaControlloAutomatico();
        }
      } 
    }

    readJsonServerScrivania();
    checkLuceLetto(); // OGNI SECONDO
    
  }
}


void accendiLuceLetto(){
  if (DEBUG) Serial.println("Accensione luce letto");
  digitalWrite(relayLettoPin, HIGH);
  stateLuceLetto = true;
  //sendLogToCloud(getGiornoAndOra() + String(" Luce letto accesa!<br>"));
}

void spegniLuceLetto(){
  if (DEBUG) Serial.println("Spegnimento luce letto");
  digitalWrite(relayLettoPin, LOW);
  stateLuceLetto = false;
  //sendLogToCloud(getGiornoAndOra() + String(" Luce letto spenta!<br>"));
}

void abilitaControlloAutomatico(){
  if (DEBUG) Serial.println("Abilito controllo automatico");
  controlloAutomatico = true;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo automatico abilitato!<br>"));
}

void disabilitaControlloAutomatico(){
  if (DEBUG) Serial.println("Disabilito controllo automatico");
  controlloAutomatico = false;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo automatico disabilitato!<br>"));
}

void abilitaControlloAutomaticoLuminosita() {
  if (DEBUG) Serial.println("Abilito controllo automatico luminosità");
  controlloAutomaticoLuminosita = true;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo automatico luminosità abilitato!<br>"));
  
}

void disabilitaControlloAutomaticoLuminosita() {
  if (DEBUG) Serial.println("Disabilito controllo automatico luminosità");
  controlloAutomaticoLuminosita = false;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo automatico luminosità disabilitato!<br>"));
 
}

void abilitaControlloSincronoConScrivania() {
  if (DEBUG) Serial.println("Abilito controllo sincrono con Scrivania");
  controlloSincronoConScrivania = true;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo sincrono con Scrivania abilitato!<br>"));
}

void disabilitaControlloSincronoConScrivania() {
  if (DEBUG) Serial.println("Disabilito controllo sincrono con Scrivania");
  controlloSincronoConScrivania = false;
  //sendLogToCloud(getGiornoAndOra() + String(" Controllo sincrono con Scrivania disabilitato!<br>"));
 
}


void impostaTimer(int timer){
  if (DEBUG) Serial.println("Imposto timer " + String(timer) +" minuti");
  timerLuceLetto = timer;
}


void impostaDistanza(int distanza) {
  if (DEBUG) Serial.println("Imposto distanza " + String(distanza) +" CM");
  distanzaImpostata = distanza;
}


void sendLogToCloud(String message) {
  lastAction = message;
  if (message.indexOf(" ") != -1) {
    message.replace(" ", "%20");
  }
  http.begin(client, "http://" + IPServerNas + "/LogData?message=" + message);
  
  //   int httpCode = http.GET();
  //   if (httpCode > 0) {
  //     //IPPub = http.getString();
  //     Serial.println("Messaggio scritto con successo!"+ message);
  //   }
  //   else { Serial.println(F("Error on HTTP request"));}
  http.end();
}

bool checkRadar(){
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);

      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);

      duration = pulseIn(echoPin, HIGH);
      distanza = duration * 0.034 / 2;

      Serial.print("distanza: ");
      Serial.println(distanza);
      if (distanza < distanzaImpostata) {
        return true;
      }
      return false;
}

bool checkPir(int pinPir){
  bool rilevatoMovimento = false;
  int valPir = digitalRead(pinPir);
  int statePir = LOW;
  
  if (valPir == HIGH) {
    if (statePir == LOW) {
      Serial.println("Rilevato movimento");
      //timeClient.update();
      
      ultMovLetto = getGiornoAndOra();

      rilevatoMovimento = true;
      statePir = HIGH;
    }
  }
  else {
    if (statePir == HIGH) {
     
      statePir = LOW;
    }
  }

  return rilevatoMovimento;
}  


String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons += "<h4>Output - GPIO 2</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" ><span class=\"slider\"></span></label>";
    
    return buttons;
  }
  else if(var == "getGiornoAndOra"){
    String giornoAndOra = getGiornoAndOra();
        
    return giornoAndOra;
  }
  return String();
}

void setupSpiffs(){
  if(!SPIFFS.begin()){
     Serial.println("An Error has occurred while mounting SPIFFS");
     return;
  }
}

void setupOta(){
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = F("sketch");
    } else { // U_FS
      type = F("filesystem");
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println(F("Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println(F("Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println(F("Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println(F("Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial.println(F("End Failed"));
    }
  });
  
  ArduinoOTA.begin();
}
