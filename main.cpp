#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"
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
