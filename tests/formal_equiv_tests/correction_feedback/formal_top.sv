module formal_top (
    input wire       clk_i,
    input wire       en_i,
    input wire [7:0] data_i,
    input wire [1:0] fault_lane
);
    wire [7:0] gold_state;
    wire       gold_error;
    wire [7:0] gate_state;
    wire       gate_error;
    wire [7:0] lane_a_raw;
    wire [7:0] lane_b_raw;
    wire [7:0] lane_c_raw;
    wire [7:0] lane_a_data;
    wire [7:0] lane_b_data;
    wire [7:0] lane_c_data;
    reg        previous_clk;

    always @($global_clock)
        previous_clk <= clk_i;

    gold_top gold (
        .clk_i(clk_i),
        .en_i(en_i),
        .data_i(data_i),
        .state_o(gold_state),
        .err_o(gold_error)
    );

    gate_top gate (
        .clk_i(clk_i),
        .en_i(en_i),
        .data_i(data_i),
        .state_o(gate_state),
        .err_o(gate_error),
        .feedback_lane_a_0(lane_a_raw),
        .feedback_lane_b_0(lane_b_raw),
        .feedback_lane_c_0(lane_c_raw),
        .feedback_data_a_0(lane_a_data),
        .feedback_data_b_0(lane_b_data),
        .feedback_data_c_0(lane_c_data)
    );

    always @* begin
        if ($initstate) begin
            assume (!clk_i);
            assume (fault_lane < 2'd3);
            if (fault_lane != 2'd0)
                assume (lane_a_raw == gold_state);
            if (fault_lane != 2'd1)
                assume (lane_b_raw == gold_state);
            if (fault_lane != 2'd2)
                assume (lane_c_raw == gold_state);
        end else begin
            assume (clk_i != previous_clk);
            assert (lane_a_raw == gold_state);
            assert (lane_b_raw == gold_state);
            assert (lane_c_raw == gold_state);
        end

        assert (gate_state == gold_state);
        if (en_i) begin
            assert (lane_a_data == data_i);
            assert (lane_b_data == data_i);
            assert (lane_c_data == data_i);
        end else begin
            assert (lane_a_data == gate_state);
            assert (lane_b_data == gate_state);
            assert (lane_c_data == gate_state);
        end
        assert (gate_error == ((lane_a_raw != lane_b_raw) ||
                               (lane_a_raw != lane_c_raw) ||
                               (lane_b_raw != lane_c_raw)));
    end
endmodule
