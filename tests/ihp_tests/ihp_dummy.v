module top (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire in0_i,
    input  wire in1_i,
    output wire out_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    reg sig_q;
    wire sig_d, res_y;

    assign sig_d = res_y ^ in1_i;


    submodule u_sub (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .a_i(in0_i),
        .b_i(sig_q),
        .y_o(res_y)
    );


    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            sig_q <= 1'b0;
        else
            sig_q <= sig_d;
    end

    assign out_o = sig_q;

endmodule



module submodule (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire a_i,
    input  wire b_i,
    output wire y_o
);

    reg q;

    wire d = (a_i & b_i) ^ q;

    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            q <= 1'b0;
        else
            q <= d;
    end

    assign y_o = q | a_i;

endmodule
