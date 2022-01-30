// M68HC705C8S dumper

// CPU reset
#define P_CPURESET	10

// CPLD SPI bus
#define P_SPI_DATA	13
#define P_SPI_CLOCK	12
#define P_SPI_NRESET	11
#define P_CPURESET	10

// Serial I/O port linked to CPU
#define S_CPU		Serial1

void sendGlitchConfig(uint8_t clockCount, uint8_t glitchStart, uint8_t glitchStop) 
{
	uint16_t shiftval;

	shiftval = (glitchStart & 0x0F) | ((glitchStop & 0x0F) << 4) | ((clockCount & 0x7F) << 8) | 0x8000;

	digitalWrite(P_SPI_NRESET, LOW);
	digitalWrite(P_SPI_NRESET, HIGH);
	
	for (uint8_t i=0; i<16; i++) {
		digitalWrite(P_SPI_DATA, shiftval & 0x8000 ? HIGH : LOW);
		digitalWrite(P_SPI_CLOCK, HIGH);
		digitalWrite(P_SPI_CLOCK, LOW);
		shiftval <<= 1;
	}
}


void setup() {	
	// set up console serial
	Serial.begin(115200);
	Serial.println(F("Booting..."));

	// hold CPU in reset
	digitalWrite(P_CPURESET, LOW);
	pinMode(P_CPURESET, OUTPUT);

	// set up SPI pins
	digitalWrite(P_SPI_NRESET, LOW);
	digitalWrite(P_SPI_CLOCK, LOW);
	digitalWrite(P_SPI_DATA, LOW);
	pinMode(P_SPI_NRESET, OUTPUT);
	pinMode(P_SPI_CLOCK, OUTPUT);
	pinMode(P_SPI_DATA, OUTPUT);

	// set up CPU serial
	S_CPU.begin(4800);

	Serial.println(F("Ready..."));
}

/////
// Glitch search parameters

// Number of clock phases (depends on the clock divide ratio)
#define NPHASES 16
// Maximum glitch position
#define POSMAX 31
// Minimum glitch position
#define POSMIN 21

// Minimum glitch width
#define WIDMIN 0
// Maximum glitch width
#define WIDMAX 8

void loop() {

	// glitch is 32 clocks, we've found it
//	findGlitch();

	for (uint8_t glitchPos = POSMAX; glitchPos > POSMIN; glitchPos--) {
		for (uint8_t glitchPhase = 0; glitchPhase < NPHASES; glitchPhase++) {		
			for (uint8_t glitchWidth = WIDMIN; glitchWidth < WIDMAX; glitchWidth++) {
				// hold chip in reset
				digitalWrite(P_CPURESET, LOW);
	
				// clear the serial buffer
				while (S_CPU.available() > 0) {
					S_CPU.read();
				}
	
				Serial.print(F("Glitch:  clks="));
				Serial.print(glitchPos);
				Serial.print(F(" phase="));
				Serial.print(glitchPhase);
				Serial.print(F(" width="));
				Serial.print(glitchWidth);
				Serial.println();
		
				// load glitch configuration
				sendGlitchConfig(glitchPos, glitchPhase, glitchPhase+glitchWidth);
		
				// CPU internal reset delay. Min 8192 cycles (we use 15,000 for safety) of the 2MHz CPU clock
				delay(100);
		
				// release chip from reset
				digitalWrite(P_CPURESET, HIGH);
	
				// 1 byte at 4800bd = 2ms, this is 20 bytes
				// the Arduino buffers a max of 64 bytes
				delay(40);
	
				if (S_CPU.available() > 3) {
					Serial.println(F("--> DATA RECEIVED"));
					while (true) {
						uint8_t databyte = S_CPU.read();
						Serial.print(databyte, HEX);
						Serial.print(F(" "));
					}
				}
			}
		}
	}

}





///////
// Next is the code to try and find the glitch offset using the CLKO pin

#if 0
// CPU reset
#define P_CPURESET	2

// CPU clock and fast write macro -- connect to CLK1
// this is PE5 / OC3C
#define P_CPUCLK	3
#define W_CPUCLK_slow(v)  digitalWrite(P_CPUCLK, v); /* if(v) { PORTE |= 0x20; } else { PORTE &= 0x20; } */
#define W_CPUCLK(v)  if(v) { PORTE |= _BV(PE5); } else { PORTE &= _BV(PE5); }

// CPU clock feedback -- connect to CLK2
#define P_CPUCLKFB	4

// Serial I/O port linked to CPU
#define S_CPU		Serial1

// nop macro
#define NOP __asm__ __volatile__ ("nop\n\t")

void setup() {	
	// set up console serial
	Serial.begin(115200);
	Serial.println(F("Booting..."));

	// hold CPU in reset
	digitalWrite(P_CPURESET, LOW);
	pinMode(P_CPURESET, OUTPUT);

	// clock low
	digitalWrite(P_CPUCLK, LOW);
	pinMode(P_CPUCLK,	OUTPUT);

	// set clock feedback as an input
	pinMode(P_CPUCLKFB,	INPUT);

	// set up CPU serial
	S_CPU.begin(4800);

	Serial.println(F("Ready..."));
}


void findGlitch() {
	// *******
	// * Find glitch time
	// *

	// reset cpu
	digitalWrite(P_CPURESET, LOW);
	for (word i=0; i<9000; i++) {		// Oscillator startup delay -- 4096 internal processor cycles per datasheet section 3.1.1 = 8128 external clocks
		W_CPUCLK_slow(1);
		W_CPUCLK_slow(0);
	}
	digitalWrite(P_CPURESET, HIGH);

	// clock
	for (word i=0; i<60000; i++) {
		// one clock
		W_CPUCLK_slow(0);
		W_CPUCLK_slow(1);

		// settling delay
		//NOP;
		
		// Per MC68HC05C8/D Rev 1 page 2-2 fig 2-1b, the oscillator is a NAND gate with inputs /STOP and OSC1.
		// That means that when the oscillator stops, OSC2 (feedback) will go high and stay there.
		// We've set CPUCLK=1 so we should get CPUCLK=0 out.
		if (digitalRead(P_CPUCLKFB)) {
			Serial.print(F("Extclk stopped after "));
			Serial.print(i+1);
			Serial.println(F(" clocks."));
			return;
		}
	}
	Serial.println(F("No glitch found"));
}


void loop() {

	// glitch is 32 clocks, we've found it
//	findGlitch();

	digitalWrite(P_CPURESET, HIGH);
	digitalWrite(P_CPURESET, LOW);


	digitalWrite(P_CPURESET, LOW);

	delay(2000);

}
#endif
