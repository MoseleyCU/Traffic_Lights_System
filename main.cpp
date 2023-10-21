#include "mbed.h"
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
InterruptIn pedSwitch(p21);  // Pedestrian Request Switch
DigitalOut pedRed(p22);  // Red Pedestrian Signal
DigitalOut pedGreen(p23);  // Green Pedestrian Signal
BusOut segment(p24,p25,p26,p27,p28,p29,p30); // Pedestrian Timer (e,d,c,g,f,a,b)

// Bluetooth Adapter
Serial bth(p9,p10,9600);


// Character Mapping for 7 segment display
struct CharacterHexCodes {
    int ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, A, b, C, c, d, E, F, g, G, H, h, i, I, j, L, l, n, N, O, o, p, q, r, S, t, U, u, y;

    // Constructor to initialize the mappings
    CharacterHexCodes() {
        ZERO = 0x88; ONE = 0xBB; TWO = 0x94; THREE = 0x91; FOUR = 0xA3; FIVE = 0xC1; SIX = 0xC0; SEVEN = 0x9B; EIGHT = 0x80; NINE = 0x81; A = 0x82; b = 0xE0; C = 0xCC; 
        c = 0xF4; d = 0xB0; E = 0xC4; F = 0xC6; g = 0x81; G = 0xC8; H = 0xA2; h = 0xE2; i = 0xDB; I = 0xBB; j = 0xD9; L = 0xEC; l = 0xEE; n = 0xF2; N = 0x8A; O = 0x88; 
        o = 0xF0; p = 0x86; q = 0x83; r = 0xF6; S = 0xC1; t = 0xE4; U = 0xA8; u = 0xF8; y = 0xA1;
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
    float sensitivity; // Presense Sensor Sensivitity, can be changed on the fly, for example with the use of a potentiometer.

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
    friend class Junction; // Allow Junction class to access private members
    private:
        // Define the IO, private as they should not be accessible outside of class
        DigitalOut* Red_Light;
        DigitalOut* Green_Light;
        bool isGreen; // Flag to indicate if the Junction is on Green

    public: 
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
    Timer junctionTimer; // Measure time vehicle waiting
    float junctionWaitLimit; // Time a vehicle needs to wait before registering as a Waiting Vehicle.
    bool triggered; // Flag variable for triggering junction without vehicle waiting.
    bool previousState; // Temp variable for holding previous state of junction (for counting vehicles)
    bool vehicleCounterStarted; // Flag for enabling/disabling vehicle counting.
    int surplusVehicleCount; // Current vehicle count (while other junction waiting)
    int surplusVehicleLimit; // Max vehicle count (while other junction waiting)
    bool remoteTrigger; // Allows for bluetooth override


    //Constructor
    Junction(string _junctionName, DigitalOut* _irEmitter, AnalogIn* _irReceiver, DigitalOut* _indicatorLamp, float _sensitivity, DigitalOut* _redLight, DigitalOut* _greenLight, float _junctionWaitLimit, int _surplusVehicleLimit)
        : junctionName(_junctionName)
        , sensor(_irEmitter, _irReceiver, _indicatorLamp, _sensitivity)
        , signal(_redLight,_greenLight)
        , junctionWaitLimit(_junctionWaitLimit)
        , surplusVehicleLimit(_surplusVehicleLimit){
            triggered = false;
            vehicleCounterStarted = false;
            previousState = false;
            surplusVehicleCount = 0;
            remoteTrigger = false;
    }

    // Accessible method to check state of Junction
    bool isGreen(){
        return signal.isGreen;
    }

    bool isVehicleWaiting(){
        //Check presence sensor
        if(sensor.checkForVehicle()){
            //Vehicle spotted
            previousState = true;
            //Check if Timer started
            if((junctionTimer.read()==0)){
                //If a vehicle shows up, start a timer.    
                junctionTimer.start();
                return false;
            } else if(junctionTimer.read()>junctionWaitLimit){
                //Once the vehicle is confirmed waiting greater than time limit, reset the timer.
                junctionTimer.stop();
                junctionTimer.reset();
                return true;
            } else {
                return false;
            }

        } else {
            //Count vehicles
            if(previousState){
                surplusVehicleCount++;
                if(isGreen()){
                   if(vehicleCounterStarted){
                    bth.printf("%s: %i vehicles left to pass.\n",junctionName,(surplusVehicleLimit - surplusVehicleCount));
                    } 
                }
                previousState = false;
            }

            //If there is no vehicle waiting any more, reset the timer.
            junctionTimer.stop();
            junctionTimer.reset();

            return false;
        }
    }
    void changeRed(){
        if(signal.isGreen){
            signal.turnRed();
            bth.printf("%s: Turned Red\n", junctionName);
        }
        
    }
    void changeGreen(){
        if(!signal.isGreen){
            signal.turnGreen();
            bth.printf("%s: Turned Green\n", junctionName);
            
        }
        
    }
    void startVehicleCounter(){
            vehicleCounterStarted = true;
            surplusVehicleCount = 0;
    }
    void stopVehicleCounter(){
        vehicleCounterStarted = false;
    }
};

class PedestrianCrossing {
    private:
    DigitalOut* pedRedLight;
    DigitalOut* pedGreenLight;
    CharacterHexCodes character;
    bool isGreen;

    public:
    Timer waitingTimer;
    bool remoteTrigger;

    PedestrianCrossing(DigitalOut* _pedRedLight, DigitalOut* _pedGreenLight)
    : pedRedLight(_pedRedLight)
    , pedGreenLight(_pedGreenLight){
        isGreen = false;
        remoteTrigger = false;
    }
    void StartWaitingTimer(){
        bth.printf("Pedestrian Waiting...\n");
        waitingTimer.start();
    }
    void changeGreen(){
        *pedRedLight = 0;
        *pedGreenLight = 1;
        isGreen = true;
        bth.printf("Pedestrian Crossing is Green\n");
        startCountdown();
        changeRed();
    }
    void changeRed(){
        *pedGreenLight = 0;
        *pedRedLight = 1;
        isGreen = false;
        waitingTimer.stop();
        waitingTimer.reset();
        bth.printf("Pedestrian Crossing is Red\n");
    }
    void startCountdown(){
        bth.printf("Starting Pedestrian Countdown\n");
        for(int i = 9;i>=0;i--){
            switch (i){
                case 9: segment = character.NINE; break;
                case 8: segment = character.EIGHT; break;
                case 7: segment = character.SEVEN; break;
                case 6: segment = character.SIX; break;
                case 5: segment = character.FIVE; break;
                case 4: segment = character.FOUR; break;
                case 3: segment = character.THREE; break;
                case 2: segment = character.TWO; break;
                case 1: segment = character.ONE; break;
                case 0: segment = character.ZERO; break;
                default:break;         
            }
            wait(1); // Wait is used here as a safety feature (we don't want accidental trigger of the lights)
        }
        segment = 0xFF; // Clear 7 seg
    }
};

void startup(){
    bth.printf("Starting up...\n");
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
    bth.printf("Start up complete.\n");
}

// Declared globally to work with interrupt
PedestrianCrossing ped( 
    &pedRed,
    &pedGreen
);

//Interrupt function
void StartPedTimer(){ //
    ped.StartWaitingTimer();
}

int main() {

    // Run start up diagnostics
    startup();

    float junctionWaitLimit = 0.5; // Minimum required time for a vehicle to be waiting at a junction before the safePassageTimer is started

    // Instatiate a Junction object for each Junction in the Intersection
    Junction junctionOne(
        "Junction 1", // Junction Name
        &irTx_J1, // IR Transmitter
        &irRx_J1, // IR Reciever
        &ind_J1, // Sensor Indicator Lamp
        0.3f, // Sensor Sensitivity
        &rLight_J1, // Traffic Signal Red Light
        &gLight_J1, // Traffic Signal Green Light
        junctionWaitLimit, // min. Wait Time at Junction
        5 // Surplus vehicle limit
    );

    Junction junctionTwo(
        "Junction 2", // Junction Name
        &irTx_J2, // IR Transmitter
        &irRx_J2, // IR Reciever
        &ind_J2, // Sensor Indicator Lamp
        0.3f, // Sensor Sensitivity
        &rLight_J2, // Traffic Signal Red Light
        &gLight_J2, // Traffic Signal Green Light
        junctionWaitLimit, // min. Wait Time at Junction
        4 // Surplus vehicle limit
    );

    float safePassageTime = 30; // How long the junction is green for.
    float transitionTime = 2; // How long all lights red between transitions
    float pedWaitLimit = 5; // How long a pedestrian waits before changing.

    Timer safePassageTimer;
    Timer transitionTimer;
    Timer pedTimer;

    // Interrupt Setup
    pedSwitch.rise(&StartPedTimer);

    // Initial Start
    pedRed = 1;
    junctionTwo.changeRed();
    junctionOne.changeGreen();

    while(1){
        // If a vehicle is waiting (or has been seen waiting)
        if(junctionOne.isVehicleWaiting() || junctionOne.triggered || junctionTwo.remoteTrigger){
      
            // First check the junction isn't already green
            if(!junctionOne.isGreen()){
                // Set the light change in motion, regardless if there is no longer a vehicle at the junction.
                // This stops the lights from staying red.
                junctionOne.triggered = true;

                //Only log a waiting vehicle is there is still a vehicle and the junction is triggered.
                if(junctionOne.isVehicleWaiting() && junctionOne.triggered){
                    bth.printf("Junction 1: Vehicle Waiting\n");
                }    
                
                // Continue if safety timer elapsed, or vehicle limit reached, or remotely operated.
                if(safePassageTimer.read() > safePassageTime || (junctionTwo.vehicleCounterStarted && junctionTwo.surplusVehicleCount >= junctionTwo.surplusVehicleLimit) ||
                junctionOne.remoteTrigger){

                    // If it has, change junction two to red
                    junctionTwo.changeRed();

                    // Before changing green, make sure suitable time passed.
                    if(transitionTimer.read()>transitionTime){

                        // If so, change green
                        junctionOne.changeGreen();
                        // Reset Trigger
                        junctionOne.triggered = false;

                        //Reset Timers and counters and reset remote trigger
                        transitionTimer.stop();
                        transitionTimer.reset();                    
                        safePassageTimer.stop();
                        safePassageTimer.reset();
                        junctionTwo.stopVehicleCounter();
                        junctionOne.remoteTrigger = false;

                    } else if(transitionTimer.read()==0) {
                        // If not, and the timer has not started, start it
                        transitionTimer.start();
                    }
                } else if (safePassageTimer.read()==0){
                    // If not, and the timer has not started, start it
                    safePassageTimer.start();
                }
            }

            //Only when the Junction Two is green and vehicle waiting at Junction One do we set Junction Two to count cars 
            if(junctionTwo.isGreen()){
                // If the count hasn't started and there is no vehicle waiting (to account for the vehicle waiting)
                if(!junctionTwo.vehicleCounterStarted && !junctionTwo.isVehicleWaiting()){
                    junctionTwo.startVehicleCounter();
                }
                
            }
        }

        // If a vehicle is waiting (or has been seen waiting) or is remotely triggered.
        if(junctionTwo.isVehicleWaiting() || junctionTwo.triggered || junctionTwo.remoteTrigger){
            // First check the junction isn't already green
            if(!junctionTwo.isGreen()){
                // Set the light change in motion, regardless if there is no longer a vehicle at the junction.
                // This stops the lights from staying red.
                junctionTwo.triggered = true;

                //Only log a waiting vehicle is there is still a vehicle and the junction is triggered.
                if(junctionTwo.isVehicleWaiting() && junctionTwo.triggered){
                    bth.printf("Junction 2: Vehicle Waiting\n");
                } 

                // Continue if safety timer elapsed, or vehicle limit reached, or remotely operated.
                if(safePassageTimer.read() > safePassageTime || (junctionOne.vehicleCounterStarted && junctionOne.surplusVehicleCount >= junctionOne.surplusVehicleLimit) || junctionTwo.remoteTrigger){

                    // If it has, change junction one to red
                    junctionOne.changeRed();

                    // Before chaning green, make sure suitable time passed.
                    if(transitionTimer.read()>transitionTime){

                        // If so, change green
                        junctionTwo.changeGreen();
                        // Reset Trigger
                        junctionTwo.triggered = false;

                        //Start timer to transition back, this makes it so Junction One is the primarily green.
                        junctionOne.triggered = true;

                        //Reset Timers and counters and reset remote trigger
                        transitionTimer.stop();
                        transitionTimer.reset();                    
                        safePassageTimer.stop();
                        safePassageTimer.reset();
                        junctionOne.stopVehicleCounter();
                        junctionTwo.remoteTrigger = false;

                    } else if(transitionTimer.read()==0) {
                        // If not, and the timer has not started, start it
                        transitionTimer.start();
                    }
                } else if (safePassageTimer.read()==0){
                    // If not, and the timer has not started, start it
                    safePassageTimer.start();
                }
            }

            //Only when the Junction One is green and vehicle waiting at Junction Two do we set Junction One to count cars 
            if(junctionOne.isGreen()){
                // If the count hasn't started and there is no vehicle waiting (to account for the vehicle waiting)
                if(!junctionOne.vehicleCounterStarted && !junctionOne.isVehicleWaiting()){
                    junctionOne.startVehicleCounter();
                }
                
            }
        }

        //Check Pedestrian Waiting Timer (or remote trigger)
        if(ped.waitingTimer.read() > pedWaitLimit || ped.remoteTrigger){
            junctionOne.changeRed();
            junctionTwo.changeRed();

            // Before chaning green, make sure suitable time passed.
            transitionTimer.start();
            if(transitionTimer > transitionTime){
                ped.changeGreen();
                //Reset Timers and Triggers
                transitionTimer.stop();
                transitionTimer.reset();
                ped.remoteTrigger = false;
                wait(2);
                junctionOne.changeGreen();
            }
        }

        // Remote Control
        if(bth.readable()){
            char input = bth.getc();
            switch (input){
                case '1': junctionOne.remoteTrigger = true; break;
                case '2': junctionTwo.remoteTrigger = true; break;
                case 'P': ped.remoteTrigger = true; break;
                default: break;
            } 
        }
    }
    return 0;
  }
