#include <msp430.h> 

#define TAxCCR_05Hz 0xffff /* timer upper bound count value */
#define BUTTON_DELAY 0x0500

int current_number = 3184;
int current_adder = -591;

unsigned short int button_halt = 0;

#pragma vector = PORT1_VECTOR
__interrupt void S1_handler(void){
	if(P1IFG & BIT7){
		if (~button_halt & BIT7){
			if(P1IES & BIT7){

				TA2CCR1 = TA2R + BUTTON_DELAY;
				TA2CCTL1 = (TA2CCTL1 & (~0x010)) | CCIE;
				button_halt |= BIT7;
			}
			P1IES ^= BIT7;
		}
		P1IFG &= ~BIT7;
	}
}

#pragma vector = PORT2_VECTOR
__interrupt void S2_handler(void){
	if(P2IFG & BIT2){
		if(~button_halt & BIT2){
			if(P2IES & BIT2){
				current_number += current_adder;
				printNumber(current_number);

				TA2CCR2 = TA2R + BUTTON_DELAY;
				TA2CCTL2 = (TA2CCTL2 & (~0x010)) | CCIE;
				button_halt |= BIT2;
			}
			P2IES ^= BIT2;
		}
		P2IFG &= ~BIT2;
	}
}

#pragma vector = TIMER2_A1_VECTOR
__interrupt void TA2_handler(void){
	switch(TA2IV){
		case TA2IV_TACCR1:
			// S1
			button_halt &= ~BIT7;
			TA2CCTL1 = (TA2CCTL1 & (~0x010)) | (~CCIE & (0x010)); // & ~CCIE;
			break;
		case TA2IV_TACCR2:
			// S2
			button_halt &= ~BIT2;
			TA2CCTL2 = (TA2CCTL2 & (~0x010)) | (~CCIE & (0x010)); // & ~CCIE;
			break;
		default:
			break;
	}
}

void writeCommand(uint8_t *sCmd, uint8_t i) {
    // Store current GIE state
    uint16_t gie = __get_SR_register() & GIE;

    // Make this operation atomic
    __disable_interrupt();

    // CS Low
    P7OUT &= ~CS;

    // CD Low
    P5OUT &= ~CD;
    while (i){
        // USCI_B1 TX buffer ready?
        while (!(UCB1IFG & UCTXIFG)) ;

        // Transmit data
        UCB1TXBUF = *sCmd;

        // Increment the pointer on the array
        sCmd++;

        // Decrement the Byte counter
        i--;
    }

    // Wait for all TX/RX to finish
    while (UCB1STAT & UCBUSY) ;

    // Dummy read to empty RX buffer and clear any overrun conditions
    UCB1RXBUF;

    // CS High
    P7OUT |= CS;

    // Restore original GIE state
    __bis_SR_register(gie);
}

void writeData(uint8_t *sData, uint8_t i) {
    // Store current GIE state
    uint16_t gie = __get_SR_register() & GIE;

    // Make this operation atomic
    __disable_interrupt();

    if (drawmode == DOGS102x6_DRAW_ON_REFRESH){
      while (i){
          dogs102x6Memory[2 + (currentPage * 102) + currentColumn] = (uint8_t)*sData++;
          currentColumn++;

          // Boundary check
          if (currentColumn > 101){
              currentColumn = 101;
          }

          // Decrement the Byte counter
          i--;
      }
    }
    else {
      // CS Low
      P7OUT &= ~CS;
      //CD High
      P5OUT |= CD;

      while (i){
          dogs102x6Memory[2 + (currentPage * 102) + currentColumn] = (uint8_t)*sData;
          currentColumn++;

          // Boundary check
          if (currentColumn > 101){
              currentColumn = 101;
          }

          // USCI_B1 TX buffer ready?
          while (!(UCB1IFG & UCTXIFG)) ;

          // Transmit data and increment pointer
          UCB1TXBUF = *sData++;

          // Decrement the Byte counter
          i--;
      }

      // Wait for all TX/RX to finish
      while (UCB1STAT & UCBUSY) ;

      // Dummy read to empty RX buffer and clear any overrun conditions
      UCB1RXBUF;

      // CS High
      P7OUT |= CS;
    }

    // Restore original GIE state
    __bis_SR_register(gie);
}

#define SET_COLUMN_ADDRESS_MSB        0x10
#define SET_COLUMN_ADDRESS_LSB        0x00
#define SET_PAGE_ADDRESS              0xB0
void setPosition(unsigned char page, unsigned char col){
	unsigned char cmd[3] = {SET_COLUMN_ADDRESS_MSB, SET_COLUMN_ADDRESS_LSB, SET_PAGE_ADDRESS};
	cmd[0] = (cmd[0] & (~0x0f)) | ((page >> 4) & (0x0f));
	cmd[1] = (cmd[1] & (~0x0f)) | (page & 0x0f);
	cmd[2] = (cmd[2] & (~0x0f)) | (col & 0x0f);
	writeCommand(cmd, 3);
}

void printNumber(int num){
	unsigned char current_page = 7;
	unsigned char current_col = 0;
}

void printSymbol(int index, unsigned char page, unsigned int col){
	setPosition(page-1, col);
	writeData(_font[index], 6);
	setPosition(page, col);
	writeData(_font[index] + 6, 6);
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    __bis_SR_register(GIE);

	P1SEL &= ~BIT7;
	P1DIR &= ~BIT7;
	P1OUT |= BIT7;
	P1REN |= BIT7;
	P1IES |= BIT7;
	P1IFG &= ~BIT7;
	P1IE |= BIT7;


	P2SEL &= ~BIT2;
	P2DIR &= ~BIT2;
	P2OUT |= BIT2;
	P2REN |= BIT2;
	P2IES |= BIT2;
	P2IFG &= ~BIT2;
	P2IE |= BIT2;

	P7SEL &= ~(BIT4 | BIT6);
	P7DIR |= (BIT4 | BIT6);

	P5SEL &= ~(BIT6 | BIT7);
	P5DIR |= (BIT6 | BIT7);

	// reset LCD
	P7OUT |= BIT4;
	P5OUT |= BIT7;

	// P4SEL |= BIT3;
	P7OUT &= ~BIT6;




	UCSCTL3 = (UCSCTL3 & (~0x070)) | SELREF__XT1CLK;
	UCSCTL3 = (UCSCTL3 & (~0x07)) | FLLREFDIV__2;
	UCSCTL2 = (UCSCTL2 & (~0x0cff)) | ((8 - 1) & (0x0cff)); // FLLN multiplier
	UCSCTL2 = (UCSCTL2 & (~0x07000)) | FLLD__8; // FLLD divider
	UCSCTL1 = (UCSCTL1 & (~0x070)) | DCORSEL_1;

	UCSCTL4 = (UCSCTL4 & (~0x070)) | SELS__DCOCLKDIV;
	UCSCTL5 = (UCSCTL5 & (~0x070)) | DIVS__1;

	TA2CTL = (TA2CTL & (~0x0300)) | TASSEL__SMCLK;
	TA2CTL = (TA2CTL & (~0x030)) | MC__CONTINOUS;
	TA2CTL = (TA2CTL & (~0x0c0)) | ID__4;
	TA2CTL |= TACLR;
	TA2CCTL1 = (TA2CCTL1 & (~0x0100)) & ~CAP;
	TA2CCTL2 = (TA2CCTL2 & (~0x0100)) & ~CAP;
	TA2CCTL1 = (TA2CCTL1 & (~0x010)) & ~CCIE;
	TA2CCTL2 = (TA2CCTL2 & (~0x010)) & ~CCIE;
	
	UCB1IFG = (UCB1IFG & (~0x03)) | ((~UCTXIFG & (0x02)) | (~UCRXIFG & (0x01)));
	UCB1IE = (UCB1IE & (~0x03)) | UCTXIE | UCRXIE;


	return 0;
}
