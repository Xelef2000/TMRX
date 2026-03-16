// Simple 2-bit counter used as the DUT for the custom voter test.
// The multi-bit output exercises the per-bit voter insertion path
// (two 1-bit simple_voter instances are expected on count_o).
module top (
    input  wire       clk_i,
    input  wire       rst_ni,
    input  wire       en_i,
    output reg  [1:0] count_o,
    (* tmrx_error_sink *)
    output wire       err_o
);
    always @(posedge clk_i) begin
        if (!rst_ni)
            count_o <= 2'd0;
        else if (en_i)
            count_o <= count_o + 1'd1;
    end
endmodule
