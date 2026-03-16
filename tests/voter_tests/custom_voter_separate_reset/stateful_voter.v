// Stateful 1-bit majority voter: the error output is only asserted after
// 3 consecutive cycles in which at least two of the three inputs disagree.
//
// Clock and reset ports are connected by TMRX using either:
//   - The explicit config options tmr_voter_clock_port_name / tmr_voter_reset_port_name
//     (used by this test via tmrx_config.toml), or
//   - The (* tmrx_clk_port *) / (* tmrx_rst_port *) attributes on the ports (fallback).
module stateful_voter (
    (* tmrx_clk_port *) input wire clk_i,
    (* tmrx_rst_port *) input wire rst_ni,
    input wire a,
    input wire b,
    input wire c,
    output wire y,
    output wire err
);
    // Majority vote
    assign y = (a & b) | (a & c) | (b & c);

    // Disagreement: at least one pair of inputs differs
    wire dis = (a ^ b) | (b ^ c);

    // Saturating 2-bit counter tracking consecutive disagreement cycles.
    // Asynchronous reset so dfflibmap routes rst_ni to the hardware RESET_B pin
    // rather than burying it in the D-input mux.
    reg [1:0] cnt;
    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            cnt <= 2'd0;
        else if (dis)
            cnt <= (cnt == 2'd3) ? 2'd3 : cnt + 2'd1;
        else
            cnt <= 2'd0;
    end

    // Error only after 3 consecutive disagreements
    assign err = (cnt == 2'd3);
endmodule
