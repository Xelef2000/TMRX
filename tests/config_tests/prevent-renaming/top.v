module top (
    input wire clk_i,
    input wire rst_ni,
    input wire d_i,
    output wire q_o
);
    wire child_q;

    child u_child (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .d_i(d_i),
        .q_o(child_q)
    );

    assign q_o = child_q;
endmodule

module child (
    input wire clk_i,
    input wire rst_ni,
    input wire d_i,
    output wire q_o
);
    reg q_q;

    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            q_q <= 1'b0;
        else
            q_q <= d_i;
    end

    assign q_o = q_q;
endmodule
