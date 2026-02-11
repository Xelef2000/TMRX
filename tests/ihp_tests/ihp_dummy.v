module top(
    input  wire clk,
    input  wire d,
    output wire q,
    output wire qn,
    (* tmrx_error_sink *)
    output wire err
);

    reg r;

    always @(posedge clk)
        r <= d;

    assign q  = r;
    assign qn = ~r;

endmodule
