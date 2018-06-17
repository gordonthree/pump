#include <ArduinoOTA.h>
#include <EasyNTPClient.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include <Hash.h>
#include <EEPROM.h>

//  I2C device address is 0 1 0 0   A2 A1 A0
#define MCP 0x20
#define VAC 0x12
#define VACONOFF 0x10
#define VACLOW 0x14
#define VACHI 0x15
#define OFF 0x0
#define ON 0x1
#define IODIR 0x00
#define GPIO 0x09
#define ADCAN7 0x61
#define ADCAN6 0x62
#define ADCVREF 0x63
#define ADCVBATT 0x64

const int timeZone = -4;  // Eastern Daylight Time (USA)
const char* ssid = "Tell my WiFi I love her";
const char* password = "2317239216";
const char* mqtt_server = "192.168.2.30";
const char* mqttbase = "home/pump01";
const char* mySub = "home/pump01/cmd";
const char* clientid = "pump01";

long lastMsg = 0;
char msg[200];
int value = 0;
double celsius;
int vpres = 0;
int vref = 0;
int freespace = 0;
int reporting = 0;
int vstart = 200;
int vend = 300;
int vdur = 8;
int vrest = 5;
int vreps = 4;
int vmin = 0;
int vmax = 0;
int vdiff = 0;
int vinterval = 0;
int pumpEnable = 0;
uint8_t vrunning = 0;
char i2cbuff[16];
int thePres = 0;

unsigned int pumpSpecs[8];
uint8_t clientCon = false;
char txtMsg[32];
uint8_t ledState = 0;
char theURL[200];
uint8_t setReset = false;
uint8_t setPolo = false;
uint8_t sendTxt = false;
uint8_t mqttControl = false;
uint8_t sendPres = false;
uint8_t sendUptime = true;
uint16_t loopCnt = 0; // update on first run

long bootTime = 0, vstartTime = 0, vendTime = 0, vlastInt = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);
ESP8266WebServer server(80);

WebSocketsServer webSocket = WebSocketsServer(81);

#define USE_SERIAL Serial

WiFiUDP udp;

EasyNTPClient ntpClient(udp, "us.pool.ntp.org");

void eeByteWrite(uint16_t address, uint8_t val) {
  // nothing special here, function is just cosmetic
  EEPROM.write(address, val);
  EEPROM.commit();
}

void eeWordWrite(uint16_t address, uint16_t val) {
  // function writes 2 bytes to eeprom starting at address
  byte lowB = lowByte(val);
  byte hiB = highByte(val);
  EEPROM.write(address, lowB);
  EEPROM.write(address + 1, hiB);
  EEPROM.commit();
}

uint8_t eeByteRead(uint16_t address) {
  // function just cosmetic
  uint8_t val = EEPROM.read(address);
  return val;
}

uint16_t eeWordRead(uint16_t address) {
  // reads two bytes from eeprom, returns them as a word
  uint16_t val = 0;
  uint8_t lowB = EEPROM.read(address);
  uint8_t hiB = EEPROM.read(address+1);
  val = (hiB << 8) | lowB; // reassemble word
  return val;
}

void eeStrWrite(uint16_t address, char* str, uint8_t len) {
  // write a C string of len bytes to eeprom
  for (byte i=0; i<len; i++) {
    EEPROM.write(address+i, str[i]);
  }
  EEPROM.commit();
}

char* eeStrRead(uint16_t address, uint8_t len) {
  // return a C string of len bytes from eeprom
  char str[len];

  for (byte i=0; i<len; i++) {
    str[i] = EEPROM.read(address + i);
  }
  return str;
}

void i2c_wordwrite(int address, int cmd, int theWord) {
  //  Send output register address
  Wire.beginTransmission(address);
  Wire.write(cmd); // control register
  Wire.write(lowByte(theWord));  //  send low byte of word data
  Wire.write(highByte(theWord));  //  high byte
  Wire.endTransmission();
}

void i2c_write(int address, int cmd, int data) {
  //  Send output register address
  Wire.beginTransmission(address);
  Wire.write(cmd); // control register
  Wire.write(data);  //  send byte data
  Wire.endTransmission();
}

int i2c_wordread(int address, int cmd) {
  int result;
  int xlo, xhi;

  Wire.beginTransmission(address);
  Wire.write(cmd); // control register
  Wire.endTransmission();

  Wire.requestFrom(address, 2); // request two bytes
  xlo = Wire.read();
  xhi = Wire.read();

  result = xhi << 8; // hi byte
  result = result | xlo; // add in the low byte

  return result;
}

void i2c_readbytes(byte address, byte cmd, byte bytecnt) {

  Wire.beginTransmission(address);
  Wire.write(cmd); // control register
  Wire.endTransmission();

  Wire.requestFrom(address, bytecnt); // request cnt bytes
  for (byte x = 0; x < bytecnt; x++) {
    i2cbuff[x] = Wire.read();
  }
}


void i2c_scan() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C Bus...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Device found at 0x");
      if (address<16)
      Serial.print("0");
      Serial.print(address,HEX);
      Serial.println();

      nDevices++;
    }
  }

}

long getTime() {
  long theTime=0;
  theTime = ntpClient.getUnixTime();

  return theTime;
}

void socketTxt(char* str, int num) {
  char buf[100];
  sprintf(buf, str, num);
  if (clientCon) {
    webSocket.sendTXT(0, buf);
  }
}

void mqttPrintStr(char* _topic, char* myStr) {
  char myTopic[64];
  sprintf(myTopic, "%s/%s", mqttbase, _topic);
  mqtt.publish(myTopic, myStr);
}

void mqttPrintInt(char* myTopic, int myNum) {
  char myStr[8];
  sprintf(myStr, "%d", myNum);
  mqttPrintStr(myTopic, myStr);
}

void doReset() { // reboot on command
      mqttPrintStr("msg", "Rebooting!");
      socketTxt("Rebooting!", 0);
      delay(50);
      ESP.reset();
      delay(5000); // allow time for reboot
}

void setPump(int pumpmin, int pumpmax, int pumppwr) {

  if (pumppwr != 0) {
    socketTxt("pumping=%d", ((pumpmax + pumpmin) / 2));
  }

  i2c_wordwrite(VAC, VACLOW, pumpmin);
  i2c_wordwrite(VAC, VACHI, pumpmax);
  i2c_wordwrite(VAC, VACONOFF, pumppwr);
}

bool loadFromSpiffs(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.html";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

void handleNotFound(){
  if(loadFromSpiffs(server.uri())) return;
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  // Serial.println(message);
}

void readId(byte address) {
  i2c_readbytes(address, 0x65, 6);
  for (byte x = 0; x < 6; x++) {
    // Serial.print((char)i2cbuff[x]);
  }
}

void saveSettings() {
  eeWordWrite(0, vstart);
  eeWordWrite(2, vend);
  eeByteWrite(4, vdur);
  eeByteWrite(5, vrest);
  eeByteWrite(6, vreps);
  socketTxt("settings saved",0);
}

void loadSettings() {
  vstart = eeWordRead(0);
  vend   = eeWordRead(2);
  vdur   = eeByteRead(4);
  vrest  = eeByteRead(5);
  vreps  = eeByteRead(6);
  socketTxt("settings loaded", 0);
}

void printUptime() {
  sendUptime = false;
  socketTxt("boottime=%d", bootTime);
  mqttPrintInt("boottime", bootTime);
}

void printSettings() {
  char settings[100];
  sprintf(settings, "settings=%d,%d,%d,%d,%d", vstart, vend, vdur, vrest, vreps);
  webSocket.sendTXT(0, settings);
}

void printRSSI() {
  int rssi = WiFi.RSSI();
  socketTxt("rssi=%d", rssi);
  mqttPrintInt("rssi", rssi);
}

void printVBAT() {
  int vBat = i2c_wordread(VAC, ADCVBATT);
  socketTxt("bat=%d", vBat);
  mqttPrintInt("bat", vBat);
}

void handleMsg(char* cmdStr) { // handle commands from mqtt or websockets
  String cmdTxt = String(strtok(cmdStr, "="));
  String cmdVal = String(strtok(NULL, "="));

  //webSocket.sendTXT(0, cmdStr);

  if ((cmdTxt != "action") && (vrunning == 0)) { // action command uses text values, don't try to change it to integer
    int i = cmdVal.toInt();
    if      (cmdTxt == "vstart") vstart = i;
    else if (cmdTxt == "vend")   vend = i;
    else if (cmdTxt == "vdur")   vdur = i;
    else if (cmdTxt == "vrest")  vrest = i;
    else if (cmdTxt == "vreps")  vreps = i;
    else if (cmdTxt == "pres")   sendPres = true;
    else if (cmdTxt == "marco")  setPolo = true;
    else if (cmdTxt == "vmin")   vmin = i;
    else if (cmdTxt == "vmax")   vmax = i;
    else if (cmdTxt == "vpwr") {
      if (i==1) {
        mqttControl = true;
        setPump(vmin, vmax, 1);
        strcpy(txtMsg, "pump on");
      } else {
        mqttControl = false;
        setPump(0, 0, 0);
        strcpy(txtMsg, "pump off");
      }
    }
  } else { // handle action commands here
    if (cmdVal == "Start") {
      vstartTime = getTime(); // record start time
      vlastInt = getTime();
      vrunning = 1;

      socketTxt("session=start", 0);
      saveSettings(); // record these pump settings
    } else

    if (cmdVal == "Stop") { // stop session
      vrunning = 0;
      socketTxt("session=stop", 0);
    } else

    if (cmdVal == "Pause") { // pause session
      vendTime = getTime() - 10; // fake end of rep/session
      socketTxt("session=pause", 0);
    }
    else if (cmdVal == "Reboot") doReset(); // reboot controller
    else if (cmdVal = "Save")    saveSettings(); // save to eeprom
    else if (cmdVal = "Load")    loadSettings(); // load from eeprom
    else if (cmdVal = "Print")   printSettings(); // print config
  }
}

void callback(char* topic, byte* payload, unsigned int len) {
  char tmp[200];
  strncpy(tmp, (char*)payload, len);
  tmp[len] = 0x00;
  handleMsg(tmp);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(clientid)) {
      Serial.println("connected");
      sendUptime = true;
      // Once connected, publish an announcement...
      mqttPrintStr("msg", "Hello, world!");
      // ... and resubscribe
      mqtt.subscribe(mySub);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            //USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                //USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

                // send message to client
                webSocket.sendTXT(num, "Connected");
                socketTxt("time=%d", getTime());
                sendUptime = true;
                printSettings();

                clientCon = true;
            }
            break;
        case WStype_TEXT:
            //USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
            //USE_SERIAL.println();
            //memcpy(action, payload, length); // copy payload to new buffer
            //action[length] = NULL;
            //newMsg = true;
            payload[length] = '\0'; // null terminate
            handleMsg((char *)payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");
            // Serial.println(presStr);
            // webSocket.sendTXT(0, presStr);

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            //USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
            //hexdump(payload, lengtt);

            // send message to client
            // webSocket.sendBIN(num, payload, lenght);
            break;
    }

}




void doPumping() {

  if ((vrunning == 0) && (!mqttControl)) { // stop pumping session
    setPump(0, 0, 0); // stop pump
  }

  if ((vrunning == 1) && (vlastInt > getTime())) { // waiting to restart pumping session
     socketTxt("rest=%d", vlastInt - getTime());
  }

  if ((vrunning == 1) && (vlastInt <= getTime())) { // starting up a pumping session
    vdiff = (vend - vstart) / (vdur - 1);
    vlastInt = getTime() + 60; // add one minute in seconds
    vmin = vstart - vdiff;
    vmax = vstart + vdiff;
    setPump(vmin, vmax, 1);
    vrunning = 2;
    vendTime = getTime() + (vdur * 60); // end time is now + duration seconds
    socketTxt("remain=%d", vendTime - getTime());
    socketTxt("suction=%d", vmax);
  } else {
    if (vrunning == 2) { // pump session already started
      if (getTime() >= vendTime) { // pump session ended
        setPump(0, 0, 0); // stop pump
        if (vreps > 1) { // multiple reps requested
          //socketTxt("rest=%d", vrest);
          vlastInt = getTime() + (vrest * 60); // new start time is now plus rest period in seconds
          socketTxt("rest=%d", vlastInt - getTime());
          vreps--; // one less rep
          socketTxt("repsremain=%d", vreps);
          vrunning = 1; // go back to startup condition
        } else { // no more reps
          socketTxt("session=complete", 0);
          vrunning = 0; // stop
        }
      } else { // pump session continues
        socketTxt("remain=%d", vendTime - getTime());
        if (getTime() >= vlastInt) { // time to increase suction
          vmin = vmin + vdiff;
          if (vmin > vend) vmin = vend;

          vmax = vmin + vdiff;
          if (vmax > vend) vmax = vend;

          socketTxt("suction=%d", vmax);

          setPump(vmin, vmax, 1);
          vlastInt = getTime() + 60; // add one minute in seconds
        }
      }
    }
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  String hostname(clientid);
  WiFi.hostname(hostname);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    // Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }


  server.onNotFound(handleNotFound);


  // start the webserver
  server.begin();
  // udp.begin(localPort);
  ArduinoOTA.setHostname("pump01");


  ArduinoOTA.onStart([]() {
    // Serial.println();
    // Serial.println();
    // Serial.print("Start");
  });
  ArduinoOTA.onEnd([]() {
    // Serial.print("End");
    // Serial.println();
    // Serial.println();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Serial.print(".");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    /* Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) // Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) // Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) // Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) // Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) // Serial.println("End Failed"); */
  });
  ArduinoOTA.begin();
  // Serial.println("OTA is ready.");

}

void setup() {
  // setup 512 bytes of eeprom space
  EEPROM.begin(512);

  Serial.begin(115200);

  Wire.begin(12, 13);


  // ledBlinker.attach(0.5, blinkLed); // start background task of blinking led

  setup_wifi();

  i2c_scan();

  mqtt.setServer(mqtt_server, 1883); // setup mqqt stuff
  mqtt.setCallback(callback);

  Serial.print("Starting webSocket server... ");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("done!");

  SPIFFS.begin();

  loadSettings();

  Serial.println("Pumping controller online");

  bootTime = getTime(); // save the time of last reboot

  if (bootTime==0) {
    delay(1000);
    bootTime = getTime(); // save the time of last reboot
  }
}

void loop() {
  if (!mqtt.connected()) {
    reconnect(); // check mqqt status
  }

  ArduinoOTA.handle();
  // check for incomming client connections frequently in the main loop:
  server.handleClient();
  mqtt.loop();
  webSocket.loop();

  if (!mqttControl) doPumping(); // if not under mqtt control, implement onboard controls

  thePres = i2c_wordread(VAC, ADCAN7);

  socketTxt("pres=%d", thePres);
  socketTxt("time=%d", getTime());

  if (sendPres) {
    mqttPrintInt("pres", thePres);
    sendPres=false;
  }

  if (setPolo) {
    mqttPrintStr("msg", "Polo");
    setPolo=false;
  }

  if (sendTxt) {
    sendTxt = false;
    mqttPrintStr("msg", txtMsg);
  }

  if (setReset) doReset;

  if (sendUptime) printUptime();

  if (loopCnt++ > 150)  { // update every few loops
    loopCnt = 0;
    printRSSI();
    printVBAT();
    mqttPrintInt("time", getTime());
  }

  delay(100);
} // end of main loop
