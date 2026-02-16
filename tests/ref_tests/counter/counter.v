module top (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire en_i,
    output reg [31:0] count_o
);

always @(posedge clk_i) begin
    if (!rst_ni)
        count_o <= 32'd0;
    else if (en_i)
        count_o <= count_o + 1'd1;
end

endmodule
