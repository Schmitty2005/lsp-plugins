/*
 * fastconv.h
 *
 *  Created on: 31 окт. 2018 г.
 *      Author: sadko
 */

#ifndef DSP_ARCH_ARM_NEON_D32_FASTCONV_H_
#define DSP_ARCH_ARM_NEON_D32_FASTCONV_H_

#ifndef DSP_ARCH_ARM_NEON_32_IMPL
    #error "This header should not be included directly"
#endif /* DSP_ARCH_ARM_NEON_32_IMPL */

namespace neon_d32
{
    void fastconv_parse(float *dst, const float *src, size_t rank)
    {
        size_t items        = 1 << rank; // number of complex numbers per half of block (16 for rank=4)
        size_t n            = items >> 1; // number of butterflies per block (8 for rank=4)

        IF_ARCH_ARM(
            const float *fdw    = &XFFT_DW[(rank - 3) << 3];
            const float *fw     = &XFFT_A[(rank - 3) << 4];
            size_t k, p;
            float *a, *b;
        );

        if (n > 4)
        {
            // First loop
            ARCH_ARM_ASM(
                __ASM_EMIT("mov         %[a], %[dst]")                  // a    = dst
                __ASM_EMIT("add         %[b], %[a], %[n], LSL $3")      // b    = &a[n*2]
                __ASM_EMIT("mov         %[k], %[n]")
                __ASM_EMIT("vldm        %[XFFT_A], {q8-q11}")           // q8   = wr0, q9 = wr1, q10 = wi0, q11 = wi1

                // 8x butterflies
                __ASM_EMIT("1:")
                __ASM_EMIT("vldm        %[src]!, {q0-q1}")              // q0   = ar0, q1 = ar1
                __ASM_EMIT("veor        q2, q2")                        // q2   = 0
                __ASM_EMIT("veor        q3, q3")                        // q3   = 0
                __ASM_EMIT("vswp        q2, q1")                        // q1   = 0, q2 = ar1
                __ASM_EMIT("vmul.f32    q5, q0, q10")                   // q5   = ar0 * wi0
                __ASM_EMIT("vmul.f32    q4, q0, q8")                    // q4   = ar0 * wr0 = br0
                __ASM_EMIT("vneg.f32    q5, q5")                        // q5   = - ar0 * wi0 = bi0
                __ASM_EMIT("vmul.f32    q7, q2, q11")                   // q7   = ar1 * wi1
                __ASM_EMIT("vmul.f32    q6, q2, q9")                    // q6   = ar1 * wr1 = br1
                __ASM_EMIT("vneg.f32    q7, q7")                        // q7   = - ar1 * wi1 = bi1
                __ASM_EMIT("vstm        %[a]!, {q0-q3}")
                __ASM_EMIT("vstm        %[b]!, {q4-q7}")
                __ASM_EMIT("subs        %[k], $8")
                __ASM_EMIT("bls         2f")

                // Rotate angle
                __ASM_EMIT("vld1.32     {q0-q1}, [%[XFFT_DW]]")         // q0   = dr, q1 = di
                __ASM_EMIT("vmul.f32    q12, q8, q1")                   // q12  = wr0 * di
                __ASM_EMIT("vmul.f32    q13, q9, q1")                   // q13  = wr1 * di
                __ASM_EMIT("vmul.f32    q14, q10, q1")                  // q14  = wi0 * di
                __ASM_EMIT("vmul.f32    q15, q11, q1")                  // q15  = wi1 * di
                __ASM_EMIT("vmul.f32    q8, q8, q0")                    // q8   = wr0 * dr
                __ASM_EMIT("vmul.f32    q9, q9, q0")                    // q9   = wr1 * dr
                __ASM_EMIT("vmul.f32    q10, q10, q0")                  // q10  = wi0 * dr
                __ASM_EMIT("vmul.f32    q11, q11, q0")                  // q11  = wi1 * dr
                __ASM_EMIT("vsub.f32    q8, q8, q14")                   // q8   = wr0*dr - wi0*di
                __ASM_EMIT("vsub.f32    q9, q9, q15")                   // q9   = wr1*dr - wi1*di
                __ASM_EMIT("vadd.f32    q10, q10, q12")                 // q10  = wi0*dr + wr0*di
                __ASM_EMIT("vadd.f32    q11, q11, q13")                 // q11  = wi1*dr + wr1*di
                __ASM_EMIT("b           1b")

                // Post-process
                __ASM_EMIT("2:")
                __ASM_EMIT("sub         %[XFFT_A], $0x40")              // fw  -= 16
                __ASM_EMIT("sub         %[XFFT_DW], $0x20")             // fdw -= 8
                __ASM_EMIT("lsr         %[n], $1")                      // n >>= 1

                : [src] "+r" (src),
                  [a] "=&r" (a), [b] "=&r" (b),
                  [XFFT_A] "+r" (fw), [XFFT_DW] "+r" (fdw),
                  [k] "=&r" (k), [n] "+r" (n)
                : [dst] "r" (dst)
                : "cc", "memory",
                  "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"
            );

            // All other loops while n > 4
            ARCH_ARM_ASM(
                __ASM_EMIT("cmp         %[n], $8")
                __ASM_EMIT("blo         8f")

                __ASM_EMIT("1:")
                __ASM_EMIT("mov         %[a], %[dst]")                  // a    = dst
                __ASM_EMIT("mov         %[p], %[items]")                // p    = items

                    __ASM_EMIT("3:")
                    __ASM_EMIT("add         %[b], %[a], %[n], LSL $3")      // b    = &a[n*2]
                    __ASM_EMIT("vldm        %[XFFT_A], {q8-q11}")           // q8   = wr0, q9 = wr1, q10 = wi0, q11 = wi1
                    __ASM_EMIT("mov         %[k], %[n]")
                        // 8x butterflies
                        __ASM_EMIT("5:")
                        __ASM_EMIT("vldm        %[a], {q0-q3}")                 // q0   = ar0, q1 = ai0, q2 = ar1, q3 = ai1
                        __ASM_EMIT("vldm        %[b], {q4-q7}")                 // q4   = br0, q5 = bi0, q6 = br1, q7 = bi1
                        __ASM_EMIT("vsub.f32    q12, q0, q4")                   // q12  = ar0 - br0 = cr0
                        __ASM_EMIT("vsub.f32    q13, q1, q5")                   // q13  = ai0 - bi0 = ci0
                        __ASM_EMIT("vsub.f32    q14, q2, q6")                   // q14  = ar1 - br1 = cr1
                        __ASM_EMIT("vsub.f32    q15, q3, q7")                   // q15  = ai1 - bi1 = ci1
                        __ASM_EMIT("vadd.f32    q0, q0, q4")                    // q0   = ar0 + br0 = ar0'
                        __ASM_EMIT("vadd.f32    q1, q1, q5")                    // q1   = ai0 + bi0 = ai0'
                        __ASM_EMIT("vadd.f32    q2, q2, q6")                    // q2   = ar1 + br1 = ar1'
                        __ASM_EMIT("vadd.f32    q3, q3, q7")                    // q3   = ai1 + bi1 = ai1'
                        __ASM_EMIT("vstm        %[a]!, {q0-q3}")
                        __ASM_EMIT("vmul.f32    q0, q8, q12")                   // q0   = wr0 * cr0
                        __ASM_EMIT("vmul.f32    q2, q9, q14")                   // q2   = wr1 * cr1
                        __ASM_EMIT("vmul.f32    q1, q8, q13")                   // q1   = wr0 * ci0
                        __ASM_EMIT("vmul.f32    q3, q9, q15")                   // q3   = wr1 * ci1
                        __ASM_EMIT("vmla.f32    q0, q10, q13")                  // q0   = wr0*cr0 + wi0*ci0 = br0'
                        __ASM_EMIT("vmla.f32    q2, q11, q15")                  // q2   = wr1*cr1 + wi1*ci1 = bi0'
                        __ASM_EMIT("vmls.f32    q1, q10, q12")                  // q1   = wr0*ci0 - wi0*cr0 = br1'
                        __ASM_EMIT("vmls.f32    q3, q11, q14")                  // q3   = wr1*ci1 - wi1*cr1 = bi1'
                        __ASM_EMIT("vstm        %[b]!, {q0-q3}")
                        __ASM_EMIT("subs        %[k], $8")
                        __ASM_EMIT("bls         6f")
                        // Rotate angle & repeat loop
                        __ASM_EMIT("vld1.32     {q0-q1}, [%[XFFT_DW]]")         // q0   = dr, q1 = di
                        __ASM_EMIT("vmul.f32    q12, q8, q1")                   // q12  = wr0 * di
                        __ASM_EMIT("vmul.f32    q13, q9, q1")                   // q13  = wr1 * di
                        __ASM_EMIT("vmul.f32    q14, q10, q1")                  // q14  = wi0 * di
                        __ASM_EMIT("vmul.f32    q15, q11, q1")                  // q15  = wi1 * di
                        __ASM_EMIT("vmul.f32    q8, q8, q0")                    // q8   = wr0 * dr
                        __ASM_EMIT("vmul.f32    q9, q9, q0")                    // q9   = wr1 * dr
                        __ASM_EMIT("vmul.f32    q10, q10, q0")                  // q10  = wi0 * dr
                        __ASM_EMIT("vmul.f32    q11, q11, q0")                  // q11  = wi1 * dr
                        __ASM_EMIT("vsub.f32    q8, q8, q14")                   // q8   = wr0*dr - wi0*di
                        __ASM_EMIT("vsub.f32    q9, q9, q15")                   // q9   = wr1*dr - wi1*di
                        __ASM_EMIT("vadd.f32    q10, q10, q12")                 // q10  = wi0*dr + wr0*di
                        __ASM_EMIT("vadd.f32    q11, q11, q13")                 // q11  = wi1*dr + wr1*di
                        __ASM_EMIT("b           5b")
                    __ASM_EMIT("6:")
                    __ASM_EMIT("mov         %[a], %[b]")                        // a    = b
                    __ASM_EMIT("subs        %[p], %[n], LSL $1")                // p   -= n*2
                    __ASM_EMIT("bne         3b")

                __ASM_EMIT("lsr         %[n], $1")                      // n >>= 1
                __ASM_EMIT("sub         %[XFFT_A], $0x40")              // fw  -= 16
                __ASM_EMIT("sub         %[XFFT_DW], $0x20")             // fdw -= 8
                __ASM_EMIT("cmp         %[n], $8")
                __ASM_EMIT("bge         1b")

                // Loop for n=4
                __ASM_EMIT("8:")
                __ASM_EMIT("mov         %[a], %[dst]")                  // a    = dst
                __ASM_EMIT("mov         %[p], %[items]")                // p    = items
                __ASM_EMIT("vldm        %[XFFT_A], {q8-q11}")           // q8   = wr0, q9 = wr1, q10 = wi0, q11 = wi1

                // 8x butterflies
                __ASM_EMIT("9:")
                __ASM_EMIT("vldm        %[a], {q0-q7}")                 // q0   = ar0, q1 = ai0, q2 = br0, q3 = bi0, q4 = ar1, q5 = ai1, q6 = br1, q7 = bi1
                __ASM_EMIT("vsub.f32    q12, q0, q2")                   // q12  = ar0 - br0 = cr0
                __ASM_EMIT("vsub.f32    q13, q1, q3")                   // q13  = ai0 - bi0 = ci0
                __ASM_EMIT("vsub.f32    q14, q4, q6")                   // q14  = ar1 - br1 = cr1
                __ASM_EMIT("vsub.f32    q15, q5, q7")                   // q15  = ai1 - bi1 = ci1

                __ASM_EMIT("vadd.f32    q0, q0, q2")                    // q0   = ar0 + br0 = ar0'
                __ASM_EMIT("vadd.f32    q1, q1, q3")                    // q1   = ai0 + bi0 = ai0'
                __ASM_EMIT("vadd.f32    q4, q4, q6")                    // q4   = ar1 + br1 = ar1'
                __ASM_EMIT("vadd.f32    q5, q5, q7")                    // q5   = ai1 + bi1 = ai1'

                __ASM_EMIT("vmul.f32    q2, q8, q12")                   // q2   = wr0 * cr0
                __ASM_EMIT("vmul.f32    q6, q9, q14")                   // q6   = wr1 * cr1
                __ASM_EMIT("vmul.f32    q3, q8, q13")                   // q3   = wr0 * ci0
                __ASM_EMIT("vmul.f32    q7, q9, q15")                   // q7   = wr1 * ci1

                __ASM_EMIT("vmla.f32    q2, q10, q13")                  // q2   = wr0*cr0 + wi0*ci0 = br0'
                __ASM_EMIT("vmla.f32    q6, q11, q15")                  // q6   = wr1*cr1 + wi1*ci1 = br1'
                __ASM_EMIT("vmls.f32    q3, q10, q12")                  // q3   = wr0*ci0 - wi0*cr0 = bi0'
                __ASM_EMIT("vmls.f32    q7, q11, q14")                  // q7   = wr1*ci1 - wi1*cr1 = bi1'
                __ASM_EMIT("vstm        %[a]!, {q0-q7}")
                __ASM_EMIT("subs        %[p], $16")                     // p   -= 16
                __ASM_EMIT("bne         9b")

                : [a] "=&r" (a), [b] "=&r" (b),
                  [k] "=&r" (k), [p] "=&r" (p),
                  [XFFT_A] "+r" (fw), [XFFT_DW] "+r" (fdw),
                  [n] "+r" (n)
                : [dst] "r" (dst), [items] "r" (items)
                : "cc", "memory",
                  "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"
            );
        }
        else
        {
            ARCH_ARM_ASM(
                __ASM_EMIT("vld1.32     {q0}, [%[src]]")
                __ASM_EMIT("veor        q1, q1")
                __ASM_EMIT("vst1.32     {q0-q1}, [%[dst]]")
                :
                : [src] "r" (src), [dst] "r" (dst)
                : "memory",
                  "q0", "q1"
            );
        }
    }
}

#endif /* DSP_ARCH_ARM_NEON_D32_FASTCONV_H_ */
