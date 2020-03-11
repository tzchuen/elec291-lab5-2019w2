


/* My implementation
 * I haven't added any PuTTY or LCD stuff yet, just an outline to see if it makes sense to you
 */
#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L

// LCD pins
#define LCD_RS P2_6
// #define LCD_RW Px_x // Not used in this code.  Connect to GND
#define LCD_E  P2_5
#define LCD_D4 P2_4
#define LCD_D5 P2_3
#define LCD_D6 P2_2
#define LCD_D7 P2_1

#define SQRT_2 1.414213562373095
#define DEBUG_VALUE -12345678
#define MILLI_TO_MICRO 1000
#define BASE_TO_MILLI  1000
#define PERIOD_IN_DEGRESS 360
#define PI 3.14159265359
#define PERIOD_IN_RADIANS 2*PI
#define CHARS_PER_LINE 16

#define TRUE 1
#define FALSE 0

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X00;
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
  	
	return 0;
}

void InitADC (void)
{
	SFRPAGE = 0x00;
	ADC0CN1 = 0b_10_000_000; //14-bit,  Right justified no shifting applied, perform and Accumulate 1 conversion.
	ADC0CF0 = 0b_11111_0_00; // SYSCLK/32
	ADC0CF1 = 0b_0_0_011110; // Same as default for now
	ADC0CN0 = 0b_0_0_0_0_0_00_0; // Same as default for now
	ADC0CF2 = 0b_0_01_11111 ; // GND pin, Vref=VDD
	ADC0CN2 = 0b_0_000_0000;  // Same as default for now. ADC0 conversion initiated on write of 1 to ADBUSY.
	ADEN=1; // Enable ADC
}

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

#define VDD 3.3035 // The measured value of VDD in volts

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask); // Set pin as analog input
			P0SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 1:
			P1MDIN &= (~mask); // Set pin as analog input
			P1SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 2:
			P2MDIN &= (~mask); // Set pin as analog input
			P2SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;   // Select input from pin
	ADBUSY=1;       // Dummy conversion first to select new pin
	while (ADBUSY); // Wait for dummy conversion to finish
	ADBUSY = 1;     // Convert voltage at the pin
	while (ADBUSY); // Wait for conversion to complete
	return (ADC0);
}

float Volts_at_Pin(unsigned char pin)
{
	 return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);
}

void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	// LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

int getsn (char * buff, int len)
{
	int j;
	char c;
	
	for(j=0; j<(len-1); j++)
	{
		c=getchar();
		if ( (c=='\n') || (c=='\r') )
		{
			buff[j]=0;
			return j;
		}
		else
		{
			buff[j]=c;
		}
	}
	buff[j]=0;
	return len;
}

void main (void)
{
	
    char unit_choice[2];

	char * rms_char = malloc(17*sizeof(char));
	char * phase_char = malloc(17* sizeof(char));

	double ref_rms;
	double test_rms;

	unsigned long ref_half_period_us = 0;
	unsigned long test_half_period_us = 0;

	unsigned long ref_half_period;
	unsigned long test_half_period;

	double ref_freq;
	double test_freq;

	unsigned long time_difference_us = 0;

	double phase_deg;
	double phase_rad;
	
	int no_ref  = TRUE;
	int no_test = TRUE;

	int no_signal = 11;

	unsigned long Period;


    waitms(500); // Give PuTTy a chance to start before sending
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	
	printf ("Lab 5: AC Voltmeter\n"
	        "Authors: Ryan Acapulco, Zhi Chuen Tan\n"
			"Lab Section: L2B (M/W 12-3pm)\n"
			"Term: 2019W2\n\n"
	        "Compiled: %s, %s\n\n",
	        __DATE__, __TIME__);
	
	LCD_4BIT();
	
	// P2.2-2.5 used for LCD, use these ones instead
    InitPinADC(1, 4); // Configure P1.4 as analog input
    InitPinADC(1, 5); // Configure P1.5 as analog input
    InitPinADC(1, 6); // Configure P1.6 as analog input
    InitPinADC(1, 7); // Configure P1.7 as analog input
    InitADC();
			//1234567890123456
	LCDprint("  Hello world!  ", 1, 1);
	LCDprint("     Lab 5      ", 2, 1);

    printf ("\rPlease select units for phase:\n"
            "1: Radians\n"
            "2: Degrees\n\n");

    getsn(unit_choice, sizeof(unit_choice));


	while(1)
	{	
		// printf("\rV = %f \n",Volts_at_Pin(QFP32_MUX_P1_2));
		// // Measure half period at pin P1.0 using timer 0
		// TR0=0; // Stop timer 0
		// printf("test 1 \n");
		// TMOD=0B_0000_0001; // Set timer 0 as 16- // Set timer 0 as 16-bit timer bit timer
		// printf("test 2 \n");
		// TH0=0; TL0=0; // Reset the timer
		// printf("test 3 \n");
		// while (P1_7==1);// Wait for the signal to be zero
		// while (P1_7==0); // Wait for the signal to be one
		// TR0=1; // Start timing
		// while (P1_7==1); // Wait for the signal to be zero
		// TR0=0; // Stop timer 0
		// // [TH0,TL0] is half the period in multiples of 12/CLK, so:
		// Period=(TH0*0x100+TL0)*2; // Assume Period is unsigned int 
		
		// printf("\rT=%lu", Period);
		
	    //Use peak detector just to check if there's a signal
		if(P1_2 != 0.0)
			no_ref = FALSE;
		
		//printf("\rno_ref %d", no_ref);

		if(P1_3 != 0.0)
			no_test = FALSE;
		
		// printf("\rgot to P1_3");

		no_signal = (no_ref * 10) + no_test;
		
		//printf("\rgot to no_signal\n");

		printf("\r%d\n", no_signal); // debug
		
		switch (no_signal)
		{
			case 00:
				// Read 14-bit value from the pins configured as analog inputs
			
				while(P1_7 != 0);
				while(P1_7 == 0);
				while(P1_7 != 0) 
				{
					Timer3us(1);
					ref_half_period_us++;
				}
				// ref_half_period *= (1000*1000);

				while(P1_7 != 0);
				while(P1_7 == 0);
				Timer3us( (ref_half_period_us / 2.0) );
				ref_rms = ( (double) Volts_at_Pin(QFP32_MUX_P1_4) )/SQRT_2; 

				while(P1_6 != 0);
				while(P1_6 == 0);
				while(P1_6 != 0) 
				{
					Timer3us(1);
					test_half_period_us++;
				}
				// test_half_period *= (1000*1000);

				while(P1_6 != 0);
				while(P1_6 == 0); 
				Timer3us( (test_half_period_us / 2.0) );
				test_rms = ( (double) Volts_at_Pin(QFP32_MUX_P1_5))/SQRT_2;

				ref_freq = 1.0 / (ref_half_period * 2.0);
				test_freq = 1.0 / (test_half_period * 2.0);

				while(P1_7 != 0);
				while(P1_7 == 0);
				while(P1_7 != 0) 
				{
					Timer3us(1);
					time_difference_us++;
					
					if(P1_6 != 0)
						break;
				}

				phase_deg = time_difference_us * ( (double) PERIOD_IN_DEGRESS / (ref_half_period_us * 2.0) );
				phase_rad = time_difference_us * ( (double) PERIOD_IN_RADIANS / (ref_half_period_us * 2.0) );

				switch (unit_choice[0])
				{
				case '1':
					        //1234567890123456
					LCDprint("               V",1,0);
					                //1234567890123456
									//Vrms=
					sprintf(rms_char,"     %.3f", test_rms);

					LCDprint("             rad",2,0);
					                  //Phase=
					sprintf(phase_char,"      %.3f", phase_rad);

					printf("\rVrms (REF) = %.3fV, Vrms (TEST) = %.3fV, Frequency = %.3fHz, Phase (radians) = %.3frad",
							ref_rms, test_rms, phase_rad);
					printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
					break;

				case '2':
					       //1234567890123456
					LCDprint("               V",1,0);
					                //1234567890123456
									//Vrms=
					sprintf(rms_char,"     %.3f", test_rms);

					LCDprint("             deg",2,0);
					                  //Phase=
					sprintf(phase_char,"      %.3f", phase_deg);
					printf("\rVrms (REF) = %.3fV, Vrms (TEST) = %.3fV, Frequency = %.3fHz, Phase (degress) = %.3fdeg",
							ref_rms, test_rms, ref_freq, phase_deg);
					printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
					break;
				
				default:
					LCDprint("ERROR(units)",1,1);
					printf("\rDEFAULT CASE (units): ERROR");
					printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
					break;
				}
				break;
			
			case 01:
				//printf("%f",Volts_at_Pin(QFP32_MUX_P1_7));
				while(P1_7 != 0);
				while(P1_7 == 0);
				//printf("Passed ==0\n");
				while(P1_7 != 0) 
				{
					Timer3us(1);
					ref_half_period_us++;
				}
				// ref_half_period *= (1000*1000);
				printf("Passed timer\n");

				while(Volts_at_Pin(QFP32_MUX_P1_7) != 0);
				while(Volts_at_Pin(QFP32_MUX_P1_7) == 0);
				Timer3us( (ref_half_period_us / 2.0) );
				ref_rms = ( (double) Volts_at_Pin(QFP32_MUX_P1_4) )/SQRT_2; 
				//	ref_freq = 1.0 / (ref_half_period * 2.0);
				
				        //1234567890123456
				LCDprint("  CONNECT TEST  ", 1, 1);
				LCDprint("     SIGNAL     ", 2, 1);
				printf("\rNo TEST signal detected! Vrms (REF) = %.3fV, Frequency = %.3fHz", ref_rms, ref_freq);
				printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
				break;
			
			case 10:
				        //1234567890123456
				LCDprint("  CONNECT REF   ", 1, 1);
				LCDprint("     SIGNAL     ", 2, 1);
				printf("\rNo REFERENCE signal detected! Vrms (TEST) = %.3fV, Frequency = %.3fHz", test_rms, test_freq);
				printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
				break;

			case 11:
				        //1234567890123456
				LCDprint("   NO SIGNALS   ", 1, 1);
				LCDprint("    DETECTED    ", 2, 1);
				printf("\rNo signals detected!");
				printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
				break;
		
		default:
			LCDprint("ERROR: SIG",1,1);
			printf("\rDEFAULT CASE (signals): ERROR");
			printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
			break;
		}

        waitms(500);
	 }  
}	
