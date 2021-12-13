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


// Adafruit BLE
Serial bluemod(p9,p10); // see pinout section

RawSerial  pc(USBTX, USBRX); //mainly for testing
RawSerial  dev(p9,p10); //should match bluetooth pin

//bluetooth console buffer (used to store chars being read) (i.e. chars are added 1 at a time)
std::string btBuffer = ""; 

//bluetooth console input (stores the entire new line)
//NOTE: Changes value after each new line
std::string btInput = ""; 
std::string msgPrompt = "";

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
        LCD.printf("\n\nRound %u", roundNum);
        LCD.printf("\n\nScores");
        LCD.printf("\nTeam A: %u  ", teamScoreA);
        LCD.printf("\nTeam B: %u  ", teamScoreB);
        mut.unlock();
        Thread::wait(500);
    }
}

// threat for the bluetooth app console
void threadBTConsole(void const *args){
    while(true){
        
        //print msg to console
  
        if(printToConsole){ //sends msgPrompt var to bluetooth console
            mut.lock();
            for(int i = 0; i < msgPrompt.length(); i++){
                dev.putc(msgPrompt[i]);
            }
            mut.unlock();
            Thread::wait(100);
            printToConsole = false;
        }

        // team has not been chosen yet
        if(!teamChosen && btInput.compare("") != 0){
            // check if correct team name has been sent
            if(!((btInput.compare("A") == 0) || (btInput.compare("B") == 0)
            || (btInput.compare("a") == 0) || (btInput.compare("b") == 0))){
                msgPrompt = "Invalid team name!";
                printToConsole = true;
            }
            else{
                winningTeam = btInput;
                teamChosen = true;
            }
        }
        
        // team has won the round, but judge hasn't given points
        if(teamChosen && !pointsAwarded){
            unsigned int score = std::atoi(btInput.c_str());
            if(score){
                if(score > 999){
                    // incorrect input
                    msgPrompt = "Number is too big";
                    printToConsole = true;
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
    LCD.baudrate(9600);
    
    dev.attach(&dev_recv, Serial::RxIrq); //attach interrupt method to bt message
    
    // buzzerA.mode(PullUp);
    // buzzerB.mode(PullUp);

    buzzerA.mode(PullDown);
    buzzerB.mode(PullDown);
    
    // setup Buzzer interrupts
    buzzerA.attach_deasserted(&pb_hit_TeamA);
    buzzerB.attach_deasserted(&pb_hit_TeamB);
    
    // playIntroMusic
    //introMusic();
    
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
        printToConsole = true;
        
        // wait for judge to start round
        
        // while(true){
        //     msgPrompt = btInput;
        //     printToConsole = true;
        //     wait(1);
        // }

        do{
            wait(1);
            teamScoreA++;
            teamScoreB+= 2;
        } while(true);
        // } while (btInput.compare("") == 0);
        
        
        // do{
            // Thread::yield();
        // } while (btInput.compare("") == 0);

        msgPrompt = "Passed initial yield..\n";
        printToConsole = true;

        // increment round num
        ++roundNum;

        // play showdown music
        showdownSound();
        msgPrompt = "Played showdown MUS..\n";
        printToConsole = true;

        // wait for buzzer to be pressed
        do{
            Thread::yield();
        } while (!buzzerHit);

        // bluetooth console prompts judge to say which team won the round
        // which team won the round?

        msgPrompt = "Insert the Team that won the round:\n";
        printToConsole = true;

        // wait for judge to say which team won the round
        do{
            Thread::yield();
        } while(!teamChosen);
        // } while ((btInput.compare("A") == 0) || (btInput.compare("B") == 0));    // flag = !teamChosen    

        // bluetooth console prompts judge to input number of points
        msgPrompt = "How many points won?\n";
        printToConsole = true;

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
    LCD.printf("Winner: Team ");
    LCD.printf(winningTeam.c_str());
    LCD.printf("\n\nThank you");
    LCD.printf("\nfor playing!");
}
