///////////////////////////////////////////////////////////////////////////////
// Description: SPI (Serial Peripheral Interface) Master
//              Creates master based on input configuration.
//              Sends a byte one bit at a time on MOSI
//              Will also receive byte data one bit at a time on MISO.
//              Any data on input byte will be shipped out on MOSI.
//
//              To kick-off transaction, user must pulse i_TX_DV.
//              This module supports multi-byte transmissions by pulsing
//              i_TX_DV and loading up i_TX_Byte when o_TX_Ready is high.
//
//              This module is only responsible for controlling Clk, MOSI, 
//              and MISO.  If the SPI peripheral requires a chip-select, 
//              this must be done at a higher level.
//
// Note:        clk_i must be at least 2x faster than i_SPclk_i
//
// Parameters:  SPI_MODE, can be 0, 1, 2, or 3.  See above.
//              Can be configured in one of 4 modes:
//              Mode | Clock Polarity (CPOL/CKP) | Clock Phase (CPHA)
//               0   |             0             |        0
//               1   |             0             |        1
//               2   |             1             |        0
//               3   |             1             |        1
//              More: https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus#Mode_numbers
//              CLKS_PER_HALF_BIT - Sets frequency of o_SPclk_i.  o_SPclk_i is
//              derived from clk_i.  Set to integer number of clocks for each
//              half-bit of SPI data.  E.g. 100 MHz clk_i, CLKS_PER_HALF_BIT = 2
//              would create o_SPclk_i of 25 MHz.  Must be >= 2
//
//
//
// MIT License
//
// Copyright (c) 2019 russell-merrick
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// source: https://github.com/nandland/spi-master
//
///////////////////////////////////////////////////////////////////////////////

module top
  #(parameter SPI_MODE = 0,
    parameter CLKS_PER_HALF_BIT = 2)
  (
   // Control/Data Signals,
   input        rst_ni,     // FPGA Reset
   input        clk_i,       // FPGA Clock
   
   // TX (MOSI) Signals
   input [7:0]  i_TX_Byte,        // Byte to transmit on MOSI
   input        i_TX_DV,          // Data Valid Pulse with i_TX_Byte
   output       o_TX_Ready,       // Transmit Ready for next byte
   
   // RX (MISO) Signals
   output       o_RX_DV,     // Data Valid pulse (1 clock cycle)
   output [7:0] o_RX_Byte,   // Byte received on MISO

   // SPI Interface
   output o_SPclk_i,
   input      i_SPI_MISO,
   output o_SPI_MOSI
   );
  // tmrg default triplicate
  // tmrg tmr_error true
  // tmrg do_not_triplicate rst_ni clk_i i_TX_Byte i_TX_DV o_TX_Ready o_RX_DV o_RX_Byte o_SPclk_i i_SPI_MISO o_SPI_MOSI

  // SPI Interface (All Runs at SPI Clock Domain)
  wire w_CPOL;     // Clock polarity
  wire w_CPHA;     // Clock phase

  reg [$clog2(CLKS_PER_HALF_BIT*2)-1:0] r_SPclk_i_Count;
  reg r_SPclk_i;
  reg [4:0] r_SPclk_i_Edges;
  reg r_Leading_Edge;
  reg r_Trailing_Edge;
  reg       r_TX_DV;
  reg [7:0] r_TX_Byte;
  reg [2:0] r_RX_Bit_Count;
  reg [2:0] r_TX_Bit_Count;
  reg       o_TX_Ready_q;
  reg       o_RX_DV_q;
  reg [7:0] o_RX_Byte_q;
  reg       o_SPclk_i_q;
  reg       o_SPI_MOSI_q;

  wire [$clog2(CLKS_PER_HALF_BIT*2)-1:0] r_SPclk_i_CountVoted;
  wire r_SPclk_iVoted;
  wire [4:0] r_SPclk_i_EdgesVoted;
  wire r_Leading_EdgeVoted;
  wire r_Trailing_EdgeVoted;
  wire r_TX_DVVoted;
  wire [7:0] r_TX_ByteVoted;
  wire [2:0] r_RX_Bit_CountVoted;
  wire [2:0] r_TX_Bit_CountVoted;
  wire o_TX_Ready_qVoted;
  wire o_RX_DV_qVoted;
  wire [7:0] o_RX_Byte_qVoted;
  wire o_SPclk_i_qVoted;
  wire o_SPI_MOSI_qVoted;

  assign r_SPclk_i_CountVoted = r_SPclk_i_Count;
  assign r_SPclk_iVoted = r_SPclk_i;
  assign r_SPclk_i_EdgesVoted = r_SPclk_i_Edges;
  assign r_Leading_EdgeVoted = r_Leading_Edge;
  assign r_Trailing_EdgeVoted = r_Trailing_Edge;
  assign r_TX_DVVoted = r_TX_DV;
  assign r_TX_ByteVoted = r_TX_Byte;
  assign r_RX_Bit_CountVoted = r_RX_Bit_Count;
  assign r_TX_Bit_CountVoted = r_TX_Bit_Count;
  assign o_TX_Ready_qVoted = o_TX_Ready_q;
  assign o_RX_DV_qVoted = o_RX_DV_q;
  assign o_RX_Byte_qVoted = o_RX_Byte_q;
  assign o_SPclk_i_qVoted = o_SPclk_i_q;
  assign o_SPI_MOSI_qVoted = o_SPI_MOSI_q;

  assign o_TX_Ready = o_TX_Ready_qVoted;
  assign o_RX_DV = o_RX_DV_qVoted;
  assign o_RX_Byte = o_RX_Byte_qVoted;
  assign o_SPclk_i = o_SPclk_i_qVoted;
  assign o_SPI_MOSI = o_SPI_MOSI_qVoted;

  // CPOL: Clock Polarity
  // CPOL=0 means clock idles at 0, leading edge is rising edge.
  // CPOL=1 means clock idles at 1, leading edge is falling edge.
  assign w_CPOL  = (SPI_MODE == 2) | (SPI_MODE == 3);

  // CPHA: Clock Phase
  // CPHA=0 means the "out" side changes the data on trailing edge of clock
  //              the "in" side captures data on leading edge of clock
  // CPHA=1 means the "out" side changes the data on leading edge of clock
  //              the "in" side captures data on the trailing edge of clock
  assign w_CPHA  = (SPI_MODE == 1) | (SPI_MODE == 3);



  // Purpose: Generate TX Ready flag (not triplicated)
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
      o_TX_Ready_q <= 1'b0;
    else if (i_TX_DV || r_SPclk_i_EdgesVoted > 0)
      o_TX_Ready_q <= 1'b0;
    else
      o_TX_Ready_q <= 1'b1;
  end

  // Purpose: Generate SPI Clock correct number of times when DV pulse comes
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
    begin
      r_SPclk_i_Edges <= 0;
      r_Leading_Edge  <= 1'b0;
      r_Trailing_Edge <= 1'b0;
      r_SPclk_i       <= w_CPOL; // assign default state to idle state
      r_SPclk_i_Count <= 0;
    end
    else
    begin

      // Default assignments
      r_Leading_Edge  <= 1'b0;
      r_Trailing_Edge <= 1'b0;

      if (i_TX_DV)
        r_SPclk_i_Edges <= 16;  // Total # edges in one byte ALWAYS 16
      else if (r_SPclk_i_EdgesVoted > 0)
      begin
        if (r_SPclk_i_CountVoted == CLKS_PER_HALF_BIT*2-1)
        begin
          r_SPclk_i_Edges <= r_SPclk_i_EdgesVoted - 1'b1;
          r_Trailing_Edge <= 1'b1;
          r_SPclk_i_Count <= 0;
          r_SPclk_i       <= ~r_SPclk_iVoted;
        end
        else if (r_SPclk_i_CountVoted == CLKS_PER_HALF_BIT-1)
        begin
          r_SPclk_i_Edges <= r_SPclk_i_EdgesVoted - 1'b1;
          r_Leading_Edge  <= 1'b1;
          r_SPclk_i_Count <= r_SPclk_i_CountVoted + 1'b1;
          r_SPclk_i       <= ~r_SPclk_iVoted;
        end
        else
          r_SPclk_i_Count <= r_SPclk_i_CountVoted + 1'b1;
      end

    end // else: !if(~rst_ni)
  end // always @ (posedge clk_i or negedge rst_ni)


  // Purpose: Register i_TX_Byte when Data Valid is pulsed.
  // Keeps local storage of byte in case higher level module changes the data
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
    begin
      r_TX_Byte <= 8'h00;
      r_TX_DV   <= 1'b0;
    end
    else
      begin
        r_TX_DV <= i_TX_DV; // 1 clock cycle delay
        if (i_TX_DV)
        begin
          r_TX_Byte <= i_TX_Byte;
        end
      end // else: !if(~rst_ni)
  end // always @ (posedge clk_i or negedge rst_ni)


  // Purpose: Generate MOSI data (not triplicated)
  // Works with both CPHA=0 and CPHA=1
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
      o_SPI_MOSI_q <= 1'b0;
    else
    begin
      // Catch the case where we start transaction and CPHA = 0
      if (r_TX_DVVoted & ~w_CPHA)
        o_SPI_MOSI_q <= r_TX_ByteVoted[3'b111];
      else if ((r_Leading_EdgeVoted & w_CPHA) | (r_Trailing_EdgeVoted & ~w_CPHA))
        o_SPI_MOSI_q <= r_TX_ByteVoted[r_TX_Bit_CountVoted];
    end
  end

  // Purpose: Track TX bit position
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
      r_TX_Bit_Count <= 3'b111; // send MSb first
    else
    begin
      // If ready is high, reset bit counts to default
      if (o_TX_Ready_qVoted)
        r_TX_Bit_Count <= 3'b111;
      else if (r_TX_DVVoted & ~w_CPHA)
        r_TX_Bit_Count <= 3'b110;
      else if ((r_Leading_EdgeVoted & w_CPHA) | (r_Trailing_EdgeVoted & ~w_CPHA))
        r_TX_Bit_Count <= r_TX_Bit_CountVoted - 1'b1;
    end
  end


  // Purpose: Read in MISO data (not triplicated).
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
    begin
      o_RX_Byte_q <= 8'h00;
      o_RX_DV_q   <= 1'b0;
    end
    else
    begin
      // Default Assignments
      o_RX_DV_q <= 1'b0;

      if ((r_Leading_EdgeVoted & ~w_CPHA) | (r_Trailing_EdgeVoted & w_CPHA))
      begin
        o_RX_Byte_q[r_RX_Bit_CountVoted] <= i_SPI_MISO;  // Sample data
        if (r_RX_Bit_CountVoted == 3'b000)
          o_RX_DV_q <= 1'b1;   // Byte done, pulse Data Valid
      end
    end
  end

  // Purpose: Track RX bit position
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
      r_RX_Bit_Count <= 3'b111;
    else
    begin
      if (o_TX_Ready_qVoted) // Check if ready is high, if so reset bit count to default
        r_RX_Bit_Count <= 3'b111;
      else if ((r_Leading_EdgeVoted & ~w_CPHA) | (r_Trailing_EdgeVoted & w_CPHA))
        r_RX_Bit_Count <= r_RX_Bit_CountVoted - 1'b1;
    end
  end
  
  
  // Purpose: Add clock delay to signals for alignment.
  always @(posedge clk_i or negedge rst_ni)
  begin
    if (~rst_ni)
    begin
      o_SPclk_i_q <= w_CPOL;
    end
    else
      begin
        o_SPclk_i_q <= r_SPclk_iVoted;
      end // else: !if(~rst_ni)
  end // always @ (posedge clk_i or negedge rst_ni)
  

endmodule // SPI_Master
