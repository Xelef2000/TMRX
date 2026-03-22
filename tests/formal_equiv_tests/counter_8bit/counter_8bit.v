// Inner module: 8-bit counter with async active-low reset.
// 'initial' blocks fix the FF initial state to 0; TMRX propagates this
// init=0 attribute to all three TMR copies, giving SymbiYosys a concrete
// starting point for BMC.
module counter_8bit_core (
    input  wire       clk_i,
    input  wire       rst_ni,
    input  wire       en_i,
    output reg  [7:0] count_o
);
    initial count_o = 8'h00;

    always @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            count_o <= 8'h00;
        else if (en_i)
            count_o <= count_o + 8'h01;
    end
endmodule

// Thin wrapper – TMR target with preserved ports.
module counter_8bit (
    input  wire       clk_i,
    input  wire       rst_ni,
    input  wire       en_i,
    output wire [7:0] count_o
);
    counter_8bit_core u_core (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .en_i(en_i),
        .count_o(count_o)
    );
endmodule
