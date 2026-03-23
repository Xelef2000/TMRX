// Simple 8-bit up-counter used as the fault injection test module.
// All state lives in count_q, which with LogicTMR becomes three independent
// copies (_a/_b/_c) with majority voters on the outputs.
module fi_counter (
    input  wire        clk_i,
    input  wire        rst_ni,
    input  wire        en_i,
    output wire [7:0]  count_o,
    (* tmrx_error_sink *)
    output wire        err_o
);
    reg [7:0] count_q;

    always @(posedge clk_i or negedge rst_ni)
        if (!rst_ni) count_q <= 8'h0;
        else if (en_i) count_q <= count_q + 8'h1;

    assign count_o = count_q;
    assign err_o   = 1'b0;
endmodule
