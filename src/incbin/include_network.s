.section .rodata
.global weights_bin_start
.global weights_bin_end

weights_bin_start:
    .incbin "./import/weights.nnue"
weights_bin_end: