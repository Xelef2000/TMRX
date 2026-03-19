// Inner module: contains the flip-flop logic.
// TMRX triplicates the internals of this module.
// The 'initial' blocks set init=0 on all FFs; TMRX propagates this attribute
// to all three TMR copies so SymbiYosys has a fixed starting state for BMC.
module simple_ff_core (
    input  wire clk,
    input  wire rst,  // synchronous reset, active-high
    input  wire d,
    output reg  q1,
    output reg  q2
);
    initial begin
        q1 = 1'b0;
        q2 = 1'b0;
    end

    always @(posedge clk) begin
        if (rst) begin
            q1 <= 1'b0;
            q2 <= 1'b0;
        end else begin
            q1 <= d;
            q2 <= q1;
        end
    end
endmodule

// Thin wrapper: the TMR target.
// preserve_module_ports = true keeps clk/rst/d/q1/q2 intact after TMR,
// so gold_mod and gate_mod have identical port signatures for miter -equiv.
module simple_ff (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output wire q1,
    output wire q2
);
    simple_ff_core u_core (
        .clk(clk),
        .rst(rst),
        .d(d),
        .q1(q1),
        .q2(q2)
    );
endmodule
