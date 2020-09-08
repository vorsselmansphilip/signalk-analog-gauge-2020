#include <Arduino.h>

#include "sensesp_app.h"
#include "transforms/linear.h"
#include "sensors/analog_input.h"
#include "sensors/digital_input.h"

#include "DFRobot_ST7687S_Latch.h"
#include <TimeLib.h>
#include "SPI.h"
#include "gauges/DFR0529.h"


#include "displays/AnalogGauge.h"
#include "transforms/analogvoltage.h"
#include "transforms/voltagedividerR2.h"
#include "transforms/temperatureinterpreter.h"
#include "transforms/kelvintocelsius.h"
#include "transforms/kelvintofahrenheit.h"
#include "transforms/debounce.h"
#include "transforms/moving_average.h"
#include "transforms/change_filter.h"
#include "transforms/median.h"
#include "signalk/signalk_output.h"
#include "transforms/transform.h"

const char* sk_path = "electrical.generator.engine.coolantTemperature";

const float Vin = 5; // Voltage sent into the voltage divider circuit that includes the analog sender
const float R1 = 5100.0; // The resistance, in Ohms, of the R1 resitor in the analog sender divider circuit


const float minAnalogGaugeVal = 327.594; // Minimum value to display on analog gauge
const float maxAnalogGaugeVal = 377.594; // Max value to display on analog gauge

//having fun with sinwaves
float sinVal;                                                   // delete me: variable which can hold the sine value
int x = 0;
double demoValue = 0.0;

#define Display DFRobot_ST7687S_Latch
// Wiring configuration and setup for TFT display
#ifdef ARDUINO_ESP8266_NODEMCU
  uint8_t pin_cs = D8, pin_rs = D1, pin_wr = D4, pin_lck = D2;
  uint8_t button_pin = D0; 
#else
  uint8_t pin_cs = D2, pin_rs = D1, pin_wr = D3, pin_lck = D4;
  uint8_t button_pin = D8;  
#endif

ReactESP app([] () {

#ifdef ARDUINO
  #ifndef SERIAL_DEBUG_DISABLED
  Serial.begin(115200);

  // A small arbitrary delay is required to let the
  // serial port catch up

  delay(1000);
  Debug.setSerialEnabled(true);
  #endif
#endif

  sensesp_app = new SensESPApp();


    

  // Initialize the LCD display
  Display* pDisplay = new Display(pin_cs, pin_rs, pin_wr, pin_lck);
  pDisplay->begin();

  DFR0529 *pGauge = new DFR0529(pDisplay, minAnalogGaugeVal, maxAnalogGaugeVal, "/gauge/display");
  pGauge->addGaugeEvent(DFR0529::GaugeEvent(1000.00,LL,DISPLAY_RED,alarm,buzzer));
  pGauge->addGaugeEvent(DFR0529::GaugeEvent(3000.00,L,DISPLAY_ORANGE,event,none));
  pGauge->addGaugeEvent(DFR0529::GaugeEvent(7000.00,H,DISPLAY_RED,event,none));
  pGauge->addGaugeEvent(DFR0529::GaugeEvent(8000.00,HH,DISPLAY_RED,alarm,buzzer));
  pGauge->addValueRange(DFR0529::ValueColor(349.817, 360.928, DISPLAY_GREEN));
  pGauge->addValueRange(DFR0529::ValueColor(360.928, 369.261, DISPLAY_YELLOW));
  pGauge->addValueRange(DFR0529::ValueColor(369.261, maxAnalogGaugeVal, DISPLAY_RED));

  pGauge->set_simulation(true);

  

  AnalogInput* pAnalogInput = new AnalogInput();
  DigitalInputValue* pButton = new DigitalInputValue(button_pin, INPUT, CHANGE);
  
  float actual_value = sinVal * 1024; //1024 is the analog value
 
 

  NumericTransform* pTempInKelvin;
  NumericTransform* pVoltage;
  NumericTransform* pR2;

  pAnalogInput->connectTo(new Median(10, "/gauge/voltage/samples")) ->
                connectTo(new MovingAverage(5, 1.0, "/gauge/voltage/avg")) ->
                connectTo(new AnalogVoltage()) ->
                connectTo(pVoltage = new Linear(1.0, 0.0, "/gauge/voltage/calibrate")) ->
                connectTo(pR2 = new VoltageDividerR2(R1, Vin)) ->
                connectTo(new TemperatureInterpreter("/gauge/temp/curve")) ->
                connectTo(new Linear(1.0, 0.0, "/gauge/temp/calibrate")) ->
                connectTo(pTempInKelvin = new ChangeFilter(1, 10, 6, "/gauge/output/change")) ->
                connectTo(new SKOutputNumber(sk_path, "/gauge/output/sk")) ->
                connectTo(pGauge);
 
  pGauge->setValueSuffix('k', 0);


  pTempInKelvin->connectTo(new KelvinToFahrenheit()) -> connectTo(pGauge, 1);
  pGauge->setValueSuffix('f', 1);
  pGauge->setDefaultDisplayIndex(1);
  
  
  pTempInKelvin->connectTo(new KelvinToCelsius()) -> connectTo(pGauge, 2);
  pGauge->setValueSuffix('c', 2);
    
  pVoltage->connectTo(pGauge, 3);
  pGauge->setValueSuffix('v', 3);
  pGauge->setPrecision(3, 3);

  pR2->connectTo(pGauge, 4);
  pGauge->setValueSuffix('o', 4);
  pR2->connectTo(new ChangeFilter(1, 1000, 6)) ->
       connectTo(new SKOutputNumber("", "/gauge/temp/sender/sk"));


  pButton->connectTo(new Debounce()) -> connectTo(pGauge);



  sensesp_app->enable();

});
