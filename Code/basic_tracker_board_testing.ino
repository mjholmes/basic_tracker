/*
      Basic tracker board code. MJH 2012-10-17
	  TODO: Add time to telemtry string (hh:mm)
	        Remove #ifdefs for DEBUG
			Replace strcat (strncat) and add overflow checking
			Modify to call getNAV5dynModel all the time, not just when debugging
			Make sure all acknowledgements are in place (and correct)
*/

#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DBG_PORT debugSerial
#define GPS_PORT Serial

#define DEBUG 2

const int   VBATT_PIN     = 0;
const float VBATT_ADC_CAL = 2.0;

const int BAUDRATE         = 50;
const int RADIO_TX_PIN     = 7;
const int RADIO_ENABLE_PIN = 6;

// Dallas "micro-lan" is on digital pin 9 of the Arduino and we're using 11 bit precision
// although 9 is probably adequate.
const int ONEWIRE_PIN           = 3;
const int TEMPERATURE_PRECISION = 11;

// The Status LED is on digital pin 2
const int LED_PIN = 2;

// Our callsign for tracking
const char callSign[] = "MJH";

// Preamble/sync/start of string chars for fldigi. Not included in checksum
const char rttyPreamble[] = "$$$$";

// Setup the software serial library for debugging on pins 4 (Tx) and 5 (Rx)
SoftwareSerial debugSerial(5, 4);

// Setup a TinyGPS instance to do the heavy lifting for us
TinyGPS gps;

// Setup a onewire instance and pass its reference to to DTC
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// The addresses of our DS18B20s, you can get these from the "multiple" example that
// ships with the DallasTemperature library.
DeviceAddress intThermometer = { 0x28, 0x54, 0x19, 0x4A, 0x00, 0x00, 0x00, 0x98 };
DeviceAddress extThermometer = { 0x28, 0xEB, 0xE4, 0x54, 0x02, 0x00, 0x00, 0x93 };


// GPS data will be stored here
float gpsLat = 0.0, gpsLong = 0.0;
long  gpsAltitude = -1;
int   gpsSatCount = -1;
byte  gpsHour = 100, gpsMinute = 100, gpsSecond = 100, gpsHundredths = 0;
unsigned long gpsDate = 0, gpsTime = 0, gpsFixAge = 0, ge = 0;

// Stringified versions of the GPS data.
char gpsLatBuf[10] = "NONE", gpsLongBuf[10] = "NONE";

// Stringified verions of temperature
char intTempBuf[7] = "NONE", extTempBuf[7] = "NONE";

// Stringified version of battery voltage
char vBattBuf[6] = "NONE";

// Our telemetry string will be stored here.
char rttyBuf[100];

unsigned long stringID = 0;
boolean hadFix = 0;


// Setup routines. This is run once at power on before loop(). It also runs
// when the WDT expires (if enabled)
void setup(void) {

  // Just wait a couple of secs so external devices can get connected up (eg OpenLog)
  delay(1000);
  // Initialise LED and switch on to show setup has started
  initLED();
  statusLEDOn();
  
  // start serial ports
  GPS_PORT.begin(9600);
  DBG_PORT.begin(9600);
  
  if(DEBUG > 1 ) DBG_PORT.println("Boot start");
  
  // Initilise the other components
  delay(2000);
  initGPS();
  delay(1000);
  
  initDallasSensors();
  
  initRadio();
  enableRadio();
  
  initBattSensor();
  if(DEBUG > 1) DBG_PORT.println("Boot complete");
  statusLEDOff();

}


unsigned long lastPoll = 0, lastTx = 0;

void loop(void) {
  
  if (DEBUG > 1) DBG_PORT.println("");

  boolean validEncode = 0;
  unsigned long encodeStartTime = millis();
  
  // Process GPS device. If TinyGPS doesnt have a valid string
  // within 5 seconds then we drop back to the main loop. 
  while( !validEncode ) {
    if ( (millis() - encodeStartTime ) > 5000) {
      if (DEBUG > 1) DBG_PORT.println("Timeout");
      break;
    }

    // Poll the GPS every few seconds
    while (!GPS_PORT.available()) {
      unsigned long now = millis();

      if( (now - lastPoll) > 3000) {
        pollGPS();
        lastPoll = now;
      }
    }

    while (GPS_PORT.available()) {
      // Process the GPS data and update all the global variables
      if (gps.encode(GPS_PORT.read())) {
        validEncode = 1;
        updateGPSData();
      }
    }
  }

  // Get temperatures
  sensors.requestTemperatures();
  dtostrf(sensors.getTempC(intThermometer),3,1,intTempBuf);
  dtostrf(sensors.getTempC(extThermometer),3,1,extTempBuf);
 
  // Get battery voltage
  dtostrf(getVBatt(),3,2,vBattBuf);

  // Wait at least 2secs between each Tx (RTTY normally takes at least this long to Tx anyway)
  if (millis() - lastTx > 2000) {
    
    sprintf(rttyBuf, "%s%s,%ld,%d,%s,%s,%ld,%s,%s,%s", rttyPreamble, callSign, stringID, gpsSatCount, gpsLatBuf, gpsLongBuf, gpsAltitude, intTempBuf, extTempBuf, vBattBuf);
    unsigned int checksum = gps_CRC16_checksum(rttyBuf, strlen(rttyPreamble));
    char checksumBuf[7] = "*TEST\n";
    sprintf(checksumBuf, "*%04X\n", checksum);
    strcat(rttyBuf, checksumBuf);

    if (DEBUG) DBG_PORT.print(rttyBuf);

    lastTx = millis();
    rtty_txstring(rttyBuf);

    stringID++;
  }
}












void updateGPSData(void) {
      // Get some stats from GPS. UBX POLL string doesnt not include date.
      gps.get_datetime(&gpsDate, &gpsTime, &gpsFixAge);
      gpsSatCount = gps.sats();

      // Only update location info if we have view of at least 3 sats.
      if (gpsSatCount > 2) {
        gpsAltitude = gps.altitude()/100;
        gps.f_get_position(&gpsLat, &gpsLong, &gpsFixAge);
        
        // Put the position data into string buffers (sprintf on Arduino
        // does not print floats).
        dtostrf(gpsLat,7,4,gpsLatBuf);
        dtostrf(gpsLong,7,4,gpsLongBuf);
        
        // We've updated our GPS location and altitude data so update the
        // timestamp associated with it as well.
        ge = millis();
      }
      
      if(DEBUG > 1) {
        DBG_PORT.print("Alt: ");
        DBG_PORT.println(gpsAltitude);

        DBG_PORT.print("Lat: ");
        DBG_PORT.print(gpsLat);
        DBG_PORT.print(" Long: ");
        DBG_PORT.println(gpsLong);
        
        // Print TinyGPS stats every 20 loops
        if ( (stringID % 20) == 0 ) {
          gpsPrintStats();
        }
      }

      // From time to time check NAV5 dynamic model
      #ifdef DEBUG
        if ( (stringID % 20) == 0 ) {
          if (getNAV5dynModel() == 6) {
            DBG_PORT.println("dynModel OK");
          }
          else {
            DBG_PORT.println("dynModel wrong");
            setNav5();
          }
        }
      #endif
      DBG_PORT.print("Time ");
      DBG_PORT.println(gpsTime);
}
