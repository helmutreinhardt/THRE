#define FW_VERSION 360    //Firmware Version  
#define ledPIN 27         //  Digitalen Pin WS2812 bzw. NeoPixel
#define busyPin 33        //  from DFPlayer
#define Taster 21
#define rxMP3 16
#define txMP3 17

#define NUMPIXELS 121          // Länge NeoPixel
#define BeginOfStatus 110      // 1. LED Statusleiste
#define AnzLedsStatus 12       // Anzahl StatusLEDs
#define DemoBeginOfStatus 0    // 1. led DemoStatusleiste (DemoJahr)
#define DemoAnzLedsStatus 120  // Anzahl StatusLEDs
#define AnzF_Feiertage 30
#define AnzV_Feiertage 20
#define Anz_Helptexte 21
#define EEPROM_SIZE 222
#define Sommerzeit 3600
#define HandyButtons 24
#define gags 7
#define DimLogbuch 100
#define HTML_PORT ":5000"
#define PUSH_PORT ":6000"
#define SERVER_IP_WETEAM "87.106.79.109"
#define SERVER_IP_BERG "192.168.178.64"

#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

//#include <driver/adc.h>

Adafruit_NeoPixel pixels(NUMPIXELS, ledPIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial mySerial(rxMP3, txMP3);
WiFiClient push;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

WebServer server(80);
Preferences prefs;


typedef struct {             // Int.nr  Anzahl
  int lastDay;               //   0        1   letzter gültiger Tag
  int ledtoday = 1;          //   1        1   Merker je LED
  int printDetail = 0;       //   2        1   Informationen drucken Null entspricht false, alle anderen Werte true
  int isUpDate;              //   3        1   struct-PROM in EEPROM speichern, wenn sich geändert
  int piezo;                 //   4        1   Piezo lautsprecher
  int bigben;                //   5        1
  int blinker;               //   6        1
  int sommer = 0;            //   7        1   Winterzeit normal
  int demozeit = 0;          //   8        1
  int demoJahr = 0;          //   9        1
  int nextzufall = 308;      //  10        1
  int lautst = 6;            //  11        1
  int brightness;            //  12        1
  int skinnumber = 1;        //  13        1
  int ansage = 1;            //  14        1
  int bbvon = 800;           //  15        1
  int bbbis = 2000;          //  16        1
  int otaEnable = 0;         //  17        1   erst für webOTA
  int vfeiertage[20] = {};   //  18       20   variable Feiertage pogammierbar über Funktion 6
  int ansagevon = 700;       //  39        1
  int ansagebis = 2100;      //  40        1
  int nachtlichtvon = 2200;  //  41        1
  int nachtlichtbis = 600;   //  42        1
  int dunkel = 2;            //  43        1
  int online = 0;            //  44        1
} EEP;
EEP PROM;


typedef struct {
  char mac[20];
  int lfdNr;
  int tag;
  int uhrzeit;
  int gesendet;
  int sendetag;
  int sendeuhrzeit;
  int fktnr;
  char version[4];
  char text[40];
} PROTO;
PROTO Protokoll[DimLogbuch];

int PointerLogIn;
int PointerLogOut;
bool FirstLoop = true;
bool datenfree = false;

char Ausgabebuffer[200];

String weekDays[7] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" };
int morest[12] = { 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };
int tarest[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int smileyplus[]  = { 3, 4, 5, 6, 7, 13, 19, 23, 31, 33, 37, 40, 43, 44, 54, 55, 65, 66, 76, 77, 80, 85, 87, 89, 92, 93, 94, 95, 97, 101, 107, 113, 114, 115, 116, 117, 200 };
int smileyminus[] = { 3, 4, 5, 6, 7, 13, 19, 23, 31, 33, 36, 40, 43, 44, 54, 55, 65, 66, 76, 77,  87, 89 ,84, 83, 82, 81, 80, 97, 101, 107, 113, 114, 115, 116, 117, 200 };

long BEGTime = 8000;
long FORTime = 2000;

unsigned long otaStartTime = 0;
const unsigned long otaDuration = 10 * 60 * 1000;  // 10 Minuten in Millisekunden
const unsigned long otaTrigger = 2 * 60 * 1000;    //  2 Minuten in Millisekunden

bool otaRun = false;
bool nachtON = false;
bool tagON = false;
bool tagDemo = false;
byte scrollBuffer[8] = { 0 };
const char *buttonLabels[HandyButtons] = {
  "Sommerzeit", "Demo Zeit", "Demo  Jahr",
  "BigBen", "Farbe", "Lautstärke",
  "Feiertag", "Feiertag", "Feiertage",
  "Speicher", "Speicher", "Werkseinstellung",
  "Nachtlicht", "Helligkeit", "Ansage Uhrzeit",
  "Zufall", "BigBen", "Seriennummer",
  "Update", "Spitzname", "Info",
  "Online ?", "Meldung", "Online"
};
const char *buttonLabels1[HandyButtons] = {
  "on/off", "on/off", "on/off",
  "1-4", "1-10", "1-29",
  "ta.mo,Name", "Löschen", "Anzeige",
  "Anzeige", "Löschen", "...",
  "xx:xx xx:xx", "1-255", "von - bis",
  "xx:xx", "von - bis", "...",
  "erlauben+Taster", " ", " ",
  "...", "...", "on/off"
};
const char *zustand[HandyButtons] = {};

String HELPText[Anz_Helptexte] = {
  "\nHELP:",
  "\n1.\tBlinker ein/aus"

};
byte currentColorIndex = 0;
byte MatrixFarbe[][3] = { { 200, 0, 0 }, { 0, 200, 0 }, { 0, 0, 200 }, { 200, 200, 0 }, { 0, 200, 200 }, { 200, 0, 200 }, { 200, 100, 0 }, { 0, 200, 100 }, { 200, 0, 100 }, { 100, 200, 0 }, { 0, 100, 200 }, { 100, 0, 200 } };
byte MASKE[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
String Worte[24] = {
  { "ES" }, { "IST" }, { "FÜNF" }, { "ZEHN" }, { "VOR" }, { "NACH" }, { "VIERTEL" }, { "HALB" }, { "VOR" }, { "NACH" }, { "EINS" }, { "ZWEI" }, { "DREI" }, { "VIER" }, { "FÜNF" }, { "SECHS" }, { "SIEBEN" }, { "ACHT" }, { "NEUN" }, { "ZEHN" }, { "ELF" }, { "ZWÖLF" }, { "UHR" }, { "" }
};

unsigned long startMessung;
unsigned long endeMessung;

byte BUFFER[8];

int MONTAG;  // MonatTag
int Demo_MONTAG;
int BREMSE = 0;  // Programm ausbremsen
int differenzMessung;
int ZUFALL;  // Zufallszahl 200-300
int TÜRKIS = 2;
int RED = 0;
int GREEN = 1;
int FARBE[] = { TÜRKIS, GREEN, RED, GREEN };
int MP3busy;
int SONG = 0;
int Uhrzeit[24];
int WERTE[10];
int last5Minuten;
int Statusleft, Statusright;
int LEDFTMerker[NUMPIXELS + 1];  // für DemoJahr

int Anz_Feiertage;
int FT[AnzF_Feiertage + AnzV_Feiertage];


int Holidays[AnzF_Feiertage] = { 2502, 2907, 2008, 3108, 2807, 1611, 1309, 3008,       //Helvi,Arik,Tim,Paula,Stephan,Grit,Sigrid,Helmut
                                 708, 1211, 2401, 2701, 2712,2803,                     //Claudia,Manfred,Thomas,Klaus,Stefan,Karo
                                 1410, 811, 3003, 1510,                                //Heinrich,Adelheid,Gerhard,Hilde
                                 205,2802,805,502,2805,1911,                           //Micha,Katja,Lewi,Mia,Taawi,Anne
                                 2609, 2308, 2202, 2604, 2707,3112 };                  //Gabi,Anja,Jonas,Peter,Nick };

String Names[AnzF_Feiertage] = {"Helvi","Arik","Tim","Paula","Stephan","Grit","Sigrid","Helmut",
                                "Claudia","Manfred","Thomas","Klaus","Stefan","Karo",
                                "Heinrich","Adelheid","Gerhard","Hilde",  
                                "Micha","Katja","Lewi","Mia","Taawi","Anne",
                                "Gabi","Anja","Jonas","Peter","Nick","Silvester"};

String FN[AnzF_Feiertage+ AnzV_Feiertage];
String vnames[AnzV_Feiertage] ;    
String Holiday_Name;
String lastPASSWORD;
String lastSSID;
String lastIP;
String Passwort_vorgemerkt;
String Detail[50];
String Message = "";
String TAMOJA;  // TagMonatJahr
String Demo_TAMOJA;
String STDMI;  // StundeMinute
String MISI;   // MinuteSekunde
String TIME;
String ZMinute = "0000";
String DRUCK;
String WTAG;
String Demozeit = "2250";
String TEMP;
String PARAS[10];
String htmlRumpf;
String Protokoll_IP = SERVER_IP_WETEAM;
String apiW="bbc0df7596a44e1b9cc134505262905";

volatile bool interruptTriggered = false;
volatile bool switchPressed = false;

volatile bool touchTriggered = false;
volatile bool touchPressed = false;

typedef void (*OtaProgressCallback)(int percent);

//Interrupt-Service-Routine (ISR)
ICACHE_RAM_ATTR void TasterUnterbricht() {
  interruptTriggered = true;
}



void handleLabels() {
  String json = "[";
  for (int i = 0; i < HandyButtons; i++) {
    json += "\"" + String(buttonLabels[i]) + "\"";
    json += ",";
    json += "\"" + String(buttonLabels1[i]) + "\"";
    if (i < HandyButtons - 1) json += ",";
  }
  json += "]";
  if (PROM.printDetail) { Serial.println(json); }
  server.send(200, "application/json", json);
  if (PROM.printDetail) { Serial.println("/label"); }
}
void handleUpdateFinished() {
  if (Update.hasError()) {
    server.send(200, "text/plain", "Update Fehler!");
  } else {
    server.send(200, "text/plain", "Update OK! Neustart...");
    delay(1000);
    ESP.restart();
  }
}
void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}
void handleZustand() {
  String json = "[";
  for (int i = 0; i < HandyButtons; i++) {
    json += "\"" + String(zustand[i]) + "\"";
    if (i < HandyButtons - 1) json += ",";
  }
  json += "]";

  server.send(200, "application/json", json);
  if (PROM.printDetail) { Serial.println("/zustand"); }
}

void handleSETZustand() {
  bool ServerSave = true;
  String body = server.arg("plain");
  if (PROM.printDetail) {
    Serial.print("\nSETZustand empfangen:");
    Serial.println(body);  // z. B. [true,false,...]
                           // JSON-Array einlesen
  }
  const size_t CAPACITY = JSON_ARRAY_SIZE(18) + 100;
  DynamicJsonDocument doc(CAPACITY);
  deserializeJson(doc, body);
  JsonArray zustand = doc["zustand"];
  String zusatz = doc["zusatzwert"] | "";

  int aktiv = -1;
  for (int i = 0; i < zustand.size(); i++) {
    if (zustand[i] == true) {
      aktiv = i;
      break;  // nur ein Wert darf true sein
    }
  }

  aktiv += 1;
  htmlRumpf = "";
  //Piepe();
  switch (aktiv) {
    case 0:
      htmlRumpf = "?";
    case 1:
      SETSommer();
      break;
    case 2:
      SETDemozeit();
      break;
    case 3:
      SETDemojahr();
      break;
    case 4:
      SETBigBen(zusatz);
      break;
    case 5:
      SETFarbe(zusatz);
      break;
    case 6:
      SETVolume(zusatz);
      break;
    case 7:
      SetFeiertag(zusatz);
      break;
    case 8:
      DELlastFeiertag();
      break;
    case 9:
      ShowFT(Anz_Feiertage);
      break;
    case 10:
      ShowEEPROM();
      break;
    case 11:
      EraseEEPROM();
      break;
    case 12:
      Werkseinstellung();
      break;
    case 13:
      SETnachtlicht(zusatz);
      break;
    case 14:
      SetBrightness(zusatz);
      break;
    case 15:
      SETAnsage(zusatz);
      break;
    case 16:
      SetZufall(zusatz);
      break;
    case 17:
      BigBenTime(zusatz);
      break;
    case 18:
      ChipID();
      break;
    case 19:
      OTAEnable();
      break;
    case 20:
      ServerSave = SETnickname(zusatz);
      break;
    case 21:
      Info();
      break;
    case 22:
      QONLINE();
      break;
    case 23:
      PUSHTO(zusatz);
      break;
    case 24:
      SetOnline();
      break;
    default:
      HELP();
      break;
  }
  if (ServerSave) { SavePROT(aktiv, zusatz); }
  server.send(200, "text/plain", htmlRumpf);
  if (PROM.printDetail) { Serial.println("/setZustand"); }
}

void handleRoot() {

  String html = "";

  html += "<!DOCTYPE html><html><head><title>Status Buttons</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += ".okstyle { font-size: 20px; padding: 10px 20px; background-color: green; color: white; }";
  html += "body { font-family: sans-serif; padding: 20px; text-align: center; }";
  html += ".grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; max-width: 400px; margin: auto; }";
  html += "button { padding: 10px; font-size: 12px; border: none; border-radius: 8px; color: white; width: 100%; }";
  html += ".on { background: green; }";
  html += ".off { background: gray; }";
  html += ".true { background: yellow; }";
  html += "#okbtn { background: grey; margin-top: 20px; padding: 16px 32px; }";
  html += "#okbtn { width: 100%; max-width: 400px; margin: 20px auto; display: block; background: grey; padding: 16px 32px; }";
  html += "#okbtn:active { background: green; }";
  html += "</style></head><body>";

  html += "<h2>ESP32 Uhr THRE</h2>";
  html += DelNull(lastIP) + "<br>";
  html += DelNull(lastSSID) + "<br>";

  html += "<div class='grid' id='buttonGrid'></div>";
html += "<div id='antwort' style='text-align:center; font-family:Courier New, monospace; white-space:pre;'></div>";
  html += "<div id='pwfeld' style='display:initial; margin-top:20px;'>";
  html += "<p>Bitte Wert eingeben:</p>";
  html += "<input type='text' id='pwinput' style='padding:8px; font-size:16px;' />";
  html += "</div>";


  html += "<button id='okbtn' class='okstyle' onclick='sendeZustand()'>So soll es sein</button>";
  html += "<p id='antwort'></p>";
  //html += "<p><a href='/ota' style='font-size:22px;'>Firmware Update</a></p>";


  html += "<script>";
  html += "let ian=24;";
  html += "let beschriftung = [];";
  html += "let zustand = [];";
  html += "let grid = document.getElementById('buttonGrid');";
  html += "let aktiveTaste = null;";
  html += "let okTimeout = null;";
  html += "let okbtn = document.getElementById('okbtn');";
  html += "okbtn.disabled = true;";

  // hole Beschriftung und erstelle ian Buttons
  html += "fetch('/labels')";
  html += "  .then(r => r.json())";
  html += "  .then(data => {";
  html += "    beschriftung = data;";
  html += "    erstelleButtons();";  // Buttons mit geladenen Labels erzeugen
  html += "  });";

  // Buttons generieren
  html += "function erstelleButtons() {";
  html += "  for (let i = 0; i < ian; i++) {";
  html += "    let btn = document.createElement('button');";
  html += "    btn.id = 'btn' + ('0'+i).slice(-2);";
  html += "    btn.className = 'off';";
  html += "    btn.innerHTML = beschriftung[i + i] + '<br>' + beschriftung[i + i + 1];";
  html += "    btn.onclick = function() { toggle(i); };";
  html += "    grid.appendChild(btn);";
  html += "  }";
  html += "}";

  html += "function updateButton(i) {";
  html += "  let btn = document.getElementById('btn' + ('0'+i).slice(-2));";
  html += "  btn.className = zustand[i] ? 'on' : 'off';";
  html += "  if (zustand[i]) {";
  html += "    document.getElementById('antwort').innerText = ' ';";
  html += "  }";
  html += "}";

  // Umschalten
  html += "function toggle(i) {";  // wird ausgeführt bei btn.onclick
  html += "if (i === 3 ||i === 4 || i === 5 || i === 6|| i === 12|| i === 13|| i === 14|| i === 15|| i === 16|| i === 19 || i === 22) {";
  html += "  document.getElementById('pwfeld').style.display = 'block';";
  html += "} else {";
  html += "  document.getElementById('pwfeld').style.display = 'none';";
  html += "}";

  html += "  zustand = Array(ian).fill(false);";
  html += "  zustand[i] = true;";
  html += "  aktiveTaste = i;";
  html += "  for (let j = 0; j < ian; j++) updateButton(j);";

  html += "  if (okTimeout) clearTimeout(okTimeout);";
  html += "  okbtn.disabled = false;";
  html += "  okbtn.style.backgroundColor = 'green';";

  html += "  okTimeout = setTimeout(() => {";
  html += "    zustand = Array(ian).fill(false);";
  html += "    aktiveTaste = null;";
  html += "    for (let j = 0; j < ian; j++) updateButton(j);";
  html += "    okbtn.disabled = true;";
  html += "    okbtn.style.backgroundColor = 'grey';";
  html += "    document.getElementById('antwort').innerText = '';";  // 🔹 Meldung löschen
  html += "  }, 20000);";

  html += "}";

  html += "function sendeZustand() {";
  html += "  if (okTimeout) clearTimeout(okTimeout);";
  html += "  okbtn.disabled = true;";
  html += "  okbtn.style.backgroundColor = 'grey';";
  html += "  aktiveTaste = null;";
  // Sonderfälle: Eingabe nötig
  html += " if (zustand[3]||zustand[4]||zustand[5]||zustand[6]||zustand[12]||zustand[13]||zustand[14]||zustand[15]||zustand[16]||zustand[19]||zustand[22]) {";
  html += "    let pw = document.getElementById('pwinput').value;";
  html += "    document.getElementById('pwfeld').style.display = 'none';";
  html += " }";
  // Übertragung
  html += "  let pw = document.getElementById('pwinput').value;";
  html += "  let daten = { zustand: zustand, zusatzwert: pw };";
  html += "  fetch('/setZustand', {";
  html += "    method: 'POST',";
  html += "    headers: { 'Content-Type': 'application/json' },";
  html += "    body: JSON.stringify(daten)";
  html += "  })";
  html += "  .then(r => r.text())";
  html += "  .then(text => {";
  html += "    document.getElementById('antwort').innerText = ' ' + text;";  

  // rücksetzen
  html += "    document.getElementById('pwinput').value = '';";
  html += "    zustand = Array(ian).fill(false);";
  html += "   for (let j = 0; j < ian; j++) updateButton(j);";
  html += "  });";
  html += "}";

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void SavePROT(int fk, const String &t) {
  int lfn = PointerLogIn;
  String nick, version;
  version = FW_VERSION;
  nick = getNick() + ":" + t;
  Protokoll[lfn].lfdNr = lfn;
  Protokoll[lfn].tag = MONTAG;
  Protokoll[lfn].uhrzeit = STDMI.toInt();
  version.toCharArray(Protokoll[lfn].version, sizeof(Protokoll[lfn].version));
  Protokoll[lfn].gesendet = 0;
  Protokoll[lfn].fktnr = fk;
  nick.toCharArray(Protokoll[lfn].text, sizeof(Protokoll[lfn].text));
  if (PROM.printDetail) {
    Serial.print("Programmversion:");
    Serial.println(Protokoll[lfn].version);
  }
  PointerLogIn += 1;
  if (PointerLogIn >= DimLogbuch - 1) {
    Serial.println("Überlauf PROT");
    PointerLogIn -= 1;
  }
}
void Show11x11smileyminus() {
  int r, g, b, x, y, index;
  int farbe = PROM.skinnumber;
  LEDSetrgb(farbe, r, g, b);
  pixels.clear();
  for (int p = 0; p < 121; p++) {
    index = smileyminus[p];
    if (index >= 200) { break; }
    y = index / 11;
    x = index % 11;
    index = getPixelNumber(x, y);
    pixels.setPixelColor(index, pixels.Color(r, g, b));
  }
  pixels.show();
  delay(5000);
  pixels.clear();
  pixels.show();
}
String leftTrim(String s) {
  while (s.length() > 0 && isspace(s[0])) {
    s.remove(0, 1);
  }
  return s;
}
void extractPs(String zeile, String tr = ",") {
  int colonIndex = 0;
  int colonStart = 0;
  int ian = 0;
  //zeile=zeile+tr+tr+tr+tr;
  for (int z = 0; z > 5; z++) { PARAS[z] = ""; }
  while ((colonIndex = zeile.indexOf(tr, colonIndex)) != -1) {
    if (colonIndex >= 0 && colonIndex < zeile.length()) {
      PARAS[ian] = zeile.substring(colonStart, colonIndex);
      if (PROM.printDetail) {
        Serial.print("\nPARAS:");
        Serial.print(PARAS[ian]);
        Serial.print(";");
      }
      colonStart = colonIndex + 1;
      ian++;
    }
    colonIndex++;  // kein Treffer, nächsten Doppelpunkt suchen
    if (ian > 5) { return; }
  }
  PARAS[ian] = zeile.substring(colonStart, colonIndex);
}



void LoadAndShow(const char *pfad, String nr = "0", int t = 5000) {
    WiFiClient client;
    HTTPClient http;
    if (PROM.online == 0) return;
    String url = "http://" + Protokoll_IP + HTML_PORT +  "/bild/0/" ;
    http.begin(client, url);
    if (http.GET() != 200) {
      Show11x11smileyminus();
      return;
    }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[NUMPIXELS * 3];  // 363 Bytes
  int read = stream->readBytes(buf, sizeof(buf));
  if (read != sizeof(buf)) {
    Serial.println("Unvollständige Daten!");
    return;
  }

  int p = 0;
  for (int y = 0; y < 11; y++) {
    for (int x = 0; x < 11; x++) {
      uint8_t r = buf[p++];
      uint8_t g = buf[p++];
      uint8_t b = buf[p++];
      int index = getPixelNumber(x, y);
      pixels.setPixelColor(index, pixels.Color(r, g, b));
    }
  }
  pixels.show();
  delay(t);
  pixels.clear();
  pixels.show();
}

void LoadHelp(const char *pfad, String nr = "0", int t = 5000) {
  WiFiClient client;
  HTTPClient http;
  String url = "http://" + Protokoll_IP + HTML_PORT + String(pfad) + "/" + nr + "/";
  http.begin(client, url);
  if (http.GET() != 200) {
    Serial.println("HTTP Fehler!");
    Show11x11smileyminus();
    return;
  }

  WiFiClient *stream = http.getStreamPtr();
  char buf[300];
  int read = stream->readBytes(buf, sizeof(buf));

  String html;
  for (int z = 0; z < read; z++) {
    html += buf[z];
  }
  sprintf(Ausgabebuffer, "HELP: %s", html.c_str());
}

void ToHTMLTextClear() {
  htmlRumpf = "";
}
void ToHTMLText(String text) {
  htmlRumpf += text;  // text anhängen
  Serial.print(text);
}
void ToHTMLTextln(String text) {
  htmlRumpf = text;  // nur eine Zeile
  Serial.println(text);
}
void ToHTMLTextTop(String text) {
  htmlRumpf = text + htmlRumpf;  // ober ran
  Serial.println(text);
}

void HELP() {
  String html;
  ToHTMLTextClear();
  for (int z = 0; z < Anz_Helptexte; z++) {
    html += HELPText[z];
  }
  ToHTMLTextln(html);
}

int AnzahlFT() {
  int ian = AnzF_Feiertage + AnzV_Feiertage;
  for (int z = 0; z < AnzF_Feiertage; z++) {
    FT[z] = Holidays[z];
    FN[z] = Names[z];
  } 
  for (int z = 0; z < AnzV_Feiertage; z++) {
    FT[AnzF_Feiertage + z] = PROM.vfeiertage[z];
    FN[AnzF_Feiertage + z] = vnames[z];
  }
  return ian;
}


bool waitForButtonPress(long timeout, int leds = 1) {
  bool press = false;
  String t;
  LEDclearstatus();
  delay(100);
  LEDgreen(BeginOfStatus);
  if (leds == 2) { LEDgreen(BeginOfStatus + 1); }
  unsigned long startTime = millis();
  while ((millis() - startTime < timeout) and (not press)) {
    if (digitalRead(Taster) == LOW) { press = true; }
    server.handleClient();
    if (otaRun) { ArduinoOTA.handle(); }
    delay(100);  // Small delay to avoid busy-waiting
  }
  if (press) {
    t = " erfolgreich gestartet!";
  } else {
    t = "  abgebrochen !";
  }
  ToHTMLText(t);
  return press;
}


void PlayStop() {
  mp3_stop();
}

void PlayMP3(int song, int volume = 0, int wait = 0) {
  if (volume == 0) {
    volume = PROM.lautst;
    delay(150);
  }
  if (PROM.printDetail) {
    Serial.print("\nPLAY Volume:");
    Serial.print(volume);
    Serial.print("  Song:");
    Serial.print(song);
  }
  mp3_set_volume(volume);
  delay(200);
  while (!digitalRead(busyPin)) {
    delay(500);
    Serial.print("🔊x");
  }
  mp3_play(song);
  if (wait == 0) { return; }
  while (!digitalRead(busyPin)) {
    delay(500);
    Serial.print("🔊");
  }
  delay(wait);
}
void PlayMP3Folge(int song, int folge, int volume = 0) {
  for (int i = 0; i < folge; i++) {
    while (!digitalRead(busyPin)) {
      delay(500);
      Serial.print("🔊");
    }
    PlayMP3(song, 0);
  }
}
void LEDoff(int led) {
  pixels.setPixelColor(led, pixels.Color(0, 0, 0));
  pixels.show();
  delay(50);
}

void LEDsClear() {
  pixels.clear();
  delay(100);
  pixels.show();
  delay(100);
}
void LEDclearstatus() {
  for (int i = BeginOfStatus; i < BeginOfStatus + 12; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  delay(100);
  pixels.show();
  delay(100);
}
void LEDclear(int led) {
  pixels.setPixelColor(led, pixels.Color(0, 0, 0));
  delay(100);
  pixels.show();
  delay(100);
}
void LEDgreen(int led) {
  int r, g, b;
  LEDSetrgb(GREEN, r, g, b);
  pixels.setPixelColor(led, pixels.Color(r, g, b));
  pixels.show();
}
void LEDSetrgb(int f, int &r, int &g, int &b) {
  r = MatrixFarbe[f][0];
  g = MatrixFarbe[f][1];
  b = MatrixFarbe[f][2];
}

void LEDlauflicht(int l, int farbe, int ian) {
  int r, g, b;
  ian = ian - 2;
  LEDSetrgb(farbe, r, g, b);
  int led = l % (ian + ian);
  if (led > ian) { led = (ian + ian) - led; };
  pixels.clear();
  pixels.setPixelColor(BeginOfStatus + led, pixels.Color(r, g, b));
  pixels.show();
}
void LEDhinher(int l, int farbe) {
  int r, g, b;
  LEDSetrgb(farbe, r, g, b);
  int hin, her;
  hin = l % 6;
  her = 10 - hin;
  pixels.clear();
  delay(100);
  pixels.setPixelColor(BeginOfStatus + hin, pixels.Color(r, g, b));
  delay(100);
  pixels.setPixelColor(BeginOfStatus + her, pixels.Color(r, g, b));
  delay(100);
  pixels.show();
}
void LEDblink(int led, int farbe) {
  int r, g, b;
  LEDSetrgb(farbe, r, g, b);
  for (int z = 0; z < 10; z += 1) {
    pixels.setPixelColor(led, pixels.Color(10, 10, 10));
    pixels.show();
    delay(100);
    pixels.setPixelColor(led, pixels.Color(r, g, b));
    pixels.show();
    delay(100);
  }
}
void showRing(int centerX, int centerY, int radius, uint32_t color) {
  int wi = 11;
  int hei = 11;
  int x0 = centerX - radius;
  int y0 = centerY - radius;
  int x1 = centerX + radius;
  int y1 = centerY + radius;

  if (x0 < 0 || y0 < 0 || x1 >= wi || y1 >= hei) return;

  // Obere und untere Kante
  for (int x = x0; x <= x1; x++) {
    pixels.setPixelColor(getPixelNumber(x, y0), color);  // oben
    pixels.setPixelColor(getPixelNumber(x, y1), color);  // unten
  }

  // Linke und rechte Kante (ohne Ecken doppelt)
  for (int y = y0 + 1; y < y1; y++) {
    pixels.setPixelColor(getPixelNumber(x0, y), color);  // links
    pixels.setPixelColor(getPixelNumber(x1, y), color);  // rechts
  }
  pixels.show();
}

void LEDRing(int farbe, int ringe = 5, bool revers = false) {
  int r, g, b;
  LEDSetrgb(farbe, r, g, b);
  //for (int r = 0; r <= ringe; r++) {  // Maximal 5 Ringe bei 11x11
  for (int i = revers ? ringe : 0;
       revers ? i >= 0 : i <= ringe;
       revers ? i-- : i++) {


    pixels.clear();
    showRing(5, 5, i, pixels.Color(r, g, b));
    delay(300);
  }
  delay(1000);
}

void Blinky(int z) {
  if (PROM.printDetail) {
    Serial.print("Blinky:");
    Serial.println(z);
  }
  for (int i = 0; i < z; i++) {
    LEDtwinkle(0xff, 10, 30, 100, 30, false);
  }
}
void LEDtwinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne) {
  pixels.clear();
  delay(100);
  pixels.show();
  delay(100);
  for (int i = 0; i < Count; i++) {
    pixels.setPixelColor(random(NUMPIXELS), random(0, 100), random(0, 100), random(0, 100));
    pixels.setPixelColor(random(NUMPIXELS), 0, 0, 0);
    pixels.show();
    delay(SpeedDelay);
  }
  delay(SpeedDelay);
}
int SETorGETpara(int P, String zu, int mot = 10000) {
  int v = zu.toInt();
  v = v % mot;
  if (v == 0) {
    v = P;
  } else {
    if (v < 0) { v = 0; }
    PROM.isUpDate = true;
  }
  return v;
}
void SETVolume(String volume) {
  int v = PROM.lautst = SETorGETpara(PROM.lautst, volume, 30);
  sprintf(Ausgabebuffer, "\nLautstärke: %i", v);
  ToHTMLTextln(Ausgabebuffer);
  PlayMP3(145, v);
}
void SETFarbe(String farbe) {
  int v = PROM.skinnumber = SETorGETpara(PROM.skinnumber, farbe, 10);
  sprintf(Ausgabebuffer, "\nFarbe: %i", v);
  ToHTMLTextln(Ausgabebuffer);
}

void SetBrightness(String hell) {
  int v = PROM.brightness = SETorGETpara(PROM.brightness, hell, 255);
  sprintf(Ausgabebuffer, "\nHelligkeit: %i", v);
  ToHTMLTextln(Ausgabebuffer);
}


void SetZufall(String zuf) {
  int zu;
  extractTimes(zuf, ":");
  if (WERTE[0] >= 0) { zu = WERTE[0]; }
  if (timetrue(zu) == false) {
    sprintf(Ausgabebuffer, "\nZeit %i ist nicht gültig", zuf);
    ToHTMLTextln(Ausgabebuffer);
    return;
  }
  zu = intNachKomma(zuf);
  if (zu != -1) {
    PROM.isUpDate = false;
    PlayMP3(zu, 0);
  }
  PROM.nextzufall = WERTE[0];
  ZMinute = String(WERTE[0]);
  sprintf(Ausgabebuffer, "\nLass dich überraschen ( %s ) !", ZMinute);
  ToHTMLTextln(Ausgabebuffer);
  PROM.isUpDate = true;
}

void ResetFeiertage() {
  for (int z = 0; z < AnzV_Feiertage; z++) {
    if (PROM.vfeiertage[z] != 0) {
      PROM.isUpDate = true;
      PROM.vfeiertage[z] = 0;
    }
  }
}

String rechtsBuendig(String s,int breite=30) {
    while (s.length() < breite) {
        s = " " + s;
    }
    return s;
}
String linksBuendig(String s, int breite = 30) {
    while (s.length() < breite) {
        s += " ";
    }
    return s;
}
String RTL(String SL,String SR,String TR=" : "){
  String text="\n"+rechtsBuendig(SL)+TR+linksBuendig(SR);
  return text;
}

void ShowFT(int ian) {
  String a = "\nFeiertage:\n";
  for (int z = 0; z < ian; z++) {
    if(FT[z]!=0){
        if (z%2==0){a += rechtsBuendig(FN[z])+ " "+String(10000 + FT[z]).substring(1, 5);}
        else{a += " | "+String(10000 + FT[z]).substring(1, 5) + " "+ linksBuendig(FN[z])+"\n";}
    }
  }
  a += "\n----------------------------";
  ToHTMLTextln(a);
}
void SETWetter(){
  Serial.print("Wetter");
}
void SETBlinker() {
  PROM.blinker++;
  PROM.blinker = PROM.blinker % 2;
  PROM.isUpDate = true;
  sprintf(Ausgabebuffer, "\nBlinker jetzt: %i", PROM.blinker);
  ToHTMLTextln(Ausgabebuffer);
}
void SETPiezo() {
  PROM.piezo++;
  PROM.piezo = PROM.piezo % 2;
  PROM.isUpDate = true;
  sprintf(Ausgabebuffer, "\nPiezoton jetzt: %i", PROM.piezo);
  ToHTMLTextln(Ausgabebuffer);
}
void SETSommer() {
  PROM.sommer++;
  PROM.sommer = PROM.sommer % 2;
  PROM.isUpDate = true;
  SetSommerWinter();
  sprintf(Ausgabebuffer, "\nSommerzeit jetzt: %i", PROM.sommer);
  ToHTMLTextln(Ausgabebuffer);
}
void SETBigBen(String nr) {
  int v = PROM.bigben = SETorGETpara(PROM.bigben, nr, 5);
  sprintf(Ausgabebuffer, "\nBigBen : %i", v);
  ToHTMLTextln(Ausgabebuffer);
  if (v == 1) { PlayMP3(161, 0); }
  if (v == 2) { PlayMP3(181, 0); }
  if (v == 3) { PlayMP3(206, 0); }
  if (v == 4) { PlayMP3(207, 0); }
}
void Swap(int &i1, int &i2) {
  int ch;
  ch = i2;
  i2 = i1;
  i1 = ch;
}

bool CheckTime2(String t, bool chng = false) {  // chng=true: werte0 und werte1 werden getauscht
  int zu;
  bool ret = false;
  extractTimes(t, ":");
  if (WERTE[0] >= 0) {
    zu = WERTE[0];
  } else {
    zu = 0;
    WERTE[0] = 0;
  }
  if (timetrue(zu) == false) {
    sprintf(Ausgabebuffer, "\nZeit %i ist nicht gültig.!", zu);
    ToHTMLTextln(Ausgabebuffer);
    return ret;
  }
  if (WERTE[1] >= 0) {
    zu = WERTE[1];
  } else {
    zu = 0;
    WERTE[1] = 0;
  }
  if (timetrue(zu) == false) {
    sprintf(Ausgabebuffer, "\nZeit %i ist nicht gültig!!", zu);
    ToHTMLTextln(Ausgabebuffer);
    return ret;
  }
  if (chng) { Swap(WERTE[0], WERTE[1]); }
  if (WERTE[1] < WERTE[0]) {
    sprintf(Ausgabebuffer, "\nZeitangabe ist nicht gültig!!!", zu);
    ToHTMLTextln(Ausgabebuffer);
    return ret;
  }
  return true;
}
void BigBenTime(String t) {
  int vv, vb;
  if (!isEmpty(t)) {
    bool ch = CheckTime2(t);
    if (!ch) { return; }
    vv = WERTE[0];
    vb = WERTE[1];
  } else {
    vv = vb = 0;
  }
  vv = PROM.bbvon = SETorGETpara(PROM.bbvon, String(vv));
  vb = PROM.bbbis = SETorGETpara(PROM.bbbis, String(vb));
  sprintf(Ausgabebuffer, "\nBigBen von: %i\nBigBen bis: %i ", vv, vb);
  ToHTMLTextln(Ausgabebuffer);
}
void SETnachtlicht(String t) {
  int vv, vb, vw;
  if (!isEmpty(t)) {
    extractPs(t, ",");
    bool ch = CheckTime2(PARAS[0], true);
    if (!ch) { return; }
    vv = WERTE[1];
    vb = WERTE[0];
    vw = PARAS[1].toInt();
  } else {
    vv = vb = vw = 0;
  }
  vv = PROM.nachtlichtvon = SETorGETpara(PROM.nachtlichtvon, String(vv));
  vb = PROM.nachtlichtbis = SETorGETpara(PROM.nachtlichtbis, String(vb));
  vw = PROM.dunkel = SETorGETpara(PROM.dunkel, String(vw));
  sprintf(Ausgabebuffer, "\nNachtlicht von : %i\nNachtlicht bis : %i\nHelligkeit : %i", vv, vb, vw);
  ToHTMLTextln(Ausgabebuffer);
}

void SETAnsage(String t) {
  int vv, vb;
  if (!isEmpty(t)) {
    bool ch = CheckTime2(t);
    if (!ch) { return; }
    vv = WERTE[0];
    vb = WERTE[1];
  } else {
    vv = vb = 0;
  }
  vv = PROM.ansagevon = SETorGETpara(PROM.ansagevon, String(vv), 10000);
  vb = PROM.ansagebis = SETorGETpara(PROM.ansagebis, String(vb), 10000);
  sprintf(Ausgabebuffer, "\nAnsage von : %i \nAnsage bis : %i", vv, vb);
  ToHTMLTextln(Ausgabebuffer);
}

void SETDemozeit() {
  PROM.demozeit++;
  PROM.demozeit = PROM.demozeit % 2;
  PROM.isUpDate = true;
  sprintf(Ausgabebuffer, "\nDemozeit jetzt: %i", PROM.demozeit);
  ToHTMLTextln(Ausgabebuffer);
}
void SETDemojahr() {
  PROM.demoJahr++;
  PROM.demoJahr = PROM.demoJahr % 2;
  PROM.isUpDate = true;
  sprintf(Ausgabebuffer, "\nDemojahr jetzt:%i", PROM.demoJahr);
  ToHTMLTextln(Ausgabebuffer);
}
void SETPrintDetail() {
  PROM.printDetail++;
  PROM.printDetail = PROM.printDetail % 2;
  PROM.isUpDate = true;
  sprintf(Ausgabebuffer, "\nPrintDetail jetzt:%i", PROM.printDetail);
  ToHTMLTextln(Ausgabebuffer);
}
bool SetTag(String tag) {
  bool ret = true;
  int ta;
  extractTimes(tag, ".");
  if (WERTE[0] >= 0) { ta = WERTE[0]; }
  if (daytrue(ta) == false) {
    sprintf(Ausgabebuffer, "\nTag %i ist nicht gültig", WERTE[0]);
    ToHTMLTextln(Ausgabebuffer);
    return false;
  }
  return true;
}

void SetFeiertag(String tag) {
  if (!SetTag(tag)) { return; }
  String p1, p2, p3;
  tag.toUpperCase();
  extractPs(tag, ",");
  p1 = PARAS[0];
  p2 = PARAS[1];
  p3 = PARAS[2];
  Serial.println(p1);
  Serial.println(p2);
  Serial.println(p3);
  for (int z = 0; z < AnzV_Feiertage; z++) {
    if (PROM.vfeiertage[z] == 0) {
      PROM.vfeiertage[z] = WERTE[0];
      FN[z]=p2;
      vnames[z]=p2;
      PROM.isUpDate = true;
      Anz_Feiertage = AnzahlFT();
      break;
    }
  }
}
void DELlastFeiertag() {
  for (int z = AnzV_Feiertage; z >= 0; z--) {
    if (PROM.vfeiertage[z] != 0) {
      PROM.vfeiertage[z] = 0;
      vnames[z]="Misterx";
      PROM.isUpDate = true;
      Anz_Feiertage = AnzahlFT();
      sprintf(Ausgabebuffer, "\nLetzter Feiertag gelöscht!( %i )", z);
      ToHTMLTextln(Ausgabebuffer);
      break;
    }
  }
}

void ChipID() {
  String sn = "THRE." + getChipID();  // Name der Uhr + IP
  sn.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
  ToHTMLTextln(Ausgabebuffer);
  scrollText(Ausgabebuffer, 200, 30, 1, 1);  // Text und Geschwindigkeit (ms pro Schritt)
  delay(2000);
  LEDsClear();
}
void OTAEnable() {
  int vr = FW_VERSION;
  String v;
  v = VersNachHause("/version3");
  int vs = v.toInt();
  if (vs > vr) {
    PROM.otaEnable = 1;
    sprintf(Ausgabebuffer, "\nOTA Fenster kann geöffnet werden! (%i)", vs);
  } else {
    sprintf(Ausgabebuffer, "\nServerVersion : (%i)", vs);
    PlayMP3(154, 0);
  }
  ToHTMLTextln(Ausgabebuffer);
}
int Day(String Ta, String Mo, String Ja = "1951") {  //return: Tag des Jahres
  int mo = Mo.toInt();
  int ta = Ta.toInt();
  int ja = Ja.toInt();
  if (mo < 1 or mo > 12) { return -1; }  // kein gültiges Datum
  if (ta < 1 or ta > 31) { return -1; }
  int today = ta + morest[mo - 1] - 1;
  if (ja % 4 == 0) {
    if (mo > 2) { today += 1; }
  }
  return today;
}

void SETDay(String tag) {
  String ta, mo;
  if (!SetTag(tag)) {
    tagDemo = false;
    return;
  }
  ta = tag.substring(0, 2);
  mo = tag.substring(3, 5);
  int today = Day(ta, mo);
  ta = ta + mo + "51";
  tagDemo = true;
  Demo_TAMOJA = ta;
  Demo_MONTAG = today;
}

bool SETnickname(String na) {
  bool scroll = true;
  bool ServerSave = true;
  String nick, p1, p2, p3, msg;
  na.toUpperCase();
  extractPs(na, ",");
  p1 = PARAS[0];
  p2 = PARAS[1];
  p3 = PARAS[2];
  nick = getNick();
  if (p1 == "HOME") {
    p2.toLowerCase();
    if (p2 == "*") {
      p2 = SERVER_IP_WETEAM;
      ServerSave = false;
    }
    if (p2 == "+") {
      p2 = SERVER_IP_BERG;
      ServerSave = false;
    }
    Protokoll_IP = p2;
    sprintf(Ausgabebuffer, "\nHome ADR gesetzt: %s ", p2.c_str());
    scroll = false;
    putTHREhome(p2);
  } else if (na == "SERV") {
    sprintf(Ausgabebuffer, "\nSpitzname nicht erlaubt: %s", na.c_str());
    scroll = false;
  } else if (nick == "THRE") {
    nick = na;
    putNick(nick);
  } else if (p1 == "NEW") {
    if (p1 == "SERV") {
      sprintf(Ausgabebuffer, "\nSpitzname nicht erlaubt: %s", p2.c_str());
      scroll = false;
    } else {
      putNick(p2);
      nick = p2;
    }
  } else if (p1 == "UPDATE") {
    if (p2 == "NOKEY") {
      sprintf(Ausgabebuffer, "\nUpdate ohne Kontrolle gestartet! %s (%s)", p2.c_str(), p3.c_str());
      scroll = false;
       otaFirmware(p2,p3);
    }
    if (p2 == "TOKIO") {
      sprintf(Ausgabebuffer, "\nUpdate ohne Kontrolle von Tokio gestartet! %s (%s)", p2.c_str(), p3.c_str());
      delay(2000);
      scroll = false;
      otaFirmware(p2,p3);
    }
  } else if (p1 == "MESSAGE") {
    sprintf(Ausgabebuffer, "\nMessage: %s ", p2.c_str());
    scroll = false;
    SavePROT(98, p2);
    push.print("ONLINE?\n");
  } else if (p1 == "IMAGE") {
    sprintf(Ausgabebuffer, "\nImage: %s ", p2.c_str());
    ServerSave = false;
    scroll = false;
    LoadAndShow("/bild", p2, 5000);
  } else if (p1 == "HELP") {
    ServerSave = false;
    scroll = false;
    LoadHelp("/help", p2, 5000);
  } else if (p1 == "SETFF") {
    if (p2 == "BLINK") { SETBlinker(); }
    if (p2 == "PRINT") { SETPrintDetail(); }
    if (p2 == "PIEZO") { SETPiezo(); }
    if (p2 == "DAY") { SETDay(p3); }
    if (p2 == "RESET") { ESP.restart(); }
    if (p2 == "WETTER") { SETWetter(); }
    scroll = false;
  } else if (p1 != "") {
    sprintf(Ausgabebuffer, "\nSpitzname kann nicht geändert werden: %s", nick.c_str());
    scroll = false;
  }
  nick = nick + "\0";
  if (scroll) {
    nick.toUpperCase();
    sprintf(Ausgabebuffer, nick.c_str());
    scrollText(Ausgabebuffer, 100, 30, 1, 1);  // Text und Geschwindigkeit (ms pro Schritt)
  }
  ToHTMLTextln(Ausgabebuffer);
  return ServerSave;
}

void Info() {
  String v, da, hom, nick;
  int ver = FW_VERSION;
  da = "30.08.2022";
  nick = getNick();
  hom = "http://" + Protokoll_IP + HTML_PORT;
  v = VersNachHause("/version3");
  if (v == "0000") { PlayMP3(240, 0, 5000); }
  htmlRumpf=RTL("Version",String(ver));
  htmlRumpf+=RTL("Datum",da);
  htmlRumpf+=RTL("Spitzname",nick);
  htmlRumpf+=RTL("Lizenz","HReinhardt,Uckermark");
  htmlRumpf+=RTL("Zuhause",hom);
  htmlRumpf+=RTL("Serverversion",v);
  //sprintf(Ausgabebuffer, "\nVersion : %i \nDatum : %s \nSpitzname : %s \nLizenz: HReinhardt,Uckermark \nZuhause: %s \nServerversion : %s", ver, da.c_str(), nick.c_str(), hom.c_str(), v.c_str());
  
  //ToHTMLTextln(Ausgabebuffer);
  PlayMP3(250, 0);
}
void QONLINE() {
  String onl = "ONLINE?\n";
  push.print(onl);
  Serial.println(onl);
  sprintf(Ausgabebuffer, onl.c_str());
  ToHTMLTextln(Ausgabebuffer);
}
void PUSHTO(String msg) {
  msg.toUpperCase();
  push.print("SEND:" + msg + ":\n");
  sprintf(Ausgabebuffer, msg.c_str());
  ToHTMLTextln(Ausgabebuffer);
}
void SetOnline() {
  PROM.online++;
  PROM.online = PROM.online % 2;
  PROM.isUpDate = true;
  String z = "jetzt";
  if (PROM.online == 0) {
    z = "nicht";
    disconnectPush();
  }
  sprintf(Ausgabebuffer, "\nVerbindung zum Server %s möglicht!", z);
  ToHTMLTextln(Ausgabebuffer);
}
void Werkseinstellung() {
  PlayMP3(151, 0, 5000);
  bool Tout = waitForTimeout(10000);
  if (Tout) {
    PlayMP3(152, 0);
    ToHTMLTextln("--> Werkseinstellung abgebrochen.");
    return;
  }
  WiFiManager wm;
  wm.resetSettings();
  PROM.sommer = 0;
  PROM.demozeit = 0;
  PROM.demoJahr = 0;
  PROM.lautst = 13;
  PROM.printDetail = 0;
  PROM.nextzufall = 308;
  PROM.piezo = 1;
  PROM.bigben = 1;
  PROM.blinker = 1;
  PROM.brightness = 6;
  PROM.skinnumber = 3;
  PROM.ansage = 1;
  PROM.bbvon = 800;
  PROM.bbbis = 2000;
  PROM.otaEnable = 0;
  for (int z = 0; z < AnzV_Feiertage; z++) {PROM.vfeiertage[z] = 0;}
  PROM.ansagevon = 700;
  PROM.ansagebis = 2100;
  PROM.nachtlichtvon = 2200;
  PROM.nachtlichtbis = 600;
  PROM.dunkel = 1;
  PROM.online = 0;
  SaveEEPROM();
  ToHTMLTextln("--> Werkseinstellung gesetzt.");
}

//EEPROM Routinen

void SaveEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  PutOneInteger(0, PROM.lastDay);
  PutOneInteger(1, PROM.ledtoday);
  PutOneInteger(2, PROM.printDetail);
  PutOneInteger(3, PROM.isUpDate);
  PutOneInteger(4, PROM.piezo);
  PutOneInteger(5, PROM.bigben);
  PutOneInteger(6, PROM.blinker);
  PutOneInteger(7, PROM.sommer);
  PutOneInteger(8, PROM.demozeit);
  PutOneInteger(9, PROM.demoJahr);
  PutOneInteger(10, PROM.nextzufall);
  PutOneInteger(11, PROM.lautst);
  PutOneInteger(12, PROM.brightness);
  PutOneInteger(13, PROM.skinnumber);
  PutOneInteger(14, PROM.ansage);
  PutOneInteger(15, PROM.bbvon);
  PutOneInteger(16, PROM.bbbis);
  PutOneInteger(17, PROM.otaEnable);
  for (int z = 0; z < AnzV_Feiertage; z++) {PutOneInteger(z + 18, PROM.vfeiertage[z]); }
  PutOneInteger(39, PROM.ansagevon);
  PutOneInteger(40, PROM.ansagebis);
  PutOneInteger(41, PROM.nachtlichtvon);
  PutOneInteger(42, PROM.nachtlichtbis);
  PutOneInteger(43, PROM.dunkel);
  PutOneInteger(44, PROM.online);
  EEPROM.commit();
  EEPROM.end();
}

//Serial.println("\nSave EEPROM");
void LoadEEPROM() {
  PROM.lastDay = GetOneInteger(0);
  PROM.ledtoday = GetOneInteger(1);
  PROM.printDetail = GetOneInteger(2);
  PROM.isUpDate = GetOneInteger(3);
  PROM.piezo = GetOneInteger(4);
  PROM.bigben = GetOneInteger(5);
  PROM.blinker = GetOneInteger(6);
  PROM.sommer = GetOneInteger(7);
  PROM.demozeit = GetOneInteger(8);
  PROM.demoJahr = GetOneInteger(9);
  PROM.nextzufall = GetOneInteger(10);
  PROM.lautst = GetOneInteger(11);
  PROM.brightness = GetOneInteger(12);
  PROM.skinnumber = GetOneInteger(13);
  PROM.ansage = GetOneInteger(14);
  PROM.bbvon = GetOneInteger(15);
  PROM.bbbis = GetOneInteger(16);
  PROM.otaEnable = GetOneInteger(17);
  for (int z = 0; z < AnzV_Feiertage; z++) { PROM.vfeiertage[z] = GetOneInteger(z + 18); }
  PROM.ansagevon = GetOneInteger(39);
  PROM.ansagebis = GetOneInteger(40);
  PROM.nachtlichtvon = GetOneInteger(41);
  PROM.nachtlichtbis = GetOneInteger(42);
  PROM.dunkel = GetOneInteger(43);
  PROM.online = GetOneInteger(44);
}

void EraseEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int z = 0; z < EEPROM_SIZE; z++) { EEPROM.writeByte(z, 0); }
  EEPROM.commit();
  EEPROM.end();
  ToHTMLTextln("--> EEPROM gelöscht.");
}

void ShowEEPROM() {
  String a;
  String o = "Nicht erlaubt";
  if (otaRun) { o = "erlaubt"; }

  a=RTL("Blinker",String(PROM.blinker));
  a += RTL("Sommerzeit",String(PROM.sommer));
  a += RTL("BigBen" ,String(PROM.bigben));
  a += RTL("Demozeit",String(PROM.demozeit));
  a += RTL("Demo Jahr",String(PROM.demoJahr));
  a += RTL("printDetail",String(PROM.printDetail));
  a += RTL("Farbindex",String(PROM.skinnumber));
  a += RTL("Lautstärke",String(PROM.lautst));
  a += RTL("Zufall",String(PROM.nextzufall));
  a += RTL("Helligkeit",String(PROM.brightness));
  a += RTL("Ansage",String(PROM.ansage));
  a += RTL("BBvon",String(PROM.bbvon));
  a += RTL("BBbis",String(PROM.bbbis));
  a += RTL("OTA",o);
  a += RTL("Ansage von",String(PROM.ansagevon));
  a += RTL("Ansage bis",String(PROM.ansagebis));
  a += RTL("Nachtlicht von",String(PROM.nachtlichtvon));
  a += RTL("Nachtlicht bis",String(PROM.nachtlichtbis));
  a += RTL("Nachtlicht",String(PROM.dunkel));
  a += RTL("Online",String(PROM.online));
  ToHTMLTextln(a);
}




String DelNull(String s) {
  String ret = "";
  int i;
  s += char(0);
  while (char(s[i]) > 0) {
    ret += s[i];
    i++;
  }
  return ret;
}

void PutOneInteger(int adr, int i) {
  byte ee;
  adr = adr + adr;
  EEPROM.begin(EEPROM_SIZE);
  ee = EEPROM.readByte(adr);
  if (ee != lowByte(i)) {
    EEPROM.writeByte(adr, lowByte(i));
    delay(100);
  }
  adr++;
  ee = EEPROM.read(adr);
  if (ee != highByte(i)) {
    EEPROM.write(adr, highByte(i));
    delay(100);
  }
  EEPROM.commit();
  EEPROM.end();
}
int GetOneInteger(int adr) {
  int a = adr;
  adr = adr + adr;
  int ret, ret1, ret2;
  EEPROM.begin(EEPROM_SIZE);
  ret1 = EEPROM.readByte(adr);
  adr++;
  ret2 = EEPROM.readByte(adr);
  ret = (ret2 << 8) + ret1;
  EEPROM.end();
  return ret;
}
void PutString(int adr, String daten) {
  EEPROM.begin(EEPROM_SIZE);
  byte ee;
  for (int z = 0; z < 32; z++) {
    ee = EEPROM.readByte(adr + z);
    if (ee != daten[z]) { EEPROM.writeByte(adr + z, daten[z]); }
  }
  EEPROM.end();
}
String GetString(int adr) {
  String ret = "";
  byte ee;
  EEPROM.begin(EEPROM_SIZE);
  for (int z = 0; z < 32; z++) {
    ee = EEPROM.readByte(z + adr);
    ret = ret + char(ee);
  }
  EEPROM.end();
  return ret;
}
int IndexFromStundeMinute(String StundeMinute) {
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  if (st > 11) { st = st - 12; };
  int index = (st * 12) + (mi / 5);
  if (index == 0) { index = 144; }
  return index;
}
void AnsageUhrzeit(String StundeMinute) {
  if (keineAnsage(StundeMinute)) { return; }
  while (!digitalRead(busyPin)) { delay(500); }
  delay(200);
  for (int i = 0; i < 24; i++) { Uhrzeit[i] = 0; }  // Ansage der 5 MinutenZeiten auf null stellen
  int index = IndexFromStundeMinute(StundeMinute);
  if (last5Minuten == index) { return; }
  PlayMP3(index, 0);
  last5Minuten = index;
}

// 5x7 Font für Ziffern 0–9
byte digits[10][7] = {
  { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 },  // 0
  { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },  // 1
  { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 },  // 2
  { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 },  // 3
  { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 },  // 4
  { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 },  // 5
  { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 },  // 6
  { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000 },  // 7
  { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 },  // 8
  { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 }   // 9
};
byte BUCHX[50][8] = {
  // 8x8 Font: Großbuchstaben A–Z
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // Space
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 },  // .
  { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00 },  // /
  { 0x1C, 0x22, 0x26, 0x2A, 0x32, 0x22, 0x1C, 0x00 },  // 0
  { 0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x3E, 0x00 },  // 1
  { 0x3C, 0x42, 0x02, 0x0C, 0x30, 0x40, 0x7E, 0x00 },  // 2
  { 0x3C, 0x42, 0x02, 0x1C, 0x02, 0x42, 0x3C, 0x00 },  // 3
  { 0x0C, 0x14, 0x24, 0x44, 0x7E, 0x04, 0x04, 0x00 },  // 4
  { 0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00 },  // 5
  { 0x1C, 0x20, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00 },  // 6
  { 0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00 },  // 7
  { 0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00 },  // 8
  { 0x3C, 0x42, 0x42, 0x3E, 0x02, 0x04, 0x38, 0x00 },  // 9
  { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00 },  // 58  :
  { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x10, 0x20 },  // 59  ;
  { 0x00, 0x06, 0x18, 0x60, 0x18, 0x06, 0x00, 0x00 },  // 60  <
  { 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00 },  // 61  =
  { 0x00, 0x60, 0x18, 0x06, 0x18, 0x60, 0x00, 0x00 },  // 62  >
  { 0x3C, 0x42, 0x02, 0x0C, 0x10, 0x00, 0x10, 0x00 },  // 63  ?
  { 0x3C, 0x42, 0x5A, 0x5A, 0x5A, 0x40, 0x3C, 0x00 },  // 64  @
  { 0x1C, 0x22, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x00 },  // A
  { 0x7E, 0x41, 0x41, 0x7E, 0x41, 0x41, 0x7E, 0x00 },  // B
  { 0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00 },  // C
  { 0x7C, 0x42, 0x41, 0x41, 0x41, 0x42, 0x7C, 0x00 },
  { 0x7F, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7F, 0x00 },
  { 0x7F, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00 },
  { 0x3E, 0x40, 0x40, 0x4F, 0x41, 0x41, 0x3F, 0x00 },
  { 0x41, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x41, 0x00 },
  { 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00 },
  { 0x1F, 0x02, 0x02, 0x02, 0x42, 0x42, 0x3C, 0x00 },
  { 0x41, 0x42, 0x44, 0x78, 0x44, 0x42, 0x41, 0x00 },
  { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7F, 0x00 },
  { 0x41, 0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x00 },
  { 0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x00 },
  { 0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E, 0x00 },
  { 0x7E, 0x41, 0x41, 0x7E, 0x40, 0x40, 0x40, 0x00 },
  { 0x3C, 0x42, 0x81, 0x81, 0x89, 0x42, 0x3D, 0x00 },
  { 0x7E, 0x41, 0x41, 0x7E, 0x44, 0x42, 0x41, 0x00 },
  { 0x3C, 0x42, 0x40, 0x3C, 0x02, 0x42, 0x3C, 0x00 },
  { 0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },  // T
  { 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E, 0x00 },  // U
  { 0x41, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00 },  // V
  { 0x41, 0x41, 0x41, 0x49, 0x55, 0x63, 0x41, 0x00 },  // W
  { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 },  // X
  { 0x81, 0x42, 0x24, 0x18, 0x08, 0x08, 0x08, 0x00 },  // Y
  { 0x7F, 0x02, 0x04, 0x08, 0x10, 0x20, 0x7F, 0x00 }   // Z
};

byte VLED[24][8] = {
  { 1, 2, 0, 0, 0, 0, 0, 0 },            // ES        0, 0
  { 4, 5, 6, 0, 0, 0, 0, 0 },            // IST       0, 1
  { 8, 9, 10, 11, 0, 0, 0, 0 },          // FÜNF      0, 2
  { 22, 21, 20, 19, 0, 0, 0, 0 },        // ZEHN      0, 3
  { 15, 14, 13, 0, 0, 0, 0, 0 },         // VOR       0, 4
  { 23, 24, 25, 26, 0, 0, 0, 0 },        // NACH      0, 5
  { 27, 28, 29, 30, 31, 32, 33, 0 },     // VIERTEL   0, 6
  { 44, 43, 42, 41, 0, 0, 0, 0 },        // HALB      0, 7
  { 40, 39, 38, 0, 0, 0, 0, 0 },         // VOR       1, 0
  { 37, 36, 35, 34, 0, 0, 0, 0 },        // NACH      1, 1
  { 45, 46, 47, 48, 0, 0, 0, 0 },        // EINS      1, 2
  { 52, 53, 54, 55, 0, 0, 0, 0 },        // ZWEI      1, 3
  { 66, 65, 64, 63, 0, 0, 0, 0 },        // DREI      1, 4
  { 59, 58, 57, 56, 0, 0, 0, 0 },        // VIER      1, 5
  { 67, 68, 69, 70, 0, 0, 0, 0 },        // FÜNF      1, 6
  { 73, 74, 75, 76, 77, 0, 0, 0 },       // SECHS     1, 7
  { 88, 87, 86, 85, 84, 83, 0, 0 },      // SIEBEN    2, 0
  { 81, 80, 79, 78, 0, 0, 0, 0 },        // ACHT      2, 1
  { 89, 90, 91, 92, 0, 0, 0, 0 },        // NEUN      2, 2
  { 93, 94, 95, 96, 0, 0, 0, 0 },        // ZEHN      2, 3
  { 97, 98, 99, 0, 0, 0, 0, 0 },         // ELF       2, 4
  { 110, 109, 108, 107, 106, 0, 0, 0 },  // ZWÖLF     2, 5
  { 102, 101, 100, 0, 0, 0, 0, 0 },      // UHR       2, 6
  { 0, 0, 0, 0, 0, 0, 0, 0 }             //           2, 7
};

//   LEDs
byte LED_Matrix[144][3] = {
  { 0b11000000, 0b00000000, 0b00000110 }, { 0b11100100, 0b00000000, 0b00000100 }, { 0b11010100, 0b00000000, 0b00000100 },  // 0.00  0.05  0.10
  { 0b11000010, 0b00100000, 0b00000000 },
  { 0b11011001, 0b00100000, 0b00000000 },
  { 0b11101001, 0b00100000, 0b00000000 },  // 0.15  0.20  0.25
  { 0b11000001, 0b00100000, 0b00000000 },
  { 0b11100101, 0b00100000, 0b00000000 },
  { 0b11010101, 0b00100000, 0b00000000 },  // 0.30  0.35  0.40
  { 0b11000010, 0b10100000, 0b00000000 },
  { 0b11011000, 0b00100000, 0b00000000 },
  { 0b11101000, 0b00100000, 0b00000000 },  // 0.45  0.50  0.55

  { 0b11000000, 0b00100000, 0b00000000 },
  { 0b11100100, 0b00100000, 0b00000000 },
  { 0b11010100, 0b00100000, 0b00000000 },  // 1.00  1.05  1.10
  { 0b11000010, 0b00010000, 0b00000000 },
  { 0b11011001, 0b00010000, 0b00000000 },
  { 0b11101001, 0b00010000, 0b00000000 },  // 1.15  1.20  1.25
  { 0b11000001, 0b00010000, 0b00000000 },
  { 0b11100101, 0b00010000, 0b00000000 },
  { 0b11010101, 0b00010000, 0b00000000 },  // 1.30  1.35  1.40
  { 0b11000010, 0b10010000, 0b00000000 },
  { 0b11011000, 0b00010000, 0b00000000 },
  { 0b11101000, 0b00010000, 0b00000000 },  // 1.45  1.50  1.55

  { 0b11000000, 0b00010000, 0b00000010 },
  { 0b11100100, 0b00010000, 0b00000000 },
  { 0b11010100, 0b00010000, 0b00000000 },  // 2.00
  { 0b11000010, 0b00001000, 0b00000000 },
  { 0b11011001, 0b00001000, 0b00000000 },
  { 0b11101001, 0b00001000, 0b00000000 },
  { 0b11000001, 0b00001000, 0b00000000 },
  { 0b11100101, 0b00001000, 0b00000000 },
  { 0b11010101, 0b00001000, 0b00000000 },
  { 0b11000010, 0b10001000, 0b00000000 },
  { 0b11011000, 0b00001000, 0b00000000 },
  { 0b11101000, 0b00001000, 0b00000000 },

  { 0b11000000, 0b00001000, 0b00000010 },
  { 0b11100100, 0b00001000, 0b00000000 },
  { 0b11010100, 0b00001000, 0b00000000 },  // 3.00
  { 0b11000010, 0b00000100, 0b00000000 },
  { 0b11011001, 0b00000100, 0b00000000 },
  { 0b11101001, 0b00000100, 0b00000000 },
  { 0b11000001, 0b00000100, 0b00000000 },
  { 0b11100101, 0b00000100, 0b00000000 },
  { 0b11010101, 0b00000100, 0b00000000 },
  { 0b11000010, 0b10000100, 0b00000000 },
  { 0b11011000, 0b00000100, 0b00000000 },
  { 0b11101000, 0b00000100, 0b00000000 },

  { 0b11000000, 0b00000100, 0b00000010 },
  { 0b11100100, 0b00000100, 0b00000000 },
  { 0b11010100, 0b00000100, 0b00000000 },  // 4.00
  { 0b11000010, 0b00000010, 0b00000000 },
  { 0b11011001, 0b00000010, 0b00000000 },
  { 0b11101001, 0b00000010, 0b00000000 },
  { 0b11000001, 0b00000010, 0b00000000 },
  { 0b11100101, 0b00000010, 0b00000000 },
  { 0b11010101, 0b00000010, 0b00000000 },
  { 0b11000010, 0b10000010, 0b00000000 },
  { 0b11011000, 0b00000010, 0b00000000 },
  { 0b11101000, 0b00000010, 0b00000000 },

  { 0b11000000, 0b00000010, 0b00000010 },
  { 0b11100100, 0b00000010, 0b00000000 },
  { 0b11010100, 0b00000010, 0b00000000 },  // 5.00
  { 0b11000010, 0b00000001, 0b00000000 },
  { 0b11011001, 0b00000001, 0b00000000 },
  { 0b11101001, 0b00000001, 0b00000000 },
  { 0b11000001, 0b00000001, 0b00000000 },
  { 0b11100101, 0b00000001, 0b00000000 },
  { 0b11010101, 0b00000001, 0b00000000 },
  { 0b11000010, 0b10000001, 0b00000000 },
  { 0b11011000, 0b00000001, 0b00000000 },
  { 0b11101000, 0b00000001, 0b00000000 },

  { 0b11000000, 0b00000001, 0b00000010 },
  { 0b11100100, 0b00000001, 0b00000000 },
  { 0b11010100, 0b00000001, 0b00000000 },  // 6.00
  { 0b11000010, 0b00000000, 0b10000000 },
  { 0b11011001, 0b00000000, 0b10000000 },
  { 0b11101001, 0b00000000, 0b10000000 },
  { 0b11000001, 0b00000000, 0b10000000 },
  { 0b11100101, 0b00000000, 0b10000000 },
  { 0b11010101, 0b00000000, 0b10000000 },
  { 0b11000010, 0b10000000, 0b10000000 },
  { 0b11011000, 0b00000000, 0b10000000 },
  { 0b11101000, 0b00000000, 0b10000000 },

  { 0b11000000, 0b00000000, 0b10000001 },
  { 0b11100100, 0b00000000, 0b10000001 },
  { 0b11010100, 0b00000000, 0b10000001 },  // 7.00
  { 0b11000010, 0b00000000, 0b01000000 },
  { 0b11011001, 0b00000000, 0b01000000 },
  { 0b11101001, 0b00000000, 0b01000000 },
  { 0b11000001, 0b00000000, 0b01000000 },
  { 0b11100101, 0b00000000, 0b01000000 },
  { 0b11010101, 0b00000000, 0b01000000 },
  { 0b11000010, 0b10000000, 0b01000000 },
  { 0b11011000, 0b00000000, 0b01000000 },
  { 0b11101000, 0b00000000, 0b01000000 },

  { 0b11000000, 0b00000000, 0b01000001 },
  { 0b11100100, 0b00000000, 0b01000001 },
  { 0b11010100, 0b00000000, 0b01000001 },  // 8.00
  { 0b11000010, 0b00000000, 0b00100000 },
  { 0b11011001, 0b00000000, 0b00100000 },
  { 0b11101001, 0b00000000, 0b00100000 },
  { 0b11000001, 0b00000000, 0b00100000 },
  { 0b11100101, 0b00000000, 0b00100000 },
  { 0b11010101, 0b00000000, 0b00100000 },
  { 0b11000010, 0b10000000, 0b00100000 },
  { 0b11011000, 0b00000000, 0b00100000 },
  { 0b11101000, 0b00000000, 0b00100000 },

  { 0b11000000, 0b00000000, 0b00100001 },
  { 0b11100100, 0b00000000, 0b00100001 },
  { 0b11010100, 0b00000000, 0b00100001 },  // 9.00
  { 0b11000010, 0b00000000, 0b00010000 },
  { 0b11011001, 0b00000000, 0b00010000 },
  { 0b11101001, 0b00000000, 0b00010000 },
  { 0b11000001, 0b00000000, 0b00010000 },
  { 0b11100101, 0b00000000, 0b00010000 },
  { 0b11010101, 0b00000000, 0b00010000 },
  { 0b11000010, 0b10000000, 0b00010000 },
  { 0b11011000, 0b00000000, 0b00010000 },
  { 0b11101000, 0b00000000, 0b00010000 },

  { 0b11000000, 0b00000000, 0b00010010 },
  { 0b11100100, 0b00000000, 0b00010000 },
  { 0b11010100, 0b00000000, 0b00010000 },  // 10.00
  { 0b11000010, 0b00000000, 0b00001000 },
  { 0b11011001, 0b00000000, 0b00001000 },
  { 0b11101001, 0b00000000, 0b00001000 },
  { 0b11000001, 0b00000000, 0b00001000 },
  { 0b11100101, 0b00000000, 0b00001000 },
  { 0b11010101, 0b00000000, 0b00001000 },
  { 0b11000010, 0b10000000, 0b00001000 },
  { 0b11011000, 0b00000000, 0b00001000 },
  { 0b11101000, 0b00000000, 0b00001000 },

  { 0b11000000, 0b00000000, 0b00001010 },
  { 0b11100100, 0b00000000, 0b00001000 },
  { 0b11010100, 0b00000000, 0b00001000 },  // 11.00
  { 0b11000010, 0b00000000, 0b00000100 },
  { 0b11011001, 0b00000000, 0b00000100 },
  { 0b11101001, 0b00000000, 0b00000100 },
  { 0b11000001, 0b00000000, 0b00000100 },
  { 0b11100101, 0b00000000, 0b00000100 },
  { 0b11010101, 0b00000000, 0b00000100 },
  { 0b11000010, 0b10000000, 0b00000100 },
  { 0b11011000, 0b00000000, 0b00000100 },
  { 0b11101000, 0b00000000, 0b00000100 }
};

int SetPixel(int nr) {
  int bright, led, ret, r, g, b, ww;
  int farbe = PROM.skinnumber;
  if (isNacht(STDMI)) {
    bright = PROM.dunkel;
    if (!nachtON) {
      nachtON = true;
      tagON = false;
      SavePROT(1, "isNacht!");
    }
  } else {
    bright = PROM.brightness;
    if (!tagON) {
      tagON = true;
      nachtON = false;
      SavePROT(1, "isTag!");
    }
  }
  pixels.setBrightness(bright);
  LEDSetrgb(farbe, r, g, b);
  ww = 0;
  for (int i = 0; i < 8; i++) {
    led = VLED[nr][i];
    if (led > 0) {
      ww++;
      pixels.setPixelColor(led - 1, pixels.Color(r, g, b));
      delay(100);
    }
  }
  ret = 0;
  if (ww > 0) {
    String w = Worte[nr] + " ";
    Serial.print(w);
    ret = nr + 1;
  }
  return ret;
}

void SetVLed(byte ix1, byte ix2, byte ix3) {
  byte v;
  int w = 0;
  for (int i = 0; i < 8; i++) {
    v = ix1 & MASKE[i];
    if (not v == 0) { Uhrzeit[i] = SetPixel(i); }
  }
  for (int i = 0; i < 8; i++) {
    v = ix2 & MASKE[i];
    if (not v == 0) { Uhrzeit[i + 8] = SetPixel(i + 8); }
  }
  for (int i = 0; i < 8; i++) {
    v = ix3 & MASKE[i];
    if (not v == 0) { Uhrzeit[i + 16] = SetPixel(i + 16); }
  }
  //Serial.println();
}

// Minute innerhalb 5 Minuten
void ShowMinute(String StundeMinute, int led, int off = 0) {
  int r, g, b;
  int farbe = PROM.skinnumber;
  LEDSetrgb(farbe, r, g, b);
  int minute = StundeMinute.substring(3, 5).toInt();
  minute = minute % 5;
  for (int i = 1; i < 5; i++) {
    if (i <= minute) {
      if (off == 0) { pixels.setPixelColor(led + i - 1, pixels.Color(r, g, b)); }
      if (off == 11) { pixels.setPixelColor(led + off - i, pixels.Color(r, g, b)); }
      delay(100);
    }
  }
}

void AnzeigeMatrix(String StundeMinute) {
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  if (st > 11) { st = st - 12; };
  int indexx = (st * 12) + (mi / 5);
  byte ix1 = LED_Matrix[indexx][0];
  byte ix2 = LED_Matrix[indexx][1];
  byte ix3 = LED_Matrix[indexx][2];
  /*
  if (PROM.printDetail) {
    Serial.print("\nMinutenIndex:[");
    Serial.print(indexx);
    Serial.print(":");
    Serial.print(StundeMinute);
    Serial.print(":");
    Serial.print(ix1, BIN);
    Serial.print("-");
    Serial.print(ix2, BIN);
    Serial.print("-");
    Serial.print(ix3, BIN);
    Serial.print("]--> ");
    Serial.println(last5Minuten);
  }
  */
  SetVLed(ix1, ix2, ix3);
  delay(500);
}

String strMitNull(int d) {  // 12-->12;3-->03
  String ret = String(d);
  int i = ret.length();
  if (i == 1) { ret = "0" + ret; };
  return ret;
}
String strMitNull4(int d) {
  String ret = String(d);
  ret = "0000" + ret;
  ret = ret.substring(ret.length() - 4);  //
  return ret;
}
String changeTaMo(int tag) {
  String ta = strMitNull(tag / 100);
  String mo = strMitNull(tag % 100);
  return ta + mo;
}

int tageDiff(int vonTag, int zuTag) {
  int diff = zuTag - vonTag;

  if (diff > 182) {
    diff -= 365;
  } else if (diff < -182) {
    diff += 365;
  }

  return -diff;
}



bool SetMerker(String TAMOJA, int AnzLEDs) {  // auch für DemoJahr
  
  int diff, f;
  bool ft = false;
  Statusleft = false;
  Statusright = false;
  String tamo;
  String ta = TAMOJA.substring(0, 2);
  String mo = TAMOJA.substring(2, 4);
  int today = Day(ta, mo);
  int null = AnzLEDs / 2 - 1;
  for (int i = 0; i < AnzLEDs; i++) { LEDFTMerker[i] = 0; }
  Holiday_Name="";
  for (int z = 0; z < Anz_Feiertage; z++) {
    f = FT[z];
    tamo = changeTaMo(f);
    ta = tamo.substring(0, 2);
    mo = tamo.substring(2, 4);
    f = Day(ta, mo);
    if (f != -1) {
      diff = tageDiff(today, f);
      //if(PROM.printDetail){Serial.print("\n|");Serial.print(f);Serial.print("|");Serial.print(diff);}
      //diff = today - f;
      if (diff >= -null and diff <= null) {
        //  es ist ein FT, der angezeigt werden sollte
        f = diff + null;  //f noch mal verwenden
        LEDFTMerker[f] = 1; 
        ft = true;
        if (diff==0) {Serial.println(FN[z]);Holiday_Name=FN[z];}
        if (f < 5) { Statusleft = true; }   //  ein FT liegt auf der linken Seite
        if (f > 5) { Statusright = true; }  //  ein FT liegt auf der rechten Seite
      }
    }
  }
  return ft;
}

// Anzeige LED: kommender und verpasster Feiertag
//(0-4 kommender; 5 heute; 6 - 12 verpasster)
void SetLEDs(int today, int led, int AnzLEDs) {
  int farbe, r, g, b;
  bool ft = false;
  bool fttoday = false;
  int null = AnzLEDs / 2 - 1;

  for (int z = 0; z < AnzLEDs; z++) {
    if (LEDFTMerker[z] == 1) {
      ft = true;
      if (z <= null) {
        farbe = 1;
      } else {
        farbe = 5;
      }
      LEDSetrgb(farbe, r, g, b);
      pixels.setPixelColor(led + z, pixels.Color(r, g, b));
      if (z == null) {
        fttoday = true;
        if (LEDFTMerker[z] == 1) {
          if (PROM.blinker == 1) { 
            LEDblink(led + z, farbe);
          } 
        }
      }
    }
  }
  farbe = PROM.skinnumber;
  if (farbe == 1) { farbe = 6; }
  LEDSetrgb(farbe, r, g, b);

  if (!fttoday and ft) { pixels.setPixelColor(led + null, pixels.Color(r, g, b)); }  //HEUTE}}
}

void AnzeigeWochentag(String StundeMinute){
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  if (mi!=5) return;
  WTAG="..."+WTAG;
  WTAG.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
  scroll(Ausgabebuffer);
}
bool IsServer(){                        // gibt es Bild0? auf dem Server
    WiFiClient client;
    HTTPClient http;
    String url = "http://" + Protokoll_IP + HTML_PORT +  "/bild/0/" ;
    http.begin(client, url);
    if (http.GET() != 200) return false;
    return true;
}

bool ShowServer(String StundeMinute){
    bool ret=false;
    String SO;
    if (PROM.online == 0) return ret;
    int st = StundeMinute.substring(0, 2).toInt();
    int mi = StundeMinute.substring(2, 4).toInt();
    if (mi!=35) return ret;
    ret=IsServer();
    if (ret){
      SO="...SERVER ONLINE";
    }else{
      SO="...SERVER OFFLINE"; 
    }
    SO.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
    scroll(Ausgabebuffer);
    return ret;
  }

int waitforDatum(String &DRUCK, String &TIME, String &STDMI) {
  int today = 999;
  int s = 1;
  while (today == 999) {
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    String formattedTime = timeClient.getFormattedTime();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    int currentSecond = timeClient.getSeconds();
    String weekDay = weekDays[timeClient.getDay()];

    //Get a time structure
    struct tm *ptm = gmtime((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon + 1;
    int currentYear = ptm->tm_year + 1900;

    String Mo = strMitNull(currentMonth);
    String Ta = strMitNull(monthDay);
    String Ja = strMitNull(currentYear);
    String St = strMitNull(currentHour);
    String Mi = strMitNull(currentMinute);
    String Si = strMitNull(currentSecond);

    today = currentMonth + monthDay * 100;
    //Print complete date:waitforDatum ntp
    String currentDate = weekDay + " der " + Ta + "." + Mo + "." + String(currentYear) + "  " + formattedTime + " (" + String(Day(Ta, Mo, Ja)) + ".Tag des Jahres)";
    //Serial.println(formattedTime);
    if (currentYear <= 2025 || currentYear >= 2032) {
      today = 999;
      Serial.print("\n⚡");
      Serial.print("NTP:");
      Serial.println(currentDate);
      if (s % 100 == 0) {
        Serial.println();
        SavePROT(99, "NTP ? " + STDMI);
      }
      delay(2000);
      LEDhinher(s, TÜRKIS);
      s += 1;
    } else {
      TAMOJA = Ta + Mo + Ja;
      DRUCK = "\nZeit: 🌟 " + currentDate;
      WTAG= weekDay;
    }
    TIME = formattedTime;
    STDMI = St + Mi;
    MISI = Mi + Si;
  }
  return today;
}

void BigBen(String StundeMinute) {
  static String lastStundeMinute;
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  if (mi != 0) { return; }
  if (lastStundeMinute == StundeMinute) return;
  lastStundeMinute=StundeMinute;
  SavePROT(1, "rennt!");  // jede volle Stunde einen Eintrag ist Protokoll
  // Laufschrift ???
  if (keinBigBen(StundeMinute)) { return; }
  if (PROM.bigben == 0) { return; }
  if (st > 12) { st -= 12; }
  if (st == 0) { st = 12; }
  if (PROM.bigben == 1) { PlayMP3(160 + st, 0); }  // song volume = 0 bedeutet vom PROM einlesen
  if (PROM.bigben == 2) { PlayMP3(180 + st, 0); }
  if (PROM.bigben == 3) { PlayMP3Folge(206, st, 0); }
  if (PROM.bigben == 4) { PlayMP3Folge(207, st, 0); }
}

void SetSommerWinter() {
  if (PROM.sommer) {
    timeClient.setTimeOffset(7200);
  } else {
    timeClient.setTimeOffset(3600);
  }  // Berlin Winter-/Sommerzeit
}

String Demo(String zeit) {
  int st = zeit.substring(0, 2).toInt();
  int mi = zeit.substring(2, 4).toInt();
  mi += 5;
  mi = mi % 60;
  if (mi == 0) {
    st++;
    st = st % 12;
  }
  zeit = strMitNull(st) + strMitNull(mi);
  return zeit;
}

String ZufallsZeit(){
  //randomSeed(nr + millis());
  int nr = random(60);
  int h = random(24);
  return strMitNull(h) + strMitNull(nr);
}

String Zufall(int nr) {
  String ret;
  for (int i = 0; i < 18; i++){
    ret=ZufallsZeit();
    if (!keinKrach(ret)) return ret;
  }
  return ret;
}

void PlayZufall(String StundeMinute) {
  if (keinKrach(StundeMinute)) { return; }
  int nr = random(gags);
  nr += 200;
  PlayMP3(nr, 0);
}

int ZeitMessung() {
  unsigned long differenz;
  endeMessung = millis();
  differenz = endeMessung - startMessung;
  startMessung = millis();
  delay(10);
  return differenz;
}
int ZyklusBis00() {
  // Bremse berechnen um NTP Server zu schonen
  int bis00 = 61 - MISI.substring(2, 4).toInt();
  if (PROM.bigben != 0) { bis00--; }  // wenn BigBen, etws länger
  return bis00;
}

//*************************

void clearScrollBuffer() {
  extern byte buffer[8];
  for (int i = 0; i < 8; i++) buffer[i] = 0;
}

byte getFontIndex(char c) {
  byte myByte = byte(c) - 45;
  if (myByte < 1 or myByte > 46) { myByte = 0; }
  return myByte;
}

void rotate90(int index) {
  byte ix, v;
  for (int i = 0; i < 8; i++) {
    BUFFER[i] = 0;
    for (int j = 0; j < 8; j++) {
      ix = BUCHX[index][j];
      v = ix & MASKE[i];
      if (not v == 0) { BUFFER[i] = BUFFER[i] | MASKE[j]; }
    }
  }
}
void scroll(String t) {
  t.toUpperCase();
  t = " " + t + " ";
  t.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
  scrollText(Ausgabebuffer, 100, 30, 1, 1);
}

void setScrollColorIndex(byte f) {
  currentColorIndex = f;
}

void scrollText(const char *msg, int speed, int switchPos, byte color1, byte color2) {
  memset(scrollBuffer, 0, 8);
  int charIndex = 0;

  for (int i = 0; msg[i] != '\0'; i++) {
    byte index = getFontIndex(msg[i]);
    rotate90(index);

    // Farbwahl je nach Zeichenposition
    if (charIndex < switchPos)
      setScrollColorIndex(color1);
    else
      setScrollColorIndex(color2);

    for (int col = 0; col < 8; col++) {
      scrollColumn(BUFFER[col]);
      delay(speed);
    }

    // Abstand
    scrollColumn(0x00);
    delay(speed);

    charIndex++;
  }
}

// Scrollt eine Spalte rein
void scrollColumn(byte colData) {
  int r, g, b;

  for (int row = 0; row < 8; row++) {
    scrollBuffer[row] <<= 1;
    if (colData & (1 << (7 - row))) {
      scrollBuffer[row] |= 1;
    }
  }

  pixels.clear();
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (scrollBuffer[y] & (1 << (7 - x))) {
        LEDSetrgb(currentColorIndex, r, g, b);
        pixels.setPixelColor(getPixelNumber(x, y + 2), pixels.Color(r, g, b));
      }
    }
  }
  pixels.show();
}

//*****************

void LEDCounter(int von, int bis) {
  int step = (von < bis) ? 1 : -1;
  for (int i = von; i != bis + step; i += step) {
    byte tens = i / 10;
    byte ones = i % 10;
    displayTwoDigits(tens, ones);
    delay(1000);
  }
}

void displayTwoDigits(byte d1, byte d2) {
  pixels.clear();
  for (int y = 0; y < 7; y++) {
    byte row1 = digits[d1][y];
    byte row2 = digits[d2][y];
    for (int x = 0; x < 5; x++) {
      if (row1 & (1 << (4 - x))) {
        int idx = getPixelNumber(x, y + 2);                  // etwas nach unten zentriert
        pixels.setPixelColor(idx, pixels.Color(0, 150, 0));  // Grün
      }
      if (row2 & (1 << (4 - x))) {
        int idx = getPixelNumber(x + 6, y + 2);  // zweites Digit nach rechts
        pixels.setPixelColor(idx, pixels.Color(0, 150, 0));
      }
    }
  }
  pixels.show();
}

// Pixel-Index für Serpentinen-Verdrahtung (Zickzack)
int getPixelNumber(int x, int y) {
  if (y % 2 == 0) {
    return y * 11 + x;
  } else {
    return y * 11 + (11 - 1 - x);
  }
}
// === Mit WLAN verbinden ===
bool connectToWiFi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  lastSSID = ssid;
  prefs.end();

  if (ssid == "") {
    Serial.println("Keine gespeicherten WLAN-Daten.");
    return false;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);
  lastSSID = WiFi.SSID();
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ Verbunden!");
      Serial.print("IP-Adresse: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n❌ Verbindung fehlgeschlagen.");
  return false;
}



void showProgress(int percent) {
  int ledsToLight = (percent * NUMPIXELS) / 100;
  for (int i = 0; i < NUMPIXELS; i++) {
    if (i < ledsToLight)
      pixels.setPixelColor(i, pixels.Color(0, 50, 0));  // grün
    else
      pixels.setPixelColor(i, 0);  // aus
  }
  pixels.show();
}

void otaProgress(int p) {
  showProgress(p);
}

bool otaUpdate(String url, void (*progressCb)(int)) {
HTTPClient http;
WiFiClient client;
http.begin(client, url);
int httpCode = http.GET();
Serial.printf("HTTP Code: %d\n", httpCode);
if (httpCode != HTTP_CODE_OK) {
  Serial.println("Download fehlgeschlagen");
  http.end();
  return false;
}
int contentLength = http.getSize();
Serial.printf("Firmware Größe: %d\n", contentLength);
if (!Update.begin(contentLength)) {
  Serial.println("Update.begin fehlgeschlagen");
  http.end();
  return false;
}
WiFiClient *stream = http.getStreamPtr();
size_t written = Update.writeStream(*stream);
Serial.printf("Geschrieben: %d\n", written);
if (Update.end() && Update.isFinished()) {
  Serial.println("OTA erfolgreich");
  http.end();
  ESP.restart();
  return true;
}
Serial.println("OTA fehlgeschlagen");
http.end();
return false;
}


void  OHNEotaUpdate(String url)
{
    WiFiClientSecure client;
    client.setInsecure();   // GitHub Zertifikat ignorieren


HTTPClient http;
http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
http.begin(client, "https://github.com/helmutreinhardt/THRE/releases/latest/download/firmwareV3.bin");

int code = http.GET();

Serial.printf("HTTP Code=%d\n", code);

if (code == HTTP_CODE_MOVED_PERMANENTLY ||
    code == HTTP_CODE_FOUND ||
    code == HTTP_CODE_TEMPORARY_REDIRECT)
{
    Serial.println(http.getLocation());
}
Serial.println(http.getString());
http.end();

    Serial.println("Load firmware:");
    Serial.println(url);

    t_httpUpdate_return ret = httpUpdate.update(client, url);
                                          
    switch(ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("Update error (%d): %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No update");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("Update done");
            break;
    }
}


bool otaFirmware(String p2,String p3) {

  int version = FW_VERSION;

  const char* baseRaw = "https://raw.githubusercontent.com/helmutreinhardt/THRE/main/";
  const char* baseRelease = "https://github.com/helmutreinhardt/THRE/releases/latest/download/firmwareV3.bin/";
  const char* baseLocal = "http://192.168.178.64:5000/";
  const char* firmwareName = "firmwareV3.bin";

  String url,pfad,IP;
  pfad="/bin3";
  IP=Protokoll_IP;                        

  if (p2 == "TOKIO") {
    if(p3 !="LATEST"){
      OHNEotaUpdate("https://raw.githubusercontent.com/helmutreinhardt/THRE/main/firmwareV3.bin");
    }
    else{ OHNEotaUpdate(baseRelease);}
  }
  else if (p2 == "NOKEY") {
     url = "http://" + IP+ HTML_PORT+ String(pfad);
    otaUpdate(url,otaProgress);
  }
  else {
    url = String(baseRaw) + firmwareName;
  }
 
  return false;
}
//***************************************************************************************** httpCode
void performOTA(int xx=0) {               
  WiFiClient client;
  HTTPClient http;
  String url,pfad,IP;
  pfad="/bin3";
  IP=Protokoll_IP;                        
  url = "http://" + IP+ HTML_PORT+ String(pfad);
  Serial.print("Verbinde mit OTA-Server: ");
  Serial.println(url);
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    WiFiClient *stream = http.getStreamPtr();

    if (Update.begin(contentLength)) {
      Serial.printf("Starte Update (%d Bytes)...\n", contentLength);

      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        Serial.println("Firmware vollständig geschrieben.");
      } else {
        Serial.printf("Nur %d von %d Bytes geschrieben!\n", written, contentLength);
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("Update erfolgreich! Neustart...");
          delay(1000);
          const char* baseRelease = "https://github.com/helmutreinhardt/THRE/releases/latest/download/";
        } else {
          Serial.println("Update nicht vollständig abgeschlossen!");
        }
      } else {
        Serial.printf("Update-Fehler: %s\n", Update.errorString());
      }
    } else {
      Serial.println("Konnte Update nicht starten!");
    }
  } else {
    Serial.printf("HTTP-Fehler beim OTA: %d\n", httpCode);
  }
  http.end();
}
//***************************************************************************************** httpCode


void startOTA() {
  bool Tout = false;  // Vorbereitung für Hardware Trigger. Nur starten wenn Taster an der Uhr gedrückt.
  int nr;
  ArduinoOTA.begin();
  PlayMP3(146, 0, 2500);  // UpdateFenster geöffnet
  PlayMP3(158, 0, 6500);  // Taster in 10 Sek.
  Tout = waitForTimeout(10000);
  if (!Tout) {
    PlayMP3(148, 0);  // UpdateFenster fürs Internet geöffnet
    LEDRing(RED, 5, false);
    performOTA();
    delay(5000);
    LEDRing(RED, 5, true);
  } else {
    PlayMP3(152, 0, 3000);
  }
  PlayMP3(147, 0, 5000);
  stopOTA();
}

void stopOTA() {
  // OTA beenden, indem man keine OTA-Requests mehr behandelt
  PROM.otaEnable = 0;
  otaRun = false;
}

void OTAInit() {
  //OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
}

//*****************************************************************************

void OTAOhneKey(String HW) {  // Ohne Versionskontrolle und Hardware Trigger
  PlayMP3(159, 0);
  //LEDRing(RED, 5, false);
  performOTA();
  //delay(5000);
  //LEDRing(RED, 5, true);
  //PlayMP3(147, 0, 5000);
  //stopOTA();
}

bool isNacht(String StundeMinute) {
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  int uz = st * 100 + mi;
  if (uz < PROM.nachtlichtbis or uz > PROM.nachtlichtvon) { return true; }
  return false;
}
bool isTag(String StundeMinute) {
  return !isNacht(StundeMinute);
}
bool isEmpty(String zeile) {
  int colonIndex = 0;
  while (colonIndex < zeile.length()) {
    char h1 = zeile.charAt(colonIndex);
    if (h1 > 30) { return false; }
    colonIndex++;
  }
  return true;
}

bool keinKrach(String StundeMinute) {
  bool ret = keineAnsage(StundeMinute) or keinBigBen(StundeMinute);
  return ret;
}
bool keineAnsage(String StundeMinute) {
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  int uz = st * 100 + mi;
  if (uz < PROM.ansagevon or uz > PROM.ansagebis) { return true; }
  return false;
}
bool keinBigBen(String StundeMinute) {
  int st = StundeMinute.substring(0, 2).toInt();
  int mi = StundeMinute.substring(2, 4).toInt();
  if (st * 100 < PROM.bbvon or st * 100 > PROM.bbbis) { return true; }
  return false;
}

bool waitForTimeout(long timeout) {
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (digitalRead(Taster) == LOW) { return false; }
    delay(10);  // Small delay to avoid busy-waiting
  }
  return true;  // Timeout
}


String getChipID() {
  // Chip-ID aus der MAC-Adresse generieren (48 Bit)
  uint64_t chipid = ESP.getEfuseMac();
  char id[20];
  sprintf(id, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(id);
}

String getNick() {
  String nick = "None ";
  prefs.begin("device", false);  // Namespace öffnen
  if (prefs.isKey("nickname")) { nick = prefs.getString("nickname"); }
  prefs.end();
  return nick;
}

void putNick(const String &nick) {
  prefs.begin("device", false);  // Namespace öffnen
  prefs.putString("nickname", nick.substring(0, 5));
  prefs.end();
}
String getTHREhome() {
  String home = Protokoll_IP;
  prefs.begin("device", false);  // Namespace öffnen
  if (prefs.isKey("home")) { home = prefs.getString("home"); }
  prefs.end();
  return home;
}
void putTHREhome(const String &home) {
  prefs.begin("device", false);  // Namespace öffnen
  prefs.putString("home", home);
  prefs.end();
}
void getFNs() {
  prefs.begin("device", true);
  for (int i = 0; i < 20; i++) {
    String key = "vn" + String(i);
    if (prefs.isKey(key.c_str())) {
      vnames[i] = prefs.getString(key.c_str());
    }
    else {
      vnames[i] = "MisterX";
    }
  }
  prefs.end();
  
}

void putFNs(String vnames[20]) {
  prefs.begin("device", false);
  for (int i = 0; i < 20; i++) {
    String key = "vn" + String(i);
    prefs.putString(key.c_str(), vnames[i]);
  }
  prefs.end();
}

void THREid() {
  prefs.begin("device", false);  // Namespace öffnen

  // Prüfen ob Seriennummer existiert
  if (!prefs.isKey("serial")) {
    // Seriennummer = ChipID (automatisch vergeben)
    String chipID = getChipID();
    prefs.putString("serial", "THRE." + chipID);
  } else {
    Serial.print("\n🌱🌟 Seriennummer: ");
    Serial.println(prefs.getString("serial"));
  }
  if (!prefs.isKey("nickname")) {
    prefs.putString("nickname", "THRE");
  } else {
    Serial.print("😊🌟 Spitzname: ");
    Serial.println(prefs.getString("nickname"));
  }
  if (!prefs.isKey("home")) {
    prefs.putString("home", Protokoll_IP + HTML_PORT);
  } else {
    Serial.print("😊🌟 HOME: ");
    Serial.println(prefs.getString("home"));
  }
  if (!prefs.isKey("vn0")) {
    prefs.putString("vn0", vnames[0]);
  } else {
    Serial.print("😊🌟 vn0: ");
    Serial.println(prefs.getString("vn0"));
  }
  prefs.end();
}


bool daytrue(int tag) {
  bool ret = false;
  int m = tag % 100;
  int t = tag / 100;
  if (m < 1 || m > 12) { return ret; }
  int tmax = tarest[m - 1];
  if (t < 1 || t > tmax) { return ret; }
  return true;
}

bool timetrue(int ti) {
  bool ret = false;
  int mi = ti % 100;
  int st = ti / 100;
  if (st < 0 || st > 24) { return ret; }
  if (mi < 0 || mi > 59) { return ret; }
  return true;
}

int intNachKomma(const String &text) {
  int pos = text.indexOf(',');
  if (pos != -1) {
    String t = text.substring(pos + 1);
    pos = t.toInt();
  }
  return pos;  // Kein Komma gefunden, wert=0
}


// Uhrzeiten im Format HH:MM extrahieren
void extractTimes(String str, String tr) {
  int colonIndex = 0;
  int ian = 0;
  for (int z = 0; z > 5; z++) { WERTE[z] = -1; }
  while ((colonIndex = str.indexOf(tr, colonIndex)) != -1) {
    // Versuch, 2 Ziffern vor dem ':' zu holen (für HH)
    if (colonIndex >= 2 && colonIndex + 2 < str.length()) {
      char h1 = str.charAt(colonIndex - 2);
      char h2 = str.charAt(colonIndex - 1);
      char m1 = str.charAt(colonIndex + 1);
      char m2 = str.charAt(colonIndex + 2);

      // Prüfen, ob es Ziffern sind
      if (isDigit(h1) && isDigit(h2) && isDigit(m1) && isDigit(m2)) {
        String time = "";
        time += h1;
        time += h2;
        time += m1;
        time += m2;
        WERTE[ian] = time.toInt();
        ian++;
        colonIndex += 3;  // weiter suchen hinter der gefundenen Uhrzeit
        continue;
      }
    }
    colonIndex++;  // kein Treffer, nächsten Doppelpunkt suchen
    if (ian > 5) { return; }
  }
}


//****************************************************************

String ProtZeile(int index) {
  String a;
  a = String(Protokoll[index].mac) = getChipID() + ":";
  a += strMitNull4(Protokoll[index].lfdNr) + " ";
  a += strMitNull4(Protokoll[index].tag) + " ";
  a += strMitNull4(Protokoll[index].uhrzeit) + " ";
  a += strMitNull4(FW_VERSION) + " ";
  a += strMitNull(Protokoll[index].gesendet) + " ";
  a += strMitNull(Protokoll[index].fktnr) + " ";
  a += String(Protokoll[index].text) + "\n";
  return a;
}

int NachHause(String url, String daten, String &antwort) {
  HTTPClient http;
  if (PROM.printDetail == 1) {
    Serial.print("\nNachHause Url: ");
    Serial.print(url);
    Serial.print(" Daten : ");
    Serial.print(daten);
  }
  http.begin(url);
  http.addHeader("Content-Type", "text/plain");
  int httpResponseCode = http.POST(daten);
  if (httpResponseCode > 0) {
    antwort = http.getString();
    if (PROM.printDetail == 1) { Serial.print("\nAntwort: " + antwort); }
  }
  http.end();
  delay(500);
  return httpResponseCode;
}

void ProtNachHause(const char *pfad) {
  String url, zeile, antwort;
  int httpResponseCode;
  if(!datenfree) {datenfree=true;return;}
  for (int i = PointerLogOut; i < PointerLogIn; i++) {  // Kopfzeile bleibt frei
    zeile = ProtZeile(i);
    url = "http://" + Protokoll_IP + HTML_PORT + String(pfad);
    httpResponseCode = NachHause(url, zeile, antwort);
    if (httpResponseCode > 0) {
      PointerLogOut += 1;
    } else {
      //if (PROM.printDetail == 1) { Serial.printf("\nUrl: %s  ResponseCode: %d ", url.c_str(), httpResponseCode); }
      break;
    }
    delay(3000);
  }

  if (PointerLogOut >= PointerLogIn) {
    PointerLogIn = 0;
    PointerLogOut = 0;
  }
}

String VersNachHause(const char *pfad) {
  String zeile = "v";
  String antwort;
  int httpResponseCode;
  String url = "http://" + Protokoll_IP + HTML_PORT + String(pfad);
  httpResponseCode = NachHause(url, zeile, antwort);
  if (httpResponseCode > 0) {
    Serial.print("\nServerVersion: " + antwort);
    return antwort;
  }
  return "0000";
}

static void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1700000000) {  // any "recent enough" epoch works
    delay(250);
    now = time(nullptr);
  }
}
void connectPush() {
  if (PROM.online == 0) return;
  push.connect("192.168.178.64", 6000);  // 1️⃣ TCP-Verbindung
  push.print(getChipID());               // 2️⃣ IDENTITÄT
  push.print("\n");
  if (PROM.printDetail) {
    Serial.println("PUSH");
  }
}
void disconnectPush() {
  if (push.connected()) {
    push.stop();  // TCP sauber schließen
    Serial.println("PUSH getrennt");
  }
}

void pushKeepAlive() {
  uint8_t ka = 0x00;
  if (PROM.online == 0) return;
  push.write(ka);
}

void handlePush() {
  if (PROM.online == 0) return;
  if (!push.connected()) {
    connectPush();
    return;
  }
  handlePushReceive();
}

void sendACK(int id) {
  if (PROM.online == 0) return;
  if (!push.connected()) return;
  push.printf("ACK:%d\n", id);
}

void handlePushReceive() {
  String p1, p2;
  int switchpos;
  if (PROM.online == 0) return;
  if (!push.connected()) return;
  if (!push.available()) return;  // nicht auf timeout warten
  String msg = push.readStringUntil('\n');
  msg.trim();
  if (msg.length() == 0) return;


  extractPs(msg, ":");
  p1 = PARAS[1];
  p2 = PARAS[2];
  p2.toUpperCase();
  switchpos = p2.indexOf("VON");
  sendACK(p1.toInt());
  Serial.print("Receive:");
  Serial.println(msg);
  p2.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));

  Serial.print(p2);
  Serial.print(switchpos);
  Serial.println(Ausgabebuffer);

  PlayMP3(145, 0);
  Message = p2;
  delay(2000);
  scrollText(Ausgabebuffer, 100, switchpos, 1, 3);
  ANZEIGE();
}



void ANZEIGE() {
  pixels.clear();
  if (PROM.demoJahr) {
    AnsageUhrzeit(STDMI);
    AnzeigeMatrix(STDMI);
    BigBen(STDMI);
    SetMerker(TAMOJA, DemoAnzLedsStatus);
    SetLEDs(MONTAG, DemoBeginOfStatus, DemoAnzLedsStatus);  // today auf LEDs Feiertage anzeigen
  } else {
    AnsageUhrzeit(STDMI);
    AnzeigeMatrix(STDMI);
    BigBen(STDMI);
    if (tagDemo) {
      TAMOJA = Demo_TAMOJA;
      MONTAG = Demo_MONTAG;
    }
    bool ft = SetMerker(TAMOJA, AnzLedsStatus);
    SetLEDs(MONTAG, BeginOfStatus, AnzLedsStatus);  // today auf LEDs Feiertage anzeigen
    if (!ft) {
      ShowMinute(STDMI, BeginOfStatus);
    } else if (!Statusleft) {
      ShowMinute(STDMI, BeginOfStatus, 0);
    } else if (!Statusright) {
      ShowMinute(STDMI, BeginOfStatus, 11);
    }
    if (ZMinute == STDMI) {
      if (PROM.printDetail) {
        Serial.print("ZM erreicht:");
        Serial.print(ZMinute);
      }
      PlayZufall(STDMI);  // Zufallsminute erreicht, spiele ZufallsMP3 ab und neue minute vorbereiten
      ZMinute = "0000";
    }
  }

  pixels.show();
}

//****************************************************************
void setup() {
  int Success;
  String nick;
  PointerLogIn = 0;
  PointerLogOut = 0;
  Serial.begin(115200);
  delay(3000);
  pinMode(Taster, INPUT_PULLUP);
  delay(300);
  pinMode(busyPin, INPUT_PULLUP);
  delay(300);
  //ShowSpeicher();
  //delay(300);

  Serial.print("\nInit Pins :🌟 ");
  delay(300);

  mySerial.begin(9600);
  delay(500);
  mp3_set_serial(mySerial);  //set softwareSerial for DFPlayer-mini mp3 module
  delay(300);
  LoadEEPROM();
  delay(1000);
  PlayStop();
  Serial.print("\nInit MP3 :🌟 ");
  delay(300);
  Serial.print("\nInit EEPROM :🌟 ");
  delay(300);
  pixels.begin();
  delay(300);
  Serial.print("\nInit LEDs :🌟 ");
  LEDsClear();
  delay(300);
  PlayMP3(159, 0);
  if (digitalRead(Taster) == LOW) { Werkseinstellung(); }
  LEDRing(GREEN);
  LEDsClear();
  getFNs();
  Anz_Feiertage = AnzahlFT();
  Serial.println("\nSuche WiFi :📲");
  WiFi.disconnect(true);
  delay(300);  // optional
  WiFi.mode(WIFI_STA);
  delay(12000);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("ESP32_Uhr_Setup", "12345678")) {
    PlayMP3(150, 0, 17000);
    Serial.println("❌ Verbindung fehlgeschlagen.");
    delay(3000);
    ESP.restart();  //  🛠️📲🌟🚀😊🧩📦🧠🔶🔁🧰🧪⚠️📌⚡💡🔌👇🔄🐍🔍🎛️🔊📜👉 📶🎵🌱✨🖼️
  }

  Serial.print("✅🌟 Verbunden mit: ");
  Serial.println(WiFi.SSID());
  lastSSID = WiFi.SSID();
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);


  OTAInit();
  ArduinoOTA.setHostname("THRE");
  ArduinoOTA.setPassword("admin");
  delay(100);
  ArduinoOTA.begin();

  // Initialize a NTPClient to get time
  timeClient.begin();
  Serial.println("NTP Client wird gesucht!");
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdateFinished, handleUpdateUpload);
  server.on("/setZustand", HTTP_POST, handleSETZustand);
  server.on("/labels", HTTP_GET, handleLabels);
  server.begin();
  IPAddress ip = WiFi.localIP();
  lastIP = ip.toString();
  //THREid();
  nick = getNick();
  Protokoll_IP = getTHREhome();
  
  
  String ipString = nick + ip.toString() + " ";  // Name der Uhr + IP
  ipString.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
  SetSommerWinter();
  LEDCounter(3, 0);
  scrollText(Ausgabebuffer, 100, 5, 1, 2);  // Text und Geschwindigkeit (ms pro Schritt) mit Farbwechsel
  last5Minuten = 0;
  PlayMP3(145, 0);
  delay(3000);

  attachInterrupt(digitalPinToInterrupt(Taster), TasterUnterbricht, FALLING);  // Hier findet die Einbindung unseres Interrupt-Befehls statt

  // shortBeep(3);                    // 🔔 3 kurze Starttöne
  randomSeed(millis());
  LEDRing(TÜRKIS);
  delay(5000);
  LoadAndShow("/bild", "0", 5000);
 
  connectPush();
}
//****************************************************************
void loop() {
  bool ft;
  uint8_t keepalive = 0x00;  // ich bin noch da!

  if (FirstLoop) {
    FirstLoop = false;
    SavePROT(0, "Start");
    delay(3000);
  }
  MONTAG = waitforDatum(DRUCK, TIME, STDMI);  //warten auf gültiges Datum = Monat/Tag; DRUCK TIME STDMI werden verändert

  BEGTime = millis();

  if (PROM.demozeit == 1) {
    Demozeit = Demo(Demozeit);
    STDMI = Demozeit;
    BREMSE = 5;
  } else {
    BREMSE = ZyklusBis00();  // Bremse, um den NTP Server zu entlasten und um zu der vollen Minute zu kommen
  }

  TEMP = Zufall(STDMI.toInt());


  if (ZMinute == "0000") {
    ZMinute = TEMP;
    PROM.nextzufall = ZMinute.toInt();  // zufallszahl= stunde.minute  speichern
    PROM.isUpDate = true;
  }

  if (PROM.printDetail) {
    Serial.print("STMI: ");
    Serial.print(STDMI);
    Serial.print(" TAMO: ");
    Serial.println(MONTAG);
  }
  Serial.print(DRUCK);
  Serial.print("  ");
  pixels.clear();

  if (PROM.demoJahr) {
    AnsageUhrzeit(STDMI);
    AnzeigeMatrix(STDMI);
    BigBen(STDMI);
    SetMerker(TAMOJA, DemoAnzLedsStatus);
    SetLEDs(MONTAG, DemoBeginOfStatus, DemoAnzLedsStatus);  // today auf LEDs Feiertage anzeigen
  } else {
    AnsageUhrzeit(STDMI);
    AnzeigeWochentag(STDMI);
    ShowServer(STDMI);
    //TAMOJA="300851";
    ft = SetMerker(TAMOJA, AnzLedsStatus);
    if (ft){
      Holiday_Name.toUpperCase();
      Holiday_Name.toCharArray(Ausgabebuffer, sizeof(Ausgabebuffer));
      scrollText(Ausgabebuffer, 100, 30, 1, 1);  // Text und Geschwindigkeit (ms pro Schritt) mit Farbwechsel
      Holiday_Name="";
      pixels.clear();
    }
    AnzeigeMatrix(STDMI);
    BigBen(STDMI);
    if (tagDemo) {
      TAMOJA = Demo_TAMOJA;
      MONTAG = Demo_MONTAG;
    }

    SetLEDs(MONTAG, BeginOfStatus, AnzLedsStatus);  // today auf LEDs Feiertage anzeigen
    if (!ft) {
      ShowMinute(STDMI, BeginOfStatus);
    } else if (!Statusleft) {
      ShowMinute(STDMI, BeginOfStatus, 0);
    } else if (!Statusright) {
      ShowMinute(STDMI, BeginOfStatus, 11);
    }
    if (ZMinute == STDMI) {
      if (PROM.printDetail) {
        Serial.print("ZM erreicht:");
        Serial.print(ZMinute);
      }
      PlayZufall(STDMI);  // Zufallsminute erreicht, spiele ZufallsMP3 ab und neue minute vorbereiten
      ZMinute = "0000";
    }
  }

  pixels.show();

  ProtNachHause("/daten");

  BEGTime = millis()-BEGTime;
  long ian=BREMSE+2-(BEGTime/1000);                      // 2 Sekunden sicherheitshalber
  if (FORTime > 1500){ ian=ian/(FORTime/1000)+1;}        //  FORTime lange, wenn  Push Server nicht läuft 
  if (PROM.printDetail) {
    Serial.print("\nBremse:");Serial.println(BREMSE);
    Serial.print("BEGTime:");Serial.println(BEGTime);  //StartTime);
    Serial.print("FORTime:");Serial.println(FORTime);
    Serial.print("Anzahl:");Serial.println(ian);
  }
  for (int i = 1; i < ian; i++) {  // Programm läuft in dieser Schleife und wartet auf einen Interupt
    FORTime= millis();
    delay(1000);                          // BREMSE in milliSekunden

    if (i % 10 == 0) {
      pushKeepAlive();
    }

    handlePush();

    if (otaRun) {
      ArduinoOTA.handle();
      if (millis() - otaStartTime > otaDuration) { stopOTA();}
    }
    if (PROM.otaEnable == 1 and !otaRun) {
      PROM.otaEnable = 0;
      startOTA();
    }

    server.handleClient();

    if (interruptTriggered) {           // Taster
      interruptTriggered = false;
      bool busy = digitalRead(busyPin);
      if (busy == LOW) {
        mp3_stop();
      } else {
        if (Message != "") {
          PlayMP3(145, 0);
          scroll(Message);
        }
      }
    }

    if (PROM.isUpDate or MONTAG != PROM.lastDay) {
      PROM.lastDay = MONTAG;
      PROM.isUpDate = false;
      detachInterrupt(digitalPinToInterrupt(Taster));  // kein Interupt während SaveEEPROM
      SaveEEPROM();
      putFNs(vnames);
      attachInterrupt(digitalPinToInterrupt(Taster), TasterUnterbricht, FALLING);  // Interupt wieder möglich
      Anz_Feiertage = AnzahlFT();                                                  // Neuberechnung falls ein FT eingegeben wurde
      pixels.clear();
      Blinky(1);  // Leds blinken --> EEPROM wurde programmiert
      pixels.clear();
      AnzeigeMatrix(STDMI);
      ft = SetMerker(TAMOJA, AnzLedsStatus);
      SetLEDs(MONTAG, BeginOfStatus, AnzLedsStatus);
      if (!ft) {
        ShowMinute(STDMI, BeginOfStatus);
      } else if (!Statusleft) {
        ShowMinute(STDMI, BeginOfStatus, 0);
      } else if (!Statusright) {
        ShowMinute(STDMI, BeginOfStatus, 11);
      }
      pixels.show();
    }
    FORTime=(millis()-FORTime);
  }
  
  attachInterrupt(digitalPinToInterrupt(Taster), TasterUnterbricht, FALLING);  // Interupt wieder möglich sicherheitshalber
}
