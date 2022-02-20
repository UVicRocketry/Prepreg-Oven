/*
    Prepreg Oven Main Control Sketch

	This sketch controls the oven through PWM control of an AC heater circuit.
	Buttons and an encoder are used to control the settings on an LCD and
	2 thermocouples are used for feedback.

	Author: JJ D

	UVic Rocketry
	October 2021
*/

#include <Wire.h>

// https://www.pjrc.com/teensy/td_libs_Encoder.html
#include <Encoder.h>

// Make sure to adjust the contrast with the little pot on the back
// if the text isn't very visible
#include <LiquidCrystal_I2C.h>

// https://github.com/VaSe7u/LiquidMenu 
// Note that in order for this to work you will need to edit the 
// LiquidMenu_config.h file in the library folder to use the I2C library
// since by default it uses the non I2C version of the LiquidCrystal lib
#include <LiquidMenu.h>

// https://github.com/fabianoriccardi/dimmable-light
#include <dimmable_light.h>

// Encoder pins for interacting with screen, setting temp/time
// For more info https://www.pjrc.com/teensy/td_libs_Encoder.html
#define encPinInt 3 // Must be interrupt capable
#define encPinReg 4 // Next two are any digital/analog pin (not 13 A6/A7 tho)
#define encPinBtn 5

// Analog pins for the 2 thermocouples  
#define therm1 A3
#define therm2 A2

// Controls the heater dimmer circuit
#define heaterSync 2 // Must be interrupt capable
#define heaterThy  7 // Any digital/analog

// Button for starting oven
#define startBtn 6

// TODO tune these PID values. 
#define kP 1.0
#define kI 1.0
#define kD 1.0

// PID variables
#define int_thresh 10   // Prevents "integral windup". See set_PID_heater()
#define PID_scale 0.1   // Scales PID value to roughly 0-255
float prev_error = 0;   // Error of the previous PID calculation
float integral   = 0;   // Accumulation of integral values in PID
long  timer_dt   = 0;   // Tracks millis() for PID dt value

// Target, and current real temperature
int tempSet  = 0;
int tempReal = 0;

// Remaining time of oven being on in seconds
long timeRemaining = 0;

// These temporarily store a new user selected variable since the user
// has the option to either save or discard it
int tempSetNew = 0;
long timeRemainingNew = 0;

// This stores value of millis() when timeRemaining was set
long timeStarted = 0;

// Stores the last value of the encoder so we can tell which dir it was spun
long oldEncoderPos = 0;

// Heater dimmer object
DimmableLight heater(heaterThy);

// Char arrays that contain formatted text for display
// For more info on why we can't just use a string:
// https://github.com/VaSe7u/LiquidMenu/issues/13
// TODO https://github.com/VaSe7u/LiquidMenu/blob/master/examples/C_functions_menu/C_functions_menu.ino
// shows how to do this without a char pointer. Can't get this to work?
char tempChars[10];   // eg [150/200 C] + null term
char* tempCharPtr = tempChars;

char timeChars[8];  // eg [10h 30m] + null term
char* timeCharPtr = timeChars;

char statusChars[20]; // eg Finished, or Running etc.
char* statusCharsPtr = statusChars;

// Refresh our display every 1 second
long timerLCD = 0;

// Set true when startBtn is pressed
bool started = false;

// Instantiate our LCD
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Encoder object will keep track of pulses from rotary knob
Encoder encoder(encPinInt, encPinReg);

// Here we set up our menus for the GUI. For more info look 
// at examples here: https://github.com/VaSe7u/LiquidMenu 

// General purpose lines
LiquidLine saveLine(1, 2, "Save");
LiquidLine cancelLine(1, 3, "Cancel");

// Home screen that is shown by default
LiquidLine tempLine(1, 0, "Temp: ", tempCharPtr);
LiquidLine timeLine(1, 1, "Time: ", timeCharPtr);
LiquidLine statusLine(1, 3, statusCharsPtr);
LiquidScreen mainScreen(tempLine, timeLine, statusLine);

// Screen for changing oven settings
LiquidLine newTempLine(1, 0, "Set Temp?");
LiquidLine newTimeLine(1, 1, "Set Time?");
LiquidScreen settingsScreen(newTempLine, newTimeLine, cancelLine);

// Temp changing screen
LiquidLine setTempLine(1, 0, "Temp: ", tempCharPtr);
LiquidScreen setTempScreen(setTempLine, cancelLine, saveLine);

// Time changing screen
LiquidLine setTimeLine(1, 0, "Time: ", timeCharPtr);
LiquidScreen setTimeScreen(setTimeLine, cancelLine, saveLine);

// This will contain all our screens
LiquidMenu menu(lcd);

// Indicator symbol Arrow thing for showing the selected line on the menu
// http://omerk.github.io/lcdchargen/ also in Focus Menu example on LiquidMenu
uint8_t rFocus[8] = {
	0b11111,
	0b11011,
	0b11101,
	0b00000,
	0b11101,
	0b11011,
	0b11111,
	0b11111
};

// This code is run once when the Arduino is powered on
void setup() {
	Serial.begin(9600);

	// Set whether pins will be inputs or outputs. Buttons are set to pullup so 
	// that they are not "floating" when not in use to prevent phantom presses
    pinMode(encPinBtn, INPUT_PULLUP);
    pinMode(startBtn, INPUT_PULLUP);

	// Initialize the display
	lcd.init();
	lcd.backlight();

    // Add our screens to the menu
    menu.init();
    menu.add_screen(mainScreen);
    menu.add_screen(settingsScreen);
    menu.add_screen(setTempScreen);
    menu.add_screen(setTimeScreen);

    // Adds a line select arrow symbol to our screens
    menu.set_focusSymbol(Position::LEFT, rFocus);
    settingsScreen.set_focusPosition(Position::LEFT);
    setTempScreen.set_focusPosition(Position::LEFT);
    setTimeScreen.set_focusPosition(Position::LEFT);

    // Attach functions to the LiquidLines
    // These run when the user clicks the encoder button on this line
    // The numbers are required so Liquid can keep track of what func is what
    newTempLine.attach_function(1, set_temp);
    newTimeLine.attach_function(1, set_time);

    // It would be nice to be able to re select the temp setting line if needed
    // This doesn't seem to really work tho.
    // setTempLine.attach_function(1, set_temp); 
    // setTimeLine.attach_function(1, set_time);

    saveLine.attach_function(1, return_to_mm);
    cancelLine.attach_function(1, return_to_mm);

    // Set up the heater dimmer object
    DimmableLight::setSyncPin(heaterSync);
    DimmableLight::begin();
}

// Main loop where code is run continuously.
void loop() {

    // Set heater dimmer value
    tempReal = (get_temperature(therm1) + get_temperature(therm2)) / 2;
    set_PID_heater(millis() - timer_dt);
    timer_dt = millis();

    // Update the display/time every 1 second (if on the main menu)
    if(millis() - timerLCD > 1000 && menu.get_currentScreen() == &mainScreen){

        // 1s has passed, check if the oven is finished
        if(started && timeRemaining > 0){
            timeRemaining -= (millis() - timeStarted) / 1000; 
            timeStarted = millis();
        }
        else if(started)
            oven_finished();

        // Update the strings for the display
        set_temp_str(tempSet);
        set_time_str(timeRemaining);
        set_status_chars();

        menu.update();
        timerLCD = millis();


        Serial.println("error: " + String(prev_error));
    }

    check_io();

    // TODO add a preheat function that gets called when the user presses start
}

// Encoder/btn logic
void check_io(){

    // This will only run once
    if(!started && digitalRead(startBtn) == LOW){
        started = true;
        timeStarted = millis();
    }

    // If the encoder button is pressed, run the function attached to that LiquidLine
    if(digitalRead(encPinBtn) == LOW && menu.get_currentScreen() != &mainScreen){
        delay(1000);
        menu.call_function(1);
    }
    else if (digitalRead(encPinBtn) == LOW){
        menu.next_screen();
        delay(1000);
    }

    // Let the encoder scroll through lines
    // the +/- 2 prevents double scrolling
    if(encoder.read() > oldEncoderPos + 2){
        menu.switch_focus(true); // Next line
        oldEncoderPos = 0;
        encoder.write(0);
    }
    else if(encoder.read() < oldEncoderPos - 2){
        menu.switch_focus(false); // Previous line
        oldEncoderPos = 0;
        encoder.write(0);
    }
}

// Status that appears on the main menu
void set_status_chars(){
        if(!started)
            strcpy(statusCharsPtr, "Press start...");
        else if(prev_error < 5) // Is the oven not at set temp? ie error (-ve)?
            strcpy(statusCharsPtr, "Heating...");
        else
            strcpy(statusCharsPtr, "Running...");
}

// Functions called when user clicks encoder button on menu line
void set_temp(){

    encoder.write(tempSet);
    oldEncoderPos = encoder.read();

    // Display the correct screen
    menu.change_screen(&setTempScreen);

    // Read encoder. Update screen/temp until the button is pressed
    while(digitalRead(encPinBtn) != LOW){

        // Only update the screen if the encoder has actually been turned
        if(oldEncoderPos != encoder.read()){
            
            // Each "tick" on the encoder is ~3 pulses so we divide out
            tempSetNew = encoder.read() / 3;

            // Max temp is 200C, min 20C
            if(tempSetNew > 200){
                tempSetNew = 200;
                encoder.write(200*3);
            }
            else if(tempSetNew < 20){
                tempSetNew = 20;
                encoder.write(20*3);
            }

            set_temp_str(tempSetNew);
            menu.update();
            oldEncoderPos = encoder.read();
        }
    }
    delay(500);

    encoder.readAndReset();

    // Once finished selecting their new temp, user can
    // save or cancel it in return_to_mm()
}

void set_time(){

    encoder.write(timeRemaining);
    oldEncoderPos = encoder.read();

    // Display the correct screen
    menu.change_screen(&setTimeScreen);

    // Read encoder. Update screen/time until the button is pressed
    while(digitalRead(encPinBtn) != LOW){

        // Only update the screen if the encoder has actually been turned
        if(oldEncoderPos != encoder.read()){

            // Each "tick" on the encoder is 4 pulses so we divide out
            if(encoder.read() > 0)
                timeRemainingNew = (encoder.read() * 60 * 5) / 4; // 5min increments
            else{
                timeRemainingNew = 0;
                encoder.readAndReset();
            }

            set_time_str(timeRemainingNew);
            menu.update();
            oldEncoderPos = encoder.read();
        }
    }
    delay(500);

    encoder.readAndReset();

    // Update time once the user is finished selecting their new time
    // The user has option of saving or canceling this in return_to_mm()
}

void return_to_mm(){

    // If the user clicked Save, update the temp
    if(menu.get_currentScreen() == &setTempScreen && 
       menu.get_focusedLine() == 2){
            tempSet = tempSetNew;
    }
    
    // If the user clicked Save, update the time. 2 is saveLine
    if(menu.get_currentScreen() == &setTimeScreen && 
       menu.get_focusedLine() == 2){
       timeRemaining = timeRemainingNew;
       timeStarted = millis();
    }

    menu.change_screen(&mainScreen);
}

void oven_finished(){
    tempSet = 0;
    strcpy(statusChars, "Finished!");
}

// This will fill tempChars array with formatted text for the display
void set_temp_str(int temp){
    /* 
        This builds a char array like this:
        "real/set C"
        "200/200 C"
        "20/200 C" etc.

        temp is passed as a param so it can be previewed like in set_temp()
    */

    String str = String(tempReal);
    str += '/';
    str += String(temp);
    str += " C";

    // Ensure we don't overflow the char array
    byte len = sizeof(tempChars)/sizeof(tempChars[0]);
    if(str.length() < len)
        str.toCharArray(tempChars, len);
    else
        strcpy(tempChars, "overflow");

}

// This will fill timeChars array with formated text for the display
void set_time_str(long time){
    /* 
        This builds a char array like this:
        "10h 30m"
        "9h 30m"
        "0h 0m"

        time is passed as a param so it can be previewed like in set_time()
    */

    String str;

    str += String(time / 3600) + "h ";
    str += String((time % 3600) / 60)  + "m";

    // Ensure we don't overflow the char array
    byte len = sizeof(timeChars)/sizeof(timeChars[0]);
    if(str.length() < len)
        str.toCharArray(timeChars, len);
    else
        strcpy(timeChars, "overflow");
}

// Given the pin number of a thermocouple, return the temperature
float get_temperature(int thermocouple_pin){

	// This is based on:
	// https://learn.adafruit.com/ad8495-thermocouple-amplifier/arduino?gclid=CjwKCAjwn8SLBhAyEiwAHNTJbcfRaDKa8byHDRf9tj9dTAZcaBSblQFl8Carp5acLA-WUgteql9GRBoCcCoQAvD_BwE
    
    #define AREF 4.55 // Nano is 4.5V 
    #define ADC_RESOLUTION 10 // Nano is 10 bit

	// Get the raw voltage pin on the thermocouple pin
	float voltage = analogRead(thermocouple_pin) * (AREF / (pow(2, ADC_RESOLUTION) - 1));

	// Convert the voltage to a value in C 
    // Adjust the x in voltage - x to tune the temperature.
	float temperature = (voltage - 1.59) / 0.005;

	return temperature;
}

// Sets a 0-100 value for heater using PID loop
// dt is the change in time since this was last called in milliseconds
void set_PID_heater(float dt){

    float error = tempSet - tempReal;

    // We only include integral when the oven is already
    // within int_thresh degrees of the tempSet to prevent it from dominating
    // This behaviour is called "integral windup"
    if(abs(error) < int_thresh)
        integral += error*dt / 1000; // ms->s
    else
        integral = 0;

    float derivative = (error - prev_error) / dt;
    float pwr = (error*kP) + (integral*kI) + (derivative*kD);

    pwr *= PID_scale;

    // The circuit is not rated for the full power of the heater
    // so we limit the values to 0-100 rather than 0-255
    if(pwr > 100)
        pwr = 100;
    else if(pwr < 0)
        pwr = 0;

    heater.setBrightness(pwr);

    prev_error = error;
}

// This function allows us to reset the Arduino in software (to restart oven)
// https://www.instructables.com/two-ways-to-reset-arduino-in-software/
void(* resetFunc) (void) = 0;