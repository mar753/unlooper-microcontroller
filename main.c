//attiny2313, clock (clk): 14,29MHz; tested unit clock: 1,78MHz
//v1.1
//Marcel Szewczyk

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

//#define F_CPU 14285714

volatile unsigned char reset = 1;
volatile unsigned char delay[10];
volatile unsigned char numberOfGlitches = 0;

void RSInit(void){
	UCSRA = (1<<U2X); // /8
	UBRRH = 0;
	UBRRL = 185; //if clk=14,29MHz (100/7) -> 9600 baud	
	UCSRB = (1<<RXEN)|(1<<TXEN);//enable transmit and receive
	UCSRC = (1<<UCSZ1)|(1<<UCSZ0);//8 bits of data
}

//send byte to RS232
void RSTransmit(unsigned char data){
	while( !( UCSRA & (1<<UDRE)) );
	UDR = data;
}

//receive byte from RS232
unsigned char RSReceive(void){
	while( !(UCSRA & (1<<RXC)) );
	return UDR;
}

ISR(TIMER1_COMPB_vect){
	
	//because of the current clock frequency - max 3 glitches, one by one
	asm volatile(
		
	"L_l0%=:""\n\t"  
		"dec %[nog]""\n\t"
		"breq L_l2%=" "\n\t"
		"ld r24, %a0+""\n\t"
		"sbi 0x12, 3" "\n\t" //port D, bit 3
		"cbi 0x12, 3" "\n\t"		
		
	"L_l1%=:""\n\t"
		"dec r24" "\n\t"
		"breq L_l0%=" "\n\t" 
		"nop""\n\t"
		"nop""\n\t"
		"nop""\n\t"
		"nop""\n\t"
		"brne L_l1%=" "\n\t"		
		
	"L_l2%=:""\n\t"
		"nop""\n\t" 
		"sbi 0x12, 3""\n\t" //port D
		"cbi 0x12, 3""\n\t"
		
		: 
		: "e" (delay+2), [nog] "r" (numberOfGlitches)
		: "r24"		
    );

	TIMSK &= ~(1 << OCIE1B);
}

int main(void){
	RSInit();
	DDRD = 0xDF;//pd5 must be equal 0 - otherwise T1 input will consume more power unnecessarily
	DDRB |= (1 << 0)|(1 << 1)|(1 << 2)|(1 << 3)|(1 << 5);
	TCCR0A = (1 << COM0A1)|(1 << WGM00)|(1 << WGM01);
	TCCR0B = (1 << CS00); //enable 8bit timer; clocking from clk, no prescaler
	PORTD &= ~(1 << 4);//74hc4053 enable=0 signal
	PORTB &= ~(1 << 5);//tested unit reset signal - low
	
	//welcome message
	RSTransmit(0x48);
	RSTransmit(0x45);
	RSTransmit(0x4C);
	RSTransmit(0x4C);
	RSTransmit(0x4F);
	RSTransmit('!');
	RSTransmit(' ');
	
	unsigned char c;
	
	while(1){
		c = RSReceive();
		
		switch(c){
		case 0x0:	//version
			RSTransmit(0x0); //ACK
			RSTransmit(' ');
			RSTransmit('U');
			RSTransmit('n');
			RSTransmit('l');
			RSTransmit('o');
			RSTransmit('o');
			RSTransmit('p');
			RSTransmit('e');
			RSTransmit('r');
			RSTransmit(' ');
			RSTransmit('v');
			RSTransmit('2');
			RSTransmit(' ');
			RSTransmit('(');
			RSTransmit('M');
			RSTransmit('S');
			RSTransmit(')');
			RSTransmit(' ');
			RSTransmit('f');
			RSTransmit('i');
			RSTransmit('r');
			RSTransmit('m');
			RSTransmit('w');
			RSTransmit('a');
			RSTransmit('r');
			RSTransmit('e');
			RSTransmit(' ');
			RSTransmit('v');
			RSTransmit('1');
			RSTransmit('.');
			RSTransmit('1');
			RSTransmit(' ');
			break;			
			
		case 0x1://set tested unit supply voltage to 5V
			RSTransmit(0x1); //ACK
			PORTD |= (1 << 2);
			break;
			
		case 0x2://set tested unit supply voltage to an analog value from attiny2313 (default)
			RSTransmit(0x2); //ACK
			PORTD &= ~(1 << 2);
			break;
			
		case 0x3://tested unit reset signal - low
			RSTransmit(0x3); //ACK
			PORTB &= ~(1 << 5);
			break;
			
		case 0x4://tested unit reset signal - high
			RSTransmit(0x4); //ACK
			PORTB |= (1 << 5);
			break;
			
		case 0x5://tested unit clock signal - disabled
			RSTransmit(0x5); //ACK			
			PORTB |= (1 << 3);
			break;
			
		case 0x6://tested unit clock signal - enabled
			RSTransmit(0x6); //ACK
			PORTB &= ~(1 << 3);
			break;
			
		case 0x7: //set voltage level (for 0x2 instruction)
			RSTransmit(0x7); //ACK
			OCR0A = RSReceive();			
			break;			
		
		case 0x8://after each 0xB instruction tested unit is resetted (default)
			RSTransmit(0x8); //ACK
			reset = 1;
			break;
		
		case 0x9://after each 0xB instruction tested unit is not resetted
			RSTransmit(0x9); //ACK
			reset = 0;
			break;
		
		case 0xA:
			RSTransmit(0xA); //ACK
			//
			RSTransmit(' '); 
			RSTransmit('N');
			RSTransmit('o');
			RSTransmit('t');
			RSTransmit(' ');
			RSTransmit('i');
			RSTransmit('m');
			RSTransmit('p');
			RSTransmit('l');
			RSTransmit('e');
			RSTransmit('m');
			RSTransmit('e');
			RSTransmit('n');
			RSTransmit('t');
			RSTransmit('e');
			RSTransmit('d');
			RSTransmit(' ');
			break;
		
		case 0xB: //generate precise one/two/three glitches. Minimum gap between two consecutive glitches is 1-255 cycles of the tested unit clock (1,78MHz)
			RSTransmit(0xB); //ACK	
			PORTB |= (1 << 0);
			numberOfGlitches = RSReceive(); //(min 1, max 3)
			if((numberOfGlitches<1) || (numberOfGlitches>3))
				numberOfGlitches = 1;
			
			delay[0] = RSReceive();
			for(int i=1; i<numberOfGlitches+1; i++){
				delay[i] = RSReceive();
				if(delay[i] == 0)
					delay[i]++;
			}
			
			TCNT1 = 0;//timera/counter reset
			
			PORTB |= (1 << 3); //enable tested unit clock signal
			
			if(reset){
			PORTB &= ~(1 << 5);//tested unit reset signal - low
			_delay_ms(5);
			//disable reset
			PORTB |= (1 << 5);
			_delay_ms(50);		
			}

			OCR1BH = delay[0]; //value to be compared with the 16-bit attiny2313 timer
			OCR1BL = delay[1];
			
			sleep_enable();
			TIMSK |= (1 << OCIE1B);	
			sei();			
			
			numberOfGlitches++;
			TCCR1B = (1 << CS10)|(1 << CS11)|(1 << CS12);//16-bit is clocked from T1 input
			
			sleep_cpu(); //cpu is in sleep mode now
			sleep_disable();
			
			cli();			
			TCCR1B = 0;
			PORTB &= ~(1 << 0);
			
			break;
		
		default:
			RSTransmit(' '); //wrong instruction
			RSTransmit('W');
			RSTransmit('r');
			RSTransmit('o');
			RSTransmit('n');
			RSTransmit('g');
			RSTransmit(' ');
			RSTransmit('i');
			RSTransmit('n');
			RSTransmit('s');
			RSTransmit('t');
			RSTransmit('r');
			RSTransmit('u');
			RSTransmit('c');
			RSTransmit('t');
			RSTransmit('i');
			RSTransmit('o');
			RSTransmit('n');
			RSTransmit(' ');
			break;
		}	
	}	
}
