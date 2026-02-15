module level5 (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output reg  q
);
    always @(posedge clk or posedge rst) begin
        if (rst)
            q <= 1'b0;
        else
            q <= d;
    end
endmodule


module level4 (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output wire q
);
    reg stage;

    always @(posedge clk or posedge rst) begin
        if (rst)
            stage <= 1'b0;
        else
            stage <= d ^ 1'b1;
    end

    level5 u5 (
        .clk(clk),
        .rst(rst),
        .d(stage),
        .q(q)
    );
endmodule


module level3 (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output wire q
);
    reg [1:0] counter;

    always @(posedge clk or posedge rst) begin
        if (rst)
            counter <= 2'b00;
        else
            counter <= counter + 1'b1;
    end

    level4 u4 (
        .clk(clk),
        .rst(rst),
        .d(d ^ counter[0]),
        .q(q)
    );
endmodule


module level2 (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output wire q
);
    reg stage;

    always @(posedge clk or posedge rst) begin
        if (rst)
            stage <= 1'b0;
        else
            stage <= d;
    end

    level3 u3 (
        .clk(clk),
        .rst(rst),
        .d(stage),
        .q(q)
    );
endmodule


module level1 (
    input  wire clk,
    input  wire rst,
    input  wire d,
    output wire q
);
    reg stage;

    always @(posedge clk or posedge rst) begin
        if (rst)
            stage <= 1'b0;
        else
            stage <= ~d;
    end

    level2 u2 (
        .clk(clk),
        .rst(rst),
        .d(stage),
        .q(q)
    );
endmodule
