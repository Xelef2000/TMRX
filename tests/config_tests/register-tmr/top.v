module top(
    input wire clk_i,
    input wire rst_ni,
    input wire d,
    output wire y
);
    reg q;

    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            q <= 1'b0;
        else
            q <= d;
    end

    // This logic must remain a single shared instance in RegisterTMR mode.
    assign y = q ^ d;
endmodule
