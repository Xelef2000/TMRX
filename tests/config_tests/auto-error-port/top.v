// Simple counter with no (* tmrx_error_sink *) annotation.
// Used to test that auto_error_port = true creates tmrx_err_o automatically.
module top (
    input  wire        clk_i,
    input  wire        rst_ni,
    input  wire        en_i,
    output wire [7:0]  count_o
);
    reg [7:0] count_q;

    always @(posedge clk_i or negedge rst_ni)
        if (!rst_ni) count_q <= 8'h0;
        else if (en_i) count_q <= count_q + 8'h1;

    assign count_o = count_q;
endmodule
