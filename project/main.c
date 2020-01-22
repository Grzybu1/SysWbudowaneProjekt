#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"
#include "Board_Buttons.h"
#include "Board_Joystick.h"
#include "GPIO_LPC17xx.h"
#include "stdio.h"
//#include "mbed.h"

int debug = 0;
volatile uint32_t ticks = 0;	// zmienna do delay
uint32_t randomTick = 2124;
double alfaRandom = 1.02435;
volatile unsigned int I2C_Timeout = 0;

int readId = 0;	
int writeId = 0;
//Tu bedziemy dostawac dane
volatile uint16_t buforRead[20];
//Tu nalezy zapisac adres slave'a razem z bitem R/W
volatile uint16_t buforWrite[20];
//Tu wrzucamy ile bajt�w danych sie spodziewamy
volatile int maxRead; 
//Tu wrzucamy ile bajt�w danych wysylamy
volatile int maxWrite;

short int side = 10;
uint16_t snakeSize = 1;
uint8_t bestScores[20] = {0};
uint16_t bestScoresSize = 20;

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
uint8_t tailDirection[1000] = {2};
short int direction = 2;
short int currentTail = 0;
void I2C_transmit();
void accelerometr_read()
{
	//Zakładam że przy wielobajtowym odczycie akcelerometr przesyła bajty z kolejno występujących po sobie rejestrach. 
	//Jeśli nie zadziała to trzeb to rozbić na  osobne transmisje.
	maxRead = 4;
	maxWrite = 3;
	for (int i = 0; i < 4; ++i)
	{
		buforRead[i] = 0;
	}
	buforWrite[0] = 0x3A; //3A adres urządzenia z bitem write/read -> 0
	buforWrite[1] = 0x32;	//Adres rejestru za wartością odczytaną X(1 z 2 bajtów)
	buforWrite[2] = 0x3B; //adres urządzenia (0x1D) z bitem write/read -> 1
	I2C_transmit();
	delayMS(5);
	randomTick += buforRead[0] + buforRead[1] + buforRead[2] + buforRead[3];
	randomTick *= alfaRandom;
	if(randomTick > 10000){
		randomTick = buforRead[0] + buforRead[1] + buforRead[2] + buforRead[3];
		randomTick *= alfaRandom;
	
	}
	//w bufor read mamy kolejno - mniej znaczący bajt X, bardziej znaczący bajt X,
	//mniej znaczący bajt Y, bardziej znaczący bajt Y
}


void configureUART0()
{
	LPC_UART0->LCR = 0x83;
	LPC_UART0->DLL = 13;
	LPC_UART0->DLM = 0;
	LPC_UART0->FDR = 0xF1;
	LPC_UART0->LCR = 0x03;

	PIN_Configure(0, 2, 1, 0, 0); 
	PIN_Configure(0, 3, 1, 0, 0); 
}


void sendString(char* str, short int length)
{
	for(int i = 0; i < length; i++)
	{
		/*THRE is set immediately upon detection of an empty UARTn THR and is cleared on a UnTHR write.
1
0 UnTHR contains valid data.
1 UnTHR is empty.*/
		while(!(LPC_UART0->LSR & 1 << 5));
		LPC_UART0->THR = str[i];
	}
}

void sendIntString(int num)
{
	char tmp[32] = {" "};
	sprintf(tmp, "%d", num);
	sendString(tmp, 32);
}


void SysTick_Handler(void)
{
	++ticks;
	/*if((ticks % 1000) == 0)
	{
		for(int i = 0; i < randomTick%20; i++){
			randomTick = (int)((double)randomTick * alfaRandom);
		}
		if(randomTick > 10000)
			randomTick = randomTick/(alfaRandom*0.0000002);
	}*/
}

void delayMS(int ms) //miliseconds
{
	ticks = 0;
	while(ticks < (ms*1000))
		__WFI(); // uspienie procesora
}

void I2C_enable()
{
	//DOMYSLNE - wlaczenie zegara dla I2C
	//LPC_SC->PCONP |= 1<<7;


	//Ustawienie zegara pclk domyslne na cclk/4
	//PCLKSEL0 = 0; 


	//Konfiguracja pin�w na SDA0 i SCL0(s.129 dokumentacji)
	LPC_PINCON->PINSEL1 |= ((0x01<<22)|(0x01<<24)); 


	//Domyslne - ustawienie pin�w w tryb normalny
	//LPC_PINCON->I2CPADCFG |= 0<<0 | 0<<1;

	//Czyscimy wszystkie flagi
	LPC_I2C0 -> I2CONCLR = 0;

	//Wlaczenie przerwan w nvic
	NVIC_EnableIRQ(I2C0_IRQn);

	//Ustawienie flagi EN zeby ustawic sie w tryb master
	LPC_I2C0->I2CONSET = 1<<6;
}

/*
* Jesli chcemy skonfigurowac rejestr to do bufor write wrzucamy kolejno:
* 1) adres sla + w
* 2) adres rejestru
* 3) nowa wartosc rejestru
* Max read ustawiamy na 0. Max write na 3.
*
*
* Jesli chcemy odczytac rejestr to do bufor write zapisujemy kolejno:
* 1) adres sla + w
* 2) adres rejestru kt�ry chcemy odczytac
* 3) adres sla + r
* Ustawiamy max write na 3, a max read na ilosc bajt�w kt�rych sie spodziewamy otrzymac w odpowiedzi.
*/
void I2C_transmit()
{
	//informacje ile juz odebralismy.
	readId = 0;
	writeId = 0;
	if(debug == 1) sendString("transmit ", 9);
	//Ustawienie bitu STA -> ustawienie flagi startu
	LPC_I2C0->I2CONSET = 1<<5;
	//Po ustawieniu flagi (teoretycznie) uruchamia sie przerwanie z kodem 0x08 w I2STAT

	//Czyscimy flage startu tak na wszelki wypadek
	LPC_I2C0->I2CONCLR = 1<<5;
	
	
}


//Manual strony 468 i 477+
extern void I2C0_IRQHandler(void)
{
	//W tej zmiennej mamy aktualny status transmisji kt�ry nalezy obsluzyc.
	unsigned char status = LPC_I2C0->I2STAT;
	switch(status)
	{
		//Wyslano pojedynczy sygnal startu i otrzymano ACK. Po tym idziemy do 0x40 lub 0x48(odczyt), 0x18 lub 0x20(zapis)
		case 0x08:
			if(debug == 1) sendString("start ", 6);
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
		//adres z bitem W to przejdziemy do trybu transmiter i na odwr�t
		case 0x10:
			if(debug == 1) sendString("repeated start ", 15);
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
			if(debug == 1) sendString("slaw -> ack ", 12);
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
			if(debug == 1) sendString("slaw -> notack ", 15);
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//wyslano bajt, otrzymano ack. Jesli wysylany ma byc ostatni bajt to sprawdz czy spodziewamy sie odczytu.
		//Jesli tak to ostatni bajt jest adresem sla + R,
		//wiec powinnismy wyslac najpierw ponowiony warunek start a dopiero p�zniej zawartosc ostatniej kom�rki bufora write.
		case 0x28:
			if(debug == 1) sendString("wyslano bajt -> ack -> ", 23);
			//jesli kolejny bajt nie bedzie ostatnim to:
			if((writeId + 1) < maxWrite)
			{
				if(debug == 1) sendString("wysyłam nie ostatni bajt ", 27);
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
					if(debug == 1) sendString("wracam do odczytu ", 18);
					//Ustaw flage startu -> przejdzie do stanu 10 zeby wyslac ostatni element bufor write czyli sla + r
					LPC_I2C0->I2CONSET = 1<<5;
				}
				else
				{
					//sprawdzamy czy mamy cos jescze do wyslania. Jesli nie to trzeba ustawic flage stop.
					//Jesli tak to nie jest to adres a raczej ostatni bajt konfiguracyjny przeznaczony do wyslania.
					if(writeId < maxWrite)
					{
						if(debug == 1) sendString("wysylam ostatni ", 16);
						//ladujemy cos do wyslania
						LPC_I2C0->I2DAT = buforWrite[writeId];
				
						//oznaczamy elementjako wyslany
						writeId++;

						//Ustawiamy bit AA
						LPC_I2C0->I2CONSET = 0x04;
					}
					else
					{
						if(debug == 1) sendString("koniec", 6);
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
			if(debug == 1) sendString("wyslano bajt -> notack ", 23);
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//stracono arbitraz podczas wysylania danych lub sla+w. 
		//Transmitujemy start kiedy magistralawolna
		case 0x38:
			if(debug == 1) sendString("stracono arbitraz ", 18);
			//ustawiamy flagi aa i sta
			LPC_I2C0->I2CONSET = 0x24;

			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po wyslaniu w 0x08 SLA + R otrzymalismy ACK
		case 0x40:
			if(debug == 1) sendString("slar -> ack ", 12);
			//jesli czekamy na wiecej niz bajt danych to oznaczamy ack do wyslania (po kolejnym otrzymanym 
			//bajcie), jesli to bedzie ostatni bajt to po nim wyslemy NOT ACK
			if((readId + 1) < maxRead)
				LPC_I2C0->I2CONSET = 0x04; //Idziemy do 0x50
			else
				LPC_I2C0->I2CONCLR = 0x04;	//Idziemy do 0x58
	
			LPC_I2C0->I2CONCLR = 1<<5;
			//Czyscimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Otrzymalismy not ACK po wyslaniu SLA + R wiec nalezy przerwac transmisje
		case 0x48:
			if(debug == 1) sendString("slar -> notack ", 15);
			//Ustawiamy flagi AA i STO, zeby przerwac transmisje
			LPC_I2C0->I2CONSET = 0x14;

			//Tradycyjne czyszczenie flagi przerwan
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po odebraniu bajtu danych odpowiedzilismy ack - po odebraniu danych czekamy na wiecej
		case 0x50:
			if(debug == 1) sendString("dostano dane -> ack ", 20);
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
			if(debug == 1) sendString("odebrano -> notack ", 19);
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



void bubbleSort(int *tab, int size)
{
	for (int i = 0; i < size-1; i++) {
		for (int j = 0; j < size-i-1; j++) {
			if (tab[j] < tab[j+1]) { 
				int temp = tab[j]; 
				tab[j] = tab[j+1]; 
				tab[j+1] = temp; 
			} 
		}
	}
}

void readBestScores(){
	for(int i = 0; i < 20; i++){ 
		if(i < 4)
			bestScores[i] = ( LPC_RTC->GPREG0>>(i*8) ) & 0xFF;
		else if(i < 8)
			bestScores[i] = ( LPC_RTC->GPREG0>>((i%4)*8) ) & 0xFF;
		else if(i < 16)
			bestScores[i] = ( LPC_RTC->GPREG0>>((i%4)*8) ) & 0xFF;
		else if(i < 24)
			bestScores[i] = ( LPC_RTC->GPREG0>>((i%4)*8) ) & 0xFF;
		else if(i < 32)
			bestScores[i] = ( LPC_RTC->GPREG0>>((i%4)*8) ) & 0xFF;
	}
	bubbleSort(bestScores, bestScoresSize);
}

void writeBestScores(){
	bubbleSort(bestScores, bestScoresSize);
	if(bestScores[bestScoresSize-1] < snakeSize){
		bestScores[bestScoresSize] = snakeSize;
		bubbleSort(bestScores, bestScoresSize);
	}
	LPC_RTC->GPREG0 = LPC_RTC->GPREG1 = LPC_RTC->GPREG2 =LPC_RTC->GPREG3 =LPC_RTC->GPREG4 = 0;
	for(int i = 0; i < 20; i++){ 
		if(i < 4)
			LPC_RTC->GPREG0 |= bestScores[i]<<(i*8);
		else if(i < 8)
			LPC_RTC->GPREG1 |= bestScores[i]<<((i%4)*8);
		else if(i < 16)
			LPC_RTC->GPREG2 |= bestScores[i]<<((i%4)*8);
		else if(i < 24)
			LPC_RTC->GPREG3 |= bestScores[i]<<((i%4)*8);
		else if(i < 32)
			LPC_RTC->GPREG4 |= bestScores[i]<<((i%4)*8);
	}

}

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

void printScoresBoard(){
  drawLine(20, 110, 160, 100, LCDCyan);
  drawString(120, 170, "Wynik", 5, LCDRed);
	char wynikStr[3] = {" "};
	sprintf(wynikStr, "%d", snakeSize);
	drawString(140, 170, wynikStr, 3, LCDRed);
	
	for(int i = 0; i < 10; i++){
		char wynikStr[3] = {" "};
		char liczba[2] = {" "};
		sprintf(liczba, "%d", i+1);
		sprintf(wynikStr, "%d", bestScores[i]);
		drawString(120, 170-i*18, liczba, 2, LCDBlack);
		drawString(140, 170-i*18, wynikStr, 2, LCDBlack);
	}
	while(Joystick_GetState() != JOYSTICK_CENTER){
	}

}

void gameOver(void)
{
	if(snakeSize > bestScores[bestScoresSize-1])
		writeBestScores();
	printScoresBoard();
	snakeSize = 1;
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
    int indexX = ((float)randomTick/10000.0)*32.;
		int indexY = ((float)randomTick/10000.)*20.;
		/*sendIntString(indexX);
		sendIntString(indexY);
		sendIntString(randomTick);*/
    if((coloredTab[32*indexY+indexX] != 'S') && (coloredTab[32*indexY+indexX] != 'T') && (coloredTab[32*indexY+indexX] != 'F'))
    {
      coloredTab[32*indexY+indexX] = 'F';
      drawCubeFromCenter(xTab[indexX], yTab[indexY], LCDGreen);
      generated = 1;
    }
  }
}

short int updatePosition()
{
	
  drawCubeFromCenter(xTab[headTail[0][0]], yTab[headTail[0][1]], LCDBlack);
	int nextTailDirection_index = currentTail+1;
	if(nextTailDirection_index >= 1000)
	{
		nextTailDirection_index = 0;
	}
	tailDirection[nextTailDirection_index] = direction;
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
    drawCubeFromCenter(xTab[headTail[1][0]], yTab[headTail[1][1]], LCDWhite);
    coloredTab[32 * headTail[1][1] + headTail[1][0]] = '0';
    switch (tailDirection[currentTail])
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
		
		currentTail += 1;
		if(currentTail >= 1000)
		{
			currentTail = 0;
		}
   
  }
  

  return 1;
}

void updateDirection(void)
{
  accelerometr_read();
	sendIntString(buforRead[0] + buforRead[1]);
	sendString("  ",2);
	sendIntString(buforRead[2] + buforRead[3]);
	sendString("\n",2);
	delayMS(5);
	int left = 0;
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

void accelerometr_enable() //0x3A dla zapisu i 0x3B dla odczytu
{
	I2C_enable();
	maxRead = 0;
	maxWrite = 3;
	buforWrite[0] = 0x3A; //3A adres urządzenia z bitem write/read -> 0
	buforWrite[1] = 0x24; //0x24 - adres rejestru z progami, których przekroczenie wywoła przerwanie
	buforWrite[2] = 0x04; //250 mg (wrzucamy liczbę. 62.5 * wrzucona liczba to próg)
	I2C_transmit();
	delayMS(10);
	
	maxRead = 0;
	maxWrite = 3;
	buforWrite[0] = 0x3A; //3A adres urządzenia z bitem write/read -> 0
	buforWrite[1] = 0x27; // adres rejestru w którym sa dane jakie zdarzenia mają powodować przerwanie
	buforWrite[2] = 0x60; // Włączenie przerwań na płaszczyznach x oraz y
	I2C_transmit();
	delayMS(10);
	
	maxRead = 0;
	maxWrite = 3;
	buforWrite[0] = 0x3A; //3Aadres urządzenia z bitem write/read -> 0
	buforWrite[1] = 0x2E; // adres rejestru w którym uruchamiamy przerwania
	buforWrite[2] = 0x10; // Włączenie przerwań pojedynczych
	I2C_transmit();
	delayMS(10);
	
	maxRead = 0;
	maxWrite = 3;
	buforWrite[0] = 0x3A; //3Aadres urządzenia z bitem write/read -> 0
	buforWrite[1] = 0x2D; // adres rejestru w którym uruchamiamy przerwania
	buforWrite[2] = 0x08; // Włączenie przerwań pojedynczych
	I2C_transmit();
	delayMS(10);
}


void RTC_IRQHandler(void)
{
	// if(isTick)
	// {
	// 	isTick = 0;
	// 	if(debug == 1) sendString("Tick ", 5);
	// }
	// else
	// {
	// 	isTick = 1;
	// 	if(debug == 1) sendString("Tack ", 5);
	// }
	LPC_RTC->ILR = 1;

}

void configureRTC()
{
		// wlaczenie RTC jako zrodla przerwania	
	LPC_RTC->CCR = 1;
	LPC_RTC->ILR = 1;
	LPC_RTC->CIIR = 1;
	NVIC_EnableIRQ(RTC_IRQn);
}



int main(void)
{
	configureUART0();
	configureRTC();
	Buttons_Initialize();
	Joystick_Initialize();
	SysTick_Config(SystemCoreClock / 1000 / 1000);
	lcdConfiguration();
	init_ILI9325();
	lcdCleanBackground(LCDWhite);

	writeBestScores(); // to trzeba uruchomic tylko raz dla danej plytki
	
	/*maxWrite = 3;
	buforWrite[0] = 0x1D << 1;
	buforWrite[1] = 0x32;
	buforWrite[2] = (0x1D << 1) | 1;
	maxRead = 1;
	I2C_transmit();*/
	
	//drawGUI();
	accelerometr_enable();
	readBestScores(); // funkcja zapisujaca najlepsze wyniki gry do tablicy bestScores
	char toDraw[50] = {'_'};
	if(debug == 1) sendString("\n", 2);
while(1)
{
	lcdCleanBackground(LCDWhite);
	for(int i = 0; i < 50; i++)
		toDraw[i] = '_';
	
	if(Joystick_GetState() == JOYSTICK_CENTER)
  {
		drawGUI();
    playGame();
  }
	/*accelerometr_read();
	sendIntString(buforRead[0]);
	sendString("  ",2);
	sendIntString(buforRead[1]);
		sendString("  ",2);
	sendIntString(buforRead[2]);
		sendString("  ",2);
	sendIntString(buforRead[3]);
		sendString("\n",2);
	delayMS(100);*/

	
}

}
