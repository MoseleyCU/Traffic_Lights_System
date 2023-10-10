#include "mbed.h"
#include <cstdio>
#include <string>


//Junction 1 Setup
//Traffic Signals
DigitalOut rLight_J1(p16);
DigitalOut gLight_J1(p15);
//IR Emitter
DigitalOut irTx_J1(p18);
//IR Receiver
AnalogIn irRx_J1(p20);
//Object Detected Indicator
DigitalOut ind_J1(p12);

//Junction 2 Setup
//Traffic Signals
DigitalOut rLight_J2(p14);
DigitalOut gLight_J2(p13);
//IR Emitter
DigitalOut irTx_J2(p17);
//IR Receiver
AnalogIn irRx_J2(p19);
//Object Detector Indicator
DigitalOut ind_J2(p11);


// Class for the Presence Sensor
class TL_Sensor{
    private:
    // Define the IO, private as they should not be accessible outside of class
    DigitalOut irEmitter;
    AnalogIn irReceiver;
    DigitalOut indicator;

    // To account for Ambient IR light 
    float measureAmbient(){
            float ambientReading;
            //Turn off emitter to read ambient
            irEmitter = 0;
            //Wait for emitter to turn off completely
            wait(0.1);
            ambientReading = irReceiver.read();
            return ambientReading;
        }

    float takeReadingFloat(){
        float ambient = measureAmbient();
        //Turn on emitter
        irEmitter = 1;
        //wait for emitter to power reach max brightness
        wait(0.1);
        //Take the current IR reading, discounting the ambient IR light.
        float current = irReceiver.read() - ambient; 

        return current;
    }

    public:
    // Presense Sensor Sensivitity, can be changed on the fly, for example with the use of a potentiometer.
    float sensitivity; 

    // Constructor
        TL_Sensor(DigitalOut _irEmitter, AnalogIn _irReceiver, DigitalOut _indicator, float _sensitivity)
    : irEmitter(_irEmitter)
    , irReceiver(_irReceiver)
    , indicator(_indicator)
    , sensitivity(_sensitivity)
    {
        //Initially set all LEDs to LO
        irEmitter = 0;
        indicator = 0;     
    } 

    // Method for reporting back if a Vehicle is detected.
    // Does not implement any timing logic here. 
    bool checkForVehicle(){
        
        if(takeReadingFloat() > sensitivity){
            indicator = 1;
            return true;
        } else {
            indicator = 0;
            return false;
        }
    }
    


};

// Class for the Signal Lamps
class TL_Signal{
    private:
        // Define the IO, private as they should not be accessible outside of class
        DigitalOut Red_Light;
        DigitalOut Green_Light;

    public:
        bool isGreen; // Flag to indicate if the Junction is on Green

        // Constructor
        TL_Signal(DigitalOut red, DigitalOut green)
        : Red_Light(red)
        , Green_Light(green)
        {
            //Set Junction to Red initially
            Red_Light = 1;
            Green_Light = 0;
            isGreen = false;
        }

        void turnGreen(){
            Red_Light = 0;
            Green_Light = 1;
            isGreen = true;
        }
        void turnRed(){
            Green_Light = 0;
            Red_Light = 1;
            isGreen = false;
        }
};

//This struct contains all the parameters required to initialise a Junction object.
struct JunctionParams{
    //TODO: Pass the IO using pointers rather than by reference.

    string junctionName; //Name of the Junction, used for monitoring
    float junctionWaitLimit; // The length of time a vehicle needs to wait before triggering the lights

    //TL_Sensor
    DigitalOut irEmitter; //The IR Emitter object
    AnalogIn irReceiver; // The IR Receiver object
    DigitalOut indicatorLamp; // The Indicator lamp, triggers when Sensor is HI
    float sensitivity; // Desired sensitivity of the Sensor

    //TL_Signal
    DigitalOut redLight; // The Red Signal lamp
    DigitalOut greenLight; // The Green Signal lamp
};

class Junction{
    public:
    string junctionName; //Name of the Junction, used for monitoring
    TL_Sensor sensor; //Instance of the Presence sensor object
    TL_Signal signal; //Instance of the Traffic Light Signals object
    float junctionWaitLimit; // The length of time a vehicle needs to wait before triggering the lights
    Timer junctionTimer; // Timer to register how long a vehicle has been waiting

    //Constructor
    Junction(JunctionParams jp)
        : junctionName(jp.junctionName)
        , junctionWaitLimit(jp.junctionWaitLimit)
        , sensor(jp.irEmitter, jp.irReceiver, jp.indicatorLamp, jp.sensitivity)
        , signal(jp.redLight,jp.greenLight){
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


int main() {
    //TODO: Integrate Timers into the main loop, in place of wait()
    //TODO: Use SPI to connect two Mbeds, one for each Junction
    //TODO: Implement BLE for monitoring state of the Intersection
    //TODO: Use pointers for passing IO
    //TODO: Create Update method for indicator

    //Junction setup. Creates two instances of a struct made for sending settings to the Junction class.
    JunctionParams j1p = {
        "Junction 1", //Junction Name
        2, // Junction Wait Limit (seconds)
        irTx_J1, // IR Transmitter
        irRx_J1, // IR Reciever
        ind_J1, // Sensor Indicator Lamp
        0.5f, // Sensor Sensitivity
        rLight_J1, //Traffic Signal Red Light
        gLight_J1 // Traffic Signal Green Light
    };

    JunctionParams j2p = {
        "Junction 2", //Junction Name
        5, // Junction Wait Limit (seconds)
        irTx_J2, // IR Transmitter
        irRx_J2, // IR Reciever
        ind_J2, // Sensor Indicator Lamp
        0.5f, // Sensor Sensitivity
        rLight_J2, //Traffic Signal Red Light
        gLight_J2 // Traffic Signal Green Light
    };

    //Instatiate a Junction object for each Junction in the Intersection
    Junction junctionOne(j1p);
    Junction junctionTwo(j2p);

    while(1){
        //Check Junction Two is Red
        if(!(junctionTwo.isGreen())){
            printf("Junction Two is Red\n");
            // If red, check if vehicle waiting
           if(junctionTwo.isVehicleWaiting()){
               printf("Vehicle waiting at junction two\n");
               //If vehicle waiting, change lights.
               junctionOne.changeRed();
               wait(2); //Safety timer (TODO: Replace with timer)
               junctionTwo.changeGreen();
           }
        //Check Junction One is Red
        } else if(!(junctionOne.isGreen())) {
            printf("Junction One is Red\n");
            // If red, check if vehicle waiting
            if(junctionOne.isVehicleWaiting()){
                printf("Vehicle waiting at junction two\n");
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
