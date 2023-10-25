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

// Class for the Presence Sensor
class TL_Sensor{
    private:
    // Define the IO, private as they should not be accessible outside of class
    DigitalOut* irEmitter; // IR Emitter LED
    AnalogIn* irReceiver; // IR Receiver LED
    DigitalOut* indicator; // Indicator LED for notifying operator is object detected
    const float sensitivity; // Sensor Sensivitity
    float ambientReading; // To account for Ambient IR light 
    
    // Take a reading of IR level, adjusting for the ambient IR light.
    float takeIRReading(){
         return (irReceiver->read() - ambientReading);        
    }
    
    public:
    // Constructor
        TL_Sensor(DigitalOut* _irEmitter, AnalogIn* _irReceiver, DigitalOut* _indicator, float _sensitivity)
    : irEmitter(_irEmitter)
    , irReceiver(_irReceiver)
    , indicator(_indicator)
    , sensitivity(_sensitivity)
    {
        //Initially set all LEDs to off
        *irEmitter = 0;
        *indicator = 0;     
    } 

    // Update ambient light reading. (Only to be run on start up or on command, do not use in main loop)
    float calibrate(){
            *irEmitter = 0; // Turn off emitter, as to not skew the results
            wait(0.2); // Allow for time to completely turn off.
            ambientReading = irReceiver->read(); // Take reading.
            wait(0.2); // Wait before switching emitter back on.
            *irEmitter = 1;
            return ambientReading;
    }

    // Method for reporting back if a Vehicle is detected.
    bool checkForVehicle(){
        if(takeIRReading() > sensitivity){
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

//Class for the entire Junction
class Junction{
    private:
    /*Do not allow the hardware to be directly accessible in main.*/
    TL_Sensor sensor; //Instance of the Presence sensor object
    TL_Signal signal; //Instance of the Traffic Light Signals object

    string junctionName; //Name of the Junction, used for monitoring
    bool previousSensorState; // Temp variable for holding previous state of junction (for counting vehicles)

    public:
     
    int surplusVehicleLimit; // Max vehicle count (while other junction waiting)
    bool changeTriggered; // Flag variable for triggering junction without vehicle waiting.
    bool vehicleCounterStarted; // Flag for enabling/disabling vehicle counting.
    int surplusVehicleCount; // Current vehicle count (while other junction waiting)
    bool remoteTrigger; // Allows for bluetooth override


    //Constructor
    Junction(string _junctionName, DigitalOut* _irEmitter, AnalogIn* _irReceiver, DigitalOut* _indicatorLamp, float _sensitivity, DigitalOut* _redLight, DigitalOut* _greenLight, int _surplusVehicleLimit)
        : junctionName(_junctionName)
        , sensor(_irEmitter, _irReceiver, _indicatorLamp, _sensitivity)
        , signal(_redLight,_greenLight)
        , surplusVehicleLimit(_surplusVehicleLimit){
            // Set initial values
            changeTriggered = false;
            vehicleCounterStarted = false;
            previousSensorState = false;
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
            previousSensorState = true;
            return true;
        } else {
            //Count vehicles
            if(previousSensorState){
                //Vehicle gone
                surplusVehicleCount++;
                if(isGreen()){
                   if(vehicleCounterStarted){
                    bth.printf("%s: %i vehicles left to pass.\n",junctionName,(surplusVehicleLimit - surplusVehicleCount));
                    } 
                }
                previousSensorState = false;
            }
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
    
    // Vehicle counter used when a vehicle is waiting at another junction
    void startVehicleCounter(){
            vehicleCounterStarted = true;
            surplusVehicleCount = 0;
    }
    void stopVehicleCounter(){
        vehicleCounterStarted = false;
    }


    void CalibrateSensor(){
        bth.printf("%s: Calibrating sensor...\n", junctionName);
        bth.printf("%s: Sensor Calibration - %0.2f\n",junctionName,sensor.calibrate());
        bth.printf("%s: Sensor Calibrated.\n", junctionName);
    }
};

// Character Mapping for 7 segment display
struct CharacterHexCodes {
    int ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, A, b, C, c, d, E, F, g, G, H, h, i, I, j, L, l, n, N, O, o, p, q, r, S, t, U, u, y, CLEAR;

    // Constructor to initialize the mappings
    CharacterHexCodes() {
        ZERO = 0x88; ONE = 0xBB; TWO = 0x94; THREE = 0x91; FOUR = 0xA3; FIVE = 0xC1; SIX = 0xC0; SEVEN = 0x9B; EIGHT = 0x80; NINE = 0x81; A = 0x82; b = 0xE0; C = 0xCC; 
        c = 0xF4; d = 0xB0; E = 0xC4; F = 0xC6; g = 0x81; G = 0xC8; H = 0xA2; h = 0xE2; i = 0xDB; I = 0xBB; j = 0xD9; L = 0xEC; l = 0xEE; n = 0xF2; N = 0x8A; O = 0x88; 
        o = 0xF0; p = 0x86; q = 0x83; r = 0xF6; S = 0xC1; t = 0xE4; U = 0xA8; u = 0xF8; y = 0xA1; CLEAR = 0xFF;
    }
};

class PedestrianCrossing {
    private:
    DigitalOut* Red_Light;
    DigitalOut* Green_Light;
    CharacterHexCodes character;
    bool isGreen;

    // Start a countdown to warn pedestrian of time left to cross.
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
        segment = character.CLEAR; // Clear 7 seg
    }

    public:
    Timer waitingTimer;
    bool remoteTrigger;

    PedestrianCrossing(DigitalOut* _pedRedLight, DigitalOut* _pedGreenLight)
    : Red_Light(_pedRedLight)
    , Green_Light(_pedGreenLight){
        isGreen = false;
        remoteTrigger = false;
    }

    // Have pedestrians wait a certain amount of time before changing the lights
    void StartWaitingTimer(){
        bth.printf("Pedestrian Waiting...\n");
        waitingTimer.start();
    }

    void changeGreen(){
        *Red_Light = 0;
        *Green_Light = 1;
        isGreen = true;
        bth.printf("Pedestrian Crossing is Green\n");
        startCountdown();
        changeRed();
    }
    void changeRed(){
        *Green_Light = 0;
        *Red_Light = 1;
        isGreen = false;
        waitingTimer.stop();
        waitingTimer.reset();
        bth.printf("Pedestrian Crossing is Red\n");
    }
};

// Declared globally to work with interrupt
PedestrianCrossing ped( 
    &pedRed,
    &pedGreen
);

//Interrupt function for pedestrian Crossing
void StartPedTimer(){ //
    ped.StartWaitingTimer();
}

// Start up function, for cycling through the lights to ensure connectivity
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

int main() {

    // Run start up diagnostics
    startup();

    // Instatiate a Junction object for each Junction in the Intersection
    Junction junctionOne(
        "Junction 1", // Junction Name
        &irTx_J1, // IR Transmitter
        &irRx_J1, // IR Reciever
        &ind_J1, // Sensor Indicator Lamp
        0.5f, // Sensor Sensitivity
        &rLight_J1, // Traffic Signal Red Light
        &gLight_J1, // Traffic Signal Green Light
        5 // Surplus vehicle limit
    );

    Junction junctionTwo(
        "Junction 2", // Junction Name
        &irTx_J2, // IR Transmitter
        &irRx_J2, // IR Reciever
        &ind_J2, // Sensor Indicator Lamp
        0.5f, // Sensor Sensitivity
        &rLight_J2, // Traffic Signal Red Light
        &gLight_J2, // Traffic Signal Green Light
        4 // Surplus vehicle limit
    );

    // Calibrate both IR sensors
    junctionOne.CalibrateSensor();
    junctionTwo.CalibrateSensor();

    Timer safePassageTimer;
    float safePassageTime = 15; // How long the junction is green for.

    Timer transitionTimer;
    float transitionTime = 2; // How long all lights red between transitions

    Timer pedTimer;
    float pedWaitLimit = 5; // How long a pedestrian waits before changing.

    Timer timeoutJunctionTimer;
    float timeoutTime = 10; // How long before lights change back to default

    // Interrupt Setup
    pedSwitch.rise(&StartPedTimer);

    // Initial Start
    pedRed = 1;
    junctionTwo.changeRed();
    junctionOne.changeGreen();

    while(1){
        // If a vehicle is waiting (or has been seen waiting)
        if(junctionOne.isVehicleWaiting() || junctionOne.changeTriggered || junctionOne.remoteTrigger){
            // First check the junction isn't already green
            if(!junctionOne.isGreen()){
                
                // Only log a waiting vehicle is there is still a vehicle and the junction is triggered. (not when remotely triggered and only log once per change) 
                if(junctionOne.isVehicleWaiting() && !junctionOne.changeTriggered){
                    bth.printf("Junction 1: Vehicle Waiting\n");
                }  
                
                // Set the light change in motion, regardless if there is no longer a vehicle at the junction.
                junctionOne.changeTriggered = true;

                // Continue if safety timer elapsed, or vehicle limit reached, or remotely operated.
                if(safePassageTimer.read() > safePassageTime || (junctionTwo.vehicleCounterStarted && junctionTwo.surplusVehicleCount >= junctionTwo.surplusVehicleLimit) ||
                junctionOne.remoteTrigger){

                    // If it has, change junction two to red and reset timers
                    junctionTwo.changeRed();
                    timeoutJunctionTimer.stop();
                    timeoutJunctionTimer.reset();

                    // Before changing green, make sure suitable time passed.
                    if(transitionTimer.read()>transitionTime){

                        // If so, change green
                        junctionOne.changeGreen();
                        // Reset Trigger
                        junctionOne.changeTriggered = false;

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
            if(junctionTwo.isGreen() && junctionOne.isVehicleWaiting()){
                // If the count hasn't started and there is no vehicle waiting (to account for the vehicle waiting)
                if(!junctionTwo.vehicleCounterStarted && !junctionTwo.isVehicleWaiting()){
                    junctionTwo.startVehicleCounter();
                }
                
            }
        }

        // If a vehicle is waiting (or has been seen waiting) or is remotely triggered.
        if(junctionTwo.isVehicleWaiting() || junctionTwo.changeTriggered || junctionTwo.remoteTrigger){
            // First check the junction isn't already green

            if(!junctionTwo.isGreen()){
                
                //Only log a waiting vehicle is there is still a vehicle and the junction was not already triggered
                if(junctionTwo.isVehicleWaiting() && !junctionTwo.changeTriggered){
                    bth.printf("Junction 2: Vehicle Waiting\n");
                } 
                // Set the light change in motion, regardless if there is no longer a vehicle at the junction.
                // This stops the lights from staying red.
                junctionTwo.changeTriggered = true;


                // Continue if safety timer elapsed, or vehicle limit reached, or remotely operated.
                if(safePassageTimer.read() > safePassageTime || (junctionOne.vehicleCounterStarted && junctionOne.surplusVehicleCount >= junctionOne.surplusVehicleLimit) || 
                junctionTwo.remoteTrigger){

                    // If it has, change junction one to red
                    junctionOne.changeRed();

                    // Before chaning green, make sure suitable time passed.
                    if(transitionTimer.read()>transitionTime){

                        // If so, change green
                        junctionTwo.changeGreen();

                        // Reset Trigger
                        junctionTwo.changeTriggered = false;

                        timeoutJunctionTimer.start();
                        //Start timer to transition back, this makes it so Junction One is the primarily green.
                        //junctionOne.changeTriggered = true;

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
            } else{
                //If a vehicle goes through the junction, reset the default timeout (this is overridden when a vehicle is waiting at Junction One)
                timeoutJunctionTimer.stop();
                timeoutJunctionTimer.reset();
                timeoutJunctionTimer.start();
            }

            //Only when the Junction One is green and vehicle waiting at Junction Two do we set Junction One to count cars 
            if(junctionOne.isGreen() && junctionTwo.isVehicleWaiting()){
                // If the count hasn't started and there is no vehicle waiting (to account for the vehicle waiting)
                if(!junctionOne.vehicleCounterStarted && !junctionOne.isVehicleWaiting()){
                    junctionOne.startVehicleCounter();
                }
                
            }
        }

        // If no vehicles have passed through a secondary junction, reset to default.
        if(timeoutJunctionTimer > timeoutTime){
            bth.printf("Lights timed out, reverting to J1\n");
            junctionOne.remoteTrigger = true;
            timeoutJunctionTimer.stop();
            timeoutJunctionTimer.reset();
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

            // Stop all timers if Bluetooth command
            safePassageTimer.stop(); 
            safePassageTimer.reset();
            transitionTimer.stop();
            transitionTimer.reset();
            timeoutJunctionTimer.stop();
            timeoutJunctionTimer.reset();

            switch (input){

                // Change to junction two.
                case '2': junctionTwo.remoteTrigger = true; break;

                // Change to junction one.
                case '1': junctionOne.remoteTrigger = true;  break;

                // Switch to pedestrian crossing
                case 'P': ped.remoteTrigger = true; break;

                // Calibrate the IR sensors on command.
                case 'C': junctionOne.CalibrateSensor(); junctionTwo.CalibrateSensor(); break;

                // Stop all traffic (emergency situaton)
                case 'S': 
                junctionOne.changeRed(); 
                junctionTwo.changeRed();

                // Does not restart lights until give the go-ahead
                bth.printf("All functions stopped, enter 'G' to restart.\n"); 
                while(1){
                    if(bth.getc()=='G'){
                        break;
                    }
                };
                bth.printf("Functions restarted, please select a Junction to start or wait for vehicles.\n");
                break;
                default: break;
            } 
        }
        wait(0.1); // Encountered a bug with noisy data, this small wait between checks fixes. Do not increase.
    }
    return 0;
  }
