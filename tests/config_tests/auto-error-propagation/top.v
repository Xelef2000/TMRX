module top (
    input wire a_i,
    input wire b_i
);
    (* keep *) wire unused_y;

    (* keep *) child u_child (
        .a_i(a_i),
        .b_i(b_i),
        .y_o(unused_y)
    );
endmodule

module child (
    input wire a_i,
    input wire b_i,
    output wire y_o
);
    assign y_o = a_i & b_i;
endmodule
