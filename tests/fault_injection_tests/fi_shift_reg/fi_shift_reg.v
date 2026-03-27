// 8-bit rotate-left shift register used as a fault injection test module.
// All state lives in shift_q, which with LogicTMR becomes three independent
// copies (_a/_b/_c) with majority voters on the outputs.
//
// After reset, shift_q = 0x01.  Each enabled clock rotates the value left
// by one bit, so after N ticks: data_o = 0x01 << (N % 8).
module fi_shift_reg (
    input  wire        clk_i,
    input  wire        rst_ni,
    input  wire        en_i,
    output wire [7:0]  data_o,
    (* tmrx_error_sink *)
    output wire        err_o
);
    reg [7:0] shift_q;

    always @(posedge clk_i or negedge rst_ni)
        if (!rst_ni) shift_q <= 8'h01;
        else if (en_i) shift_q <= {shift_q[6:0], shift_q[7]};

    assign data_o = shift_q;
    assign err_o  = 1'b0;
endmodule
