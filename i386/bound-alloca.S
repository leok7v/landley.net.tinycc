/* Implementation of alloca() for tinycc (tcc) on x86, bound-checking.
 * See alloca86.S */

/* alloca_padding is the minimum number of bytes AFTER the allocation that will
 * be unused.  Must be at LEAST 1 for bound-checking to work, 4 if you want
 * off-by-one word-writes to not overwrite something important, and 0
 * if stack space is an absolute premium. Often wise to keep bounded and
 * unbounded values the same. */
bounded_alloca_padding=4

/* Alignment: usually 4, 8, or 16. Power of 2. Result % alignment == 0. */
bounded_alloca_alignment=4

.globl __bound__alloca_tcc
__bound__alloca_tcc:
    pop    %edx        /* yank return address from stack */
    pop    %ecx        /* Get parameter (which is size). */

    /* See if we got 0, and if so, handle specially. */
    or     $0,%ecx
    jz     bound_alloc_zero

    /* Allocate memory on the stack */
    mov    %ecx,%eax
    add    $(bounded_alloca_padding+bounded_alloca_alignment-1),%eax
    and    $(-bounded_alloca_alignment),%eax
    sub    %eax,%esp   /* Allocate!  MODIFIES STACK POINTER HERE */

/* Call __bound_new_region(void *p, unsigned long size)
 * if doing bound checks, where *p is %esp, and size is size (NOT size+1).
 * For maximum efficiency could merge this with the code afterwards, but
 * it's easier to see what it does this way. */
    mov    %esp,%eax
    push   %edx
    push   %ecx
    push   %eax
    call   __bound_new_region
    add    $8, %esp
    pop    %edx

    mov    %esp,%eax   /* Return beginning of allocated area to caller */
    push   %edx        /* Re-allocate param space for the caller to remove */
    push   %edx        /* Restore return address to return to. */
    ret

bound_alloc_zero:
    mov    %ecx,%eax   /* Return NULL */
    push   %eax        /* Re-allocate param space for the caller to remove */
    push   %edx        /* Restore return address to return to. */
    ret
