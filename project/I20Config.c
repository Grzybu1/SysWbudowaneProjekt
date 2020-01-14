

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

/*
* Jeśli chcemy skonfigurować pojedynczy rejestr to do bufor write wrzucamy kolejno:
* 1) adres sla + w
* 2) adres rejestru
* 3) nowa wartość rejestru
* Max read ustawiamy na 0. Max write na 3.
*
* Jeśli chcemy skonfigurować kilka rejestrów zapisujemy do bufor write kolejno:
* 1) adres sla + w
* 2) adres rejestru 1
* 3) nowa wartość rejestru 1
* 4) adres rejestru 2
* 5) nowa wartość rejestru 2
* 			...
* Max read ustawiamy na 0. max write w zależności od ilości modyfikowanych rejestrów.
*
* Jeśli chcemy odczytać rejestr to do bufor write zapisujemy kolejno:
* 1) adres sla + w
* 2) adres rejestru który chcemy odczytać
* 3) adres sla + r
* Ustawiamy max write na 3, a max read na ilość bajtów których się spodziewamy otrzymać w odpowiedzi.
*/
void I2C_transmit()
{
	//informacje ile już odebraliśmy.
	int readId = 0;
	int writeId = 0;

	//Ustawienie bitu STA -> ustawienie flagi startu
	LPC_I2C0->I2CONSET = 1<<5;
	//Po ustawieniu flagi (teoretycznie) uruchamia się przerwanie z kodem 0x08 w I2STAT

	//Czyścimy flagę startu tak na wszelki wypadek
	LPC_I2C0->I2CONCLR = 1<<5;
}


//Manual strony 468 i 477+
extern void I2C0_IRQHandler(void)
{
	//W tej zmiennej mamy aktualny status transmisji który należy obsłużyć.
	unsigned char status = LPC_I2C0->I2STAT;

	switch(status)
	{
		//Wysłano pojedynczy sygnał startu i otrzymano ACK. Po tym idziemy do 0x40 lub 0x48
		case 0x08:
			//zapisujemy adres do wysłania - UWAGA! Adres musi posiadać na końcu bit W/R
			LPC_I2C0->I2DAT = buforWrite[writeId];

			//zwiększamy indeks wysyłania
			writeId++;

			//Ustawiamy bit AA
			LPC_I2C0->I2CONSET = 0x04;

			//Czyścimy bit SI
			LPC_I2C0->I2CONCLR = 0x08;
		break;

		//Wysłaliśmy ponowny sygnał startu,
		//robimy w zasadzie to co przy 0x08. Jeśli jesteśmy w trybie reciever i wrzucimy 
		//adres z bitem W to przejdziemy do trybu transmiter i na odwrót
		case 0x10:
			//zapisujemy adres do wysłania - UWAGA! Adres musi posiadać na końcu bit W/R
			LPC_I2C0->I2DAT = buforWrite[writeId];

			//zwiększamy indeks wysyłania
			writeId++;

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

		//wysłano bajt, otrzymano ack. Jeśli wysyłany ma być ostatni bajt to sprawdź czy spodziewamy się odczytu.
		//Jeśli tak to ostatni bajt jest adresem sla + R,
		//więc powinniśmy wysłać najpierw ponowiony warunek start a dopiero później zawartość ostatniej komórki bufora write.
		case 0x28:
			//jeśli kolejny bajt nie będzie ostatnim to:
			if((writeId + 1) < maxWrite)
			{
				//ładujemy coś do wysłania
				LPC_I2C0->I2DAT = buforWrite[writeId];
				
				//oznaczamy elementjako wysłany
				writeId++;

				//Ustawiamy bit AA
				LPC_I2C0->I2CONSET = 0x04;
			}
			else
			{
				//zostało coś do odczytu
				if(readId < maxRead)
				{
					//Ustaw flagę startu -> przejdzie do stanu 10 żeby wysłać ostatni element bufor write czyli sla + r
					LPC_I2C0->I2CONSET = 1<<5;
				}
				else
				{
					//sprawdzamy czy mamy coś jescze do wysłania. Jeśli nie to trzeba ustawić flagę stop.
					//Jeśli tak to nie jest to adres a raczej ostatni bajt konfiguracyjny przeznaczony do wysłania.
					if(writeId < maxWrite)
					{
						//ładujemy coś do wysłania
						LPC_I2C0->I2DAT = buforWrite[writeId];
				
						//oznaczamy elementjako wysłany
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