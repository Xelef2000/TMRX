module one_ff_with_logic (
    input  wire clk,
    input  wire rst,
    input  wire a,
    input  wire b,
    output wire y,
    output wire x
);

    reg q;

    always @(posedge clk) begin
        if (rst)
            q <= 1'b0;
        else
            q <= a ^ b;
    end

    assign y = q & a;
    assign x = q ^ a;

endmodule
