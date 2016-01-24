/*
 * This routine will collect key information from the attached STC-1000+
 * on a frequency as specified herein, and write this information to a
 * SparkFun (phant) datastream.  This version was written for the Adafruit
 * Huzzah ESP-8266, and utilizes the WiFi library for communications with
 * the ESP.  It also supports an attached OLED to display status information
 * during operation, along with output to the serial monitor.
 * 
 * This variation of the original uses the Phant library to build a request 
 * and post the data to the datastream (vs. the original HTTP GET).  It also 
 * includes the enhancements made to get the NTP time periodically and 
 * display it to the first line of the OLED (no-scroll region) so that it is
 * constantly displayed.  See README file for information on (small) changes 
 * made to the Adafruit SSD1306 OLED library to support the ESP8266 h/w SPI.
 * 
 *    Modified by DK Fowler...11-Dec-2015
 *    Extensions to the original code by Mats Staffansson by
 *    Keith Fowler...01-Dec-2015.
 *    
 *    Copyright 2016 Keith Fowler
 *    
 **** Mats original comments follow ***
 * 
 * STC1000+, improved firmware and Arduino based firmware uploader for the STC-1000 dual stage thermostat.
 * 
 *
 * Copyright 2014 Mats Staffansson
 *
 * This file is part of STC1000+.
 *
 * STC1000+ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * STC1000+ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with STC1000+.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Phant.h>

#include <Adafruit_GFX.h>
#include <ESP_SSD1306.h>

// Define pins to use hardware SPI for OLED display
#define OLED_DC     2
#define OLED_CS     15
#define OLED_RESET  16
ESP_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);

#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

//  Define display size in lines, characters.  Change
//  for different display sizes (or fonts used).
#define DISPLAY_LINES 8
#define DISPLAY_WIDTH_CHARS 21
byte displayLineIndex = 0;
String OLEDdisplayBuff[DISPLAY_LINES];

int status = WL_IDLE_STATUS;

//********* Change the following values for a different SSID or
//********* password.
char ssid[] = "SSID";               // your network SSID (name)
char pass[] = "SSIDpassword";       // your network password

// Initialize the Wifi client library
WiFiClient client;
// Set the IP address for the client
//********* Change the following values to configure a different
//********* client IP address, gateway, or subnet mask.
IPAddress clientIP(192, 168, 0, 5);
IPAddress clientGateway(192,168,0,1);
IPAddress clientSubnet(255,255,255,0);

////////////////////////////
// Sparkfun (Phant) Stuff //
////////////////////////////
//********* Create the Sparkfun datastream before first use following
//********* directions at https://data.sparkfun.com/.  For this code,
//********* the fieldnames must match the names in fieldNames exactly
//********* as listed below.
const byte NUM_FIELDS = 7;
const String fieldNames[NUM_FIELDS] = {
  "temperature","cooling","heating","setpoint","runmode","step","duration" };

String fieldData[NUM_FIELDS];
String displayField;

// Sparkfun (Phant) server address for posting data to stream.
//********* Change the following public and private key values to post to 
//********* your own Phant datastream once created.  Alternatively, you
//********* can specify your own local Phant server.
char server[] = "data.sparkfun.com";
const String publicKey = "";
const String privateKey = "";
const char POSTGood[16]="HTTP/1.1 200 OK";
Phant phant(server, publicKey, privateKey);   // Initialize the Phant datastream 
  
// Setup NTP relevant parameters

unsigned int localPort = 2390;        // local port to listen for UDP packets
// IPAddress timeServer(129, 6, 15, 28);  // time.nist.gov NTP server
IPAddress timeServer(24, 56, 178, 140);   // www.nist.gov NTP server, Ft. Collins, CO
const int NTP_PACKET_SIZE = 48;       // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];  //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP

WiFiUDP Udp;
  
long nextTime = 0;      //next time in millis that the read cycle will fire

//period between posts, set at 30 seconds
#define DELAY_PERIOD 30000

// Period between NTP update calls, displayed on OLED and on serial 
// monitor.
#define DELAY_NTP_UPDATE 30000
long nextNTPTime = 0;   // next time in millis that NTP update will fire
boolean NTPUpdate = false;

#define DEBUG 1

#define	COM_PIN			5	// ICSPCLK, used for the half-duplex comm with STC-1000

#define COM_READ_EEPROM		0x20
#define COM_WRITE_EEPROM	0xE0
#define COM_READ_TEMP		0x01
#define COM_READ_COOLING	0x02
#define COM_READ_HEATING	0x03
#define COM_ACK			0x9A
#define COM_NACK		0x66

void write_bit(unsigned const char data){
	pinMode(COM_PIN, OUTPUT);
	digitalWrite(COM_PIN, HIGH);
	delayMicroseconds(7);
	if(!data){
		pinMode(COM_PIN, INPUT);
		digitalWrite(COM_PIN, LOW);
	}
	delayMicroseconds(400);
	pinMode(COM_PIN, INPUT);
	digitalWrite(COM_PIN, LOW);
	delayMicroseconds(100);
}

unsigned char read_bit(){
	unsigned char data;

	pinMode(COM_PIN, OUTPUT);
	digitalWrite(COM_PIN, HIGH);
	delayMicroseconds(7);
	pinMode(COM_PIN, INPUT);
	digitalWrite(COM_PIN, LOW);
	delayMicroseconds(200);
	data = digitalRead(COM_PIN);
	delayMicroseconds(300);

	return data;
}

void write_byte(unsigned const char data){
	unsigned char i;
	
	for(i=0;i<8;i++){
		write_bit(((data << i) & 0x80));
	}
	delayMicroseconds(500);
}

unsigned char read_byte(){
	unsigned char i, data;
	
	for(i=0;i<8;i++){
		data <<= 1;
		if(read_bit()){
			data |= 1;
		}
	}
	delayMicroseconds(500);

	return data;
}


bool write_eeprom(const unsigned char address, unsigned const int value){
	unsigned char ack;
	write_byte(COM_WRITE_EEPROM);
	write_byte(address);
	write_byte(((unsigned char)(value >> 8)));
	write_byte((unsigned char)value);
	write_byte(COM_WRITE_EEPROM ^ address ^ ((unsigned char)(value >> 8)) ^ ((unsigned char)value));
	delay(6); // Longer delay needed here for EEPROM write to finish, but must be shorter than 10ms
	ack = read_byte();
	return ack == COM_ACK; 
}

bool read_eeprom(const unsigned char address, int *value){
	unsigned char xorsum;
	unsigned char ack;
        unsigned int data;

//        Serial.println("Read EEPROM called...");    // Debug
        
	write_byte(COM_READ_EEPROM);
	write_byte(address);
	data = read_byte();
	data = (data << 8) | read_byte();
	xorsum = read_byte();
        ack = read_byte();
	if(ack == COM_ACK && xorsum == (COM_READ_EEPROM ^ address ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
	        *value = (int)data;
//                Serial.print("Address read = ");    // Debug
//                Serial.println(address);            // Debug
//                Serial.print("Data returned = ");   // Debug
//                Serial.println(*value);             // Debug
		return true;
	}
	return false;
}

bool read_command(unsigned char command, int *value){
	unsigned char xorsum;
	unsigned char ack;
        unsigned int data;

//        Serial.println("Read command called...");    // Debug
        
	write_byte(command);
	data = read_byte();
	data = (data << 8) | read_byte();
	xorsum = read_byte();
        ack = read_byte();
	if(ack == COM_ACK && xorsum == (command ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
	        *value = (int)data;
//                Serial.print("Command = ");         // Debug
//                Serial.println(command);            // Debug
//                Serial.print("Value returned = ");  // Debug
//                Serial.println(*value);             // Debug
		return true;
	}
	return false;
}

bool read_temp(int *temperature){
	return read_command(COM_READ_TEMP, temperature); 
}

bool read_heating(int *heating){
	return read_command(COM_READ_HEATING, heating); 
}

bool read_cooling(int *cooling){
	return read_command(COM_READ_COOLING, cooling); 
}

/* End of communication implementation */

/* From here example implementation begins, this can be exchanged for your specific needs */
enum set_menu_enum {
	hysteresis,			          // hy (hysteresis)
	hysteresis2,			        // hy2 (hysteresis probe 2)
	temperature_correction,		// tc (temperature correction)
	temperature_correction2,	// tc2 (temperature correction probe 2)
	setpoint_alarm,			      // SA (setpoint alarm)
	setpoint,			            // SP (setpoint)
	step,				              // St (current running profile step)	
	duration,			            // dh (current running profile step duration in hours)
	cooling_delay,			      // cd (cooling delay minutes)
	heating_delay,			      // hd (heating delay minutes)
	ramping,			            // rP (0=disable, 1=enable ramping)
	probe2,				            // Pb (0=disable, 1=enable probe2 for regulation)
	run_mode			            // rn (0-5 run profile, 6=thermostat)
};

/* Defines for EEPROM config addresses */
#define EEADR_PROFILE_SETPOINT(profile, stp)	(((profile)*19) + ((stp)<<1))
#define EEADR_PROFILE_DURATION(profile, stp)	(EEADR_PROFILE_SETPOINT(profile, stp) + 1)
#define EEADR_SET_MENU				EEADR_PROFILE_SETPOINT(6, 0)
#define EEADR_SET_MENU_ITEM(name)		(EEADR_SET_MENU + (name))
#define EEADR_POWER_ON				127

const char menu_opt[][4] = {
	"hy",
	"hy2",
	"tc",
	"tc2",
	"SA",
	"SP",
	"St",
	"dh",
	"cd",
	"hd",
	"rP",
	"Pb2",
	"rn"
};

bool isBlank(char c){
	return c == ' ' || c == '\t';
}

bool isDigit(char c){
	return c >= '0' && c <= '9';
}

bool isEOL(char c){
	return c == '\r' || c == '\n';
}

void print_temperature(int temperature){
	if(temperature < 0){
		temperature = -temperature;
		Serial.print('-');
	}
	if(temperature >= 1000){
		temperature /= 10;
		Serial.println(temperature);
	} else {
		Serial.print(temperature/10);
		Serial.print('.');
		Serial.println(temperature%10);
	}
}

String format_temperature(int temperature){
        String strTemperature="";
  	if(temperature < 0){
		strTemperature = "-" + String(temperature);
	}
	if(temperature >= 1000){
		strTemperature = String(temperature/10.0,1);
	} else {
                strTemperature = String(temperature/10.0,1);
	}
        return strTemperature;
}

void print_config_value(unsigned char address, int value){
	if(address < EEADR_SET_MENU){
		unsigned char profile=0;
		while(address >= 19){
			address-=19;
			profile++;
		}
		if(address & 1){
			Serial.print("dh");
		} else {
			Serial.print("SP");
		}
		Serial.print(profile);
		Serial.print(address >> 1);
		Serial.print('=');
		if(address & 1){
			Serial.println(value);
		} else {
			print_temperature(value);
		}
	} else {
		Serial.print(menu_opt[address-EEADR_SET_MENU]);
		Serial.print('=');
		if(address == EEADR_SET_MENU_ITEM(run_mode)){
			if(value >= 0 && value <= 5){
				Serial.print("Pr");
				Serial.println(value);
			} else {
				Serial.println("th");
			}
		} else if(address <= EEADR_SET_MENU_ITEM(setpoint)){
			print_temperature(value);
		} else {
			Serial.println(value);
		}
	}
}

unsigned char parse_temperature(const char *str, int *temperature){
	unsigned char i=0;
	bool neg = false;

	if(str[i] == '-'){
		neg = true;
		i++;
	}

	if(!isDigit(str[i])){
		return 0;
	}

	*temperature = 0;
	while(isDigit(str[i])){
		*temperature = *temperature * 10 + (str[i] - '0');
		i++;
	}
	*temperature *= 10;
	if(str[i] == '.'){
		i++;
		if(isDigit(str[i])){
			*temperature += (str[i] - '0');
			i++;
		} else {
			return 0;
		} 
	}

	if(neg){
		*temperature = -(*temperature);
	}

	return i;
} 

unsigned char parse_address(const char *cmd, unsigned char *addr){
	char i;	

	if(!strncmp("SP", cmd, 2)){
		if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6'){
			*addr = EEADR_PROFILE_SETPOINT(cmd[2]-'0', cmd[3]-'0');
			return 4;
		}
	}

	if(!strncmp("dh", cmd, 2)){
		if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6' && cmd[3] < '9'){
			*addr = EEADR_PROFILE_DURATION(cmd[2]-'0', cmd[3]-'0');
			return 4;
		}
	}

	for(i=0; i<(sizeof(menu_opt)/sizeof(menu_opt[0])); i++){
		unsigned char len = strlen(menu_opt[i]);
		if(!strncmp(cmd, &menu_opt[i][0], len) && (isBlank(cmd[len]) || isEOL(cmd[len]))){
			*addr = EEADR_SET_MENU + i;
			return strlen(menu_opt[i]);
		}
	}

	*addr = 0;
	for(i=0; i<30; i++){
		if(isBlank(cmd[i]) || isEOL(cmd[i])){
			break;
		}
		if(isDigit(cmd[i])){
			if(*addr>12){
				return 0;
			} else {
				*addr = *addr * 10 + (cmd[i] - '0');
		 	}
		} else {
			return 0;
		}
	}

	if(*addr > 127){
		return 0;
	}

	return i;
}

unsigned char parse_config_value(const char *cmd, int address, bool pretty, int *data){
	unsigned char i=0;
	bool neg=false;

	if(pretty){
		if(address < EEADR_SET_MENU){
			while(address >= 19){
				address-=19;
			}
			if((address & 1) == 0){
				return parse_temperature(cmd, data);
			}
		} else if(address <= EEADR_SET_MENU_ITEM(setpoint)){
			return parse_temperature(cmd, data);
		} else if(address == EEADR_SET_MENU_ITEM(run_mode)) {
			if(!strncmp(cmd, "Pr", 2)){
				*data = cmd[2] - '0';
				if(*data >= 0 && *data <= 5){
					return 3;
				}
			} else if(!strncmp(cmd, "th", 2)){
				*data = 6;
				return 2;
			}
			return 0;
		}
	}	

	if(cmd[i] == '-'){
		neg = true;
		i++;
	}

	if(!isDigit(cmd[i])){
		return 0;
	}

	for(*data=0; i<6; i++){
		if(!isDigit(cmd[i])){
			break;
		}
		if(isDigit(cmd[i]) && *data < 3276){
			*data = *data * 10 + (cmd[i] - '0');
		} else {
			return 0;
		}
	}

	if((neg && *data > 32768) || (!neg && *data > 32767)){
		return 0;
	}

	if(neg){
		*data = -(*data);
	}
	
	return i;
}

void parse_command(char *cmd){
	int data;

	if(cmd[0] == 't'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_temp(&data)){
			Serial.print("Temperature=");
			print_temperature(data);
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'h'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_heating(&data)){
			Serial.print("Heating=");
			Serial.println(data ? "on" : "off");
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'c'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_cooling(&data)){
			Serial.print("Cooling=");
			Serial.println(data ? "on" : "off");
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'r' || cmd[0] == 'w') {
		unsigned char address=0;
		unsigned char i=0, j;
		bool neg = false;

		if(!isBlank(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}

		j = parse_address(&cmd[2], &address);
		i+=j+2;

		if(j==0){
			Serial.println("?Syntax error");
			return;
		}

		if(cmd[0] == 'r'){
			if(!isEOL(cmd[i])){
				Serial.println("?Syntax error");
				return;
			}
			if(read_eeprom(address, &data)){
				if(isDigit(cmd[2])){
					Serial.print("EEPROM[");
					Serial.print(address);
					Serial.print("]=");
					Serial.println(data);
				} else {
					print_config_value(address, data);
				}
			} else {
				Serial.println("?Communication error");
			}
			return;
		}

		if(!isBlank(cmd[i])){
			Serial.println("?Syntax error");
			return;
		}
		i++;

		j = parse_config_value(&cmd[i], address, !isDigit(cmd[2]), &data);
		i += j;
		if(j == 0){
			Serial.println("?Syntax error");
			return;
		} else {
			if(!isEOL(cmd[i])){
				Serial.println("?Syntax error");
				return;
			}
			if(write_eeprom(address, data)){
				Serial.println("Ok");
			} else {
				Serial.println("?Communication error");
			}
		}
	}
}

void setup() {

  Serial.begin(115200);

	Serial.println("STC-1000+ communication data stream sketch.");
	Serial.println("Copyright 2014/2016 Mats Staffansson / Keith Fowler");
	Serial.println("");
//	Serial.println("Commands: 't' to read temperature");
//	Serial.println("          'c' to read state of cooling relay");
//	Serial.println("          'h' to read state of heating relay");
//	Serial.println("          'r [addr]' to read EEPROM address");
//	Serial.println("          'w [addr] [data]' to write EEPROM address");
//	Serial.println("");
//	Serial.println("[addr] can be literal (0-127) or mnemonic SPxy/dhxy, hy, tc and so on");
//	Serial.println("[data] will also be literal (as stored in EEPROM) or human friendly");
//	Serial.println("depending on addressing mode");
//      Serial.println("");
  Serial.println("This sketch will read current values of the running STC, e.g., temperature");
  Serial.println("status of cooling / heating relays, setpoint, run-mode, profile step,");
  Serial.println("and duration hours, and send these to a data stream at Sparkfun at");
  Serial.println("the interval specified herein.");
  Serial.println("");

  // SSD1306 Init
  display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);
  // Clear the buffer.
  display.clearDisplay();

  WiFi.disconnect();
  connectWiFi();
  if (status != WL_CONNECTED) {
    Serial.println("WiFi not connected...exiting.") ;
//    exit(0);
  }  
  else {
    Serial.println("Connected to WiFi...");
    displayOLED("WiFi connected");
    printWifiStatus();
  }
 
  Serial.println("\nStarting UDP connection to timeserver...");
  displayOLED("Start UDP on " + String(localPort));
  Udp.begin(localPort);

//  Get time from NTP server and display it...
  getNTPTime();

}

void connectWiFi() {

  byte connectAttempt=0 ;
  
  display.clearDisplay();
  display.setCursor(0,0);

// attempt to connect to Wifi network:

  while (status != WL_CONNECTED) {
    connectAttempt++ ;
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    displayOLED("Connect: " + String(ssid));
// Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    WiFi.config(clientIP, clientGateway, clientSubnet);

// wait 10 seconds for connection:
    delay(10000);
// If not connected after 100 attempts, let's abort...something's seriously wrong.    
      if (connectAttempt > 100) {
        Serial.println("Exceeded maximum connection attempts, aborting.") ;
        displayOLED("Exceeded attempts");
        break ;
      }
    }
}

void printWifiStatus() {

  char formatIP[15] = "";
  IPAddress ip;
 
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  displayOLED("SSID: " + String(WiFi.SSID()));


// Since calls to return the local IP return all 0's for the
// octets at times, let's loop a few times if this happens
// so we get a good return.

  for (int i=1; i<=20; i++) {
    // get the local WiFi shield's IP address:
    ip = WiFi.localIP();
    delay(1000);
    if (!((ip[0]==0) && (ip[1]==0) && (ip[2]==0) && (ip[3]==0))) {
      break;
    }
  }
  
  Serial.print("IP Address: ");
  Serial.println(ip);
  sprintf(formatIP,"%d:%d:%d:%d", ip[0],ip[1],ip[2],ip[3]);
  displayOLED("IP: " + String(formatIP));
}

void displayOLED(String displayString) {
 
  int16_t saveCursorX, saveCursorY;
  display.setTextSize(1);
  display.setTextColor(WHITE);
//  Serial.print("Line index = "); Serial.println(displayLineIndex);

//  Check to see if we're processing an update to the time, displayed
//  on the first line (not in scroll region).
  if (NTPUpdate) {
    OLEDdisplayBuff[0] = displayString;

//  Save the cursor position and display the time on the first line,
//  then restore the cursor for subsequent writes.
      saveCursorX = display.getCursorX();
      saveCursorY = display.getCursorY();
      display.setCursor(0,0);
      display.fillRect(0,0,DISPLAY_WIDTH_CHARS*6,8,BLACK);  // blank the line first
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println(displayString);
      display.setCursor(saveCursorX,saveCursorY);
  } else { 

//  This is the normal case for NOT a time update.
//  Check to see if we've exceeded the maximum lines on the OLED.
    if (displayLineIndex > (DISPLAY_LINES-1)) {
      for ( byte i = 1; i < (DISPLAY_LINES-1); i++ ) {
//  Scroll the display buffer up one line, losing the first line.
        OLEDdisplayBuff[i] = OLEDdisplayBuff[i+1];
//        Serial.print("i="); Serial.print(i); Serial.println(OLEDdisplayBuff[i]);
      }

//  Set the last line of the display buffer to contain the new line
//  to be displayed.
      OLEDdisplayBuff[(DISPLAY_LINES-1)] = displayString;

//  Now display the results on the OLED, scrolled up one line
      display.clearDisplay();
      display.setCursor(0,0);

      for ( byte i = 0; i <= (DISPLAY_LINES-1); i++ ) {
//      Serial.println(OLEDdisplayBuff[i]);
        display.println(OLEDdisplayBuff[i]);
        display.display();
      }
    } else {

//  No need to scroll yet, so just accumulate the line in the 
//  display buffer, and put it on the OLED.

      OLEDdisplayBuff[displayLineIndex] = displayString;
//    Serial.print("li="); Serial.print(displayLineIndex); 
//    Serial.println(" " + OLEDdisplayBuff[displayLineIndex]);
      display.println(displayString);
    }
    displayLineIndex++;
  }  

// Finally, refresh the display.
  display.display();
  delay(2000);
}

void getNTPTime() {
//********* Change the following offset from GMT to correspond to your
//********* local timezone.  Currently set for EDT.
  const int UTCoffset = -5;   // Eastern Daylight Time

  int UDPBytes;
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(2000);

  Serial.println("Call to get NTP time made.");
  UDPBytes = 0;
  UDPBytes = Udp.parsePacket();
  if (UDPBytes) {
    Serial.print("Packet received...");
    Serial.print(UDPBytes);
    Serial.println(" bytes.");
    Serial.println("Parsing returned packet...");

    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // Combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // Mow convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // Subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // Print Unix time:
    Serial.println(epoch);

    // Print the hour, minute and second:
    char displayNTPTime[8]="";
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)

    Serial.print(':');
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second

//  Adjust UTC time for EDT / EST...

    epoch = epoch + (UTCoffset * 3600);
    sprintf(displayNTPTime, "%02u:%02u:%02u",
      (((epoch  % 86400L) / 3600)),
      ((epoch % 3600) / 60),
      (epoch % 60) );

    // And display it on the OLED...
    // Print the hour, minute and second:
    NTPUpdate = true;
    displayOLED("EDT is " + String(displayNTPTime));
    NTPUpdate = false;
 
  } else {
    Serial.println("No response received from NTP server.");
  }
}

unsigned long sendNTPpacket(IPAddress& address) {
// send an NTP request to the time server at the given address

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();

}

void postHTTP() {

// This method makes a HTTP connection to the data stream server
// and POST's the sensor data:
  boolean TCPConnect, statusPOST;
  int HTTPReplyIndex = 0;
  byte ASCIIChar;
  char HTTPReplyBuff[DISPLAY_WIDTH_CHARS+1]="";

  // Close any connection before sending a new request.
  // This will free the socket.
  client.stop();
  // Make a TCP connection to remote host
  Serial.println("Starting HTTP request...");
  displayOLED("Start HTTP request");

  //  Try up to 5 times to establish a TCP connection to the
  //  server this pass, delaying a few seconds between attempts.
  for (byte i=0; i<5; i++) {
    TCPConnect = client.connect(server, 80);
    if (TCPConnect) {
      break;
    }
    delay(2000);
  }
  
  if (client.connected())
  {
    // Post the data!
    Serial.println("Connected to data stream...");
    displayOLED("HTTP client connect");

    Serial.println("Sending stream post request...");
    displayOLED("Send POST");

//  Begin building the Phant request...
    
    for (int i=0; i<NUM_FIELDS; i++)
    {
      phant.add(fieldNames[i], fieldData[i]);
    }
//    Serial.println("----TEST URL----");
//    Serial.println(phant.url());
    Serial.println("----HTTP POST----");
//    Serial.println(phant.post());
    statusPOST = client.println(phant.post());
//    if (statusPOST) {
//      Serial.println("**HTTP POST success**");
//    } else {
//      Serial.println("**HTTP POST failed**");
//    }
  }
  else
  {
    Serial.println("Connection failed");
    displayOLED("HTTP connect failed");
  } 

  // Check for a response from the server, and route it
  // out the serial port.
  while (client.connected())
  {
    if ( client.available() )
    {
//  In addition to sending the returned results to the serial monitor,
//  let's check the status by looking for the success reply
//  "HTTP/1.1 200 OK".  Accumulate the first characters returned,
//  up to the width of the display.
      char c = client.read();
      Serial.print(c);
      HTTPReplyIndex++;
      if (HTTPReplyIndex < (DISPLAY_WIDTH_CHARS+1))
      {
        ASCIIChar = byte(c);
        //  Only save printable characters in reply buffer
        if ((ASCIIChar>=32) && (ASCIIChar<=126)) {
           HTTPReplyBuff[HTTPReplyIndex-1]=c;
        }
      } 
        
     }
   }    

//  Now check the HTTP POST status...

    HTTPReplyBuff[DISPLAY_WIDTH_CHARS+1]='\0';   // Terminate the reply buffer

  if (strncmp(HTTPReplyBuff, POSTGood, 15) == 0)
  {
    Serial.println("Success detected on POST:  " + String(HTTPReplyBuff));
    displayOLED("POST success");
    displayOLED(String(HTTPReplyBuff));
    client.stop();
  }
  else
  {
    Serial.println("Error detected on POST:  " + String(HTTPReplyBuff));
    displayOLED("POST failed");
    displayOLED(String(HTTPReplyBuff));
    client.stop();
  }
}


void clearRx() {
    while(Serial.available() > 0) {
        Serial.read();
    }
}

void loop() {

  char serialbuffer[100];//serial buffer for request command
//  char myserialbuffer[512];//serial buffer for ESP8266
  byte padLen;
  int data, rn, St, dh;
  String strTemperature;
  String strCooling;
  String strHeating;
  String strSP;
  String strRn;
  String strSt;
  String strDh;

  #define COM_READ_SP	0x77
  #define COM_READ_rn	0x7E
  #define COM_READ_rP 0x7C
  #define COM_READ_St	0x78
  #define COM_READ_dh	0x79
   
  byte cooling, heating, commErr, commErrThisPass;

  static char cmd[32], rxchar=' ';
  static unsigned char index=0; 	

  if(Serial.available() > 0){
      char c = Serial.read();
      if(!(isBlank(rxchar) && isBlank(c))){
	      cmd[index] = c;
	      rxchar = c;
	      index++;
      }

      if(index>=31 || isEOL(rxchar)){
	      cmd[index] = '\0';
	      parse_command(cmd);
	      index = 0;
	      rxchar = ' ';
      }
  }

  if (status != WL_CONNECTED) {
    connectWiFi();
  }  

  if(nextTime<millis()){
    Serial.println();
    Serial.print("Timer reset: ");
    Serial.println(nextTime);

    //reset timer
    nextTime = millis() + DELAY_PERIOD;

//  Flush the serial receive buffer in preparation for further processing...

    clearRx();
    
//   Read data (current temperature, heating / cooling relays, setpoint, running profile, step, and duration hours)

    commErr=0;

//  Try up to 5 times to get a valid reading; otherwise flag as an error this pass and continue.

    for (int i = 0; i < 5; i++) {
      if(read_temp(&data)){
        strTemperature=format_temperature(data);
        Serial.print("Temperature\t= ");
        Serial.println(strTemperature);
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        strTemperature="err";
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
      }
      delay(500);
    }
    fieldData[0] = strTemperature;
    if(commErrThisPass) {commErr=1;}
    
    for (int i = 0; i < 5; i++) {
      if(read_cooling(&data)){
        cooling=data;
        strCooling=(cooling ? "on" : "off");
        Serial.print("Cooling\t\t= ");
        Serial.println(data ? "on" : "off");
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        strCooling="err";
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
      }
      delay(500);
    }
    fieldData[1] = strCooling;
    if(commErrThisPass) {commErr=1;}

    for (int i = 0; i < 5; i++) {
      if(read_heating(&data)){
        heating=data;
        strHeating=(heating ? "on" : "off");
        Serial.print("Heating\t\t= ");
        Serial.println(data ? "on" : "off");
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        strHeating="err";
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
      }
      delay(500);
    }
    fieldData[2] = strHeating;
    if(commErrThisPass) {commErr=1;}

    for (int i = 0; i < 5; i++) {
      if(read_eeprom(COM_READ_SP, &data)){
        strSP=format_temperature(data);
        Serial.print("SP\t\t= ");
        Serial.println(strSP);
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
        strSP="err";
      }
      delay(500);
    }
    fieldData[3] = strSP;
    if(commErrThisPass) {commErr=1;}

    for (int i = 0; i < 5; i++) {
      if(read_eeprom(COM_READ_rn, &data)){
        rn=data;
        if ((rn >= 0) && (rn <=5)) {
          strRn="Pr" + String(rn);
        } else {
          strRn="th";
        }
        Serial.print("rn\t\t= ");
        Serial.println(strRn);
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
        strRn="err";
      }
      delay(500);
    }
    fieldData[4] = strRn;
    if(commErrThisPass) {commErr=1;}

    for (int i = 0; i < 5; i++) {
      if(read_eeprom(COM_READ_St, &data)){
        St=data;
        strSt=String(St);
        Serial.print("St\t\t= ");
        Serial.println(strSt);
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
        strSt="99";    // Flag for an error occurred
      }
      delay(500);
    }
    fieldData[5] = strSt;
    if(commErrThisPass) {commErr=1;}

    for (int i = 0; i < 5; i++) {
      if(read_eeprom(COM_READ_dh, &data)){
        dh=data;
        strDh=String(dh);
        Serial.print("dh\t\t= ");
        Serial.println(strDh);
        commErrThisPass=0;
        break;
      } else {
        commErrThisPass=1;
        Serial.print("?Communication error for attempt ");
        Serial.println(i+1);
        strDh="99";    // Flag for an error occurred
      }
      delay(500);
    }
    fieldData[6] = strDh;
    if(commErrThisPass) {commErr=1;}

//  If all of the data is good, send data to SparkFun stream (temperature, set point, cooling, heating, run mode, step, duration hours)
    if (!commErr) {
      //  Display the data on the OLED...pad it with spaces to align.
      for (byte i=0; i<NUM_FIELDS; i++) {
        padLen = 11 - fieldNames[i].length();
        displayField=fieldNames[i];
        for (byte ii=0; ii<padLen; ii++) {
          displayField = displayField + " ";
        }
        displayOLED(displayField + " = " + fieldData[i]);
      }
      postHTTP();
    } else {
      Serial.println("Skipped logging to data stream due to comm error this pass...");
    }
    delay(1000);
  }
  
  if(nextNTPTime<millis()){
    Serial.println();
    Serial.print("NTP timer reset: ");
    Serial.println(nextNTPTime);

    // Reset NTP timer
    nextNTPTime = millis() + DELAY_NTP_UPDATE;

    // Update NTP time on display an in serial console.
    getNTPTime();
  }
}

