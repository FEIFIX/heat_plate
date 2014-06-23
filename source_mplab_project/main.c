#include <p18F4221.h>
#include <i2c.h>
#include <delays.h>
#include <math.h>

/*
 00
5  1
 66
4  2
 33  7
*/

#define	P1	LATEbits.LATE0
#define	P2	LATEbits.LATE1
#define	P3	LATEbits.LATE2

#define	FET			LATCbits.LATC1
#define	FET_T		TRISCbits.TRISC1

#define	BEEPER			LATCbits.LATC2
#define	BEEPER_T		TRISCbits.TRISC2


#define BTN			PORTBbits.RB0
#define BTN_T		TRISBbits.TRISB0

#define	LEDG		LATBbits.LATB3
#define	LEDG_T		TRISBbits.TRISB3
#define	LEDY		LATBbits.LATB4
#define	LEDY_T		TRISBbits.TRISB4
#define	LEDR		LATBbits.LATB5
#define	LEDR_T		TRISBbits.TRISB5


//kty210
//#define	ADC_RES		1500

//kty110
#define	ADC_RES		980

/*
//kty 210
#define	TCALC_A		-153846
#define	TCALC_B		1175
#define	TCALC_C		-121
*/

//kty84-120
#define	TCALC_A		-13196
#define	TCALC_B		319
#define	TCALC_C		-136

/*
//kty110
#define	TCALC_A		-42319
#define	TCALC_B		600
#define	TCALC_C		-117
*/

#define	TEMP_SET	105
#define	TIME_SET	5400

#define	TEMPER_VAL_MAX	9
const rom unsigned char temperatures[TEMPER_VAL_MAX] = {0,80,90,100,110,120,130,140,150};


const rom unsigned char disp_numbers[16] = 
{0b00111111,0b00000110,
0b01011011,0b01001111,
0b01100110,0b01101101,
0b01111101,0b00000111,
0b01111111,0b01101111,
0b00110111,0b00000000,
0b01000000,0b00000000,
0b01111001,0b01010000,
};

unsigned char disp_counter;
unsigned char disp_array[3];
volatile unsigned char iic_timeout;
unsigned char msec_counter;
volatile unsigned int sec_counter;
unsigned int secs;
unsigned char minutes,minutes_set,state_count;
volatile unsigned char beeper,beeper_state;

unsigned char temp_set, temper_val;

unsigned char temper,state;
unsigned char I2C_BUFFER;    // temp buffer for R/W operations
unsigned char BIT_COUNTER;   // temp buffer for bit counting

unsigned char err_counter;

unsigned char temp_read(void);
void SWStopI2C ( void );
void SWStartI2C ( void );
void SWRestartI2C ( void );
void SWStopI2C ( void );
signed char SWAckI2C( void );
unsigned int SWReadI2C( void );
signed char SWWriteI2C( auto unsigned char data_out );
signed char Clock_test( void );

unsigned char calc_avg (unsigned char * array, unsigned char len);
unsigned int get_secs (void);
void set_secs (unsigned int val);
void set_beeper (unsigned char count);

#define TEMP_ARRAY_SIZE	10
unsigned char temp_array[TEMP_ARRAY_SIZE];
unsigned char temp_array_pointer;

void InterruptHandlerHigh (void);
#pragma code InterruptVectorHigh = 0x08
void InterruptVectorHigh (void)
{
  _asm
    goto InterruptHandlerHigh //jump to interrupt routine
  _endasm
}

#pragma code

#pragma interrupt InterruptHandlerHigh
void InterruptHandlerHigh (void)
{
if (INTCONbits.TMR0IF)
	{
	INTCONbits.TMR0IF=0;
	disp_counter++;
	if (disp_counter>2) disp_counter = 0;
	if (disp_counter==0)
		{
		P1 = 0;			
		P2 = 0;			
		P3 = 0;
		LATD = disp_numbers[disp_array[0]];
		P3 = 1;
		}
	if (disp_counter==1)
		{
		P1 = 0;			
		P2 = 0;			
		P3 = 0;
		LATD = disp_numbers[disp_array[1]];
		P2 = 1;
		}
	if (disp_counter==2)
		{
		P1 = 0;			
		P2 = 0;			
		P3 = 0;
		LATD = disp_numbers[disp_array[2]];	
		P1 = 1;		
		}
	iic_timeout++;
	TMR0H = 0xF8;
	TMR0L = 0x00;
	}
if ((PIR1bits.TMR1IF)&(PIE1bits.TMR1IE))
	{
	PIR1bits.TMR1IF = 0;
	msec_counter++;
	if (msec_counter>9)
		{
		msec_counter=0;
		if (sec_counter>0) sec_counter--;
		}
	if (beeper>0)
		{
		beeper_state++;
		if (beeper_state&0x01)
			{
			BEEPER = 1;
			}
		else
			{
			BEEPER = 0;
			if (beeper>0) beeper--;
			}
		}
	TMR1H = 0x3C;
	TMR1L = 0xB8;
	}
}
#pragma code



void main (void)
{
sec_counter=0;
ADCON0 = 0x01;
ADCON1 = 0x0E;
ADCON2 = 0xFF;
T0CON = 0x80;
INTCON = 0xE0;
T1CON = 0x11;
PIR1bits.TMR1IF = 0;
PIE1bits.TMR1IE = 1;
OSCCON = 0x70;
TRISD = 0;
TRISE = 0;
LATE = 0;
LATD = 0;
BTN_T = 1;
LEDR_T = 0;
LEDY_T = 0;
LEDG_T = 0;
disp_array[0] = 0;
disp_array[1] = 1;
disp_array[2] = 2;
temp_array_pointer = 0;
FET = 0;
FET_T = 0;
state  = 1;
state_count = 5;
minutes_set = 1;
beeper = 0;
err_counter = 0;
BEEPER_T = 0;
BEEPER = 0;
temper_val=0;
while (1)
	{
	temper = temp_read();
	if ((temper>2)&(temper<200))
		{
		Delay10KTCYx(10);
		err_counter = 0;
		temp_array[temp_array_pointer] = temper;
		temp_array_pointer++;
		if (temp_array_pointer==TEMP_ARRAY_SIZE) temp_array_pointer = 0;
		temper = calc_avg(temp_array,TEMP_ARRAY_SIZE);
		if (temper<temp_set) 
			{
			LEDR = 1;
			FET = 1;
			}
		else 
			{
			FET = 0;
			LEDR = 0;
			}
		if (state==0)	
			{
			LEDG = 1;
			LEDY = 0;
			disp_array[0] = temper/100;
			disp_array[1] = (temper - (disp_array[0]*100))/10;
			disp_array[2] = temper - (disp_array[0]*100) - (disp_array[1]*10);
			state_count = 0;
			if (BTN==0)
				{
				state = 2;
				if (temper_val<(TEMPER_VAL_MAX-1))
					temper_val++;
				else
					temper_val=0;
				}
			}
		temp_set = temperatures[temper_val];
		if (state==1)	
			{
			LEDG = 0;
			LEDY = 1;
			if (state_count>0) 
				state_count--;
			else
				state = 0;
			disp_array[0] = temp_set/100;
			disp_array[1] = (temp_set - (disp_array[0]*100))/10;
			disp_array[2] = temp_set - (disp_array[0]*100) - (disp_array[1]*10);
			}
		if (state==2)	
			{
			LEDG = 0;
			LEDY = 0;
			disp_array[0] = temp_set/100;
			disp_array[1] = (temp_set - (disp_array[0]*100))/10;
			disp_array[2] = temp_set - (disp_array[0]*100) - (disp_array[1]*10);
			state_count = 5;
			if (BTN==1)
				state = 1;
			}
		}
		else
			{
			}
	
	
	}

}

void set_beeper (unsigned char count)
{
PIE1bits.TMR1IE = 0;
beeper = count;
PIE1bits.TMR1IE = 1;
}

unsigned int get_secs (void)
{
unsigned int temp;
PIE1bits.TMR1IE = 0;
temp = sec_counter;
PIE1bits.TMR1IE = 1;
return temp;
}

void set_secs (unsigned int val)
{
PIE1bits.TMR1IE = 0;
sec_counter = val;
PIE1bits.TMR1IE = 1;
}

unsigned char calc_avg (unsigned char * array, unsigned char len)
{
unsigned char i;
unsigned int acc;
acc=0;
for (i=0;i<len;i++) acc = acc + array[i];
acc = acc / len;
return ((unsigned char) (acc));

}

unsigned char temp_read(void)
{
long res,res2,res3;
unsigned char ret;
//float val1,val2;
unsigned int adcres;
ADCON0bits.GO_DONE = 1;
while (ADCON0bits.GO_DONE == 1);
	
adcres = ((unsigned int)(ADRESH))<<8 | ((unsigned int)(ADRESL))<<0;
res = (ADC_RES*((unsigned long)(adcres)))/(1023-adcres);
res2 = res*res;
res2 = res2/TCALC_A;
res3 = (res*100)/TCALC_B;
res2 = res2 + res3 + TCALC_C;
/*
val2 = res;
val2 = val2*val2;
val1 = TCALC_A*val2 + TCALC_B*res + TCALC_C;
*/
ret = (unsigned char)(res2);
return ret;
}













