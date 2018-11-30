# mark_description "Intel(R) C Intel(R) 64 Compiler XE for applications running on Intel(R) MIC Architecture, Version 14.0.1.106";
# mark_description " Build 20131008";
# mark_description "-ffreestanding -mmic -S -mGLOB_default_function_attrs=knc_stream_store_controls=8 -unroll-aggressive -opt-pr";
# mark_description "efetch=4 -O3 -o copy_user_generic_vector.S";
	.file "copy_user_generic_vector.c"
	.text
..TXTST0:
# -- Begin  copy_user_generic_vector
# mark_begin;
# Threads 4
        .align    16,0x90
	.globl copy_user_generic_vector
copy_user_generic_vector:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %rdx
..B1.1:                         # Preds ..B1.0 Latency 17
        pushq     %r14                                          #3.59 c1
1:      movq      %rdi, %rcx                                    #12.41 c1
        andq      $63, %rcx                                     #12.41 c5
2:      movq      %rsi, %rax                                    #9.5 c5
3:      movl      %ecx, %r8d                                    #12.41 c9 r8d = dst
4:      movq      %rdi, %r14                                    #10.5 c13
        cmpq      $256, %rdx                                    #14.13 c13
        jb        ..B1.24       # Prob 28%                      #14.13 c17
                                # LOE rax rdx rcx rbx rbp rsi rdi r12 r13 r14 r15 r8d
..B1.2:                         # Preds ..B1.1 Latency 1
        testl     %r8d, %r8d                                    #16.22 c1
        je        ..B1.10       # Prob 50%                      #16.22 c1
                                # LOE rax rdx rcx rbx rbp rsi rdi r12 r13 r14 r15 r8d
..B1.3:                         # Preds ..B1.2 Latency 13
        negq      %rcx                                          #18.28 c1  rcx = -dst_align
        xorl      %r9d, %r9d                                    #6.5 c5 nn=0
        addq      $64, %rcx                                     #18.28 c5 LINESIZE - dst_align =ntail
        subl      %r8d, %r9d                                    #6.5 c9
        subq      %rcx, %rdx                                    #19.9 c9 rdx = n = n -ntail,
        addl      $64, %r9d                                     #6.5 c13
                                # LOE rax rdx rbx rbp rsi rdi r12 r13 r14 r15 r8d r9d
..B1.4:                         # Preds ..B1.3 Latency 17
        negl      %r8d                                          #20.9 c1 -ntail
        addl      $64, %r8d                                     #20.9 c5
        movl      $1, %ecx                                      #20.9 c5
        shrl      $1, %r8d                                      #20.9 c9
        xorl      %r11d, %r11d                                  #20.9 c9
        xorl      %r10d, %r10d                                  # c13
        testq     %r8, %r8                                      #20.9 c13
        jbe       ..B1.8        # Prob 3%                       #20.9 c17
                                # LOE rax rdx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 ecx r9d
..B1.5:                         # Preds ..B1.4 Latency 5
        vprefetch0 (%rsi)                                       #20.9 c1
5:      movb      %al, %al                                      #20.9 c1
        vprefetche0 (%rdi)                                      #20.9 c5
                                # LOE rax rdx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d
..B1.6:                         # Preds ..B1.6 ..B1.5 Latency 25
6:      movb      (%rax), %cl                                   #20.46 c1
        vprefetch1 128(%r10,%rsi)                               #20.9 c1
7:      movb      %cl, (%r14)                                   #20.37 c5
        vprefetch0 16(%r10,%rsi)                                #20.9 c5
8:      movb      1(%rax), %cl                                  #20.46 c9
        vprefetche1 128(%r10,%rdi)                              #20.9 c9
        incq      %r11                                          #20.9 c13
        vprefetche0 16(%r10,%rdi)                               #20.9 c13
9:      movb      %cl, 1(%r14)                                  #20.37 c17
10:      addq      $2, %r14                                      #20.37 c17
11:      addq      $2, %rax                                      #20.46 c21
        addq      $2, %r10                                      #20.9 c21
        cmpq      %r8, %r11                                     #20.9 c25
        jb        ..B1.6        # Prob 63%                      #20.9 c25
                                # LOE rax rdx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d
..B1.7:                         # Preds ..B1.6 Latency 1
        lea       1(%r11,%r11), %ecx                            #20.37 c1
                                # LOE rax rdx rbx rbp r12 r13 r14 r15 ecx r9d
..B1.8:                         # Preds ..B1.7 ..B1.4 Latency 5
        decl      %ecx                                          #20.9 c1
        cmpl      %ecx, %r9d                                    #20.9 c5
        jbe       ..B1.10       # Prob 3%                       #20.9 c5
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.9:                         # Preds ..B1.8 Latency 5
12:     movb      (%rax), %cl                                   #20.46 c1
        incq      %rax                                          #20.46 c1
13:     movb      %cl, (%r14)                                   #20.37 c5
        incq      %r14                                          #20.37 c5
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.10:                        # Preds ..B1.9 ..B1.8 ..B1.2 Latency 9
14:     movq      %rax, %rcx                                    #24.43 c1
        andq      $63, %rcx                                     #24.43 c5
        testl     %ecx, %ecx                                    #26.23 c9
        jne       ..B1.16       # Prob 50%                      #26.23 c9
                                # LOE rax rdx rbx rbp r12 r13 r14 r15 ecx
..B1.11:                        # Preds ..B1.10 Latency 13
        movzbl    %dl, %r8d                                     #27.14 c1
        xorl      %ecx, %ecx                                    #28.15 c1
        negq      %r8                                           #27.14 c5
        addq      %rdx, %r8                                     #27.14 c9 n - n%MEMCPY_KNC_UNROLL
        testl     %r8d, %r8d                                    #28.22 c13 r8d= nn
        jle       ..B1.15       # Prob 10%                      #28.22 c13
                                # LOE rax rdx rbx rbp r12 r13 r14 r15 ecx r8d
..B1.13:                        # Preds ..B1.11 ..B1.13 Latency 9
        addl      $256, %ecx                                    #28.25 c1
15:     vmovapd   (%rax), %zmm0                                 #29.0
        vprefetch0 256(%rax)                                    #29.0
        vprefetch2 2048(%rax)                                   #29.0
16:     vmovapd   64(%rax), %zmm1                               #29.0
        vprefetch0 320(%rax)                                    #29.0
        vprefetch2 2112(%rax)                                   #29.0
17:     vmovapd   128(%rax), %zmm2                              #29.0
        vprefetch0 384(%rax)                                    #29.0
        vprefetch2 2176(%rax)                                   #29.0
18:     vmovapd   192(%rax), %zmm3                              #29.0
        vprefetch0 448(%rax)                                    #29.0
        vprefetch2 2240(%rax)                                   #29.0
19:     vmovnrngoapd %zmm0, (%r14)                              #29.0
20:     vmovnrngoapd %zmm1, 64(%r14)                            #29.0
21:     vmovnrngoapd %zmm2, 128(%r14)                           #29.0
22:     vmovnrngoapd %zmm3, 192(%r14)                           #29.0
        addq      $256, %rax                                    #30.17 c5
        addq      $256, %r14                                    #31.17 c5
        cmpl      %r8d, %ecx                                    #28.22 c9
        jl        ..B1.13       # Prob 82%                      #28.22 c9
                                # LOE rax rdx rbx rbp r12 r13 r14 r15 ecx r8d
..B1.15:                        # Preds ..B1.13 ..B1.11 Latency 6
        movslq    %ecx, %rcx                                    #34.13 c1
        subq      %rcx, %rdx                                    #34.11 c5  n = n -i
        jmp       ..B1.22       # Prob 100%                     #34.11 c5
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.16:                        # Preds ..B1.10 Latency 1
        andl      $3, %ecx                                      #35.28 c1
        jne       ..B1.24       # Prob 10%                      #35.33 c1
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.17:                        # Preds ..B1.16 Latency 17
        movzbl    %dl, %ecx                                     #36.14 c1
        xorl      %r8d, %r8d                                    #37.15 c1
        negq      %rcx                                          #36.14 c5
        addq      %rdx, %rcx                                    #36.14 c9 rcx = nn
        movslq    %ecx, %rcx                                    #36.9 c13
        testq     %rcx, %rcx                                    #37.22 c17
        jle       ..B1.21       # Prob 10%                      #37.22 c17
                                # LOE rax rdx rcx rbx rbp r8 r12 r13 r14 r15
..B1.19:                        # Preds ..B1.17 ..B1.19 Latency 9
        addq      $256, %r8                                     #37.25 c1
23:     vloadunpackld (%rax), %zmm0                             #38.0
        vprefetch0 256(%rax)                                    #38.0
24:     vloadunpackhd 64(%rax), %zmm0                           #38.0
        vprefetch2 2048(%rax)                                   #38.0
25:     vloadunpackld 64(%rax), %zmm1                           #38.0
        vprefetch0 320(%rax)                                    #38.0
26:     vloadunpackhd 128(%rax), %zmm1                          #38.0
        vprefetch2 2112(%rax)                                   #38.0
27:     vloadunpackld 128(%rax), %zmm2                          #38.0
        vprefetch0 384(%rax)                                    #38.0
28:     vloadunpackhd 192(%rax), %zmm2                          #38.0
        vprefetch2 2176(%rax)                                   #38.0
29:     vloadunpackld 192(%rax), %zmm3                          #38.0
        vprefetch0 448(%rax)                                    #38.0
30:     vloadunpackhd 256(%rax), %zmm3                          #38.0
        vprefetch2 2240(%rax)                                   #38.0
31:     vmovnrngoapd %zmm0, (%r14)                              #38.0
32:     vmovnrngoapd %zmm1, 64(%r14)                            #38.0
33:     vmovnrngoapd %zmm2, 128(%r14)                           #38.0
34:     vmovnrngoapd %zmm3, 192(%r14)                           #38.0
        addq      $256, %rax                                    #39.17 c5
        addq      $256, %r14                                    #40.17 c5
        cmpq      %rcx, %r8                                     #37.22 c9
        jl        ..B1.19       # Prob 82%                      #37.22 c9
                                # LOE rax rdx rcx rbx rbp r8 r12 r13 r14 r15
..B1.21:                        # Preds ..B1.19 ..B1.17 Latency 1
        subq      %r8, %rdx                                     #43.11 c1 n = n -i
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.22:                        # Preds ..B1.15 ..B1.21 Latency 1
        testq     %rdx, %rdx                                    #50.12 c1 n==0
        jne       ..B1.24       # Prob 22%                      #50.12 c1
                                # LOE rax rdx rbx rbp r12 r13 r14 r15
..B1.23:                        # Preds ..B1.22 Latency 9
        xorl      %eax, %eax                                    #50.22 c1
        popq      %r14                                          #50.22
        ret                                                     #50.22 c9
                                # LOE
..B1.24:                        # Preds ..B1.16 ..B1.1 ..B1.22 Latency 9
35:     movq      %r14, %rdi                                    #52.4 c1
36:     movq      %rax, %rsi                                    #52.4 c1
        xorl      %eax, %eax                                    #52.4 c5
        call      copy_user_generic_string                                        #52.4
                                # LOE rbx rbp r12 r13 r15
..B1.25:                        # Preds ..B1.24 Latency 9
        xorl      %eax, %eax                                    #54.12 c1
        popq      %r14                                          #54.12
        ret                                                     #54.12 c9
        .align    16,0x90
                                # LOE
         .section .fixup, "ax"
90:      movslq %ecx, %rcx
         addq %rcx, %rdx
         jmp 120f
100:     movslq    %ecx, %rcx
         subq      %rcx, %rdx                                    # rcx has i, rdx is rem bytes
         jmp 120f
110:     subq      %r8, %rdx
120:     movq      %rdx, %rax
         jmp copy_user_handle_tail
         .previous


        .section __ex_table, "a"
        .quad 1b, 90b
        .quad 2b, 90b
        .quad 3b, 90b
        .quad 4b, 90b
        .quad 5b, 90b
        .quad 6b, 90b
        .quad 7b, 90b
        .quad 8b, 90b
        .quad 9b, 90b
        .quad 10b, 90b
        .quad 11b, 90b
        .quad 12b, 90b
        .quad 13b, 90b
        .quad 14b, 120b
        .quad 15b, 100b
        .quad 16b, 100b
        .quad 17b, 100b
        .quad 18b, 100b
        .quad 19b, 100b
        .quad 20b, 100b
        .quad 21b, 100b
        .quad 22b, 100b
        .quad 23b, 110b
        .quad 24b, 110b
        .quad 25b, 110b
        .quad 26b, 110b
        .quad 27b, 110b
        .quad 28b, 110b
        .quad 29b, 110b
        .quad 30b, 110b
        .quad 31b, 110b
        .quad 32b, 110b
        .quad 33b, 110b
        .quad 34b, 110b
        .quad 35b, 120b
        .quad 36b, 120b
				.previous

# mark_end;
	.type	copy_user_generic_vector,@function
	.size	copy_user_generic_vector,.-copy_user_generic_vector
# -- End  copy_user_generic_vector
