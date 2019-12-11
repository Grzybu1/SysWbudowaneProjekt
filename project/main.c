#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"

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


/*void drawLetter(short int offsetX, short int offsetY, char litera)
{
	int asciiLetter = litera;
	for(int x = 0; x < 8; x++)
	{
		for(int y = 0; y < 16; y++)
		{
			//GetASCIICode
			if(asciiLib[0][asciiLetter][y]<<x == 1)
			{
				lcdWriteReg(ADRX_RAM,x+offsetX);
				lcdWriteReg(ADRY_RAM,y+offsetY);
				lcdWriteReg(DATA_RAM, LCDBlack);
		
			}
		}
	}
}*/
//https://sites.google.com/site/johnkneenmicrocontrollers/18b-i2c/i2c_lpc1768

void I2C_enable()
{
	LPC_I2C0->I2CONSET |= 1<< 6;
}

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

void I2C0_IRQHandler()
{
	
}
// strona 466 dokumentacja
int main(void)
{
	//SDA P0.27
	//SCL P0.28 
PIN_ConfigureI2C0Pins(0, 0);
NVIC_EnableIRQ(I2C0_IRQn);

lcdConfiguration();
init_ILI9325();
while(1)
{
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
