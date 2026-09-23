(* blackbox *)
module MOCK_DFFE (
    input  wire CLK,
    input  wire D,
    input  wire EN_N,
    output wire Q
);
endmodule

module top (
    input  wire clk_i,
    input  wire en_i,
    input  wire d_i,
    output wire builtin_q_o,
    output wire mapped_q_o
);
    reg builtin_q;

    always @(posedge clk_i)
        if (en_i)
            builtin_q <= d_i;

    MOCK_DFFE mapped_ff (
        .CLK(clk_i),
        .D(d_i),
        .EN_N(~en_i),
        .Q(mapped_q_o)
    );

    assign builtin_q_o = builtin_q;
endmodule
