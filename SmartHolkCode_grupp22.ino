#include<stdlib.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "Adafruit_HDC1000.h"
#include "HardwareSerial.h"
#include <math.h>

//#define DEBUG_MODE //Uncomment for debug mode

#define SSID "SmartHolk-NET"
#define PASS "penisridarna1"
#define IP "184.106.153.149" // thingspeak.com
#define THERMISTORPIN A0

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define sleepLoops 225 // Determine how many 8sec sleep cycles it will do until upload, 225 cycles = upload every 30 min
int countSleep = 0;

String GETdata = "GET /update?key=RRILTDXZF0S5TRM9";
String GETtemp = "&field1=";
String GEThum = "&field2=";
String GETin = "&field3=";
String GETout = "&field4=";
SoftwareSerial upload(4,5); //RX och TX

const byte IRpin1 = 2, IRpin2 = 3;
bool recentlyConnected1, recentlyConnected2;
int birdsIn = 0, birdsOut = 0;
Adafruit_HDC1000 hdc = Adafruit_HDC1000();
bool timeToUpload;

//Watchdog Interrupt Service. This is executed when watchdog timed out. [27]
volatile int f_wdt=1;
ISR(WDT_vect)
{
	if(f_wdt == 0){
		f_wdt=1;
	}
	else
	{
	#ifdef DEBUG_MODE
		Serial.println("WDT Overrun!!!");
	#endif
	}
}

void setup()
{
	DDRB = B11111111; //Make pin 8-13 to inputs, so that the pin13 LED will turn off, and the other unused ports
	PORTB = B00000000;
	//digitalWrite(13, LOW);
	
	upload.begin(9600);
	Serial.begin(9600);
	
	recentlyConnected1 = true;
	recentlyConnected2 = true;
	timeToUpload = true;
	hdc.begin();
	
	sendDebug("AT");
	delay(5000);
	if(upload.find("OK")){
		#ifdef DEBUG_MODE
		Serial.println("RECEIVED: OK");
		#endif
		connectWiFi();
	}
	
	 /*** Setup the WDT ***/ //Ref [27]
	MCUSR &= ~(1<<WDRF); /* Clear the reset flag. */
	WDTCSR |= (1<<WDCE) | (1<<WDE); 
	WDTCSR = 1<<WDP0 | 1<<WDP3; /* set new watchdog timeout prescaler value *//* 8.0 seconds */
	WDTCSR |= _BV(WDIE);/* Enable the WD interrupt (note no reset). */
	
	attachInterrupt(digitalPinToInterrupt(IRpin1),birdDetect1,RISING);
	attachInterrupt(digitalPinToInterrupt(IRpin2),birdDetect2,RISING);
}

void loop()
{
	if(f_wdt == 1 && timeToUpload==false){
		countSleep++;
		f_wdt = 0; //Clear watchdog flag
		#ifdef DEBUG_MODE
		Serial.print("Sleep again: ");
		Serial.println(countSleep);
		delay(100);
		#endif
		if(countSleep <= sleepLoops)
			enterSleep();/* Re-enter sleep mode. */
		else if(countSleep > sleepLoops)
			timeToUpload = true;
	}
	else if(timeToUpload){
		countSleep = 0;
		String strTemp = String(getTemp());
		String strHum = String(getHumHDC());
		String strBirdsIn = String(birdsIn);
		String strBirdsOut = String(birdsOut);
		sendThing(strTemp, strHum, strBirdsIn, strBirdsOut);
		delay(1000);
		timeToUpload = false;
	}
	else{
		#ifdef DEBUG_MODE
		Serial.println("W00t mate? Im in the void!");
		#endif
		enterSleep();
	}
}

void sendThing(String temp, String hum, String in, String out){ //Temp, hum, birdsIn, birdsOut //Reference [25] http://www.instructables.com/id/ESP8266-Wifi-Temperature-Logger/?ALLSTEPS
	String cmd = "AT+CIPSTART=\"TCP\",\"";
	cmd += IP;
	cmd += "\",80";
	sendDebug(cmd);
	delay(2000);
	if(upload.find("Error")){
		#ifdef DEBUG_MODE
		Serial.print("RECEIVED: Error");
		#endif
		return;
	}
	cmd = GETdata;
	cmd += GETtemp;
	cmd += temp;
	cmd += GEThum;
	cmd += hum;
	cmd += GETin;
	cmd += in;
	cmd += GETout;
	cmd += out;
	cmd += "\r\n";
	upload.print("AT+CIPSEND=");
	upload.println(cmd.length());
	if(upload.find(">")){
		#ifdef DEBUG_MODE
		Serial.print(">");
		Serial.print(cmd);
		#endif
		upload.print(cmd);
	}
	else{
		sendDebug("AT+CIPCLOSE");
	}
	if(upload.find("OK")){
		#ifdef DEBUG_MODE
		Serial.println("RECEIVED: OK");
		#endif
	}
	else{
		#ifdef DEBUG_MODE
		Serial.println("RECEIVED: Error");
		#endif
	}
	delay(17000);
}
void sendDebug(String cmd)
{
	#ifdef DEBUG_MODE
	Serial.print("SEND: ");
	Serial.println(cmd);
	#endif
	upload.println(cmd);
} 
 
boolean connectWiFi()
{
	upload.println("AT+CWMODE=1");
	delay(2000);
	String cmd="AT+CWJAP=\"";
	cmd+=SSID;
	cmd+="\",\"";
	cmd+=PASS;
	cmd+="\"";
	sendDebug(cmd);
	delay(5000);
	if(upload.find("OK")){
		#ifdef DEBUG_MODE
		Serial.println("RECEIVED: OK");
		#endif
		return true;
	}else{
		#ifdef DEBUG_MODE
		Serial.println("RECEIVED: Error");
		#endif
		return false;
	}
}

float getTemp() //Temperature for voltage div with R_series=10k, and NTC 10k.
{
	float readAnalogTemp = analogRead(THERMISTORPIN);
	readAnalogTemp = (5.0/1023)*readAnalogTemp; //convert to voltge
	readAnalogTemp = (10000*readAnalogTemp)/(5-readAnalogTemp); //Get the thermistor resistance, spanningsdelning
	float temp = -20.85*log(readAnalogTemp)+216.97; // Get the temperature with the logarithmic function Temperature(resistance)
	
	return temp;
}

float getHumHDC() //Get humidity from HDC1008
{
	return (hdc.readHumidity());
}

void birdDetect1()
{
	recentlyConnected1 = false;
	if(recentlyConnected2 == false)
	{
		#ifdef DEBUG_MODE
		Serial.println("Bird got in");
		#endif
		recentlyConnected1 = true;
		recentlyConnected2 = true;
		birdsIn++;
		//timeToUpload = true;
	} 
} 
 
void birdDetect2()
{
	recentlyConnected2 = false;
	if(recentlyConnected1 == false)
	{
		#ifdef DEBUG_MODE
		Serial.println("Bird got out");
		#endif
		recentlyConnected1 = true;
		recentlyConnected2 = true;
		birdsOut++;
		//timeToUpload = true;
	} 
}


void enterSleep(void) //Reference [27] http://donalmorrissey.blogspot.se/2010/04/sleeping-arduino-part-5-wake-up-via.html 
{
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);	 /* EDIT: could also use SLEEP_MODE_PWR_DOWN for lowest power consumption. Else SLEEP_MODE_PWR_SAVE */
	sleep_enable();
		 /* Now enter sleep mode. */
	sleep_mode();
	 /* The program will continue from here after the WDT timeout*/
	sleep_disable(); /* First thing to do is disable sleep. */
	 /* Re-enable the peripherals. */
	power_all_enable();
}



