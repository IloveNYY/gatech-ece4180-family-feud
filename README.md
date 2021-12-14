# Family Feud (Spinoff)

## Developers

* Allen Ayala
* Ruben Quiros
* Tyrell Ramos-Lopez
* Rishab Tandon

## Overview

We recreated the system of the popular game show Family Feud ©. We included sound effects, a buzzer system, a score display system, and a judging system. Everything runs on an ARM mbed in C++.

This project was created by students from the Georgia Institute of Technology for ECE 4180: Embedded Systems Design.

* Note: We do not claim any of the sound effects used in this project. The theme music and buzzer sound is owned by the creators of Family Feud ©.

## Hardware Used

1. ARM mbed
2. Adafruit Bluefruit LE UART Friend - Bluetooth Low Energy (BLE)
3. Any smartphone with the Bluefruit Connect app
4. SparkFun Mono Audio Amp Breakout - TPA2005D1
5. Speaker - PCB Mount
6. Momentary Pushbutton Switch - 12mm Square (2)
7. LED - Basic Red 5mm (2)
8. SparkFun microSD Transflash Breakout
9. Serial Miniature LCD Module - 1.44" (uLCD-144-G2 GFX)
10. A large breadboard
11. Two small breadboards
12. Jumper wires

## How to Set Up

* The block diagram for the hardware schematic can be found below:
<img width="827" alt="Schematic" src="https://user-images.githubusercontent.com/94718462/145887019-2519a564-6d19-41d9-b08a-9a1a48d29927.png">

* uLCD Wiring:

| mbed | uLCD |
| -- | --------- |
| 5V (External) | 5V |
| GND| GND |
| P27 | TX |
| P28 | RX |
| P30 | Reset |

* Class D-Amp and Speaker Wiring:

| mbed | TPA2005D1 | Speaker - PCB Mount |
| --- | --------- | ------------------- |
| GND | PWR-, In- | X |
| 5V (External) | PWR+ | X |
| P26 | IN+ | X |
| X | Out+ | Speaker+ |
| X | Out- | Speaker- |

* SD Card Reader Wiring:

| mbed | SD Card Reader |
| ---- | -------------- |
| P8 | CS |
| P5 | DI |
| VOUT | VCC | 
| P7 | SCK | 
| GND | GND | 
| P6 | DO |
| NC | CD |

* Adafruit Bluetooth Wiring:

| mbed | Adafruit Bluetooth | 
| ---- | ------------------ |
| 5V (External) | Vin |
| GND | GND, CTS |
| P10 | TXO |
| P9 | RXI |

* Pushbutton and LED Wiring:

| Component | mbed |
| --------- | ---- |
| Buzzer A | P19 |
| LED A | P20 |
| Buzzer B | P22 |
| LED B | P21 |

## Game Logic

1. Game begins, and intro music plays to completion.
2. Bluefruit Connect console tells the judge to begin the round by sending any message.
3. When the judge begins the round, the showdown music plays.
4. When the showdown music is over, the players can prepare to press their buzzer/pushbutton (while the judge asks the question).
5. Whichever buzzer/pushbutton pressed first lights up their corresponding LED (e.g. 2 buzzers and 2 LEDs).
6. The team that wins the showdown controls the round, etc...
    * The real gameshow has other rules at this step, chich can be found [here](https://www.ultraboardgames.com/family-feud/game-rules.php).
7. Bluefruit Connect console asks the judge which team won the round. The judge will type A or B for the team that won the round.
8. Bluefruit Connect console asks the judge how many points should be awarded to the team for that round. The judge enters the amount of points, which does not exceed 3 digits for any round (possible "invalid input" scenario)
9. Repeat steps 2-8 until a team's score goes over 300 points.
    * Note: Most games last four rounds. However, if a team does not reach 300 points after the 4th round, then a 5th "sudden death showdown" round must be played to determine the winner.

## Code

main.cpp

```cpp
#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"
#include "PinDetect.h"
#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pin layout section
//////////////////////////////////////////////////////////////////////////////////////////////////

// uLCD
// +5V  -   external 5V
// TX   -   RX cable - p27
// RX   -   TX cable - p28
// GND  -   GND
// RES  -   p30

// TPA2005D1 (Class D Amp)
// in-  -   gnd 
// in+  -   p26 (PWM)
// pwr- -   gnd
// pwr+ -   3.3V or 5V (recommend external 5V)
// out- -   speaker-
// out+ -   speaker+

// SD card reader
// CS  o-------------o p8    (DigitalOut cs)
// DI  o-------------o p5    (SPI mosi)
// VCC o-------------o VOUT  (3.3V)
// SCK o-------------o p7    (SPI sclk)
// GND o-------------o GND  
// DO  o-------------o p6    (SPI miso)3
// CD   -   NOT USED

// Adafruit BLE
// gnd              -   gnd 
// Vin (3.3-16V)    -   VU 5V (external 5V recommended)
// RTS              -   NOT USED
// CTS              -   gnd
// TXO              -   p10 (RX)
// RXI              -   p9  (TX)

//////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////////////////////////////

//For BlueTooth Console
RawSerial  pc(USBTX, USBRX); //mainly for testing
RawSerial  dev(p28,p27);

//bluetooth console buffer (used to store chars being read) (i.e. chars are added 1 at a time)
std::string btBuffer = ""; 

//bluetooth console input (stores the entire new line)
//NOTE: Changes value after each new line
std::string btInput = ""; 
std::string msgPrompt;

char lastChar;

std::string winningTeam = "";

// flags
volatile bool startGame = false;
volatile bool startRound = false;
volatile bool buzzerHit = false;
volatile bool playBuzzerSound = false;
volatile bool teamChosen = false;
volatile bool pointsAwarded = false;
volatile bool printToConsole = false;

// team scores
volatile unsigned int teamScoreA = 0;
volatile unsigned int teamScoreB = 0;

// round #
volatile unsigned int roundNum = 0;

// PushButtons (Buzzers)
PinDetect buzzerA(p19);
PinDetect buzzerB(p22);

// LEDs
DigitalOut ledA(p20);
DigitalOut ledB(p21);

// uLCD
uLCD_4DGL LCD(p28, p27, p30); // see pinout section

// mutex for getc and putc commands
Mutex mut;

// Adafruit BLE
Serial bluemod(p9,p10); // see pinout section

// SD Card reader
SDFileSystem sd(p5, p6, p7, p8, "sd"); // see pinout section

// speaker output pin
PwmOut speaker(p26); // see pinout section

// DACout pin (not used, but needed for wave_player)
AnalogOut DACout(p18);

//wave player plays a *.wav file to D/A and a PWM
wave_player waver(&DACout);

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////////////////////////////

// buzzer sound effect
void buzzerSound(){
    FILE *wave_file;
    wave_file=fopen("/sd/family-feud-buzzer.wav", "r");
    waver.play(wave_file);
    fclose(wave_file);
    wait(2); // buzzer = 2 second wait
}

// showdown sound effect
void showdownSound(){
    FILE *wave_file;
    wave_file=fopen("/sd/family-feud-showdown.wav", "r");
    waver.play(wave_file);
    fclose(wave_file);
    wait(4.5); // showdown = 4.5 second wait
}

// intro music sound effect
void introMusic(){
    FILE *wave_file;
    wave_file=fopen("/sd/family-feud-intro.wav", "r");
    waver.play(wave_file);
    fclose(wave_file);
    wait(28); // intro music = 28 second wait
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupts
//////////////////////////////////////////////////////////////////////////////////////////////////

// when team A hits buzzer first
void pb_hit_TeamA(void){
    if(!buzzerHit){
        buzzerHit = true;
        ledA = 1;
        buzzerSound();
    }
}

// when team B hits buzzer first
void pb_hit_TeamB(void){
    if(!buzzerHit){
        buzzerHit = true;
        ledB = 1;
        buzzerSound();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Threads
//////////////////////////////////////////////////////////////////////////////////////////////////

// LCD thread - displays round number and team scores
void threadLCD(void const *args){
    while(true){
        mut.lock();
        LCD.locate(0,0);
        LCD.printf("Welcome to Family Feud!");
        LCD.printf("\n\nRound %d", roundNum);
        LCD.printf("\n\nTeam A Score: %d  ", teamScoreA);
        LCD.printf("\n\nTeam B Score: %d  ", teamScoreB);
        mut.unlock();
        Thread::wait(100);
    }
}

// threat for the bluetooth app console
void threadBTConsole(void const *args){
    while(true){
        
        //print msg to console
  
        if(printToConsole){ //sends msgPrompt var to bluetooth console
            mut.lock();
            for (int i = 0; i < msgPrompt.length(); i++){
                dev.putc(msgPrompt[i]);
            }
            mut.unlock();
            printToConsole = false;
        }

        // team has not been chosen yet
        if(!teamChosen){
            // check if correct team name has been sent
            if(!((btInput.compare("A") == 0) || (btInput.compare("B") == 0)
            || (btInput.compare("a") == 0) || (btInput.compare("b") == 0))){
                msgPrompt = "Invalid team name!";
                printToConsole = true;
                teamChosen = true;
            }
            else{
                winningTeam = btInput;
            }
        }
        
        // team has won the round, but judge hasn't given points
        if(teamChosen && !pointsAwarded){
            unsigned int score = std::atoi(btInput.c_str());
            if(score){
                if(score > 999){
                    msgPrompt = "Number is too big";
                    printToConsole = true;
                    pointsAwarded = true;
                }
                else{
                    if((winningTeam.compare("A") == 0) || (winningTeam.compare("a") == 0)){
                        teamScoreA += score;
                    }
                    else{
                        teamScoreB += score;
                    }
                }
            }
            else{
                msgPrompt = "Invalid input!";
                printToConsole = true;
            }

            // try{
            //     // attempt to convert to integer
            //     unsigned int score = std::stoi(btInput, nullptr, 10);
                
            //     if(score > 1000){ //invalid input
            //         msgPrompt = "Number is too big";
            //         printToConsole = true;
            //         pointsAwarded = true;
            //     }
            //     else{
            //         if(winningTeam.compare("A") == 0){
            //             teamScoreA += score;
            //         }
            //         else{
            //             teamScoreB += score;
            //         }
            //     }  
            // }
            // catch(const std::invalid_argument& e){
            //     msgPrompt = "Input is not an integer!"
            //     printToConsole = true;
            //     // std::cerr << e.what() << '\n';
            // }
            // catch(const std::out_of_range& e){
            //     msgPrompt = "Number is too big!"
            //     printToConsole = true;
            //     // std::cerr << e.what() << '\n';
            // }
        }
    }
}

//reads input from Mbed Bluetooth app terminal
void dev_recv() {
    while(dev.readable()){
        //pc.putc(dev.getc());
        lastChar = dev.getc();
        btBuffer += lastChar;
    }
    if(lastChar == '\n'){
        pc.printf(btBuffer.c_str());
        pc.printf("--\n");
        btInput = btBuffer;
        // btInput = btInput.c_str().toupper(); //ensures we only need to parse uppercase strings
        btBuffer= ""; //rest buffer to recieve new Line
    }
//    pc.printf("finished reading\n");
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// main - most of the logic here is controlled by the judge
//////////////////////////////////////////////////////////////////////////////////////////////////
int main(){
    
    dev.baud(9600);
    pc.baud(9600);
    
    dev.attach(&dev_recv, Serial::RxIrq); //attach interrupt method to bt message
    
    // setup Buzzer interrupts
    buzzerA.attach_deasserted(&pb_hit_TeamA);
    buzzerB.attach_deasserted(&pb_hit_TeamB);
    
    // playIntroMusic
    introMusic();
    
    // clear and setup uLCD
    LCD.cls();    
    wait(2);

    // start LCD thread
    Thread t1(threadLCD);
    Thread t2(threadBTConsole);    

    // start game
    while((teamScoreA < 300) && ((teamScoreB < 300))){
        
        //reset flags for the new round
        // startGame = false; // might not need
        // startRound = false;
        buzzerHit = false;
        playBuzzerSound = false;
        teamChosen = false;
        pointsAwarded = false;

        // bluetooth console prompts judge to start the round
        msgPrompt = "Insert anything to start...\n";
        // msgCode = 0;
        
        // wait for judge to start round
        do{
            Thread::yield();
        } while (btInput.compare("") == 0);

        // increment round num
        ++roundNum;

        // play showdown music
        showdownSound();

        // wait for buzzer to be pressed
        do{
            Thread::yield();
        } while (!buzzerHit);

        // bluetooth console prompts judge to say which team won the round
        // which team won the round?

        msgPrompt = "Insert the Team that won the round:\n";

        // wait for judge to say which team won the round
        do{
            Thread::yield();
        } while(!teamChosen);
        // } while ((btInput.compare("A") == 0) || (btInput.compare("B") == 0));    // flag = !teamChosen    

        // std::string winningTeam = btInput;

        // bluetooth console prompts judge to input number of points
        msgPrompt = "How many points won?\n";

        // wait for judge to award points to the winning team
        do{
            Thread::yield();
        } while(!pointsAwarded);
        // } while (std::stoi(btInput, nullptr, 10) < 999); // 
    }
    
    //End of Game

    t1.terminate();
    t2.terminate();

    LCD.cls();
    LCD.locate(0,0);
    LCD.printf("We have a winner:");
    LCD.printf("\n\nCongrats ");
    LCD.printf(winningTeam.c_str());
    LCD.printf("!!!");
}

```

## YouTube Demo Video

<a href="http://www.youtube.com/watch?feature=player_embedded&v=dQw4w9WgXcQ
" target="_blank"><img src="http://img.youtube.com/vi/dQw4w9WgXcQ/0.jpg"
alt="Title" width="240" height="180" border="10" /></a>
