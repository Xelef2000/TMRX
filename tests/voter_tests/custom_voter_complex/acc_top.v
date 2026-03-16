// Saturating 4-bit accumulator used as the DUT for the complex custom voter test.
// Features: load, enable, clear, saturation detection, and async-free synchronous reset.
// The 4-bit acc_o exercises four 1-bit voter instances; sat_o is combinational.
module top (
    input  wire       clk_i,
    input  wire       rst_ni,
    input  wire [3:0] data_i,
    input  wire       load_i,
    input  wire       en_i,
    input  wire       clear_i,
    output reg  [3:0] acc_o,
    output wire       sat_o,
    (* tmrx_error_sink *)
    output wire       err_o
);
    wire [4:0] next_sum     = {1'b0, acc_o} + 5'd1;
    wire       will_overflow = next_sum[4];

    always @(posedge clk_i) begin
        if (!rst_ni || clear_i)
            acc_o <= 4'd0;
        else if (load_i)
            acc_o <= data_i;
        else if (en_i)
            acc_o <= will_overflow ? 4'hF : next_sum[3:0];
    end

    // Saturated when all bits are 1
    assign sat_o = &acc_o;
endmodule
