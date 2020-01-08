

//Tu będziemy dostawać dane
volatile unsigned char buforRead[20];

//Tu należy zapisać adres slave'a razem z bitem R/W
volatile unsigned char buforWrite[20];

//Tu wrzucamy ile bajtów danych się spodziewamy
volatile int maxRead; 

//Tu wrzucamy ile bajtów danych wysyłamy
volatile int maxWrite;

void I2C_enable()
{
	//DOMYŚLNE - włączenie zegara dla I2C
	//LPC_SC->PCONP |= 1<<7;


	//Ustawienie zegara pclk domyślne na cclk/4
	//PCLKSEL0 = 0; 


	//Konfiguracja pinów na SDA0 i SCL0(s.129 dokumentacji)
	LPC_PINCON->PINSEL1 |= ((0x01<<22)|(0x01<<24)); 


	//Domyślne - ustawienie pinów w tryb normalny
	//LPC_PINCON->I2CPADCFG |= 0<<0 | 0<<1;

	//Czyścimy wszystkie flagi
	LPC_I2C0 -> I2CONCLR = 0;

	//Włączenie przerwań w nvic
	NVIC_EnableIRQ(I2C0_IRQn);

	//Ustawienie flagi EN żeby ustawić się w tryb master
	LPC_I2C0->I2CONSET = 1<<6;
}

void I2C_transmit()
{

	//Ustawienie bitu STA -> ustawienie flagi startu
	LPC_I2C0->I2CONSET = 1<<5;
	//Po ustawieniu flagi (teoretycznie) uruchamia się przerwanie z kodem 0x08 w I2STAT

	//Czyścimy flagę startu tak na wszelki wypadek
	LPC_I2C0->I2CONCLR = 1<<5;
}


//Manual strony 468 i 477+
extern void I2C0_IRQHandler(void)
{
	//informacje ile już odebraliśmy.
	int readId = 0;
	
	//informacje ile już wysłaliśmy.
	int writeId = 0;
	
	//W tej zmiennej mamy aktualny status transmisji który należy obsłużyć.
	unsigned char status = LPC_I2C0->I2STAT;

	switch(status)
	{
		//Wysłano pojedynczy sygnał startu i otrzymano ACK. Po tym idziemy do 0x40 lub 0x48
		case 0x08:
			//zapisujemy adres do wysłania - UWAGA! Adres musi posiadać na końcu bit W/R
			LPC_I2C0->I2DAT = slaveAdr;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Wysłaliśmy ponowny sygnał startu,
		//robimy w zasadzie to co przy 0x08 bo nie chcemy przechodzić do trybu transmisji
		case 0x10:
			//zapisujemy adres do wysłania - UWAGA! Adres musi posiadać na końcu bit W/R(w zależności od niego przechodzimy do reciever/transmiter)
			LPC_I2C0->I2DAT = slaveAdr;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//wysłano sla+w, otrzymano ACK. Wyślemy 1 bajt.
		case 0x18:
			//ładujemy coś do wysłania
			LPC_I2C0->I2DAT = buforWrite[writeId];
			
			//oznaczamy elementjako wysłany
			writeId++;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;

		break;

		//Wysłano SLA + w, otrzymano not ACK. Wysyłamy stop
		case 0x20:
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//wysłano bajt, otrzymano ack. Jeśli jestcoś do wysłania to wyśli jeśli nie to stop
		case 0x28:
			if((writeId) < maxWrite)
			{
				//ustawiamy flagi aa i sto
				LPC_I2C0->I2CONSET = 0x14;
			}
			else
			{
				//ładujemy coś do wysłania
				LPC_I2C0->I2DAT = buforWrite[writeId];
				
				//oznaczamy elementjako wysłany
				writeId++;

				//Ustawiamy bit AA
				LPC_I2C0->I2CONSET = 0x04;
			}
			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Wysłano dane, otrzymano notACK. wysyłamy stop
		case 0x30:
			//ustawiamy flagi aa i sto
			LPC_I2C0->I2CONSET = 0x14;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//stracono arbitraż podczas wysyłania danych lub sla+w. 
		//Transmitujemy start kiedy magistralawolna
		case 0x38:
			//ustawiamy flagi aa i sta
			LPC_I2C0->I2CONSET = 0x24;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po wysłaniu w 0x08 SLA + R otrzymaliśmy ACK
		case 0x40:

			//jeśli czekamy na więcej niż bajt danych to oznaczamy ack do wysłania (po kolejnym otrzymanym 
			//bajcie), jeśli to będzie ostatni bajt to po nim wyślemy NOT ACK
			if((readId + 1) < maxRead)
				LPC_I2C0->I2CONSET = 0x04; //Idziemy do 0x50
			else
				LPC_I2C0->I2CONCLR = 0x04;	//Idziemy do 0x58

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Otrzymaliśmy not ACK po wysłaniu SLA + R więc należy przerwać transmisję
		case 0x48:
			//Ustawiamy flagi AA i STO, żeby przerwać transmisję
			LPC_I2C0->I2CONSET = 0x14;

			//Tradycyjne czyszczenie flagi przerwań
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po odebraniu bajtu danych odpowiedziliśmy ack - po odebraniu danych czekamy na więcej
		case 0x50:
			//Odczytujemy otrzymane dane przysłane ze slave'a i zapisujemy do bufora
			buforRead[readId] = LPC_I2C0->I2DAT;

			//zwiększamy indeks żeby nie nadpisać odebranych danych
			readId++;

			//Jeśli kolejny otrzymany bajt jest ostatnim to wyślemy not ack żeby nie przyszło więcej. Tak jak w 0x40
			if((readId + 1) < maxRead)
				LPC_I2C0->I2CONSET = 0x04; //Idziemy do 0x50
			else
				LPC_I2C0->I2CONCLR = 0x04;	//Idziemy do 0x58

			//Tradycyjne czyszczenie flagi przerwań
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Po odebraniu bajtu danych odpowiedziliśmy not ack co znaczy że nie chcemy więcej danych
		case 0x58:
			//Odczytujemy otrzymane dane przysłane ze slave'a i zapisujemy do bufora
			buforRead[readId] = LPC_I2C0->I2DAT;

			//zwiększamy indeks żeby nie nadpisać odebranych danych
			readId++;

			//Ustawiamy flagi AA i STO, żeby przerwać transmisję
			LPC_I2C0->I2CONSET = 0x14;

			//Tradycyjne czyszczenie flagi przerwań
			LPC_I2C0->I2CONCLR = 0x08;
		break;

	}	
	
}