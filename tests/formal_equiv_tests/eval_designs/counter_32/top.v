module top (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire en_i,
    output wire [31:0] count_o
);
// tmrg default triplicate
// tmrg tmr_error true
// tmrg do_not_triplicate clk_i rst_ni en_i
// tmrg do_not_triplicate count_o

reg [31:0] count_q;
reg [31:0] count_d;
wire [31:0] count_qVoted;

assign count_qVoted = count_q;

always @(*) begin
    count_d = count_qVoted;
    if (!rst_ni)
        count_d = 32'd0;
    else if (en_i)
        count_d = count_qVoted + 1'd1;
end

always @(posedge clk_i) begin
    count_q <= count_d;
end

assign count_o = count_qVoted;

endmodule
