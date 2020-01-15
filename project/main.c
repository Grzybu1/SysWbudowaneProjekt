#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"
#include "Board_Buttons.h"
#include "Board_Joystick.h"
#include "GPIO_LPC17xx.h"
#include "stdio.h"
//#include "mbed.h"

volatile uint32_t ticks = 0;	// zmienna do delay
uint32_t randomTick =5000;
volatile unsigned int I2C_Timeout = 0;

int readId = 0;	
int writeId = 0;
//Tu bedziemy dostawac dane
volatile unsigned char buforRead[20];
//Tu nalezy zapisac adres slave'a razem z bitem R/W
volatile unsigned char buforWrite[20];
//Tu wrzucamy ile bajtów danych sie spodziewamy
volatile int maxRead; 
//Tu wrzucamy ile bajtów danych wysylamy
volatile int maxWrite;

short int side = 10;
short int snakeSize = 1;
short int direction = 2;
short int xTab[32] = {0};
short int yTab[20] = {0};
short int headTail[2][2];
char coloredTab[640];
/*
  Direction
    0
  1   2
    3
*/

void SysTick_Handler(void)
{
	++ticks;
	randomTick+=6961;
	if(randomTick > 30001)
		randomTick = randomTick%30000;
}

void delayMS(int ms) //miliseconds
{
	ticks = 0;
	while(ticks < (ms*1000))
		__WFI(); // uspienie procesora
}

/*

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
*/




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

/*
* Jesli chcemy skonfigurowac pojedynczy rejestr to do bufor write wrzucamy kolejno:
* 1) adres sla + w
* 2) adres rejestru
* 3) nowa wartosc rejestru
* Max read ustawiamy na 0. Max write na 3.
*
* Jesli chcemy skonfigurowac kilka rejestrów zapisujemy do bufor write kolejno:
* 1) adres sla + w
* 2) adres rejestru 1
* 3) nowa wartosc rejestru 1
* 4) adres rejestru 2
* 5) nowa wartosc rejestru 2
* 			...
* Max read ustawiamy na 0. max write w zaleznosci od ilosci modyfikowanych rejestrów.
*
* Jesli chcemy odczytac rejestr to do bufor write zapisujemy kolejno:
* 1) adres sla + w
* 2) adres rejestru który chcemy odczytac
* 3) adres sla + r
* Ustawiamy max write na 3, a max read na ilosc bajtów których sie spodziewamy otrzymac w odpowiedzi.
*/
void I2C_transmit()
{
	//informacje ile juz odebralismy.
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

			//zwiekszamy indeks wysylania
			writeId++;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Wyslalismy ponowny sygnal startu,
		//robimy w zasadzie to co przy 0x08. Jesli jestesmy w trybie reciever i wrzucimy 
		//adres z bitem W to przejdziemy do trybu transmiter i na odwrót
		case 0x10:
			//zapisujemy adres do wyslania - UWAGA! Adres musi posiadac na koncu bit W/R
			LPC_I2C0->I2DAT = buforWrite[writeId];

			//zwiekszamy indeks wysylania
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

		//wyslano bajt, otrzymano ack. Jesli wysylany ma byc ostatni bajt to sprawdz czy spodziewamy sie odczytu.
		//Jesli tak to ostatni bajt jest adresem sla + R,
		//wiec powinnismy wyslac najpierw ponowiony warunek start a dopiero pózniej zawartosc ostatniej komórki bufora write.
		case 0x28:
			//jesli kolejny bajt nie bedzie ostatnim to:
			if((writeId + 1) < maxWrite)
			{
				//ladujemy cos do wyslania
				LPC_I2C0->I2DAT = buforWrite[writeId];
				
				//oznaczamy elementjako wyslany
				writeId++;

				//Ustawiamy bit AA
				LPC_I2C0->I2CONSET = 0x04;
			}
			else
			{
				//zostalo cos do odczytu
				if(readId < maxRead)
				{
					//Ustaw flage startu -> przejdzie do stanu 10 zeby wyslac ostatni element bufor write czyli sla + r
					LPC_I2C0->I2CONSET = 1<<5;
				}
				else
				{
					//sprawdzamy czy mamy cos jescze do wyslania. Jesli nie to trzeba ustawic flage stop.
					//Jesli tak to nie jest to adres a raczej ostatni bajt konfiguracyjny przeznaczony do wyslania.
					if(writeId < maxWrite)
					{
						//ladujemy cos do wyslania
						LPC_I2C0->I2DAT = buforWrite[writeId];
				
						//oznaczamy elementjako wyslany
						writeId++;

						//Ustawiamy bit AA
						LPC_I2C0->I2CONSET = 0x04;
					}
					else
					{
						//ustawiamy flagi aa i sto
						LPC_I2C0->I2CONSET = 0x14;
					}
				}
			}
			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
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
// strona 466 dokumentacja

void playGame(void);

void lcdCleanBackground(uint16_t color)
{
	lcdWriteReg(ADRX_RAM,0);
	lcdWriteReg(ADRY_RAM,0);
	lcdWriteIndex(DATA_RAM);
	for(int x = 0; x < 240; x++)
	{
		for(int y = 0; y < 320; y++)
		{
			lcdWriteData(color);
		}
	}
}



void drawLetter(short int offsetX, short int offsetY, char litera, uint16_t color)
{
	unsigned char letter[16];
	GetASCIICode(1, letter, litera);
	for(int y = 0; y < 16; y++)
	{
		for(int x = 7; x >=0; x--)
		{
			//GetASCIICode
			if((letter[y]>>x) & (1))
			{
				lcdWriteReg(ADRX_RAM,offsetY-y);
				lcdWriteReg(ADRY_RAM,-x+offsetX);
				lcdWriteReg(DATA_RAM, color);
		
			}
		}
	}
}

void drawString(short int startX, short int startY, char* str, short int size, uint16_t color)
{
  int row = 0;
  int column = 0;
  for(int i = 0; i < size; i++)
  {
    if((startX + column*10) > 310)
    {
      column = 0;
      row++;
    }
    drawLetter(startX + column*10, startY + row*20, str[i], color);
    column++;
  }
}



void drawLine(short int y_, short int x_, short int length, short int thickness, uint16_t color)
{
	for(int y = x_; y < (x_+thickness); y++)
	{
		for(int x = y_; x < (y_+length); x++)
		{
			lcdWriteReg(ADRX_RAM,x);
			lcdWriteReg(ADRY_RAM,y);
			lcdWriteReg(DATA_RAM,color);
		}
	}
}

void drawCubeFromCenter(short int x, short int y, uint16_t color)
{
  drawLine(y-side/2,x-side/2, side, side, color);
}

void drawGUI(void)
{
  lcdCleanBackground(LCDWhite);
  drawLine(200, 0, 40, 320, LCDGrey);
  drawString(50, 230, "Gra Snake", 9, LCDBlack);


  for(int i = 0; i < 32; i += 1)
    xTab[i] = 4 + side*i;

  for(int i = 0; i < 20; i += 1)
    yTab[i] = 4 + side*i;

  for(int i = 0; i < 640; i += 1)
    coloredTab[i] = '0';

  headTail[0][0] = 16;
  headTail[0][1] = 10;
  headTail[1][0] = 15;
  headTail[1][1] = 10;
  coloredTab[32*10 + 16] = 'S';
  coloredTab[32*10+15] = 'T';
	drawCubeFromCenter(xTab[headTail[0][0]], yTab[headTail[0][1]], LCDRed);
	drawCubeFromCenter(xTab[headTail[1][0]], yTab[headTail[1][1]], LCDBlack);
  snakeSize = 1;

  
}

void gameOver(void)
{
  lcdCleanBackground(LCDWhite);
  drawLine(200, 0, 40, 320, LCDGrey);
  drawString(50, 230, "Gra Snake", 9, LCDBlack);

  drawString(50, 150, "Przegrana", 9, LCDRed);


  delayMS(3000);
  //drawGUI();  
}


void generateFood()
{
  short int generated = 0;
  while(generated == 0)
  {
    int indexX = ((float)randomTick/30001)*32;
		int indexY = ((float)randomTick/30001)*20;
		
    if((coloredTab[32*indexY+indexX] != 'S') && (coloredTab[32*indexY+indexX] != 'T'))
    {
      coloredTab[32*indexY+indexX] = 'F';
      drawCubeFromCenter(indexX, indexY, LCDGreen);
      generated = 1;
    }
  }
}

short int updatePosition()
{
	
  drawCubeFromCenter(xTab[headTail[0][0]], yTab[headTail[0][1]], LCDBlack);
  switch (direction)
  {
    case 0:
      headTail[0][1] += 1;
        if(headTail[0][1] > 20)
          return 0;
    break;
    case 1:
        headTail[0][0] -= 1;
        if(headTail[0][0] < 0)
          return 0;
      break;
    case 2:
        headTail[0][0] += 1;
        if(headTail[0][0] > 32)
          return 0;
      break;
    case 3:
        headTail[0][1] -= 1;
        if(headTail[0][1] < 0)
          return 0;
      break;
      default: break;
  }

  char headChar = coloredTab[32 * headTail[0][1] + headTail[0][0]];
  if(headChar == 'S') // uderzenie w weza
    return 0;
  else if(headChar == 'F') // zdobycie punktu
  {
    snakeSize++;
    coloredTab[32 * headTail[0][1] + headTail[0][0]] = 'S';
    drawCubeFromCenter(xTab[headTail[0][0]], yTab[headTail[0][1]], LCDRed);
    generateFood();
  }
  else
  {
    drawCubeFromCenter(xTab[headTail[1][0]], yTab[headTail[1][1]], LCDBlueSea);
    coloredTab[32 * headTail[1][1] + headTail[1][0]] = '0';
    switch (direction)
    {
      case 0:
        headTail[1][1] += 1;
      break;
      case 1:
          headTail[1][0] -= 1;
        break;
      case 2:
          headTail[1][0] += 1;
        break;
      case 3:
          headTail[1][1] -= 1;
        break;
        default: break;
    }
		coloredTab[32 * headTail[1][1] + headTail[1][0]] = 'T';
		drawCubeFromCenter(xTab[headTail[0][0]], yTab[headTail[0][1]], LCDRed);
    coloredTab[32 * headTail[0][1] + headTail[0][0]] = 'S';
   
  }
  

  return 1;
}

void updateDirection(void)
{
  
  if(Joystick_GetState() == JOYSTICK_UP)
    direction = 0;
  if(Joystick_GetState() == JOYSTICK_LEFT)
    direction = 1;
  if(Joystick_GetState() == JOYSTICK_RIGHT)
    direction = 2;
  if(Joystick_GetState() == JOYSTICK_DOWN)
    direction = 3;
}

void playGame(void)
{
	
  drawString(200, 230, "Start", 5, LCDBlack);
  short int running  = 1;
	generateFood();
  while(running == 1)
  {
    updateDirection();
    delayMS(100);
    running = updatePosition();
  }
  gameOver();

}

void accelerometr_enable()
{
	maxWrite = 8;
	buforWrite[0] = 0x1D << 1;
	buforWrite[1] = 0x2A;
	buforWrite[3] = 0x06;
	buforWrite[4] = 0x2E;
	buforWrite[5] = 1<<7 | 1<<6;
	buforWrite[6] = 0x1D;
	buforWrite[7] = 0x95;
	maxRead = 0;
	I2C_transmit();
}

int main(void)
{
	Buttons_Initialize();
	Joystick_Initialize();
	SysTick_Config(SystemCoreClock / 1000 / 1000);
	lcdConfiguration();
	init_ILI9325();
	lcdCleanBackground(LCDWhite);
	
	/*maxWrite = 3;
	buforWrite[0] = 0x1D << 1;
	buforWrite[1] = 0x32;
	buforWrite[2] = (0x1D << 1) | 1;
	maxRead = 1;
	I2C_transmit();*/
	
	//drawGUI();
	accelerometr_enable();
	char toDraw[50] = {'_'};
while(1)
{
	lcdCleanBackground(LCDWhite);
	for(int i = 0; i < 50; i++)
		toDraw[i] = '_';
	maxWrite = 3;
	buforWrite[0] = 0x1D << 1;
	buforWrite[1] = 0x2E;
	buforWrite[2] = (0x1D << 1) | 1;
	maxRead = 1;
	I2C_transmit();
	//drawGUI();
	sprintf(toDraw, "%d", buforRead[0]);
	drawString(50,50,toDraw,50,LCDBlack);
	delayMS(100);
	
	/*if(Joystick_GetState() == JOYSTICK_CENTER)
  {
		drawGUI();
    playGame();
  }*/
  //drawLine(0, 0, 10, 320, LCDGrey);
	//drawLetter(50, 50, 'K', LCDBlack);
	
}

}
