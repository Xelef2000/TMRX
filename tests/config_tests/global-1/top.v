module top(
    input wire clk_i,
    inout wire rst_ni,
    input wire input_a_i,
    input wire input_b_i,
    output wire out_o,
    output wire sub_module_err_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    wire signal_d = input_a_i & input_b_i;
    wire res_sub_1_d, res_sub_2_d, res_sub_3_d, res_bb_d;
    reg signal_q;

    wire submod_err_1, submod_err_3, submod_err_3_sub;

    submodule_1 s1(
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .input_a_i(signal_d),
        .out_o(res_sub_1_d),
        .err_o(submod_err_1)
    );

    submodule_2 s2(
        .input_a_i(input_b_i),
        .out_o(res_sub_2_d)
    );

    submodule_3 s3(
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .input_a_i(res_sub_2_d),
        .out_o(res_sub_3_d),
        .submod_err_o(submod_err_3_sub),
        .err_o(submod_err_3)
    );

    blackbox_1 bb(
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .input_a_i(input_a_i),
        .out_o(res_bb_d)
    );

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <=1'b0;
        else
            signal_q <=signal_d;
    end



    assign out_o = signal_q | (res_bb_d & res_sub_3_d) | res_sub_1_d;
    assign sub_module_err_o = submod_err_1 | submod_err_3 | submod_err_3_sub;

endmodule

module submodule_1(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    reg signal_q;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <=1'b0;
        else
            signal_q <=input_a_i;
    end

    assign out_o = signal_q;

endmodule

module submodule_2 (
    input wire input_a_i,
    output wire out_o
);

    assign out_o = !input_a_i;
endmodule

module submodule_3(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o,
    output wire submod_err_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    reg signal_q;
    wire signal_d, res_d, sub_err;
    assign signal_d = input_a_i ^ res_d;


    submodule_4 sub4 (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .input_a_i(signal_q),
        .out_o(res_d),
        .err_o(sub_err)
    );

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <=1'b0;
        else
            signal_q <= signal_d;
    end

    assign out_o = signal_q;
    assign submod_err_o = sub_err;


endmodule


module submodule_4(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o,
    (* tmrx_error_sink *)
    output wire err_o
);

    reg signal_q;
    wire signal_d;

    assign signal_d = input_a_i;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <=1'b0;
        else
            signal_q <= signal_d;
    end

    assign out_o = signal_q & signal_d;

endmodule

(* blackbox *)
module blackbox_1(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o
);

    reg signal_q;
    wire signal_d;

    assign signal_d = input_a_i;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <=1'b0;
        else
            signal_q <= signal_d;
    end

    assign out_o = signal_q | signal_d;

endmodule
