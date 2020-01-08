#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"

/*#define I2CONCLR_AAC        (0x1<<2)
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

#define I2C_MAX_TIMEOUT       0x00FFFFFF*/

volatile uint32_t ticks = 0;	// zmienna do delay
/*volatile unsigned int I2C_MasterState = 0;
volatile unsigned int I2C_Timeout = 0;*/



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

void drawLine(short int y_, short int x_, short int length, short int thickness, uint16_t color)
{
	for(int y = y_; y < (y_+thickness); y++)
	{
		for(int x = x_; x < (x_+length); x++)
		{
			lcdWriteReg(ADRX_RAM,x);
			lcdWriteReg(ADRY_RAM,y);
			lcdWriteReg(DATA_RAM,color);
		}
	}
}






void drawLetter(short int offsetX, short int offsetY, char litera, uint16_t color)
{
	unsigned char letter[16];
	GetASCIICode(1, letter, litera);
	for(int y = 0; y < 16; y++)
	{
		for(int x = 0; x < 8; x++)
		{
			//GetASCIICode
			if((letter[y]>>x) & (1))
			{
				lcdWriteReg(ADRX_RAM,offsetY-y);
				lcdWriteReg(ADRY_RAM,x+offsetX);
				lcdWriteReg(DATA_RAM, color);
		
			}
		}
	}
}
//Tu bedziemy dostawac dane
volatile unsigned char buforRead[20];

//Tu nalezy zapisac adres slave'a razem z bitem R/W
volatile unsigned char buforWrite[20];

//Tu wrzucamy ile bajtów danych sie spodziewamy
volatile int maxRead; 

//Tu wrzucamy ile bajtów danych wysylamy
volatile int maxWrite;

//informacje ile juz odebralismy.
int readId = 0;
	
//informacje ile juz wyslalismy.
int writeId = 0;


void I2C_enable()
{
	//DOMYSLNE - wlaczenie zegara dla I2C
	//LPC_SC->PCONP |= 1<<7;


	//Ustawienie zegara pclk domyslne na cclk/4
	//PCLKSEL0 = 0; 


	//Konfiguracja pinów na SDA0 i SCL0(s.129 dokumentacji)
	LPC_PINCON->PINSEL1 |= ((0x01<<22)|(0x01<<24)); 


	//Domyslne - ustawienie pinów w tryb normalny
	//LPC_PINCON->I2CPADCFG |= 0<<0 | 0<<1;

	//Czyscimy wszystkie flagi
	LPC_I2C0 -> I2CONCLR = 0;

	//Wlaczenie przerwan w nvic
	NVIC_EnableIRQ(I2C0_IRQn);

	//Ustawienie flagi EN zeby ustawic sie w tryb master
	LPC_I2C0->I2CONSET = 1<<6;
}

void I2C_transmit()
{
  readId = 0;
	
	writeId = 0;
	
	//Ustawienie bitu STA -> ustawienie flagi startu
	LPC_I2C0->I2CONSET = 1<<5;
	//Po ustawieniu flagi (teoretycznie) uruchamia sie przerwanie z kodem 0x08 w I2STAT

	//Czyscimy flage startu tak na wszelki wypadek
	LPC_I2C0->I2CONCLR = 1<<5;
}


//Manual strony 468 i 477+
extern void I2C0_IRQHandler(void)
{
	
	//W tej zmiennej mamy aktualny status transmisji który nalezy obsluzyc.
	unsigned char status = LPC_I2C0->I2STAT;

	switch(status)
	{
		//Wyslano pojedynczy sygnal startu i otrzymano ACK. Po tym idziemy do 0x40 lub 0x48
		case 0x08:
			//zapisujemy adres do wyslania - UWAGA! Adres musi posiadac na koncu bit W/R
			LPC_I2C0->I2DAT = buforWrite[writeId];

			writeId++;
			
			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;

		break;

		//Wyslalismy ponowny sygnal startu,
		//robimy w zasadzie to co przy 0x08 bo nie chcemy przechodzic do trybu transmisji
		case 0x10:
			//zapisujemy adres do wyslania - UWAGA! Adres musi posiadac na koncu bit W/R(w zaleznosci od niego przechodzimy do reciever/transmiter)
			LPC_I2C0->I2DAT = buforWrite[writeId];
		
			writeId++;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//wyslano sla+w, otrzymano ACK. Wyslemy 1 bajt.
		case 0x18:
			//ladujemy cos do wyslania
			LPC_I2C0->I2DAT = buforWrite[writeId];
			
			//oznaczamy elementjako wyslany
			writeId++;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;

		break;

		//Wyslano SLA + w, otrzymano not ACK. Wysylamy stop
		case 0x20:
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//wyslano bajt, otrzymano ack. Jesli jestcos do wyslania to wysli jesli nie to stop
		case 0x28:
			if((writeId) > maxWrite)
			{
					//ustawiamy flagi aa i sto
					LPC_I2C0->I2CONSET = 0x14;
			}
			else
			{
				//ladujemy cos do wyslania
				LPC_I2C0->I2DAT = buforWrite[writeId];
				
				//oznaczamy elementjako wyslany
				writeId++;

				//Ustawiamy bit AA
				LPC_I2C0->I2CONSET = 0x04;
			}
			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
			drawLine(20,20,20,20,LCDBlack);
		break;

		//Wyslano dane, otrzymano notACK. wysylamy stop
		case 0x30:
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//stracono arbitraz podczas wysylania danych lub sla+w. 
		//Transmitujemy start kiedy magistralawolna
		case 0x38:
			//ustawiamy flagi aa i sta
			LPC_I2C0->I2CONSET = 0x24;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po wyslaniu w 0x08 SLA + R otrzymalismy ACK
		case 0x40:
			//jesli czekamy na wiecej niz bajt danych to oznaczamy ack do wyslania (po kolejnym otrzymanym 
			//bajcie), jesli to bedzie ostatni bajt to po nim wyslemy NOT ACK
			if((readId + 1) < maxRead)
				LPC_I2C0->I2CONSET = 0x04; //Idziemy do 0x50
			else
				LPC_I2C0->I2CONCLR = 0x04;	//Idziemy do 0x58

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Otrzymalismy not ACK po wyslaniu SLA + R wiec nalezy przerwac transmisje
		case 0x48:
			//Ustawiamy flagi AA i STO, zeby przerwac transmisje
			LPC_I2C0->I2CONSET = 0x14;

			//Tradycyjne czyszczenie flagi przerwan
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po odebraniu bajtu danych odpowiedzilismy ack - po odebraniu danych czekamy na wiecej
		case 0x50:
			//Odczytujemy otrzymane dane przyslane ze slave'a i zapisujemy do bufora
			buforRead[readId] = LPC_I2C0->I2DAT;

			//zwiekszamy indeks zeby nie nadpisac odebranych danych
			readId++;

			//Jesli kolejny otrzymany bajt jest ostatnim to wyslemy not ack zeby nie przyszlo wiecej. Tak jak w 0x40
			if((readId + 1) < maxRead)
				LPC_I2C0->I2CONSET = 0x04; //Idziemy do 0x50
			else
				LPC_I2C0->I2CONCLR = 0x04;	//Idziemy do 0x58

			//Tradycyjne czyszczenie flagi przerwan
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po odebraniu bajtu danych odpowiedzilismy not ack co znaczy ze nie chcemy wiecej danych
		case 0x58:
			//Odczytujemy otrzymane dane przyslane ze slave'a i zapisujemy do bufora
			buforRead[readId] = LPC_I2C0->I2DAT;

			//zwiekszamy indeks zeby nie nadpisac odebranych danych
			readId++;

			//Ustawiamy flagi AA i STO, zeby przerwac transmisje
			LPC_I2C0->I2CONSET = 0x14;

			//Tradycyjne czyszczenie flagi przerwan
			LPC_I2C0->I2CONCLR = 0x08;
		break;

	}	
	
}

//https://sites.google.com/site/johnkneenmicrocontrollers/18b-i2c/i2c_lpc1768

/*void I2C_enable()
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
	//LPC_I2C0->I2CONSET = 0;
	//LPC_I2C0->I2CONSET |= 1 << 6 | 1 << 2;
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



unsigned int I2C_Start() {
	unsigned int retVal = 0;
 
  I2C_Timeout = 0;

	//--- Issue a start condition ---
	LPC_I2C0->I2CONSET = I2CONSET_STA;	// Set Start flag 
		
	//--- Wait until START transmitted ---
	
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
	LPC_I2C0->I2CONSET = I2CONSET_STO;      // Set Stop flag 
	LPC_I2C0->I2CONCLR = I2CONCLR_SIC;  // Clear SI flag /
						
	//--- Wait for STOP detected ---
	while(LPC_I2C0->I2CONSET & I2CONSET_STO);

  return 1;
}

extern void I2C0_IRQHandler(void)
{
	//drawLine(50,50,100,20);
	LPC_I2C0->I2CONCLR |= 1<<3;
}*/
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
maxWrite = 3;
buforWrite[0] = 0x1D << 1;
buforWrite[1] = 0x28;
buforWrite[2] = 0x07;
I2C_transmit();

	
while(1)
{
	//I2C_Start();
	//I2C_Stop();
	//drawLine(0, 200, 40, 320, LCDBlue);
	//drawLetter(10, 230, 'A', LCDBlack);
	//lcdCleanBackground();
	//drawLetter(150, 50, 'A');
	/* Odczyt rejestru wyswietlacza
	uint16_t zmienna = lcdReadReg(0);
	lcdWriteIndex(zmienna);
	uint16_t zmienna2 = lcdReadData();
	*/
}
}
