/*
Copyright 2017, Tilden Groves.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "median.h"

#define MAX_BATTERIES 8
#define cutoffVoltage 2.8

#define discharging 1
#define off 0

long timeInverval = 1000;
long nextMs = 0;
byte mosfetPins[MAX_BATTERIES];
boolean batteryState[MAX_BATTERIES];
String recieveString;
boolean running = false;
int batteriesStopped = 0;
double resistorRatio = 10.0/82.8;


void setup() {
  for (int a=0;a<MAX_BATTERIES;a++){ // setup pins and turn all batteries off !
		mosfetPins[a] = a + 2;		// 2 through 9 by default
		pinMode(mosfetPins[a],OUTPUT);
		digitalWrite(mosfetPins[a],LOW);
		batteryState[a] = off;
	}
	running = false;
  Serial.begin ( 2000000 );
  }

  void loop() {
  
  long currentMs = millis();
  if (currentMs >= nextMs){
	//Serial.println(currentMs - nextMs); // extra time between reads
	long lostTime = currentMs - nextMs;
	nextMs = currentMs + timeInverval - lostTime; // keep it accurate
	if (running){
		readAllBatteries();
	}
  }
  checkForSerialData();
  }
  
  void checkForSerialData(){
	if ( Serial.available()) {// if data available over serial
		recieveString = Serial.readString();
		//Serial.println(recieveString);
		// process data here
		if (recieveString.indexOf("START") != -1){//start
			start();
		}
		if (recieveString.indexOf("STOP") != -1){//stop
			stop();
		}
		recieveString="";
	}
  
  }
  void readAllBatteries(){
	double voltage;
	String data = "data";
	for (int a=0; a<8;a++){
	if (batteryState[a] == discharging){
		resetFilter();
		for (int b=0 ; b<MAX_MEDIAN_SAMPLES ;b++){
			addToFilter(analogRead(a));
		}
		voltage = (getMedian() / 1023.0) * 5.0 * analogCompensation();
		voltage = (resistorRatio * voltage) + voltage; // correct for voltage divider
		//Serial.println("median " + String(voltage));
		if (voltage <= cutoffVoltage){
			voltage = 0;
			batteriesStopped++;
			batteryState[a] = off;
			digitalWrite(mosfetPins[a],LOW);
		}
	}
	else voltage = 0;
		data += ",";
		data += String(voltage,4);
	}
	Serial.println(data);
	if (batteriesStopped == MAX_BATTERIES){
	running = false;
	}
  }
  void start(){
  for (int a=0;a<MAX_BATTERIES;a++){ // setup pins and turn all batteries on
		digitalWrite(mosfetPins[a],HIGH);
		batteryState[a] = discharging;
	}
	running = true;
	batteriesStopped = 0;
  }
  void stop(){
  double voltage = 0.0;
  String data = "data";
    for (int a=0;a<MAX_BATTERIES;a++){ // Turn all batteries off
		digitalWrite(mosfetPins[a],LOW);
		batteryState[a] = off;
		data += ",";
		data += String(voltage,4);
	}
	running = false;
	Serial.println(data); // send all zero's so to let it know its in its off state
  }
  long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference

  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
  
 //  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
 //  ADCSRB &= ~_BV(MUX5); // Without this the function always returns -1 on the ATmega2560
   
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000 // last value was 1101381L
  return result; // Vcc in millivolts
}
double analogCompensation(){ // for use with battery low voltage
  double result;
  result=((readVcc()/1000.0)/5.0);// actual vcc vs percieved vcc
  //Serial.println("Vcc " + String(readVcc()) + "Comp " + String(result,4));
  return result; // multiply this with analog reading
}