// a simple chunk of memory that you can fill with samples on the write side
// and then allows playing back in order on the read side
// (both sides work on different clocks)
module player(r_clk, r_reset_n, r_out, r_done, w_clk, w_enable, w_addr, w_in);
    parameter timeBits = 10;

    // read: clock, reset, and output, and a done flag
    input r_clk;
    input r_reset_n;
    output reg [31:0] r_out;
    output r_done;

    // the internal read cursor, with an extra bit
    // when this bit is set, we are done playing
    reg [timeBits:0] r_addr = 1 << timeBits;
    assign r_done = r_addr[timeBits];

    // write: clock, enable, address, output
    input w_clk;
    input w_enable;
    input [timeBits-1:0] w_addr;
    input [31:0] w_in;

    // our memory
    reg [31:0] memory [(2**timeBits)-1:0];

    // read side
    always @(posedge r_clk)
    begin
        // if we're not reset, and we're playing...
        if (r_reset_n && !r_done)
        begin
            r_out <= memory[r_addr[timeBits-1:0]];
            r_addr <= r_addr + 1;
        end

        // if we're reset
        if (!r_reset_n)
        begin
            r_out <= memory[0];
            r_addr <= 0;
        end
    end

    // write side
    always @(posedge w_clk)
    begin
        if (w_enable)
            memory[w_addr] <= w_in;
    end
endmodule

// wrapping the above in a nice qsys-friendly package
module qsys_player
    #(parameter outputBits = 32,
      parameter words_log_2 = 0,
      parameter words = 1,
      parameter timeBits = 10
      )
    (// read side
     input r_clk,
     output [outputBits-1:0] r_out,
     output r_reset_n,
     input r_enable,
                    
     // write side
     input clk,
     input reset_n,
     input buffer_write,
     input [timeBits + words_log_2 - 1:0] buffer_address,
     input [31:0] buffer_writedata,

     // control
     input csr_write,
     input [31:0] csr_writedata,
     input csr_read,
     output reg [31:0] csr_readdata,
     output reg irq = 0
     );

    // other inputs to the sampler, driven elsewhere
    wire [timeBits-1:0] w_addr;
    wire [words-1:0] w_enable;
    wire [words-1:0] r_dones;
    wire r_done = r_dones[0];
    reg csr_enable = 0;

    // our r_reset_n is driven by both the csr_enable and r_enable
    assign r_reset_n = csr_enable || r_enable;

    // control
    // bits, least significant to most
    // - reset_n (rw)
    // - done (ro)
    // - irq (rw -- can only set to 0)
    
    reg old_done = 0;
    always @(posedge clk)
    begin
        if (csr_write)
        begin
            csr_enable <= csr_writedata[0];
            irq <= 0;
        end
        else if (csr_read)
        begin
            csr_readdata[0] <= csr_enable;
            csr_readdata[1] <= r_done;
            csr_readdata[2] <= irq;
        end

        // fire irq when we finish
        if (old_done == 0 && r_done == 1)
            irq <= 1;
        old_done <= r_done;

        // if reset, then reset our reset (eww)
        if (!reset_n)
        begin
            csr_enable <= 0;
            old_done <= 0;
            irq <= 0;
        end
    end

    // write
    assign w_addr = buffer_address >> words_log_2;
    generate
        if (words_log_2 > 0)
            assign w_enable = buffer_write << buffer_address[words_log_2-1:0];
        else
            assign w_enable = buffer_write;
    endgenerate
    
    genvar i;
    generate
        for (i = 0; i < words; i = i + 1)
        begin : players
            player #(timeBits) p(r_clk, r_reset_n, r_out[((i == words-1) ? (outputBits-1) : (32*i+31)):32*i], r_dones[i], clk, w_enable[i], w_addr, buffer_writedata);
        end
    endgenerate
endmodule
