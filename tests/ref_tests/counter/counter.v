module top (
    input  wire clk,
    input  wire rst,
    input  wire en,
    output reg [31:0] count
);

always @(posedge clk) begin
    if (rst)
        count <= 32'd0;
    else if (en)
        count <= count + 1'd1;
end

endmodule
