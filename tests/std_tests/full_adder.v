module full_adder(
    input [3:0] a,
    input [3:0] b,
    output [3:0] out
    );
    
    wire [3:0] res;

    assign res = a + b;

    assign out = res;

endmodule

 
