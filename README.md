# Family Feud (Spinoff)

## Developers

* Allen Ayala
* Ruben Quiros
* Tyrell Ramos-Lopez
* Rishab Tandon

## Overview

We recreated the system of the popular game show Family Feud ©. We included sound effects, a buzzer system, a score display system, and a judging system. Everything runs on an ARM mbed LPC1768 in C++.

This project was created by students from the Georgia Institute of Technology for ECE 4180: Embedded Systems Design.

> **Note:** We do not claim any of the sound effects used in this project. The theme music and buzzer sound is owned by the creators of Family Feud ©.

## About This Repository

The main branch was created by manually exporting each file and folder individually from our mbed compiler repo.

The develop branch was created by using the option to export to a zip archive from our mbed repo.

If you would like to export our repo using the mbed compiler, our repo can be found at the following link: https://os.mbed.com/users/rquiros6/code/gatech-ece4180-finalProject/

## Hardware Used

1. ARM mbed LPC1768
2. Adafruit Bluefruit LE UART Friend - Bluetooth Low Energy (BLE)
3. Any smartphone with the [Bluefruit Connect app](https://learn.adafruit.com/bluefruit-le-connect)
4. SparkFun Mono Audio Amp Breakout - TPA2005D1
5. Speaker - PCB Mount
6. Momentary Pushbutton Switches - 12mm Square (2)
7. Blue LEDs (2)
8. SparkFun microSD Transflash Breakout
9. Serial Miniature LCD Module - 1.44" (uLCD-144-G2 GFX)
10. A large breadboard
11. Two small breadboards (one for each pushbutton)
12. Jumper wires
13. 100 Ohm resistor (2)

> **Note:** Most parts listed above can be found at [SparkFun Electronics](https://www.sparkfun.com/).

## How to Set Up

* The block diagram for the hardware schematic can be found below:
<img width="812" alt="Screen Shot 2021-12-14 at 20 03 29" src="https://user-images.githubusercontent.com/94718462/146103771-3e96d75e-c870-46d7-9df8-c0754575167e.png">

* uLCD Wiring:

| mbed | uLCD | uLCD Cable |
| -- | --------- | ----|
| 5V (External) | 5V | 5V |
| GND | GND | GND |
| RX = P27 | TX | RX |
| TX = P28 | RX | TX
| P30 | Reset |

* Class D-Amp and Speaker Wiring:

| mbed | TPA2005D1 | Speaker - PCB Mount |
| --- | --------- | ------------------- |
| GND | PWR-, In- | X |
| 5V (External) | PWR+ | X |
| P18 | In+ | X |
| X | Out+ | Speaker+ |
| X | Out- | Speaker- |

* SD Card Reader Wiring:

| mbed | SD Card Reader |
| ---- | -------------- |
| P8 | CS |
| P5 | DI |
| Vout | Vcc |
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
  * Pushbuttons have internal pull-up resistors.
  * The LEDs have external pull-up resistors of 100Ω.

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
    * The real gameshow has other rules at this step, which can be found [here](https://www.ultraboardgames.com/family-feud/game-rules.php).
7. Bluefruit Connect console asks the judge which team won the round. The judge will type A or B for the team that won the round.
8. Bluefruit Connect console asks the judge how many points should be awarded to the team for that round. The judge enters the amount of points, which does not exceed 3 digits for any round (possible "invalid input" scenario)
9. Repeat steps 2-8 until a team's score goes over 300 points.
    * Note: Most games last 4 rounds. However, if a team does not reach 300 points after the 4th round, then a 5th "sudden death showdown" round must be played to determine the winner.

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

// Authors: Allen Ayala, Ruben Quiros, Tyrell Ramos-Lopez, and Rishab Tandon
// ECE 4180 - Fall 2021 (Section B)

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
// in+  -   p26 (PWM) - p18 INSTEAD
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

// RawSerial  pc(USBTX, USBRX); // mainly for testing
// Note: dev is the Bluetooth module in this project
RawSerial  dev(p9,p10); // should match bluetooth pin - see pinout section


std::string btBuffer = "";      // used to store chars being read (i.e. chars are added 1 at a time)
std::string btInput = "";       // bluetooth console input (stores the input from judge as a string)
std::string btInputPrev = "";   //used to check if btInput changed from previous value
std::string msgPrompt = "";     //to be printed on BT console for jusdge to read

// stores last character added to btBuffer
char lastChar;

// stores the name of the winning team - a/A or b/B
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
DigitalIn buzzerA(p19);
DigitalIn buzzerB(p22);

// LEDs
DigitalOut ledA(p20); //20
DigitalOut ledB(p21); //21

// uLCD
uLCD_4DGL LCD(p28, p27, p30); // see pinout section

// mutex for getc, putc, printf, and accessing strings
Mutex mut;

// SD Card reader
SDFileSystem sd(p5, p6, p7, p8, "sd"); // see pinout section

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
}

// showdown sound effect
void showdownSound(){
    FILE *wave_file;
    wave_file=fopen("/sd/family-feud-showdown.wav", "r");
    waver.play(wave_file);
    fclose(wave_file);
}

// intro music sound effect
void introMusic(){
    FILE *wave_file;
    wave_file=fopen("/sd/family-feud-intro.wav", "r");
    waver.play(wave_file);
    fclose(wave_file);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Threads
//////////////////////////////////////////////////////////////////////////////////////////////////

// buzzer A thread
void threadBuzzerA(void const *args){
    while(true){
        Thread::wait(100);
        if((buzzerA == 0) && !buzzerHit){
            buzzerHit = true;
            ledA = 1;
            buzzerSound();
        }
    }
}

// buzzer B thread
void threadBuzzerB(void const *args){
    while(true){
        Thread::wait(100);
        if((buzzerB == 0) && !buzzerHit){
            buzzerHit = true;
            ledB = 1;
            buzzerSound();
        } 
    }
}

// LCD thread - displays round number and team scores
void threadLCD(void const *args){
    while(true){
        mut.lock();
        LCD.locate(0,0);
        LCD.printf("Welcome to Family Feud!");
        LCD.printf("\n\nRound %d", roundNum);
        LCD.printf("\n\nScores");
        LCD.printf("\nTeam A: %d  ", teamScoreA);
        LCD.printf("\nTeam B: %d  ", teamScoreB);
        mut.unlock();
        Thread::wait(500);
    }
}

// thread for handling data from the bluetooth app console
void threadBTReceive(void const *args){
    while(true){
        // lock to read/print strings
        mut.lock();

        // print msg to bluetooth console
        if(printToConsole){ // sends msgPrompt var to bluetooth console
            for(int i = 0; i < msgPrompt.length(); ++i){
                dev.putc(msgPrompt[i]);
            }
            Thread::wait(100);
            printToConsole = false;
        }

        // team has not been chosen yet
        if(startRound && !teamChosen && btInput.compare("") != 0){
            // check if correct team name has been sent
            if(!((btInput.compare("A") == 0) || (btInput.compare("B") == 0)
            || (btInput.compare("a") == 0) || (btInput.compare("b") == 0))){
                msgPrompt = "Invalid team name!\n";
                printToConsole = true;
            }
            else{
                winningTeam = btInput;
                btInputPrev = btInput; // prevent following if statement from executing early
                teamChosen = true;
            }
        }
        
        // team has won the round, but judge hasn't given points
        if(teamChosen && !pointsAwarded && (btInput.compare(btInputPrev) != 0)){
            // attempt to convert to integer
            unsigned int score = std::atoi(btInput.c_str());
            if(score){
                if(score > 999){
                    // incorrect input
                    msgPrompt = "Number is too big!\n";
                    printToConsole = true;
                    btInputPrev = btInput;
                }
                else{
                    // correct input
                    pointsAwarded = true;

                    // check which team won
                    if((winningTeam.compare("A") == 0) || (winningTeam.compare("a") == 0)){
                        teamScoreA += score;
                    }
                    else{
                        teamScoreB += score;
                    }
                }
            }
            else{
                //prevent duplicate error messaging
                if(msgPrompt.compare("Invalid input!\nPlease type an integer less than 1000:\n") != 0){
                    msgPrompt = "Invalid input!\nPlease type an integer less than 1000:\n";
                    printToConsole = true;
                    btInputPrev = btInput;
                }
            }
        }
        // unlock mutex
        mut.unlock();
    }
}

// thread for sending data to the bluetooth app console
void threadBTSend(void const *args){
    while(true){
        mut.lock();
        while(dev.readable()){
            // pc.putc(dev.getc()); // for testing purposes
            lastChar = dev.getc();
            btBuffer.push_back(lastChar);
        }

        if (lastChar == '\n'){
            btBuffer = btBuffer.substr(0, btBuffer.length() - 1);
            // pc.printf(btBuffer.c_str()); // for testing purposes
            btInput = btBuffer;
            btBuffer = "";
        }
        mut.unlock();
        // Thread::wait(100);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// main - most of the logic here is controlled by the judge
//////////////////////////////////////////////////////////////////////////////////////////////////
int main(){
    
    // initialize baud rates
    dev.baud(9600);
    // pc.baud(9600); // for testing purposes
    LCD.baudrate(9600);
        
    // set pull up mode on pushbuttons
    buzzerA.mode(PullUp);
    buzzerB.mode(PullUp);
    wait(0.1);
        
    // playIntroMusic
    introMusic();
    
    // clear and setup uLCD
    LCD.cls();
    wait(2);

    // start LCD thread
    Thread t1(threadLCD);
    Thread t2(threadBTReceive);  
    Thread t3(threadBTSend);  
    Thread t4(threadBuzzerA);
    Thread t5(threadBuzzerB);

    // start game - game ends when a team gets more than 300 points
    while((teamScoreA < 300) && ((teamScoreB < 300))){
        
        // reset variables for the new round
        btInput = "";
        startRound = false;
        buzzerHit = false;
        playBuzzerSound = false;
        teamChosen = false;
        pointsAwarded = false;
        ledA = 0;
        ledB = 0;

        // bluetooth console prompts judge to start the round
        mut.lock();
        msgPrompt = "Type \"start\" to begin the round...\n";
        printToConsole = true;
        mut.unlock();
        
        // wait for judge to start round        
        do{
        } while (btInput.compare("start") != 0);
        startRound = true;

        mut.lock();
        msgPrompt = "Round has been started...\n";
        printToConsole = true;
        mut.unlock();

        // increment round num
        ++roundNum;

        // play showdown music
        showdownSound();
        mut.lock();
        msgPrompt = "Played showdown music...\nWaiting for buzzer to be pressed\n";
        printToConsole = true;
        mut.unlock();

        // wait for buzzer to be pressed
        do{
        } while (!buzzerHit);

        // bluetooth console prompts judge to say which team won the round
        mut.lock();
        msgPrompt = "Insert the Team that won the round: \n";
        printToConsole = true;
        mut.unlock();

        // wait for judge to say which team won the round
        do{
        } while(!teamChosen);

        // bluetooth console prompts judge to input number of points
        mut.lock();
        msgPrompt = "Insert number of points won: \n";
        printToConsole = true;
        mut.unlock();

        // wait for judge to award points to the winning team
        do{
        } while(!pointsAwarded);
    
    }
    
    // End of Game message to judge's screen
    mut.lock();
    msgPrompt = "Winner: Team " + winningTeam + "\n";
    printToConsole = true;
    mut.unlock();
    Thread::wait(3000);
    
    // terminate threads
    t1.terminate();
    t2.terminate();
    t3.terminate();
    t4.terminate();
    t5.terminate();

    // End of Game message to LCD screen
    LCD.cls();
    LCD.locate(0,0);
    LCD.printf(msgPrompt.c_str());
    LCD.printf("\n\nThank you\nfor playing!");
}

```

## YouTube Demo Video

<a href="http://www.youtube.com/watch?feature=player_embedded&v=p3WgddTrSso
" target="_blank"><img src="http://img.youtube.com/vi/p3WgddTrSso/0.jpg"
alt="Title" width="240" height="180" border="10" /></a>
