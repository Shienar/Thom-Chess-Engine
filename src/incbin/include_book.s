.section .rodata
.global book_bin_start
.global book_bin_end

book_bin_start:
    .incbin "./import/komodo.bin"
book_bin_end:
