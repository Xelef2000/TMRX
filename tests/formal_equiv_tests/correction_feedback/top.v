module top (
    input  wire       clk_i,
    input  wire       en_i,
    input  wire [7:0] data_i,
    output wire [7:0] state_o,
    (* tmrx_error_sink *)
    output wire       err_o
);
    reg [7:0] state_q;

    always @(posedge clk_i)
        if (en_i)
            state_q <= data_i;

    assign state_o = state_q;
    assign err_o = 1'b0;
endmodule
