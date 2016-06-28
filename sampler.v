// a simple bit of memory that fills itself with samples from the write side
// and then allows reading out on the read side
// (both sides work on different clocks)
module sampler(w_clk, w_reset_n, w_in, w_done, r_clk, r_enable, r_addr, r_out);
    parameter width = 8;
    parameter timeBits = 10;

    // write: clock, reset, and input, and a done flag
    input w_clk;
    input w_reset_n;
    input [width-1:0] w_in;
    output w_done;

    // the internal write cursor, with an extra bit
    // when this bit is set, we are done sampling
    reg [timeBits:0] w_addr = 1 << timeBits;
    assign w_done = w_addr[timeBits];

    // read: clock, enable, address, output
    input r_clk;
    input r_enable;
    input [timeBits-1:0] r_addr;
    output reg [width-1:0] r_out;

    // our memory
    reg [width-1:0] memory [(2**timeBits)-1:0];

    // write side
    always @(posedge w_clk)
    begin
        // if we're not reset, and we're sampling...
        if (w_reset_n && !w_done)
        begin
            memory[w_addr[timeBits-1:0]] <= w_in;
            w_addr <= w_addr + 1;
        end

        // if we're reset
        if (!w_reset_n)
            w_addr <= 0;
    end

    // read side
    always @(posedge r_clk)
    begin
        if (r_enable)
            r_out <= memory[r_addr];
    end
endmodule

// wrapping the above in a nice qsys-friendly package
module qsys_sampler
    #(parameter words_log_2 = 0,
      parameter words = 1,
      parameter timeBits = 10
      )
    (// write side
     input w_clk,
     input [32*words-1:0] w_in,
     output reg w_reset_n = 0,
                    
     // read side
     input clk,
     input reset_n,
     input buffer_read,
     input [timeBits + words_log_2 - 1:0] buffer_address,
     output [31:0] buffer_readdata,

     // control
     input csr_write,
     input [31:0] csr_writedata,
     input csr_read,
     output reg [31:0] csr_readdata,
     output reg irq = 0
     );

    // other inputs to the sampler, driven elsewhere
    wire [timeBits-1:0] r_addr;
    wire [32*words-1:0] r_out;
    wire w_done;

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
            w_reset_n <= csr_writedata[0];
            irq <= 0;
        end
        else if (csr_read)
        begin
            csr_readdata[0] <= w_reset_n;
            csr_readdata[1] <= w_done;
            csr_readdata[2] <= irq;
        end

        // fire irq when we finish
        if (old_done == 0 && w_done == 1)
            irq <= 1;
        old_done <= w_done;

        // if reset, then reset our reset (eww)
        if (!reset_n)
        begin
            w_reset_n <= 0;
            old_done <= 0;
            irq <= 0;
        end
    end

    // read
    reg [(words_log_2 > 0 ? words_log_2 + 5 - 1 : 0):0] saved_addr = 0;
    assign r_addr = buffer_address >> words_log_2;
    assign buffer_readdata = r_out >> (saved_addr << 5);
    always @(posedge clk)
    begin
        if (words_log_2 > 0 && buffer_read)
            saved_addr <= buffer_address[words_log_2-1:0];
    end
        
    
    sampler #(32*words, timeBits) s(w_clk, w_reset_n, w_in, w_done, clk, buffer_read, r_addr, r_out);
endmodule
