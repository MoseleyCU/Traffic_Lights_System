#include "mbed.h"
#include <exception>
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



class TL_Sensor{
    private:
    DigitalOut irEmitter;
    AnalogIn irReceiver;
    DigitalOut indicator;

    public:
    float sensitivity;

        TL_Sensor(DigitalOut _irEmitter, AnalogIn _irReceiver, DigitalOut _indicator, float _sensitivity)
    : irEmitter(_irEmitter)
    , irReceiver(_irReceiver)
    , indicator(_indicator)
    , sensitivity(_sensitivity)
    {
        irEmitter = 0;
        indicator = 0;
        
    } 

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
        float current = irReceiver.read() - ambient;
        return current;
    }

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

class TL_Signal{
    public:
    TL_Signal(DigitalOut red, DigitalOut green)
    : Red_Light(red)
    , Green_Light(green)
    {
        Red_Light = 1;
        Green_Light = 0;
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

    DigitalOut Red_Light;
    DigitalOut Green_Light;
    bool isGreen;

};

struct JunctionParams{
    string junctionName;
    float junctionWaitLimit;
    Timer junctionTimer;
    //TL_Sensor
    DigitalOut irEmitter;
    AnalogIn irReceiver;
    DigitalOut indicatorLamp;
    float sensitivity;

    //TL_Signal
    DigitalOut redLight;
    DigitalOut greenLight;
};

class Junction{
    public:
    string junctionName;
    TL_Sensor sensor;
    TL_Signal signal;
    float junctionWaitLimit;
    Timer junctionTimer;

    Junction(JunctionParams jp)
        : junctionName(jp.junctionName)
        , junctionWaitLimit(jp.junctionWaitLimit)
        , junctionTimer(jp.junctionTimer)
        , sensor(jp.irEmitter, jp.irReceiver, jp.indicatorLamp, jp.sensitivity)
        , signal(jp.redLight,jp.greenLight){
    }
    bool isGreen(){
        return signal.isGreen;
    }
    bool isVehicleWaiting(){
        if(sensor.checkForVehicle()){
            if(!(junctionTimer.read()==0)){
                //If a vehicle shows up, start a timer.
                junctionTimer.start();
                return false;
            }
            if(junctionTimer.read()>junctionWaitLimit){
                //Once the vehicle is confirmed waiting greater than time limit, reset the timer.
                junctionTimer.stop();
                junctionTimer.reset();
                return true;
            } else {
                return false;
            }
        } else {
            //If there is no vehicle waiting, reset the timer.
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

    //Junction setup
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

    Junction junctionOne(j1p);
    Junction junctionTwo(j2p);

    while(1){
        if(junctionOne.isGreen()){
           if(junctionTwo.isVehicleWaiting()){
               junctionOne.changeRed();
               wait(2);
               junctionTwo.changeGreen();
           }
        } else if(junctionTwo.isGreen()) {
            if(junctionOne.isVehicleWaiting()){
                junctionTwo.changeRed();
                wait(2);
                junctionOne.changeGreen();
            }
        } else {
            junctionOne.changeGreen();
        }
        
    }
    return 0;
  }
