// Misc utilities


// Set the LED PIN to be output
void initLED(void){
  pinMode(LED_PIN, OUTPUT);
}

// Switch the LED on
void statusLEDOn(void) {
  digitalWrite(LED_PIN, HIGH);
}

// Switch the LED off
void statusLEDOff(void) {
  digitalWrite(LED_PIN, LOW);
}
