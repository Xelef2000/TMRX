module formal_top (
    input wire       clk_i,
    input wire       rst_ni,
    input wire [7:0] i_TX_Byte,
    input wire       i_TX_DV,
    input wire       i_SPI_MISO
);

    wire       gold_o_TX_Ready;
    wire       gold_o_RX_DV;
    wire [7:0] gold_o_RX_Byte;
    wire       gold_o_SPclk_i;
    wire       gold_o_SPI_MOSI;
    wire       gate_o_TX_Ready;
    wire       gate_o_RX_DV;
    wire [7:0] gate_o_RX_Byte;
    wire       gate_o_SPclk_i;
    wire       gate_o_SPI_MOSI;
    wire       gate_err_o;

    wire [1:0] gold_r_SPclk_i_Count;
    wire       gold_r_SPclk_i;
    wire [4:0] gold_r_SPclk_i_Edges;
    wire       gold_r_Leading_Edge;
    wire       gold_r_Trailing_Edge;
    wire       gold_r_TX_DV;
    wire [7:0] gold_r_TX_Byte;
    wire [2:0] gold_r_RX_Bit_Count;
    wire [2:0] gold_r_TX_Bit_Count;
    wire       gold_o_TX_Ready_q;
    wire       gold_o_RX_DV_q;
    wire [7:0] gold_o_RX_Byte_q;
    wire       gold_o_SPclk_i_q;
    wire       gold_o_SPI_MOSI_q;

    wire [1:0] gate_r_SPclk_i_Count;
    wire       gate_r_SPclk_i;
    wire [4:0] gate_r_SPclk_i_Edges;
    wire       gate_r_Leading_Edge;
    wire       gate_r_Trailing_Edge;
    wire       gate_r_TX_DV;
    wire [7:0] gate_r_TX_Byte;
    wire [2:0] gate_r_RX_Bit_Count;
    wire [2:0] gate_r_TX_Bit_Count;
    wire       gate_o_TX_Ready_q;
    wire       gate_o_RX_DV_q;
    wire [7:0] gate_o_RX_Byte_q;
    wire       gate_o_SPclk_i_q;
    wire       gate_o_SPI_MOSI_q;

    gold_top gold (
        .rst_ni(rst_ni),
        .clk_i(clk_i),
        .i_TX_Byte(i_TX_Byte),
        .i_TX_DV(i_TX_DV),
        .o_TX_Ready(gold_o_TX_Ready),
        .o_RX_DV(gold_o_RX_DV),
        .o_RX_Byte(gold_o_RX_Byte),
        .o_SPclk_i(gold_o_SPclk_i),
        .i_SPI_MISO(i_SPI_MISO),
        .o_SPI_MOSI(gold_o_SPI_MOSI),
        .r_SPclk_i_CountVoted(gold_r_SPclk_i_Count),
        .r_SPclk_iVoted(gold_r_SPclk_i),
        .r_SPclk_i_EdgesVoted(gold_r_SPclk_i_Edges),
        .r_Leading_EdgeVoted(gold_r_Leading_Edge),
        .r_Trailing_EdgeVoted(gold_r_Trailing_Edge),
        .r_TX_DVVoted(gold_r_TX_DV),
        .r_TX_ByteVoted(gold_r_TX_Byte),
        .r_RX_Bit_CountVoted(gold_r_RX_Bit_Count),
        .r_TX_Bit_CountVoted(gold_r_TX_Bit_Count),
        .o_TX_Ready_qVoted(gold_o_TX_Ready_q),
        .o_RX_DV_qVoted(gold_o_RX_DV_q),
        .o_RX_Byte_qVoted(gold_o_RX_Byte_q),
        .o_SPclk_i_qVoted(gold_o_SPclk_i_q),
        .o_SPI_MOSI_qVoted(gold_o_SPI_MOSI_q)
    );

    gate_top gate (
        .rst_ni(rst_ni),
        .clk_i(clk_i),
        .i_TX_Byte(i_TX_Byte),
        .i_TX_DV(i_TX_DV),
        .o_TX_Ready(gate_o_TX_Ready),
        .o_RX_DV(gate_o_RX_DV),
        .o_RX_Byte(gate_o_RX_Byte),
        .o_SPclk_i(gate_o_SPclk_i),
        .i_SPI_MISO(i_SPI_MISO),
        .o_SPI_MOSI(gate_o_SPI_MOSI),
        .tmrx_err_o(gate_err_o),
        .r_SPclk_i_CountVoted_a(gate_r_SPclk_i_Count),
        .r_SPclk_iVoted_a(gate_r_SPclk_i),
        .r_SPclk_i_EdgesVoted_a(gate_r_SPclk_i_Edges),
        .r_Leading_EdgeVoted_a(gate_r_Leading_Edge),
        .r_Trailing_EdgeVoted_a(gate_r_Trailing_Edge),
        .r_TX_DVVoted_a(gate_r_TX_DV),
        .r_TX_ByteVoted_a(gate_r_TX_Byte),
        .r_RX_Bit_CountVoted_a(gate_r_RX_Bit_Count),
        .r_TX_Bit_CountVoted_a(gate_r_TX_Bit_Count),
        .o_TX_Ready_qVoted_a(gate_o_TX_Ready_q),
        .o_RX_DV_qVoted_a(gate_o_RX_DV_q),
        .o_RX_Byte_qVoted_a(gate_o_RX_Byte_q),
        .o_SPclk_i_qVoted_a(gate_o_SPclk_i_q),
        .o_SPI_MOSI_qVoted_a(gate_o_SPI_MOSI_q)
    );

    always @* begin
        if ($initstate)
            assume (!rst_ni);
        else begin
            assume (rst_ni);
            assert (gate_r_SPclk_i_Count == gold_r_SPclk_i_Count);
            assert (gate_r_SPclk_i == gold_r_SPclk_i);
            assert (gate_r_SPclk_i_Edges == gold_r_SPclk_i_Edges);
            assert (gate_r_Leading_Edge == gold_r_Leading_Edge);
            assert (gate_r_Trailing_Edge == gold_r_Trailing_Edge);
            assert (gate_r_TX_DV == gold_r_TX_DV);
            assert (gate_r_TX_Byte == gold_r_TX_Byte);
            assert (gate_r_RX_Bit_Count == gold_r_RX_Bit_Count);
            assert (gate_r_TX_Bit_Count == gold_r_TX_Bit_Count);
            assert (gate_o_TX_Ready_q == gold_o_TX_Ready_q);
            assert (gate_o_RX_DV_q == gold_o_RX_DV_q);
            assert (gate_o_RX_Byte_q == gold_o_RX_Byte_q);
            assert (gate_o_SPclk_i_q == gold_o_SPclk_i_q);
            assert (gate_o_SPI_MOSI_q == gold_o_SPI_MOSI_q);
            assert (gate_o_TX_Ready == gold_o_TX_Ready);
            assert (gate_o_RX_DV == gold_o_RX_DV);
            assert (gate_o_RX_Byte == gold_o_RX_Byte);
            assert (gate_o_SPclk_i == gold_o_SPclk_i);
            assert (gate_o_SPI_MOSI == gold_o_SPI_MOSI);
            assert (!gate_err_o);
        end
    end
endmodule
