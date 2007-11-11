/* Implementation of alloca() for tinycc (tcc) on x86.
 * Based on grischka case_8, modified by David A. Wheeler 2007-05-09.
 * Plays games with stack, so it omits the usual prologue and epilogue.
 * We use the normal cdecl calling convention to reduce the risk of error. */


/* alloca_padding is the minimum number of unused bytes AFTER the allocation.
 * It must be at LEAST 1 for bound-checking to work, 4 if you want
 * off-by-one word-writes to not overwrite something important, and 0
 * if stack space is an absolute premium */
alloca_padding=4

/* Alignment: usually 4, 8, or 16. Power of 2. Result % alignment == 0. */
alloca_alignment=4

.globl _alloca_tcc
_alloca_tcc:
    pop    %edx        /* yank return address from stack */
    pop    %ecx        /* Get parameter (which is size). */

    /* See if we got 0, and if so, handle specially. */
    or     $0,%ecx
    jz     alloc_zero

    /* Allocate memory on the stack */
    mov    %ecx,%eax
    add    $(alloca_padding+alloca_alignment-1),%eax
    and    $(-alloca_alignment),%eax
    sub    %eax,%esp   /* Allocate!  MODIFIES STACK POINTER HERE */

    mov    %esp,%eax   /* Return beginning of allocated area to caller */
    push   %edx        /* Re-allocate param space for the caller to remove */
    push   %edx        /* Restore return address to return to. */
    ret

alloc_zero:
    mov    %ecx,%eax   /* Return NULL */
    push   %eax        /* Re-allocate param space for the caller to remove */
    push   %edx        /* Restore return address to return to. */
    ret

