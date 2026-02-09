module full_adder(
    input a,
    input b,
    output out
    );
    
    wire res;

    assign res = a ^ b;

    assign out = res;

endmodule

 
