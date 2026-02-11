module dff_dual (
    input  wire clk,
    input  wire d,
    output wire q,
    output wire qn
);

    reg r;

    always @(posedge clk)
        r <= d;

    assign q  = r;
    assign qn = ~r;

endmodule
