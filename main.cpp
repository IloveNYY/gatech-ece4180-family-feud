#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Pin layout section
/////////////////////////////////////////////////////////////////////////////////////////////////// 

// uLCD //
// +5V  -   external 5V
// TX   -   RX cable - p27
// RX   -   TX cable - p28
// GND  -   GND
// RES  -   p30

// TPA2005D1 (Class D Amp)
// in+  -   p26 (PWM)
// rest are irrelevant...

// SD card reader
// CS  o-------------o 8    (DigitalOut cs)
// DI  o-------------o 5    (SPI mosi)
// VCC o-------------o VOUT (3.3V)
// SCK o-------------o 7    (SPI sclk)
// GND o-------------o GND  
// DO  o-------------o 6    (SPI miso)

// Adafruit BLE
// gnd      -   gnd
// VU(5v)   -   Vin (3.3-16V)
// nc       -   RTS
// Gnd      -   CTS
// p10 (RX) -   TXO
// p9  (TX) -   RXI

///////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
///////////////////////////////////////////////////////////////////////////////////////////////////

// uLCD
uLCD_4DGL LCD(p28, p27, p30);
Mutex lcd_mutex;   // needed because 2 threads share this
// uLCD.circle(...); -- for example, but with mutex

// Adafruit BLE
Serial bluemod(p9,p10);

// RGB LED
PwmOut ledR(p23);
PwmOut ledG(p22);
PwmOut ledB(p21);

// SD Card reader
SDFileSystem sd(p5, p6, p7, p8, "sd");

// speaker output pin
PwmOut speaker(p26);

// DACout pin (not used, but needed for wave_player)
AnalogOut DACout(p18);

//wave player plays a *.wav file to D/A and a PWM
//wave_player waver(&DACout,&speaker);
wave_player waver(&DACout);

// fire intensity for RGB LED
volatile float fireIntensity = 0.0; // volative b/c 2 threads read/write

// RGB numbers from BLE (volatile b/c main and thread 2 read/write)
volatile char bred = 0;
volatile char bgreen = 0;
volatile char bblue = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// helper function - random values for fire
///////////////////////////////////////////////////////////////////////////////////////////////////
// Use C's random number generator rand(), but scaled and
// converted to a float (range 0.0 to 1.0) for PWM output
inline float random_number(){
    return (rand()/(float(RAND_MAX)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Thread 1 - fire sound effect
///////////////////////////////////////////////////////////////////////////////////////////////////
void fireSound(void const *args){
    // declare file
    FILE *wave_file;
    
    while(true){
        // open wav file and play it
        wave_file=fopen("/sd/fire.wav", "r");
        waver.play(wave_file);
        fclose(wave_file);
    }
}
//void fire(void const *args){
//    while(1) {
//        //get a new random number for PWM
//        fireIntensity = random_number();
//        //a bit slower time delay can be used for fire
//        wait(0.04);
//    }
//}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Thread 2 - display RGB intensities on uLCD
///////////////////////////////////////////////////////////////////////////////////////////////////
void displayRGBnums(void const *args){
    while(true) {       // thread loop
        lcd_mutex.lock();
        LCD.locate(0,0);
//        LCD.set_font((unsigned char*) Arial_9);
//        LCD.printf("\n\n\nR: %d G: %d B: %d", bred, bgreen, bblue);
        LCD.printf("\n\n\n\nR: %d  ", bred);
        LCD.printf("\nG: %d  ", bgreen);
        LCD.printf("\nB: %d  ", bblue);
        lcd_mutex.unlock();
        Thread::wait(500); // wait 0.5s
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Thread 3 - display seconds passed on uLCD
///////////////////////////////////////////////////////////////////////////////////////////////////
void displaySeconds(void const *args){
    int ii = 0;
    while(true) {       // thread loop
        lcd_mutex.lock();
        LCD.locate(0,0);
//        LCD.set_font((unsigned char*) Small_6);
        LCD.printf("\nSeconds passed: %d", ii);
        lcd_mutex.unlock();
        ++ii;
        Thread::wait(1000);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// main (thread 0 - outputs light to RGB for fire effect and gets color from BLE phone app) 
///////////////////////////////////////////////////////////////////////////////////////////////////
int main(){
    // clear uLCD
    LCD.cls();
    
    wait(2);
    
    ledR = ledG = ledB = 0.0;
    
    // start threads
    Thread t1(fireSound);
    Thread t2(displayRGBnums);
    Thread t3(displaySeconds);
    
    //char bred = 0;
//    char bgreen = 0;
//    char bblue = 0;
    while(1){
        lcd_mutex.lock();
        if(bluemod.readable() && bluemod.getc()=='!'){
            if(bluemod.getc()=='C'){ //color data packet
                bred = bluemod.getc(); // RGB color values
                bgreen = bluemod.getc();
                bblue = bluemod.getc();
//                if(bluemod.getc()==char(~('!' + 'C' + bred + bgreen + bblue))){ //checksum OK?
//                    fireIntensity = random_number();
                    //ledR = (bred/255.0) * fireIntensity; //send new color to RGB LED PWM outputs
//                    ledG = (bgreen/255.0) * fireIntensity;
//                    ledB = (bblue/255.0) * fireIntensity;
//                    wait(0.04);
//                }
            }
//            fireIntensity = random_number();
//            ledR = (bred/255.0) * fireIntensity; //send new color to RGB LED PWM outputs
//            ledG = (bgreen/255.0) * fireIntensity;
//            ledB = (bblue/255.0) * fireIntensity;
//            wait(0.04);
        }
        lcd_mutex.unlock();
        
        fireIntensity = random_number();
        ledR = (bred/255.0) * fireIntensity; //send new color to RGB LED PWM outputs
        ledG = (bgreen/255.0) * fireIntensity;
        ledB = (bblue/255.0) * fireIntensity;
        wait(0.04);
    }
}
