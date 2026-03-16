// 4-bit counter with two independent reset nets:
//   rst_ni       - resets the data flip-flops (in reset_port_names)
//   voter_rst_ni - separate reset wired only to the voter FFs via tmr_voter_reset_net
//
// This lets the test verify that tmr_voter_reset_net explicitly overrides the
// default "first reset port" auto-detection, routing voter reset independently
// from the main data-path reset.
module top (
    input  wire       clk_i,
    input  wire       rst_ni,
    input  wire       voter_rst_ni,
    input  wire       en_i,
    output reg  [3:0] count_o,
    (* tmrx_error_sink *)
    output wire       err_o
);
    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            count_o <= 4'd0;
        else if (en_i)
            count_o <= count_o + 4'd1;
    end
endmodule
