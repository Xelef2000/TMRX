module top (
    input  wire clk,
    input  wire rst,
    input  wire en,
    output reg [2:0] count
);

always @(posedge clk) begin
    if (rst)
        count <= 2'd0;
    else if (en)
        count <= count + 2'd1;
end

endmodule
