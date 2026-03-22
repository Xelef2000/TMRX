// Minimal behavioral stubs for IHP sg13g2 standard cells used in tc_clk.sv.
// These replace the full sg13g2_stdcell.v which contains specify blocks with
// conditional path delays that Yosys cannot parse.  The implementations are
// cycle-accurate for formal equivalence purposes.

module sg13g2_buf_1  (output X, input A);         assign X = A;         endmodule
module sg13g2_inv_1  (output Y, input A);         assign Y = ~A;        endmodule
module sg13g2_mux2_1 (output X, input A0, A1, S); assign X = S ? A1 : A0; endmodule
module sg13g2_xor2_1 (output X, input A, B);      assign X = A ^ B;     endmodule
// Clock gate: combinational approximation sufficient for equivalence checking.
module sg13g2_slgcp_1 (output GCLK, input GATE, SCE, CLK);
  assign GCLK = CLK & (GATE | SCE);
endmodule
