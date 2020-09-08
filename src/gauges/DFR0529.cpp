#include <Arduino.h>
#include "DFR0529.h"
#include "GaugeIcons.h"
#include "images.h"
#include "sensesp_app.h"
#include <ESP8266WiFi.h>
#include <math.h>
#include <cstring>

#ifndef PI
#define PI 3.1457
#endif

#define MAX_GAUGE_INPUTS 5


DFR0529::GaugeEvent::GaugeEvent() {
}

//DFR0529::GaugeEvent::GaugeEvent(float Threshold, uint16_t Event_color) :  Threshold{Threshold}, Event_color{Event_color}{
//}


DFR0529::GaugeEvent::GaugeEvent(float Threshold, alarm_type Alarm_type, uint16_t Event_color, notification_type Notification_type,notification_action Notification_action) : Threshold{Threshold}, Alarm_type{Alarm_type}, Event_color{Event_color},Notification_type{Notification_type} ,Notification_action{Notification_action} {

}


DFR0529::ValueColor::ValueColor() {
}

DFR0529::ValueColor::ValueColor(float minVal, float maxVal, uint16_t color) : minVal{minVal}, maxVal{maxVal}, color{color} {
}

DFR0529::ValueColor::ValueColor(JsonObject& obj) : minVal{obj["minVal"]}, maxVal{obj["maxVal"]}, color{obj["color"]} {

}


DFR0529::DFR0529(DFRobot_Display* pDisplay,
                         double minVal, double maxVal,
                         String config_path) :
   NumericConsumer(),
   Configurable(config_path),
   display(*pDisplay), minVal{minVal}, maxVal{maxVal} {

   maxDisplayChannel = 0;
   currentDisplayChannel = 0;

   input = new double[MAX_GAUGE_INPUTS];
   valueSuffix = new char[MAX_GAUGE_INPUTS];
   std::memset(valueSuffix, ' ', MAX_GAUGE_INPUTS);
   precision = new uint8[MAX_GAUGE_INPUTS];
   std::memset(precision, 0, MAX_GAUGE_INPUTS);

   display.fillScreen(DISPLAY_BLACK);
   display.setTextColor(DISPLAY_WHITE);
   display.setTextBackground(DISPLAY_BLACK);
   display.setCursor(26, 58);
   display.drawBmp((uint8_t*)gImage_signalk_b_100_100, -50, -80, 100, 100,1,DISPLAY_BLUE);
   display.setTextSize(2);
   display.printf("SensESP");
   display.setCursor(25, 80);
   display.setTextSize(1);
   display.println("Connecting to");
   display.setCursor(20, 90);
   display.println("Signal K server");
   pGaugeIcon = (uint8_t*)image_data_icontemp;

}


void DFR0529::addValueRange(const ValueColor& newRange) {
   ValueColor* pRange = new ValueColor(newRange);
   valueColors.insert(*pRange);
}

void DFR0529::addGaugeEvent(const GaugeEvent& newEvent) {
   GaugeEvent* pGaugeEvent = new GaugeEvent(newEvent);
   //gaugeEvents.insert(*pGaugeEvent);
}


uint16_t DFR0529::getValueColor(float value) {

  std::set<ValueColor>::iterator it = valueColors.begin();

  while (it != valueColors.end()) {
     auto& range = *it;

     if (value >= range.minVal && value <= range.maxVal) {
        // Here is our match
        return range.color;
     }

     it++;
  } // while

  return defaultValueColor;

}



void DFR0529::setValueSuffix(char suffix, int inputChannel) {
   if (inputChannel >= 0 && inputChannel < MAX_GAUGE_INPUTS) {

      if (inputChannel > maxDisplayChannel) {
         maxDisplayChannel = inputChannel;
      }

      valueSuffix[inputChannel] = suffix;
   }
   else {
      debugW("WARNING! calling DFR0529::setValueSuffix with an input channel out of range - ignoring.");
   }
}


void DFR0529::setPrecision(uint8 precision, int inputChannel) {
   if (inputChannel >= 0 && inputChannel < MAX_GAUGE_INPUTS) {

      if (inputChannel > maxDisplayChannel) {
         maxDisplayChannel = inputChannel;
      }

      this->precision[inputChannel] = precision;
   }
   else {
      debugW("WARNING! calling DFR0529::setPrecision with an input channel out of range - ignoring.");
   }
}



void DFR0529::calcPoint(double pct, uint16_t radius, int16_t& x, int16_t& y) {
    double angle = PI * (1.0 - pct);
    x = radius * cos(angle);
    y = -(radius * sin(angle));
}


void DFR0529::drawHash(double pct, uint16_t startRadius, uint16_t endRadius, uint16_t color) {

  int16_t x0, y0, x1, y1;
  calcPoint(pct, startRadius, x0, y0);
  calcPoint(pct, endRadius, x1, y1);
  display.drawLine(x0, y0, x1, y1, color);
}


void DFR0529::drawGaugeRange(double minPct, double maxPct, double inc, uint16_t color) {
  for (double pct = minPct; pct <= maxPct; pct += inc) {
     drawHash(pct, 58, 70, color);
  }
}


void DFR0529::drawGaugeTick(double pct, uint16_t startRadius, uint16_t endRadius, uint16_t color) {
  double minVal = pct - 0.01;
  double maxVal = pct + 0.01;
  for (double val = minVal; val <= maxVal; val += 0.0033) {
     drawHash(val, startRadius, endRadius, color);
  }
}



void DFR0529::drawNeedle(double value, uint16_t color) {
    double pct = (value - minVal) / valueRange;
    drawHash(pct, 5, 40, color);
}


void DFR0529::drawDisplay() {
   
   double ax,ay,bx,by,cx,cy;
   double currentangle = gauge_start;
   double gauge_w = display.getWidth();
   double gauge_end = gauge_start - (360 - gauge_range);
 
   display.fillScreen(DISPLAY_BLACK);

   //TODO: Remove these lines
   //display.drawBmp((uint8_t*)gImage_fuel_56_56, -50, -50, 56, 56);
   //display.drawBmp((uint8_t*)gImage_preheat_28, -60, -30, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_battery_28, -30, -30, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_oil_28, 0, -30, 28, 28,1,DISPLAY_RED);
   //display.drawBmp((uint8_t*)gImage_fuel_28, 30, -30, 28, 28,1,DISPLAY_ORANGE);
   total_div = LL_div + L_div + IR_div + H_div + HH_div;
   div_angle = (gauge_range  / total_div); 
   gauge_angle = (gauge_range - (total_div-2 * gauge_div))/total_div;

   display.fillCircle(0,0,gauge_w/2,inRange_color);
   display.setTextBackground(inRange_color);
   
   display.setCursor(64,10);
   //display.print(total_div);
   display.setCursor(64,20);
   //display.print(div_angle);
   display.setCursor(64,30);
   //display.print(gauge_angle);
   display.setCursor(64,40);
   //display.print(currentangle);
   display.setCursor(64,50);
   //display.print(gauge_end);
   display.setCursor(64,60);
   //display.print(gauge_w);


   //calulate side b
   ax = 0;
   ay = 0;
   //calculate coordinates
   // b coordinates
   bx = gauge_w * cos(radians(gauge_start));
   by = gauge_w * sin(radians(gauge_start));
   //c coordinates
      //div_angle = 2.0;
   cx = gauge_w * cos(radians(gauge_end));
   cy = gauge_w * sin(radians(gauge_end));
   

   
   if(LL_div > 0){
      // Calculate the LL
      bx = gauge_w * cos(radians(gauge_start));
      by = gauge_w * sin(radians(gauge_start));
      // recalculate c point
      currentangle = currentangle + (LL_div * gauge_angle);
      cx = gauge_w  * cos(radians(currentangle));
      cy = gauge_w  * sin(radians(currentangle));
      display.fillTriangle(ax,ay,bx,by,cx,cy,LL_color);
   }

   if(L_div > 0){
      // Calculate the L
      bx = cx;
      by = cy;
      // recalculate c point
      currentangle = currentangle + (L_div * gauge_angle);
      cx = gauge_w  * cos(radians(currentangle));
      cy = gauge_w  * sin(radians(currentangle));
      display.fillTriangle(ax,ay,bx,by,cx,cy,L_color);
   }


      if(IR_div > 0){
      // Calculate the InRange point
      bx = cx;
      by = cy;
      // recalculate c point
      currentangle = currentangle + (IR_div * gauge_angle);
      cx = gauge_w  * cos(radians(currentangle));
      cy = gauge_w  * sin(radians(currentangle));
      }

   if(H_div > 0){
      // Calculate the H
      bx = cx;
      by = cy;
      // recalculate c point
      currentangle = currentangle + (H_div * gauge_angle);
      cx = gauge_w  * cos(radians(currentangle));
      cy = gauge_w  * sin(radians(currentangle));
      display.fillTriangle(ax,ay,bx,by,cx,cy,H_color);
   }

   if(HH_div > 0){
      // Calculate the HH
      bx = cx;
      by = cy;
      // recalculate c point
      currentangle = currentangle + (HH_div * gauge_angle);
      cx = gauge_w  * cos(radians(currentangle));
      cy = gauge_w  * sin(radians(currentangle));
      display.fillTriangle(ax,ay,bx,by,cx,cy,HH_color);
   }

   gauge_blindspot = 360 - gauge_range;
   
   if(gauge_blindspot >= 180){

      //calulate side b
      ax = 0;
      ay = 0;
      //calculate coordinates
      // b coordinates
      bx = gauge_w * cos(radians(90));
      by = gauge_w * sin(radians(90));
      //c coordinates
         //div_angle = 2.0;
      //cx = gauge_w * cos(radians(gauge_end));
      //cy = gauge_w * sin(radians(gauge_end));
      display.fillTriangle(ax,ay,bx,by,cx,cy,DISPLAY_BLACK);

      cx = bx;
      cy = by;

      gauge_blindspot = gauge_blindspot - (90 - currentangle);

   }

   if(gauge_blindspot >= 90){

      //calulate side b
      ax = 0;
      ay = 0;
      //calculate coordinates
      // b coordinates
      bx = gauge_w * cos(radians(gauge_start));
      by = gauge_w * sin(radians(gauge_start));
      //c coordinates
      //div_angle = 2.0;
      //cx = gauge_w * cos(radians(gauge_end));
      //cy = gauge_w * sin(radians(gauge_end));
      display.fillTriangle(ax,ay,bx,by,cx,cy,DISPLAY_BLACK);

   }





   //draw gauge lines starting from gauge_start
   display.setLineWidth(2);
   currentangle = gauge_start;
   bx = gauge_w * cos(radians(currentangle));
   by = gauge_w * sin(radians(currentangle));

   display.drawLine(ax, ay ,bx,by ,div_color);

   //display.drawLine(cx*7/8,cy*7/8,cx,cy,div_color);

   for(int x= 0; x < total_div; x++){
      currentangle = currentangle + gauge_angle;
      bx = gauge_w * cos(radians(currentangle));
      by = gauge_w * sin(radians(currentangle));
      display.drawLine(ax, ay ,bx ,by ,div_color);
   }
   // add a gauge plate
   display.fillCircle(0,0,50,DISPLAY_BLACK);





  // Do some pre-calc
  valueRange = maxVal - minVal;

  // Draw gauge ranges
  //drawGaugeTick(0.01, 45, 57, DISPLAY_WHITE);
  //drawGaugeTick(0.25, 50, 57, DISPLAY_WHITE);
  //drawGaugeTick(0.5, 45, 57, DISPLAY_WHITE);
  //drawGaugeTick(0.75, 50, 57, DISPLAY_WHITE);
  //drawGaugeTick(1.0, 45, 57, DISPLAY_WHITE);


  // Draw color ranges (if any)...
  std::set<ValueColor>::iterator it = valueColors.begin();
  while (it != valueColors.end()) {
     auto& range = *it;
     double minPct = (range.minVal - minVal) / valueRange;
     double maxPct = (range.maxVal - minVal) / valueRange;
     //drawGaugeRange(minPct, maxPct, 0.002, range.color);
     it++;
  } // while

  display.fillCircle(0, 0, 3, DISPLAY_WHITE);
  display.setLineWidth(1);
  //draw alignment crosshair
  //display.drawLine(0,-64,0,64,DISPLAY_WHITE);
  //display.drawLine(-64,0,64,0,DISPLAY_WHITE);

   //display.drawBmp((uint8_t*)gImage_fuel_56_56, -50, -50, 56, 56);
   //display.drawBmp((uint8_t*)gImage_preheat_28, -42, -28, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_battery_28, -13, -28, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_oil_28, 17, -28, 28, 28,1,DISPLAY_RED);
   //display.drawBmp((uint8_t*)gImage_fuel_28, 30, -30, 28, 28,1,DISPLAY_ORANGE);


  

}



void DFR0529::updateWifiStatus() {

   bool wifiConnected = sensesp_app->isWifiConnected();
   bool sigkConnected = sensesp_app->isSignalKConnected();

   int blinkRate = 2;

   bool blinkDisplay =  (blinkCount % blinkRate == 0);

   int newIcon;

   if (wifiConnected && sigkConnected) {
      newIcon = 2;
   }
   else if (blinkDisplay) {
     newIcon = (displayIcon+1) % 4;
     if (!wifiConnected) {
        if (newIcon == 2) {
           newIcon = 0;
        }
     } 
   }
   else {
     newIcon = displayIcon;
   }

   switch (newIcon) {
      case 0:
         if (displayIcon != 0) {
            //display.drawBmp((uint8_t*)image_data_iconwifi, -12, 35, 23, 20); 
            displayIcon = 0;
         }
         break;

      case 1:
      case 3:
         if (displayIcon != 1 && displayIcon != 3) {
            //display.fillRect(-14, 35, 28, 26, DISPLAY_BLACK);
            displayIcon = newIcon;
         }
         break;

      case 2:
         if (displayIcon != 2) {
            //temporary disabeld
            //display.drawBmp(pGaugeIcon, -14, 35, 28, 26);   
            displayIcon = 2;
         }
         break;

   } // switch

   blinkCount = (blinkCount + 1) % blinkRate;
}

void DFR0529::set_simulation(bool SimulationOn){
   SimulationActivated = SimulationOn;
}

void DFR0529::updateGauge() {

   float newDialValue = input[0];

   if (newDialValue > maxVal) {
      newDialValue = maxVal;
   }
   else if (newDialValue < minVal) {
      newDialValue = minVal;
   }

   uint16_t gaugeColor = getValueColor(newDialValue);

   // Update the dial...
   if (newDialValue != lastDialValue) {
      drawNeedle(lastDialValue, DISPLAY_BLACK);
      lastDialValue = newDialValue;
      drawNeedle(newDialValue, gaugeColor);

   }


   bool wifiConnected = sensesp_app->isWifiConnected();
   if (!wifiConnected || currentDisplayChannel > maxDisplayChannel) {
      if (!ipDisplayed) {
         // Display the device's IP address...
         display.fillRect(-64, 6, 128, 25, DISPLAY_BLACK);
         display.setTextColor(gaugeColor);
         display.setTextBackground(DISPLAY_BLACK);
         display.setCursor(40, 80);
         display.setTextSize(1);
         display.printf("%s", WiFi.localIP().toString().c_str());
         ipDisplayed = true;
      }
   }
   else {
      if (ipDisplayed) {
         // Clear out the IP address
         display.fillRect(-64, 6, 128, 25, DISPLAY_BLACK);
         ipDisplayed = false;
      }

      // Update the digital display...
      float newDisplayValue = input[currentDisplayChannel];
      if (newDisplayValue != lastDisplayValue) {

         display.setTextColor(gaugeColor);
         display.setTextBackground(DISPLAY_BLACK);
         display.setCursor(40, 78);
         display.setTextSize(2);
         char fmtStr[10];
         uint8 prec = precision[currentDisplayChannel];
         if (prec > 0) {
            int wholeSize = 6 - prec;
            sprintf(fmtStr, "%%%d.%df%%c" , wholeSize, (int)prec);
         }
         else {
            strcpy(fmtStr, "%5.0f%c ");
         }
         //temporary disabeld
         //display.printf(fmtStr, newDisplayValue, valueSuffix[currentDisplayChannel]);

      }
   }     // add dummy text for alignment
   display.setTextColor(inRange_color);
   display.setCursor(42,80);
   display.setTextSize(2);
   display.print("9999");
   // add the engineering units
   display.setCursor(49,100);
   display.setTextSize(1);
   display.print(" " + eng_units);
   // add placeholder hour counter
   display.setCursor(40,114);
   display.setTextSize(1);
   display.print("00:00:00");

}


void DFR0529::enable() {
    load_configuration();
    drawDisplay();
    //app.onRepeat(500, [this]() { this->updateWifiStatus(); });
   if(!SimulationActivated){
      app.onRepeat(750, [this]() { this->updateGauge(); });
   } else {
      app.onRepeat(50, [this]() { 
         Simulation_x ++;
         //== generatingh the next sine value ==//
         SimulationSinVal = (sin(radians(Simulation_x)));
         if (Simulation_x == 180){
            Simulation_x = 0;}
         display.setTextBackground(DISPLAY_BLACK);
         display.setCursor(15,84);
         display.print("Sin Value: ");
         //display.println(SimulationSinVal);
         display.print(SimulationSinVal,4);
         //display.printf("%s", WiFi.localIP().toString().c_str());
         this->SimulationActive(); 
         });
   }
}

void DFR0529::SimulationActive(){
   // do a bit the same as the original code
   display.setCursor(15,74);
   display.print("Simulation Active");
   display.setCursor(7,94);
   display.print("-: ");
   display.print(minVal);
   display.print(" +: ");
   display.print(maxVal);
   display.setCursor(15,104);
   display.print("Current : ");
   display.print(minVal+(SimulationSinVal * (maxVal - minVal)));
   input[0] = minVal+(SimulationSinVal * (maxVal - minVal));
   updateGauge();
   


}


void DFR0529::set_input(float newValue, uint8_t idx) {

   if (idx >= 0 && idx < MAX_GAUGE_INPUTS) {

      input[idx] = newValue;
   }
}


void DFR0529::set_input(bool buttonPressed, uint8_t idx) {

   if (buttonPressed) {
       if (currentDisplayChannel > maxDisplayChannel) {
         currentDisplayChannel = 0;
       }
       else {
         currentDisplayChannel++;
       }
      updateGauge();
   }
}


JsonObject& DFR0529::get_configuration(JsonBuffer& buf) {
  JsonObject& root = buf.createObject();
  root["default_display"] = currentDisplayChannel;
  root["minVal"] = minVal;
  root["maxVal"] = maxVal;

  JsonArray& jRanges = root.createNestedArray("ranges");
  for (auto& range : valueColors) {
    JsonObject& entry = buf.createObject();
    entry["minVal"] = range.minVal;
    entry["maxVal"] = range.maxVal;
    entry["color"] = range.color;
    jRanges.add(entry);
  }

  return root;
}


bool DFR0529::set_configuration(const JsonObject& config) {

  String expected[] = { "default_display", "minVal", "maxVal" };
  for (auto str : expected) {
    if (!config.containsKey(str)) {
      debugE("Can not set DFR0529 configuration: missing json field %s\n", str.c_str());
      return false;
    }
  }

  currentDisplayChannel = config["default_display"];
  minVal = config["minVal"];
  maxVal = config["maxVal"];

  JsonArray& arr = config["ranges"];
  if (arr.size() > 0) {
    valueColors.clear();
    for (auto& jentry : arr) {
        ValueColor range(jentry.as<JsonObject>());
        valueColors.insert(range);
    }

  }

  return true;

}


static const char SCHEMA[] PROGMEM = R"({
   "type": "object",
   "properties": {
      "minVal": { "title": "Minimum analog value", "type": "number"},
      "maxVal": { "title": "Maximum analog value", "type": "number"},
      "default_display": { "title": "Default input to display", "type": "number", "minimum": 0, "maximum": 5 },
      "ranges": { "title": "Ranges and colors",
                     "type": "array",
                     "items": { "title": "Range",
                                 "type": "object",
                                 "properties": {
                                       "minVal": { "title": "Min value for color", "type": "number" },
                                       "maxVal": { "title": "Max value for color", "type": "number" },
                                       "color": { "title": "Display color RGB", "type": "number" }
                                 }}}

   }
})";


String DFR0529::get_config_schema() {
   return FPSTR(SCHEMA);
}
