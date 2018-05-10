/**************************************************************************
    Souliss - Multi Power Socket Porting for Expressif ESP8266

	It use static IP Addressing

    Load this code on ESP8266 board using the porting of the Arduino core
    for this platform.
        
***************************************************************************/
// Ultima cifra dell'indirizzo IP
#define IP_ADDRESS	135
#define HOSTNAME	"pwrskt05"

#define	VNET_RESETTIME_INSKETCH
#define VNET_RESETTIME			0x00042F7	// ((20 Min*60)*1000)/70ms = 17143 => 42F7
#define VNET_HARDRESET			ESP.reset()

// Configure the framework
#include "bconf/MCU_ESP8266.h"              // Load the code directly on the ESP8266

// **** Define the WiFi name and password ****
#include "D:\__User\Administrator\Documents\Privati\ArduinoWiFiInclude\wifi.h"
//To avoide to share my wifi credentials on git, I included them in external file
//To setup your credentials remove my include, un-comment below 3 lines and fill with
//Yours wifi credentials
//#define WIFICONF_INSKETCH
//#define WiFi_SSID               "wifi_name"
//#define WiFi_Password           "wifi_password"    

// Include framework code and libraries
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include "Souliss.h"

// Define the network configuration according to your router settings
uint8_t ip_address[4]  = {192, 168, 1, IP_ADDRESS};
uint8_t subnet_mask[4] = {255, 255, 255, 0};
uint8_t ip_gateway[4]  = {192, 168, 1, 1};


// This identify the number of the Slot
#define T_RELE_1		0      
#define T_RELE_2		1      
#define T_RELE_3		2      
#define T_RELE_4		3      
#define T_IN_1			4
#define T_WIFI_STRDB	5	//It takes 2 slots
#define T_WIFI_STR		7	//It takes 2 slots

//Deadband for analog values
#define NODEADBAND		0				//Se la variazione è superio del 0,1% aggiorno    

// **** Define here the right pin for your ESP module **** 
#define	PIN_RELE_1	5
#define	PIN_RELE_2	4
#define	PIN_RELE_3	16
#define	PIN_RELE_4	13

#define	PIN_IN_1	12
#define	PIN_BUTTON	0
#define PIN_LED		14

//Useful Variable
byte led_status = 0;
byte joined = 0;
U8 value_hold=0x068;

//Variable to Handle WiFio Signal
long rssi = 0;
int bars = 0;

void setup()
{   
	delay((IP_ADDRESS - 128) * 5000);
    Initialize();

	//Pin Setup
	pinMode(PIN_RELE_1, OUTPUT);
	pinMode(PIN_RELE_2, OUTPUT);
	pinMode(PIN_RELE_3, OUTPUT);
	pinMode(PIN_RELE_4, OUTPUT);
	pinMode(PIN_IN_1, INPUT);
	pinMode(PIN_LED, OUTPUT);



    // Connect to the WiFi network with static IP
	Souliss_SetIPAddress(ip_address, subnet_mask, ip_gateway);
    
	Set_SimpleLight(T_RELE_1);			// Define a T11 light logic
	Set_SimpleLight(T_RELE_2);			// Define a T11 light logic
	Set_SimpleLight(T_RELE_3);			// Define a T11 light logic
	Set_SimpleLight(T_RELE_4);			// Define a T11 light logic
	Souliss_SetT13(memory_map, T_IN_1);
	Souliss_SetT51(memory_map, T_WIFI_STRDB);	//Imposto il tipico per contenere il segnale del Wifi in decibel
	Souliss_SetT51(memory_map, T_WIFI_STR);	//Imposto il tipico per contenere il segnale del Wifi in barre da 1 a 5
  
	// Init the OTA
	ArduinoOTA.setHostname(HOSTNAME);
	ArduinoOTA.begin();

	Serial.begin(115200);
    Serial.println("Node Init");
}

void loop()
{ 
    // Here we start to play
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        FAST_50ms() {   // We process the logic and relevant input and output every 50 milliseconds
			
			// Detect the button press. Short press toggle, long press reset the node
			U8 invalue = LowDigInHold(PIN_BUTTON,Souliss_T1n_ToggleCmd,value_hold,T_RELE_1);
			if(invalue==Souliss_T1n_ToggleCmd){
				Serial.println("TOGGLE");
				mInput(T_RELE_1)=Souliss_T1n_ToggleCmd;
			} else if(invalue==value_hold) {
				// reset
				Serial.println("REBOOT");
				delay(1000);
				ESP.reset();
			}
			
			//Output Handling
			DigOut(PIN_RELE_1, Souliss_T1n_Coil,T_RELE_1);
			DigOut(PIN_RELE_2, Souliss_T1n_Coil, T_RELE_2);
			DigOut(PIN_RELE_3, Souliss_T1n_Coil, T_RELE_3);
			DigOut(PIN_RELE_4, Souliss_T1n_Coil, T_RELE_4);


			//Check if joined and take control of the led
			if (joined==1) {
				if (mOutput(T_RELE_1)==1) {
					digitalWrite(PIN_LED,HIGH);
				} else {
					digitalWrite(PIN_LED,LOW);
				}
			}
        } 
    FAST_70ms() {
      Souliss_LowDigIn2State(PIN_IN_1, Souliss_T1n_OnCmd, Souliss_T1n_OffCmd, memory_map, T_IN_1);
    }
		FAST_90ms() { 
			//Apply logic if statuses changed
			Logic_SimpleLight(T_RELE_1);
			Logic_SimpleLight(T_RELE_2);
			Logic_SimpleLight(T_RELE_3);
			Logic_SimpleLight(T_RELE_4);
      Souliss_Logic_T13(memory_map, T_IN_1, &data_changed);

		}
		FAST_210ms() {
			//Processa le logiche per il segnale WiFi
			Souliss_Logic_T51(memory_map, T_WIFI_STRDB, NODEADBAND, &data_changed);
			Souliss_Logic_T51(memory_map, T_WIFI_STR, NODEADBAND, &data_changed);
		}
		FAST_510ms() {
			//Check if joined to gateway
			check_if_joined();
		}

        FAST_PeerComms();
		ArduinoOTA.handle();
    }
	EXECUTESLOW() {
		UPDATESLOW();
		SLOW_10s() {
			//Check wifi signal
			check_wifi_signal();
		}
	}
} 

void check_wifi_signal() {
	rssi = WiFi.RSSI();

	if (rssi > -55) {
		bars = 5;
	}
	else if (rssi < -55 & rssi >= -65) {
		bars = 4;
	}
	else if (rssi < -65 & rssi >= -70) {
		bars = 3;
	}
	else if (rssi < -70 & rssi >= -78) {
		bars = 2;
	}
	else if (rssi < -78 & rssi > -82) {
		bars = 1;
	}
	else {
		bars = 0;
	}
	float f_rssi = (float)rssi;
	float f_bars = (float)bars;
	Souliss_ImportAnalog(memory_map, T_WIFI_STRDB, &f_rssi);
	Souliss_ImportAnalog(memory_map, T_WIFI_STR, &f_bars);
#ifdef SERIAL_DEBUG
	Serial.print("wifi rssi:");
	Serial.println(rssi);
	Serial.print("wifi bars:");
	Serial.println(bars);
#endif
}

//This routine check for peer is joined to Souliss Network
//If not blink the led every 500ms, else led is a mirror of rel� status
void check_if_joined() {
	if(JoinInProgress() && joined==0){
		joined=0;
		if (led_status==0){
			digitalWrite(PIN_LED,HIGH);
			led_status=1;
		}else{
			digitalWrite(PIN_LED,LOW);
			led_status=0;
		}
	}else{
		joined=1;
	}		
}
