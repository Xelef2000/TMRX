// Simple 1-bit majority voter for use as a custom TMRX voter cell.
// Interface matches the TMRX voter contract:
//   a, b, c  – three redundant copies of the 1-bit input
//   y        – majority-voted output
//   err      – set when any two copies disagree (fault detected)
module simple_voter (
    input  wire a,
    input  wire b,
    input  wire c,
    output wire y,
    output wire err
);
    assign y   = (a & b) | (a & c) | (b & c);
    assign err = (a ^ b) | (b ^ c);
endmodule
