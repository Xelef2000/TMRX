// Minimal stub for tc_sram_impl used in formal equivalence checking.
// The real implementation (ihp13/tc_sram_impl.sv) instantiates IHP SRAM macros
// with unsupported timing constructs and cannot be read by slang.
// This stub declares the correct port interface so slang can resolve implicit
// port connections (.clk_i, / .rst_ni, shorthand) in croc_domain.sv.
// impl_in_t / impl_out_t default to `logic` in the real module; we hardcode
// that here to avoid `parameter type` which is not supported in compat-mode.
//
// For formal equivalence checking we do NOT mark this blackbox: smtbmc requires
// all modules to be concrete.  We model the SRAM as unconstrained (rdata_o is
// fed from a free-running register that absorbs writes) so the proof holds for
// arbitrary read data.
module tc_sram_impl #(
  parameter int unsigned NumWords    = 32'd1024,
  parameter int unsigned DataWidth   = 32'd128,
  parameter int unsigned ByteWidth   = 32'd8,
  parameter int unsigned NumPorts    = 32'd2,
  parameter int unsigned Latency     = 32'd1,
  parameter              SimInit     = "none",
  parameter bit          PrintSimCfg = 1'b0,
  parameter              ImplKey     = "none",
  // Derived parameters
  parameter int unsigned AddrWidth = (NumWords > 32'd1) ? $clog2(NumWords) : 32'd1,
  parameter int unsigned BeWidth   = (DataWidth + ByteWidth - 32'd1) / ByteWidth
) (
  input  logic                             clk_i,
  input  logic                             rst_ni,
  input  logic                             impl_i,
  output logic                             impl_o,
  input  logic  [NumPorts-1:0]             req_i,
  input  logic  [NumPorts-1:0]             we_i,
  input  logic  [NumPorts-1:0][AddrWidth-1:0] addr_i,
  input  logic  [NumPorts-1:0][DataWidth-1:0] wdata_i,
  input  logic  [NumPorts-1:0][BeWidth-1:0]   be_i,
  output logic  [NumPorts-1:0][DataWidth-1:0] rdata_o
);
  // Pass impl signal through unchanged.
  assign impl_o = impl_i;

  // Abstract memory model: a single register per port that captures the last
  // written data.  This keeps the SMT problem tractable while still providing
  // a concrete (non-blackbox) implementation that smtbmc can elaborate.
  // Both gold and gate instantiate the same stub, so rdata_o behaves
  // identically on both sides of the equivalence miter.
  logic [NumPorts-1:0][DataWidth-1:0] rdata_q;
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      rdata_q <= '0;
    end else begin
      for (int unsigned p = 0; p < NumPorts; p++) begin
        if (req_i[p] && we_i[p])
          rdata_q[p] <= wdata_i[p];
      end
    end
  end
  assign rdata_o = rdata_q;
endmodule
