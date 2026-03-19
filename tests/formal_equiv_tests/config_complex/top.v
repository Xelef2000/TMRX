// Adapted from tests/config_tests/global-1/top.v for formal equivalence:
//   - blackbox_1 (* blackbox *) attribute removed so the solver can see inside
//   - top rst_ni changed from inout to input
//   - initial blocks added to every FF so TMRX propagates init=0 to all copies
// err_o and sub_module_err_o removed from ports:
//   - err_o is a tmrx_error_sink: undriven in original RTL, driven after TMR.
//   - sub_module_err_o aggregates inner submodule error sinks which are also
//     undriven in original RTL, so it is undefined (x) in gold but 0 in gate.
// Both are wired to internal signals so the miter only checks out_o.
module top(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    input wire input_b_i,
    output wire out_o
);

    wire signal_d = input_a_i & input_b_i;
    wire res_sub_1_d, res_sub_2_d, res_sub_3_d, res_bb_d;
    reg signal_q;
    initial signal_q = 1'b0;

    wire err_nc;           // err_o wired internally, not checked in equivalence
    wire sub_err_nc;       // sub_module_err_o wired internally, same reason
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
            signal_q <= 1'b0;
        else
            signal_q <= signal_d;
    end

    assign err_nc     = 1'b0;
    assign sub_err_nc = submod_err_1 | submod_err_3 | submod_err_3_sub;
    assign out_o = signal_q | (res_bb_d & res_sub_3_d) | res_sub_1_d;

endmodule

(* tmrx_logic_path_1_suffix = "_aa" *)
module submodule_1(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o,
    (* tmrx_error_sink *)
    output wire err_o
);
    reg signal_q;
    initial signal_q = 1'b0;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <= 1'b0;
        else
            signal_q <= input_a_i;
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
    initial signal_q = 1'b0;

    wire signal_d, res_d, inv_d, sub_err;
    assign signal_d = input_a_i ^ res_d;

    submodule_4 sub4 (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .input_a_i(signal_q),
        .out_o(res_d),
        .err_o(sub_err)
    );

    submodule_2 sub2 (
        .input_a_i(signal_q),
        .out_o(inv_d)
    );

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <= 1'b0;
        else
            signal_q <= signal_d | (res_d ^ inv_d);
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
    initial signal_q = 1'b0;

    wire signal_d;
    assign signal_d = input_a_i;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <= 1'b0;
        else
            signal_q <= signal_d;
    end

    assign out_o = signal_q & signal_d;

endmodule

// blackbox_1: (* blackbox *) removed so formal can verify the internals.
module blackbox_1(
    input wire clk_i,
    input wire rst_ni,
    input wire input_a_i,
    output wire out_o
);
    reg signal_q;
    initial signal_q = 1'b0;

    wire signal_d;
    assign signal_d = input_a_i;

    always @(posedge clk_i or negedge rst_ni) begin
        if(!rst_ni)
            signal_q <= 1'b0;
        else
            signal_q <= signal_d;
    end

    assign out_o = signal_q | signal_d;

endmodule
