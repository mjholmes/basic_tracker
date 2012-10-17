#include <util/crc16.h>

// Initialise the radio
void initRadio(void) {
  // Setup pins
  if (DEBUG) DBG_PORT.println("Init radio pins");
  pinMode(RADIO_ENABLE_PIN,OUTPUT);
  pinMode(RADIO_TX_PIN,    OUTPUT);
}

// Enable the radio, on the NTX2 that means take EN high
void enableRadio(void){
  digitalWrite(RADIO_ENABLE_PIN, HIGH);
  if (DEBUG > 1) DBG_PORT.println("Radio enabled");
}

// Disable the readio, on the NTX2 take the EN pin low
void disableRadio(void){
  digitalWrite(RADIO_ENABLE_PIN, LOW);
  if (DEBUG > 1) DBG_PORT.println("Radio disabled");
}


// Function to generate a recognisable tone on the NTX2
void squwark(void) {
  digitalWrite(RADIO_TX_PIN, HIGH);
  delay(300);
  digitalWrite(RADIO_TX_PIN, LOW);
  delay(300);
  digitalWrite(RADIO_TX_PIN, HIGH);
  delay(300);
  digitalWrite(RADIO_TX_PIN, LOW);
  delay(300);
  digitalWrite(RADIO_TX_PIN, HIGH);
  delay(300);
  digitalWrite(RADIO_TX_PIN, LOW);
  delay(300);
  digitalWrite(RADIO_TX_PIN, HIGH);
}


/* Simple function to sent a char at a time to 
** rtty_txbyte function. 
** NB Each char is one byte (8 Bits)
*/
void rtty_txstring (char * string) {
  char c;

  //noInterrupts();
  c = *string++;
  while ( c != '\0') {
    rtty_txbyte (c);
    c = *string++;
  }
  //interrupts();
}


/* Simple function to sent each bit of a char to 
** rtty_txbit function. 
** NB The bits are sent Least Significant Bit first
**
** All chars should be preceded with a 0 and 
** proceded with a 1. 0 = Start bit; 1 = Stop bit
**
*/
void rtty_txbyte (char c) {
  int i;
  rtty_txbit (0); // Start bit

  // Send bits for for char LSB first
  // Change this here 7 or 8 for ASCII-7 / ASCII-8	
  for (i=0;i<7;i++) {
    if (c & 1) rtty_txbit(1); 
    else rtty_txbit(0);	
    
    c = c >> 1;
  }

  // Send stop bits. This also ensures that the radio is left in the MARK position as per RTTY spec
  rtty_txbit (1);
  rtty_txbit (1);
}

void rtty_txbit (int bit) {
  if (bit) {
    // high
    digitalWrite(RADIO_TX_PIN, HIGH);
  }
  else {
    // low
    digitalWrite(RADIO_TX_PIN, LOW);
  }
  
  // Inter bit delay sets the baudrate. We support 50 or 300, default is 50.
  if(300 == BAUDRATE) {
    // 300 BAUD
    delayMicroseconds(3333);
  }
  else {
    // 50 BAUD
    delayMicroseconds(20000);
  }

}


uint16_t gps_CRC16_checksum (char *string, const size_t preamble) {
  size_t i;
  uint16_t crc;
  uint8_t c;
 
  crc = 0xFFFF;
 
  // Calculate checksum ignoring the preamble chars
  for (i = preamble; i < strlen(string); i++) {
    c = string[i];
    crc = _crc_xmodem_update (crc, c);
  }
 
  return crc;
}
