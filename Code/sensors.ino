// Initialise the ADC
void initBattSensor(void) {
  // Just in case it has been changed elsewhere
  analogReference(DEFAULT);
}

float getVBatt(void) {
  // Read the pin level. We'll discard this reading and then wait a little to make sure
  // that the multiplexer is on the right pin and the S/H caps have had time to charge.
  int vBatt = analogRead(VBATT_PIN);
  delay(5);
  
  // Now take the proper reading
  vBatt = analogRead(VBATT_PIN);
  if (DEBUG) {
    DBG_PORT.print("VBatt raw: ");
    DBG_PORT.println(vBatt, DEC);
  }
  return (float) vBatt * 0.00322 * VBATT_ADC_CAL;
}








// Initialuse and configure the dallas devices
void initDallasSensors(void) {
   // Start up the library
  sensors.begin();
  
  if (sensors.isParasitePowerMode()) DBG_PORT.println("1wire parasite on!");
  
  // locate devices on the bus
  unsigned int numSensors = sensors.getDeviceCount();
  if (DEBUG) {
    DBG_PORT.print("Found ");
    DBG_PORT.print(numSensors, DEC);
    DBG_PORT.println(" 1wire devs.");
  }
 
  // See how many sensors we've got and initialise them
  if (numSensors > 0) {
    // At least one sensor was found
    if (sensors.isConnected(intThermometer)) {
      // We've found our internal sensor
      if (DEBUG) {
        DBG_PORT.print("Int tsens found");
        DBG_PORT.println();
      }
    
      // Set resolution
      sensors.setResolution(intThermometer, TEMPERATURE_PRECISION);
        
      // Verify resolution
      unsigned int resolution = sensors.getResolution(intThermometer);
      if (resolution != TEMPERATURE_PRECISION) {
        DBG_PORT.print("Int tsens precis failed");
      }
      else DBG_PORT.println("Int tsens precis set");
    }
    else DBG_PORT.println("Int tsens not found");


    if (sensors.isConnected(extThermometer)) {
      // We've found our external sensor
      if (DEBUG) {
        DBG_PORT.print("Ext tsens found");
        DBG_PORT.println();
      }
    
      // Set resolution
      sensors.setResolution(extThermometer, TEMPERATURE_PRECISION);
        
      // Verify resolution
      unsigned int resolution = sensors.getResolution(extThermometer);
      if (resolution != TEMPERATURE_PRECISION) {
        DBG_PORT.print("Ext tsens precis failed");
      }
      else DBG_PORT.println("Ext tsens precis set");
    }
    else DBG_PORT.println("Ext tsens not found");
 
  }
  else {
    DBG_PORT.println("No tsens found");
  }

}
