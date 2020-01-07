#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"

#define I2CONCLR_AAC        (0x1<<2)
#define I2CONCLR_SIC        (0x1<<3)
#define I2CONCLR_STAC       (0x1<<5)
#define I2CONCLR_I2ENC      (0x1<<6)

#define I2SCLH_SCLH			0x00000080
#define I2SCLL_SCLL			0x00000080

#define I2CONSET_I2EN       (0x1<<6) 
#define I2CONSET_AA         (0x1<<2)
#define I2CONSET_SI         (0x1<<3)
#define I2CONSET_STO        (0x1<<4)
#define I2CONSET_STA        (0x1<<5)

#define I2C_MAX_TIMEOUT       0x00FFFFFF

volatile uint32_t ticks = 0;	// zmienna do delay
volatile unsigned int I2C_MasterState = 0;
volatile unsigned int I2C_Timeout = 0;


void SysTick_Handler(void)
{
	++ticks;
}

void delayMS(int ms) //miliseconds
{
	ticks = 0;
	while(ticks < (ms*1000))
		__WFI(); // uspienie procesora
}


void lcdCleanBackground(void)
{
	lcdWriteReg(ADRX_RAM,0);
	lcdWriteReg(ADRY_RAM,0);
	lcdWriteIndex(DATA_RAM);
	for(int x = 0; x < 240; x++)
	{
		for(int y = 0; y < 320; y++)
		{
			lcdWriteData(LCDGreen);
		}
	}
}

void drawLine(short int x_, short int y_, short int length, short int thickness)
{
	for(int x = x_; x < (x_+length); x++)
	{
		for(int y = y_; y < (y_+thickness); y++)
		{
			lcdWriteReg(ADRX_RAM,x);
			lcdWriteReg(ADRY_RAM,y);
			lcdWriteReg(DATA_RAM,LCDBlack);
		}
	}
}



void drawLetter(short int offsetX, short int offsetY, unsigned char litera)
{
	unsigned char letter[16];
	GetASCIICode(0, letter, litera);
	for(int x = 0; x < 8; x++)
	{
		for(int y = 0; y < 16; y++)
		{
			//GetASCIICode
			if((letter[y] << x) == 1)
			{
				lcdWriteReg(ADRX_RAM,x+offsetX);
				lcdWriteReg(ADRY_RAM,y+offsetY);
				lcdWriteReg(DATA_RAM, LCDBlack);
		
			}
		}
	}
}
//https://sites.google.com/site/johnkneenmicrocontrollers/18b-i2c/i2c_lpc1768

void I2C_enable()
{
	//(0, 0);
	LPC_PINCON->PINSEL1 &= ~((0x03<<22)|(0x03<<24));
	LPC_PINCON->PINSEL1 |= ((0x01<<22)|(0x01<<24));	
	
	LPC_I2C0->I2CONCLR = I2CONCLR_AAC | I2CONCLR_SIC | I2CONCLR_STAC | I2CONCLR_I2ENC;
	
	LPC_PINCON->I2CPADCFG &= ~((0x1<<0)|(0x1<<2));
	LPC_I2C0->I2SCLL   = I2SCLL_SCLL;
	LPC_I2C0->I2SCLH   = I2SCLH_SCLH;
	
	NVIC_EnableIRQ(I2C0_IRQn);

	LPC_I2C0->I2CONSET = I2CONSET_I2EN;
	/*
	LPC_I2C0->I2CONSET = 0;
	LPC_I2C0->I2CONSET |= 1 << 6 | 1 << 2;
*/
}

/*
void I2C_start()
{
	LPC_GPIO0->FIOSET0 = 1;
	LPC_I2C0->I2CONSET |= 1<<5; 
	while (!(LPC_I2C0->I2CONSET & (1<<3)));
}

void I2C_stop()
{
	LPC_I2C0->I2CONSET |= 1<<4; 
	LPC_I2C0->I2CONCLR = 1<<3;
	while (LPC_I2C0->I2CONSET & (1<<4));
	LPC_GPIO0->FIOCLR0 = 1;
}
*/


unsigned int I2C_Start() {
	unsigned int retVal = 0;
 
  I2C_Timeout = 0;

	/*--- Issue a start condition ---*/
	LPC_I2C0->I2CONSET = I2CONSET_STA;	/* Set Start flag */
		
	/*--- Wait until START transmitted ---*/
	while(1)
	{
		if(I2C_MasterState == 1)
		{
			retVal = 1;
			break;	
		}
		if(I2C_Timeout >= I2C_MAX_TIMEOUT)
		{
			retVal = 0;
			break;
		}
		I2C_Timeout++;
	}
	
  return(retVal);
}

unsigned int I2C_Stop() {
	LPC_I2C0->I2CONSET = I2CONSET_STO;      /* Set Stop flag */ 
	LPC_I2C0->I2CONCLR = I2CONCLR_SIC;  /* Clear SI flag */ 
						
	/*--- Wait for STOP detected ---*/
	while(LPC_I2C0->I2CONSET & I2CONSET_STO);

  return 1;
}

extern void I2C0_IRQHandler(void)
{
	//drawLine(50,50,100,20);
	LPC_I2C0->I2CONCLR |= 1<<3;
}
// strona 466 dokumentacja
int main(void)
{
	//SDA P0.27
	//SCL P0.28 
SysTick_Config(SystemCoreClock / 1000 / 1000);

I2C_enable();
lcdConfiguration();
init_ILI9325();
lcdCleanBackground();
	
while(1)
{
	//I2C_Start();
	//I2C_Stop();
	drawLetter(100, 100, 'z');
	//lcdCleanBackground();
	//drawLine(30, 20, 50, 3);
	//drawLetter(50, 50, 'A');
	/* Odczyt rejestru wyswietlacza
	uint16_t zmienna = lcdReadReg(0);
	lcdWriteIndex(zmienna);
	uint16_t zmienna2 = lcdReadData();
	*/
}
}
