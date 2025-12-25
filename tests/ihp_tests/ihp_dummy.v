module top(input clk, rst, input [7:0] a, b, output [7:0] out);
    wire [7:0] count;
    wire [7:0] alu_res;
    
    counter c0 (.clk(clk), .rst(rst), .val(count));
    alu a0 (.op_a(a), .op_b(b), .mode(count[0]), .res(alu_res));
    
    assign out = alu_res;
endmodule

module counter(input clk, rst, output reg [7:0] val);
    always @(posedge clk) begin
        if (rst) val <= 0;
        else val <= val + 1;
    end
endmodule

module alu(input [7:0] op_a, op_b, input mode, output [7:0] res);
    assign res = mode ? (op_a + op_b) : (op_a - op_b);
endmodule