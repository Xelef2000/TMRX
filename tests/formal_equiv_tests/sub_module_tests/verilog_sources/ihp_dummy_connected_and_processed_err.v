module top_impl (
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

    reg nsig_q;
    wire nclk;

    initial begin
        sig_q  = 1'b0;
        nsig_q = 1'b0;
    end

    assign sig_d = res_y ^ in1_i;
    wire sub_err;

    assign err_o = !sub_err;


    submodule u_sub (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .a_i(in0_i),
        .b_i(sig_q),
        .y_o(res_y),
        .err_o(sub_err)
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
    output wire y_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    reg q;
    initial q = 1'b0;

    wire d = (a_i & b_i) ^ q;

    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            q <= 1'b0;
        else
            q <= d;
    end

    assign y_o = q | a_i;

endmodule


// err_o excluded from top ports: tmrx_error_sink, not part of functional equivalence.
module top (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire in0_i,
    input  wire in1_i,
    output wire out_o
);
    wire err_nc;
    top_impl u_top (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .in0_i(in0_i),
        .in1_i(in1_i),
        .out_o(out_o),
        .err_o(err_nc)
    );
endmodule
