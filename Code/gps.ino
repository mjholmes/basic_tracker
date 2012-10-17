// GPS functions


// Sync chars that are at the start of each string
const byte uBloxSync1 = 0xB5;
const byte uBloxSync2 = 0x62;


void initGPS(void) {
  // Set airbourne mode
  setNav5();

  // Switch off periodic NMEA sentences, we'll poll the module when
  // we want some information. 40 is message ID, GGA/GLL/etc is the NMEA
  // sentence to change. The first 5 0s relate to an output port. On the u-blox max-6
  // the ports are ddc, usart1, usart2, usb and spi. The last 0 is reserved.
  // See UBLOX protocol spec section 21.11 (page 80).
  GPS_PORT.println("$PUBX,40,GGA,0,0,0,0,0,0*5A");  // MJH changed these from GGA,0,0,0,0*5A ie added 2 more 0s
  GPS_PORT.println("$PUBX,40,GLL,0,0,0,0,0,0*5C");
  GPS_PORT.println("$PUBX,40,GSA,0,0,0,0,0,0*4E");
  GPS_PORT.println("$PUBX,40,GSV,0,0,0,0,0,0*59");
  GPS_PORT.println("$PUBX,40,RMC,0,0,0,0,0,0*47");
  GPS_PORT.println("$PUBX,40,VTG,0,0,0,0,0,0*5E");

  // Allow the GPS time to process the requests
  delay(1000); // CHECK THIS TIMING WITH DATASHEET
  
  // Delete anything that might still be in the GPS Rx buffer
  clearGPSBuffer();
}
  
void setNav5(void) {
  // Make sure the GPS is in airbourne mode. See u-blox protocol spec
  // section 31.10.2 for details of the CFG-NAV5 command and section 24
  // (page 82) for UBX packet structure. u-center binary console can be
  // used to sniff this data rather than having to work it all out.
  uint8_t cfgNav5[] = {0xB5, 0x62, 0x06, 0x24,                // Sync chars mu and b plus class and id values
                       0x24, 0x00,                            // Length of data (in bytes, little endian) - 36dec
                       0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00,    // Data
                       0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
                       0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00,
                       0x64, 0x00, 0x2C, 0x01, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x16, 0xDC};                           // 16bit Checksum, 8bit Fletcher (RFC1146) calculated
                                                              // over class, id, length and data (not sync chars).
                      
  sendUBX(cfgNav5, sizeof(cfgNav5)/sizeof(uint8_t));
}


// Return the status of the GPS
boolean gpsLock(void) {
}

// Poll the GPS for UBX proprietary message. See UBLOX protocol spec section 21.1 (page 66)
void pollGPS(void) {
  GPS_PORT.println("$PUBX,00*33");
}

// Print some status on valid data received by TinyGPS
void gpsPrintStats() {
  unsigned long chars = 0;
  unsigned short good_sentences = 0, failed_cs = 0;
  gps.stats(&chars, &good_sentences, &failed_cs);
  #ifdef DEBUG
    DBG_PORT.print("TinyGPS Stats - ");
    DBG_PORT.print(chars, DEC);
    DBG_PORT.print("chars, ");
    DBG_PORT.print(good_sentences, DEC);
    DBG_PORT.print("good, ");
    DBG_PORT.print(failed_cs, DEC);
    DBG_PORT.println("failed cs.");
  #endif
}

// THIS IS NOT RIGHT
void gpsChecksum(byte *buffer) {
  byte CK_A = 0, CK_B = 0;
  for(int i=0;i<sizeof(&buffer)/sizeof(byte);i++) {
    CK_A = CK_A + buffer[i];
    CK_B = CK_B + CK_A;
  }
}

// Send a byte array of UBX protocol to the GPS
void sendUBX(uint8_t *MSG, uint8_t len) {
  for(int i=0; i<len; i++) {
    GPS_PORT.write(MSG[i]);
  }
}

// Calculate expected UBX ACK packet and parse UBX response from GPS
boolean getUBX_ACK(uint8_t *MSG) {
  uint8_t b;
  uint8_t ackByteID = 0;
  uint8_t ackPacket[10];
  unsigned long startTime = millis();
  #ifdef DEBUG
  DBG_PORT.print(" * Reading ACK response: ");
  #endif
 
  // Construct the expected ACK packet    
  ackPacket[0] = 0xB5;	  // header
  ackPacket[1] = 0x62;	  // header
  ackPacket[2] = 0x05;	  // class
  ackPacket[3] = 0x01;	  // id
  ackPacket[4] = 0x02;	  // length
  ackPacket[5] = 0x00;
  ackPacket[6] = MSG[2];  // ACK class
  ackPacket[7] = MSG[3];  // ACK id
  ackPacket[8] = 0;       // Checksum
  ackPacket[9] = 0;
 
  // Calculate the checksums
  for (uint8_t i=2; i<8; i++) {
    ackPacket[8] = ackPacket[8] + ackPacket[i];
    ackPacket[9] = ackPacket[9] + ackPacket[8];
  }
 
  while (1) {
 
    // Test for success
    if (ackByteID > 9) {
      // All packets in order!
      #ifdef DEBUG
      DBG_PORT.println(" (SUCCESS!)");
      #endif
      return true;
    }
 
    // Timeout if no valid response in 3 seconds
    if (millis() - startTime > 3000) {
      #ifdef DEBUG
      DBG_PORT.println(" (FAILED!)");
      #endif
      return false;
    }
 
    // Make sure data is available to read
    if (GPS_PORT.available()) {
      b = GPS_PORT.read();
 
      // Check that bytes arrive in sequence as per expected ACK packet
      if (b == ackPacket[ackByteID]) { 
        ackByteID++;
        #ifdef DEBUG
        DBG_PORT.print(b, HEX);
        #endif
      } 
      else {
        ackByteID = 0;	// Reset and look again, invalid order
      }
 
    }
  }
}




// Function to poll the "dynamic platform model" being used by the Ublox GPS module
// Sends a UBX command (requires the function sendUBX()) and waits 3 seconds
// for a reply from the module. It then isolates the byte which contains 
// the information regarding current mode. Possible values are:
//   0 Portable
//   2 Stationary
//   3 Pedestrian 
//   4 Automotive
//   5 Sea
//   6 Airborne with <1g Acceleration
//   7 Airborne with <2g Acceleration
//   8 Airborne with <4g Acceleration
// A full decription of the above modes can be found in section 2.1 (page 1)
// of the uBLOX protocol spec
// Adapted by jcoxon from getUBX_ACK() from the example code on UKHAS wiki
// http://wiki.ukhas.org.uk/guides:falcom_fsa03

// PROBLEM - THIS DOESNT DO CHECKSUMS AND DOESNT VERIFY THAT THE START BYTES ARE CORRECT
// NEED TO ADD SOME ERROR CHECKING AND START BIT CHECKING
uint8_t getNAV5dynModel(){
  uint8_t b, bytePos = 0;
  // Empty cfg-NAV5 message,  see UBLOX protocol spec section 31.10.1 (page 116)
  uint8_t getNAV5[] = { 0xB5, 0x62, 0x06, 0x24, // Sync chars mu and b plus class and id values
                        0x00, 0x00,             // 0 length data
                        0x2A, 0x84 };           // Checksum

  clearGPSBuffer();
  unsigned long startTime = millis();
  sendUBX(getNAV5, sizeof(getNAV5)/sizeof(uint8_t));
  while (1) {
    // Make sure data is available to read
    if (GPS_PORT.available()) {
      b = GPS_PORT.read();
      // Dynamic platform model is 1 byte unsigned int at addr 8
      // Header(2), ID(2), Length(2), Mask(2), dynModel(1)
      if(bytePos == 8){
        return b;
      }
      bytePos++;
    }

    // Timeout if no valid response in 3 seconds
    if (millis() - startTime > 3000) {
      return 98;
    }
  }
}

// Clear any data in the GPS serial port buffers by reading and discarding.
// If we're still reading after 3 seconds then return so we dont get locked
// into this loop. From Arduino > 1.0 Serial.flush() flushes data out of the
// port onto the transmit lines, it does not clear the Rx buffer.
void clearGPSBuffer(void) {
  unsigned int starttime = millis();
  while(GPS_PORT.available()) {
    GPS_PORT.read();
    if (millis() - starttime > 3000) break;
  }
}
