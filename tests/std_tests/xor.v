module full_adder(
    input wire a,
    input wire b,
    output wire out
    );

    wire res;

    assign res = a ^ b;

    assign out = res;

endmodule
