#include "mbed.h"
#include <cstdio>
#include <string>


//Junction 1 Setup
DigitalOut rLight_J1(p11); // Red Traffic Signal
DigitalOut gLight_J1(p12); // Green Traffic Signal
DigitalOut irTx_J1(p19); // IR Emitter
AnalogIn irRx_J1(p20); // IR Receiver
DigitalOut ind_J1(p15); // Sensor Indicator

//Junction 2 Setup
DigitalOut rLight_J2(p13); // Red Traffic Signal
DigitalOut gLight_J2(p14); // Green Traffic Signal
DigitalOut irTx_J2(p17); // IR Emitter
AnalogIn irRx_J2(p18); // IR Receiver
DigitalOut ind_J2(p16); // Sensor Indicator

// Pedestrian Crossing
DigitalIn pedSwitch(p21);  // Pedestrian Request Switch
DigitalOut pedRed(p22);  // Red Pedestrian Signal
DigitalOut pedGreen(p23);  // Green Pedestrian Signal
BusOut segment(p24,p25,p26,p27,p28,p29,p30); // Pedestrian Timer (e,d,c,g,f,a,b)

// Bluetooth Monitoring
Serial bth(p9,p10,9600);


// Character Mapping for 7 segment display
struct CharacterHexCodes {
    int ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, A, b, C, c, d, E, F, g, G, H, h, i, I, j, L, l, n, N, O, o, p, q, r, S, t, U, u, y;

    // Constructor to initialize the mappings
    CharacterHexCodes() {
        ZERO = 0x88; ONE = 0xBB; TWO = 0x94; THREE = 0x91; FOUR = 0xA3; FIVE = 0xC1; SIX = 0xC0; SEVEN = 0x9B; EIGHT = 0x80; NINE = 0x81; A = 0x82; b = 0xE0; C = 0xCC; c = 0xF4; d = 0xB0; E = 0xC4; F = 0xC6; g = 0x81; G = 0xC8; H = 0xA2; h = 0xE2; i = 0xDB; I = 0xBB; j = 0xD9; L = 0xEC; l = 0xEE; n = 0xF2; N = 0x8A; O = 0x88; o = 0xF0; p = 0x86; q = 0x83; r = 0xF6; S = 0xC1; t = 0xE4; U = 0xA8; u = 0xF8; y = 0xA1;
    }
};

// Class for the Presence Sensor
class TL_Sensor{
    private:
    // Define the IO, private as they should not be accessible outside of class
    DigitalOut* irEmitter;
    AnalogIn* irReceiver;
    DigitalOut* indicator;

    // To account for Ambient IR light 
    float measureAmbient(){
            float ambientReading;
            //Turn off emitter to read ambient
            *irEmitter = 0;
            //Wait for emitter to turn off completely
            wait(0.1);
            ambientReading = irReceiver->read();
            return ambientReading;
        }

    float takeReadingFloat(){
        float ambient = measureAmbient();
        //Turn on emitter
        *irEmitter = 1;
        //wait for emitter to power reach max brightness
        wait(0.1);
        //Take the current IR reading, discounting the ambient IR light.
        float current = irReceiver->read() - ambient; 

        return current;
    }

    public:
    // Presense Sensor Sensivitity, can be changed on the fly, for example with the use of a potentiometer.
    float sensitivity; 

    // Constructor
        TL_Sensor(DigitalOut* _irEmitter, AnalogIn* _irReceiver, DigitalOut* _indicator, float _sensitivity)
    : irEmitter(_irEmitter)
    , irReceiver(_irReceiver)
    , indicator(_indicator)
    , sensitivity(_sensitivity)
    {
        //Initially set all LEDs to LO
        *irEmitter = 0;
        *indicator = 0;     
    } 

    // Method for reporting back if a Vehicle is detected.
    // Does not implement any timing logic here. 
    bool checkForVehicle(){
        
        if(takeReadingFloat() > sensitivity){
            *indicator = 1;
            return true;
        } else {
            *indicator = 0;
            return false;
        }
    }
    


};

// Class for the Signal Lamps
class TL_Signal{
    private:
        // Define the IO, private as they should not be accessible outside of class
        DigitalOut* Red_Light;
        DigitalOut* Green_Light;

    public:
        bool isGreen; // Flag to indicate if the Junction is on Green

        // Constructor
        TL_Signal(DigitalOut* red, DigitalOut* green)
        : Red_Light(red)
        , Green_Light(green)
        {
            //Set Junction to Red initially
            *Red_Light = 1;
            *Green_Light = 0;
            isGreen = false;
        }

        void turnGreen(){
            *Red_Light = 0;
            *Green_Light = 1;
            isGreen = true;
        }
        void turnRed(){
            *Green_Light = 0;
            *Red_Light = 1;
            isGreen = false;
        }
};

class Junction{
    public:
    string junctionName; //Name of the Junction, used for monitoring
    TL_Sensor sensor; //Instance of the Presence sensor object
    TL_Signal signal; //Instance of the Traffic Light Signals object
    float junctionWaitLimit; // The length of time a vehicle needs to wait before triggering the lights
    Timer junctionTimer; // Timer to register how long a vehicle has been waiting

    //Constructor
    Junction(string _junctionName, float _junctionWaitLimit, DigitalOut* _irEmitter, AnalogIn* _irReceiver, DigitalOut* _indicatorLamp, float _sensitivity, DigitalOut* _redLight, DigitalOut* _greenLight)
        : junctionName(_junctionName)
        , junctionWaitLimit(_junctionWaitLimit)
        , sensor(_irEmitter, _irReceiver, _indicatorLamp, _sensitivity)
        , signal(_redLight,_greenLight){
    }

    // Accessible method to check state of Junction
    bool isGreen(){
        return signal.isGreen;
    }

    bool isVehicleWaiting(){
        printf("%s: Checking vehicle waiting\n", junctionName.c_str());

        //Check presence sensor
        if(sensor.checkForVehicle()){
            //Vehicle spotted
            printf("%s: Vehicle spotted, check Timer\n", junctionName.c_str());

            //Check if Timer started
            if((junctionTimer.read()==0)){
                //If a vehicle shows up, start a timer.
                printf("%s: Starting Timer...\n", junctionName.c_str());     
                junctionTimer.start();
                return false;
            } else if(junctionTimer.read()>junctionWaitLimit){
                printf("%s: Vehicle waiting %0.2f\n", junctionName.c_str(), junctionTimer.read());
                //Once the vehicle is confirmed waiting greater than time limit, reset the timer.
                junctionTimer.stop();
                junctionTimer.reset();
                return true;
            } else {
                return false;
            }
        } else {
            //If there is no vehicle waiting any more, reset the timer.
            printf("%s: No vehicle waiting.\n", junctionName.c_str());
            junctionTimer.stop();
            junctionTimer.reset();
            return false;
        }
    }
    void changeRed(){
        signal.turnRed();
    }
    void changeGreen(){
        signal.turnGreen();
    }
 
};
void setup(){
    int setup = 1;
    int i = 0;
    while(setup){
        switch(i){
            case 0: segment = 0x00; i++; break;
            case 1: segment = 0xFF; pedRed = 1; i++; break;
            case 2: pedRed = 0; pedGreen = 1; i++; break;
            case 3: pedGreen = 0; rLight_J1 = 1; i++; break;
            case 4: rLight_J1 = 0; gLight_J1 = 1; i++; break;
            case 5: gLight_J1 = 0; ind_J1 = 1; i++; break;
            case 6: ind_J1 = 0; irTx_J1 = 1; i++; break;
            case 7: irTx_J1 = 0; irTx_J2 = 1; i++; break;
            case 8: irTx_J2 = 0; ind_J2 = 1; i++; break;
            case 9: ind_J2 = 0; gLight_J2 = 1; i++; break;
            case 10: gLight_J2 = 0; rLight_J2 = 1; i++; break;
            case 11: rLight_J2 = 0; setup = 0; break;
            default: break;
        }
        wait(0.2);
    }
}

int main() {

    // Run start up diagnostics
    setup();

    // Instatiate a Junction object for each Junction in the Intersection
    Junction junctionOne(
        "Junction 1", // Junction Name
        2, // Junction Wait Limit (seconds)
        &irTx_J1, // IR Transmitter
        &irRx_J1, // IR Reciever
        &ind_J1, // Sensor Indicator Lamp
        0.3f, // Sensor Sensitivity
        &rLight_J1, // Traffic Signal Red Light
        &gLight_J1 // Traffic Signal Green Light
    );

    Junction junctionTwo(
        "Junction 2", // Junction Name
        5, // Junction Wait Limit (seconds)
        &irTx_J2, // IR Transmitter
        &irRx_J2, // IR Reciever
        &ind_J2, // Sensor Indicator Lamp
        0.3f, // Sensor Sensitivity
        &rLight_J2, // Traffic Signal Red Light
        &gLight_J2 // Traffic Signal Green Light
    );
    while(1){
        //Check Junction Two is Red
        if((junctionOne.isGreen())){
            bth.printf("Junction Two is Red\n");
            // If red, check if vehicle waiting
           if(junctionTwo.isVehicleWaiting()){
               bth.printf("Vehicle waiting at junction two\n");
               //If vehicle waiting, change lights.
               junctionOne.changeRed();
               wait(2); //Safety timer (TODO: Replace with timer)
               junctionTwo.changeGreen();
           }
        //Check Junction One is Red
        } else if((junctionTwo.isGreen())) {
            bth.printf("Junction One is Red\n");
            // If red, check if vehicle waiting
            if(junctionOne.isVehicleWaiting()){
                bth.printf("Vehicle waiting at junction two\n");
                //If vehicle waiting, change lights.
                junctionTwo.changeRed();
                wait(2); //Safety timer (TODO: Replace with timer)
                junctionOne.changeGreen();
            }
        } else {
            // Default Junction One to Green
            junctionOne.changeGreen();
        }
        //Update Indicator, replace with general update()
        junctionOne.sensor.checkForVehicle();
        junctionTwo.sensor.checkForVehicle();

    }
    return 0;
  }
