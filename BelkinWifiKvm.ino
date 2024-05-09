/*
  ESP8266 KVM Switch Automator.
  Phil Pemberton <philpem@philpem.me.uk>

  This is wired into the front-panel connections of a Belkin SOHO KVM switch (model F1DD104L) and
  allows a HTTP client (e.g. Bitfocus Companion or cURL) to read the KVM's status or switch the
  ports.

  Control is done via HTTP with no authentication, so this should only be used on a trusted network.

  Instructions:
  - Edit config.h and set your WiFi settings.
  - Flash the firmware onto the ESP8266 (remember to set your board type to Wemos D1 R2 and Mini)
  - Connect as below. (use plug-in "Dupont" crimp connectors so the ESP can be removed if needed)
  - Point your browser to http://kvm-switch.local, you should see a response.

  HTTP requests available:
  - http://kvm-switch,local/
     - returns a default homepage
  - http://kvm-switch.local/get
     - returns the current port number
  - http://kvm-switch.local/switch?n=1 switches to port 1 --
     - change the port number parameter as needed
     - the response will be the same as "get", i.e. the current port number.
     - Response is not returned until the switch has activated (~100ms).

  Wiring:
  - +5V: front panel pin 1 or 2
  - GND: front panel pin 11, 12, 18, 23 or 24
  - D1: Switch 1 "push" -- front panel pin 5
  - D2: Switch 2 "push" -- front panel pin 8
  - D3: Switch 3 "push" -- front panel pin 17
  - D4: Switch 4 "push" -- front panel pin 22
  - D7: PC LED binary low -- LS139 pin 2 / front panel pin 15
  - D8: PC LED binary low -- LS139 pin 3 / front panel pin 16

  The full 26-pin pinout is as follows:
     1: V+ (5V)
     2: V+ (5V)
     3: Port 1, Right (switch PC)
     4: Port 1, Left  (switch audio)
     5: Port 1, Push  (switch both)
     6: Port 2, Right
     7: Port 2, Left
     8: Port 2, Push
     9: Port 3, Right
    10: Port 3, Left
    11: GND
    12: GND
    13: LS139 pin 14 (Audio LED binary select)
    14: LS139 pin 13 (Audio LED binary select)
    15: LS139 pin 2  (KVM LED binary select)
    16: LS139 pin 3  (KVM LED binary select)
    17: Port 3, Push
    18: GND
    19: ? (possibly no connect)
    20: Port 4, Right
    21: Port 4, Left
    22: Port 4, Push
    23: GND
    24: GND
    25: USB data (to front USB port)
    26: USB data (to front USB port)

  The switches are 10x10mm through-hole directional switches, pinned as follows:

    PUSH  UP  COMMON
    ||    ||  ||
  +--------------+
  |              |
  |              |
  |              |
  |              |
  |              |
  +--------------+
    ||    ||  ||
   LEFT  DOWN RIGHT

  Common connects to ground via a 1k resistor.
  When a direction is pushed, the direction pin connects to Common.

  LED selection is done with a 74LS139 mux. The incoming binary inputs select the active LED. LEDs 2 and 3 seem to be swapped.
*/


#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "builtinFiles.h"

#include "config.h"

#if !(defined(STASSID) && defined(STAPSK) && defined(STAHOST))
#  error "STASSID, STAPSK and STAHOST need to be defined in config.h. Use config.h.example as an example."
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* host = STAHOST;

// TCP server at port 80 will respond to HTTP requests
ESP8266WebServer webServer(80);


/****************************
 * Hardware interface
 */

// Pin definitions
// Switches - pull low to switch, normally float
#define PIN_SWITCH1 D1 /* D1=GPIO5 */
#define PIN_SWITCH2 D2 /* D2=GPIO4 */
#define PIN_SWITCH3 D3 /* D3=GPIO0 */
#define PIN_SWITCH4 D4 /* D4=GPIO2 */
// LED state inputs
#define PIN_LED0 D8  /* D8=GPIO15 => 139.pin3 p16*/
#define PIN_LED1 D7  /* D7=GPIO13 => 139.pin2 p15*/


// Pin index array
const byte SWITCH_PINS[] = { PIN_SWITCH1, PIN_SWITCH2, PIN_SWITCH3, PIN_SWITCH4 };

// Switch hold-down time in milliseconds
const int SWITCH_HOLD_TIME = 100;

// Switch post-push wait time in milliseconds
const int SWITCH_POSTWAIT_TIME = 50;


// Utility function: get active switchport
byte getActivePort(void)
{
	byte n;
	
	// Read LED state pins and convert to port number
	n  = digitalRead(PIN_LED0) ? 1 : 0;
	n += digitalRead(PIN_LED1) ? 2 : 0;

	switch (n) {
		case 0:  n = 1; break;
		case 1:  n = 3; break;
		case 2:  n = 2; break;
		case 3:  n = 4; break;
		
		default: n = 99; break;
	}

	return n;
}

// Utility function: switch to port
void setActivePort(const byte port)
{
	// Trigger switch for SWITCH_HOLD_TIME
	digitalWrite(SWITCH_PINS[port-1], LOW);
	pinMode(SWITCH_PINS[port-1], OUTPUT);
	delay(SWITCH_HOLD_TIME);
	pinMode(SWITCH_PINS[port-1], INPUT);
}

/****************************
 * HTTP request handlers
 */

// HTTP request handler for root
void handleRoot() {
	String s;

	Serial.println("handleRoot()");

	IPAddress ip = WiFi.localIP();
	String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
	s = "<!DOCTYPE HTML>\r\n<html><head><title>KVM switch</title></head><body>\r\n";
	s += "Hello from the KVM Switch at " + String(STAHOST) + "(" + ipStr + ")\r\n";
	s += "<p>\r\n";
	s += "<strong>Available API endpoints:</strong>\r\n";
	s += "<ul>\r\n";
	s += "  <li><a href=\"/get\">Get current port</a></li>\r\n";
	for (int i=0; i<(sizeof(SWITCH_PINS) / sizeof(SWITCH_PINS[0])); i++) {
		s += "  <li><a href=\"/switch?n=" + String(i+1) + "\">Switch to port " + String(i+1) + "</a></li>\r\n";
	}
	s += "</ul>\r\n";
	s += "</body></html>\r\n\r\n";

	webServer.send(200, "text/html", s);
}

// HTTP request handler for /get
// "/get" endpoint: get current switchport
void handleGetPort() {
	String s;

	Serial.println("handleGetPort()");

	int n=getActivePort();

	s = String(n) + "\r\n";
	//s = "{\"port\":" + String(n) + "}\r\n";
	Serial.println("GetPort = " + String(n));

	webServer.send(200, "text/plain", s);
}

// HTTP request handler for /switch/<N>
void handleSwitchPort() {
	String s;

	Serial.println("handleSwitchPort()");

	int n=-1;

	// Get port number from URL
	for (uint8_t i = 0; i < webServer.args(); i++) {
		if (webServer.argName(i) == "n") {
			n = webServer.arg(i).toInt();
		}
	}

	// Bounds check the port number
	if ((n < 1) || (n > (sizeof(SWITCH_PINS) / sizeof(SWITCH_PINS[0])))) {
		webServer.send(400, "text/plain", "Invalid port number");
		return;
	}

	// Switch port
	setActivePort(n);

/*
	s = "Switched to port " + String(n) + "\r\n";
	Serial.println("SwitchPort to " + String(n));

	webServer.send(200, "text/plain", s);
*/

	// Wait for KVM to see the push and update the LEDs
	delay(SWITCH_POSTWAIT_TIME);

	// Return the same as a getPort request
	handleGetPort();
}


/****************************
 * Setup entry point
 */

void setup(void) {
	Serial.begin(115200);

	// Pin setup --
	// LED inputs	
	pinMode(PIN_LED0, INPUT);
	pinMode(PIN_LED1, INPUT);

#ifdef SET_SWITCHPORT_ON_BOOT
	// -- this is a workaround for the switchport changing when the pins are
	// Get current switchport
	byte bootPort = getActivePort();
#endif

	// Switch outputs. These stay as inputs but change to low outputs when triggered.
	for (int i=0; i<(sizeof(SWITCH_PINS)/sizeof(SWITCH_PINS[0])); i++) {
		pinMode(SWITCH_PINS[i], INPUT);
	}

#ifdef SET_SWITCHPORT_ON_BOOT
	// Set switchport to detected port
	setActivePort(bootPort);
#endif

	// Boot banner
	Serial.println("");
	Serial.println("");
	Serial.println("");

	Serial.println("WiFi KVM Controller -- built " __DATE__ " " __TIME__);
	Serial.println("Phil Pemberton <philpem@philpem.me.uk>");
	Serial.println("");

	// Connect to WiFi network
	Serial.println("Connecting to WiFi...");
	WiFi.mode(WIFI_STA);
#ifdef STAHOST
	WiFi.hostname(STAHOST);
#endif
	WiFi.begin(ssid, password);
	Serial.println("");

	// Wait for connection
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}
	/*
	  while (WiFi.status() != WL_CONNECTED) {
	    delay(500);
	    Serial.print(".");
	  }
	*/
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Set up mDNS responder:
	// - first argument is the domain name, in this example
	//   the fully-qualified domain name is "esp8266.local"
	// - second argument is the IP address to advertise
	//   we send our IP address on the WiFi network
	if (!MDNS.begin(STAHOST)) {
		Serial.println("Error setting up MDNS responder!");
		while (1) {
			delay(1000);
		}
	}
	Serial.println("mDNS responder started");

	// Set up OTA
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// No authentication by default
	// ArduinoOTA.setPassword("admin");

	// Password can be set with it's md5 value as well
	// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

	// Hostname defaults to esp8266-[ChipID]
#ifdef STAHOST
	ArduinoOTA.setHostname(STAHOST);
#endif

	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else { // U_FS
			type = "filesystem";
		}

		// NOTE: if updating FS this would be the place to unmount FS using FS.end()
		Serial.println("Start updating " + type);
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
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
	ArduinoOTA.begin();


	// Set up webserver
	webServer.on("/", handleRoot);
	webServer.on("/get", handleGetPort);
	webServer.on("/switch", handleSwitchPort);

	// enable CORS header in webserver results
	webServer.enableCORS(true);

	// enable ETAG header in webserver results from serveStatic handler
	//webServer.enableETag(true);

	/*
	// serve all static files
	webServer.serveStatic("/", LittleFS, "/");
	*/

	// handle cases when file is not found
	webServer.onNotFound([]() {
		// standard not found in browser.
		webServer.send(404, "text/html", FPSTR(notFoundContent));
	});

	// Start HTTP server
	webServer.begin();
	Serial.println("Web server started");

	// Add service to MDNS-SD
	MDNS.addService("http", "tcp", 80);
}

/****************************
 * Main loop
 */

void loop(void) {
	ArduinoOTA.handle();
	MDNS.update();
	webServer.handleClient();
}
