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

#define TRUE 1
#define FALSE 0

#define CHARS_PER_LINE 16
#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CURSOR_END_LINE "\x1b[0K"

#define AVG_UPPER_BOUND 10
#define TIMER_0_RESULT TH0*256.0+TL0

#define P1_6_TEST QFP32_MUX_P1_6
#define P1_7_REF  QFP32_MUX_P1_7
#define PI 3.14159265359
#define SQRT_2 1.4142135

#define NO_SIGNALS 00
#define NO_REF	   01
#define NO_TEST    10
#define NORMAL     11



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
	double half_period = 0.0;
	double period = 0.0;

	double ref_rms = 0.0;
	double ref_peak = 0.0;

	double test_rms = 0.0;
	double test_peak = 0.0;

	double time_diff = 0.0;
	double phase_diff = 0.0;

	double peak_time;
	
	int error_flag = FALSE;
	int iteration_count = 1;
	int average_count = 0;
	int signal = NORMAL;

	char signal_detect_arr[2] = "11";
	char unit_choice[2];
	char iteration_count_arr[3];
	char units[] = "deg";

	char * rms_char = malloc(17*sizeof(char));
	char * phase_char = malloc(17* sizeof(char));

	waitms(500); //PuTTY startup
	printf(ANSI_CLEAR_SCREEN);

	printf ("Lab 5: AC Voltmeter\n"
	        "Authors: Ryan Acapulco, Zhi Chuen Tan\n"
			"Lab Section: L2B (M/W 12-3pm)\n"
			"Term: 2019W2\n\n"
	        "Compiled: %s, %s\n\n",
	        __DATE__, __TIME__);

	printf ("Please select units for phase: \n"
			"1 - Degrees \n"
			"2 - Radians \n");

	getsn(unit_choice, sizeof(unit_choice));

	printf ("This program will measure voltage, period, and time difference n times and find their average values.\n"
			"Please enter a value for n\n"
			"n must be between 1 and %d\n"
			"n = \n", AVG_UPPER_BOUND);

	do
	{
		getsn(iteration_count_arr, sizeof(iteration_count_arr));
		sscanf(iteration_count_arr, "%d", &iteration_count);

		if (iteration_count < 1 || iteration_count > AVG_UPPER_BOUND) 
		{
			printf("ERROR: Invalid input!\n"
				   "n must be between 1 and %d\n", AVG_UPPER_BOUND);
			error_flag == TRUE;
		}
		else
			error_flag = FALSE;
			
	} while (error_flag == TRUE);
	

	InitPinADC(1, 6);
	InitPinADC(1, 7);
	InitADC();

	LCD_4BIT(); // initialize LCD

	        //1234567890123456
	LCDprint("   Welcome to   ", 1, 1);
	LCDprint("     Lab 6!     ", 2, 1);

	// initialize timer 0
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit timer
	TR0=0; // Stop Timer/Counter 0


	while (1) {

		half_period = 0.0;
		period = 0.0;
		peak_time = 0.0;
		ref_peak = 0.0;
		test_peak = 0.0;
		time_diff = 0.0;

		// Checks if there are any signals;

		if(Volts_at_Pin(P1_7_REF) == 0)
			waitms(100); // if no signal detected on P1_7, wait 100ms and check again
		
		if(Volts_at_Pin(P1_7_REF) == 0)
			signal_detect[1] = '0';	// if no signal detected at this point, then REF isnt connected
		
		if(Volts_at_Pin(P1_6_TEST) == 0)
			waitms(100);
		
		if(Volts_at_Pin(P1_6_TEST) == 0)
			signal_detect[0] = '0';

		// writes data as int
		sscanf(signal_detect_arr, "%d", &signal);

		switch (signal)
		{
			case NORMAL:
				for (average_count = 0; average_count < iteration_count; average_count++)
				{
					Volts_at_Pin(P1_7_REF); // dump overflow value
					TL0=0;
					TH0=0;
					while(Volts_at_Pin(P1_7_REF)!=0);
					// Wait for the signal to be zero
					while(Volts_at_Pin(P1_7_REF)==0);
					// Wait for the signal to be positive
					TR0=1;
					while(Volts_at_Pin(P1_7_REF)!=0);
					TR0=0;
					// Stop timer 0

					half_period += TIMER_0_RESULT;
				}

				half_period /= iteration_count;
				period = half_period * 2 * (12/SYSCLK);
				peak_time = half_period / 4;


				// REF: P1_7
				for (average_count = 0; average_count < iteration_count; average_count++)
				{
					TL=0;
					TH=0;
					while(Volts_at_Pin(P1_7_REF) != 0);
					// Wait for the signal to be zero
					while(Volts_at_Pin(P1_7_REF) == 0);
					// Wait for the signal to be positive
					TR0=1;
					while(TIMER_0_RESULT <= peak_time);
					TR0=0;

					ref_peak += Volts_at_Pin(P1_7_REF);
				}

				ref_peak /= iteration_count;
				ref_rms = ref_peak / SQRT_2;

				// TEST: P1_6
				for (average_count = 0; average_count < iteration_count; average_count++)
				{
					TL=0;
					TH=0;
					while(Volts_at_Pin(P1_6_TEST) != 0);
					// Wait for the signal to be zero
					while(Volts_at_Pin(P1_6_TEST) == 0);
					// Wait for the signal to be positive
					TR0=1;
					while(TIMER_0_RESULT <= peak_time);
					TR0=0;

					test_peak += Volts_at_Pin(P1_6_TEST);
				}

				test_peak /= iteration_count;

				// Phase difference
				for (average_count = 0; average_count < iteration_count; average_count++)
				{
					TL=0;
					TH=0;
					while(Volts_at_Pin(P1_6_TEST) != 0);
					
					while(Volts_at_Pin(P1_6_TEST) == 0);
					TR0=1;

					while(Volts_at_Pin(P1_7_REF) != 0);
					TR0=0;

					time_diff += TIMER_0_RESULT * (12/SYSCLK);
				}

				time_diff /= iteration_count;

				switch (unit_choice[0])
				{
					case '1':
						phase_diff = time_diff * (360 / period);
						units = "deg";
								//1234567890123456
						LCDprint("             deg", 2, 0);
						break;
					
					case '2':
						phase_diff = time_diff * ((2*PI)/ period);
						units = "rad";
						//1234567890123456
						LCDprint("             deg", 2, 0);
						break;
					
					default:
						printf("\rERROR: UNIT DEFAULT CASE");
								//1234567890123456
						LCDprint("  DEFAULT ERROR ", 1, 1);
						LCDprint("      UNITS     ", 2, 1);
						break;
				}

				printf("Vrms(REF) = %.4f V, Vrms(TEST) = %.4f V, Period = %.4fs, Phase difference = %.4f%s"
						ref_rms, test_rms, period, phase_diff, units);
				printf(ANSI_CURSOR_END_LINE); // ANSI: Clear from cursor to end of line.
								//1234567890123456
				sprintf(rms_char, "     %.3f", test_rms);
						//1234567890123456
				LCDprint("               V", 1, 0);
				LCDprint(rms_char, 1, 0);	
				LCDprint("Vrms:           ", 1, 0);

								//Phase:
				sprintf(phase_char, "      %.3f", phase_diff);

						//1234567890123456
				LCDprint(rms_char, 1, 0);	
				LCDprint("Phase:           ", 2, 0);
				break;
			
			case NO_REF:
				printf("\rNo reference signal detected!");
				printf(ANSI_CURSOR_END_LINE);

						//1234567890123456
				LCDprint("    CONNECT    ", 1, 1);
				LCDprint("   REFERENCE   ", 2, 1);	
				break;
			
			case NO_TEST:
				printf("\rNo test signal detected!");
				printf(ANSI_CURSOR_END_LINE);

						//1234567890123456
				LCDprint("    CONNECT    ", 1, 1);
				LCDprint("      TEST     ", 2, 1);	
				break;

			case NO_SIGNALS:
				printf("\rNo signals detected!");
				printf(ANSI_CURSOR_END_LINE);

						//1234567890123456
				LCDprint("   NO SIGNALS   ", 1, 1);
				LCDprint("    DETECTED    ", 2, 1);	
				break;
			
			default:
				printf("\rERROR: SIGNAL DEFAULT CASE");
						//1234567890123456
				LCDprint("  DEFAULT ERROR ", 1, 1);
				LCDprint("      SIGNAL    ", 2, 1);	
				break;
		}

		waitms(500);
		waitms(500);	
	}
}	
