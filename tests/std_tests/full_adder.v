module full_adder(
    input wire [1:0] a,
    input wire [1:0] b,
    output wire [1:0] out
    );

    wire [1:0] res;

    assign res = a + b;

    assign out = res;

endmodule
