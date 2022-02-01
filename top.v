module top(

output DIL_1,			// paired with DIL_1_GCK
input  DIL_1_GCK,		// main clock

output DIL_2,			// paired with DIL_2_GCK
input  DIL_2_GCK,

input  DIL_3,			// reset
/* 
	DIL_4,
	DIL_5,
	DIL_6,
	DIL_7,
   DIL_8,
	DIL_9,
	DIL_10,
*/
input	DIL_11,		// SPI Register nRST (clear)
input	DIL_12,		// SPI SCK
input	DIL_13,		// SPI SDI
/*
	// DIL_14 is ground
	DIL_15,
	DIL_16,
	DIL_17,
	DIL_18,
	DIL_19,
	DIL_20,
	DIL_21,
output DIL_22,
output DIL_23,
*/
output DIL_24,
output DIL_25,		// glitch trigger
output DIL_26,		// MCU clock out
output DIL_27,		// synchronised nrst
	// DIL_28 is power
output _PGND1,		// tie to GND
output _PGND2		// tie to GND
);

	// PGND need to be pulled low to avoid ground bounce
	assign _PGND1 = 1'b0;
	assign _PGND2 = 1'b0;
	  
	// DIL1 and DIL2 are paired with the GCKs and need to be assigned hi-Z
	assign DIL_1 = 1'bZ;
	assign DIL_2 = 1'bZ;


	//////////////////
	// SPI input port
	wire SPI_NRST = DIL_11;
	wire SPI_SCK = DIL_12;
	wire SPI_SDI = DIL_13;
  
	reg [15:0] spi_reg;
	always @(posedge SPI_SCK or negedge SPI_NRST) begin
		if (!SPI_NRST) begin
			spi_reg <= 16'd0;
		end else begin
			// Clock in MSB to LSB
			spi_reg <= {spi_reg[14:0], SPI_SDI};
		end
	end

	// SPI config register
	wire [3:0]	cfg_glitchstart	= spi_reg[3:0];		// glitch start time
	wire [3:0]	cfg_glitchstop		= spi_reg[7:4];		// glitch stop time
	wire [6:0]	cfg_clkcnt			= spi_reg[14:8];		// number of (normal) clocks before enabling glitch
	wire			cfg_glitchenable	= spi_reg[15];			// enable clock glitch 1=yes
	
	
	//////////////////
 	// primary clock divider -- generates 2MHz from 16MHz
	// 2 meg clk = 4800 baud
	wire MCLK = DIL_1_GCK;
	
	parameter DIVISOR = 32_000_000/2_000_000;	// 32MHz in, 2MHz out
	
	reg [7:0] clkdiv_r;
	reg clk_out;
	always @(posedge MCLK) begin
		clkdiv_r <= clkdiv_r + 8'd1;
		if (clkdiv_r >= (DIVISOR-1)) begin
			clkdiv_r <= 8'd0;
		end
		
		clk_out <= (clkdiv_r < DIVISOR/2) ? 1'b1 : 1'b0;
	end


	//////////////////
	// reset synchroniser
	
	// this synchronises the reset signal against the 2MHz generated clock.
	// this keeps the CPU in sync with the glitch generator
	wire NRST = DIL_3;

	reg nrst_sync_r;
	always @(posedge clk_out) begin
		nrst_sync_r <= NRST;
	end

	assign DIL_27 = nrst_sync_r;

	
	//////////////////
	// glitch generator
	reg glitch;
	always @(posedge MCLK) begin
		glitch <= (clkdiv_r >= cfg_glitchstart) && (clkdiv_r <= cfg_glitchstop) ? 1'b1 : 1'b0;
	end


	//////////////////
	// glitch trigger generator
	reg glitch_trigger;
	reg [6:0] trigcnt_r;
	always @(posedge clk_out or negedge nrst_sync_r) begin
		if (!nrst_sync_r) begin
			trigcnt_r <= 7'd0;
			glitch_trigger <= 1'd0;
		end else begin
			trigcnt_r <= (trigcnt_r == 7'h7F) ? 7'h7F : trigcnt_r + 7'd1;
			glitch_trigger <= (trigcnt_r == cfg_clkcnt) ? 1'b1 : 1'b0;
		end
	end

	
	//////////////////
	// glitch delay to allow glitch to straddle l->h edge of next pulse

	wire gxin = !cfg_glitchenable && (glitch && glitch_trigger);
	reg[3:0] gsr;
	always @(posedge MCLK) begin
		gsr <= {gsr[2:0], gxin};
	end
	wire gxout = gsr[3];
	
	
	//////////////////
	// glitch generator
	
	assign DIL_26 = cfg_glitchenable ? clk_out ^ (glitch && glitch_trigger) : clk_out;
	assign DIL_25 = cfg_glitchenable & glitch_trigger;
	assign DIL_24 = gxout; //!cfg_glitchenable && (glitch && glitch_trigger);
	
endmodule
