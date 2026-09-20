module formal_top (
    input wire       clk_i,
    input wire       rst_ni,
    input wire       wr_en_i,
    input wire [7:0] wr_data_i,
    input wire       rd_en_i
);

    wire       gold_full_o;
    wire [7:0] gold_rd_data_o;
    wire       gold_empty_o;
    wire       gate_full_o;
    wire [7:0] gate_rd_data_o;
    wire       gate_empty_o;
    wire       gate_err_o;

    wire [3:0] gold_wr_ptr;
    wire [3:0] gold_rd_ptr;
    wire [4:0] gold_count;
    wire [127:0] gold_mem;
    wire       gold_full_q;
    wire [7:0] gold_rd_data_q;
    wire       gold_empty_q;
    wire [3:0] gate_wr_ptr;
    wire [3:0] gate_rd_ptr;
    wire [4:0] gate_count;
    wire [127:0] gate_mem;
    wire       gate_full_q;
    wire [7:0] gate_rd_data_q;
    wire       gate_empty_q;

    gold_top gold (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .wr_en_i(wr_en_i),
        .wr_data_i(wr_data_i),
        .full_o(gold_full_o),
        .rd_en_i(rd_en_i),
        .rd_data_o(gold_rd_data_o),
        .empty_o(gold_empty_o),
        .wr_ptrVoted(gold_wr_ptr),
        .rd_ptrVoted(gold_rd_ptr),
        .countVoted(gold_count),
        .memVoted(gold_mem),
        .full_qVoted(gold_full_q),
        .rd_data_qVoted(gold_rd_data_q),
        .empty_qVoted(gold_empty_q)
    );

    gate_top gate (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .wr_en_i(wr_en_i),
        .wr_data_i(wr_data_i),
        .full_o(gate_full_o),
        .rd_en_i(rd_en_i),
        .rd_data_o(gate_rd_data_o),
        .empty_o(gate_empty_o),
        .tmrx_err_o(gate_err_o),
        .wr_ptrVoted_a(gate_wr_ptr),
        .rd_ptrVoted_a(gate_rd_ptr),
        .countVoted_a(gate_count),
        .memVoted_a(gate_mem),
        .full_qVoted_a(gate_full_q),
        .rd_data_qVoted_a(gate_rd_data_q),
        .empty_qVoted_a(gate_empty_q)
    );

    always @* begin
        if ($initstate) begin
            assume (!rst_ni);
            assume (gate_mem == gold_mem);
        end else begin
            assume (rst_ni);
            assert (gate_wr_ptr == gold_wr_ptr);
            assert (gate_rd_ptr == gold_rd_ptr);
            assert (gate_count == gold_count);
            assert (gate_mem == gold_mem);
            assert (gate_full_q == gold_full_q);
            assert (gate_rd_data_q == gold_rd_data_q);
            assert (gate_empty_q == gold_empty_q);
            assert (gate_full_o == gold_full_o);
            assert (gate_rd_data_o == gold_rd_data_o);
            assert (gate_empty_o == gold_empty_o);
        end
    end
endmodule
