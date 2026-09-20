// Simple synchronous FIFO buffer
// Parameterised data width and depth (depth must be a power of 2)
module top #(
    parameter DATA_WIDTH = 8,
    parameter DEPTH      = 16,
    parameter ADDR_WIDTH = $clog2(DEPTH)
) (
    input  wire                  clk_i,
    input  wire                  rst_ni,

    // Write port
    input  wire                  wr_en_i,
    input  wire [DATA_WIDTH-1:0] wr_data_i,
    output wire                  full_o,

    // Read port
    input  wire                  rd_en_i,
    output wire [DATA_WIDTH-1:0] rd_data_o,
    output wire                  empty_o
);
// tmrg default triplicate
// tmrg tmr_error true
// tmrg do_not_triplicate clk_i rst_ni wr_en_i wr_data_i full_o rd_en_i rd_data_o empty_o

    reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];
    reg [ADDR_WIDTH-1:0] wr_ptr;
    reg [ADDR_WIDTH-1:0] rd_ptr;
    reg [ADDR_WIDTH  :0] count;
    reg                  full_q;
    reg [DATA_WIDTH-1:0] rd_data_q;
    reg                  empty_q;

    wire [DATA_WIDTH-1:0] memVoted [0:DEPTH-1];
    wire [ADDR_WIDTH-1:0] wr_ptrVoted;
    wire [ADDR_WIDTH-1:0] rd_ptrVoted;
    wire [ADDR_WIDTH  :0] countVoted;
    wire                  full_qVoted;
    wire [DATA_WIDTH-1:0] rd_data_qVoted;
    wire                  empty_qVoted;

    genvar mem_vote_idx;
    generate
        for (mem_vote_idx = 0; mem_vote_idx < DEPTH; mem_vote_idx = mem_vote_idx + 1) begin : gen_mem_vote
            assign memVoted[mem_vote_idx] = mem[mem_vote_idx];
        end
    endgenerate

    assign wr_ptrVoted = wr_ptr;
    assign rd_ptrVoted = rd_ptr;
    assign countVoted = count;
    assign full_qVoted = full_q;
    assign rd_data_qVoted = rd_data_q;
    assign empty_qVoted = empty_q;

    assign full_o = full_qVoted;
    assign rd_data_o = rd_data_qVoted;
    assign empty_o = empty_qVoted;

    // Write logic
    always @(posedge clk_i) begin
        if (!rst_ni) begin
            wr_ptr <= '0;
        end else if (wr_en_i && !full_qVoted) begin
            mem[wr_ptrVoted] <= wr_data_i;
            wr_ptr           <= wr_ptrVoted + 1'b1;
        end
    end

    // Read logic
    always @(posedge clk_i) begin
        if (!rst_ni) begin
            rd_ptr    <= '0;
            rd_data_q <= '0;
        end else if (rd_en_i && !empty_qVoted) begin
            rd_data_q <= memVoted[rd_ptrVoted];
            rd_ptr    <= rd_ptrVoted + 1'b1;
        end
    end

    // Count tracking
    always @(posedge clk_i) begin
        if (!rst_ni) begin
            count <= '0;
        end else begin
            case ({wr_en_i && !full_qVoted, rd_en_i && !empty_qVoted})
                2'b10:   count <= countVoted + 1'b1;
                2'b01:   count <= countVoted - 1'b1;
                default: count <= countVoted;
            endcase
        end
    end

    // Status flags
    always @(posedge clk_i) begin
        if (!rst_ni) begin
            full_q  <= 1'b0;
            empty_q <= 1'b1;
        end else begin
            full_q  <= (countVoted == DEPTH[ADDR_WIDTH:0]);
            empty_q <= (countVoted == '0);
        end
    end

endmodule
