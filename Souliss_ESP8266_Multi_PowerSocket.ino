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
#define T_RELE_1	0      
#define T_RELE_2	1      
#define T_RELE_3	2      
#define T_RELE_4	3      
#define T_IN_1		4      
     

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

		FAST_510ms() {
			//Check if joined to gateway
			check_if_joined();
		}

        FAST_PeerComms();                                        
    }
	// Look for a new sketch to update over the air
	ArduinoOTA.handle();
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
