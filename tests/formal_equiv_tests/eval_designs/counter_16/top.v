module top (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire en_i,
    output wire [15:0] count_o
);
// tmrg default triplicate
// tmrg tmr_error true
// tmrg do_not_triplicate clk_i rst_ni en_i
// tmrg do_not_triplicate count_o

logic [15:0] count_d, count_q, count_qVoted;
assign count_qVoted = count_q;

always_comb begin
    count_d = count_qVoted;
    if (!rst_ni)
        count_d = 16'd0;
    else if (en_i)
        count_d = count_qVoted + 1'd1;
end

always_ff @(posedge clk_i) begin
    count_q <= count_d;
end


assign count_o = count_qVoted;

endmodule
