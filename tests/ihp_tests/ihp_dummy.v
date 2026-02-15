module top (
    input  wire clk,
    input  wire rst,
    input  wire in0,
    input  wire in1,
    output wire out,
    (* tmrx_error_sink *)
    output wire err
);

    reg state;

    wire sub_y;

    submodule u_sub (
        .clk(clk),
        .rst(rst),
        .a(in0),
        .b(state),
        .y(sub_y)
    );

    wire next_state = sub_y ^ in1;

    always @(posedge clk or posedge rst) begin
        if (rst)
            state <= 1'b0;
        else
            state <= next_state;
    end

    assign out = state;

endmodule



module submodule (
    input  wire clk,
    input  wire rst,
    input  wire a,
    input  wire b,
    output wire y
);

    reg r;

    wire next_r = (a & b) ^ r;

    always @(posedge clk or posedge rst) begin
        if (rst)
            r <= 1'b0;
        else
            r <= next_r;
    end

    assign y = r | a;

endmodule
