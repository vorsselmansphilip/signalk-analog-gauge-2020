#include <Arduino.h>
#include "DFR0529.h"
#include "GaugeIcons.h"
#include "images.h"
//#include "weixinpic.c"
#include "sensesp_app.h"

#include <ESP8266WiFi.h>
#include <math.h>
#include <cstring>

#ifndef PI
#define PI 3.1457
#endif

#define MAX_GAUGE_INPUTS 5


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

  
  display.fillScreen(DISPLAY_BLACK);

  //display.drawBmp((uint8_t*)gImage_fuel_56_56, -50, -50, 56, 56);
   
   display.fillScreen(DISPLAY_BLACK);

   //TODO: Remove these lines
   //display.drawBmp((uint8_t*)gImage_preheat_28, -60, -30, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_battery_28, -30, -30, 28, 28,1,DISPLAY_ORANGE);
   //display.drawBmp((uint8_t*)gImage_oil_28, 0, -30, 28, 28,1,DISPLAY_RED);
   //display.drawBmp((uint8_t*)gImage_fuel_28, 30, -30, 28, 28,1,DISPLAY_ORANGE);



  // Do some pre-calc
  valueRange = maxVal - minVal;

  // Draw gauge ranges
  drawGaugeTick(0.01, 45, 57, DISPLAY_WHITE);
  drawGaugeTick(0.25, 50, 57, DISPLAY_WHITE);
  drawGaugeTick(0.5, 45, 57, DISPLAY_WHITE);
  drawGaugeTick(0.75, 50, 57, DISPLAY_WHITE);
  drawGaugeTick(1.0, 45, 57, DISPLAY_WHITE);


  // Draw color ranges (if any)...
  std::set<ValueColor>::iterator it = valueColors.begin();
  while (it != valueColors.end()) {
     auto& range = *it;
     double minPct = (range.minVal - minVal) / valueRange;
     double maxPct = (range.maxVal - minVal) / valueRange;
     drawGaugeRange(minPct, maxPct, 0.002, range.color);
     it++;
  } // while


  display.fillCircle(0, 0, 3, DISPLAY_WHITE);

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
            display.drawBmp((uint8_t*)image_data_iconwifi, -12, 35, 23, 20); 
            displayIcon = 0;
         }
         break;

      case 1:
      case 3:
         if (displayIcon != 1 && displayIcon != 3) {
            display.fillRect(-14, 35, 28, 26, DISPLAY_BLACK);
            displayIcon = newIcon;
         }
         break;

      case 2:
         if (displayIcon != 2) {
            display.drawBmp(pGaugeIcon, -14, 35, 28, 26);   
            displayIcon = 2;
         }
         break;

   } // switch

   blinkCount = (blinkCount + 1) % blinkRate;
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
         display.printf(fmtStr, newDisplayValue, valueSuffix[currentDisplayChannel]);

      }
   }

}


void DFR0529::enable() {
    load_configuration();
    drawDisplay();
    app.onRepeat(500, [this]() { this->updateWifiStatus(); });
    app.onRepeat(750, [this]() { this->updateGauge(); });
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
