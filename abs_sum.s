.arch armv8-a
.text
.align 2
.global abs_sum
.type abs_sum, %function

// Compute the sum of the absolute value of values in the array.
// x0: address of long array[32]
// x1: size of array (32)
abs_sum:
    // Modify below here
    movz x20, #8
    movz    x2, #0              // Running sum.
.loop:
    cmp    x1, x20         // Check if there are elements left in array to sum.
    b.le    .default_impl               // If not, we are done.

    ldur    x3, [x0, #0]        // Load element from memory.
    ldur   x4, [x0, #8] //Load second element from memory
    ldur x5, [x0, #16]
    ldur x6, [x0, #24]
    ldur x7, [x0, #32]        // Load element from memory.
    ldur x8, [x0, #40] //Load second element from memory
    ldur x9, [x0, #48]
    ldur x10, [x0, #56]


    cmp     x3, xzr             // Check if element is positive
    b.ge    .positive1

.negative1:
    subs    x3, xzr, x3         // Element is negative, negate it.
                                // Fall through to positive case.

.positive1:
    adds    x2, x2, x3  
    cmp x4, xzr
    b.ge .positive2        // Element is positive, add it to sum.

.negative2:                     // Element is negative, negate it.
    subs x4, xzr, x4

.positive2:
    adds    x2, x2, x4          // Element is positive, add it to sum.
    cmp x5, xzr
    b.ge .positive3        // Element is positive, add it to sum.

.negative3:                     // Element is negative, negate it.
    subs x5, xzr, x5

.positive3:
    adds    x2, x2, x5          // Element is positive, add it to sum.
    cmp x6, xzr
    b.ge .positive4        // Element is positive, add it to sum.

.negative4:                     // Element is negative, negate it.
    subs x6, xzr, x6

.positive4:
    adds    x2, x2, x6          // Element is positive, add it to sum.
    cmp x7, xzr
    b.ge .positive5 

.negative5:                     // Element is negative, negate it.
    subs x7, xzr, x7

.positive5:
    adds    x2, x2, x7          // Element is positive, add it to sum.
    cmp x8, xzr
    b.ge .positive6

.negative6:                     // Element is negative, negate it.
    subs x8, xzr, x8

.positive6:
    adds    x2, x2, x8          // Element is positive, add it to sum.
    cmp x9, xzr
    b.ge .positive7

.negative7:                     // Element is negative, negate it.
    subs x9, xzr, x9

.positive7:
    adds    x2, x2, x9          // Element is positive, add it to sum.
    cmp x10, xzr
    b.ge .positive8  

.negative8:                     // Element is negative, negate it.
    subs x10, xzr, x10

.positive8:
    adds    x2, x2, x10          // Element is positive, add it to sum.


.end:
    add     x0, x0, #64          // Increment pointer.
    sub     x1, x1, #8          // Decrement remaining size.
    b       .loop               // Continue loop.

.default_impl:
    subs    x1, x1, xzr         // Check if there are elements left in array to sum.
    b.eq    .done               // If not, we are done.

    ldur    x3, [x0, #0]        // Load element from memory.

    cmp     x3, xzr             // Check if element is positive
    b.ge    .positive

.negative:
    subs    x3, xzr, x3         // Element is negative, negate it.
                                // Fall through to positive case.

.positive:
    adds    x2, x2, x3          // Element is positive, add it to sum.

.end2:
    add     x0, x0, #8          // Increment pointer.
    sub     x1, x1, #1          // Decrement remaining size.
    b       .default_impl               // Continue loop.

.done:
    adds    x0, xzr, x2         // Move sum into x0.
    ret
.size   abs_sum, .-abs_sum
