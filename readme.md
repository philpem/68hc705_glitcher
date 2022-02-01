# 68HC705C8 Glitcher

Phil Pemberton <philpem@philpem.me.uk>, Februrary 2022

This is provided completely as-is, in the hope that it might be somewhat useful to someone out there.
I don't guarantee any form of support, but I'll do my best to answer your questions within my very
limited free time.


## What this does

TL/DR: Uses fault injection attacks (power and clock glitching) to dump the EPROM in secured (`OPTION.SEC=1`) MC68HC705C8 microcontrollers.

There are two parts to the glitcher: the Arduino and the CPLD. The Arduino code runs on a Mega2560 style board (I used an Elegoo Mega2560 R3).
The CPLD code runs on an Altera MAX7000S EPM7064STC44-10 on an FPGAArcade DIP28 CPLD board.

The pin assignments for the Arduino are in `#define` statements at the top of the sketch; the ones for the CPLD are in the Quartus constraints
file (`qsf` file) and below -- assuming you're using the same type of CPLD board I am. If not, you can easily port it to another MAX7000S board
with minimal effort. If you don't have a MAX7000S, I stuck to standard Verilog, so it'll probably port quite easily to other 5V CPLDs.
If you can't find a 5V CPLD, you can use a 3.3V CPLD or FPGA with suitable level shifting hardware.


### Theory of operation

On boot, the Arduino sets up the hardware then enters a loop which shifts a configuration block into the CPLD's `glitch_config` register. This
register contains the following settings:

  * Glitch mode (power or clock)
  * Number of 2MHz clocks to wait before glitching
  * Glitch phase (start in 1/16th-cycle increments)
  * Glitch width (end in 1/16th-cycle increments)

There is also a 4-clock delay on the power glitch output. This proved necessary to successfully glitch an older (`9C11C` mask rev) 68HC705C8 part,
but not for glitching a later (`0C16W` mask rev) 68HC705C8. It is left as an exercise to the reader to add an extra bit to turn this skewing on
and off dynamically, or to allow glitches to cross a clock boundary (this will probably require a second counter).

The glitch search process is essentially brute-force. The Arduino loops over all reasonable glitch phase and width combinations for every possible
clock delay value. The clock delay counts down, as the `STOP` instruction will be the last thing executed (see below).

When the clock cycle counter reaches the configured target, the CPLD will generate an internal `Glitch Trigger` signal. This signal gates the glitches
(which are continuously generated) onto either the target MCU clock (clock glitch mode) or power supply FET gate (power glitch mode).

The MCU reset signal from the Arduino is synchronised with the 2MHz clock to try and keep the MCU boot process as deterministic as possible. In practice
there's still a +/- 1-clock uncertainty in the number of clocks before the MCU actually stops running, but this beats a +/- 2-clock uncertainty.

There are 16 possible start and stop timing values, which gives a resolution of `1/32MHz = 31.25ns`. It's nowhere near as good as the ChipWhisperer can do,
but it's good enough.

The CPLD is clocked by a 14-pin CMOS tin-can oscillator of the 32MHz variety, which is divided down inside the CPLD to produce the nominal 2MHz clock to
run the 68HC705C8. 

Finally, the 68HC705C8 is wired for bootstrap readout mode, and its serial port is wired to the Arduino's `TX1`/`RX1` serial port. This allows the Arduino to
detect when the 68HC705C8 has started sending data (the ROM contents), capture it, and relay it to the attached PC.

Power glitching is done by the same fundamental method the ChipWhisperer uses: a FET shorting the 5V rail. I've found this particular part of the circuit to
be quite finicky. What works for one chip may not work for another. Here are some things I had to try to successfully read out my parts:

  * **Different FETs**. Some chips glitched with a single 2N7000, others glitched with four in parallel. A Fairchild FDS4435 was used for other testing, with some success.
  * **Series resistor**. There is a series resistor between the 5V rail and the CPU and the FET, to limit current. I tried several values between 10 Ohms and 220 Ohms.

If something doesn't work, look at what's happening and change one of the variables. If the Vcc rail is recovering too slowly, you probably a lower resistor value.
If the glitch is too steep and is resetting the CPU, either shorten the glitch interval or try a different FET.
If all else fails, try a different type of FET. This will influence the pulse shape and may be exactly what's needed to get a successful glitch.


# Attacking the 68HC705C8, a step by step guide

## Finding the timing value

While testing all possible timing values is possible, it's likely to be quite slow. Ideally we need to learn a little about how the security scheme is implemented
Thankfully Motorola produced a [very useful technical update on the 68HC05C8, which includes the bootloader source code](http://bitsavers.org/components/motorola/6805/_technical_update/Motorola_Technical_Update_MC68HC705C.pdf).

I started by tracing the bootloader's execution, and found that it checks the `SEC` bit in the `OPTION` register quite early in the bootstrap code.
If this bit was set (security enabled), the 68HC705C8 would execute a `STOP` instruction. This instruction puts the 68HC705C8 to sleep, which turns off the
internal crystal oscillator circuit. This is extremely helpful as it allows us to measure the approximate number of clock cycles the MCU has executed, and
to see if it is still executing code.

Briefly, if the 68HC705C8 is still running code, the `CLK2` will be the inverse of the `CLK1` input. If the core is `STOP`ped, the `CLK2` output will be stuck
either high or low.

It stands to reason that we'd want to trigger a glitch shortly before the `STOP` instruction is executed, to make the 68HC05 core skip over the instruction.
There is, however, a sting in the tail: the 68HC05C8 also has an internal reset timer, which counts 4096 CPU execution cycles (8192 external clocks due to the
2:1 divide ratio).

Getting around the power-up timer proved simple: the timer is started on the high-to-low edge of `/RESET`, and the output of the timer is logically `AND`-ed with
the incoming reset signal. That means that to release the CPU from reset, the timer must have expired, and the 

To remove the timer's influence from our cycle counts, we need to hold the CPU in reset for at least 8192 cycles of the `CLK1` input before releasing the reset pin
and allowing the CPU to start executing code.

After implementing this workaround, the cycle count becomes immediately apparent: the CPU core stops executing after around 30-31 clocks if the security bit is set.

See [MC68HC705C8 Technical Data (MC68HC705C8/D Rev 1)](http://bitsavers.org/components/motorola/6805/_dataSheets/MC68HC705C8_Technical_Data_1990.pdf) section 3.1.1
"Poweron Reset" for more on the POR circuit.


## Glitching

Sadly I have found no better option than "brute force and ignorance" to find the timing values. This isn't really a big deal, because it doesn't take long to try
all 32 possible start/stop value combinations.





# CPLD internals

## Pin assignments

```
DIL Pin			Function
1	CLK			Master clock input
2
3	NRST			MCU reset input / glitch trigger
4	

11	SPI_NRST		SPI register clear
12	SPI_SCK		SPI clock input (L->H)
13	SPI_SDA		SPI data input

14	GND			ground

24	PWRGLITCH	Positive-going pulse to fire an n-mosfet which shorts the 5V rail of the MCU
25	SCOPETRIG	Glitch trigger e.g. for oscilloscope triggering or indication (pulses when the glitch is active)
26	MCUCLK_O		Clock out to MCU, with glitches
27	NRST_SYNC	MCU /RESET, syncronised with 2MHz clock (MCUCLK_O)

28	VCC			+5V power
```


# SPI configuration register
==========================

The register is cleared to zero when `SPI_NRST` is low.

Bits are shifted in on the L->H edge of `SPI_SCK`, from MSB to LSB.

```
Bit		Function
15			GLITCH_MODE		1= clock glitching, 0= power glitching
14..8		CLK_COUNT		Number of normal clocks to wait before triggering a glitch
									0 means trigger on the first clock pulse after the synchronised reset pulse.
7..4		GLITCH_END		Fast Clock phase (0-7) to end the glitch
3..0		GLITCH_START	Fast Clock phase (0-7) to start the glitch
									Setting GLITCH_START = GLITCH_END results in a 1-MCLK wide glitch.
```