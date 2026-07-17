.section .rodata
.global int_weights_bin

int_weights_bin:
    .incbin "./import/quantized.nnue"
