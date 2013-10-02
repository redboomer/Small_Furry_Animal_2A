/******************************************************************************
 * Timer Output Compare Demo
 *
 * Description:
 *
 * This demo configures the timer to a rate of 1 MHz, and the Output Compare
 * Channel 1 to toggle PORT T, Bit 1 at rate of 10 Hz. 
 *
 * The toggling of the PORT T, Bit 1 output is done via the Compare Result Output
 * Action bits.  
 * 
 * The Output Compare Channel 1 Interrupt is used to refresh the Timer Compare
 * value at each interrupt
 * 
 * Author:
 *  Jon Szymaniak (08/14/2009)
 *  Tom Bullinger (09/07/2011)	Added terminal framework
 *
 *****************************************************************************/


// system includes
#include <hidef.h>      /* common defines and macros */
#include <stdio.h>      /* Standard I/O Library */

// project includes
#include "types.h"
#include "derivative.h" /* derivative-specific definitions */

// Definitions

// Change this value to change the frequency of the output compare signal.
// The value is in Hz.
#define OC_FREQ_HZ    ((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency
// in Hz (OC_FREQ_HZ). The formula is:
//
// TC1_VAL = ((Bus Clock Frequency / Prescaler value) / 2) / Desired Freq in Hz
//
// Where:
//        Bus Clock Frequency     = 2 MHz
//        Prescaler Value         = 2 (Effectively giving us a 1 MHz timer)
//        2 --> Since we want to toggle the output at half of the period
//        Desired Frequency in Hz = The value you put in OC_FREQ_HZ
//
#define BUS_CLK_FREQ  ((UINT32) 2000000)   
#define PRESCALE      ((UINT16)  2)         
#define TC1_VAL       ((UINT16)  (((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))

// Define some booleans for code readability.
#define TRUE 1
#define FALSE 0

// These are used to hold the user input.
UINT8 servo1UserInput = 0;
UINT8 servo2UserInput = 0;

// These are used to extract the command and
// any parameters attached to those commands.
#define firstThree(x) ((x>>5)<<5)
#define lastFive(y) (y&31)

// These are the basic PWMPER values
// They vary depending on where the positions 
// are marked on the boxes.  1 tick = ~ 10 degrees.
// TODO.  Needs some tuning. 
//POS0_TICKS   0x05
//POS1_TICKS   0X09
//POS2_TICKS   0X0C
//POS3_TICKS   0X0F
//POS4_TICKS   0X14
//POS5_TICKS   0X18
const UINT8 servoPositionTicks[6] = {0x05, 0X09, 0X0C, 0X0F, 0X14, 0X18};  
  
// buffers to hold the recipies for each servo.
UINT8 bufferServoA[100] = {0};  // Commands buffer for ServoA
UINT8 bufferServoB[100] = {0};  // Commands buffer for ServoB

// Possible Task Statuses.
enum TASKSTATUS
{
  ready = 0,
  running,
  error,
  paused,
  donothing,
};

// the order of these commands match the op codes 
// listed in the assignment.
enum COMMANDS
{
  RECIPE_END = 0,
  MOV = 32,
  WAIT = 64,
  TBD1,
  LOOP_START = 128,
  END_LOOP = 160,
  BREAK_LOOP = 96,
  TBD3
};

// Holds the information for each task.
struct TaskControlBlock 
{
   enum TASKSTATUS status;    
   UINT8 * currentCommand;      // points to the current command in a bufferServo
   
   // Loop bookkeeping stuff.
   UINT8 loopFlag;              // True if we're in a loop, otherwise false.
   UINT8 loopCounter;           // Loop will run n+1 times.
   UINT8* firstLoopInstruction; // Pointer to the instructon after the LOOP_START command.
   
   // MOV bookkeeping stuff
   UINT8 currentServoPosition;  // 0-5
   UINT8 expectedServoPosition; // 0-5
   
   // MOV and WAIT bookkeeping stuff
   INT16 timeLeftms;            // timeleft to execute the current
                                // command.
};

// Look Ma TCBS!!!
struct TaskControlBlock servoA;
struct TaskControlBlock servoB;

// Function definitions
UINT8 GetChar(void);
void getUserInput(void);
void initializeServos(void);
void initializeCommands(void);
void processCommand(struct TaskControlBlock* servo, enum COMMANDS command, UINT8 commandContext);
void processUserCommand(void);
void runTasks(void);
void updateTaskStatus(struct TaskControlBlock* servo);

// Flags to show the reciepe end.
UINT8 reciepeEndServoA =0;
UINT8 reciepeEndServoB =0;


//*****************************************************************************
// This unmitigated piece of crap will get user input from the keyboard for each
// servo and assign the values to global variables for processing. 
//
// Parameters: NONE
//
// Return: None
//*****************************************************************************
void getUserInput(void) 
{
   UINT8 buffer [3];
   INT8 bufferIndex = 0;
   UINT8 carriageRet = '\r';
   UINT16 value = 0;
   
   // Read the digits into a buffer until you get a carage return.
   // Fetch and echo the user input
   printf("\n\rCommand for first Servo: ");
   buffer[bufferIndex] = GetChar();
   bufferIndex++;
   printf("\n\rCommand for second Servo: ");
   buffer[bufferIndex] = GetChar();
      
      
   (void)printf("\r\nServoA Command: %c", buffer[0]);
   (void)printf("\r\nServoB Command: %c\r\n", buffer[1]);
   
   servo1UserInput = buffer[0];
   servo2UserInput = buffer[1];
}

//*****************************************************************************
// This unmitigated piece of crap holds the recipies to be run by each servo. 
//
// Parameters: NONE
//
// Return: None
//*****************************************************************************
void initializeCommands(void)
{

    enum COMMANDS myCommand, myCommand2, myCommand3, myCommand4, myCommand5;

    myCommand = MOV;
    myCommand2 = WAIT;
    myCommand3 = LOOP_START;
    myCommand4 = END_LOOP; 
    myCommand5 = BREAK_LOOP;
    
    // Fill in the commands for servo A
    *(servoA.currentCommand) = myCommand+5;  
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+0;  
    servoA.currentCommand++;
    //Simple move command check
    *(servoA.currentCommand) = myCommand+2;  // Test-3
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand;    // Test-3
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+3;  // Test-3
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+3;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+4;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand;
    servoA.currentCommand++;
    //This test case is loop check.
    *(servoA.currentCommand) = myCommand+3;      //Test-2
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand3+0;     //Test-2
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+1;      //Test-2
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+4;      //Test-2
    servoA.currentCommand++;
     *(servoA.currentCommand) = myCommand4;      //Test-2
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand;        //Test-2
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand2 + 20;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+1;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+5;
    servoA.currentCommand++;
    // this test case is for Break command check.
    *(servoA.currentCommand) = myCommand+3;      //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand3+2;     //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+1;      //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand5;      //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+4;      //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+0;      //Test-6
    servoA.currentCommand++;
     *(servoA.currentCommand) = myCommand4;      //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+5;        //Test-6
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+2;
    servoA.currentCommand++;
    *(servoA.currentCommand) = myCommand+3;
    servoA.currentCommand++;
    *(servoA.currentCommand) = RECIPE_END;
  
    
    // set the pointer back to the beginning of the buffer.
    servoA.currentCommand = &bufferServoA;  

    // Fill in the commands for servo B
    *(servoB.currentCommand) = myCommand+5;  
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+0;  
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+4;  
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+0;  
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;  
    servoB.currentCommand++;
    // this is MOV command test.
    *(servoB.currentCommand) = myCommand;     //Test1
    servoB.currentCommand++;                  
    *(servoB.currentCommand) = myCommand+5;   //Test1
    servoB.currentCommand++;                  
    *(servoB.currentCommand) = myCommand;     //Test1
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand;
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand;
    servoB.currentCommand++;
    //WAIT command test.
    *(servoB.currentCommand) = myCommand+2;        //Test4
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+3;        //Test4
    servoB.currentCommand++;
     *(servoB.currentCommand) = myCommand2 + 31;   //Test4
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand2 + 31;    //Test4
    servoB.currentCommand++;                       
    *(servoB.currentCommand) = myCommand2 + 31;    //Test4
    servoB.currentCommand++;                    
    *(servoB.currentCommand) = myCommand+4;        //Test4
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand;
    servoB.currentCommand++;
    //Test for LOOP error.
    *(servoB.currentCommand) = myCommand+3;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand3+2;     //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+1;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+4;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand3+1;     //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+1;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand4;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+0;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand4;      //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand;        //Test-5
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand+5;
    servoB.currentCommand++;
    *(servoB.currentCommand) = myCommand;
    servoB.currentCommand++;
    *(servoB.currentCommand) = RECIPE_END;

    // set the pointer back to the beginning of the buffer.
    servoB.currentCommand = &bufferServoB; 
}


// Initializes SCI0 for 8N1, 9600 baud, polled I/O
// The value for the baud selection registers is determined
// using the formula:
//
// SCI0 Baud Rate = ( 2 MHz Bus Clock ) / ( 16 * SCI0BD[12:0] )
//--------------------------------------------------------------
void InitializeSerialPort(void)
{
    // Set baud rate to ~9600 (See above formula)
    SCI0BD = 13;          
    
    // 8N1 is default, so we don't have to touch SCI0CR1.
    // Enable the transmitter and receiver.
    SCI0CR2_TE = 1;
    SCI0CR2_RE = 1;
}

//*****************************************************************************
// This unmitigated piece of crap holds the recipies to be run by each servo. 
//
// Parameters: NONE
//
// Return: None
//*****************************************************************************
void initializeServos(void) 
{
  const UINT8 TwentymsTicks = 250; // There are 250 ticks in 20ms.
  
  PWME   = 0x00; // Disable All servos
  PWMCAE = 0x00; // Set the outputs for all PWMs to left aligned
  PWMPOL = 0x03; // Set the pulse Width Channel 0 and 1 Polarity to high.
  
  // Bus Clock is 2MHz.
  PWMPRCLK=0x04; // Set clock A to bus clock / 16  = 125000 Hz
  PWMSCLA= 0x05; // Clock SA = Clock A / (2 * PWMSCLA)    = 12500 Hz
  PWMCLK = 0x03; // select Scaled Clock A (SA) for PWM channel 0 and 
                 // PWM channel 1                
  PWMCTL = 0x00; // set everthing in the PWMCTL registers to 0 to
                 // provide a baseline.
                 
  // Initialize the Task Control Blocks.
  servoA.status  = paused;
  servoA.currentCommand = &bufferServoA; 
  servoA.loopFlag = FALSE;
  servoA.loopCounter = 0;
  servoA.firstLoopInstruction = 0;
  servoA.currentServoPosition = 255;  // These are 255 so that if the first command is to 
                                      // go to position 0 it will go there. 
  servoA.expectedServoPosition = 255; // These are 255 so that if the first command is to 
                                      // go to position 0 it will go there. 
  servoA.timeLeftms = 0;
  
  servoB.status = paused;
  servoB.currentCommand = &bufferServoB;
  servoB.loopFlag = FALSE;
  servoB.loopCounter = 0;
  servoB.firstLoopInstruction = 0;
  servoB.currentServoPosition = 255;  // These are 255 so that if the first command is to 
                                      // go to position 0 it will go there.  
  servoB.expectedServoPosition = 255; // These are 255 so that if the first command is to 
                                      // go to position 0 it will go there. 
  servoB.timeLeftms = 0;
  
  //Initialize the status LED port.
  DDRA = 0xFF;
}

// Initializes I/O and timer settings for the demo.
//--------------------------------------------------------------       
void InitializeTimer(void)
{
  // Set the timer prescaler to %2, since the bus clock is at 2 MHz,
  // and we want the timer running at 1 MHz
  TSCR2_PR0 = 1;
  TSCR2_PR1 = 0;
  TSCR2_PR2 = 0;
    
  // Enable output compare on Channel 1
  TIOS_IOS1 = 1;
  
  // Set up output compare action to toggle Port T, bit 1
  TCTL2_OM1 = 0;
  TCTL2_OL1 = 1;
  
  // Set up timer compare value
  TC1 = TC1_VAL;
  
  // Clear the Output Compare Interrupt Flag (Channel 1) 
  TFLG1 = TFLG1_C1F_MASK;
  
  // Enable the output compare interrupt on Channel 1;
  TIE_C1I = 1;  
  
  //
  // Enable the timer
  // 
  TSCR1_TEN = 1;
   
  //
  // Enable interrupts via macro provided by hidef.h
  //
  EnableInterrupts;
}

//*****************************************************************************
// This unmitigated piece of crap will process the commands that make up a
// recipie.
//
// Parameters:  servo          Holds a pointer to the servos Task Control Block.
//              command        The Command to be executed by the servo.
//              commandContext The context of the comamnd extracted fromt the one
//                             byte command.
//
// Return: None
//*****************************************************************************
void processCommand (struct TaskControlBlock* servo, enum COMMANDS command, UINT8 commandContext)
{ 
  UINT16 positionChange = 0;
  const UINT16 PerPositionIncrementms = 200;
  const UINT16 waitTimeIncrementms = 100;
  
  switch(command) 
  {
     case RECIPE_END:
          //printf("\r\n processCommand: RECIPE_END\r\n");
          // Turn off the servos
           if(servo == &servoA) 
              {
                 // if both servos are on only turn off servo A
                 if (PWME == 0x03) 
                 {
                    PWME = 0x02;
                 } 
                 else {
                    PWME = 0x00;
                 }
                 
                 // Set the status LED for this commands.
                 PORTA = PORTA | 0x20;
                 
                 // Flag is set for reciepe end so that the servo will not process any more commands.
                 reciepeEndServoA = 1;    
              } 
              else if(servo == &servoB)
              {
                 // if both servos are on only turn off servo B
                 if (PWME == 0x03) 
                 {
                    PWME = 0x01;
                 } 
                 else {
                    PWME = 0x00;
                 }
                 
                 // Set the status LED for this commands.
                 PORTA = PORTA | 0x02;
                 
                 // Flag is set for reciepe end so that the servo will not process any more commands.
                 reciepeEndServoB = 1; 
              } 
          break;
     case MOV:
         //printf("\r\n processCommand: MOV %d\r\n", commandContext);
        // Check to make sure the command is valid.
        // The positions are 0-5
        if(commandContext < 6) 
        {
           // if the servo position in the command is different to the
           // the current position proces it.
              // update the expected servo position
              servo->expectedServoPosition = commandContext;
        
              // Calculate out the amount of time it will take the command
              // to run.
              if(servo->currentServoPosition == 255) 
              {
                 positionChange =  commandContext;
              }
              else if(servo->expectedServoPosition < servo->currentServoPosition) 
              {
                 positionChange = servo->currentServoPosition - servo->expectedServoPosition; 
              } 
              else
              {
                 positionChange = servo->expectedServoPosition - servo->currentServoPosition;
              } 
          
              //printf("\r\nprocessCommand: setting positionChange %u\r\n", positionChange);
          
              servo->timeLeftms = positionChange * PerPositionIncrementms;
             // printf("\r\nprocessCommand: setting processCommand %u\r\n", servo->timeLeftms);
          
              // Send the commands down the PWM channel.
              // figure out which channel to send it down on.
              if(servo == &servoA) 
              {
          
                 //printf("\r\nprocessCommand: setting servoA\r\n");
                 PWMDTY0 = servoPositionTicks[servo->expectedServoPosition];
                 PWME = PWME |0x01; 
              } 
              else if(servo == &servoB)
              {
                 //printf("\r\nprocessCommand: setting servoB\r\n");
                 PWMDTY1 = servoPositionTicks[servo->expectedServoPosition];
                 PWME = PWME |0x02;
              } 
              else {
                 printf("\r\nprocessCommand: undefined servo\r\n");
              }
          
              // Update the Task Control Block Status.
              servo->status = running;
              servo->currentCommand++;

        }
    
        break;
     case WAIT:
        // The wait lengths are 0-31
        
        //printf("\r\n processCommand: WAIT %d\r\n", commandContext);
        if(commandContext < 32) 
        {   
            // Calculate out the amount of time it will take the command
            // to run.
            servo->timeLeftms = (commandContext) * waitTimeIncrementms;
            
            // Update the Task Control Block Status.
            if(servo == &servoA || servo == &servoB) 
            {    
              // increment the command buffer;
              servo->currentCommand++;
            } 
            else 
            {
              printf("\r\nprocessCommand: undefined servo\r\n");
            }
                               
            servo->status = running;    
        }
        
        break;
        
     case LOOP_START:
        //printf("\r\n processCommand: LOOPSTART %d\r\n", commandContext);
                  
        // if we do not have a nested loop set things up for
        // a loop.
        if(servo->loopFlag == FALSE) 
        {
           servo->loopFlag = TRUE;
            
           // Set the number of iterations.
           servo->loopCounter = commandContext;
            
           // Set a pointer to the instruction after the
           // LOOPSTART command and move onto the next
           // instruction.
           servo->firstLoopInstruction = servo->currentCommand + 1;
           
           // Increment the instruction pointer.
           servo->currentCommand++;
           
           // Rajeev we need LED status lights here.  
         } 
         else 
         {
            // place the task in an error state.
            servo->status = error;
            
            // indicate an error for that servo.
            if(servo == &servoA)
            {
              printf("\r\nprocessCommand: Nested Loop Error for servoA\r\n");
              PORTA = PORTA | 0x40;        // Reciepy command error.
            } else if(servo == &servoB){
              printf("\r\nprocessCommand: Nested Loop Error for servoB\r\n");
              PORTA = PORTA | 0x04;        // Reciepy command error.
      
            } 
         }
         
         break;
         
     case END_LOOP:
        //printf("\r\n processCommand: END_LOOP\r\n");
        
        // if we are not on our last iteration of the loop
        if(servo->loopCounter > 0) 
        {  
           // Go back to the instruction after the LOOP_START command.
           servo->currentCommand = servo->firstLoopInstruction;
           
           // deincrement the loop counter.
           --(servo->loopCounter);
        } 
        else 
        {
           // Ok we're done with the loop.  Clean up the TCB and
           // go to the next instruction.
           servo->loopFlag = FALSE;
           servo->firstLoopInstruction = 0;
           
           
           if(servo == &servoA || servo == &servoB) 
           {    
             // increment the command buffer;
             servo->currentCommand++;
           } 
           else 
           {
             printf("\r\nprocessCommand: undefined servo\r\n");
           }
        }
          
        break;
        
     case BREAK_LOOP :
        //printf("\r\n processCommand: BREAK_LOOP\r\n");
        
        //While loop will shift the pointer to the end of current loop. 
        while(*servo->currentCommand != END_LOOP) {
          servo->currentCommand++;
        }
        
        // clean up the TCB since were out of the loop.
        servo->loopFlag = FALSE;
        servo->firstLoopInstruction = 0;
           
        servo->currentCommand++; //finally we are pointing to next command in reciepe.
        
        break;
          
     default:
     
        // set the status lights to indicate a recipe command error.
        if(servo == &servoA)
        {
           printf("\r\nprocessCommand: undefined command for servoA\r\n");
           PORTA = PORTA | 0x80;        // Recipe command error.
        } else if(servo == &servoB){
           printf("\r\nprocessCommand: undefined command for servoB\r\n");
           PORTA = PORTA | 0x08;        // Recipe command error.
        } 
        else 
        {
           printf("\r\nprocessCommand: Unknown error occured.\r\n");
        }
  } 
}

//*****************************************************************************
// This unmitigated piece of crap will process the commands input from the user.
//
// Parameters: NONE
//
// Return: None.
//*****************************************************************************
void processUserCommand(void) 
{
   // process command
   
     // process the continue command.
   if((servo1UserInput == 0x63 || servo1UserInput == 0x43) &&
       servoA.status != error && (firstThree(*servoA.currentCommand)) != RECIPE_END) 
   {
      servoA.status  = running;
      PORTA = PORTA & 0xEF;
      //printf("\r\n processUserCommand: servoA.status = running\r\n");
   }
   
   if((servo2UserInput == 0x63 || servo2UserInput == 0x43) && 
      servoB.status != error && (firstThree(*servoB.currentCommand)) != RECIPE_END) 
   {
      servoB.status  = running;
      PORTA = PORTA & 0xFE;
     // printf("\r\n processUserCommand: servoB.status = running\r\n");
   }
   
      // process the pause command.
   if((servo1UserInput == 0x50 || servo1UserInput == 0x70) &&
       servoA.status != error  && (firstThree(*servoA.currentCommand)) != RECIPE_END) 
   {
      printf("\r\nYou have following options: \n\r l = Move left \n\r r = Move Right\n\r s = Switch Reciepe\n\r c = Continue reciepe\n\r n = no-op\n\r b = Restart reciepe");
      servoA.status  = paused;
      PORTA = PORTA | 0x10;
      //printf("\r\n processUserCommand: servoA.status = paused\r\n");
   }
   
   if((servo2UserInput == 0x50 || servo2UserInput == 0x70) && 
      servoB.status != error && (firstThree(*servoB.currentCommand)) != RECIPE_END) 
   {
      servoB.status = paused;
      PORTA = PORTA | 0x01;
      //printf("\r\n processUserCommand: servoB.status = paused\r\n");
   }
   
       // process the restart command.
   if((servo1UserInput == 0x42 || servo1UserInput == 0x62)) 
   {
      servoA.currentCommand = &bufferServoA;
      servoA.status  = ready;
      PORTA = PORTA & 0x0F;
      reciepeEndServoA = 0;
      servoA.loopFlag = FALSE;
      //printf("\r\n processUserCommand: B is pressed for ServoA.\r\n");
   }
   
    if((servo2UserInput == 0x42 || servo2UserInput == 0x62)) 
   {
      servoB.currentCommand = &bufferServoB;
      servoB.status = ready;
      PORTA = PORTA & 0xF0;
      reciepeEndServoB = 0;
      servoB.loopFlag = FALSE;
      //printf("\r\n processUserCommand: B is pressed for ServoB\r\n");
   }
   
        // process the no-op command.
   if((servo1UserInput == 0x4E || servo1UserInput == 0x6E) &&
       servoA.status != error ) 
   {
      servoA.status  = donothing;
      //printf("\r\n processUserCommand: B is pressed for ServoA.\r\n");
   }
   
    if((servo2UserInput == 0x4E || servo2UserInput == 0x6E) && 
      servoB.status != error ) 
   {
      servoB.status = donothing;
      //printf("\r\n processUserCommand: B is pressed for ServoB\r\n");
   }
   
   // process the run Right command.
   if((servo1UserInput == 0x52 || servo1UserInput == 0x72) &&
       servoA.status != error ) 
   {
      if(servoA.currentServoPosition != 0){
        processCommand(&servoA,MOV, --servoA.currentServoPosition );
      }
      servoA.status = donothing;      
   }
   
    if((servo2UserInput == 0x52 || servo2UserInput == 0x72) && 
      servoB.status != error ) 
   {
      if(servoB.currentServoPosition != 0){
      processCommand(&servoB,MOV, --servoB.currentServoPosition);
      }
      servoB.status = donothing;
   }
   
    // process the run Left command.
   if((servo1UserInput == 0x4C || servo1UserInput == 0x6C) &&
       servoA.status != error ) 
   {
      if(servoA.currentServoPosition != 5){
          processCommand(&servoA,MOV, ++servoA.currentServoPosition);
      }
      servoA.status = donothing;
   }
   
    if((servo2UserInput == 0x4C || servo2UserInput == 0x6C) && 
      servoB.status != error ) 
   {
      if(servoB.currentServoPosition != 5){
          processCommand(&servoB,MOV, ++servoB.currentServoPosition);
      }
      servoB.status = donothing;
   }
   
     // Process the swap Command which changes the reciepe for the servos.
   if((servo1UserInput == 0x53 || servo1UserInput == 0x73) &&
       servoA.status != error ) 
   {
      servoA.currentCommand = servoB.currentCommand;
   }
   
    if((servo2UserInput == 0x53 || servo2UserInput == 0x73) && 
      servoB.status != error ) 
   {
      servoB.currentCommand = servoA.currentCommand;
   }
   
   // set global variables to 0 so we know we have new input
   servo1UserInput = 0;
   servo2UserInput = 0;
}

//*****************************************************************************
// This unmitigated piece of crap process the user commands and then either
// processes a new command or updates the status on a current command for 
// each servo.
//
// Parameters: NONE
//
// Return: None.
//*****************************************************************************
void runTasks(void) 
{ 
   // first process the user commands
   processUserCommand();
 
   // then run the recipies based on the changes from the processUserCommand
   // function.
   if(servoA.status  == ready && reciepeEndServoA != 1) 
   {
     // get the next command and process it.
     processCommand(&servoA,firstThree(*servoA.currentCommand), lastFive(*servoA.currentCommand));
   } 
   else if(servoA.status  == running)  
   {
     updateTaskStatus(&servoA);
   }

   if(servoB.status  == ready && reciepeEndServoB != 1) 
   {
     processCommand(&servoB, firstThree(*servoB.currentCommand), lastFive(*servoB.currentCommand));
   } 
   else if (servoB.status  == running)
   {
     updateTaskStatus(&servoB);
   }
}

//*****************************************************************************
// This unmitigated piece of crap updates the amount of time left for a command
// running on a servo.  If the amount of time has expired then it sets the task
// status to ready.
//
// Parameters: NONE
//
// Return: None.
//*****************************************************************************
void updateTaskStatus(struct TaskControlBlock* servo) 
{
   // Right now all we need to do is deincrement the timers
   // and update the task status to ready once the timer
   // for a thread has run out.
   
   // We are processing a command
   if(servo->status  == running) {
      
      //printf("\r\nprocessCommand: updateTaskStatus servo->timeLeftms %u\r\n", servo->timeLeftms);
      
      if(servo->timeLeftms > 0) 
      {
        //printf("\r\n updateTaskStatus: updateTime == TRUE && servo->timeLeftms > 0\r\n");
      
        servo->timeLeftms -=100;
      } 
      else 
      {
        // update time is 0.  set the servo postion to the expected
        // position and update the task status to ready.
        servo->currentServoPosition = servo->expectedServoPosition;
        servo->status = ready;
          
        //printf("\r\n updateTaskStatus: servostatus = ready\r\n");
      }
   }
}
  

// Output Compare Channel 1 Interrupt Service Routine
// Refreshes TC1 and clears the interrupt flag.
//          
// The first CODE_SEG pragma is needed to ensure that the ISR
// is placed in non-banked memory. The following CODE_SEG
// pragma returns to the default scheme. This is neccessary
// when non-ISR code follows. 
//
// The TRAP_PROC tells the compiler to implement an
// interrupt funcion. Alternitively, one could use
// the __interrupt keyword instead.
// 
// The following line must be added to the Project.prm
// file in order for this ISR to be placed in the correct
// location:
//		VECTOR ADDRESS 0xFFEC OC1_isr 
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------       
void interrupt 9 OC1_isr( void )
{
  TC1     +=  TC1_VAL;      
  TFLG1   =   TFLG1_C1F_MASK;  
  
  runTasks();
}
#pragma pop


// This function is called by printf in order to
// output data. Our implementation will use polled
// serial I/O on SCI0 to output the character.
//
// Remember to call InitializeSerialPort() before using printf!
//
// Parameters: character to output
//--------------------------------------------------------------       
void TERMIO_PutChar(INT8 ch)
{
    // Poll for the last transmit to be complete
    do
    {
      // Nothing  
    } while (SCI0SR1_TC == 0);
    
    // write the data to the output shift register
    SCI0DRL = ch;
}

// Polls for a character on the serial port.
//
// Returns: Received character
//--------------------------------------------------------------       
UINT8 GetChar(void)
{ 
  // Poll for data
  
  do
  {
    // Nothing
  } while(SCI0SR1_RDRF == 0);
   
  // Fetch and return data from SCI0
  return SCI0DRL;
}


// Entry point of our application code
// Initializes the 
//--------------------------------------------------------------       
void main(void)
{
  UINT8 userInputbuffer[100];
  InitializeSerialPort();
  
  // This function has to be before the InitializeTimer function.
  initializeServos();

  InitializeTimer();
  initializeCommands(); 
   
  // Show initial prompt
  (void)printf("Hey Babe I'm just too cool!\r\n");
  
   while(1)
   {
      getUserInput();

   }
}
