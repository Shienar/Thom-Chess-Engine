.section .rodata
.global int_weights_bin
.global book_bin_start
.global book_bin_end

int_weights_bin:
    .incbin "../import/quantized.nnue"
book_bin_start:
    .incbin "../import/gm2600.bin"
book_bin_end: