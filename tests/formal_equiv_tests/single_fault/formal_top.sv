module formal_top (
    input wire        clk_i,
    input wire        rst_ni,
    input wire        en_i,
    input wire        fault_active,
    input wire [1:0]  fault_lane,
    input wire [7:0]  fault_mask
);

    wire [7:0] gold_count_o;
    wire       gold_err_o;
    wire [7:0] tmr_count_o;
    wire       tmr_err_o;

    wire [7:0] lane_a_raw;
    wire [7:0] lane_b_raw;
    wire [7:0] lane_c_raw;

    wire lane_a_fault = fault_active && fault_lane == 2'd0;
    wire lane_b_fault = fault_active && fault_lane == 2'd1;
    wire lane_c_fault = fault_active && fault_lane == 2'd2;
    wire fault_present = (lane_a_fault || lane_b_fault || lane_c_fault) && |fault_mask;

    fi_counter_gold gold (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .en_i(en_i),
        .count_o(gold_count_o),
        .err_o(gold_err_o)
    );

    fi_counter_tmr gate (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .en_i(en_i),
        .count_o(tmr_count_o),
        .err_o(tmr_err_o),
        .fault_lane_a_0(lane_a_raw),
        .fault_lane_a_0__cuti(lane_a_raw ^ (lane_a_fault ? fault_mask : 8'b0)),
        .fault_lane_b_0(lane_b_raw),
        .fault_lane_b_0__cuti(lane_b_raw ^ (lane_b_fault ? fault_mask : 8'b0)),
        .fault_lane_c_0(lane_c_raw),
        .fault_lane_c_0__cuti(lane_c_raw ^ (lane_c_fault ? fault_mask : 8'b0))
    );

    always @* begin
        if ($initstate)
            assume (!rst_ni);
        else
            assume (rst_ni);

        if (!$initstate) begin
            assert (lane_a_raw == gold_count_o);
            assert (lane_b_raw == gold_count_o);
            assert (lane_c_raw == gold_count_o);
            assert (tmr_count_o == gold_count_o);
            assert (tmr_err_o == fault_present);
        end
    end
endmodule
