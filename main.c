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

#define TRUE 1
#define FALSE 0

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

// Possible Task Statuses.
enum TASKSTATUS
{
  ready = 0,
  running,
  blocked,
  suspended
};

// the order of these commands match the op codes 
// listed in the assignment.
enum COMMANDS
{
  RECIPE_END = 0,
  MOV = 32,
  WAIT,
  TBD1,
  LOOPSTART,
  ENDLOOP,
  TBD2,
  TBD3
};

// Holds the information for each task.
struct TaskControlBlock 
{
   enum TASKSTATUS status;
   enum COMMANDS currentCommand;
   UINT8 currentServoPosition; // 0-5
   UINT8 expectedServoPosition; // 0-5
   INT16 timeLeftms;          // timeleft to execute the current
                              // command.
};

struct TaskControlBlock servoA;
struct TaskControlBlock servoB;

// This is used to tell the updateTaskStatus function
// to deincrement the time by 100ms.  It is set by the
// interrupt function.
UINT8 updateTime = FALSE;


// Function definition
void initializeServos(void);
void processCommand(struct TaskControlBlock* servo, enum COMMANDS command, UINT8 commandContext);
void runTasks(void);
void updateTaskStatus(struct TaskControlBlock* servo);


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

// Sets up the servos
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
  PWMPER0= TwentymsTicks; // PWMx Period = Channel Clock Period (SA) * (2 * PWMPERx)  Left Aligned
                 // 250 ticks = 20 ms and matches the specs.
                 // Number of SA clock ticks that make up a period.
                 
  PWMCTL = 0x00; // set everthing in the PWMCTL registers to 0 to
                 // provide a baseline.
                 
  // Initialize the Task Control Blocks.
  servoA.status  = ready;
  servoA.currentServoPosition = 255;  // These are 255 so that if the first command is to 
                                      // go to position 0 it will go there. 
  servoA.expectedServoPosition = 255;
  servoA.timeLeftms = 0;
  
  servoB.status  = ready;
  servoB.currentServoPosition = 0;
  servoB.expectedServoPosition = 0;
  servoB.timeLeftms = 0;
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

// Processes a command and calculates the time it will take to execute 
//-------------------------------------------------------------- 
void processCommand (struct TaskControlBlock* servo, enum COMMANDS command, UINT8 commandContext)
{ 
  UINT16 positionChange = 0;
  const UINT16 PerPositionIncrementms = 200;
  const UINT16 waitTimeIncrementms = 100;
  
  switch(command) 
  {
     case MOV:
         printf("\r\n processCommand: MOV\r\n");
        // Check to make sure the command is valid.
        // The positions are 0-5
        if(commandContext < 6) 
        {
            // Store the current command.
            servo->currentCommand = command;
           
           // if the servo position in the command is different to the
           // the current position proces it.
           if(servo->currentServoPosition != commandContext) 
           {
              // update the expected servo position
              servo->expectedServoPosition = commandContext;
        
              // Calculate out the amount of time it will take the command
              // to run.
              if(servo->expectedServoPosition < servo->currentServoPosition) 
              {
                 positionChange = servo->currentServoPosition - servo->expectedServoPosition; 
              } 
              else 
              {
                 positionChange = servo->expectedServoPosition - servo->currentServoPosition;
              }
          
              servo->timeLeftms = positionChange * PerPositionIncrementms;
              //printf("\r\nprocessCommand: setting processCommand %d\r\n", servo->timeLeftms);
          
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
              //printf("\r\n processCommand: servostatus = running\r\n");
           } 
        }
    
        break;
     case WAIT:
        // The wait lengths are 0-31
        
        printf("\r\n processCommand: WAIT\r\n");;
        if(commandContext < 32) 
        {
            // Store the current command.
            servo->currentCommand = command;
            
            // Calculate out the amount of time it will take the command
            // to run.
            
            // Calculate out the amount of time it will take the command
            // to run.
            // the reason it's commandContext + 1 is because wait 0 = 100ms
            servo->timeLeftms = (commandContext + 1) * waitTimeIncrementms;
                   
            // Update the Task Control Block Status.
            servo->status = running;    
        }
        
        break;
     
     default:
      printf("\r\nprocessCommand: undefined command\r\n");
  } 
}

void runTasks(void) 
{
   if(servoA.status  == ready) 
   {
     // get the next command and process it.
     processCommand(&servoA, WAIT, 0);
   } else {
      updateTaskStatus(&servoA);
   }
   
   if(servoB.status  == ready) 
   {
     processCommand(&servoB, WAIT, 31);
   } else {
     updateTaskStatus(&servoB);
   }
}

void updateTaskStatus(struct TaskControlBlock* servo) 
{
   // Right now all we need to do is deincrement the timers
   // and update the task status to ready once the timer
   // for a thread has run out.
   
   // We are processing a command
   if(servo->status  == running) {
      if(updateTime == TRUE && servo->timeLeftms > 0) 
      {
        servo->timeLeftms -=100;
        
        if(servo->timeLeftms == 0) 
        {
          servo->currentServoPosition = servo->expectedServoPosition;
          servo->status = ready;
          
          printf("\r\n updateTaskStatus: servostatus = ready\r\n");
        }
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
  
  updateTime = TRUE;
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
//--------------------------------------------------------------       
void main(void)
{

// UINT8 userInput;
  
  InitializeSerialPort();
  InitializeTimer();
  initializeServos(); 
   
// Show initial prompt
  (void)printf("Hey Babe I'm just too cool!\r\n");
       ;
while(1)
{
   runTasks();
}

   

}
