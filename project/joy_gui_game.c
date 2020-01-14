
#include "Board_Buttons.h"
#include "Board_Joystick.h"
#include "GPIO_LPC17xx.h"
#include "mbed.h"


short int side = 10;
short int snakeSize = 1;
short int direction = 0;
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

void playGame();

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
  drawLine(x-side/2, y-side/2, side, side, color);
}

void drawGUI(void)
{
  lcdCleanBackground(LCDWhite);
  drawLine(200, 0, 320, 40, LCDGrey);
  drawString(230, 50, "Gra Snake", 9, LCDBlack);


  for(int i = 0; i < 32; i += 1)
    xTab[i] = 4 + side*i;

  for(int i = 0; i < 24; i += 1)
    yTab[i] = 4 + side*i;

  for(int i = 0; i < 640; i += 1)
    coloredTab[i] = '0';

  headTail[0][0] = 16;
  headTail[0][1] = 12;
  headTail[1][0] = 16;
  headTail[1][1] = 12;
  coloredTab[16*12+16] = 'S';
  snakeSize = 1;

  while(((LPC_GPIO2->FIOPIN>>16) & 0x01) != 1)
  {
    playGame();
  }
}

void gameOver(void)
{
  lcdCleanBackground(LCDWhite);
  drawLine(200, 0, 320, 40, LCDGrey);
  drawString(230, 50, "Gra Snake", 9, LCDBlack);

  drawString(150, 50, "Przegrana", 9, LCDRed);


  delayMs(3000);
  drawGUI();  
}


void generateFood()
{
  short int generated = 0;
  while(generated == 0)
  {
    short int randomNumer =((float)rand()/RAND_MAX) * 640;
    if(coloredTab[randomNumer] != 'S')
    {
      coloredTab[randomNumer] = 'F';
      drawCubeFromCenter(randomNumer/20, randomNumer/32, LCDGreen);
      generated = 1;
    }
  }
}

short int updatePosition()
{
  switch (direction)
  {
    case 0:
      headTail[0][1] -= 1;
        if(headTail[0][1] < 0)
          return 1;
    break;
    case 1:
        headTail[0][0] -= 1;
        if(headTail[0][0] < 0)
          return 1;
      break;
    case 2:
        headTail[0][0] += 1;
        if(headTail[0][0] > 32)
          return 1;
      break;
    case 3:
        headTail[0][1] += 1;
        if(headTail[0][1] > 20)
          return 1;
      break;
      default: break;
  }

  char headChar = coloredTab[headTail[0][0] * headTail[0][1]];
  if(headChar == 'S') // uderzenie w weza
    return 1;
  else if(headChar == 'F') // zdobycie punktu
  {
    snakeSize++;
    coloredTab[headTail[0][0] * headTail[0][1]] = 'S';
    drawCubeFromCenter(xTab[headTail[0][0]], yTab[headChar[0][1]], LCDBlack);
    generateFood();
  }
  else
  {
    drawCubeFromCenter(xTab[headTail[1][0]], yTab[headChar[1][1]], LCDWhite);
    coloredTab[headTail[0][0] * headTail[0][1]] = '0';
    switch (direction)
    {
      case 0:
        headTail[1][1] -= 1;
      break;
      case 1:
          headTail[1][0] -= 1;
        break;
      case 2:
          headTail[1][0] += 1;
        break;
      case 3:
          headTail[1][1] += 1;
        break;
        default: break;
    }
  }
  

  return 0;
}

void updateDirection(void)
{
  
  if((LPC_GPIO2->FIOPIN >>23) && 0x01)
    direction = 0;
  if((LPC_GPIO2->FIOPIN >>15) && 0x01)
    direction = 1;
  if((LPC_GPIO2->FIOPIN >>24) && 0x01)
    direction = 2;
  if((LPC_GPIO2->FIOPIN >>17) && 0x01)
    direction = 3;
}

void playGame(void)
{
  short int running  = 1;
  while(running == 1)
  {
    updateDirection();
    delayMs(500);
    running = updatePosition();
  }
  gameOver();

}

int main(){
  Buttons_Initialize();
}