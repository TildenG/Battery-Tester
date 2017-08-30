
#pragma GCC optimize ("-O2")           // O0 none, O1 Moderate optimization, 02, Full optimization, O3, as O2 plus attempts to vectorize loops, Os Optimize space
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>

extern "C" { 
   #include "user_interface.h"
 } 

WiFiServer server(80);
#define MAX_SRV_CLIENTS 10              // maximum client connections
#define MAX_BATTERIES 8
#define mosfetVoltageDrop 0.015			// average voltage drop across mosfet
#define fanPin D7
#define wifiLEDPin D4
#define tonePin D8

boolean wifiLEDPinState = false;

WiFiClient serverClients[MAX_SRV_CLIENTS];
int currentClient = 0;
String closeConnectionHeader;
String currentDataString;
boolean currentDataStringComplete = true;

struct batteryInformation{
	double currentVoltage=0;
	double currentCurrent=0;
	double totalCurrent=0;
	long startTime = 0;
	double resistorValue = 1.95; // tweak for better accuracy in startTest function
	long stopTime = -1;
	double capacity = -1;
	double totalWattHours = 0;
};
batteryInformation batteryinfo[MAX_BATTERIES];

int batteriesStopped = 0;
boolean startFanDelay = false;
long fanStopAt = 0;

void setup() {
  system_update_cpu_freq(160);
  system_phy_set_max_tpw(35); // 0 - 82 radio TX power
  pinMode(wifiLEDPin,OUTPUT);
  pinMode(fanPin,OUTPUT);
  pinMode(tonePin,OUTPUT);
  Serial.begin ( 2000000 );
  Serial.println("");
  wifi_station_set_hostname("Battery tester");
  startInStationMode();
  long stationModeTimeout = millis() + 15000;
  while ( WiFi.status() != WL_CONNECTED ) {
	if (millis() > stationModeTimeout){
		setupAP();
		Serial.println("Switching to AP mode, IP 192.168.4.1");
		break;
	}
	  wifiLEDPinState = !wifiLEDPinState;
	  analogWrite(wifiLEDPin , 1023 - (wifiLEDPinState * 511));
      delay ( 500 );
      Serial.print ( "." );
    }
	if ( MDNS.begin ( "Tester" ) ) {
		Serial.println ( "\r\n MDNS responder started, type ""Tester.local"" into a web browser" );
	}
	analogWrite(wifiLEDPin , 511);
  Serial.println(WiFi.localIP());
  server.begin();
  SPIFFS.begin();
  testForFiles();
  closeConnectionHeader += F("HTTP/1.1 204 No Content\r\nConnection: Close\r\n\r\n");
  tone(tonePin,500,170);
  delay(171);
  tone(tonePin,1500,170);
  delay(171);
  tone(tonePin,2500,400);
  delay(401);
  digitalWrite(tonePin,LOW);
}

void loop() {
  delay(1); // save power
  checkForClientData();
  checkForSerialData();
  checkFanState();
}

void setupAP()
{
  const char WiFiAPPSK[] = "12345678";
  WiFi.mode(WIFI_AP);
  const char *ssid = "BatteryTester";
  WiFi.softAP(ssid, WiFiAPPSK ,2 ,0); // ssid, password, channel, hide
}

void sendFile(String path){
if(path.endsWith("/")){ path += "index.html";}
String dataType = getContentType(path); // get content type

String gzPath = path + ".gz"; // check if there's a .gz'd version and send that instead
File theBuffer;
if (SPIFFS.exists(gzPath)){             // test to see if there is a .gz version of the file
  theBuffer = SPIFFS.open(gzPath, "r");
  path = gzPath;                        // got it, use this path
}else{                                  // not here so load the standard file
  theBuffer = SPIFFS.open(path, "r");
  if (!theBuffer){                      // this one doesn't exist either, abort.
    theBuffer.close();
    serverClients[currentClient].write( closeConnectionHeader.c_str(),closeConnectionHeader.length() );
    yield();
    return; // failed to read file
  }
}

// make header
String s = F("HTTP/1.1 200 OK\r\ncache-control: max-age = 0\r\ncontent-length: ");
    s += theBuffer.size();
    s += F("\r\ncontent-type: ");
    s += dataType;
    s += F("\r\nconnection: close");
  if (path.endsWith(".gz")){
    s += F("\r\nContent-Encoding: gzip\r\n\r\n");
  }else{
    s += F("\r\n\r\n");
  }
     // send the file
  if( !serverClients[currentClient].write(s.c_str(),s.length()) ){
    // failed to send
    theBuffer.close();
    return;
  }
  int bufferLength = theBuffer.size();
  if ( serverClients[currentClient].write(theBuffer,2920) <  bufferLength){
  }
  yield();
  theBuffer.close();
}
String getContentType(String path){ // get content type
  String dataType = F("text/html");
  String lowerPath = path.substring(path.length()-4,path.length());
  lowerPath.toLowerCase();

  if(lowerPath.endsWith(".src")) lowerPath = lowerPath.substring(0, path.lastIndexOf("."));
  else if(lowerPath.endsWith(".html")) dataType = F("text/html");
  else if(lowerPath.endsWith(".htm")) dataType = F("text/html");
  else if(lowerPath.endsWith(".png")) dataType = F("image/png");
  else if(lowerPath.endsWith(".js")) dataType = F("application/javascript");
  else if(lowerPath.endsWith(".css")) dataType = F("text/css");
  else if(lowerPath.endsWith(".gif")) dataType = F("image/gif");
  else if(lowerPath.endsWith(".jpg")) dataType = F("image/jpeg");
  else if(lowerPath.endsWith(".ico")) dataType = F("image/x-icon");
  else if(lowerPath.endsWith(".xml")) dataType = F("text/xml");
  else if(lowerPath.endsWith(".pdf")) dataType = F("application/x-pdf");
  else if(lowerPath.endsWith(".zip")) dataType = F("application/x-zip");
  else if(lowerPath.endsWith(".gz")) dataType = F("application/x-gzip");
  return dataType;
}
void checkForClientData(){   // client functions here
  while (server.hasClient()){//find free/disconnected spot
    for(uint8_t i = 0; i < MAX_SRV_CLIENTS; i++){
      if (!serverClients[i] || !serverClients[i].connected()){
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        return;
      }
    }
  }
  String req = "";//check clients for data
  for(uint8_t i = currentClient; i < MAX_SRV_CLIENTS; i++){// start at current client to keep in order
    if (serverClients[i] && serverClients[i].connected()){
        if (serverClients[i].available()){
          req = serverClients[i].readStringUntil('\r');   // Read the first line of the request
          serverClients[i].flush();
          currentClient = i;
          break;
        }
    }
    currentClient = 0;
  }
 
  if (!req.length()){// empty request
      return;
      }
  //Serial.println("\r\n" + req);
String fileString = req.substring(4, (req.length() - 9));
        //Serial.println("\r\n" + fileString);
		if (fileString.indexOf("Time") != -1){ //process time
		serverClients[currentClient].write( closeConnectionHeader.c_str(),closeConnectionHeader.length() );
        yield();
		return;
		}
		if (fileString.indexOf("Start") != -1){
		serverClients[currentClient].write( closeConnectionHeader.c_str(),closeConnectionHeader.length() );
        yield();
		startTest();
		return;
		}
		if (fileString.indexOf("Cancel") != -1){
		serverClients[currentClient].write( closeConnectionHeader.c_str(),closeConnectionHeader.length() );
        yield();
		Serial.println("STOP STOP");
		batteriesStopped = MAX_BATTERIES;
		startFanDelay = true;
		fanStopAt = millis() + 5 * 60 * 1000; // 5 minutes
		return;
		}
		if (fileString.indexOf("GetResults") != -1){
			serverClients[currentClient].write( closeConnectionHeader.c_str(),closeConnectionHeader.length() );
			yield();
			//read results file and set if complete
			String dataString = "";
			String theData = "";
			int nextComma=0;
			double fileAh = 0;
			for (int a=0;a<MAX_BATTERIES;a++){
				String fileName = "/resultsFile"+String(a)+".csv";
				File dataFile = SPIFFS.open(fileName, "r");
				if (!dataFile){
					Serial.println("Failed to open/create file");
				}
				//dataString = dataFile.readString();
				dataString ="";
				for (int b=0; b<dataFile.size();b++){
					dataString += char(dataFile.read());
				}
				nextComma = dataString.indexOf(",");
				theData = dataString.substring(0,nextComma);
				fileAh = (long(theData.toFloat()*100000.0) / 100000.0); // round back to 5 dp
				if (fileAh > 0.0001){
					dataString.remove(0,nextComma+1);
					theData = dataString.substring(0,nextComma);
					double fileWh = (long(theData.toFloat()*100000.0) / 100000.0); // round back to 5 dp
					batteryinfo[a].capacity = fileAh;
					batteryinfo[a].totalWattHours = fileWh;
				}
				dataFile.close();
			}
		return;
		}
		  if (fileString.indexOf("empty.html") != -1){ // reset / power down
                String s,h;
				s = F("<!DOCTYPE HTML>\r\n<html>\r\n");
				s += F("<head><meta http-equiv='refresh' content='4' /></head><body><script>");
				s += F("var currentTime = ");
				s += (millis() - batteryinfo[0].startTime)/ 1000;
				s += F(";var bv = [];");
				for (int a=0;a<MAX_BATTERIES;a++){
					s += F("bv.push( ");
					s += String(batteryinfo[a].currentVoltage,4);
					s += F(");");
				}
				s += F("\r\nvar ba = [];");
				for (int a=0;a<MAX_BATTERIES;a++){
					s += F("ba.push( ");
					s += String(batteryinfo[a].currentCurrent,4);
					s += F(");");
				}
				s += F("\r\nvar cap = [];");
				for (int a=0;a<MAX_BATTERIES;a++){
					s += F("cap.push( ");
					s += String(batteryinfo[a].capacity,4);
					s += F(");");
				}
				s += F("\r\nvar wH = [];");
				for (int a=0;a<MAX_BATTERIES;a++){
				s += F("wH.push( ");
				s += String(batteryinfo[a].totalWattHours,4);
				s += F(");");
				}
				s += F(" </script></body></html>\n");
				h = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: Close\r\ncontent-length: ");
				h += s.length();
                h += F("\r\n\r\n");
                String ss = h + s;
                serverClients[currentClient].write(ss.c_str(),ss.length());
				yield();
                return;
          }
          sendFile(fileString);
}
void startInStationMode(){
		WiFi.mode(WIFI_STA);
		delay(1);
        char  ssid[33]  = "NobodySeenMeDoItNobodyCanProveAg";
        char  password[64]  = "ThisIsTheNewKey17.d"; 
        struct  station_config  stationConf;  
        stationConf.bssid_set = 0;            //need  not check MAC address of  AP
        os_memcpy(&stationConf.ssid,  ssid, 33);  
        os_memcpy(&stationConf.password,  password, 64);  
        wifi_station_set_config(&stationConf);
		WiFi.begin(); 						// need this in case it has been in AP mode or it wont go back into station mode properly !
}
  void checkForSerialData(){
	if ( Serial.available()) {// if data available over serial
	if (currentDataStringComplete){
	currentDataString = "";
	currentDataStringComplete = false;
	}
	for (int a=0;a<Serial.available();a++){
	char readChr = Serial.read();
	if (readChr == '\r'){
		currentDataStringComplete = true;
		Serial.read(); // get rid of the \n
		break;
	}
		currentDataString += readChr;
	}
		// process data here
		if (currentDataStringComplete){// create data block to send in next client update & save to file
		//Serial.println(currentDataString);
		if (currentDataString.startsWith("data")){ // check to see if its a valid string
			currentDataString.remove(0, currentDataString.indexOf(",")+1);
			String dataString = currentDataString;
			for (int a=0;a<MAX_BATTERIES;a++){
				int nextComma = dataString.indexOf(",");
				String theData = dataString.substring(0,nextComma);
				batteryinfo[a].currentVoltage = (long(theData.toFloat()*100000.0) / 100000.0); // round back to 5 dp
				if (batteryinfo[a].currentVoltage > 0){
					batteryinfo[a].currentVoltage += 0.00001; // TODO: complete fix for rounding error
					batteryinfo[a].currentCurrent = (batteryinfo[a].currentVoltage - mosfetVoltageDrop) / batteryinfo[a].resistorValue;
					batteryinfo[a].totalCurrent +=  batteryinfo[a].currentCurrent / 3600.0; // at one sample per second
					batteryinfo[a].totalWattHours += (batteryinfo[a].currentCurrent / 3600.0) * batteryinfo[a].currentVoltage;
				}
				//Serial.println(dataString);
				dataString.remove(0,nextComma+1);
				if (batteryinfo[a].stopTime < 0){ // still running
						String fileName = "/dataFile"+String(a)+".csv";
						File dataFile = SPIFFS.open(fileName, "a");
						if (!dataFile){
							Serial.println("Failed to open/create file");
						}
						if ( batteryinfo[a].currentVoltage == 0 && batteryinfo[a].stopTime < 0){
							batteryinfo[a].stopTime = millis() - batteryinfo[a].startTime;
							//double totalRunTime = (batteryinfo[a].stopTime - batteryinfo[a].startTime) / 3600000.0;// in hrs
							batteryinfo[a].capacity = batteryinfo[a].totalCurrent; // in AH
							batteryinfo[a].currentCurrent = 0;
							//Serial.println(totalRunTime);
							batteriesStopped++;
							if (batteriesStopped == MAX_BATTERIES){ // just stopped
								startFanDelay = true;
								fanStopAt = millis() + 5 * 60 * 1000; // 5 minutes
								tone(tonePin,1500,200);
								delay(201);
								tone(tonePin,1500,200);
								delay(201);
								digitalWrite(tonePin,LOW);
							}
							saveResults(a);//save results here
						}
						dataFile.println(String((millis() - batteryinfo[a].startTime)/ 1000) + "," + String(batteryinfo[a].currentVoltage,4) + "," + String(batteryinfo[a].currentCurrent,4));
						dataFile.close();
				}
			}
		}
		}
	}

  }

void startTest(){// setup startup variables and send start to arduino
	for (int a=0;a<MAX_BATTERIES;a++){// create next file to use, this will erase the last file
			String fileName = "/dataFile"+String(a)+".csv"; //create file in spiffs for storage
			File dataFile = SPIFFS.open(fileName, "w");
			if (!dataFile){
				Serial.println("Failed to open/create file");
			}
			dataFile.println(F("0,0,0"));//Seconds,Voltage,Current
			dataFile.close();
	}
	for (int a=0;a<MAX_BATTERIES;a++){// create next results file to use, this will erase the last file
			String fileName = "/resultsFile"+String(a)+".csv";
			File dataFile = SPIFFS.open(fileName, "w");
			if (!dataFile){
				Serial.println("Failed to open/create file");
			}
			dataFile.println(F("0,0"));
			dataFile.close();
		}
	for (int a=0;a<MAX_BATTERIES;a++){
		batteryinfo[a].currentVoltage=0;
		batteryinfo[a].currentCurrent=0;
		batteryinfo[a].totalCurrent=0;
		batteryinfo[a].totalWattHours=0;
		batteryinfo[a].startTime = millis();
		batteryinfo[a].resistorValue = 1.95;
		batteryinfo[a].stopTime = -1;
		batteryinfo[a].capacity = -1;
	}
	batteriesStopped = 0;
	startFanDelay = false;
	digitalWrite(fanPin,HIGH); // turn on the fan
	Serial.println("START START");
}
void testForFiles(){// make sure there are files to prevent webpage errors

	if (!SPIFFS.exists("/dataFile0.csv")){
		for (int a=0;a<MAX_BATTERIES;a++){
			String fileName = "/dataFile"+String(a)+".csv";
			File dataFile = SPIFFS.open(fileName, "w");
			if (!dataFile){
				Serial.println("Failed to open/create file");
			}
			dataFile.println(F("0,0,0"));
			dataFile.close();
		}
	}
	if (!SPIFFS.exists("/resultsFile0.csv")){
		for (int a=0;a<MAX_BATTERIES;a++){
			String fileName = "/resultsFile"+String(a)+".csv";
			File dataFile = SPIFFS.open(fileName, "w");
			if (!dataFile){
				Serial.println("Failed to open/create file");
			}
			dataFile.println(F("0,0"));
			dataFile.close();
		}
	}
}
void saveResults(int batteryNumber){
	String fileName = "/resultsFile"+String(batteryNumber)+".csv";
	File dataFile = SPIFFS.open(fileName, "w");
	if (!dataFile){
		Serial.println("Failed to open/create file");
	}
	dataFile.println(String(batteryinfo[batteryNumber].capacity,4) + "," + String(batteryinfo[batteryNumber].totalWattHours,4));
	dataFile.close();
}
void checkFanState(){
	if (startFanDelay && (millis() > fanStopAt)){
		startFanDelay = false;
		digitalWrite(fanPin,LOW); // turn off the fan
	}
}