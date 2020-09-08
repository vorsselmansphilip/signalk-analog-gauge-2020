#ifndef _dfr0529_H_
#define _dfr0529_H_

#include <set>

#include <DFRobot_Display.h>

#include <functional>
#include <stdint.h>
#include <system/valueconsumer.h>
#include <system/configurable.h>
#include <system/enable.h>
#include <system/valueproducer.h>
/*
* Select the Alarm type LL=Low Low,L = Low,H=High,HH=High High
*/
enum alarm_type {
  LL,
  L,
  H,
  HH
};

/*
* The Notification type event or alarm
*/

enum notification_type {
  event,
  alarm
};

/*
* The Notification action none,buzzer,delta,buzzer_delta
*/

enum notification_action {
  none,
  buzzer,
  delta,
  buzzer_delta
};



/**
 * https://wiki.dfrobot.com/2.2_inches_TFT_LCD_Display_V1.0_(SPI_Interface)_SKU_DFR0529
 * An analog gauge that displays on an 2.2 inch round LCD display.  The gauge supports up to five inputs
 * with each input having a specialized display suffix.
 *   If a BooleanProducer is attached to the gauge, the gauge can scroll thru 
 * five inputs on the digital display.  The analog dial will always display 
 * the incomming value of input 0.
 * 
 */
class DFR0529 : public NumericConsumer, public BooleanConsumer,
                    public Configurable, public Enable {

    public:
        class ValueColor {
            public:
               float minVal;
               float maxVal;
               uint16_t color;

               ValueColor();
               ValueColor(float minVal, float maxVal, uint16_t color);
               ValueColor(JsonObject& obj);

               friend bool operator<(const ValueColor& lhs, const ValueColor& rhs) { return lhs.minVal < rhs.minVal; }               
        };

        class GaugeEvent {
            public:
                float Threshold;
                alarm_type Alarm_type=HH;
                uint16_t Event_color = DISPLAY_RED;
                notification_type Notification_type = alarm;
                notification_action Notification_action = delta;
                

                GaugeEvent();
                /*
                * float Threshold
                * AlarmType LL,L,H,HH
                * EventColor DISPLAYRED
                * notification_type none,buzzer,delta
                * 
                * */
                //GaugeEvent(float Threshold, uint16_t Event_color);   
                //GaugeEvent(float Threshold, alarm_type Alarm_type, uint16_t Event_color, notification_type Notification_type);              
                GaugeEvent(float Threshold, alarm_type Alarm_type,uint16_t Event_color, notification_type Notification_type ,notification_action Notification_action);
                

        };


        DFR0529(DFRobot_Display* pDisplay,
                    double minVal = 0.0, double maxVal = 100.0,
                    String config_path = "");

        virtual void enable() override;

        /**
         * Handles input of new values to display in digital portion.
         */
        virtual void set_input(float newValue, uint8_t inputChannel = 0) override;

        /**
         * Handles button presses
         */
        virtual void set_input(bool newValue, uint8_t inputChannel = 0) override;

        /*
        * Activates a simulation 
        */

        virtual void set_simulation(bool SimulationOn = false);


        void setGaugeIcon(uint8_t* pNewIcon) { pGaugeIcon = pNewIcon; } 

        void setValueSuffix(char suffix, int inputChannel = 0);

        void setPrecision(uint8 precision, int inputChannel = 0);

        void setDefaultDisplayIndex(int inputChannel) { currentDisplayChannel = inputChannel; }


        /**
         * Value ranges allow for setting gauge colors like "normal operating range"
         * and "red line" values.
         */
        void addValueRange(const ValueColor& newRange);

        /**
         * GaugeEvent allows for setting gauge colors like "normal operating range"
         * and "red line" values.
         */
        void addGaugeEvent(const GaugeEvent& newEvent);

        /**
         * Returns the color associated with the specified value. If no specific color
         * range can be found, the default color is returned.
         * @see addValueRange()
         * @see setDefaultValueColor
         */
        uint16_t getValueColor(float value);


        /**
         * Sets the default color for the display when a value range is requested an no specified
         * value range color can be found.
         */
        void setDefaultValueColor(uint16_t newDefaultColor) { defaultValueColor = newDefaultColor; }


        uint16_t getDefaultValueColor() { return defaultValueColor; }

        // For reading and writing the configuration
        virtual JsonObject& get_configuration(JsonBuffer& buf) override;
        virtual bool set_configuration(const JsonObject& config) override;
        virtual String get_config_schema() override;
        
    private:
        // values needed for the calculation
        // TODO: move precision to here uint8* precision;
        double lower_limit = 0.0;
        double upper_limit = 9999.0;
        String eng_units = "RPM";
        // values to needed to calculate gauge ring
        double gauge_range = 180.0;
        double gauge_start = 160.0;
        uint16_t inRange_color = DISPLAY_GREEN;
        uint16_t div_color = DISPLAY_WHITE;

        double LL_Threshold = 1000.0;
        double L_Threshold = 2000.0;
        double H_Threshold=6000.0;
        double HH_Threshold=7000.0;
        //colors for the limits
        uint16_t LL_color = DISPLAY_RED;
        uint16_t L_color = DISPLAY_ORANGE;
        uint16_t H_color = DISPLAY_ORANGE;
        uint16_t HH_color = DISPLAY_RED;

        // placeholders for the divisions
        int LL_div = 0;
        int L_div = 0;
        int IR_div = 4;
        int H_div = 2;
        int HH_div = 2;

        // don't touch my privates

        double gauge_div = 4;
        double gauge_needle_last_x,gauge_needle_last_y;
        int total_div; 
        double div_angle;
        double gauge_angle;
        double gauge_blindspot;

        bool SimulationActivated;
        float SimulationSinVal;                                                   
        int Simulation_x = 0;



        double minVal;
        double maxVal;
        double valueRange;

        double lastDialValue = -99.9;
        double lastDisplayValue = -99.9;
        double* input;
        char* valueSuffix;
        uint8* precision;
        int currentDisplayChannel;
        int maxDisplayChannel;
        bool ipDisplayed = false;

        uint16_t defaultValueColor = DISPLAY_WHITE;

        int blinkCount = 0;
        int displayIcon = -1;
        uint8_t* pGaugeIcon;

        DFRobot_Display& display;

        void drawDisplay();
        void updateGauge();
        void updateWifiStatus();
        void SimulationActive();

        void calcPoint(double pct, uint16_t radius, int16_t& x, int16_t& y);
        void drawHash(double pct, uint16_t startRadius, uint16_t endRadius, uint16_t color);
        void drawGaugeRange(double minPct, double maxPct, double inc, uint16_t color);
        void drawGaugeTick(double pct, uint16_t startRadius, uint16_t endRadius, uint16_t color);
        void drawNeedle(double value, uint16_t color);

        std::set<ValueColor> valueColors;
        std::set<GaugeEvent> gaugeEvents;
};

#endif