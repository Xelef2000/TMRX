module formal_top (
    input wire clk_i,
    input wire rst_ni,
    input wire en_i
);

    wire [31:0] gold_count_o;
    wire [31:0] gate_count_o;
    wire        gate_err_o;

    gold_top gold (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .en_i(en_i),
        .count_o(gold_count_o)
    );

    gate_top gate (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .en_i(en_i),
        .count_o(gate_count_o),
        .tmrx_err_o(gate_err_o)
    );

    always @* begin
        if ($initstate)
            assume (!rst_ni);
        else begin
            assume (rst_ni);
            assert (gate_count_o == gold_count_o);
            assert (!gate_err_o);
        end
    end
endmodule
