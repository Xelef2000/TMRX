module full_adder(
    input [1:0] a,
    input [1:0] b,
    output [1:0] out
    );
    
    wire [1:0] res;

    assign res = a + b;

    assign out = res;

endmodule

 
