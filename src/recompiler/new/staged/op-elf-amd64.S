	.file	"op.c"
	.text
	.p2align 4,,15
.globl op_movl_A0_EAX
	.type	op_movl_A0_EAX, @function
op_movl_A0_EAX:
.LFB347:
	movl	(%rbp), %r13d
	ret
.LFE347:
	.size	op_movl_A0_EAX, .-op_movl_A0_EAX
	.p2align 4,,15
.globl op_addl_A0_EAX
	.type	op_addl_A0_EAX, @function
op_addl_A0_EAX:
.LFB348:
	addl	(%rbp), %r13d
	ret
.LFE348:
	.size	op_addl_A0_EAX, .-op_addl_A0_EAX
	.p2align 4,,15
.globl op_addl_A0_EAX_s1
	.type	op_addl_A0_EAX_s1, @function
op_addl_A0_EAX_s1:
.LFB349:
	movl	(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE349:
	.size	op_addl_A0_EAX_s1, .-op_addl_A0_EAX_s1
	.p2align 4,,15
.globl op_addl_A0_EAX_s2
	.type	op_addl_A0_EAX_s2, @function
op_addl_A0_EAX_s2:
.LFB350:
	movl	(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE350:
	.size	op_addl_A0_EAX_s2, .-op_addl_A0_EAX_s2
	.p2align 4,,15
.globl op_addl_A0_EAX_s3
	.type	op_addl_A0_EAX_s3, @function
op_addl_A0_EAX_s3:
.LFB351:
	movl	(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE351:
	.size	op_addl_A0_EAX_s3, .-op_addl_A0_EAX_s3
	.p2align 4,,15
.globl op_movl_T0_EAX
	.type	op_movl_T0_EAX, @function
op_movl_T0_EAX:
.LFB352:
	movl	(%rbp), %ebx
	ret
.LFE352:
	.size	op_movl_T0_EAX, .-op_movl_T0_EAX
	.p2align 4,,15
.globl op_movl_T1_EAX
	.type	op_movl_T1_EAX, @function
op_movl_T1_EAX:
.LFB353:
	movl	(%rbp), %r12d
	ret
.LFE353:
	.size	op_movl_T1_EAX, .-op_movl_T1_EAX
	.p2align 4,,15
.globl op_movh_T0_EAX
	.type	op_movh_T0_EAX, @function
op_movh_T0_EAX:
.LFB354:
	movl	(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE354:
	.size	op_movh_T0_EAX, .-op_movh_T0_EAX
	.p2align 4,,15
.globl op_movh_T1_EAX
	.type	op_movh_T1_EAX, @function
op_movh_T1_EAX:
.LFB355:
	movl	(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE355:
	.size	op_movh_T1_EAX, .-op_movh_T1_EAX
	.p2align 4,,15
.globl op_movl_EAX_T0
	.type	op_movl_EAX_T0, @function
op_movl_EAX_T0:
.LFB356:
	movl	%ebx, (%rbp)
	ret
.LFE356:
	.size	op_movl_EAX_T0, .-op_movl_EAX_T0
	.p2align 4,,15
.globl op_movl_EAX_T1
	.type	op_movl_EAX_T1, @function
op_movl_EAX_T1:
.LFB357:
	movl	%r12d, (%rbp)
	ret
.LFE357:
	.size	op_movl_EAX_T1, .-op_movl_EAX_T1
	.p2align 4,,15
.globl op_movl_EAX_A0
	.type	op_movl_EAX_A0, @function
op_movl_EAX_A0:
.LFB358:
	movl	%r13d, (%rbp)
	ret
.LFE358:
	.size	op_movl_EAX_A0, .-op_movl_EAX_A0
	.p2align 4,,15
.globl op_cmovw_EAX_T1_T0
	.type	op_cmovw_EAX_T1_T0, @function
op_cmovw_EAX_T1_T0:
.LFB359:
	testl	%ebx, %ebx
	je	.L26
	movw	%r12w, (%rbp)
.L26:
	ret
.LFE359:
	.size	op_cmovw_EAX_T1_T0, .-op_cmovw_EAX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EAX_T1_T0
	.type	op_cmovl_EAX_T1_T0, @function
op_cmovl_EAX_T1_T0:
.LFB360:
	testl	%ebx, %ebx
	je	.L31
	movl	%r12d, (%rbp)
.L31:
	ret
.LFE360:
	.size	op_cmovl_EAX_T1_T0, .-op_cmovl_EAX_T1_T0
	.p2align 4,,15
.globl op_movw_EAX_T0
	.type	op_movw_EAX_T0, @function
op_movw_EAX_T0:
.LFB361:
	movw	%bx, (%rbp)
	ret
.LFE361:
	.size	op_movw_EAX_T0, .-op_movw_EAX_T0
	.p2align 4,,15
.globl op_movw_EAX_T1
	.type	op_movw_EAX_T1, @function
op_movw_EAX_T1:
.LFB362:
	movw	%r12w, (%rbp)
	ret
.LFE362:
	.size	op_movw_EAX_T1, .-op_movw_EAX_T1
	.p2align 4,,15
.globl op_movw_EAX_A0
	.type	op_movw_EAX_A0, @function
op_movw_EAX_A0:
.LFB363:
	movw	%r13w, (%rbp)
	ret
.LFE363:
	.size	op_movw_EAX_A0, .-op_movw_EAX_A0
	.p2align 4,,15
.globl op_movb_EAX_T0
	.type	op_movb_EAX_T0, @function
op_movb_EAX_T0:
.LFB364:
	movb	%bl, (%rbp)
	ret
.LFE364:
	.size	op_movb_EAX_T0, .-op_movb_EAX_T0
	.p2align 4,,15
.globl op_movh_EAX_T0
	.type	op_movh_EAX_T0, @function
op_movh_EAX_T0:
.LFB365:
	movb	%bl, 1(%rbp)
	ret
.LFE365:
	.size	op_movh_EAX_T0, .-op_movh_EAX_T0
	.p2align 4,,15
.globl op_movb_EAX_T1
	.type	op_movb_EAX_T1, @function
op_movb_EAX_T1:
.LFB366:
	movb	%r12b, (%rbp)
	ret
.LFE366:
	.size	op_movb_EAX_T1, .-op_movb_EAX_T1
	.p2align 4,,15
.globl op_movh_EAX_T1
	.type	op_movh_EAX_T1, @function
op_movh_EAX_T1:
.LFB367:
	movb	%r12b, 1(%rbp)
	ret
.LFE367:
	.size	op_movh_EAX_T1, .-op_movh_EAX_T1
	.p2align 4,,15
.globl op_movl_A0_ECX
	.type	op_movl_A0_ECX, @function
op_movl_A0_ECX:
.LFB368:
	movl	4(%rbp), %r13d
	ret
.LFE368:
	.size	op_movl_A0_ECX, .-op_movl_A0_ECX
	.p2align 4,,15
.globl op_addl_A0_ECX
	.type	op_addl_A0_ECX, @function
op_addl_A0_ECX:
.LFB369:
	addl	4(%rbp), %r13d
	ret
.LFE369:
	.size	op_addl_A0_ECX, .-op_addl_A0_ECX
	.p2align 4,,15
.globl op_addl_A0_ECX_s1
	.type	op_addl_A0_ECX_s1, @function
op_addl_A0_ECX_s1:
.LFB370:
	movl	4(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE370:
	.size	op_addl_A0_ECX_s1, .-op_addl_A0_ECX_s1
	.p2align 4,,15
.globl op_addl_A0_ECX_s2
	.type	op_addl_A0_ECX_s2, @function
op_addl_A0_ECX_s2:
.LFB371:
	movl	4(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE371:
	.size	op_addl_A0_ECX_s2, .-op_addl_A0_ECX_s2
	.p2align 4,,15
.globl op_addl_A0_ECX_s3
	.type	op_addl_A0_ECX_s3, @function
op_addl_A0_ECX_s3:
.LFB372:
	movl	4(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE372:
	.size	op_addl_A0_ECX_s3, .-op_addl_A0_ECX_s3
	.p2align 4,,15
.globl op_movl_T0_ECX
	.type	op_movl_T0_ECX, @function
op_movl_T0_ECX:
.LFB373:
	movl	4(%rbp), %ebx
	ret
.LFE373:
	.size	op_movl_T0_ECX, .-op_movl_T0_ECX
	.p2align 4,,15
.globl op_movl_T1_ECX
	.type	op_movl_T1_ECX, @function
op_movl_T1_ECX:
.LFB374:
	movl	4(%rbp), %r12d
	ret
.LFE374:
	.size	op_movl_T1_ECX, .-op_movl_T1_ECX
	.p2align 4,,15
.globl op_movh_T0_ECX
	.type	op_movh_T0_ECX, @function
op_movh_T0_ECX:
.LFB375:
	movl	4(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE375:
	.size	op_movh_T0_ECX, .-op_movh_T0_ECX
	.p2align 4,,15
.globl op_movh_T1_ECX
	.type	op_movh_T1_ECX, @function
op_movh_T1_ECX:
.LFB376:
	movl	4(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE376:
	.size	op_movh_T1_ECX, .-op_movh_T1_ECX
	.p2align 4,,15
.globl op_movl_ECX_T0
	.type	op_movl_ECX_T0, @function
op_movl_ECX_T0:
.LFB377:
	movl	%ebx, 4(%rbp)
	ret
.LFE377:
	.size	op_movl_ECX_T0, .-op_movl_ECX_T0
	.p2align 4,,15
.globl op_movl_ECX_T1
	.type	op_movl_ECX_T1, @function
op_movl_ECX_T1:
.LFB378:
	movl	%r12d, 4(%rbp)
	ret
.LFE378:
	.size	op_movl_ECX_T1, .-op_movl_ECX_T1
	.p2align 4,,15
.globl op_movl_ECX_A0
	.type	op_movl_ECX_A0, @function
op_movl_ECX_A0:
.LFB379:
	movl	%r13d, 4(%rbp)
	ret
.LFE379:
	.size	op_movl_ECX_A0, .-op_movl_ECX_A0
	.p2align 4,,15
.globl op_cmovw_ECX_T1_T0
	.type	op_cmovw_ECX_T1_T0, @function
op_cmovw_ECX_T1_T0:
.LFB380:
	testl	%ebx, %ebx
	je	.L73
	movw	%r12w, 4(%rbp)
.L73:
	ret
.LFE380:
	.size	op_cmovw_ECX_T1_T0, .-op_cmovw_ECX_T1_T0
	.p2align 4,,15
.globl op_cmovl_ECX_T1_T0
	.type	op_cmovl_ECX_T1_T0, @function
op_cmovl_ECX_T1_T0:
.LFB381:
	testl	%ebx, %ebx
	je	.L77
	movl	%r12d, 4(%rbp)
.L77:
	ret
.LFE381:
	.size	op_cmovl_ECX_T1_T0, .-op_cmovl_ECX_T1_T0
	.p2align 4,,15
.globl op_movw_ECX_T0
	.type	op_movw_ECX_T0, @function
op_movw_ECX_T0:
.LFB382:
	movw	%bx, 4(%rbp)
	ret
.LFE382:
	.size	op_movw_ECX_T0, .-op_movw_ECX_T0
	.p2align 4,,15
.globl op_movw_ECX_T1
	.type	op_movw_ECX_T1, @function
op_movw_ECX_T1:
.LFB383:
	movw	%r12w, 4(%rbp)
	ret
.LFE383:
	.size	op_movw_ECX_T1, .-op_movw_ECX_T1
	.p2align 4,,15
.globl op_movw_ECX_A0
	.type	op_movw_ECX_A0, @function
op_movw_ECX_A0:
.LFB384:
	movw	%r13w, 4(%rbp)
	ret
.LFE384:
	.size	op_movw_ECX_A0, .-op_movw_ECX_A0
	.p2align 4,,15
.globl op_movb_ECX_T0
	.type	op_movb_ECX_T0, @function
op_movb_ECX_T0:
.LFB385:
	movb	%bl, 4(%rbp)
	ret
.LFE385:
	.size	op_movb_ECX_T0, .-op_movb_ECX_T0
	.p2align 4,,15
.globl op_movh_ECX_T0
	.type	op_movh_ECX_T0, @function
op_movh_ECX_T0:
.LFB386:
	movb	%bl, 5(%rbp)
	ret
.LFE386:
	.size	op_movh_ECX_T0, .-op_movh_ECX_T0
	.p2align 4,,15
.globl op_movb_ECX_T1
	.type	op_movb_ECX_T1, @function
op_movb_ECX_T1:
.LFB387:
	movb	%r12b, 4(%rbp)
	ret
.LFE387:
	.size	op_movb_ECX_T1, .-op_movb_ECX_T1
	.p2align 4,,15
.globl op_movh_ECX_T1
	.type	op_movh_ECX_T1, @function
op_movh_ECX_T1:
.LFB388:
	movb	%r12b, 5(%rbp)
	ret
.LFE388:
	.size	op_movh_ECX_T1, .-op_movh_ECX_T1
	.p2align 4,,15
.globl op_movl_A0_EDX
	.type	op_movl_A0_EDX, @function
op_movl_A0_EDX:
.LFB389:
	movl	8(%rbp), %r13d
	ret
.LFE389:
	.size	op_movl_A0_EDX, .-op_movl_A0_EDX
	.p2align 4,,15
.globl op_addl_A0_EDX
	.type	op_addl_A0_EDX, @function
op_addl_A0_EDX:
.LFB390:
	addl	8(%rbp), %r13d
	ret
.LFE390:
	.size	op_addl_A0_EDX, .-op_addl_A0_EDX
	.p2align 4,,15
.globl op_addl_A0_EDX_s1
	.type	op_addl_A0_EDX_s1, @function
op_addl_A0_EDX_s1:
.LFB391:
	movl	8(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE391:
	.size	op_addl_A0_EDX_s1, .-op_addl_A0_EDX_s1
	.p2align 4,,15
.globl op_addl_A0_EDX_s2
	.type	op_addl_A0_EDX_s2, @function
op_addl_A0_EDX_s2:
.LFB392:
	movl	8(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE392:
	.size	op_addl_A0_EDX_s2, .-op_addl_A0_EDX_s2
	.p2align 4,,15
.globl op_addl_A0_EDX_s3
	.type	op_addl_A0_EDX_s3, @function
op_addl_A0_EDX_s3:
.LFB393:
	movl	8(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE393:
	.size	op_addl_A0_EDX_s3, .-op_addl_A0_EDX_s3
	.p2align 4,,15
.globl op_movl_T0_EDX
	.type	op_movl_T0_EDX, @function
op_movl_T0_EDX:
.LFB394:
	movl	8(%rbp), %ebx
	ret
.LFE394:
	.size	op_movl_T0_EDX, .-op_movl_T0_EDX
	.p2align 4,,15
.globl op_movl_T1_EDX
	.type	op_movl_T1_EDX, @function
op_movl_T1_EDX:
.LFB395:
	movl	8(%rbp), %r12d
	ret
.LFE395:
	.size	op_movl_T1_EDX, .-op_movl_T1_EDX
	.p2align 4,,15
.globl op_movh_T0_EDX
	.type	op_movh_T0_EDX, @function
op_movh_T0_EDX:
.LFB396:
	movl	8(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE396:
	.size	op_movh_T0_EDX, .-op_movh_T0_EDX
	.p2align 4,,15
.globl op_movh_T1_EDX
	.type	op_movh_T1_EDX, @function
op_movh_T1_EDX:
.LFB397:
	movl	8(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE397:
	.size	op_movh_T1_EDX, .-op_movh_T1_EDX
	.p2align 4,,15
.globl op_movl_EDX_T0
	.type	op_movl_EDX_T0, @function
op_movl_EDX_T0:
.LFB398:
	movl	%ebx, 8(%rbp)
	ret
.LFE398:
	.size	op_movl_EDX_T0, .-op_movl_EDX_T0
	.p2align 4,,15
.globl op_movl_EDX_T1
	.type	op_movl_EDX_T1, @function
op_movl_EDX_T1:
.LFB399:
	movl	%r12d, 8(%rbp)
	ret
.LFE399:
	.size	op_movl_EDX_T1, .-op_movl_EDX_T1
	.p2align 4,,15
.globl op_movl_EDX_A0
	.type	op_movl_EDX_A0, @function
op_movl_EDX_A0:
.LFB400:
	movl	%r13d, 8(%rbp)
	ret
.LFE400:
	.size	op_movl_EDX_A0, .-op_movl_EDX_A0
	.p2align 4,,15
.globl op_cmovw_EDX_T1_T0
	.type	op_cmovw_EDX_T1_T0, @function
op_cmovw_EDX_T1_T0:
.LFB401:
	testl	%ebx, %ebx
	je	.L119
	movw	%r12w, 8(%rbp)
.L119:
	ret
.LFE401:
	.size	op_cmovw_EDX_T1_T0, .-op_cmovw_EDX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EDX_T1_T0
	.type	op_cmovl_EDX_T1_T0, @function
op_cmovl_EDX_T1_T0:
.LFB402:
	testl	%ebx, %ebx
	je	.L123
	movl	%r12d, 8(%rbp)
.L123:
	ret
.LFE402:
	.size	op_cmovl_EDX_T1_T0, .-op_cmovl_EDX_T1_T0
	.p2align 4,,15
.globl op_movw_EDX_T0
	.type	op_movw_EDX_T0, @function
op_movw_EDX_T0:
.LFB403:
	movw	%bx, 8(%rbp)
	ret
.LFE403:
	.size	op_movw_EDX_T0, .-op_movw_EDX_T0
	.p2align 4,,15
.globl op_movw_EDX_T1
	.type	op_movw_EDX_T1, @function
op_movw_EDX_T1:
.LFB404:
	movw	%r12w, 8(%rbp)
	ret
.LFE404:
	.size	op_movw_EDX_T1, .-op_movw_EDX_T1
	.p2align 4,,15
.globl op_movw_EDX_A0
	.type	op_movw_EDX_A0, @function
op_movw_EDX_A0:
.LFB405:
	movw	%r13w, 8(%rbp)
	ret
.LFE405:
	.size	op_movw_EDX_A0, .-op_movw_EDX_A0
	.p2align 4,,15
.globl op_movb_EDX_T0
	.type	op_movb_EDX_T0, @function
op_movb_EDX_T0:
.LFB406:
	movb	%bl, 8(%rbp)
	ret
.LFE406:
	.size	op_movb_EDX_T0, .-op_movb_EDX_T0
	.p2align 4,,15
.globl op_movh_EDX_T0
	.type	op_movh_EDX_T0, @function
op_movh_EDX_T0:
.LFB407:
	movb	%bl, 9(%rbp)
	ret
.LFE407:
	.size	op_movh_EDX_T0, .-op_movh_EDX_T0
	.p2align 4,,15
.globl op_movb_EDX_T1
	.type	op_movb_EDX_T1, @function
op_movb_EDX_T1:
.LFB408:
	movb	%r12b, 8(%rbp)
	ret
.LFE408:
	.size	op_movb_EDX_T1, .-op_movb_EDX_T1
	.p2align 4,,15
.globl op_movh_EDX_T1
	.type	op_movh_EDX_T1, @function
op_movh_EDX_T1:
.LFB409:
	movb	%r12b, 9(%rbp)
	ret
.LFE409:
	.size	op_movh_EDX_T1, .-op_movh_EDX_T1
	.p2align 4,,15
.globl op_movl_A0_EBX
	.type	op_movl_A0_EBX, @function
op_movl_A0_EBX:
.LFB410:
	movl	12(%rbp), %r13d
	ret
.LFE410:
	.size	op_movl_A0_EBX, .-op_movl_A0_EBX
	.p2align 4,,15
.globl op_addl_A0_EBX
	.type	op_addl_A0_EBX, @function
op_addl_A0_EBX:
.LFB411:
	addl	12(%rbp), %r13d
	ret
.LFE411:
	.size	op_addl_A0_EBX, .-op_addl_A0_EBX
	.p2align 4,,15
.globl op_addl_A0_EBX_s1
	.type	op_addl_A0_EBX_s1, @function
op_addl_A0_EBX_s1:
.LFB412:
	movl	12(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE412:
	.size	op_addl_A0_EBX_s1, .-op_addl_A0_EBX_s1
	.p2align 4,,15
.globl op_addl_A0_EBX_s2
	.type	op_addl_A0_EBX_s2, @function
op_addl_A0_EBX_s2:
.LFB413:
	movl	12(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE413:
	.size	op_addl_A0_EBX_s2, .-op_addl_A0_EBX_s2
	.p2align 4,,15
.globl op_addl_A0_EBX_s3
	.type	op_addl_A0_EBX_s3, @function
op_addl_A0_EBX_s3:
.LFB414:
	movl	12(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE414:
	.size	op_addl_A0_EBX_s3, .-op_addl_A0_EBX_s3
	.p2align 4,,15
.globl op_movl_T0_EBX
	.type	op_movl_T0_EBX, @function
op_movl_T0_EBX:
.LFB415:
	movl	12(%rbp), %ebx
	ret
.LFE415:
	.size	op_movl_T0_EBX, .-op_movl_T0_EBX
	.p2align 4,,15
.globl op_movl_T1_EBX
	.type	op_movl_T1_EBX, @function
op_movl_T1_EBX:
.LFB416:
	movl	12(%rbp), %r12d
	ret
.LFE416:
	.size	op_movl_T1_EBX, .-op_movl_T1_EBX
	.p2align 4,,15
.globl op_movh_T0_EBX
	.type	op_movh_T0_EBX, @function
op_movh_T0_EBX:
.LFB417:
	movl	12(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE417:
	.size	op_movh_T0_EBX, .-op_movh_T0_EBX
	.p2align 4,,15
.globl op_movh_T1_EBX
	.type	op_movh_T1_EBX, @function
op_movh_T1_EBX:
.LFB418:
	movl	12(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE418:
	.size	op_movh_T1_EBX, .-op_movh_T1_EBX
	.p2align 4,,15
.globl op_movl_EBX_T0
	.type	op_movl_EBX_T0, @function
op_movl_EBX_T0:
.LFB419:
	movl	%ebx, 12(%rbp)
	ret
.LFE419:
	.size	op_movl_EBX_T0, .-op_movl_EBX_T0
	.p2align 4,,15
.globl op_movl_EBX_T1
	.type	op_movl_EBX_T1, @function
op_movl_EBX_T1:
.LFB420:
	movl	%r12d, 12(%rbp)
	ret
.LFE420:
	.size	op_movl_EBX_T1, .-op_movl_EBX_T1
	.p2align 4,,15
.globl op_movl_EBX_A0
	.type	op_movl_EBX_A0, @function
op_movl_EBX_A0:
.LFB421:
	movl	%r13d, 12(%rbp)
	ret
.LFE421:
	.size	op_movl_EBX_A0, .-op_movl_EBX_A0
	.p2align 4,,15
.globl op_cmovw_EBX_T1_T0
	.type	op_cmovw_EBX_T1_T0, @function
op_cmovw_EBX_T1_T0:
.LFB422:
	testl	%ebx, %ebx
	je	.L165
	movw	%r12w, 12(%rbp)
.L165:
	ret
.LFE422:
	.size	op_cmovw_EBX_T1_T0, .-op_cmovw_EBX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EBX_T1_T0
	.type	op_cmovl_EBX_T1_T0, @function
op_cmovl_EBX_T1_T0:
.LFB423:
	testl	%ebx, %ebx
	je	.L169
	movl	%r12d, 12(%rbp)
.L169:
	ret
.LFE423:
	.size	op_cmovl_EBX_T1_T0, .-op_cmovl_EBX_T1_T0
	.p2align 4,,15
.globl op_movw_EBX_T0
	.type	op_movw_EBX_T0, @function
op_movw_EBX_T0:
.LFB424:
	movw	%bx, 12(%rbp)
	ret
.LFE424:
	.size	op_movw_EBX_T0, .-op_movw_EBX_T0
	.p2align 4,,15
.globl op_movw_EBX_T1
	.type	op_movw_EBX_T1, @function
op_movw_EBX_T1:
.LFB425:
	movw	%r12w, 12(%rbp)
	ret
.LFE425:
	.size	op_movw_EBX_T1, .-op_movw_EBX_T1
	.p2align 4,,15
.globl op_movw_EBX_A0
	.type	op_movw_EBX_A0, @function
op_movw_EBX_A0:
.LFB426:
	movw	%r13w, 12(%rbp)
	ret
.LFE426:
	.size	op_movw_EBX_A0, .-op_movw_EBX_A0
	.p2align 4,,15
.globl op_movb_EBX_T0
	.type	op_movb_EBX_T0, @function
op_movb_EBX_T0:
.LFB427:
	movb	%bl, 12(%rbp)
	ret
.LFE427:
	.size	op_movb_EBX_T0, .-op_movb_EBX_T0
	.p2align 4,,15
.globl op_movh_EBX_T0
	.type	op_movh_EBX_T0, @function
op_movh_EBX_T0:
.LFB428:
	movb	%bl, 13(%rbp)
	ret
.LFE428:
	.size	op_movh_EBX_T0, .-op_movh_EBX_T0
	.p2align 4,,15
.globl op_movb_EBX_T1
	.type	op_movb_EBX_T1, @function
op_movb_EBX_T1:
.LFB429:
	movb	%r12b, 12(%rbp)
	ret
.LFE429:
	.size	op_movb_EBX_T1, .-op_movb_EBX_T1
	.p2align 4,,15
.globl op_movh_EBX_T1
	.type	op_movh_EBX_T1, @function
op_movh_EBX_T1:
.LFB430:
	movb	%r12b, 13(%rbp)
	ret
.LFE430:
	.size	op_movh_EBX_T1, .-op_movh_EBX_T1
	.p2align 4,,15
.globl op_movl_A0_ESP
	.type	op_movl_A0_ESP, @function
op_movl_A0_ESP:
.LFB431:
	movl	16(%rbp), %r13d
	ret
.LFE431:
	.size	op_movl_A0_ESP, .-op_movl_A0_ESP
	.p2align 4,,15
.globl op_addl_A0_ESP
	.type	op_addl_A0_ESP, @function
op_addl_A0_ESP:
.LFB432:
	addl	16(%rbp), %r13d
	ret
.LFE432:
	.size	op_addl_A0_ESP, .-op_addl_A0_ESP
	.p2align 4,,15
.globl op_addl_A0_ESP_s1
	.type	op_addl_A0_ESP_s1, @function
op_addl_A0_ESP_s1:
.LFB433:
	movl	16(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE433:
	.size	op_addl_A0_ESP_s1, .-op_addl_A0_ESP_s1
	.p2align 4,,15
.globl op_addl_A0_ESP_s2
	.type	op_addl_A0_ESP_s2, @function
op_addl_A0_ESP_s2:
.LFB434:
	movl	16(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE434:
	.size	op_addl_A0_ESP_s2, .-op_addl_A0_ESP_s2
	.p2align 4,,15
.globl op_addl_A0_ESP_s3
	.type	op_addl_A0_ESP_s3, @function
op_addl_A0_ESP_s3:
.LFB435:
	movl	16(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE435:
	.size	op_addl_A0_ESP_s3, .-op_addl_A0_ESP_s3
	.p2align 4,,15
.globl op_movl_T0_ESP
	.type	op_movl_T0_ESP, @function
op_movl_T0_ESP:
.LFB436:
	movl	16(%rbp), %ebx
	ret
.LFE436:
	.size	op_movl_T0_ESP, .-op_movl_T0_ESP
	.p2align 4,,15
.globl op_movl_T1_ESP
	.type	op_movl_T1_ESP, @function
op_movl_T1_ESP:
.LFB437:
	movl	16(%rbp), %r12d
	ret
.LFE437:
	.size	op_movl_T1_ESP, .-op_movl_T1_ESP
	.p2align 4,,15
.globl op_movh_T0_ESP
	.type	op_movh_T0_ESP, @function
op_movh_T0_ESP:
.LFB438:
	movl	16(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE438:
	.size	op_movh_T0_ESP, .-op_movh_T0_ESP
	.p2align 4,,15
.globl op_movh_T1_ESP
	.type	op_movh_T1_ESP, @function
op_movh_T1_ESP:
.LFB439:
	movl	16(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE439:
	.size	op_movh_T1_ESP, .-op_movh_T1_ESP
	.p2align 4,,15
.globl op_movl_ESP_T0
	.type	op_movl_ESP_T0, @function
op_movl_ESP_T0:
.LFB440:
	movl	%ebx, 16(%rbp)
	ret
.LFE440:
	.size	op_movl_ESP_T0, .-op_movl_ESP_T0
	.p2align 4,,15
.globl op_movl_ESP_T1
	.type	op_movl_ESP_T1, @function
op_movl_ESP_T1:
.LFB441:
	movl	%r12d, 16(%rbp)
	ret
.LFE441:
	.size	op_movl_ESP_T1, .-op_movl_ESP_T1
	.p2align 4,,15
.globl op_movl_ESP_A0
	.type	op_movl_ESP_A0, @function
op_movl_ESP_A0:
.LFB442:
	movl	%r13d, 16(%rbp)
	ret
.LFE442:
	.size	op_movl_ESP_A0, .-op_movl_ESP_A0
	.p2align 4,,15
.globl op_cmovw_ESP_T1_T0
	.type	op_cmovw_ESP_T1_T0, @function
op_cmovw_ESP_T1_T0:
.LFB443:
	testl	%ebx, %ebx
	je	.L211
	movw	%r12w, 16(%rbp)
.L211:
	ret
.LFE443:
	.size	op_cmovw_ESP_T1_T0, .-op_cmovw_ESP_T1_T0
	.p2align 4,,15
.globl op_cmovl_ESP_T1_T0
	.type	op_cmovl_ESP_T1_T0, @function
op_cmovl_ESP_T1_T0:
.LFB444:
	testl	%ebx, %ebx
	je	.L215
	movl	%r12d, 16(%rbp)
.L215:
	ret
.LFE444:
	.size	op_cmovl_ESP_T1_T0, .-op_cmovl_ESP_T1_T0
	.p2align 4,,15
.globl op_movw_ESP_T0
	.type	op_movw_ESP_T0, @function
op_movw_ESP_T0:
.LFB445:
	movw	%bx, 16(%rbp)
	ret
.LFE445:
	.size	op_movw_ESP_T0, .-op_movw_ESP_T0
	.p2align 4,,15
.globl op_movw_ESP_T1
	.type	op_movw_ESP_T1, @function
op_movw_ESP_T1:
.LFB446:
	movw	%r12w, 16(%rbp)
	ret
.LFE446:
	.size	op_movw_ESP_T1, .-op_movw_ESP_T1
	.p2align 4,,15
.globl op_movw_ESP_A0
	.type	op_movw_ESP_A0, @function
op_movw_ESP_A0:
.LFB447:
	movw	%r13w, 16(%rbp)
	ret
.LFE447:
	.size	op_movw_ESP_A0, .-op_movw_ESP_A0
	.p2align 4,,15
.globl op_movb_ESP_T0
	.type	op_movb_ESP_T0, @function
op_movb_ESP_T0:
.LFB448:
	movb	%bl, 16(%rbp)
	ret
.LFE448:
	.size	op_movb_ESP_T0, .-op_movb_ESP_T0
	.p2align 4,,15
.globl op_movh_ESP_T0
	.type	op_movh_ESP_T0, @function
op_movh_ESP_T0:
.LFB449:
	movb	%bl, 17(%rbp)
	ret
.LFE449:
	.size	op_movh_ESP_T0, .-op_movh_ESP_T0
	.p2align 4,,15
.globl op_movb_ESP_T1
	.type	op_movb_ESP_T1, @function
op_movb_ESP_T1:
.LFB450:
	movb	%r12b, 16(%rbp)
	ret
.LFE450:
	.size	op_movb_ESP_T1, .-op_movb_ESP_T1
	.p2align 4,,15
.globl op_movh_ESP_T1
	.type	op_movh_ESP_T1, @function
op_movh_ESP_T1:
.LFB451:
	movb	%r12b, 17(%rbp)
	ret
.LFE451:
	.size	op_movh_ESP_T1, .-op_movh_ESP_T1
	.p2align 4,,15
.globl op_movl_A0_EBP
	.type	op_movl_A0_EBP, @function
op_movl_A0_EBP:
.LFB452:
	movl	20(%rbp), %r13d
	ret
.LFE452:
	.size	op_movl_A0_EBP, .-op_movl_A0_EBP
	.p2align 4,,15
.globl op_addl_A0_EBP
	.type	op_addl_A0_EBP, @function
op_addl_A0_EBP:
.LFB453:
	addl	20(%rbp), %r13d
	ret
.LFE453:
	.size	op_addl_A0_EBP, .-op_addl_A0_EBP
	.p2align 4,,15
.globl op_addl_A0_EBP_s1
	.type	op_addl_A0_EBP_s1, @function
op_addl_A0_EBP_s1:
.LFB454:
	movl	20(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE454:
	.size	op_addl_A0_EBP_s1, .-op_addl_A0_EBP_s1
	.p2align 4,,15
.globl op_addl_A0_EBP_s2
	.type	op_addl_A0_EBP_s2, @function
op_addl_A0_EBP_s2:
.LFB455:
	movl	20(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE455:
	.size	op_addl_A0_EBP_s2, .-op_addl_A0_EBP_s2
	.p2align 4,,15
.globl op_addl_A0_EBP_s3
	.type	op_addl_A0_EBP_s3, @function
op_addl_A0_EBP_s3:
.LFB456:
	movl	20(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE456:
	.size	op_addl_A0_EBP_s3, .-op_addl_A0_EBP_s3
	.p2align 4,,15
.globl op_movl_T0_EBP
	.type	op_movl_T0_EBP, @function
op_movl_T0_EBP:
.LFB457:
	movl	20(%rbp), %ebx
	ret
.LFE457:
	.size	op_movl_T0_EBP, .-op_movl_T0_EBP
	.p2align 4,,15
.globl op_movl_T1_EBP
	.type	op_movl_T1_EBP, @function
op_movl_T1_EBP:
.LFB458:
	movl	20(%rbp), %r12d
	ret
.LFE458:
	.size	op_movl_T1_EBP, .-op_movl_T1_EBP
	.p2align 4,,15
.globl op_movh_T0_EBP
	.type	op_movh_T0_EBP, @function
op_movh_T0_EBP:
.LFB459:
	movl	20(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE459:
	.size	op_movh_T0_EBP, .-op_movh_T0_EBP
	.p2align 4,,15
.globl op_movh_T1_EBP
	.type	op_movh_T1_EBP, @function
op_movh_T1_EBP:
.LFB460:
	movl	20(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE460:
	.size	op_movh_T1_EBP, .-op_movh_T1_EBP
	.p2align 4,,15
.globl op_movl_EBP_T0
	.type	op_movl_EBP_T0, @function
op_movl_EBP_T0:
.LFB461:
	movl	%ebx, 20(%rbp)
	ret
.LFE461:
	.size	op_movl_EBP_T0, .-op_movl_EBP_T0
	.p2align 4,,15
.globl op_movl_EBP_T1
	.type	op_movl_EBP_T1, @function
op_movl_EBP_T1:
.LFB462:
	movl	%r12d, 20(%rbp)
	ret
.LFE462:
	.size	op_movl_EBP_T1, .-op_movl_EBP_T1
	.p2align 4,,15
.globl op_movl_EBP_A0
	.type	op_movl_EBP_A0, @function
op_movl_EBP_A0:
.LFB463:
	movl	%r13d, 20(%rbp)
	ret
.LFE463:
	.size	op_movl_EBP_A0, .-op_movl_EBP_A0
	.p2align 4,,15
.globl op_cmovw_EBP_T1_T0
	.type	op_cmovw_EBP_T1_T0, @function
op_cmovw_EBP_T1_T0:
.LFB464:
	testl	%ebx, %ebx
	je	.L257
	movw	%r12w, 20(%rbp)
.L257:
	ret
.LFE464:
	.size	op_cmovw_EBP_T1_T0, .-op_cmovw_EBP_T1_T0
	.p2align 4,,15
.globl op_cmovl_EBP_T1_T0
	.type	op_cmovl_EBP_T1_T0, @function
op_cmovl_EBP_T1_T0:
.LFB465:
	testl	%ebx, %ebx
	je	.L261
	movl	%r12d, 20(%rbp)
.L261:
	ret
.LFE465:
	.size	op_cmovl_EBP_T1_T0, .-op_cmovl_EBP_T1_T0
	.p2align 4,,15
.globl op_movw_EBP_T0
	.type	op_movw_EBP_T0, @function
op_movw_EBP_T0:
.LFB466:
	movw	%bx, 20(%rbp)
	ret
.LFE466:
	.size	op_movw_EBP_T0, .-op_movw_EBP_T0
	.p2align 4,,15
.globl op_movw_EBP_T1
	.type	op_movw_EBP_T1, @function
op_movw_EBP_T1:
.LFB467:
	movw	%r12w, 20(%rbp)
	ret
.LFE467:
	.size	op_movw_EBP_T1, .-op_movw_EBP_T1
	.p2align 4,,15
.globl op_movw_EBP_A0
	.type	op_movw_EBP_A0, @function
op_movw_EBP_A0:
.LFB468:
	movw	%r13w, 20(%rbp)
	ret
.LFE468:
	.size	op_movw_EBP_A0, .-op_movw_EBP_A0
	.p2align 4,,15
.globl op_movb_EBP_T0
	.type	op_movb_EBP_T0, @function
op_movb_EBP_T0:
.LFB469:
	movb	%bl, 20(%rbp)
	ret
.LFE469:
	.size	op_movb_EBP_T0, .-op_movb_EBP_T0
	.p2align 4,,15
.globl op_movh_EBP_T0
	.type	op_movh_EBP_T0, @function
op_movh_EBP_T0:
.LFB470:
	movb	%bl, 21(%rbp)
	ret
.LFE470:
	.size	op_movh_EBP_T0, .-op_movh_EBP_T0
	.p2align 4,,15
.globl op_movb_EBP_T1
	.type	op_movb_EBP_T1, @function
op_movb_EBP_T1:
.LFB471:
	movb	%r12b, 20(%rbp)
	ret
.LFE471:
	.size	op_movb_EBP_T1, .-op_movb_EBP_T1
	.p2align 4,,15
.globl op_movh_EBP_T1
	.type	op_movh_EBP_T1, @function
op_movh_EBP_T1:
.LFB472:
	movb	%r12b, 21(%rbp)
	ret
.LFE472:
	.size	op_movh_EBP_T1, .-op_movh_EBP_T1
	.p2align 4,,15
.globl op_movl_A0_ESI
	.type	op_movl_A0_ESI, @function
op_movl_A0_ESI:
.LFB473:
	movl	24(%rbp), %r13d
	ret
.LFE473:
	.size	op_movl_A0_ESI, .-op_movl_A0_ESI
	.p2align 4,,15
.globl op_addl_A0_ESI
	.type	op_addl_A0_ESI, @function
op_addl_A0_ESI:
.LFB474:
	addl	24(%rbp), %r13d
	ret
.LFE474:
	.size	op_addl_A0_ESI, .-op_addl_A0_ESI
	.p2align 4,,15
.globl op_addl_A0_ESI_s1
	.type	op_addl_A0_ESI_s1, @function
op_addl_A0_ESI_s1:
.LFB475:
	movl	24(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE475:
	.size	op_addl_A0_ESI_s1, .-op_addl_A0_ESI_s1
	.p2align 4,,15
.globl op_addl_A0_ESI_s2
	.type	op_addl_A0_ESI_s2, @function
op_addl_A0_ESI_s2:
.LFB476:
	movl	24(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE476:
	.size	op_addl_A0_ESI_s2, .-op_addl_A0_ESI_s2
	.p2align 4,,15
.globl op_addl_A0_ESI_s3
	.type	op_addl_A0_ESI_s3, @function
op_addl_A0_ESI_s3:
.LFB477:
	movl	24(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE477:
	.size	op_addl_A0_ESI_s3, .-op_addl_A0_ESI_s3
	.p2align 4,,15
.globl op_movl_T0_ESI
	.type	op_movl_T0_ESI, @function
op_movl_T0_ESI:
.LFB478:
	movl	24(%rbp), %ebx
	ret
.LFE478:
	.size	op_movl_T0_ESI, .-op_movl_T0_ESI
	.p2align 4,,15
.globl op_movl_T1_ESI
	.type	op_movl_T1_ESI, @function
op_movl_T1_ESI:
.LFB479:
	movl	24(%rbp), %r12d
	ret
.LFE479:
	.size	op_movl_T1_ESI, .-op_movl_T1_ESI
	.p2align 4,,15
.globl op_movh_T0_ESI
	.type	op_movh_T0_ESI, @function
op_movh_T0_ESI:
.LFB480:
	movl	24(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE480:
	.size	op_movh_T0_ESI, .-op_movh_T0_ESI
	.p2align 4,,15
.globl op_movh_T1_ESI
	.type	op_movh_T1_ESI, @function
op_movh_T1_ESI:
.LFB481:
	movl	24(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE481:
	.size	op_movh_T1_ESI, .-op_movh_T1_ESI
	.p2align 4,,15
.globl op_movl_ESI_T0
	.type	op_movl_ESI_T0, @function
op_movl_ESI_T0:
.LFB482:
	movl	%ebx, 24(%rbp)
	ret
.LFE482:
	.size	op_movl_ESI_T0, .-op_movl_ESI_T0
	.p2align 4,,15
.globl op_movl_ESI_T1
	.type	op_movl_ESI_T1, @function
op_movl_ESI_T1:
.LFB483:
	movl	%r12d, 24(%rbp)
	ret
.LFE483:
	.size	op_movl_ESI_T1, .-op_movl_ESI_T1
	.p2align 4,,15
.globl op_movl_ESI_A0
	.type	op_movl_ESI_A0, @function
op_movl_ESI_A0:
.LFB484:
	movl	%r13d, 24(%rbp)
	ret
.LFE484:
	.size	op_movl_ESI_A0, .-op_movl_ESI_A0
	.p2align 4,,15
.globl op_cmovw_ESI_T1_T0
	.type	op_cmovw_ESI_T1_T0, @function
op_cmovw_ESI_T1_T0:
.LFB485:
	testl	%ebx, %ebx
	je	.L303
	movw	%r12w, 24(%rbp)
.L303:
	ret
.LFE485:
	.size	op_cmovw_ESI_T1_T0, .-op_cmovw_ESI_T1_T0
	.p2align 4,,15
.globl op_cmovl_ESI_T1_T0
	.type	op_cmovl_ESI_T1_T0, @function
op_cmovl_ESI_T1_T0:
.LFB486:
	testl	%ebx, %ebx
	je	.L307
	movl	%r12d, 24(%rbp)
.L307:
	ret
.LFE486:
	.size	op_cmovl_ESI_T1_T0, .-op_cmovl_ESI_T1_T0
	.p2align 4,,15
.globl op_movw_ESI_T0
	.type	op_movw_ESI_T0, @function
op_movw_ESI_T0:
.LFB487:
	movw	%bx, 24(%rbp)
	ret
.LFE487:
	.size	op_movw_ESI_T0, .-op_movw_ESI_T0
	.p2align 4,,15
.globl op_movw_ESI_T1
	.type	op_movw_ESI_T1, @function
op_movw_ESI_T1:
.LFB488:
	movw	%r12w, 24(%rbp)
	ret
.LFE488:
	.size	op_movw_ESI_T1, .-op_movw_ESI_T1
	.p2align 4,,15
.globl op_movw_ESI_A0
	.type	op_movw_ESI_A0, @function
op_movw_ESI_A0:
.LFB489:
	movw	%r13w, 24(%rbp)
	ret
.LFE489:
	.size	op_movw_ESI_A0, .-op_movw_ESI_A0
	.p2align 4,,15
.globl op_movb_ESI_T0
	.type	op_movb_ESI_T0, @function
op_movb_ESI_T0:
.LFB490:
	movb	%bl, 24(%rbp)
	ret
.LFE490:
	.size	op_movb_ESI_T0, .-op_movb_ESI_T0
	.p2align 4,,15
.globl op_movh_ESI_T0
	.type	op_movh_ESI_T0, @function
op_movh_ESI_T0:
.LFB491:
	movb	%bl, 25(%rbp)
	ret
.LFE491:
	.size	op_movh_ESI_T0, .-op_movh_ESI_T0
	.p2align 4,,15
.globl op_movb_ESI_T1
	.type	op_movb_ESI_T1, @function
op_movb_ESI_T1:
.LFB492:
	movb	%r12b, 24(%rbp)
	ret
.LFE492:
	.size	op_movb_ESI_T1, .-op_movb_ESI_T1
	.p2align 4,,15
.globl op_movh_ESI_T1
	.type	op_movh_ESI_T1, @function
op_movh_ESI_T1:
.LFB493:
	movb	%r12b, 25(%rbp)
	ret
.LFE493:
	.size	op_movh_ESI_T1, .-op_movh_ESI_T1
	.p2align 4,,15
.globl op_movl_A0_EDI
	.type	op_movl_A0_EDI, @function
op_movl_A0_EDI:
.LFB494:
	movl	28(%rbp), %r13d
	ret
.LFE494:
	.size	op_movl_A0_EDI, .-op_movl_A0_EDI
	.p2align 4,,15
.globl op_addl_A0_EDI
	.type	op_addl_A0_EDI, @function
op_addl_A0_EDI:
.LFB495:
	addl	28(%rbp), %r13d
	ret
.LFE495:
	.size	op_addl_A0_EDI, .-op_addl_A0_EDI
	.p2align 4,,15
.globl op_addl_A0_EDI_s1
	.type	op_addl_A0_EDI_s1, @function
op_addl_A0_EDI_s1:
.LFB496:
	movl	28(%rbp), %eax
	leal	(%r13,%rax,2), %r13d
	ret
.LFE496:
	.size	op_addl_A0_EDI_s1, .-op_addl_A0_EDI_s1
	.p2align 4,,15
.globl op_addl_A0_EDI_s2
	.type	op_addl_A0_EDI_s2, @function
op_addl_A0_EDI_s2:
.LFB497:
	movl	28(%rbp), %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE497:
	.size	op_addl_A0_EDI_s2, .-op_addl_A0_EDI_s2
	.p2align 4,,15
.globl op_addl_A0_EDI_s3
	.type	op_addl_A0_EDI_s3, @function
op_addl_A0_EDI_s3:
.LFB498:
	movl	28(%rbp), %eax
	leal	(%r13,%rax,8), %r13d
	ret
.LFE498:
	.size	op_addl_A0_EDI_s3, .-op_addl_A0_EDI_s3
	.p2align 4,,15
.globl op_movl_T0_EDI
	.type	op_movl_T0_EDI, @function
op_movl_T0_EDI:
.LFB499:
	movl	28(%rbp), %ebx
	ret
.LFE499:
	.size	op_movl_T0_EDI, .-op_movl_T0_EDI
	.p2align 4,,15
.globl op_movl_T1_EDI
	.type	op_movl_T1_EDI, @function
op_movl_T1_EDI:
.LFB500:
	movl	28(%rbp), %r12d
	ret
.LFE500:
	.size	op_movl_T1_EDI, .-op_movl_T1_EDI
	.p2align 4,,15
.globl op_movh_T0_EDI
	.type	op_movh_T0_EDI, @function
op_movh_T0_EDI:
.LFB501:
	movl	28(%rbp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
.LFE501:
	.size	op_movh_T0_EDI, .-op_movh_T0_EDI
	.p2align 4,,15
.globl op_movh_T1_EDI
	.type	op_movh_T1_EDI, @function
op_movh_T1_EDI:
.LFB502:
	movl	28(%rbp), %eax
	movl	%eax, %r12d
	shrl	$8, %r12d
	ret
.LFE502:
	.size	op_movh_T1_EDI, .-op_movh_T1_EDI
	.p2align 4,,15
.globl op_movl_EDI_T0
	.type	op_movl_EDI_T0, @function
op_movl_EDI_T0:
.LFB503:
	movl	%ebx, 28(%rbp)
	ret
.LFE503:
	.size	op_movl_EDI_T0, .-op_movl_EDI_T0
	.p2align 4,,15
.globl op_movl_EDI_T1
	.type	op_movl_EDI_T1, @function
op_movl_EDI_T1:
.LFB504:
	movl	%r12d, 28(%rbp)
	ret
.LFE504:
	.size	op_movl_EDI_T1, .-op_movl_EDI_T1
	.p2align 4,,15
.globl op_movl_EDI_A0
	.type	op_movl_EDI_A0, @function
op_movl_EDI_A0:
.LFB505:
	movl	%r13d, 28(%rbp)
	ret
.LFE505:
	.size	op_movl_EDI_A0, .-op_movl_EDI_A0
	.p2align 4,,15
.globl op_cmovw_EDI_T1_T0
	.type	op_cmovw_EDI_T1_T0, @function
op_cmovw_EDI_T1_T0:
.LFB506:
	testl	%ebx, %ebx
	je	.L349
	movw	%r12w, 28(%rbp)
.L349:
	ret
.LFE506:
	.size	op_cmovw_EDI_T1_T0, .-op_cmovw_EDI_T1_T0
	.p2align 4,,15
.globl op_cmovl_EDI_T1_T0
	.type	op_cmovl_EDI_T1_T0, @function
op_cmovl_EDI_T1_T0:
.LFB507:
	testl	%ebx, %ebx
	je	.L353
	movl	%r12d, 28(%rbp)
.L353:
	ret
.LFE507:
	.size	op_cmovl_EDI_T1_T0, .-op_cmovl_EDI_T1_T0
	.p2align 4,,15
.globl op_movw_EDI_T0
	.type	op_movw_EDI_T0, @function
op_movw_EDI_T0:
.LFB508:
	movw	%bx, 28(%rbp)
	ret
.LFE508:
	.size	op_movw_EDI_T0, .-op_movw_EDI_T0
	.p2align 4,,15
.globl op_movw_EDI_T1
	.type	op_movw_EDI_T1, @function
op_movw_EDI_T1:
.LFB509:
	movw	%r12w, 28(%rbp)
	ret
.LFE509:
	.size	op_movw_EDI_T1, .-op_movw_EDI_T1
	.p2align 4,,15
.globl op_movw_EDI_A0
	.type	op_movw_EDI_A0, @function
op_movw_EDI_A0:
.LFB510:
	movw	%r13w, 28(%rbp)
	ret
.LFE510:
	.size	op_movw_EDI_A0, .-op_movw_EDI_A0
	.p2align 4,,15
.globl op_movb_EDI_T0
	.type	op_movb_EDI_T0, @function
op_movb_EDI_T0:
.LFB511:
	movb	%bl, 28(%rbp)
	ret
.LFE511:
	.size	op_movb_EDI_T0, .-op_movb_EDI_T0
	.p2align 4,,15
.globl op_movh_EDI_T0
	.type	op_movh_EDI_T0, @function
op_movh_EDI_T0:
.LFB512:
	movb	%bl, 29(%rbp)
	ret
.LFE512:
	.size	op_movh_EDI_T0, .-op_movh_EDI_T0
	.p2align 4,,15
.globl op_movb_EDI_T1
	.type	op_movb_EDI_T1, @function
op_movb_EDI_T1:
.LFB513:
	movb	%r12b, 28(%rbp)
	ret
.LFE513:
	.size	op_movb_EDI_T1, .-op_movb_EDI_T1
	.p2align 4,,15
.globl op_movh_EDI_T1
	.type	op_movh_EDI_T1, @function
op_movh_EDI_T1:
.LFB514:
	movb	%r12b, 29(%rbp)
	ret
.LFE514:
	.size	op_movh_EDI_T1, .-op_movh_EDI_T1
	.p2align 4,,15
.globl op_update2_cc
	.type	op_update2_cc, @function
op_update2_cc:
.LFB515:
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	ret
.LFE515:
	.size	op_update2_cc, .-op_update2_cc
	.p2align 4,,15
.globl op_update1_cc
	.type	op_update1_cc, @function
op_update1_cc:
.LFB516:
	movl	%ebx, 44(%rbp)
	ret
.LFE516:
	.size	op_update1_cc, .-op_update1_cc
	.p2align 4,,15
.globl op_update_neg_cc
	.type	op_update_neg_cc, @function
op_update_neg_cc:
.LFB517:
	movl	%ebx, %eax
	movl	%ebx, 44(%rbp)
	negl	%eax
	movl	%eax, 40(%rbp)
	ret
.LFE517:
	.size	op_update_neg_cc, .-op_update_neg_cc
	.p2align 4,,15
.globl op_cmpl_T0_T1_cc
	.type	op_cmpl_T0_T1_cc, @function
op_cmpl_T0_T1_cc:
.LFB518:
	movl	%ebx, %eax
	movl	%r12d, 40(%rbp)
	subl	%r12d, %eax
	movl	%eax, 44(%rbp)
	ret
.LFE518:
	.size	op_cmpl_T0_T1_cc, .-op_cmpl_T0_T1_cc
	.p2align 4,,15
.globl op_update_inc_cc
	.type	op_update_inc_cc, @function
op_update_inc_cc:
.LFB519:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI0:
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, 40(%r14)
	movl	%ebx, 44(%rbp)
	popq	%r14
	ret
.LFE519:
	.size	op_update_inc_cc, .-op_update_inc_cc
	.p2align 4,,15
.globl op_testl_T0_T1_cc
	.type	op_testl_T0_T1_cc, @function
op_testl_T0_T1_cc:
.LFB520:
	movl	%ebx, %eax
	andl	%r12d, %eax
	movl	%eax, 44(%rbp)
	ret
.LFE520:
	.size	op_testl_T0_T1_cc, .-op_testl_T0_T1_cc
	.p2align 4,,15
.globl op_addl_T0_T1
	.type	op_addl_T0_T1, @function
op_addl_T0_T1:
.LFB521:
	addl	%r12d, %ebx
	ret
.LFE521:
	.size	op_addl_T0_T1, .-op_addl_T0_T1
	.p2align 4,,15
.globl op_orl_T0_T1
	.type	op_orl_T0_T1, @function
op_orl_T0_T1:
.LFB522:
	orl	%r12d, %ebx
	ret
.LFE522:
	.size	op_orl_T0_T1, .-op_orl_T0_T1
	.p2align 4,,15
.globl op_andl_T0_T1
	.type	op_andl_T0_T1, @function
op_andl_T0_T1:
.LFB523:
	andl	%r12d, %ebx
	ret
.LFE523:
	.size	op_andl_T0_T1, .-op_andl_T0_T1
	.p2align 4,,15
.globl op_subl_T0_T1
	.type	op_subl_T0_T1, @function
op_subl_T0_T1:
.LFB524:
	subl	%r12d, %ebx
	ret
.LFE524:
	.size	op_subl_T0_T1, .-op_subl_T0_T1
	.p2align 4,,15
.globl op_xorl_T0_T1
	.type	op_xorl_T0_T1, @function
op_xorl_T0_T1:
.LFB525:
	xorl	%r12d, %ebx
	ret
.LFE525:
	.size	op_xorl_T0_T1, .-op_xorl_T0_T1
	.p2align 4,,15
.globl op_negl_T0
	.type	op_negl_T0, @function
op_negl_T0:
.LFB526:
	negl	%ebx
	ret
.LFE526:
	.size	op_negl_T0, .-op_negl_T0
	.p2align 4,,15
.globl op_incl_T0
	.type	op_incl_T0, @function
op_incl_T0:
.LFB527:
	incl	%ebx
	ret
.LFE527:
	.size	op_incl_T0, .-op_incl_T0
	.p2align 4,,15
.globl op_decl_T0
	.type	op_decl_T0, @function
op_decl_T0:
.LFB528:
	decl	%ebx
	ret
.LFE528:
	.size	op_decl_T0, .-op_decl_T0
	.p2align 4,,15
.globl op_notl_T0
	.type	op_notl_T0, @function
op_notl_T0:
.LFB529:
	notl	%ebx
	ret
.LFE529:
	.size	op_notl_T0, .-op_notl_T0
	.p2align 4,,15
.globl op_bswapl_T0
	.type	op_bswapl_T0, @function
op_bswapl_T0:
.LFB530:
	movl	%ebx, %eax
	movl	%ebx, %ecx
	movl	%ebx, %edx
	andl	$65280, %eax
	sall	$24, %ecx
	shrl	$24, %edx
	sall	$8, %eax
	orl	%eax, %ecx
	movl	%ebx, %eax
	andl	$16711680, %eax
	movl	%ecx, %ebx
	shrl	$8, %eax
	orl	%edx, %eax
	orl	%eax, %ebx
	ret
.LFE530:
	.size	op_bswapl_T0, .-op_bswapl_T0
	.p2align 4,,15
.globl op_mulb_AL_T0
	.type	op_mulb_AL_T0, @function
op_mulb_AL_T0:
.LFB531:
	movl	(%rbp), %edx
	movzbl	%bl, %ecx
	movzbl	%dl, %eax
	xorw	%dx, %dx
	imull	%ecx, %eax
	orl	%eax, %edx
	movl	%eax, 44(%rbp)
	andl	$65280, %eax
	movl	%edx, (%rbp)
	movl	%eax, 40(%rbp)
	ret
.LFE531:
	.size	op_mulb_AL_T0, .-op_mulb_AL_T0
	.p2align 4,,15
.globl op_imulb_AL_T0
	.type	op_imulb_AL_T0, @function
op_imulb_AL_T0:
.LFB532:
	movl	(%rbp), %eax
	movsbl	%bl,%edx
	movsbl	%al,%ecx
	xorw	%ax, %ax
	imull	%edx, %ecx
	movzwl	%cx,%edx
	movl	%ecx, 44(%rbp)
	orl	%edx, %eax
	movl	%eax, (%rbp)
	movsbl	%cl,%eax
	cmpl	%eax, %ecx
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE532:
	.size	op_imulb_AL_T0, .-op_imulb_AL_T0
	.p2align 4,,15
.globl op_mulw_AX_T0
	.type	op_mulw_AX_T0, @function
op_mulw_AX_T0:
.LFB533:
	movl	(%rbp), %eax
	movzwl	%bx, %edx
	movzwl	%ax, %ecx
	xorw	%ax, %ax
	imull	%edx, %ecx
	movzwl	%cx,%edx
	movl	%ecx, 44(%rbp)
	orl	%edx, %eax
	movl	%eax, (%rbp)
	movl	%ecx, %eax
	shrl	$16, %eax
	movw	%ax, 8(%rbp)
	movl	%eax, 40(%rbp)
	ret
.LFE533:
	.size	op_mulw_AX_T0, .-op_mulw_AX_T0
	.p2align 4,,15
.globl op_imulw_AX_T0
	.type	op_imulw_AX_T0, @function
op_imulw_AX_T0:
.LFB534:
	movl	(%rbp), %edx
	movswl	%bx,%ecx
	movswl	%dx,%eax
	xorw	%dx, %dx
	imull	%ecx, %eax
	movzwl	%ax,%ecx
	movl	%eax, 44(%rbp)
	orl	%ecx, %edx
	movl	%edx, (%rbp)
	movl	%eax, %edx
	shrl	$16, %edx
	movw	%dx, 8(%rbp)
	movswl	%ax,%edx
	cmpl	%edx, %eax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE534:
	.size	op_imulw_AX_T0, .-op_imulw_AX_T0
	.p2align 4,,15
.globl op_mull_EAX_T0
	.type	op_mull_EAX_T0, @function
op_mull_EAX_T0:
.LFB535:
	mov	(%rbp), %eax
	mov	%ebx, %edx
	imulq	%rdx, %rax
	movq	%rax, %rdx
	movl	%eax, (%rbp)
	movl	%eax, 44(%rbp)
	shrq	$32, %rdx
	movl	%edx, 8(%rbp)
	movl	%edx, 40(%rbp)
	ret
.LFE535:
	.size	op_mull_EAX_T0, .-op_mull_EAX_T0
	.p2align 4,,15
.globl op_imull_EAX_T0
	.type	op_imull_EAX_T0, @function
op_imull_EAX_T0:
.LFB536:
	movslq	(%rbp),%rax
	movslq	%ebx,%rdx
	imulq	%rdx, %rax
	movq	%rax, %rdx
	movl	%eax, (%rbp)
	movl	%eax, 44(%rbp)
	sarq	$32, %rdx
	movl	%edx, 8(%rbp)
	movslq	%eax,%rdx
	cmpq	%rdx, %rax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE536:
	.size	op_imull_EAX_T0, .-op_imull_EAX_T0
	.p2align 4,,15
.globl op_imulw_T0_T1
	.type	op_imulw_T0_T1, @function
op_imulw_T0_T1:
.LFB537:
	movswl	%bx,%eax
	movswl	%r12w,%edx
	imull	%edx, %eax
	movswl	%ax,%edx
	movl	%eax, %ebx
	movl	%eax, 44(%rbp)
	cmpl	%edx, %eax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE537:
	.size	op_imulw_T0_T1, .-op_imulw_T0_T1
	.p2align 4,,15
.globl op_imull_T0_T1
	.type	op_imull_T0_T1, @function
op_imull_T0_T1:
.LFB538:
	movslq	%ebx,%rax
	movslq	%r12d,%rdx
	imulq	%rdx, %rax
	movslq	%eax,%rdx
	movl	%eax, %ebx
	movl	%eax, 44(%rbp)
	cmpq	%rdx, %rax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE538:
	.size	op_imull_T0_T1, .-op_imull_T0_T1
	.p2align 4,,15
.globl op_movl_T0_imu
	.type	op_movl_T0_imu, @function
op_movl_T0_imu:
.LFB545:
	leal	__op_param1(%rip), %ebx
	ret
.LFE545:
	.size	op_movl_T0_imu, .-op_movl_T0_imu
	.p2align 4,,15
.globl op_movl_T0_im
	.type	op_movl_T0_im, @function
op_movl_T0_im:
.LFB546:
	leal	__op_param1(%rip), %ebx
	ret
.LFE546:
	.size	op_movl_T0_im, .-op_movl_T0_im
	.p2align 4,,15
.globl op_addl_T0_im
	.type	op_addl_T0_im, @function
op_addl_T0_im:
.LFB547:
	movl	$__op_param1, %eax
	addl	%eax, %ebx
	ret
.LFE547:
	.size	op_addl_T0_im, .-op_addl_T0_im
	.p2align 4,,15
.globl op_andl_T0_ffff
	.type	op_andl_T0_ffff, @function
op_andl_T0_ffff:
.LFB548:
	andl	$65535, %ebx
	ret
.LFE548:
	.size	op_andl_T0_ffff, .-op_andl_T0_ffff
	.p2align 4,,15
.globl op_andl_T0_im
	.type	op_andl_T0_im, @function
op_andl_T0_im:
.LFB549:
	movl	$__op_param1, %eax
	andl	%eax, %ebx
	ret
.LFE549:
	.size	op_andl_T0_im, .-op_andl_T0_im
	.p2align 4,,15
.globl op_movl_T0_T1
	.type	op_movl_T0_T1, @function
op_movl_T0_T1:
.LFB550:
	movl	%r12d, %ebx
	ret
.LFE550:
	.size	op_movl_T0_T1, .-op_movl_T0_T1
	.p2align 4,,15
.globl op_movl_T1_imu
	.type	op_movl_T1_imu, @function
op_movl_T1_imu:
.LFB551:
	leal	__op_param1(%rip), %r12d
	ret
.LFE551:
	.size	op_movl_T1_imu, .-op_movl_T1_imu
	.p2align 4,,15
.globl op_movl_T1_im
	.type	op_movl_T1_im, @function
op_movl_T1_im:
.LFB552:
	leal	__op_param1(%rip), %r12d
	ret
.LFE552:
	.size	op_movl_T1_im, .-op_movl_T1_im
	.p2align 4,,15
.globl op_addl_T1_im
	.type	op_addl_T1_im, @function
op_addl_T1_im:
.LFB553:
	movl	$__op_param1, %eax
	addl	%eax, %r12d
	ret
.LFE553:
	.size	op_addl_T1_im, .-op_addl_T1_im
	.p2align 4,,15
.globl op_movl_T1_A0
	.type	op_movl_T1_A0, @function
op_movl_T1_A0:
.LFB554:
	movl	%r13d, %r12d
	ret
.LFE554:
	.size	op_movl_T1_A0, .-op_movl_T1_A0
	.p2align 4,,15
.globl op_movl_A0_im
	.type	op_movl_A0_im, @function
op_movl_A0_im:
.LFB555:
	leal	__op_param1(%rip), %r13d
	ret
.LFE555:
	.size	op_movl_A0_im, .-op_movl_A0_im
	.p2align 4,,15
.globl op_addl_A0_im
	.type	op_addl_A0_im, @function
op_addl_A0_im:
.LFB556:
	movl	$__op_param1, %eax
	addl	%eax, %r13d
	ret
.LFE556:
	.size	op_addl_A0_im, .-op_addl_A0_im
	.p2align 4,,15
.globl op_addl_A0_AL
	.type	op_addl_A0_AL, @function
op_addl_A0_AL:
.LFB559:
	movzbl	(%rbp),%eax
	addl	%eax, %r13d
	ret
.LFE559:
	.size	op_addl_A0_AL, .-op_addl_A0_AL
	.p2align 4,,15
.globl op_andl_A0_ffff
	.type	op_andl_A0_ffff, @function
op_andl_A0_ffff:
.LFB560:
	andl	$65535, %r13d
	ret
.LFE560:
	.size	op_andl_A0_ffff, .-op_andl_A0_ffff
	.p2align 4,,15
.globl op_jmp_T0
	.type	op_jmp_T0, @function
op_jmp_T0:
.LFB618:
	movl	%ebx, 32(%rbp)
	ret
.LFE618:
	.size	op_jmp_T0, .-op_jmp_T0
	.p2align 4,,15
.globl op_movl_eip_im
	.type	op_movl_eip_im, @function
op_movl_eip_im:
.LFB619:
	movl	$__op_param1, %eax
	movl	%eax, 32(%rbp)
	ret
.LFE619:
	.size	op_movl_eip_im, .-op_movl_eip_im
	.p2align 4,,15
.globl op_cli
	.type	op_cli, @function
op_cli:
.LFB627:
	andl	$-513, 36(%rbp)
	ret
.LFE627:
	.size	op_cli, .-op_cli
	.p2align 4,,15
.globl op_sti
	.type	op_sti, @function
op_sti:
.LFB628:
	orl	$512, 36(%rbp)
	ret
.LFE628:
	.size	op_sti, .-op_sti
	.p2align 4,,15
.globl op_set_inhibit_irq
	.type	op_set_inhibit_irq, @function
op_set_inhibit_irq:
.LFB629:
	orl	$8, 56(%rbp)
	ret
.LFE629:
	.size	op_set_inhibit_irq, .-op_set_inhibit_irq
	.p2align 4,,15
.globl op_reset_inhibit_irq
	.type	op_reset_inhibit_irq, @function
op_reset_inhibit_irq:
.LFB630:
	andl	$-9, 56(%rbp)
	ret
.LFE630:
	.size	op_reset_inhibit_irq, .-op_reset_inhibit_irq
	.p2align 4,,15
.globl op_movl_T0_0
	.type	op_movl_T0_0, @function
op_movl_T0_0:
.LFB635:
	xorl	%ebx, %ebx
	ret
.LFE635:
	.size	op_movl_T0_0, .-op_movl_T0_0
	.p2align 4,,15
.globl op_exit_tb
	.type	op_exit_tb, @function
op_exit_tb:
.LFB637:
#APP
	ret
#NO_APP
	ret
.LFE637:
	.size	op_exit_tb, .-op_exit_tb
	.p2align 4,,15
	.type	compute_c_addb, @function
compute_c_addb:
.LFB639:
	movzbl	44(%rbp), %eax
	cmpb	40(%rbp), %al
	setb	%al
	movzbl	%al, %eax
	ret
.LFE639:
	.size	compute_c_addb, .-compute_c_addb
	.p2align 4,,15
	.type	compute_c_adcb, @function
compute_c_adcb:
.LFB641:
	movzbl	44(%rbp), %eax
	cmpb	40(%rbp), %al
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE641:
	.size	compute_c_adcb, .-compute_c_adcb
	.p2align 4,,15
	.type	compute_c_subb, @function
compute_c_subb:
.LFB643:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	setb	%al
	movzbl	%al, %eax
	ret
.LFE643:
	.size	compute_c_subb, .-compute_c_subb
	.p2align 4,,15
	.type	compute_c_sbbb, @function
compute_c_sbbb:
.LFB645:
	movl	44(%rbp), %eax
	movl	40(%rbp), %edx
	incl	%eax
	leal	(%rdx,%rax), %eax
	cmpb	%dl, %al
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE645:
	.size	compute_c_sbbb, .-compute_c_sbbb
	.p2align 4,,15
	.type	compute_c_logicb, @function
compute_c_logicb:
.LFB647:
	xorl	%eax, %eax
	ret
.LFE647:
	.size	compute_c_logicb, .-compute_c_logicb
	.p2align 4,,15
	.type	compute_c_shlb, @function
compute_c_shlb:
.LFB651:
	movl	40(%rbp), %eax
	shrl	$7, %eax
	andl	$1, %eax
	ret
.LFE651:
	.size	compute_c_shlb, .-compute_c_shlb
	.p2align 4,,15
.globl op_jb_subb
	.type	op_jb_subb, @function
op_jb_subb:
.LFB654:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	jae	.L475
#APP
	jmp __op_gen_label1
#NO_APP
.L475:
	ret
.LFE654:
	.size	op_jb_subb, .-op_jb_subb
	.p2align 4,,15
.globl op_jz_subb
	.type	op_jz_subb, @function
op_jz_subb:
.LFB655:
	cmpb	$0, 44(%rbp)
	jne	.L479
#APP
	jmp __op_gen_label1
#NO_APP
.L479:
	ret
.LFE655:
	.size	op_jz_subb, .-op_jz_subb
	.p2align 4,,15
.globl op_jnz_subb
	.type	op_jnz_subb, @function
op_jnz_subb:
.LFB656:
	cmpb	$0, 44(%rbp)
	je	.L483
#APP
	jmp __op_gen_label1
#NO_APP
.L483:
	ret
.LFE656:
	.size	op_jnz_subb, .-op_jnz_subb
	.p2align 4,,15
.globl op_jbe_subb
	.type	op_jbe_subb, @function
op_jbe_subb:
.LFB657:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	ja	.L487
#APP
	jmp __op_gen_label1
#NO_APP
.L487:
	ret
.LFE657:
	.size	op_jbe_subb, .-op_jbe_subb
	.p2align 4,,15
.globl op_js_subb
	.type	op_js_subb, @function
op_js_subb:
.LFB658:
	cmpb	$0, 44(%rbp)
	jns	.L491
#APP
	jmp __op_gen_label1
#NO_APP
.L491:
	ret
.LFE658:
	.size	op_js_subb, .-op_js_subb
	.p2align 4,,15
.globl op_jl_subb
	.type	op_jl_subb, @function
op_jl_subb:
.LFB659:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	jge	.L495
#APP
	jmp __op_gen_label1
#NO_APP
.L495:
	ret
.LFE659:
	.size	op_jl_subb, .-op_jl_subb
	.p2align 4,,15
.globl op_jle_subb
	.type	op_jle_subb, @function
op_jle_subb:
.LFB660:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	jg	.L499
#APP
	jmp __op_gen_label1
#NO_APP
.L499:
	ret
.LFE660:
	.size	op_jle_subb, .-op_jle_subb
	.p2align 4,,15
.globl op_setb_T0_subb
	.type	op_setb_T0_subb, @function
op_setb_T0_subb:
.LFB661:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	setb	%bl
	ret
.LFE661:
	.size	op_setb_T0_subb, .-op_setb_T0_subb
	.p2align 4,,15
.globl op_setz_T0_subb
	.type	op_setz_T0_subb, @function
op_setz_T0_subb:
.LFB662:
	xorl	%ebx, %ebx
	cmpb	$0, 44(%rbp)
	sete	%bl
	ret
.LFE662:
	.size	op_setz_T0_subb, .-op_setz_T0_subb
	.p2align 4,,15
.globl op_setbe_T0_subb
	.type	op_setbe_T0_subb, @function
op_setbe_T0_subb:
.LFB663:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	setbe	%bl
	ret
.LFE663:
	.size	op_setbe_T0_subb, .-op_setbe_T0_subb
	.p2align 4,,15
.globl op_setl_T0_subb
	.type	op_setl_T0_subb, @function
op_setl_T0_subb:
.LFB665:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	setl	%bl
	ret
.LFE665:
	.size	op_setl_T0_subb, .-op_setl_T0_subb
	.p2align 4,,15
.globl op_setle_T0_subb
	.type	op_setle_T0_subb, @function
op_setle_T0_subb:
.LFB666:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpb	%dl, %al
	setle	%bl
	ret
.LFE666:
	.size	op_setle_T0_subb, .-op_setle_T0_subb
	.p2align 4,,15
.globl op_shlb_T0_T1
	.type	op_shlb_T0_T1, @function
op_shlb_T0_T1:
.LFB667:
	movl	%r12d, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
.LFE667:
	.size	op_shlb_T0_T1, .-op_shlb_T0_T1
	.p2align 4,,15
.globl op_shrb_T0_T1
	.type	op_shrb_T0_T1, @function
op_shrb_T0_T1:
.LFB668:
	movzbl	%bl,%eax
	movl	%r12d, %ecx
	andl	$31, %ecx
	movl	%eax, %ebx
	shrl	%cl, %ebx
	ret
.LFE668:
	.size	op_shrb_T0_T1, .-op_shrb_T0_T1
	.p2align 4,,15
.globl op_sarb_T0_T1
	.type	op_sarb_T0_T1, @function
op_sarb_T0_T1:
.LFB669:
	movsbl	%bl,%eax
	movl	%r12d, %ecx
	andl	$31, %ecx
	movl	%eax, %ebx
	sarl	%cl, %ebx
	ret
.LFE669:
	.size	op_sarb_T0_T1, .-op_sarb_T0_T1
	.p2align 4,,15
.globl op_rolb_T0_T1
	.type	op_rolb_T0_T1, @function
op_rolb_T0_T1:
.LFB672:
	movl	%r12d, %esi
	andl	$7, %esi
	je	.L519
	movzbl	%bl,%eax
	movl	%esi, %ecx
	movl	%eax, %edx
	movl	%eax, %ebx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%esi, %ecx
	movl	%edx, %ebx
	shrl	%cl, %eax
	orl	%eax, %ebx
.L519:
	ret
.LFE672:
	.size	op_rolb_T0_T1, .-op_rolb_T0_T1
	.p2align 4,,15
.globl op_rorb_T0_T1
	.type	op_rorb_T0_T1, @function
op_rorb_T0_T1:
.LFB673:
	movl	%r12d, %esi
	andl	$7, %esi
	je	.L523
	movzbl	%bl,%eax
	movl	%esi, %ecx
	movl	%eax, %edx
	movl	%eax, %ebx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%esi, %ecx
	movl	%edx, %ebx
	sall	%cl, %eax
	orl	%eax, %ebx
.L523:
	ret
.LFE673:
	.size	op_rorb_T0_T1, .-op_rorb_T0_T1
	.p2align 4,,15
.globl op_shlb_T0_T1_cc
	.type	op_shlb_T0_T1_cc, @function
op_shlb_T0_T1_cc:
.LFB676:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L527
	movl	%ebx, %eax
	sall	%cl, %ebx
	decl	%ecx
	movzbl	%al, %eax
	movl	%ebx, 44(%rbp)
	movl	$34, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L527:
	ret
.LFE676:
	.size	op_shlb_T0_T1_cc, .-op_shlb_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_T0_T1_cc
	.type	op_shrb_T0_T1_cc, @function
op_shrb_T0_T1_cc:
.LFB677:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L531
	movzbl	%bl,%eax
	movl	$38, 48(%rbp)
	movl	%eax, %ebx
	shrl	%cl, %ebx
	decl	%ecx
	shrl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 40(%rbp)
.L531:
	ret
.LFE677:
	.size	op_shrb_T0_T1_cc, .-op_shrb_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_T0_T1_cc
	.type	op_sarb_T0_T1_cc, @function
op_sarb_T0_T1_cc:
.LFB678:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L535
	movsbl	%bl,%eax
	movl	$38, 48(%rbp)
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 40(%rbp)
.L535:
	ret
.LFE678:
	.size	op_sarb_T0_T1_cc, .-op_sarb_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_T0_T1_cc
	.type	op_adcb_T0_T1_cc, @function
op_adcb_T0_T1_cc:
.LFB679:
	subq	$8, %rsp
.LCFI1:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%r12d, %ecx
	movl	%r12d, 40(%rbp)
	leal	(%rbx,%rcx), %edx
	leal	(%rdx,%rax), %ebx
	leal	6(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE679:
	.size	op_adcb_T0_T1_cc, .-op_adcb_T0_T1_cc
	.p2align 4,,15
.globl op_sbbb_T0_T1_cc
	.type	op_sbbb_T0_T1_cc, @function
op_sbbb_T0_T1_cc:
.LFB680:
	subq	$8, %rsp
.LCFI2:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %edx
	movl	%r12d, 40(%rbp)
	subl	%r12d, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	14(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE680:
	.size	op_sbbb_T0_T1_cc, .-op_sbbb_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgb_T0_T1_EAX_cc
	.type	op_cmpxchgb_T0_T1_EAX_cc, @function
op_cmpxchgb_T0_T1_EAX_cc:
.LFB681:
	movl	(%rbp), %edx
	movl	%ebx, %ecx
	movl	%edx, %edi
	subl	%ebx, %edi
	testb	%dil, %dil
	jne	.L543
	movl	%r12d, %ebx
	jmp	.L545
	.p2align 4,,7
.L543:
	xorb	%dl, %dl
	movzbl	%bl,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L545:
	movl	%ecx, 40(%rbp)
	movl	%edi, 44(%rbp)
	ret
.LFE681:
	.size	op_cmpxchgb_T0_T1_EAX_cc, .-op_cmpxchgb_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftb
	.type	op_movl_T0_Dshiftb, @function
op_movl_T0_Dshiftb:
.LFB718:
	movl	52(%rbp), %ebx
	ret
.LFE718:
	.size	op_movl_T0_Dshiftb, .-op_movl_T0_Dshiftb
	.p2align 4,,15
	.type	compute_c_addw, @function
compute_c_addw:
.LFB726:
	movl	44(%rbp), %eax
	cmpw	40(%rbp), %ax
	setb	%al
	movzbl	%al, %eax
	ret
.LFE726:
	.size	compute_c_addw, .-compute_c_addw
	.p2align 4,,15
	.type	compute_c_adcw, @function
compute_c_adcw:
.LFB728:
	movl	44(%rbp), %eax
	cmpw	40(%rbp), %ax
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE728:
	.size	compute_c_adcw, .-compute_c_adcw
	.p2align 4,,15
	.type	compute_c_subw, @function
compute_c_subw:
.LFB730:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	setb	%al
	movzbl	%al, %eax
	ret
.LFE730:
	.size	compute_c_subw, .-compute_c_subw
	.p2align 4,,15
	.type	compute_c_sbbw, @function
compute_c_sbbw:
.LFB732:
	movl	44(%rbp), %eax
	movl	40(%rbp), %edx
	incl	%eax
	leal	(%rdx,%rax), %eax
	cmpw	%dx, %ax
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE732:
	.size	compute_c_sbbw, .-compute_c_sbbw
	.p2align 4,,15
	.type	compute_c_logicw, @function
compute_c_logicw:
.LFB734:
	xorl	%eax, %eax
	ret
.LFE734:
	.size	compute_c_logicw, .-compute_c_logicw
	.p2align 4,,15
	.type	compute_c_shlw, @function
compute_c_shlw:
.LFB738:
	movl	40(%rbp), %eax
	shrl	$15, %eax
	andl	$1, %eax
	ret
.LFE738:
	.size	compute_c_shlw, .-compute_c_shlw
	.p2align 4,,15
.globl op_jb_subw
	.type	op_jb_subw, @function
op_jb_subw:
.LFB741:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	jae	.L562
#APP
	jmp __op_gen_label1
#NO_APP
.L562:
	ret
.LFE741:
	.size	op_jb_subw, .-op_jb_subw
	.p2align 4,,15
.globl op_jz_subw
	.type	op_jz_subw, @function
op_jz_subw:
.LFB742:
	cmpw	$0, 44(%rbp)
	jne	.L566
#APP
	jmp __op_gen_label1
#NO_APP
.L566:
	ret
.LFE742:
	.size	op_jz_subw, .-op_jz_subw
	.p2align 4,,15
.globl op_jnz_subw
	.type	op_jnz_subw, @function
op_jnz_subw:
.LFB743:
	cmpw	$0, 44(%rbp)
	je	.L570
#APP
	jmp __op_gen_label1
#NO_APP
.L570:
	ret
.LFE743:
	.size	op_jnz_subw, .-op_jnz_subw
	.p2align 4,,15
.globl op_jbe_subw
	.type	op_jbe_subw, @function
op_jbe_subw:
.LFB744:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	ja	.L574
#APP
	jmp __op_gen_label1
#NO_APP
.L574:
	ret
.LFE744:
	.size	op_jbe_subw, .-op_jbe_subw
	.p2align 4,,15
.globl op_js_subw
	.type	op_js_subw, @function
op_js_subw:
.LFB745:
	cmpw	$0, 44(%rbp)
	jns	.L578
#APP
	jmp __op_gen_label1
#NO_APP
.L578:
	ret
.LFE745:
	.size	op_js_subw, .-op_js_subw
	.p2align 4,,15
.globl op_jl_subw
	.type	op_jl_subw, @function
op_jl_subw:
.LFB746:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	jge	.L582
#APP
	jmp __op_gen_label1
#NO_APP
.L582:
	ret
.LFE746:
	.size	op_jl_subw, .-op_jl_subw
	.p2align 4,,15
.globl op_jle_subw
	.type	op_jle_subw, @function
op_jle_subw:
.LFB747:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	jg	.L586
#APP
	jmp __op_gen_label1
#NO_APP
.L586:
	ret
.LFE747:
	.size	op_jle_subw, .-op_jle_subw
	.p2align 4,,15
.globl op_loopnzw
	.type	op_loopnzw, @function
op_loopnzw:
.LFB748:
	cmpw	$0, 4(%rbp)
	je	.L590
	testb	$64, %bl
	jne	.L590
#APP
	jmp __op_gen_label1
#NO_APP
.L590:
	ret
.LFE748:
	.size	op_loopnzw, .-op_loopnzw
	.p2align 4,,15
.globl op_loopzw
	.type	op_loopzw, @function
op_loopzw:
.LFB749:
	cmpw	$0, 4(%rbp)
	je	.L595
	testb	$64, %bl
	je	.L595
#APP
	jmp __op_gen_label1
#NO_APP
.L595:
	ret
.LFE749:
	.size	op_loopzw, .-op_loopzw
	.p2align 4,,15
.globl op_jz_ecxw
	.type	op_jz_ecxw, @function
op_jz_ecxw:
.LFB750:
	cmpw	$0, 4(%rbp)
	jne	.L600
#APP
	jmp __op_gen_label1
#NO_APP
.L600:
	ret
.LFE750:
	.size	op_jz_ecxw, .-op_jz_ecxw
	.p2align 4,,15
.globl op_jnz_ecxw
	.type	op_jnz_ecxw, @function
op_jnz_ecxw:
.LFB751:
	cmpw	$0, 4(%rbp)
	je	.L604
#APP
	jmp __op_gen_label1
#NO_APP
.L604:
	ret
.LFE751:
	.size	op_jnz_ecxw, .-op_jnz_ecxw
	.p2align 4,,15
.globl op_setb_T0_subw
	.type	op_setb_T0_subw, @function
op_setb_T0_subw:
.LFB752:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	setb	%bl
	ret
.LFE752:
	.size	op_setb_T0_subw, .-op_setb_T0_subw
	.p2align 4,,15
.globl op_setz_T0_subw
	.type	op_setz_T0_subw, @function
op_setz_T0_subw:
.LFB753:
	xorl	%ebx, %ebx
	cmpw	$0, 44(%rbp)
	sete	%bl
	ret
.LFE753:
	.size	op_setz_T0_subw, .-op_setz_T0_subw
	.p2align 4,,15
.globl op_setbe_T0_subw
	.type	op_setbe_T0_subw, @function
op_setbe_T0_subw:
.LFB754:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	setbe	%bl
	ret
.LFE754:
	.size	op_setbe_T0_subw, .-op_setbe_T0_subw
	.p2align 4,,15
.globl op_setl_T0_subw
	.type	op_setl_T0_subw, @function
op_setl_T0_subw:
.LFB756:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	setl	%bl
	ret
.LFE756:
	.size	op_setl_T0_subw, .-op_setl_T0_subw
	.p2align 4,,15
.globl op_setle_T0_subw
	.type	op_setle_T0_subw, @function
op_setle_T0_subw:
.LFB757:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpw	%dx, %ax
	setle	%bl
	ret
.LFE757:
	.size	op_setle_T0_subw, .-op_setle_T0_subw
	.p2align 4,,15
.globl op_shlw_T0_T1
	.type	op_shlw_T0_T1, @function
op_shlw_T0_T1:
.LFB758:
	movl	%r12d, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
.LFE758:
	.size	op_shlw_T0_T1, .-op_shlw_T0_T1
	.p2align 4,,15
.globl op_shrw_T0_T1
	.type	op_shrw_T0_T1, @function
op_shrw_T0_T1:
.LFB759:
	movzwl	%bx,%eax
	movl	%r12d, %ecx
	andl	$31, %ecx
	movl	%eax, %ebx
	shrl	%cl, %ebx
	ret
.LFE759:
	.size	op_shrw_T0_T1, .-op_shrw_T0_T1
	.p2align 4,,15
.globl op_sarw_T0_T1
	.type	op_sarw_T0_T1, @function
op_sarw_T0_T1:
.LFB760:
	movswl	%bx,%eax
	movl	%r12d, %ecx
	andl	$31, %ecx
	movl	%eax, %ebx
	sarl	%cl, %ebx
	ret
.LFE760:
	.size	op_sarw_T0_T1, .-op_sarw_T0_T1
	.p2align 4,,15
.globl op_rolw_T0_T1
	.type	op_rolw_T0_T1, @function
op_rolw_T0_T1:
.LFB763:
	movl	%r12d, %esi
	andl	$15, %esi
	je	.L624
	movzwl	%bx,%eax
	movl	%esi, %ecx
	movl	%eax, %edx
	movl	%eax, %ebx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%esi, %ecx
	movl	%edx, %ebx
	shrl	%cl, %eax
	orl	%eax, %ebx
.L624:
	ret
.LFE763:
	.size	op_rolw_T0_T1, .-op_rolw_T0_T1
	.p2align 4,,15
.globl op_rorw_T0_T1
	.type	op_rorw_T0_T1, @function
op_rorw_T0_T1:
.LFB764:
	movl	%r12d, %esi
	andl	$15, %esi
	je	.L628
	movzwl	%bx,%eax
	movl	%esi, %ecx
	movl	%eax, %edx
	movl	%eax, %ebx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%esi, %ecx
	movl	%edx, %ebx
	sall	%cl, %eax
	orl	%eax, %ebx
.L628:
	ret
.LFE764:
	.size	op_rorw_T0_T1, .-op_rorw_T0_T1
	.p2align 4,,15
.globl op_shlw_T0_T1_cc
	.type	op_shlw_T0_T1_cc, @function
op_shlw_T0_T1_cc:
.LFB767:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L632
	movl	%ebx, %eax
	sall	%cl, %ebx
	decl	%ecx
	movzwl	%ax, %eax
	movl	%ebx, 44(%rbp)
	movl	$35, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L632:
	ret
.LFE767:
	.size	op_shlw_T0_T1_cc, .-op_shlw_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_T0_T1_cc
	.type	op_shrw_T0_T1_cc, @function
op_shrw_T0_T1_cc:
.LFB768:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L636
	movzwl	%bx,%eax
	movl	$39, 48(%rbp)
	movl	%eax, %ebx
	shrl	%cl, %ebx
	decl	%ecx
	shrl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 40(%rbp)
.L636:
	ret
.LFE768:
	.size	op_shrw_T0_T1_cc, .-op_shrw_T0_T1_cc
	.p2align 4,,15
.globl op_sarw_T0_T1_cc
	.type	op_sarw_T0_T1_cc, @function
op_sarw_T0_T1_cc:
.LFB769:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L640
	movswl	%bx,%eax
	movl	$39, 48(%rbp)
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 40(%rbp)
.L640:
	ret
.LFE769:
	.size	op_sarw_T0_T1_cc, .-op_sarw_T0_T1_cc
	.p2align 4,,15
.globl op_shldw_T0_T1_im_cc
	.type	op_shldw_T0_T1_im_cc, @function
op_shldw_T0_T1_im_cc:
.LFB770:
	movl	%ebx, %esi
	movzwl	%r12w,%eax
	leal	__op_param1(%rip), %edi
	sall	$16, %esi
	movl	%eax, %r12d
	orl	%eax, %esi
	movl	%edi, %ecx
	movl	%esi, %edx
	sall	%cl, %edx
	cmpl	$16, %edi
	jle	.L644
	leal	-16(%rdi), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L644:
	movl	$32, %ecx
	movl	%edx, %ebx
	subl	%edi, %ecx
	shrl	$16, %ebx
	shrl	%cl, %esi
	movl	%ebx, 44(%rbp)
	movl	%esi, 40(%rbp)
	ret
.LFE770:
	.size	op_shldw_T0_T1_im_cc, .-op_shldw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_T0_T1_ECX_cc
	.type	op_shldw_T0_T1_ECX_cc, @function
op_shldw_T0_T1_ECX_cc:
.LFB771:
	movl	4(%rbp), %edi
	movq	%rbp, %r8
	andl	$31, %edi
	je	.L648
	movl	%ebx, %eax
	movzwl	%r12w,%edx
	movl	%edi, %ecx
	sall	$16, %eax
	movl	%edx, %r12d
	orl	%edx, %eax
	movl	%eax, %esi
	sall	%cl, %esi
	cmpl	$16, %edi
	jle	.L650
	leal	-16(%rdi), %ecx
	sall	%cl, %edx
	orl	%edx, %esi
.L650:
	movl	$32, %ecx
	movl	%esi, %ebx
	movl	$39, 48(%rbp)
	subl	%edi, %ecx
	shrl	$16, %ebx
	shrl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 40(%r8)
.L648:
	ret
.LFE771:
	.size	op_shldw_T0_T1_ECX_cc, .-op_shldw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_T0_T1_im_cc
	.type	op_shrdw_T0_T1_im_cc, @function
op_shrdw_T0_T1_im_cc:
.LFB772:
	movl	%r12d, %eax
	leal	__op_param1(%rip), %edi
	movzwl	%bx,%edx
	sall	$16, %eax
	movl	%r12d, %esi
	orl	%eax, %edx
	movl	%edi, %ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	cmpl	$16, %edi
	jle	.L654
	movl	$32, %ecx
	subl	%edi, %ecx
	sall	%cl, %esi
	orl	%esi, %eax
.L654:
	leal	-1(%rdi), %ecx
	movl	%eax, %ebx
	movl	%eax, 44(%rbp)
	shrl	%cl, %edx
	movl	%edx, 40(%rbp)
	ret
.LFE772:
	.size	op_shrdw_T0_T1_im_cc, .-op_shrdw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdw_T0_T1_ECX_cc
	.type	op_shrdw_T0_T1_ECX_cc, @function
op_shrdw_T0_T1_ECX_cc:
.LFB773:
	movl	4(%rbp), %esi
	movq	%rbp, %rdi
	andl	$31, %esi
	je	.L658
	movl	%r12d, %eax
	movzwl	%bx,%edx
	movl	%esi, %ecx
	sall	$16, %eax
	movl	%r12d, %r8d
	orl	%eax, %edx
	movl	%edx, %eax
	shrl	%cl, %eax
	cmpl	$16, %esi
	jle	.L660
	movl	$32, %ecx
	subl	%esi, %ecx
	sall	%cl, %r8d
	orl	%r8d, %eax
.L660:
	leal	-1(%rsi), %ecx
	movl	%eax, %ebx
	movl	%eax, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %edx
	movl	%edx, 40(%rdi)
.L658:
	ret
.LFE773:
	.size	op_shrdw_T0_T1_ECX_cc, .-op_shrdw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcw_T0_T1_cc
	.type	op_adcw_T0_T1_cc, @function
op_adcw_T0_T1_cc:
.LFB774:
	subq	$8, %rsp
.LCFI3:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%r12d, %ecx
	movl	%r12d, 40(%rbp)
	leal	(%rbx,%rcx), %edx
	leal	(%rdx,%rax), %ebx
	leal	7(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE774:
	.size	op_adcw_T0_T1_cc, .-op_adcw_T0_T1_cc
	.p2align 4,,15
.globl op_sbbw_T0_T1_cc
	.type	op_sbbw_T0_T1_cc, @function
op_sbbw_T0_T1_cc:
.LFB775:
	subq	$8, %rsp
.LCFI4:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %edx
	movl	%r12d, 40(%rbp)
	subl	%r12d, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	15(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE775:
	.size	op_sbbw_T0_T1_cc, .-op_sbbw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_T0_T1_EAX_cc
	.type	op_cmpxchgw_T0_T1_EAX_cc, @function
op_cmpxchgw_T0_T1_EAX_cc:
.LFB776:
	movl	(%rbp), %edx
	movl	%ebx, %ecx
	movl	%edx, %edi
	subl	%ebx, %edi
	testw	%di, %di
	jne	.L668
	movl	%r12d, %ebx
	jmp	.L670
	.p2align 4,,7
.L668:
	xorw	%dx, %dx
	movzwl	%bx,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L670:
	movl	%ecx, 40(%rbp)
	movl	%edi, 44(%rbp)
	ret
.LFE776:
	.size	op_cmpxchgw_T0_T1_EAX_cc, .-op_cmpxchgw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_btw_T0_T1_cc
	.type	op_btw_T0_T1_cc, @function
op_btw_T0_T1_cc:
.LFB825:
	movl	%r12d, %ecx
	movl	%ebx, %eax
	andl	$15, %ecx
	shrl	%cl, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE825:
	.size	op_btw_T0_T1_cc, .-op_btw_T0_T1_cc
	.p2align 4,,15
.globl op_btsw_T0_T1_cc
	.type	op_btsw_T0_T1_cc, @function
op_btsw_T0_T1_cc:
.LFB826:
	movl	%r12d, %ecx
	movl	$1, %eax
	movl	%ebx, %r12d
	andl	$15, %ecx
	sall	%cl, %eax
	shrl	%cl, %r12d
	orl	%eax, %ebx
	ret
.LFE826:
	.size	op_btsw_T0_T1_cc, .-op_btsw_T0_T1_cc
	.p2align 4,,15
.globl op_btrw_T0_T1_cc
	.type	op_btrw_T0_T1_cc, @function
op_btrw_T0_T1_cc:
.LFB827:
	movl	%r12d, %ecx
	movl	$-2, %eax
	movl	%ebx, %r12d
	andl	$15, %ecx
	roll	%cl, %eax
	shrl	%cl, %r12d
	andl	%eax, %ebx
	ret
.LFE827:
	.size	op_btrw_T0_T1_cc, .-op_btrw_T0_T1_cc
	.p2align 4,,15
.globl op_btcw_T0_T1_cc
	.type	op_btcw_T0_T1_cc, @function
op_btcw_T0_T1_cc:
.LFB828:
	movl	%r12d, %ecx
	movl	$1, %eax
	movl	%ebx, %r12d
	andl	$15, %ecx
	sall	%cl, %eax
	shrl	%cl, %r12d
	xorl	%eax, %ebx
	ret
.LFE828:
	.size	op_btcw_T0_T1_cc, .-op_btcw_T0_T1_cc
	.p2align 4,,15
.globl op_add_bitw_A0_T1
	.type	op_add_bitw_A0_T1, @function
op_add_bitw_A0_T1:
.LFB829:
	movl	%r12d, %eax
	sarw	$4, %ax
	cwtl
	leal	(%r13,%rax,2), %r13d
	ret
.LFE829:
	.size	op_add_bitw_A0_T1, .-op_add_bitw_A0_T1
	.p2align 4,,15
.globl op_bsfw_T0_cc
	.type	op_bsfw_T0_cc, @function
op_bsfw_T0_cc:
.LFB830:
	movl	%ebx, %eax
	andl	$65535, %eax
	je	.L683
	xorl	%edx, %edx
	testb	$1, %al
	jne	.L687
	.p2align 4,,7
.L688:
	sarl	%eax
	incl	%edx
	testb	$1, %al
	je	.L688
.L687:
	movl	%edx, %r12d
	movl	$1, 44(%rbp)
	jmp	.L690
	.p2align 4,,7
.L683:
	movl	$0, 44(%rbp)
.L690:
	ret
.LFE830:
	.size	op_bsfw_T0_cc, .-op_bsfw_T0_cc
	.p2align 4,,15
.globl op_bsrw_T0_cc
	.type	op_bsrw_T0_cc, @function
op_bsrw_T0_cc:
.LFB831:
	movl	%ebx, %eax
	andl	$65535, %eax
	je	.L695
	testw	%ax, %ax
	movl	$15, %edx
	js	.L699
	.p2align 4,,7
.L700:
	addl	%eax, %eax
	decl	%edx
	testw	%ax, %ax
	jns	.L700
.L699:
	movl	%edx, %r12d
	movl	$1, 44(%rbp)
	jmp	.L702
	.p2align 4,,7
.L695:
	movl	$0, 44(%rbp)
.L702:
	ret
.LFE831:
	.size	op_bsrw_T0_cc, .-op_bsrw_T0_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftw
	.type	op_movl_T0_Dshiftw, @function
op_movl_T0_Dshiftw:
.LFB832:
	movl	52(%rbp), %eax
	leal	(%rax,%rax), %ebx
	ret
.LFE832:
	.size	op_movl_T0_Dshiftw, .-op_movl_T0_Dshiftw
	.p2align 4,,15
	.type	compute_c_addl, @function
compute_c_addl:
.LFB840:
	movl	44(%rbp), %eax
	cmpl	40(%rbp), %eax
	setb	%al
	movzbl	%al, %eax
	ret
.LFE840:
	.size	compute_c_addl, .-compute_c_addl
	.p2align 4,,15
	.type	compute_c_adcl, @function
compute_c_adcl:
.LFB842:
	movl	44(%rbp), %eax
	cmpl	40(%rbp), %eax
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE842:
	.size	compute_c_adcl, .-compute_c_adcl
	.p2align 4,,15
	.type	compute_c_subl, @function
compute_c_subl:
.LFB844:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	setb	%al
	movzbl	%al, %eax
	ret
.LFE844:
	.size	compute_c_subl, .-compute_c_subl
	.p2align 4,,15
	.type	compute_c_sbbl, @function
compute_c_sbbl:
.LFB846:
	movl	44(%rbp), %eax
	movl	40(%rbp), %edx
	incl	%eax
	leal	(%rdx,%rax), %eax
	cmpl	%edx, %eax
	setbe	%al
	movzbl	%al, %eax
	ret
.LFE846:
	.size	compute_c_sbbl, .-compute_c_sbbl
	.p2align 4,,15
	.type	compute_c_logicl, @function
compute_c_logicl:
.LFB848:
	xorl	%eax, %eax
	ret
.LFE848:
	.size	compute_c_logicl, .-compute_c_logicl
	.p2align 4,,15
	.type	compute_c_incl, @function
compute_c_incl:
.LFB850:
	movl	40(%rbp), %eax
	ret
.LFE850:
	.size	compute_c_incl, .-compute_c_incl
	.p2align 4,,15
	.type	compute_c_shll, @function
compute_c_shll:
.LFB853:
	movl	40(%rbp), %eax
	shrl	$31, %eax
	ret
.LFE853:
	.size	compute_c_shll, .-compute_c_shll
	.p2align 4,,15
	.type	compute_c_sarl, @function
compute_c_sarl:
.LFB854:
	movl	40(%rbp), %eax
	andl	$1, %eax
	ret
.LFE854:
	.size	compute_c_sarl, .-compute_c_sarl
	.p2align 4,,15
	.type	compute_c_mull, @function
compute_c_mull:
.LFB856:
	xorl	%eax, %eax
	cmpl	$0, 40(%rbp)
	setne	%al
	ret
.LFE856:
	.size	compute_c_mull, .-compute_c_mull
	.p2align 4,,15
.globl op_jb_subl
	.type	op_jb_subl, @function
op_jb_subl:
.LFB858:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	jae	.L727
#APP
	jmp __op_gen_label1
#NO_APP
.L727:
	ret
.LFE858:
	.size	op_jb_subl, .-op_jb_subl
	.p2align 4,,15
.globl op_jz_subl
	.type	op_jz_subl, @function
op_jz_subl:
.LFB859:
	movl	44(%rbp), %eax
	testl	%eax, %eax
	jne	.L731
#APP
	jmp __op_gen_label1
#NO_APP
.L731:
	ret
.LFE859:
	.size	op_jz_subl, .-op_jz_subl
	.p2align 4,,15
.globl op_jnz_subl
	.type	op_jnz_subl, @function
op_jnz_subl:
.LFB860:
	movl	44(%rbp), %edx
	testl	%edx, %edx
	je	.L735
#APP
	jmp __op_gen_label1
#NO_APP
.L735:
	ret
.LFE860:
	.size	op_jnz_subl, .-op_jnz_subl
	.p2align 4,,15
.globl op_jbe_subl
	.type	op_jbe_subl, @function
op_jbe_subl:
.LFB861:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	ja	.L739
#APP
	jmp __op_gen_label1
#NO_APP
.L739:
	ret
.LFE861:
	.size	op_jbe_subl, .-op_jbe_subl
	.p2align 4,,15
.globl op_js_subl
	.type	op_js_subl, @function
op_js_subl:
.LFB862:
	movl	44(%rbp), %ecx
	testl	%ecx, %ecx
	jns	.L743
#APP
	jmp __op_gen_label1
#NO_APP
.L743:
	ret
.LFE862:
	.size	op_js_subl, .-op_js_subl
	.p2align 4,,15
.globl op_jl_subl
	.type	op_jl_subl, @function
op_jl_subl:
.LFB863:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	jge	.L747
#APP
	jmp __op_gen_label1
#NO_APP
.L747:
	ret
.LFE863:
	.size	op_jl_subl, .-op_jl_subl
	.p2align 4,,15
.globl op_jle_subl
	.type	op_jle_subl, @function
op_jle_subl:
.LFB864:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	jg	.L751
#APP
	jmp __op_gen_label1
#NO_APP
.L751:
	ret
.LFE864:
	.size	op_jle_subl, .-op_jle_subl
	.p2align 4,,15
.globl op_loopnzl
	.type	op_loopnzl, @function
op_loopnzl:
.LFB865:
	movl	4(%rbp), %esi
	testl	%esi, %esi
	je	.L755
	testb	$64, %bl
	jne	.L755
#APP
	jmp __op_gen_label1
#NO_APP
.L755:
	ret
.LFE865:
	.size	op_loopnzl, .-op_loopnzl
	.p2align 4,,15
.globl op_loopzl
	.type	op_loopzl, @function
op_loopzl:
.LFB866:
	movl	4(%rbp), %edi
	testl	%edi, %edi
	je	.L760
	testb	$64, %bl
	je	.L760
#APP
	jmp __op_gen_label1
#NO_APP
.L760:
	ret
.LFE866:
	.size	op_loopzl, .-op_loopzl
	.p2align 4,,15
.globl op_jz_ecxl
	.type	op_jz_ecxl, @function
op_jz_ecxl:
.LFB867:
	movl	4(%rbp), %r8d
	testl	%r8d, %r8d
	jne	.L765
#APP
	jmp __op_gen_label1
#NO_APP
.L765:
	ret
.LFE867:
	.size	op_jz_ecxl, .-op_jz_ecxl
	.p2align 4,,15
.globl op_jnz_ecxl
	.type	op_jnz_ecxl, @function
op_jnz_ecxl:
.LFB868:
	movl	4(%rbp), %r9d
	testl	%r9d, %r9d
	je	.L769
#APP
	jmp __op_gen_label1
#NO_APP
.L769:
	ret
.LFE868:
	.size	op_jnz_ecxl, .-op_jnz_ecxl
	.p2align 4,,15
.globl op_setb_T0_subl
	.type	op_setb_T0_subl, @function
op_setb_T0_subl:
.LFB869:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	setb	%bl
	ret
.LFE869:
	.size	op_setb_T0_subl, .-op_setb_T0_subl
	.p2align 4,,15
.globl op_setz_T0_subl
	.type	op_setz_T0_subl, @function
op_setz_T0_subl:
.LFB870:
	xorl	%ebx, %ebx
	cmpl	$0, 44(%rbp)
	sete	%bl
	ret
.LFE870:
	.size	op_setz_T0_subl, .-op_setz_T0_subl
	.p2align 4,,15
.globl op_setbe_T0_subl
	.type	op_setbe_T0_subl, @function
op_setbe_T0_subl:
.LFB871:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	setbe	%bl
	ret
.LFE871:
	.size	op_setbe_T0_subl, .-op_setbe_T0_subl
	.p2align 4,,15
.globl op_setl_T0_subl
	.type	op_setl_T0_subl, @function
op_setl_T0_subl:
.LFB873:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	setl	%bl
	ret
.LFE873:
	.size	op_setl_T0_subl, .-op_setl_T0_subl
	.p2align 4,,15
.globl op_setle_T0_subl
	.type	op_setle_T0_subl, @function
op_setle_T0_subl:
.LFB874:
	movl	40(%rbp), %edx
	movl	44(%rbp), %ecx
	xorl	%ebx, %ebx
	leal	(%rdx,%rcx), %eax
	cmpl	%edx, %eax
	setle	%bl
	ret
.LFE874:
	.size	op_setle_T0_subl, .-op_setle_T0_subl
	.p2align 4,,15
.globl op_shll_T0_T1
	.type	op_shll_T0_T1, @function
op_shll_T0_T1:
.LFB875:
	movl	%r12d, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
.LFE875:
	.size	op_shll_T0_T1, .-op_shll_T0_T1
	.p2align 4,,15
.globl op_shrl_T0_T1
	.type	op_shrl_T0_T1, @function
op_shrl_T0_T1:
.LFB876:
	movl	%r12d, %ecx
	andl	$31, %ecx
	shrl	%cl, %ebx
	ret
.LFE876:
	.size	op_shrl_T0_T1, .-op_shrl_T0_T1
	.p2align 4,,15
.globl op_sarl_T0_T1
	.type	op_sarl_T0_T1, @function
op_sarl_T0_T1:
.LFB877:
	movl	%r12d, %ecx
	andl	$31, %ecx
	sarl	%cl, %ebx
	ret
.LFE877:
	.size	op_sarl_T0_T1, .-op_sarl_T0_T1
	.p2align 4,,15
.globl op_roll_T0_T1
	.type	op_roll_T0_T1, @function
op_roll_T0_T1:
.LFB880:
	movl	%r12d, %ecx
	movl	%ebx, %eax
	andl	$31, %ecx
	roll	%cl, %eax
	testl	%ecx, %ecx
	cmovne	%eax, %ebx
	ret
.LFE880:
	.size	op_roll_T0_T1, .-op_roll_T0_T1
	.p2align 4,,15
.globl op_rorl_T0_T1
	.type	op_rorl_T0_T1, @function
op_rorl_T0_T1:
.LFB881:
	movl	%r12d, %ecx
	movl	%ebx, %eax
	andl	$31, %ecx
	rorl	%cl, %eax
	testl	%ecx, %ecx
	cmovne	%eax, %ebx
	ret
.LFE881:
	.size	op_rorl_T0_T1, .-op_rorl_T0_T1
	.p2align 4,,15
.globl op_shll_T0_T1_cc
	.type	op_shll_T0_T1_cc, @function
op_shll_T0_T1_cc:
.LFB884:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L797
	movl	%ebx, %eax
	sall	%cl, %ebx
	decl	%ecx
	sall	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	movl	%eax, 40(%rbp)
.L797:
	ret
.LFE884:
	.size	op_shll_T0_T1_cc, .-op_shll_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_T0_T1_cc
	.type	op_shrl_T0_T1_cc, @function
op_shrl_T0_T1_cc:
.LFB885:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L801
	movl	%ebx, %eax
	shrl	%cl, %ebx
	decl	%ecx
	shrl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	movl	%eax, 40(%rbp)
.L801:
	ret
.LFE885:
	.size	op_shrl_T0_T1_cc, .-op_shrl_T0_T1_cc
	.p2align 4,,15
.globl op_sarl_T0_T1_cc
	.type	op_sarl_T0_T1_cc, @function
op_sarl_T0_T1_cc:
.LFB886:
	movl	%r12d, %ecx
	andl	$31, %ecx
	je	.L805
	movl	%ebx, %eax
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	movl	%eax, 40(%rbp)
.L805:
	ret
.LFE886:
	.size	op_sarl_T0_T1_cc, .-op_sarl_T0_T1_cc
	.p2align 4,,15
.globl op_shldl_T0_T1_im_cc
	.type	op_shldl_T0_T1_im_cc, @function
op_shldl_T0_T1_im_cc:
.LFB887:
	leal	__op_param1(%rip), %edx
	movl	%ebx, %edi
	movl	%r12d, %eax
	movl	%ebx, %esi
	movl	%edx, %ecx
	sall	%cl, %edi
	movl	$32, %ecx
	subl	%edx, %ecx
	decl	%edx
	movl	%edi, %ebx
	shrl	%cl, %eax
	movl	%edx, %ecx
	orl	%eax, %ebx
	sall	%cl, %esi
	movl	%esi, 40(%rbp)
	movl	%ebx, 44(%rbp)
	ret
.LFE887:
	.size	op_shldl_T0_T1_im_cc, .-op_shldl_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_T0_T1_ECX_cc
	.type	op_shldl_T0_T1_ECX_cc, @function
op_shldl_T0_T1_ECX_cc:
.LFB888:
	movl	4(%rbp), %edi
	andl	$31, %edi
	je	.L811
	movl	%ebx, %esi
	movl	%edi, %ecx
	movl	%r12d, %eax
	sall	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %edx
	subl	%edi, %ecx
	movl	%esi, %ebx
	movl	$36, 48(%rbp)
	shrl	%cl, %eax
	leal	-1(%rdi), %ecx
	orl	%eax, %ebx
	sall	%cl, %edx
	movl	%ebx, 44(%rbp)
	movl	%edx, 40(%rbp)
.L811:
	ret
.LFE888:
	.size	op_shldl_T0_T1_ECX_cc, .-op_shldl_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_T0_T1_im_cc
	.type	op_shrdl_T0_T1_im_cc, @function
op_shrdl_T0_T1_im_cc:
.LFB889:
	leal	__op_param1(%rip), %edx
	movl	%ebx, %edi
	movl	%r12d, %eax
	movl	%ebx, %esi
	movl	%edx, %ecx
	shrl	%cl, %edi
	movl	$32, %ecx
	subl	%edx, %ecx
	decl	%edx
	movl	%edi, %ebx
	sall	%cl, %eax
	movl	%edx, %ecx
	orl	%eax, %ebx
	shrl	%cl, %esi
	movl	%esi, 40(%rbp)
	movl	%ebx, 44(%rbp)
	ret
.LFE889:
	.size	op_shrdl_T0_T1_im_cc, .-op_shrdl_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdl_T0_T1_ECX_cc
	.type	op_shrdl_T0_T1_ECX_cc, @function
op_shrdl_T0_T1_ECX_cc:
.LFB890:
	movl	4(%rbp), %edi
	andl	$31, %edi
	je	.L817
	movl	%ebx, %esi
	movl	%edi, %ecx
	movl	%r12d, %eax
	shrl	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %edx
	subl	%edi, %ecx
	movl	%esi, %ebx
	movl	$40, 48(%rbp)
	sall	%cl, %eax
	leal	-1(%rdi), %ecx
	orl	%eax, %ebx
	shrl	%cl, %edx
	movl	%ebx, 44(%rbp)
	movl	%edx, 40(%rbp)
.L817:
	ret
.LFE890:
	.size	op_shrdl_T0_T1_ECX_cc, .-op_shrdl_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcl_T0_T1_cc
	.type	op_adcl_T0_T1_cc, @function
op_adcl_T0_T1_cc:
.LFB891:
	subq	$8, %rsp
.LCFI5:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%r12d, %ecx
	movl	%r12d, 40(%rbp)
	leal	(%rbx,%rcx), %edx
	leal	(%rdx,%rax), %ebx
	leal	8(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE891:
	.size	op_adcl_T0_T1_cc, .-op_adcl_T0_T1_cc
	.p2align 4,,15
.globl op_sbbl_T0_T1_cc
	.type	op_sbbl_T0_T1_cc, @function
op_sbbl_T0_T1_cc:
.LFB892:
	subq	$8, %rsp
.LCFI6:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %edx
	movl	%r12d, 40(%rbp)
	subl	%r12d, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	16(,%rax,4), %eax
	movl	%ebx, 44(%rbp)
	movl	%eax, 48(%rbp)
	addq	$8, %rsp
	ret
.LFE892:
	.size	op_sbbl_T0_T1_cc, .-op_sbbl_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgl_T0_T1_EAX_cc
	.type	op_cmpxchgl_T0_T1_EAX_cc, @function
op_cmpxchgl_T0_T1_EAX_cc:
.LFB893:
	movl	(%rbp), %eax
	movl	%ebx, %edx
	movq	%rbp, %rcx
	subl	%ebx, %eax
	jne	.L825
	movl	%r12d, %ebx
	jmp	.L827
	.p2align 4,,7
.L825:
	movl	%ebx, (%rcx)
.L827:
	movl	%edx, 40(%rbp)
	movl	%eax, 44(%rbp)
	ret
.LFE893:
	.size	op_cmpxchgl_T0_T1_EAX_cc, .-op_cmpxchgl_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_btl_T0_T1_cc
	.type	op_btl_T0_T1_cc, @function
op_btl_T0_T1_cc:
.LFB942:
	movl	%r12d, %ecx
	movl	%ebx, %eax
	andl	$31, %ecx
	shrl	%cl, %eax
	movl	%eax, 40(%rbp)
	ret
.LFE942:
	.size	op_btl_T0_T1_cc, .-op_btl_T0_T1_cc
	.p2align 4,,15
.globl op_btsl_T0_T1_cc
	.type	op_btsl_T0_T1_cc, @function
op_btsl_T0_T1_cc:
.LFB943:
	movl	%r12d, %ecx
	movl	$1, %eax
	movl	%ebx, %r12d
	andl	$31, %ecx
	sall	%cl, %eax
	shrl	%cl, %r12d
	orl	%eax, %ebx
	ret
.LFE943:
	.size	op_btsl_T0_T1_cc, .-op_btsl_T0_T1_cc
	.p2align 4,,15
.globl op_btrl_T0_T1_cc
	.type	op_btrl_T0_T1_cc, @function
op_btrl_T0_T1_cc:
.LFB944:
	movl	%r12d, %ecx
	movl	$-2, %eax
	movl	%ebx, %r12d
	andl	$31, %ecx
	roll	%cl, %eax
	shrl	%cl, %r12d
	andl	%eax, %ebx
	ret
.LFE944:
	.size	op_btrl_T0_T1_cc, .-op_btrl_T0_T1_cc
	.p2align 4,,15
.globl op_btcl_T0_T1_cc
	.type	op_btcl_T0_T1_cc, @function
op_btcl_T0_T1_cc:
.LFB945:
	movl	%r12d, %ecx
	movl	$1, %eax
	movl	%ebx, %r12d
	andl	$31, %ecx
	sall	%cl, %eax
	shrl	%cl, %r12d
	xorl	%eax, %ebx
	ret
.LFE945:
	.size	op_btcl_T0_T1_cc, .-op_btcl_T0_T1_cc
	.p2align 4,,15
.globl op_add_bitl_A0_T1
	.type	op_add_bitl_A0_T1, @function
op_add_bitl_A0_T1:
.LFB946:
	movl	%r12d, %eax
	sarl	$5, %eax
	leal	(%r13,%rax,4), %r13d
	ret
.LFE946:
	.size	op_add_bitl_A0_T1, .-op_add_bitl_A0_T1
	.p2align 4,,15
.globl op_bsfl_T0_cc
	.type	op_bsfl_T0_cc, @function
op_bsfl_T0_cc:
.LFB947:
	testl	%ebx, %ebx
	movl	%ebx, %eax
	je	.L840
	xorl	%edx, %edx
	testb	$1, %al
	jne	.L844
	.p2align 4,,7
.L845:
	sarl	%eax
	incl	%edx
	testb	$1, %al
	je	.L845
.L844:
	movl	%edx, %r12d
	movl	$1, 44(%rbp)
	jmp	.L847
	.p2align 4,,7
.L840:
	movl	$0, 44(%rbp)
.L847:
	ret
.LFE947:
	.size	op_bsfl_T0_cc, .-op_bsfl_T0_cc
	.p2align 4,,15
.globl op_bsrl_T0_cc
	.type	op_bsrl_T0_cc, @function
op_bsrl_T0_cc:
.LFB948:
	testl	%ebx, %ebx
	movl	%ebx, %eax
	je	.L852
	testl	%ebx, %ebx
	movl	$31, %edx
	js	.L856
	.p2align 4,,7
.L857:
	decl	%edx
	addl	%eax, %eax
	jns	.L857
.L856:
	movl	%edx, %r12d
	movl	$1, 44(%rbp)
	jmp	.L859
	.p2align 4,,7
.L852:
	movl	$0, 44(%rbp)
.L859:
	ret
.LFE948:
	.size	op_bsrl_T0_cc, .-op_bsrl_T0_cc
	.p2align 4,,15
.globl op_update_bt_cc
	.type	op_update_bt_cc, @function
op_update_bt_cc:
.LFB949:
	movl	%r12d, 40(%rbp)
	ret
.LFE949:
	.size	op_update_bt_cc, .-op_update_bt_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftl
	.type	op_movl_T0_Dshiftl, @function
op_movl_T0_Dshiftl:
.LFB950:
	movl	52(%rbp), %eax
	leal	0(,%rax,4), %ebx
	ret
.LFE950:
	.size	op_movl_T0_Dshiftl, .-op_movl_T0_Dshiftl
	.p2align 4,,15
.globl op_movsbl_T0_T0
	.type	op_movsbl_T0_T0, @function
op_movsbl_T0_T0:
.LFB957:
	movsbl	%bl,%ebx
	ret
.LFE957:
	.size	op_movsbl_T0_T0, .-op_movsbl_T0_T0
	.p2align 4,,15
.globl op_movzbl_T0_T0
	.type	op_movzbl_T0_T0, @function
op_movzbl_T0_T0:
.LFB958:
	movzbl	%bl, %ebx
	ret
.LFE958:
	.size	op_movzbl_T0_T0, .-op_movzbl_T0_T0
	.p2align 4,,15
.globl op_movswl_T0_T0
	.type	op_movswl_T0_T0, @function
op_movswl_T0_T0:
.LFB959:
	movswl	%bx,%ebx
	ret
.LFE959:
	.size	op_movswl_T0_T0, .-op_movswl_T0_T0
	.p2align 4,,15
.globl op_movzwl_T0_T0
	.type	op_movzwl_T0_T0, @function
op_movzwl_T0_T0:
.LFB960:
	movzwl	%bx, %ebx
	ret
.LFE960:
	.size	op_movzwl_T0_T0, .-op_movzwl_T0_T0
	.p2align 4,,15
.globl op_movswl_EAX_AX
	.type	op_movswl_EAX_AX, @function
op_movswl_EAX_AX:
.LFB961:
	movswl	(%rbp),%eax
	movl	%eax, (%rbp)
	ret
.LFE961:
	.size	op_movswl_EAX_AX, .-op_movswl_EAX_AX
	.p2align 4,,15
.globl op_movsbw_AX_AL
	.type	op_movsbw_AX_AL, @function
op_movsbw_AX_AL:
.LFB962:
	movl	(%rbp), %edx
	movl	%edx, %ecx
	movsbl	%dl,%edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, (%rbp)
	ret
.LFE962:
	.size	op_movsbw_AX_AL, .-op_movsbw_AX_AL
	.p2align 4,,15
.globl op_movslq_EDX_EAX
	.type	op_movslq_EDX_EAX, @function
op_movslq_EDX_EAX:
.LFB963:
	movl	(%rbp), %eax
	sarl	$31, %eax
	movl	%eax, 8(%rbp)
	ret
.LFE963:
	.size	op_movslq_EDX_EAX, .-op_movslq_EDX_EAX
	.p2align 4,,15
.globl op_movswl_DX_AX
	.type	op_movswl_DX_AX, @function
op_movswl_DX_AX:
.LFB964:
	movl	(%rbp), %eax
	sarw	$15, %ax
	cwtl
	movw	%ax, 8(%rbp)
	ret
.LFE964:
	.size	op_movswl_DX_AX, .-op_movswl_DX_AX
	.p2align 4,,15
.globl op_addl_ESI_T0
	.type	op_addl_ESI_T0, @function
op_addl_ESI_T0:
.LFB965:
	addl	%ebx, 24(%rbp)
	ret
.LFE965:
	.size	op_addl_ESI_T0, .-op_addl_ESI_T0
	.p2align 4,,15
.globl op_addw_ESI_T0
	.type	op_addw_ESI_T0, @function
op_addw_ESI_T0:
.LFB966:
	movl	24(%rbp), %edx
	movl	%edx, %ecx
	addl	%ebx, %edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, 24(%rbp)
	ret
.LFE966:
	.size	op_addw_ESI_T0, .-op_addw_ESI_T0
	.p2align 4,,15
.globl op_addl_EDI_T0
	.type	op_addl_EDI_T0, @function
op_addl_EDI_T0:
.LFB967:
	addl	%ebx, 28(%rbp)
	ret
.LFE967:
	.size	op_addl_EDI_T0, .-op_addl_EDI_T0
	.p2align 4,,15
.globl op_addw_EDI_T0
	.type	op_addw_EDI_T0, @function
op_addw_EDI_T0:
.LFB968:
	movl	28(%rbp), %edx
	movl	%edx, %ecx
	addl	%ebx, %edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, 28(%rbp)
	ret
.LFE968:
	.size	op_addw_EDI_T0, .-op_addw_EDI_T0
	.p2align 4,,15
.globl op_decl_ECX
	.type	op_decl_ECX, @function
op_decl_ECX:
.LFB969:
	decl	4(%rbp)
	ret
.LFE969:
	.size	op_decl_ECX, .-op_decl_ECX
	.p2align 4,,15
.globl op_decw_ECX
	.type	op_decw_ECX, @function
op_decw_ECX:
.LFB970:
	movl	4(%rbp), %edx
	movl	%edx, %ecx
	decl	%edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, 4(%rbp)
	ret
.LFE970:
	.size	op_decw_ECX, .-op_decw_ECX
	.p2align 4,,15
.globl op_addl_A0_SS
	.type	op_addl_A0_SS, @function
op_addl_A0_SS:
.LFB971:
	addl	104(%rbp), %r13d
	ret
.LFE971:
	.size	op_addl_A0_SS, .-op_addl_A0_SS
	.p2align 4,,15
.globl op_subl_A0_2
	.type	op_subl_A0_2, @function
op_subl_A0_2:
.LFB972:
	subl	$2, %r13d
	ret
.LFE972:
	.size	op_subl_A0_2, .-op_subl_A0_2
	.p2align 4,,15
.globl op_subl_A0_4
	.type	op_subl_A0_4, @function
op_subl_A0_4:
.LFB973:
	subl	$4, %r13d
	ret
.LFE973:
	.size	op_subl_A0_4, .-op_subl_A0_4
	.p2align 4,,15
.globl op_addl_ESP_4
	.type	op_addl_ESP_4, @function
op_addl_ESP_4:
.LFB974:
	addl	$4, 16(%rbp)
	ret
.LFE974:
	.size	op_addl_ESP_4, .-op_addl_ESP_4
	.p2align 4,,15
.globl op_addl_ESP_2
	.type	op_addl_ESP_2, @function
op_addl_ESP_2:
.LFB975:
	addl	$2, 16(%rbp)
	ret
.LFE975:
	.size	op_addl_ESP_2, .-op_addl_ESP_2
	.p2align 4,,15
.globl op_addw_ESP_4
	.type	op_addw_ESP_4, @function
op_addw_ESP_4:
.LFB976:
	movl	16(%rbp), %edx
	movl	%edx, %ecx
	addl	$4, %edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, 16(%rbp)
	ret
.LFE976:
	.size	op_addw_ESP_4, .-op_addw_ESP_4
	.p2align 4,,15
.globl op_addw_ESP_2
	.type	op_addw_ESP_2, @function
op_addw_ESP_2:
.LFB977:
	movl	16(%rbp), %edx
	movl	%edx, %ecx
	addl	$2, %edx
	xorw	%cx, %cx
	andl	$65535, %edx
	orl	%edx, %ecx
	movl	%ecx, 16(%rbp)
	ret
.LFE977:
	.size	op_addw_ESP_2, .-op_addw_ESP_2
	.p2align 4,,15
.globl op_addl_ESP_im
	.type	op_addl_ESP_im, @function
op_addl_ESP_im:
.LFB978:
	movl	$__op_param1, %eax
	addl	%eax, 16(%rbp)
	ret
.LFE978:
	.size	op_addl_ESP_im, .-op_addl_ESP_im
	.p2align 4,,15
.globl op_addw_ESP_im
	.type	op_addw_ESP_im, @function
op_addw_ESP_im:
.LFB979:
	movl	16(%rbp), %eax
	movl	$__op_param1, %edx
	movl	%eax, %ecx
	addl	%edx, %eax
	xorw	%cx, %cx
	andl	$65535, %eax
	orl	%eax, %ecx
	movl	%ecx, 16(%rbp)
	ret
.LFE979:
	.size	op_addw_ESP_im, .-op_addw_ESP_im
	.p2align 4,,15
.globl op_aam
	.type	op_aam, @function
op_aam:
.LFB987:
	movl	(%rbp), %r8d
	leal	__op_param1(%rip), %ecx
	movzbl	%r8b,%edi
	xorw	%r8w, %r8w
	movl	%edi, %eax
	cltd
	idivl	%ecx
	movl	%eax, %edi
	orl	%edx, %r8d
	movl	%edx, 44(%rbp)
	sall	$8, %edi
	orl	%edi, %r8d
	movl	%r8d, (%rbp)
	ret
.LFE987:
	.size	op_aam, .-op_aam
	.p2align 4,,15
.globl op_aad
	.type	op_aad, @function
op_aad:
.LFB988:
	movl	(%rbp), %edx
	movl	$__op_param1, %ecx
	movzbl	%dh, %eax
	imull	%ecx, %eax
	leal	(%rdx,%rax), %eax
	xorw	%dx, %dx
	movzbl	%al, %eax
	orl	%eax, %edx
	movl	%eax, 44(%rbp)
	movl	%edx, (%rbp)
	ret
.LFE988:
	.size	op_aad, .-op_aad
	.p2align 4,,15
.globl op_aaa
	.type	op_aaa, @function
op_aaa:
.LFB989:
	subq	$8, %rsp
.LCFI7:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	(%rbp), %ecx
	movl	%eax, %r8d
	movq	%rbp, %r10
	movl	%ecx, %esi
	movzbl	%cl,%r9d
	movzbl	%ch, %edi
	andl	$15, %esi
	cmpl	$9, %esi
	setg	%dl
	shrl	$4, %eax
	andl	$1, %eax
	orb	%al, %dl
	je	.L918
	leal	6(%r9), %esi
	xorl	%eax, %eax
	andl	$15, %esi
	cmpl	$249, %r9d
	setg	%al
	leal	1(%rdi,%rax), %eax
	movzbl	%al,%edi
	movl	%r8d, %eax
	orl	$17, %eax
	jmp	.L920
	.p2align 4,,7
.L918:
	movl	%r8d, %eax
	andl	$-18, %eax
.L920:
	xorw	%cx, %cx
	sall	$8, %edi
	movl	%eax, 40(%rbp)
	orl	%esi, %ecx
	orl	%edi, %ecx
	movl	%ecx, (%r10)
	addq	$8, %rsp
	ret
.LFE989:
	.size	op_aaa, .-op_aaa
	.p2align 4,,15
.globl op_aas
	.type	op_aas, @function
op_aas:
.LFB990:
	subq	$8, %rsp
.LCFI8:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	(%rbp), %ecx
	movl	%eax, %r8d
	movq	%rbp, %r10
	movl	%ecx, %edi
	movzbl	%cl,%r9d
	movzbl	%ch, %esi
	andl	$15, %edi
	cmpl	$9, %edi
	setg	%dl
	shrl	$4, %eax
	andl	$1, %eax
	orb	%al, %dl
	je	.L923
	leal	-6(%r9), %edi
	xorl	%eax, %eax
	andl	$15, %edi
	cmpl	$5, %r9d
	setle	%al
	subl	%eax, %esi
	leal	-1(%rsi), %eax
	movzbl	%al,%esi
	movl	%r8d, %eax
	orl	$17, %eax
	jmp	.L925
	.p2align 4,,7
.L923:
	movl	%r8d, %eax
	andl	$-18, %eax
.L925:
	xorw	%cx, %cx
	sall	$8, %esi
	movl	%eax, 40(%rbp)
	orl	%edi, %ecx
	orl	%esi, %ecx
	movl	%ecx, (%r10)
	addq	$8, %rsp
	ret
.LFE990:
	.size	op_aas, .-op_aas
	.p2align 4,,15
.globl op_daa
	.type	op_daa, @function
op_daa:
.LFB991:
	subq	$8, %rsp
.LCFI9:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	(%rbp), %ecx
	movl	%eax, %r8d
	movq	%rbp, %r9
	movl	%ecx, %eax
	movzbl	%cl,%esi
	andl	$15, %eax
	cmpl	$9, %eax
	movl	%r8d, %eax
	setg	%dl
	shrl	$4, %eax
	xorl	%edi, %edi
	andl	$1, %eax
	orb	%al, %dl
	je	.L930
	leal	6(%rsi), %eax
	movb	$16, %dil
	movzbl	%al,%esi
.L930:
	cmpl	$159, %esi
	movl	%r8d, %eax
	setg	%dl
	andl	$1, %eax
	orb	%al, %dl
	je	.L931
	leal	96(%rsi), %eax
	orl	$1, %edi
	movzbl	%al,%esi
.L931:
	xorb	%cl, %cl
	movl	%esi, %eax
	orl	%esi, %ecx
	cmpl	$1, %esi
	sbbl	%edx, %edx
	andl	$128, %eax
	movl	%ecx, (%r9)
	andl	$64, %edx
	orl	%edi, %edx
	orl	%eax, %edx
	movslq	%esi,%rax
	movzbl	parity_table(%rax), %eax
	orl	%edx, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE991:
	.size	op_daa, .-op_daa
	.p2align 4,,15
.globl op_das
	.type	op_das, @function
op_das:
.LFB992:
	subq	$8, %rsp
.LCFI10:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	(%rbp), %esi
	movl	%eax, %r10d
	movq	%rbp, %r11
	andl	$1, %r10d
	movl	%r10d, %r9d
	movl	%esi, %edx
	movzbl	%sil,%r8d
	andl	$15, %edx
	movl	%r8d, %ecx
	cmpl	$9, %edx
	setg	%dl
	shrl	$4, %eax
	xorl	%edi, %edi
	andl	$1, %eax
	orb	%al, %dl
	je	.L940
	cmpl	$5, %r8d
	setle	%al
	orl	%r10d, %eax
	cmpb	$1, %al
	leal	-6(%r8), %eax
	sbbl	%edi, %edi
	addl	$17, %edi
	movzbl	%al,%ecx
.L940:
	cmpl	$153, %r8d
	setg	%al
	orb	%r9b, %al
	je	.L944
	leal	-96(%rcx), %eax
	orl	$1, %edi
	movzbl	%al,%ecx
.L944:
	andb	$0, %sil
	orl	%ecx, %esi
	cmpl	$1, %ecx
	sbbl	%eax, %eax
	movl	%esi, (%r11)
	andl	$64, %eax
	orl	%eax, %edi
	movl	%ecx, %eax
	andl	$128, %eax
	orl	%eax, %edi
	movslq	%ecx,%rax
	movzbl	parity_table(%rax), %eax
	orl	%edi, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE992:
	.size	op_das, .-op_das
	.p2align 4,,15
.globl op_movl_seg_T0_vm
	.type	op_movl_seg_T0_vm, @function
op_movl_seg_T0_vm:
.LFB994:
	leaq	__op_param1(%rbp), %rdx
	movzwl	%bx,%eax
	movl	%eax, (%rdx)
	sall	$4, %eax
	movl	$0, 12(%rdx)
	movl	%eax, 4(%rdx)
	ret
.LFE994:
	.size	op_movl_seg_T0_vm, .-op_movl_seg_T0_vm
	.p2align 4,,15
.globl op_movl_T0_seg
	.type	op_movl_T0_seg, @function
op_movl_T0_seg:
.LFB995:
	movl	$__op_param1, %eax
	salq	$2, %rax
	leaq	(%rax,%rax,4), %rax
	movl	60(%rax,%rbp), %ebx
	ret
.LFE995:
	.size	op_movl_T0_seg, .-op_movl_T0_seg
	.p2align 4,,15
.globl op_arpl
	.type	op_arpl, @function
op_arpl:
.LFB1000:
	movl	%r12d, %ecx
	movl	%ebx, %eax
	xorl	%r12d, %r12d
	andl	$3, %ecx
	andl	$3, %eax
	movl	%ebx, %edx
	cmpl	%ecx, %eax
	jae	.L957
	andl	$-4, %edx
	movl	%ecx, %ebx
	movb	$64, %r12b
	orl	%edx, %ebx
.L957:
	ret
.LFE1000:
	.size	op_arpl, .-op_arpl
	.p2align 4,,15
.globl op_arpl_update
	.type	op_arpl_update, @function
op_arpl_update:
.LFB1001:
	subq	$8, %rsp
.LCFI11:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	andl	$-65, %eax
	orl	%r12d, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1001:
	.size	op_arpl_update, .-op_arpl_update
	.p2align 4,,15
.globl op_movl_T0_env
	.type	op_movl_T0_env, @function
op_movl_T0_env:
.LFB1015:
	movl	__op_param1(%rbp), %ebx
	ret
.LFE1015:
	.size	op_movl_T0_env, .-op_movl_T0_env
	.p2align 4,,15
.globl op_movl_env_T0
	.type	op_movl_env_T0, @function
op_movl_env_T0:
.LFB1016:
	movl	%ebx, __op_param1(%rbp)
	ret
.LFE1016:
	.size	op_movl_env_T0, .-op_movl_env_T0
	.p2align 4,,15
.globl op_movl_env_T1
	.type	op_movl_env_T1, @function
op_movl_env_T1:
.LFB1017:
	movl	%r12d, __op_param1(%rbp)
	ret
.LFE1017:
	.size	op_movl_env_T1, .-op_movl_env_T1
	.p2align 4,,15
.globl op_movtl_T0_env
	.type	op_movtl_T0_env, @function
op_movtl_T0_env:
.LFB1018:
	movl	__op_param1(%rbp), %ebx
	ret
.LFE1018:
	.size	op_movtl_T0_env, .-op_movtl_T0_env
	.p2align 4,,15
.globl op_movtl_env_T0
	.type	op_movtl_env_T0, @function
op_movtl_env_T0:
.LFB1019:
	movl	%ebx, __op_param1(%rbp)
	ret
.LFE1019:
	.size	op_movtl_env_T0, .-op_movtl_env_T0
	.p2align 4,,15
.globl op_movtl_T1_env
	.type	op_movtl_T1_env, @function
op_movtl_T1_env:
.LFB1020:
	movl	__op_param1(%rbp), %r12d
	ret
.LFE1020:
	.size	op_movtl_T1_env, .-op_movtl_T1_env
	.p2align 4,,15
.globl op_movtl_env_T1
	.type	op_movtl_env_T1, @function
op_movtl_env_T1:
.LFB1021:
	movl	%r12d, __op_param1(%rbp)
	ret
.LFE1021:
	.size	op_movtl_env_T1, .-op_movtl_env_T1
	.p2align 4,,15
.globl op_clts
	.type	op_clts, @function
op_clts:
.LFB1022:
	andl	$-9, 260(%rbp)
	andl	$-2049, 56(%rbp)
	ret
.LFE1022:
	.size	op_clts, .-op_clts
	.p2align 4,,15
.globl op_goto_tb0
	.type	op_goto_tb0, @function
op_goto_tb0:
.LFB1023:
	mov	__op_param1+60(%rip), %eax
	jmp	*%rax
	.p2align 4,,7
.L978:
.L979:
	rep ; ret
.LFE1023:
	.size	op_goto_tb0, .-op_goto_tb0
	.p2align 4,,15
.globl op_goto_tb1
	.type	op_goto_tb1, @function
op_goto_tb1:
.LFB1024:
	mov	__op_param1+64(%rip), %eax
	jmp	*%rax
	.p2align 4,,7
.L982:
.L983:
	rep ; ret
.LFE1024:
	.size	op_goto_tb1, .-op_goto_tb1
	.p2align 4,,15
.globl op_jmp_label
	.type	op_jmp_label, @function
op_jmp_label:
.LFB1025:
#APP
	jmp __op_gen_label1
#NO_APP
	ret
.LFE1025:
	.size	op_jmp_label, .-op_jmp_label
	.p2align 4,,15
.globl op_jnz_T0_label
	.type	op_jnz_T0_label, @function
op_jnz_T0_label:
.LFB1026:
	testl	%ebx, %ebx
	je	.L988
#APP
	jmp __op_gen_label1
#NO_APP
.L988:
	ret
.LFE1026:
	.size	op_jnz_T0_label, .-op_jnz_T0_label
	.p2align 4,,15
.globl op_jz_T0_label
	.type	op_jz_T0_label, @function
op_jz_T0_label:
.LFB1027:
	testl	%ebx, %ebx
	jne	.L992
#APP
	jmp __op_gen_label1
#NO_APP
.L992:
	ret
.LFE1027:
	.size	op_jz_T0_label, .-op_jz_T0_label
	.p2align 4,,15
.globl op_seto_T0_cc
	.type	op_seto_T0_cc, @function
op_seto_T0_cc:
.LFB1028:
	subq	$8, %rsp
.LCFI12:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	sarl	$11, %eax
	addq	$8, %rsp
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE1028:
	.size	op_seto_T0_cc, .-op_seto_T0_cc
	.p2align 4,,15
.globl op_setb_T0_cc
	.type	op_setb_T0_cc, @function
op_setb_T0_cc:
.LFB1029:
	subq	$8, %rsp
.LCFI13:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1029:
	.size	op_setb_T0_cc, .-op_setb_T0_cc
	.p2align 4,,15
.globl op_setz_T0_cc
	.type	op_setz_T0_cc, @function
op_setz_T0_cc:
.LFB1030:
	subq	$8, %rsp
.LCFI14:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	sarl	$6, %eax
	addq	$8, %rsp
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE1030:
	.size	op_setz_T0_cc, .-op_setz_T0_cc
	.p2align 4,,15
.globl op_setbe_T0_cc
	.type	op_setbe_T0_cc, @function
op_setbe_T0_cc:
.LFB1031:
	subq	$8, %rsp
.LCFI15:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %ebx
	testb	$65, %al
	setne	%bl
	addq	$8, %rsp
	ret
.LFE1031:
	.size	op_setbe_T0_cc, .-op_setbe_T0_cc
	.p2align 4,,15
.globl op_sets_T0_cc
	.type	op_sets_T0_cc, @function
op_sets_T0_cc:
.LFB1032:
	subq	$8, %rsp
.LCFI16:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	sarl	$7, %eax
	addq	$8, %rsp
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE1032:
	.size	op_sets_T0_cc, .-op_sets_T0_cc
	.p2align 4,,15
.globl op_setp_T0_cc
	.type	op_setp_T0_cc, @function
op_setp_T0_cc:
.LFB1033:
	subq	$8, %rsp
.LCFI17:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	sarl	$2, %eax
	addq	$8, %rsp
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE1033:
	.size	op_setp_T0_cc, .-op_setp_T0_cc
	.p2align 4,,15
.globl op_setl_T0_cc
	.type	op_setl_T0_cc, @function
op_setl_T0_cc:
.LFB1034:
	subq	$8, %rsp
.LCFI18:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	%eax, %edx
	addq	$8, %rsp
	sarl	$4, %edx
	xorl	%edx, %eax
	sarl	$7, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE1034:
	.size	op_setl_T0_cc, .-op_setl_T0_cc
	.p2align 4,,15
.globl op_setle_T0_cc
	.type	op_setle_T0_cc, @function
op_setle_T0_cc:
.LFB1035:
	subq	$8, %rsp
.LCFI19:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	%eax, %esi
	movl	%eax, %edx
	movl	$1, %ecx
	sarl	$4, %esi
	xorb	%sil, %al
	js	.L1012
	movl	%edx, %ecx
	shrl	$6, %ecx
	andl	$1, %ecx
.L1012:
	movl	%ecx, %ebx
	addq	$8, %rsp
	ret
.LFE1035:
	.size	op_setle_T0_cc, .-op_setle_T0_cc
	.p2align 4,,15
.globl op_xor_T0_1
	.type	op_xor_T0_1, @function
op_xor_T0_1:
.LFB1036:
	xorl	$1, %ebx
	ret
.LFE1036:
	.size	op_xor_T0_1, .-op_xor_T0_1
	.p2align 4,,15
.globl op_set_cc_op
	.type	op_set_cc_op, @function
op_set_cc_op:
.LFB1037:
	movl	$__op_param1, %eax
	movl	%eax, 48(%rbp)
	ret
.LFE1037:
	.size	op_set_cc_op, .-op_set_cc_op
	.p2align 4,,15
.globl op_mov_T0_cc
	.type	op_mov_T0_cc, @function
op_mov_T0_cc:
.LFB1038:
	subq	$8, %rsp
.LCFI20:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1038:
	.size	op_mov_T0_cc, .-op_mov_T0_cc
	.p2align 4,,15
.globl op_movl_eflags_T0_io
	.type	op_movl_eflags_T0_io, @function
op_movl_eflags_T0_io:
.LFB1041:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$2376448, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andl	$-2376449, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1041:
	.size	op_movl_eflags_T0_io, .-op_movl_eflags_T0_io
	.p2align 4,,15
.globl op_movb_eflags_T0
	.type	op_movb_eflags_T0, @function
op_movb_eflags_T0:
.LFB1045:
	subq	$8, %rsp
.LCFI21:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	%ebx, %edx
	andl	$2048, %eax
	andl	$213, %edx
	orl	%eax, %edx
	movl	%edx, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1045:
	.size	op_movb_eflags_T0, .-op_movb_eflags_T0
	.p2align 4,,15
.globl op_movl_T0_eflags
	.type	op_movl_T0_eflags, @function
op_movl_T0_eflags:
.LFB1046:
	subq	$8, %rsp
.LCFI22:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	36(%rbp), %ecx
	movl	52(%rbp), %edx
	addq	$8, %rsp
	andl	$-196609, %ecx
	andl	$1024, %edx
	orl	%eax, %edx
	movl	%ecx, %ebx
	orl	%edx, %ebx
	ret
.LFE1046:
	.size	op_movl_T0_eflags, .-op_movl_T0_eflags
	.p2align 4,,15
.globl op_cld
	.type	op_cld, @function
op_cld:
.LFB1047:
	movl	$1, 52(%rbp)
	ret
.LFE1047:
	.size	op_cld, .-op_cld
	.p2align 4,,15
.globl op_std
	.type	op_std, @function
op_std:
.LFB1048:
	movl	$-1, 52(%rbp)
	ret
.LFE1048:
	.size	op_std, .-op_std
	.p2align 4,,15
.globl op_clc
	.type	op_clc, @function
op_clc:
.LFB1049:
	subq	$8, %rsp
.LCFI23:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	andl	$-2, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1049:
	.size	op_clc, .-op_clc
	.p2align 4,,15
.globl op_stc
	.type	op_stc, @function
op_stc:
.LFB1050:
	subq	$8, %rsp
.LCFI24:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	orl	$1, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1050:
	.size	op_stc, .-op_stc
	.p2align 4,,15
.globl op_cmc
	.type	op_cmc, @function
op_cmc:
.LFB1051:
	subq	$8, %rsp
.LCFI25:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	$1, %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1051:
	.size	op_cmc, .-op_cmc
	.p2align 4,,15
.globl op_salc
	.type	op_salc, @function
op_salc:
.LFB1052:
	subq	$8, %rsp
.LCFI26:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table+8(%rax)
	negl	%eax
	movb	%al, (%rbp)
	addq	$8, %rsp
	ret
.LFE1052:
	.size	op_salc, .-op_salc
	.p2align 4,,15
	.type	compute_all_eflags, @function
compute_all_eflags:
.LFB1053:
	movl	40(%rbp), %eax
	ret
.LFE1053:
	.size	compute_all_eflags, .-compute_all_eflags
	.p2align 4,,15
	.type	compute_c_eflags, @function
compute_c_eflags:
.LFB1054:
	movl	40(%rbp), %eax
	andl	$1, %eax
	ret
.LFE1054:
	.size	compute_c_eflags, .-compute_c_eflags
	.p2align 4,,15
.globl op_fpush
	.type	op_fpush, @function
op_fpush:
.LFB1077:
	movl	284(%rbp), %eax
	decl	%eax
	andl	$7, %eax
	movl	%eax, 284(%rbp)
	mov	%eax, %eax
	movb	$0, 296(%rax,%rbp)
	ret
.LFE1077:
	.size	op_fpush, .-op_fpush
	.p2align 4,,15
.globl op_fpop
	.type	op_fpop, @function
op_fpop:
.LFB1078:
	mov	284(%rbp), %eax
	movb	$1, 296(%rax,%rbp)
	movl	284(%rbp), %eax
	incl	%eax
	andl	$7, %eax
	movl	%eax, 284(%rbp)
	ret
.LFE1078:
	.size	op_fpop, .-op_fpop
	.p2align 4,,15
.globl op_fdecstp
	.type	op_fdecstp, @function
op_fdecstp:
.LFB1079:
	movl	284(%rbp), %eax
	andl	$-18177, 288(%rbp)
	decl	%eax
	andl	$7, %eax
	movl	%eax, 284(%rbp)
	ret
.LFE1079:
	.size	op_fdecstp, .-op_fdecstp
	.p2align 4,,15
.globl op_fincstp
	.type	op_fincstp, @function
op_fincstp:
.LFB1080:
	movl	284(%rbp), %eax
	andl	$-18177, 288(%rbp)
	incl	%eax
	andl	$7, %eax
	movl	%eax, 284(%rbp)
	ret
.LFE1080:
	.size	op_fincstp, .-op_fincstp
	.p2align 4,,15
.globl op_ffree_STN
	.type	op_ffree_STN, @function
op_ffree_STN:
.LFB1081:
	mov	284(%rbp), %eax
	addq	$__op_param1, %rax
	andl	$7, %eax
	movb	$1, 296(%rax,%rbp)
	ret
.LFE1081:
	.size	op_ffree_STN, .-op_ffree_STN
	.p2align 4,,15
.globl op_fmov_ST0_FT0
	.type	op_fmov_ST0_FT0, @function
op_fmov_ST0_FT0:
.LFB1082:
	mov	284(%rbp), %ecx
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1082:
	.size	op_fmov_ST0_FT0, .-op_fmov_ST0_FT0
	.p2align 4,,15
.globl op_fmov_FT0_STN
	.type	op_fmov_FT0_STN, @function
op_fmov_FT0_STN:
.LFB1083:
	mov	284(%rbp), %edx
	addq	$__op_param1, %rdx
	andl	$7, %edx
	salq	$4, %rdx
	movl	312(%rdx,%rbp), %ecx
	movq	304(%rdx,%rbp), %rdx
	movl	%ecx, 456(%rbp)
	movq	%rdx, 448(%rbp)
	ret
.LFE1083:
	.size	op_fmov_FT0_STN, .-op_fmov_FT0_STN
	.p2align 4,,15
.globl op_fmov_ST0_STN
	.type	op_fmov_ST0_STN, @function
op_fmov_ST0_STN:
.LFB1084:
	mov	284(%rbp), %eax
	movq	%rax, %rsi
	addq	$__op_param1, %rax
	andl	$7, %eax
	salq	$4, %rsi
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 312(%rsi,%rbp)
	movq	%rax, 304(%rsi,%rbp)
	ret
.LFE1084:
	.size	op_fmov_ST0_STN, .-op_fmov_ST0_STN
	.p2align 4,,15
.globl op_fmov_STN_ST0
	.type	op_fmov_STN_ST0, @function
op_fmov_STN_ST0:
.LFB1085:
	mov	284(%rbp), %eax
	leaq	__op_param1(%rax), %rcx
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	andl	$7, %ecx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1085:
	.size	op_fmov_STN_ST0, .-op_fmov_STN_ST0
	.p2align 4,,15
.globl op_fxchg_ST0_STN
	.type	op_fxchg_ST0_STN, @function
op_fxchg_ST0_STN:
.LFB1086:
	mov	284(%rbp), %eax
	leaq	__op_param1(%rax), %rcx
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	andl	$7, %ecx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	salq	$4, %rcx
	addq	%rbp, %rcx
	movq	304(%rcx), %rsi
	movl	312(%rcx), %edi
	movq	%rax, 304(%rcx)
	movl	%edx, 312(%rcx)
	mov	284(%rbp), %eax
	salq	$4, %rax
	movq	%rsi, 304(%rax,%rbp)
	movl	%edi, 312(%rax,%rbp)
	ret
.LFE1086:
	.size	op_fxchg_ST0_STN, .-op_fxchg_ST0_STN
	.p2align 4,,15
.globl op_fcmov_ST0_STN_T0
	.type	op_fcmov_ST0_STN_T0, @function
op_fcmov_ST0_STN_T0:
.LFB1091:
	testl	%ebx, %ebx
	je	.L1063
	mov	284(%rbp), %eax
	movq	%rax, %rsi
	addq	$__op_param1, %rax
	andl	$7, %eax
	salq	$4, %rsi
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 312(%rsi,%rbp)
	movq	%rax, 304(%rsi,%rbp)
.L1063:
	ret
.LFE1091:
	.size	op_fcmov_ST0_STN_T0, .-op_fcmov_ST0_STN_T0
	.p2align 4,,15
.globl op_fadd_ST0_FT0
	.type	op_fadd_ST0_FT0, @function
op_fadd_ST0_FT0:
.LFB1092:
	mov	284(%rbp), %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	448(%rbp)
	faddp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1092:
	.size	op_fadd_ST0_FT0, .-op_fadd_ST0_FT0
	.p2align 4,,15
.globl op_fmul_ST0_FT0
	.type	op_fmul_ST0_FT0, @function
op_fmul_ST0_FT0:
.LFB1093:
	mov	284(%rbp), %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	448(%rbp)
	fmulp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1093:
	.size	op_fmul_ST0_FT0, .-op_fmul_ST0_FT0
	.p2align 4,,15
.globl op_fsub_ST0_FT0
	.type	op_fsub_ST0_FT0, @function
op_fsub_ST0_FT0:
.LFB1094:
	mov	284(%rbp), %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	448(%rbp)
	fsubrp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1094:
	.size	op_fsub_ST0_FT0, .-op_fsub_ST0_FT0
	.p2align 4,,15
.globl op_fsubr_ST0_FT0
	.type	op_fsubr_ST0_FT0, @function
op_fsubr_ST0_FT0:
.LFB1095:
	fldt	448(%rbp)
	mov	284(%rbp), %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fsubrp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1095:
	.size	op_fsubr_ST0_FT0, .-op_fsubr_ST0_FT0
	.p2align 4,,15
.globl op_fadd_STN_ST0
	.type	op_fadd_STN_ST0, @function
op_fadd_STN_ST0:
.LFB1098:
	mov	284(%rbp), %edx
	leaq	__op_param1(%rdx), %rax
	salq	$4, %rdx
	andl	$7, %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	304(%rdx,%rbp)
	faddp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1098:
	.size	op_fadd_STN_ST0, .-op_fadd_STN_ST0
	.p2align 4,,15
.globl op_fmul_STN_ST0
	.type	op_fmul_STN_ST0, @function
op_fmul_STN_ST0:
.LFB1099:
	mov	284(%rbp), %edx
	leaq	__op_param1(%rdx), %rax
	salq	$4, %rdx
	andl	$7, %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	304(%rdx,%rbp)
	fmulp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1099:
	.size	op_fmul_STN_ST0, .-op_fmul_STN_ST0
	.p2align 4,,15
.globl op_fsub_STN_ST0
	.type	op_fsub_STN_ST0, @function
op_fsub_STN_ST0:
.LFB1100:
	mov	284(%rbp), %edx
	leaq	__op_param1(%rdx), %rax
	salq	$4, %rdx
	andl	$7, %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fldt	304(%rdx,%rbp)
	fsubrp	%st, %st(1)
	fstpt	304(%rax)
	ret
.LFE1100:
	.size	op_fsub_STN_ST0, .-op_fsub_STN_ST0
	.p2align 4,,15
.globl op_fsubr_STN_ST0
	.type	op_fsubr_STN_ST0, @function
op_fsubr_STN_ST0:
.LFB1101:
	mov	284(%rbp), %edx
	leaq	__op_param1(%rdx), %rax
	salq	$4, %rdx
	fldt	304(%rdx,%rbp)
	andl	$7, %eax
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rax
	fldt	(%rax)
	fsubrp	%st, %st(1)
	fstpt	(%rax)
	ret
.LFE1101:
	.size	op_fsubr_STN_ST0, .-op_fsubr_STN_ST0
	.p2align 4,,15
.globl op_fchs_ST0
	.type	op_fchs_ST0, @function
op_fchs_ST0:
.LFB1104:
	mov	284(%rbp), %eax
	salq	$4, %rax
	addq	%rbp, %rax
	fldt	304(%rax)
	fchs
	fstpt	304(%rax)
	ret
.LFE1104:
	.size	op_fchs_ST0, .-op_fchs_ST0
	.p2align 4,,15
.globl op_fld1_ST0
	.type	op_fld1_ST0, @function
op_fld1_ST0:
.LFB1107:
	mov	284(%rbp), %ecx
	movq	f15rk+16(%rip), %rax
	movl	f15rk+24(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1107:
	.size	op_fld1_ST0, .-op_fld1_ST0
	.p2align 4,,15
.globl op_fldl2t_ST0
	.type	op_fldl2t_ST0, @function
op_fldl2t_ST0:
.LFB1108:
	mov	284(%rbp), %ecx
	movq	f15rk+96(%rip), %rax
	movl	f15rk+104(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1108:
	.size	op_fldl2t_ST0, .-op_fldl2t_ST0
	.p2align 4,,15
.globl op_fldl2e_ST0
	.type	op_fldl2e_ST0, @function
op_fldl2e_ST0:
.LFB1109:
	mov	284(%rbp), %ecx
	movq	f15rk+80(%rip), %rax
	movl	f15rk+88(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1109:
	.size	op_fldl2e_ST0, .-op_fldl2e_ST0
	.p2align 4,,15
.globl op_fldpi_ST0
	.type	op_fldpi_ST0, @function
op_fldpi_ST0:
.LFB1110:
	mov	284(%rbp), %ecx
	movq	f15rk+32(%rip), %rax
	movl	f15rk+40(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1110:
	.size	op_fldpi_ST0, .-op_fldpi_ST0
	.p2align 4,,15
.globl op_fldlg2_ST0
	.type	op_fldlg2_ST0, @function
op_fldlg2_ST0:
.LFB1111:
	mov	284(%rbp), %ecx
	movq	f15rk+48(%rip), %rax
	movl	f15rk+56(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1111:
	.size	op_fldlg2_ST0, .-op_fldlg2_ST0
	.p2align 4,,15
.globl op_fldln2_ST0
	.type	op_fldln2_ST0, @function
op_fldln2_ST0:
.LFB1112:
	mov	284(%rbp), %ecx
	movq	f15rk+64(%rip), %rax
	movl	f15rk+72(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1112:
	.size	op_fldln2_ST0, .-op_fldln2_ST0
	.p2align 4,,15
.globl op_fldz_ST0
	.type	op_fldz_ST0, @function
op_fldz_ST0:
.LFB1113:
	mov	284(%rbp), %ecx
	movq	f15rk(%rip), %rax
	movl	f15rk+8(%rip), %edx
	salq	$4, %rcx
	movq	%rax, 304(%rcx,%rbp)
	movl	%edx, 312(%rcx,%rbp)
	ret
.LFE1113:
	.size	op_fldz_ST0, .-op_fldz_ST0
	.p2align 4,,15
.globl op_fldz_FT0
	.type	op_fldz_FT0, @function
op_fldz_FT0:
.LFB1114:
	movq	f15rk(%rip), %rax
	movl	f15rk+8(%rip), %edx
	movq	%rax, 448(%rbp)
	movl	%edx, 456(%rbp)
	ret
.LFE1114:
	.size	op_fldz_FT0, .-op_fldz_FT0
	.p2align 4,,15
.globl op_fnstsw_EAX
	.type	op_fnstsw_EAX, @function
op_fnstsw_EAX:
.LFB1130:
	movl	284(%rbp), %eax
	movl	288(%rbp), %edx
	movl	(%rbp), %esi
	andl	$7, %eax
	andb	$199, %dh
	sall	$11, %eax
	xorw	%si, %si
	orl	%eax, %edx
	orl	%edx, %esi
	movl	%esi, (%rbp)
	ret
.LFE1130:
	.size	op_fnstsw_EAX, .-op_fnstsw_EAX
	.p2align 4,,15
.globl op_fclex
	.type	op_fclex, @function
op_fclex:
.LFB1133:
	andl	$32512, 288(%rbp)
	ret
.LFE1133:
	.size	op_fclex, .-op_fclex
	.p2align 4,,15
.globl op_fninit
	.type	op_fninit, @function
op_fninit:
.LFB1135:
	movl	$0, 288(%rbp)
	movl	$0, 284(%rbp)
	movl	$895, 292(%rbp)
	movb	$1, 296(%rbp)
	movb	$1, 297(%rbp)
	movb	$1, 298(%rbp)
	movb	$1, 299(%rbp)
	movb	$1, 300(%rbp)
	movb	$1, 301(%rbp)
	movb	$1, 302(%rbp)
	movb	$1, 303(%rbp)
	ret
.LFE1135:
	.size	op_fninit, .-op_fninit
	.p2align 4,,15
.globl op_movo
	.type	op_movo, @function
op_movo:
.LFB1143:
	movl	__op_param2(%rbp), %edx
	leaq	__op_param2(%rbp), %rcx
	leaq	__op_param1(%rbp), %rsi
	movl	%edx, __op_param1(%rbp)
	movl	4(%rcx), %eax
	movl	%eax, 4(%rsi)
	movl	8(%rcx), %eax
	movl	%eax, 8(%rsi)
	movl	12(%rcx), %eax
	movl	%eax, 12(%rsi)
	ret
.LFE1143:
	.size	op_movo, .-op_movo
	.p2align 4,,15
.globl op_movq
	.type	op_movq, @function
op_movq:
.LFB1144:
	movq	__op_param2(%rbp), %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1144:
	.size	op_movq, .-op_movq
	.p2align 4,,15
.globl op_movl
	.type	op_movl, @function
op_movl:
.LFB1145:
	movl	__op_param2(%rbp), %eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1145:
	.size	op_movl, .-op_movl
	.p2align 4,,15
.globl op_movq_env_0
	.type	op_movq_env_0, @function
op_movq_env_0:
.LFB1146:
	movq	$0, __op_param1(%rbp)
	ret
.LFE1146:
	.size	op_movq_env_0, .-op_movq_env_0
	.p2align 4,,15
.globl op_enter_mmx
	.type	op_enter_mmx, @function
op_enter_mmx:
.LFB1149:
	movl	$0, 284(%rbp)
	movl	$0, 296(%rbp)
	movl	$0, 300(%rbp)
	ret
.LFE1149:
	.size	op_enter_mmx, .-op_enter_mmx
	.p2align 4,,15
.globl op_emms
	.type	op_emms, @function
op_emms:
.LFB1150:
	movl	$16843009, 296(%rbp)
	movl	$16843009, 300(%rbp)
	ret
.LFE1150:
	.size	op_emms, .-op_emms
	.p2align 4,,15
.globl op_psrlw_mmx
	.type	op_psrlw_mmx, @function
op_psrlw_mmx:
.LFB1151:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	jbe	.L1119
	movq	$0, __op_param1(%rbp)
	jmp	.L1121
	.p2align 4,,7
.L1119:
	movzbl	__op_param2(%rbp), %ecx
	movzwl	__op_param1(%rbp), %eax
	sarl	%cl, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	sarl	%cl, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	sarl	%cl, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	sarl	%cl, %eax
	movw	%ax, 6(%rsi)
.L1121:
	ret
.LFE1151:
	.size	op_psrlw_mmx, .-op_psrlw_mmx
	.p2align 4,,15
.globl op_psraw_mmx
	.type	op_psraw_mmx, @function
op_psraw_mmx:
.LFB1152:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	movl	$15, %ecx
	ja	.L1126
	movzbl	__op_param2(%rbp), %ecx
.L1126:
	movswl	(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, (%rdx)
	movswl	2(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 2(%rdx)
	movswl	4(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 4(%rdx)
	movswl	6(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 6(%rdx)
	ret
.LFE1152:
	.size	op_psraw_mmx, .-op_psraw_mmx
	.p2align 4,,15
.globl op_psllw_mmx
	.type	op_psllw_mmx, @function
op_psllw_mmx:
.LFB1153:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	jbe	.L1129
	movq	$0, __op_param1(%rbp)
	jmp	.L1131
	.p2align 4,,7
.L1129:
	movzbl	__op_param2(%rbp), %ecx
	movzwl	__op_param1(%rbp), %eax
	sall	%cl, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	sall	%cl, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	sall	%cl, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	sall	%cl, %eax
	movw	%ax, 6(%rsi)
.L1131:
	ret
.LFE1153:
	.size	op_psllw_mmx, .-op_psllw_mmx
	.p2align 4,,15
.globl op_psrld_mmx
	.type	op_psrld_mmx, @function
op_psrld_mmx:
.LFB1154:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1134
	movq	$0, __op_param1(%rbp)
	jmp	.L1136
	.p2align 4,,7
.L1134:
	movzbl	__op_param2(%rbp), %ecx
	shrl	%cl, (%rdx)
	shrl	%cl, 4(%rdx)
.L1136:
	ret
.LFE1154:
	.size	op_psrld_mmx, .-op_psrld_mmx
	.p2align 4,,15
.globl op_psrad_mmx
	.type	op_psrad_mmx, @function
op_psrad_mmx:
.LFB1155:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rax
	movl	$31, %ecx
	ja	.L1141
	movzbl	__op_param2(%rbp), %ecx
.L1141:
	sarl	%cl, (%rax)
	sarl	%cl, 4(%rax)
	ret
.LFE1155:
	.size	op_psrad_mmx, .-op_psrad_mmx
	.p2align 4,,15
.globl op_pslld_mmx
	.type	op_pslld_mmx, @function
op_pslld_mmx:
.LFB1156:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1144
	movq	$0, __op_param1(%rbp)
	jmp	.L1146
	.p2align 4,,7
.L1144:
	movzbl	__op_param2(%rbp), %ecx
	sall	%cl, (%rdx)
	sall	%cl, 4(%rdx)
.L1146:
	ret
.LFE1156:
	.size	op_pslld_mmx, .-op_pslld_mmx
	.p2align 4,,15
.globl op_psrlq_mmx
	.type	op_psrlq_mmx, @function
op_psrlq_mmx:
.LFB1157:
	cmpq	$63, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1149
	movq	$0, __op_param1(%rbp)
	jmp	.L1151
	.p2align 4,,7
.L1149:
	movzbl	__op_param2(%rbp), %ecx
	shrq	%cl, (%rdx)
.L1151:
	ret
.LFE1157:
	.size	op_psrlq_mmx, .-op_psrlq_mmx
	.p2align 4,,15
.globl op_psllq_mmx
	.type	op_psllq_mmx, @function
op_psllq_mmx:
.LFB1158:
	cmpq	$63, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1154
	movq	$0, __op_param1(%rbp)
	jmp	.L1156
	.p2align 4,,7
.L1154:
	movzbl	__op_param2(%rbp), %ecx
	salq	%cl, (%rdx)
.L1156:
	ret
.LFE1158:
	.size	op_psllq_mmx, .-op_psllq_mmx
	.p2align 4,,15
.globl op_paddb_mmx
	.type	op_paddb_mmx, @function
op_paddb_mmx:
.LFB1163:
	leaq	__op_param1(%rbp), %rax
	movzbl	__op_param2(%rbp), %edx
	leaq	__op_param2(%rbp), %rcx
	addb	%dl, (%rax)
	movzbl	1(%rcx), %edx
	addb	%dl, 1(%rax)
	movzbl	2(%rcx), %edx
	addb	%dl, 2(%rax)
	movzbl	3(%rcx), %edx
	addb	%dl, 3(%rax)
	movzbl	4(%rcx), %edx
	addb	%dl, 4(%rax)
	movzbl	5(%rcx), %edx
	addb	%dl, 5(%rax)
	movzbl	6(%rcx), %edx
	addb	%dl, 6(%rax)
	movzbl	7(%rcx), %edx
	addb	%dl, 7(%rax)
	ret
.LFE1163:
	.size	op_paddb_mmx, .-op_paddb_mmx
	.p2align 4,,15
.globl op_paddw_mmx
	.type	op_paddw_mmx, @function
op_paddw_mmx:
.LFB1164:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	addw	%dx, (%rax)
	movzwl	2(%rcx), %edx
	addw	%dx, 2(%rax)
	movl	4(%rcx), %edx
	addw	%dx, 4(%rax)
	movzwl	6(%rcx), %edx
	addw	%dx, 6(%rax)
	ret
.LFE1164:
	.size	op_paddw_mmx, .-op_paddw_mmx
	.p2align 4,,15
.globl op_paddl_mmx
	.type	op_paddl_mmx, @function
op_paddl_mmx:
.LFB1165:
	leaq	__op_param1(%rbp), %rax
	movl	__op_param2(%rbp), %ecx
	addl	%ecx, (%rax)
	movl	__op_param2+4(%rbp), %edx
	addl	%edx, 4(%rax)
	ret
.LFE1165:
	.size	op_paddl_mmx, .-op_paddl_mmx
	.p2align 4,,15
.globl op_paddq_mmx
	.type	op_paddq_mmx, @function
op_paddq_mmx:
.LFB1166:
	movq	__op_param2(%rbp), %rdx
	addq	%rdx, __op_param1(%rbp)
	ret
.LFE1166:
	.size	op_paddq_mmx, .-op_paddq_mmx
	.p2align 4,,15
.globl op_psubb_mmx
	.type	op_psubb_mmx, @function
op_psubb_mmx:
.LFB1167:
	leaq	__op_param1(%rbp), %rax
	movzbl	__op_param2(%rbp), %edx
	leaq	__op_param2(%rbp), %rcx
	subb	%dl, (%rax)
	movzbl	1(%rcx), %edx
	subb	%dl, 1(%rax)
	movzbl	2(%rcx), %edx
	subb	%dl, 2(%rax)
	movzbl	3(%rcx), %edx
	subb	%dl, 3(%rax)
	movzbl	4(%rcx), %edx
	subb	%dl, 4(%rax)
	movzbl	5(%rcx), %edx
	subb	%dl, 5(%rax)
	movzbl	6(%rcx), %edx
	subb	%dl, 6(%rax)
	movzbl	7(%rcx), %edx
	subb	%dl, 7(%rax)
	ret
.LFE1167:
	.size	op_psubb_mmx, .-op_psubb_mmx
	.p2align 4,,15
.globl op_psubw_mmx
	.type	op_psubw_mmx, @function
op_psubw_mmx:
.LFB1168:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	subw	%dx, (%rax)
	movzwl	2(%rcx), %edx
	subw	%dx, 2(%rax)
	movl	4(%rcx), %edx
	subw	%dx, 4(%rax)
	movzwl	6(%rcx), %edx
	subw	%dx, 6(%rax)
	ret
.LFE1168:
	.size	op_psubw_mmx, .-op_psubw_mmx
	.p2align 4,,15
.globl op_psubl_mmx
	.type	op_psubl_mmx, @function
op_psubl_mmx:
.LFB1169:
	leaq	__op_param1(%rbp), %rax
	movl	__op_param2(%rbp), %ecx
	subl	%ecx, (%rax)
	movl	__op_param2+4(%rbp), %edx
	subl	%edx, 4(%rax)
	ret
.LFE1169:
	.size	op_psubl_mmx, .-op_psubl_mmx
	.p2align 4,,15
.globl op_psubq_mmx
	.type	op_psubq_mmx, @function
op_psubq_mmx:
.LFB1170:
	movq	__op_param2(%rbp), %rdx
	subq	%rdx, __op_param1(%rbp)
	ret
.LFE1170:
	.size	op_psubq_mmx, .-op_psubq_mmx
	.p2align 4,,15
.globl op_pminub_mmx
	.type	op_pminub_mmx, @function
op_pminub_mmx:
.LFB1179:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rcx
	cmpb	%al, %dil
	cmovbe	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %edx
	movzbl	1(%rsi), %eax
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 7(%rsi)
	ret
.LFE1179:
	.size	op_pminub_mmx, .-op_pminub_mmx
	.p2align 4,,15
.globl op_pmaxub_mmx
	.type	op_pmaxub_mmx, @function
op_pmaxub_mmx:
.LFB1180:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rcx
	cmpb	%al, %dil
	cmovae	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %edx
	movzbl	1(%rsi), %eax
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 7(%rsi)
	ret
.LFE1180:
	.size	op_pmaxub_mmx, .-op_pmaxub_mmx
	.p2align 4,,15
.globl op_pminsw_mmx
	.type	op_pminsw_mmx, @function
op_pminsw_mmx:
.LFB1181:
	movl	__op_param1(%rbp), %eax
	cmpw	%ax, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	cmovle	__op_param2(%rbp), %ax
	leaq	__op_param2(%rbp), %rcx
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	cmpw	%ax, 2(%rcx)
	cmovle	2(%rcx), %ax
	movw	%ax, 2(%rsi)
	movl	4(%rsi), %eax
	cmpw	%ax, 4(%rcx)
	cmovle	4(%rcx), %ax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	cmpw	%ax, 6(%rcx)
	cmovle	6(%rcx), %ax
	movw	%ax, 6(%rsi)
	ret
.LFE1181:
	.size	op_pminsw_mmx, .-op_pminsw_mmx
	.p2align 4,,15
.globl op_pmaxsw_mmx
	.type	op_pmaxsw_mmx, @function
op_pmaxsw_mmx:
.LFB1182:
	movl	__op_param1(%rbp), %eax
	cmpw	%ax, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	cmovge	__op_param2(%rbp), %ax
	leaq	__op_param2(%rbp), %rcx
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	cmpw	%ax, 2(%rcx)
	cmovge	2(%rcx), %ax
	movw	%ax, 2(%rsi)
	movl	4(%rsi), %eax
	cmpw	%ax, 4(%rcx)
	cmovge	4(%rcx), %ax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	cmpw	%ax, 6(%rcx)
	cmovge	6(%rcx), %ax
	movw	%ax, 6(%rsi)
	ret
.LFE1182:
	.size	op_pmaxsw_mmx, .-op_pmaxsw_mmx
	.p2align 4,,15
.globl op_pand_mmx
	.type	op_pand_mmx, @function
op_pand_mmx:
.LFB1183:
	movq	__op_param2(%rbp), %rdx
	andq	%rdx, __op_param1(%rbp)
	ret
.LFE1183:
	.size	op_pand_mmx, .-op_pand_mmx
	.p2align 4,,15
.globl op_pandn_mmx
	.type	op_pandn_mmx, @function
op_pandn_mmx:
.LFB1184:
	movq	__op_param1(%rbp), %rax
	notq	%rax
	andq	__op_param2(%rbp), %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1184:
	.size	op_pandn_mmx, .-op_pandn_mmx
	.p2align 4,,15
.globl op_por_mmx
	.type	op_por_mmx, @function
op_por_mmx:
.LFB1185:
	movq	__op_param2(%rbp), %rdx
	orq	%rdx, __op_param1(%rbp)
	ret
.LFE1185:
	.size	op_por_mmx, .-op_por_mmx
	.p2align 4,,15
.globl op_pxor_mmx
	.type	op_pxor_mmx, @function
op_pxor_mmx:
.LFB1186:
	movq	__op_param2(%rbp), %rdx
	xorq	%rdx, __op_param1(%rbp)
	ret
.LFE1186:
	.size	op_pxor_mmx, .-op_pxor_mmx
	.p2align 4,,15
.globl op_pcmpgtb_mmx
	.type	op_pcmpgtb_mmx, @function
op_pcmpgtb_mmx:
.LFB1187:
	movzbl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	cmpb	__op_param2(%rbp), %al
	setle	%al
	decl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %eax
	cmpb	1(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	cmpb	2(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	cmpb	3(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	cmpb	4(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	cmpb	5(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	cmpb	6(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	cmpb	7(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 7(%rcx)
	ret
.LFE1187:
	.size	op_pcmpgtb_mmx, .-op_pcmpgtb_mmx
	.p2align 4,,15
.globl op_pcmpgtw_mmx
	.type	op_pcmpgtw_mmx, @function
op_pcmpgtw_mmx:
.LFB1188:
	movl	__op_param1(%rbp), %eax
	cmpw	__op_param2(%rbp), %ax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	cmpw	2(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	cmpw	4(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	cmpw	6(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%rcx)
	ret
.LFE1188:
	.size	op_pcmpgtw_mmx, .-op_pcmpgtw_mmx
	.p2align 4,,15
.globl op_pcmpgtl_mmx
	.type	op_pcmpgtl_mmx, @function
op_pcmpgtl_mmx:
.LFB1189:
	movl	__op_param1(%rbp), %eax
	cmpl	__op_param2(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movl	4(%rcx), %eax
	cmpl	__op_param2+4(%rbp), %eax
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rcx)
	ret
.LFE1189:
	.size	op_pcmpgtl_mmx, .-op_pcmpgtl_mmx
	.p2align 4,,15
.globl op_pcmpeqb_mmx
	.type	op_pcmpeqb_mmx, @function
op_pcmpeqb_mmx:
.LFB1190:
	movzbl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	cmpb	__op_param2(%rbp), %al
	setne	%al
	decl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %eax
	cmpb	1(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	cmpb	2(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	cmpb	3(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	cmpb	4(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	cmpb	5(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	cmpb	6(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	cmpb	7(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 7(%rcx)
	ret
.LFE1190:
	.size	op_pcmpeqb_mmx, .-op_pcmpeqb_mmx
	.p2align 4,,15
.globl op_pcmpeqw_mmx
	.type	op_pcmpeqw_mmx, @function
op_pcmpeqw_mmx:
.LFB1191:
	movl	__op_param1(%rbp), %eax
	cmpw	__op_param2(%rbp), %ax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	cmpw	2(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	cmpw	4(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	cmpw	6(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%rcx)
	ret
.LFE1191:
	.size	op_pcmpeqw_mmx, .-op_pcmpeqw_mmx
	.p2align 4,,15
.globl op_pcmpeql_mmx
	.type	op_pcmpeql_mmx, @function
op_pcmpeql_mmx:
.LFB1192:
	movl	__op_param1(%rbp), %eax
	cmpl	__op_param2(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movl	4(%rcx), %eax
	cmpl	__op_param2+4(%rbp), %eax
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rcx)
	ret
.LFE1192:
	.size	op_pcmpeql_mmx, .-op_pcmpeql_mmx
	.p2align 4,,15
.globl op_pmullw_mmx
	.type	op_pmullw_mmx, @function
op_pmullw_mmx:
.LFB1193:
	movl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	imull	__op_param2(%rbp), %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	imulw	2(%rsi), %ax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	imull	4(%rsi), %eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	imulw	6(%rsi), %ax
	movw	%ax, 6(%rcx)
	ret
.LFE1193:
	.size	op_pmullw_mmx, .-op_pmullw_mmx
	.p2align 4,,15
.globl op_pmulhuw_mmx
	.type	op_pmulhuw_mmx, @function
op_pmulhuw_mmx:
.LFB1194:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdi), %edx
	movzwl	2(%rsi), %eax
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	movzwl	4(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	movzwl	6(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%rsi)
	ret
.LFE1194:
	.size	op_pmulhuw_mmx, .-op_pmulhuw_mmx
	.p2align 4,,15
.globl op_pmulhw_mmx
	.type	op_pmulhw_mmx, @function
op_pmulhw_mmx:
.LFB1195:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%rdi),%edx
	movswl	2(%rsi),%eax
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%rsi)
	movswl	4(%rsi),%eax
	movswl	4(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%rsi)
	movswl	6(%rsi),%eax
	movswl	6(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%rsi)
	ret
.LFE1195:
	.size	op_pmulhw_mmx, .-op_pmulhw_mmx
	.p2align 4,,15
.globl op_pavgb_mmx
	.type	op_pavgb_mmx, @function
op_pavgb_mmx:
.LFB1196:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %esi
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdi
	leal	1(%rax,%rsi), %eax
	sarl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rdi), %edx
	movzbl	1(%rcx), %eax
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	movzbl	2(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	movzbl	3(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	movzbl	4(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	movzbl	5(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	movzbl	6(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	movzbl	7(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 7(%rcx)
	ret
.LFE1196:
	.size	op_pavgb_mmx, .-op_pavgb_mmx
	.p2align 4,,15
.globl op_pavgw_mmx
	.type	op_pavgw_mmx, @function
op_pavgw_mmx:
.LFB1197:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %ecx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	leal	1(%rax,%rcx), %eax
	sarl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdi), %edx
	movzwl	2(%rsi), %eax
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	movzwl	4(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	movzwl	6(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 6(%rsi)
	ret
.LFE1197:
	.size	op_pavgw_mmx, .-op_pavgw_mmx
	.p2align 4,,15
.globl op_pmuludq_mmx
	.type	op_pmuludq_mmx, @function
op_pmuludq_mmx:
.LFB1198:
	mov	__op_param1(%rbp), %ecx
	mov	__op_param2(%rbp), %edx
	imulq	%rcx, %rdx
	movq	%rdx, __op_param1(%rbp)
	ret
.LFE1198:
	.size	op_pmuludq_mmx, .-op_pmuludq_mmx
	.p2align 4,,15
.globl op_pmaddwd_mmx
	.type	op_pmaddwd_mmx, @function
op_pmaddwd_mmx:
.LFB1199:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%ecx
	leaq	__op_param1(%rbp), %r8
	leaq	__op_param2(%rbp), %rdi
	movswl	2(%r8),%edx
	imull	%eax, %ecx
	movswl	2(%rdi),%eax
	imull	%edx, %eax
	addl	%eax, %ecx
	movl	%ecx, __op_param1(%rbp)
	movswl	4(%r8),%eax
	movswl	4(%rdi),%ecx
	movswl	6(%r8),%edx
	imull	%eax, %ecx
	movswl	6(%rdi),%eax
	imull	%edx, %eax
	addl	%eax, %ecx
	movl	%ecx, 4(%r8)
	ret
.LFE1199:
	.size	op_pmaddwd_mmx, .-op_pmaddwd_mmx
	.p2align 4,,15
.globl op_movl_mm_T0_mmx
	.type	op_movl_mm_T0_mmx, @function
op_movl_mm_T0_mmx:
.LFB1203:
	leaq	__op_param1(%rbp), %rax
	movl	%ebx, (%rax)
	movl	$0, 4(%rax)
	ret
.LFE1203:
	.size	op_movl_mm_T0_mmx, .-op_movl_mm_T0_mmx
	.p2align 4,,15
.globl op_movl_T0_mm_mmx
	.type	op_movl_T0_mm_mmx, @function
op_movl_T0_mm_mmx:
.LFB1204:
	movl	__op_param1(%rbp), %ebx
	ret
.LFE1204:
	.size	op_movl_T0_mm_mmx, .-op_movl_T0_mm_mmx
	.p2align 4,,15
.globl op_pshufw_mmx
	.type	op_pshufw_mmx, @function
op_pshufw_mmx:
.LFB1205:
	leal	__op_param3(%rip), %eax
	leaq	__op_param2(%rbp), %rsi
	movq	%rax, %rcx
	sarl	$2, %eax
	andl	$3, %ecx
	movq	%rax, %rdx
	sarl	$2, %eax
	movzwl	(%rsi,%rcx,2), %edi
	andl	$3, %edx
	movq	%rax, %rcx
	movzwl	(%rsi,%rdx,2), %edx
	andl	$3, %ecx
	sarl	$2, %eax
	movzwl	(%rsi,%rcx,2), %ecx
	andl	$3, %eax
	movzwl	(%rsi,%rax,2), %eax
	salq	$16, %rdx
	salq	$32, %rcx
	orq	%rdi, %rdx
	salq	$48, %rax
	orq	%rcx, %rdx
	orq	%rax, %rdx
	movq	%rdx, __op_param1(%rbp)
	ret
.LFE1205:
	.size	op_pshufw_mmx, .-op_pshufw_mmx
	.p2align 4,,15
.globl op_pmovmskb_mmx
	.type	op_pmovmskb_mmx, @function
op_pmovmskb_mmx:
.LFB1206:
	leaq	__op_param1(%rbp), %rsi
	xorl	%ebx, %ebx
	movzbl	(%rsi), %edx
	movzbl	1(%rsi), %eax
	movzbl	5(%rsi), %ecx
	shrb	$7, %dl
	shrb	$6, %al
	movzbl	%dl, %edx
	andl	$2, %eax
	shrb	$2, %cl
	orl	%edx, %eax
	movl	%edx, %ebx
	movzbl	2(%rsi), %edx
	andl	$32, %ecx
	shrb	$5, %dl
	andl	$4, %edx
	orl	%eax, %edx
	movzbl	3(%rsi), %eax
	shrb	$4, %al
	andl	$8, %eax
	orl	%edx, %eax
	movzbl	4(%rsi), %edx
	shrb	$3, %dl
	andl	$16, %edx
	orl	%eax, %edx
	movzbl	6(%rsi), %eax
	orl	%edx, %ecx
	movzbl	7(%rsi), %edx
	shrb	%al
	andl	$64, %eax
	andl	$128, %edx
	orl	%ecx, %eax
	movl	%eax, %ebx
	orl	%edx, %ebx
	ret
.LFE1206:
	.size	op_pmovmskb_mmx, .-op_pmovmskb_mmx
	.p2align 4,,15
.globl op_pinsrw_mmx
	.type	op_pinsrw_mmx, @function
op_pinsrw_mmx:
.LFB1207:
	movl	$__op_param2, %eax
	cltq
	movw	%bx, __op_param1(%rbp,%rax,2)
	ret
.LFE1207:
	.size	op_pinsrw_mmx, .-op_pinsrw_mmx
	.p2align 4,,15
.globl op_pextrw_mmx
	.type	op_pextrw_mmx, @function
op_pextrw_mmx:
.LFB1208:
	movl	$__op_param2, %eax
	cltq
	movzwl	__op_param1(%rbp,%rax,2), %ebx
	ret
.LFE1208:
	.size	op_pextrw_mmx, .-op_pextrw_mmx
	.p2align 4,,15
.globl op_punpcklbw_mmx
	.type	op_punpcklbw_mmx, @function
op_punpcklbw_mmx:
.LFB1212:
	movzbl	__op_param2(%rbp), %edx
	xorl	%eax, %eax
	movb	__op_param1(%rbp), %al
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r8
	movabsq	$-4278190081, %rcx
	movb	%dl, %ah
	movzbl	1(%rdi), %edx
	andq	$-16711681, %rax
	salq	$16, %rdx
	orq	%rdx, %rax
	movzbl	1(%r8), %edx
	andq	%rcx, %rax
	movabsq	$-1095216660481, %rcx
	salq	$24, %rdx
	orq	%rdx, %rax
	movzbl	2(%rdi), %edx
	andq	%rcx, %rax
	movabsq	$-280375465082881, %rcx
	salq	$32, %rdx
	orq	%rdx, %rax
	movzbl	2(%r8), %edx
	andq	%rcx, %rax
	movabsq	$-71776119061217281, %rcx
	salq	$40, %rdx
	orq	%rdx, %rax
	movzbl	3(%rdi), %edx
	andq	%rcx, %rax
	movabsq	$72057594037927935, %rcx
	salq	$48, %rdx
	orq	%rdx, %rax
	movzbl	3(%r8), %edx
	andq	%rcx, %rax
	salq	$56, %rdx
	orq	%rdx, %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1212:
	.size	op_punpcklbw_mmx, .-op_punpcklbw_mmx
	.p2align 4,,15
.globl op_punpcklwd_mmx
	.type	op_punpcklwd_mmx, @function
op_punpcklwd_mmx:
.LFB1213:
	movzwl	__op_param2(%rbp), %edx
	movzwl	__op_param1(%rbp), %eax
	movabsq	$-281470681743361, %rcx
	salq	$16, %rdx
	orq	%rdx, %rax
	movzwl	__op_param1+2(%rbp), %edx
	andq	%rcx, %rax
	movabsq	$281474976710655, %rcx
	salq	$32, %rdx
	orq	%rdx, %rax
	movzwl	__op_param2+2(%rbp), %edx
	andq	%rcx, %rax
	salq	$48, %rdx
	orq	%rdx, %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1213:
	.size	op_punpcklwd_mmx, .-op_punpcklwd_mmx
	.p2align 4,,15
.globl op_punpckldq_mmx
	.type	op_punpckldq_mmx, @function
op_punpckldq_mmx:
.LFB1214:
	mov	__op_param2(%rbp), %eax
	movl	%eax, __op_param1+4(%rbp)
	ret
.LFE1214:
	.size	op_punpckldq_mmx, .-op_punpckldq_mmx
	.p2align 4,,15
.globl op_punpckhbw_mmx
	.type	op_punpckhbw_mmx, @function
op_punpckhbw_mmx:
.LFB1215:
	leaq	__op_param2(%rbp), %rdi
	leaq	__op_param1(%rbp), %rsi
	xorl	%eax, %eax
	movabsq	$-4278190081, %rcx
	movzbl	4(%rdi), %edx
	movb	4(%rsi), %al
	movb	%dl, %ah
	movzbl	5(%rsi), %edx
	andq	$-16711681, %rax
	salq	$16, %rdx
	orq	%rdx, %rax
	movzbl	5(%rdi), %edx
	andq	%rcx, %rax
	movabsq	$-1095216660481, %rcx
	salq	$24, %rdx
	orq	%rdx, %rax
	movzbl	6(%rsi), %edx
	andq	%rcx, %rax
	movabsq	$-280375465082881, %rcx
	salq	$32, %rdx
	orq	%rdx, %rax
	movzbl	6(%rdi), %edx
	andq	%rcx, %rax
	movabsq	$-71776119061217281, %rcx
	salq	$40, %rdx
	orq	%rdx, %rax
	movzbl	7(%rsi), %edx
	andq	%rcx, %rax
	movabsq	$72057594037927935, %rcx
	salq	$48, %rdx
	orq	%rdx, %rax
	movzbl	7(%rdi), %edx
	andq	%rcx, %rax
	salq	$56, %rdx
	orq	%rdx, %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1215:
	.size	op_punpckhbw_mmx, .-op_punpckhbw_mmx
	.p2align 4,,15
.globl op_punpckhwd_mmx
	.type	op_punpckhwd_mmx, @function
op_punpckhwd_mmx:
.LFB1216:
	leaq	__op_param2(%rbp), %rdi
	leaq	__op_param1(%rbp), %rcx
	movzwl	4(%rdi), %edx
	movzwl	4(%rcx), %eax
	movzwl	6(%rcx), %ecx
	salq	$16, %rdx
	orq	%rdx, %rax
	movzwl	6(%rdi), %edx
	salq	$32, %rcx
	orq	%rcx, %rax
	salq	$48, %rdx
	orq	%rdx, %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1216:
	.size	op_punpckhwd_mmx, .-op_punpckhwd_mmx
	.p2align 4,,15
.globl op_punpckhdq_mmx
	.type	op_punpckhdq_mmx, @function
op_punpckhdq_mmx:
.LFB1217:
	mov	__op_param2+4(%rbp), %edx
	mov	__op_param1+4(%rbp), %eax
	salq	$32, %rdx
	orq	%rdx, %rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1217:
	.size	op_punpckhdq_mmx, .-op_punpckhdq_mmx
	.p2align 4,,15
.globl op_psrlw_xmm
	.type	op_psrlw_xmm, @function
op_psrlw_xmm:
.LFB1218:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1325
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rdx)
	jmp	.L1327
	.p2align 4,,7
.L1325:
	movzbl	__op_param2(%rbp), %ecx
	movzwl	__op_param1(%rbp), %eax
	sarl	%cl, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 2(%rdx)
	movzwl	4(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 4(%rdx)
	movzwl	6(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 6(%rdx)
	movzwl	8(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 8(%rdx)
	movzwl	10(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 10(%rdx)
	movzwl	12(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 12(%rdx)
	movzwl	14(%rdx), %eax
	sarl	%cl, %eax
	movw	%ax, 14(%rdx)
.L1327:
	ret
.LFE1218:
	.size	op_psrlw_xmm, .-op_psrlw_xmm
	.p2align 4,,15
.globl op_psraw_xmm
	.type	op_psraw_xmm, @function
op_psraw_xmm:
.LFB1219:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	movl	$15, %ecx
	ja	.L1332
	movzbl	__op_param2(%rbp), %ecx
.L1332:
	movswl	(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, (%rdx)
	movswl	2(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 2(%rdx)
	movswl	4(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 4(%rdx)
	movswl	6(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 6(%rdx)
	movswl	8(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 8(%rdx)
	movswl	10(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 10(%rdx)
	movswl	12(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 12(%rdx)
	movswl	14(%rdx),%eax
	sarl	%cl, %eax
	movw	%ax, 14(%rdx)
	ret
.LFE1219:
	.size	op_psraw_xmm, .-op_psraw_xmm
	.p2align 4,,15
.globl op_psllw_xmm
	.type	op_psllw_xmm, @function
op_psllw_xmm:
.LFB1220:
	cmpq	$15, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1335
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rdx)
	jmp	.L1337
	.p2align 4,,7
.L1335:
	movzbl	__op_param2(%rbp), %ecx
	movzwl	__op_param1(%rbp), %eax
	sall	%cl, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 2(%rdx)
	movzwl	4(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 4(%rdx)
	movzwl	6(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 6(%rdx)
	movzwl	8(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 8(%rdx)
	movzwl	10(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 10(%rdx)
	movzwl	12(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 12(%rdx)
	movzwl	14(%rdx), %eax
	sall	%cl, %eax
	movw	%ax, 14(%rdx)
.L1337:
	ret
.LFE1220:
	.size	op_psllw_xmm, .-op_psllw_xmm
	.p2align 4,,15
.globl op_psrld_xmm
	.type	op_psrld_xmm, @function
op_psrld_xmm:
.LFB1221:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rax
	jbe	.L1340
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rax)
	jmp	.L1342
	.p2align 4,,7
.L1340:
	movzbl	__op_param2(%rbp), %ecx
	shrl	%cl, (%rax)
	shrl	%cl, 4(%rax)
	shrl	%cl, 8(%rax)
	shrl	%cl, 12(%rax)
.L1342:
	ret
.LFE1221:
	.size	op_psrld_xmm, .-op_psrld_xmm
	.p2align 4,,15
.globl op_psrad_xmm
	.type	op_psrad_xmm, @function
op_psrad_xmm:
.LFB1222:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rax
	movl	$31, %ecx
	ja	.L1347
	movzbl	__op_param2(%rbp), %ecx
.L1347:
	sarl	%cl, (%rax)
	sarl	%cl, 4(%rax)
	sarl	%cl, 8(%rax)
	sarl	%cl, 12(%rax)
	ret
.LFE1222:
	.size	op_psrad_xmm, .-op_psrad_xmm
	.p2align 4,,15
.globl op_pslld_xmm
	.type	op_pslld_xmm, @function
op_pslld_xmm:
.LFB1223:
	cmpq	$31, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rax
	jbe	.L1350
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rax)
	jmp	.L1352
	.p2align 4,,7
.L1350:
	movzbl	__op_param2(%rbp), %ecx
	sall	%cl, (%rax)
	sall	%cl, 4(%rax)
	sall	%cl, 8(%rax)
	sall	%cl, 12(%rax)
.L1352:
	ret
.LFE1223:
	.size	op_pslld_xmm, .-op_pslld_xmm
	.p2align 4,,15
.globl op_psrlq_xmm
	.type	op_psrlq_xmm, @function
op_psrlq_xmm:
.LFB1224:
	cmpq	$63, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1355
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rdx)
	jmp	.L1357
	.p2align 4,,7
.L1355:
	movzbl	__op_param2(%rbp), %ecx
	shrq	%cl, (%rdx)
	shrq	%cl, 8(%rdx)
.L1357:
	ret
.LFE1224:
	.size	op_psrlq_xmm, .-op_psrlq_xmm
	.p2align 4,,15
.globl op_psllq_xmm
	.type	op_psllq_xmm, @function
op_psllq_xmm:
.LFB1225:
	cmpq	$63, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rdx
	jbe	.L1360
	movq	$0, __op_param1(%rbp)
	movq	$0, 8(%rdx)
	jmp	.L1362
	.p2align 4,,7
.L1360:
	movzbl	__op_param2(%rbp), %ecx
	salq	%cl, (%rdx)
	salq	%cl, 8(%rdx)
.L1362:
	ret
.LFE1225:
	.size	op_psllq_xmm, .-op_psllq_xmm
	.p2align 4,,15
.globl op_psrldq_xmm
	.type	op_psrldq_xmm, @function
op_psrldq_xmm:
.LFB1226:
	movl	__op_param2(%rbp), %eax
	movl	$16, %edx
	leaq	__op_param1(%rbp), %r8
	cmpl	$16, %eax
	cmovg	%edx, %eax
	subl	%eax, %edx
	testl	%edx, %edx
	jle	.L1365
	cltq
	movq	%r8, %rcx
	xorl	%edi, %edi
	leaq	(%r8,%rax), %rsi
	.p2align 4,,7
.L1367:
	movzbl	(%rsi), %eax
	incl	%edi
	incq	%rsi
	movb	%al, (%rcx)
	incq	%rcx
	cmpl	%edx, %edi
	jne	.L1367
.L1365:
	cmpl	$15, %edx
	jg	.L1368
	movslq	%edx,%rax
	leaq	(%r8,%rax), %rax
	.p2align 4,,7
.L1370:
	incl	%edx
	movb	$0, (%rax)
	incq	%rax
	cmpl	$16, %edx
	jne	.L1370
.L1368:
	ret
.LFE1226:
	.size	op_psrldq_xmm, .-op_psrldq_xmm
	.p2align 4,,15
.globl op_pslldq_xmm
	.type	op_pslldq_xmm, @function
op_pslldq_xmm:
.LFB1227:
	movl	__op_param2(%rbp), %eax
	movl	$16, %r10d
	leaq	__op_param1(%rbp), %r9
	cmpl	$16, %eax
	cmovle	%eax, %r10d
	cmpl	$15, %r10d
	jg	.L1376
	movl	%r10d, %edx
	movl	$15, %eax
	xorl	%esi, %esi
	subl	%r10d, %eax
	negl	%edx
	movslq	%edx,%rdx
	mov	%eax, %edi
	movslq	%eax,%r8
	leaq	(%r9,%rdx), %rcx
	notq	%rdi
	negq	%rdx
	.p2align 4,,7
.L1378:
	leaq	(%r8,%rcx), %rax
	decq	%rcx
	movzbl	(%rdx,%rax), %eax
	movb	%al, 15(%rsi,%r9)
	decq	%rsi
	cmpq	%rdi, %rsi
	jne	.L1378
.L1376:
	testl	%r10d, %r10d
	jle	.L1379
	movq	%r9, %rax
	xorl	%edx, %edx
	.p2align 4,,7
.L1381:
	incl	%edx
	movb	$0, (%rax)
	incq	%rax
	cmpl	%edx, %r10d
	jne	.L1381
.L1379:
	ret
.LFE1227:
	.size	op_pslldq_xmm, .-op_pslldq_xmm
	.p2align 4,,15
.globl op_paddb_xmm
	.type	op_paddb_xmm, @function
op_paddb_xmm:
.LFB1228:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movzbl	__op_param2(%rbp), %edx
	addb	%dl, (%rax)
	movzbl	1(%rcx), %edx
	addb	%dl, 1(%rax)
	movzbl	2(%rcx), %edx
	addb	%dl, 2(%rax)
	movzbl	3(%rcx), %edx
	addb	%dl, 3(%rax)
	movzbl	4(%rcx), %edx
	addb	%dl, 4(%rax)
	movzbl	5(%rcx), %edx
	addb	%dl, 5(%rax)
	movzbl	6(%rcx), %edx
	addb	%dl, 6(%rax)
	movzbl	7(%rcx), %edx
	addb	%dl, 7(%rax)
	movzbl	8(%rcx), %edx
	addb	%dl, 8(%rax)
	movzbl	9(%rcx), %edx
	addb	%dl, 9(%rax)
	movzbl	10(%rcx), %edx
	addb	%dl, 10(%rax)
	movzbl	11(%rcx), %edx
	addb	%dl, 11(%rax)
	movzbl	12(%rcx), %edx
	addb	%dl, 12(%rax)
	movzbl	13(%rcx), %edx
	addb	%dl, 13(%rax)
	movzbl	14(%rcx), %edx
	addb	%dl, 14(%rax)
	movzbl	15(%rcx), %edx
	addb	%dl, 15(%rax)
	ret
.LFE1228:
	.size	op_paddb_xmm, .-op_paddb_xmm
	.p2align 4,,15
.globl op_paddw_xmm
	.type	op_paddw_xmm, @function
op_paddw_xmm:
.LFB1229:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	addw	%dx, (%rax)
	movzwl	2(%rcx), %edx
	addw	%dx, 2(%rax)
	movl	4(%rcx), %edx
	addw	%dx, 4(%rax)
	movzwl	6(%rcx), %edx
	addw	%dx, 6(%rax)
	movl	8(%rcx), %edx
	addw	%dx, 8(%rax)
	movzwl	10(%rcx), %edx
	addw	%dx, 10(%rax)
	movl	12(%rcx), %edx
	addw	%dx, 12(%rax)
	movzwl	14(%rcx), %edx
	addw	%dx, 14(%rax)
	ret
.LFE1229:
	.size	op_paddw_xmm, .-op_paddw_xmm
	.p2align 4,,15
.globl op_paddl_xmm
	.type	op_paddl_xmm, @function
op_paddl_xmm:
.LFB1230:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	addl	%edx, (%rax)
	movl	4(%rcx), %edx
	addl	%edx, 4(%rax)
	movl	8(%rcx), %edx
	addl	%edx, 8(%rax)
	movl	12(%rcx), %edx
	addl	%edx, 12(%rax)
	ret
.LFE1230:
	.size	op_paddl_xmm, .-op_paddl_xmm
	.p2align 4,,15
.globl op_paddq_xmm
	.type	op_paddq_xmm, @function
op_paddq_xmm:
.LFB1231:
	leaq	__op_param1(%rbp), %rax
	movq	__op_param2(%rbp), %rcx
	addq	%rcx, (%rax)
	movq	__op_param2+8(%rbp), %rdx
	addq	%rdx, 8(%rax)
	ret
.LFE1231:
	.size	op_paddq_xmm, .-op_paddq_xmm
	.p2align 4,,15
.globl op_psubb_xmm
	.type	op_psubb_xmm, @function
op_psubb_xmm:
.LFB1232:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movzbl	__op_param2(%rbp), %edx
	subb	%dl, (%rax)
	movzbl	1(%rcx), %edx
	subb	%dl, 1(%rax)
	movzbl	2(%rcx), %edx
	subb	%dl, 2(%rax)
	movzbl	3(%rcx), %edx
	subb	%dl, 3(%rax)
	movzbl	4(%rcx), %edx
	subb	%dl, 4(%rax)
	movzbl	5(%rcx), %edx
	subb	%dl, 5(%rax)
	movzbl	6(%rcx), %edx
	subb	%dl, 6(%rax)
	movzbl	7(%rcx), %edx
	subb	%dl, 7(%rax)
	movzbl	8(%rcx), %edx
	subb	%dl, 8(%rax)
	movzbl	9(%rcx), %edx
	subb	%dl, 9(%rax)
	movzbl	10(%rcx), %edx
	subb	%dl, 10(%rax)
	movzbl	11(%rcx), %edx
	subb	%dl, 11(%rax)
	movzbl	12(%rcx), %edx
	subb	%dl, 12(%rax)
	movzbl	13(%rcx), %edx
	subb	%dl, 13(%rax)
	movzbl	14(%rcx), %edx
	subb	%dl, 14(%rax)
	movzbl	15(%rcx), %edx
	subb	%dl, 15(%rax)
	ret
.LFE1232:
	.size	op_psubb_xmm, .-op_psubb_xmm
	.p2align 4,,15
.globl op_psubw_xmm
	.type	op_psubw_xmm, @function
op_psubw_xmm:
.LFB1233:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	subw	%dx, (%rax)
	movzwl	2(%rcx), %edx
	subw	%dx, 2(%rax)
	movl	4(%rcx), %edx
	subw	%dx, 4(%rax)
	movzwl	6(%rcx), %edx
	subw	%dx, 6(%rax)
	movl	8(%rcx), %edx
	subw	%dx, 8(%rax)
	movzwl	10(%rcx), %edx
	subw	%dx, 10(%rax)
	movl	12(%rcx), %edx
	subw	%dx, 12(%rax)
	movzwl	14(%rcx), %edx
	subw	%dx, 14(%rax)
	ret
.LFE1233:
	.size	op_psubw_xmm, .-op_psubw_xmm
	.p2align 4,,15
.globl op_psubl_xmm
	.type	op_psubl_xmm, @function
op_psubl_xmm:
.LFB1234:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %rcx
	movl	__op_param2(%rbp), %edx
	subl	%edx, (%rax)
	movl	4(%rcx), %edx
	subl	%edx, 4(%rax)
	movl	8(%rcx), %edx
	subl	%edx, 8(%rax)
	movl	12(%rcx), %edx
	subl	%edx, 12(%rax)
	ret
.LFE1234:
	.size	op_psubl_xmm, .-op_psubl_xmm
	.p2align 4,,15
.globl op_psubq_xmm
	.type	op_psubq_xmm, @function
op_psubq_xmm:
.LFB1235:
	leaq	__op_param1(%rbp), %rax
	movq	__op_param2(%rbp), %rcx
	subq	%rcx, (%rax)
	movq	__op_param2+8(%rbp), %rdx
	subq	%rdx, 8(%rax)
	ret
.LFE1235:
	.size	op_psubq_xmm, .-op_psubq_xmm
	.p2align 4,,15
.globl op_pminub_xmm
	.type	op_pminub_xmm, @function
op_pminub_xmm:
.LFB1244:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rcx
	cmpb	%al, %dil
	cmovbe	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %edx
	movzbl	1(%rsi), %eax
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 7(%rsi)
	movzbl	8(%rsi), %eax
	movzbl	8(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 8(%rsi)
	movzbl	9(%rsi), %eax
	movzbl	9(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 9(%rsi)
	movzbl	10(%rsi), %eax
	movzbl	10(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 10(%rsi)
	movzbl	11(%rsi), %eax
	movzbl	11(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 11(%rsi)
	movzbl	12(%rcx), %edx
	movzbl	12(%rsi), %eax
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 12(%rsi)
	movzbl	13(%rsi), %eax
	movzbl	13(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 13(%rsi)
	movzbl	14(%rsi), %eax
	movzbl	14(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 14(%rsi)
	movzbl	15(%rsi), %eax
	movzbl	15(%rcx), %edx
	cmpb	%al, %dl
	cmovbe	%edx, %eax
	movb	%al, 15(%rsi)
	ret
.LFE1244:
	.size	op_pminub_xmm, .-op_pminub_xmm
	.p2align 4,,15
.globl op_pmaxub_xmm
	.type	op_pmaxub_xmm, @function
op_pmaxub_xmm:
.LFB1245:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rcx
	cmpb	%al, %dil
	cmovae	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %edx
	movzbl	1(%rsi), %eax
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 7(%rsi)
	movzbl	8(%rsi), %eax
	movzbl	8(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 8(%rsi)
	movzbl	9(%rsi), %eax
	movzbl	9(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 9(%rsi)
	movzbl	10(%rsi), %eax
	movzbl	10(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 10(%rsi)
	movzbl	11(%rsi), %eax
	movzbl	11(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 11(%rsi)
	movzbl	12(%rcx), %edx
	movzbl	12(%rsi), %eax
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 12(%rsi)
	movzbl	13(%rsi), %eax
	movzbl	13(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 13(%rsi)
	movzbl	14(%rsi), %eax
	movzbl	14(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 14(%rsi)
	movzbl	15(%rsi), %eax
	movzbl	15(%rcx), %edx
	cmpb	%al, %dl
	cmovae	%edx, %eax
	movb	%al, 15(%rsi)
	ret
.LFE1245:
	.size	op_pmaxub_xmm, .-op_pmaxub_xmm
	.p2align 4,,15
.globl op_pminsw_xmm
	.type	op_pminsw_xmm, @function
op_pminsw_xmm:
.LFB1246:
	movl	__op_param1(%rbp), %eax
	cmpw	%ax, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	cmovle	__op_param2(%rbp), %ax
	leaq	__op_param2(%rbp), %rcx
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	cmpw	%ax, 2(%rcx)
	cmovle	2(%rcx), %ax
	movw	%ax, 2(%rsi)
	movl	4(%rsi), %eax
	cmpw	%ax, 4(%rcx)
	cmovle	4(%rcx), %ax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	cmpw	%ax, 6(%rcx)
	cmovle	6(%rcx), %ax
	movw	%ax, 6(%rsi)
	movl	8(%rsi), %eax
	cmpw	%ax, 8(%rcx)
	cmovle	8(%rcx), %ax
	movw	%ax, 8(%rsi)
	movzwl	10(%rsi), %eax
	cmpw	%ax, 10(%rcx)
	cmovle	10(%rcx), %ax
	movw	%ax, 10(%rsi)
	movl	12(%rsi), %eax
	cmpw	%ax, 12(%rcx)
	cmovle	12(%rcx), %ax
	movw	%ax, 12(%rsi)
	movzwl	14(%rsi), %eax
	cmpw	%ax, 14(%rcx)
	cmovle	14(%rcx), %ax
	movw	%ax, 14(%rsi)
	ret
.LFE1246:
	.size	op_pminsw_xmm, .-op_pminsw_xmm
	.p2align 4,,15
.globl op_pmaxsw_xmm
	.type	op_pmaxsw_xmm, @function
op_pmaxsw_xmm:
.LFB1247:
	movl	__op_param1(%rbp), %eax
	cmpw	%ax, __op_param2(%rbp)
	leaq	__op_param1(%rbp), %rsi
	cmovge	__op_param2(%rbp), %ax
	leaq	__op_param2(%rbp), %rcx
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rsi), %eax
	cmpw	%ax, 2(%rcx)
	cmovge	2(%rcx), %ax
	movw	%ax, 2(%rsi)
	movl	4(%rsi), %eax
	cmpw	%ax, 4(%rcx)
	cmovge	4(%rcx), %ax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	cmpw	%ax, 6(%rcx)
	cmovge	6(%rcx), %ax
	movw	%ax, 6(%rsi)
	movl	8(%rsi), %eax
	cmpw	%ax, 8(%rcx)
	cmovge	8(%rcx), %ax
	movw	%ax, 8(%rsi)
	movzwl	10(%rsi), %eax
	cmpw	%ax, 10(%rcx)
	cmovge	10(%rcx), %ax
	movw	%ax, 10(%rsi)
	movl	12(%rsi), %eax
	cmpw	%ax, 12(%rcx)
	cmovge	12(%rcx), %ax
	movw	%ax, 12(%rsi)
	movzwl	14(%rsi), %eax
	cmpw	%ax, 14(%rcx)
	cmovge	14(%rcx), %ax
	movw	%ax, 14(%rsi)
	ret
.LFE1247:
	.size	op_pmaxsw_xmm, .-op_pmaxsw_xmm
	.p2align 4,,15
.globl op_pand_xmm
	.type	op_pand_xmm, @function
op_pand_xmm:
.LFB1248:
	leaq	__op_param1(%rbp), %rax
	movq	__op_param2(%rbp), %rcx
	andq	%rcx, (%rax)
	movq	__op_param2+8(%rbp), %rdx
	andq	%rdx, 8(%rax)
	ret
.LFE1248:
	.size	op_pand_xmm, .-op_pand_xmm
	.p2align 4,,15
.globl op_pandn_xmm
	.type	op_pandn_xmm, @function
op_pandn_xmm:
.LFB1249:
	movq	__op_param1(%rbp), %rax
	leaq	__op_param1(%rbp), %rcx
	notq	%rax
	andq	__op_param2(%rbp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	8(%rcx), %rax
	notq	%rax
	andq	__op_param2+8(%rbp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1249:
	.size	op_pandn_xmm, .-op_pandn_xmm
	.p2align 4,,15
.globl op_por_xmm
	.type	op_por_xmm, @function
op_por_xmm:
.LFB1250:
	leaq	__op_param1(%rbp), %rax
	movq	__op_param2(%rbp), %rcx
	orq	%rcx, (%rax)
	movq	__op_param2+8(%rbp), %rdx
	orq	%rdx, 8(%rax)
	ret
.LFE1250:
	.size	op_por_xmm, .-op_por_xmm
	.p2align 4,,15
.globl op_pxor_xmm
	.type	op_pxor_xmm, @function
op_pxor_xmm:
.LFB1251:
	leaq	__op_param1(%rbp), %rax
	movq	__op_param2(%rbp), %rcx
	xorq	%rcx, (%rax)
	movq	__op_param2+8(%rbp), %rdx
	xorq	%rdx, 8(%rax)
	ret
.LFE1251:
	.size	op_pxor_xmm, .-op_pxor_xmm
	.p2align 4,,15
.globl op_pcmpgtb_xmm
	.type	op_pcmpgtb_xmm, @function
op_pcmpgtb_xmm:
.LFB1252:
	movzbl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	cmpb	__op_param2(%rbp), %al
	setle	%al
	decl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %eax
	cmpb	1(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	cmpb	2(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	cmpb	3(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	cmpb	4(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	cmpb	5(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	cmpb	6(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	cmpb	7(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 7(%rcx)
	movzbl	8(%rcx), %eax
	cmpb	8(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 8(%rcx)
	movzbl	9(%rcx), %eax
	cmpb	9(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 9(%rcx)
	movzbl	10(%rcx), %eax
	cmpb	10(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 10(%rcx)
	movzbl	11(%rcx), %eax
	cmpb	11(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 11(%rcx)
	movzbl	12(%rcx), %eax
	cmpb	12(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 12(%rcx)
	movzbl	13(%rcx), %eax
	cmpb	13(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 13(%rcx)
	movzbl	14(%rcx), %eax
	cmpb	14(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 14(%rcx)
	movzbl	15(%rcx), %eax
	cmpb	15(%rsi), %al
	setle	%al
	decl	%eax
	movb	%al, 15(%rcx)
	ret
.LFE1252:
	.size	op_pcmpgtb_xmm, .-op_pcmpgtb_xmm
	.p2align 4,,15
.globl op_pcmpgtw_xmm
	.type	op_pcmpgtw_xmm, @function
op_pcmpgtw_xmm:
.LFB1253:
	movl	__op_param1(%rbp), %eax
	cmpw	__op_param2(%rbp), %ax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	cmpw	2(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	cmpw	4(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	cmpw	6(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%rcx)
	movl	8(%rcx), %eax
	cmpw	8(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 8(%rcx)
	movzwl	10(%rcx), %eax
	cmpw	10(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 10(%rcx)
	movl	12(%rcx), %eax
	cmpw	12(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 12(%rcx)
	movzwl	14(%rcx), %eax
	cmpw	14(%rsi), %ax
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 14(%rcx)
	ret
.LFE1253:
	.size	op_pcmpgtw_xmm, .-op_pcmpgtw_xmm
	.p2align 4,,15
.globl op_pcmpgtl_xmm
	.type	op_pcmpgtl_xmm, @function
op_pcmpgtl_xmm:
.LFB1254:
	movl	__op_param1(%rbp), %eax
	cmpl	__op_param2(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movl	4(%rcx), %eax
	cmpl	4(%rsi), %eax
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rcx)
	movl	8(%rcx), %eax
	cmpl	8(%rsi), %eax
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%rcx)
	movl	12(%rcx), %eax
	cmpl	12(%rsi), %eax
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1254:
	.size	op_pcmpgtl_xmm, .-op_pcmpgtl_xmm
	.p2align 4,,15
.globl op_pcmpeqb_xmm
	.type	op_pcmpeqb_xmm, @function
op_pcmpeqb_xmm:
.LFB1255:
	movzbl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	cmpb	__op_param2(%rbp), %al
	setne	%al
	decl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rcx), %eax
	cmpb	1(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	cmpb	2(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	cmpb	3(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	cmpb	4(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	cmpb	5(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	cmpb	6(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	cmpb	7(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 7(%rcx)
	movzbl	8(%rcx), %eax
	cmpb	8(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 8(%rcx)
	movzbl	9(%rcx), %eax
	cmpb	9(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 9(%rcx)
	movzbl	10(%rcx), %eax
	cmpb	10(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 10(%rcx)
	movzbl	11(%rcx), %eax
	cmpb	11(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 11(%rcx)
	movzbl	12(%rcx), %eax
	cmpb	12(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 12(%rcx)
	movzbl	13(%rcx), %eax
	cmpb	13(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 13(%rcx)
	movzbl	14(%rcx), %eax
	cmpb	14(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 14(%rcx)
	movzbl	15(%rcx), %eax
	cmpb	15(%rsi), %al
	setne	%al
	decl	%eax
	movb	%al, 15(%rcx)
	ret
.LFE1255:
	.size	op_pcmpeqb_xmm, .-op_pcmpeqb_xmm
	.p2align 4,,15
.globl op_pcmpeqw_xmm
	.type	op_pcmpeqw_xmm, @function
op_pcmpeqw_xmm:
.LFB1256:
	movl	__op_param1(%rbp), %eax
	cmpw	__op_param2(%rbp), %ax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	cmpw	2(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	cmpw	4(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	cmpw	6(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%rcx)
	movl	8(%rcx), %eax
	cmpw	8(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 8(%rcx)
	movzwl	10(%rcx), %eax
	cmpw	10(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 10(%rcx)
	movl	12(%rcx), %eax
	cmpw	12(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 12(%rcx)
	movzwl	14(%rcx), %eax
	cmpw	14(%rsi), %ax
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 14(%rcx)
	ret
.LFE1256:
	.size	op_pcmpeqw_xmm, .-op_pcmpeqw_xmm
	.p2align 4,,15
.globl op_pcmpeql_xmm
	.type	op_pcmpeql_xmm, @function
op_pcmpeql_xmm:
.LFB1257:
	movl	__op_param1(%rbp), %eax
	cmpl	__op_param2(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movl	4(%rcx), %eax
	cmpl	4(%rsi), %eax
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rcx)
	movl	8(%rcx), %eax
	cmpl	8(%rsi), %eax
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%rcx)
	movl	12(%rcx), %eax
	cmpl	12(%rsi), %eax
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1257:
	.size	op_pcmpeql_xmm, .-op_pcmpeql_xmm
	.p2align 4,,15
.globl op_pmullw_xmm
	.type	op_pmullw_xmm, @function
op_pmullw_xmm:
.LFB1258:
	movl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	imull	__op_param2(%rbp), %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rcx), %eax
	imulw	2(%rsi), %ax
	movw	%ax, 2(%rcx)
	movl	4(%rcx), %eax
	imull	4(%rsi), %eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	imulw	6(%rsi), %ax
	movw	%ax, 6(%rcx)
	movl	8(%rcx), %eax
	imull	8(%rsi), %eax
	movw	%ax, 8(%rcx)
	movzwl	10(%rcx), %eax
	imulw	10(%rsi), %ax
	movw	%ax, 10(%rcx)
	movl	12(%rcx), %eax
	imull	12(%rsi), %eax
	movw	%ax, 12(%rcx)
	movzwl	14(%rcx), %eax
	imulw	14(%rsi), %ax
	movw	%ax, 14(%rcx)
	ret
.LFE1258:
	.size	op_pmullw_xmm, .-op_pmullw_xmm
	.p2align 4,,15
.globl op_pmulhuw_xmm
	.type	op_pmulhuw_xmm, @function
op_pmulhuw_xmm:
.LFB1259:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdi), %edx
	movzwl	2(%rsi), %eax
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	movzwl	4(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	movzwl	6(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%rsi)
	movzwl	8(%rsi), %eax
	movzwl	8(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 8(%rsi)
	movzwl	10(%rsi), %eax
	movzwl	10(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 10(%rsi)
	movzwl	12(%rsi), %eax
	movzwl	12(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 12(%rsi)
	movzwl	14(%rsi), %eax
	movzwl	14(%rdi), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 14(%rsi)
	ret
.LFE1259:
	.size	op_pmulhuw_xmm, .-op_pmulhuw_xmm
	.p2align 4,,15
.globl op_pmulhw_xmm
	.type	op_pmulhw_xmm, @function
op_pmulhw_xmm:
.LFB1260:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%rdi),%edx
	movswl	2(%rsi),%eax
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%rsi)
	movswl	4(%rsi),%eax
	movswl	4(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%rsi)
	movswl	6(%rsi),%eax
	movswl	6(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%rsi)
	movswl	8(%rsi),%eax
	movswl	8(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 8(%rsi)
	movswl	10(%rsi),%eax
	movswl	10(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 10(%rsi)
	movswl	12(%rsi),%eax
	movswl	12(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 12(%rsi)
	movswl	14(%rsi),%eax
	movswl	14(%rdi),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 14(%rsi)
	ret
.LFE1260:
	.size	op_pmulhw_xmm, .-op_pmulhw_xmm
	.p2align 4,,15
.globl op_pavgb_xmm
	.type	op_pavgb_xmm, @function
op_pavgb_xmm:
.LFB1261:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %esi
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdi
	leal	1(%rax,%rsi), %eax
	sarl	%eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%rdi), %edx
	movzbl	1(%rcx), %eax
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 1(%rcx)
	movzbl	2(%rcx), %eax
	movzbl	2(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 2(%rcx)
	movzbl	3(%rcx), %eax
	movzbl	3(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 3(%rcx)
	movzbl	4(%rcx), %eax
	movzbl	4(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 4(%rcx)
	movzbl	5(%rcx), %eax
	movzbl	5(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 5(%rcx)
	movzbl	6(%rcx), %eax
	movzbl	6(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 6(%rcx)
	movzbl	7(%rcx), %eax
	movzbl	7(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 7(%rcx)
	movzbl	8(%rcx), %eax
	movzbl	8(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 8(%rcx)
	movzbl	9(%rcx), %eax
	movzbl	9(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 9(%rcx)
	movzbl	10(%rcx), %eax
	movzbl	10(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 10(%rcx)
	movzbl	11(%rcx), %eax
	movzbl	11(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 11(%rcx)
	movzbl	12(%rdi), %edx
	movzbl	12(%rcx), %eax
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 12(%rcx)
	movzbl	13(%rcx), %eax
	movzbl	13(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 13(%rcx)
	movzbl	14(%rcx), %eax
	movzbl	14(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 14(%rcx)
	movzbl	15(%rcx), %eax
	movzbl	15(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movb	%al, 15(%rcx)
	ret
.LFE1261:
	.size	op_pavgb_xmm, .-op_pavgb_xmm
	.p2align 4,,15
.globl op_pavgw_xmm
	.type	op_pavgw_xmm, @function
op_pavgw_xmm:
.LFB1262:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %esi
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdi
	leal	1(%rax,%rsi), %eax
	sarl	%eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%rdi), %edx
	movzwl	2(%rcx), %eax
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 2(%rcx)
	movzwl	4(%rcx), %eax
	movzwl	4(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 4(%rcx)
	movzwl	6(%rcx), %eax
	movzwl	6(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 6(%rcx)
	movzwl	8(%rcx), %eax
	movzwl	8(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 8(%rcx)
	movzwl	10(%rcx), %eax
	movzwl	10(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 10(%rcx)
	movzwl	12(%rcx), %eax
	movzwl	12(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 12(%rcx)
	movzwl	14(%rcx), %eax
	movzwl	14(%rdi), %edx
	leal	1(%rax,%rdx), %eax
	sarl	%eax
	movw	%ax, 14(%rcx)
	ret
.LFE1262:
	.size	op_pavgw_xmm, .-op_pavgw_xmm
	.p2align 4,,15
.globl op_pmuludq_xmm
	.type	op_pmuludq_xmm, @function
op_pmuludq_xmm:
.LFB1263:
	mov	__op_param1(%rbp), %ecx
	mov	__op_param2(%rbp), %eax
	leaq	__op_param1(%rbp), %rsi
	imulq	%rcx, %rax
	movq	%rax, __op_param1(%rbp)
	mov	__op_param2+8(%rbp), %eax
	mov	8(%rsi), %edx
	imulq	%rdx, %rax
	movq	%rax, 8(%rsi)
	ret
.LFE1263:
	.size	op_pmuludq_xmm, .-op_pmuludq_xmm
	.p2align 4,,15
.globl op_pmaddwd_xmm
	.type	op_pmaddwd_xmm, @function
op_pmaddwd_xmm:
.LFB1264:
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r8
	xorl	%esi, %esi
	.p2align 4,,7
.L1610:
	movswl	(%rdi,%rsi,4),%eax
	movswl	(%r8,%rsi,4),%ecx
	movswl	2(%rdi,%rsi,4),%edx
	imull	%eax, %ecx
	movswl	2(%r8,%rsi,4),%eax
	imull	%edx, %eax
	addl	%eax, %ecx
	movl	%ecx, (%rdi,%rsi,4)
	incq	%rsi
	cmpq	$4, %rsi
	jne	.L1610
	ret
.LFE1264:
	.size	op_pmaddwd_xmm, .-op_pmaddwd_xmm
	.p2align 4,,15
.globl op_movl_mm_T0_xmm
	.type	op_movl_mm_T0_xmm, @function
op_movl_mm_T0_xmm:
.LFB1267:
	leaq	__op_param1(%rbp), %rax
	movl	%ebx, (%rax)
	movl	$0, 4(%rax)
	movq	$0, 8(%rax)
	ret
.LFE1267:
	.size	op_movl_mm_T0_xmm, .-op_movl_mm_T0_xmm
	.p2align 4,,15
.globl op_movl_T0_mm_xmm
	.type	op_movl_T0_mm_xmm, @function
op_movl_T0_mm_xmm:
.LFB1268:
	movl	__op_param1(%rbp), %ebx
	ret
.LFE1268:
	.size	op_movl_T0_mm_xmm, .-op_movl_T0_mm_xmm
	.p2align 4,,15
.globl op_shufps
	.type	op_shufps, @function
op_shufps:
.LFB1269:
	leal	__op_param3(%rip), %eax
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movl	(%rsi,%rdx,4), %edx
	movl	%edx, -24(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movl	(%rsi,%rdx,4), %edx
	movl	%edx, -20(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	andl	$3, %eax
	movl	(%rdi,%rdx,4), %edx
	movl	%edx, -16(%rsp)
	movl	(%rdi,%rax,4), %eax
	movl	%eax, -12(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rsi)
	ret
.LFE1269:
	.size	op_shufps, .-op_shufps
	.p2align 4,,15
.globl op_shufpd
	.type	op_shufpd, @function
op_shufpd:
.LFB1270:
	leal	__op_param3(%rip), %eax
	leaq	__op_param1(%rbp), %rsi
	movq	%rax, %rdx
	sarl	%eax
	andl	$1, %edx
	andl	$1, %eax
	movq	(%rsi,%rdx,8), %rdx
	movq	__op_param2(%rbp,%rax,8), %rax
	movq	%rdx, __op_param1(%rbp)
	movq	%rax, 8(%rsi)
	ret
.LFE1270:
	.size	op_shufpd, .-op_shufpd
	.p2align 4,,15
.globl op_pshufd_xmm
	.type	op_pshufd_xmm, @function
op_pshufd_xmm:
.LFB1271:
	leal	__op_param3(%rip), %eax
	leaq	__op_param2(%rbp), %rcx
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movl	(%rcx,%rdx,4), %edx
	movl	%edx, -24(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movl	(%rcx,%rdx,4), %edx
	movl	%edx, -20(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	andl	$3, %eax
	movl	(%rcx,%rdx,4), %edx
	movl	%edx, -16(%rsp)
	movl	(%rcx,%rax,4), %eax
	movl	%eax, -12(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, __op_param1+8(%rbp)
	ret
.LFE1271:
	.size	op_pshufd_xmm, .-op_pshufd_xmm
	.p2align 4,,15
.globl op_pshuflw_xmm
	.type	op_pshuflw_xmm, @function
op_pshuflw_xmm:
.LFB1272:
	leal	__op_param3(%rip), %eax
	leaq	__op_param2(%rbp), %rcx
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movzwl	(%rcx,%rdx,2), %edx
	movw	%dx, -24(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movzwl	(%rcx,%rdx,2), %edx
	movw	%dx, -22(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	andl	$3, %eax
	movzwl	(%rcx,%rdx,2), %edx
	movw	%dx, -20(%rsp)
	movzwl	(%rcx,%rax,2), %eax
	movw	%ax, -18(%rsp)
	movq	8(%rcx), %rdx
	movq	-24(%rsp), %rax
	movq	%rdx, __op_param1+8(%rbp)
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1272:
	.size	op_pshuflw_xmm, .-op_pshuflw_xmm
	.p2align 4,,15
.globl op_pshufhw_xmm
	.type	op_pshufhw_xmm, @function
op_pshufhw_xmm:
.LFB1273:
	leal	__op_param3(%rip), %eax
	leaq	__op_param2(%rbp), %rcx
	movq	__op_param2(%rbp), %rdi
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movzwl	8(%rcx,%rdx,2), %edx
	movw	%dx, -16(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	movzwl	8(%rcx,%rdx,2), %edx
	movw	%dx, -14(%rsp)
	movq	%rax, %rdx
	sarl	$2, %eax
	andl	$3, %edx
	andl	$3, %eax
	movzwl	8(%rcx,%rdx,2), %edx
	movw	%dx, -12(%rsp)
	movzwl	8(%rcx,%rax,2), %eax
	movq	%rdi, __op_param1(%rbp)
	movw	%ax, -10(%rsp)
	movq	-16(%rsp), %rax
	movq	%rax, __op_param1+8(%rbp)
	ret
.LFE1273:
	.size	op_pshufhw_xmm, .-op_pshufhw_xmm
	.p2align 4,,15
.globl op_minps
	.type	op_minps, @function
op_minps:
.LFB1290:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	minss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	minss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	minss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	minss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1290:
	.size	op_minps, .-op_minps
	.p2align 4,,15
.globl op_minss
	.type	op_minss, @function
op_minss:
.LFB1291:
	movss	__op_param1(%rbp), %xmm0
	minss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1291:
	.size	op_minss, .-op_minss
	.p2align 4,,15
.globl op_minpd
	.type	op_minpd, @function
op_minpd:
.LFB1292:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	minsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	minsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1292:
	.size	op_minpd, .-op_minpd
	.p2align 4,,15
.globl op_minsd
	.type	op_minsd, @function
op_minsd:
.LFB1293:
	movlpd	__op_param1(%rbp), %xmm0
	minsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1293:
	.size	op_minsd, .-op_minsd
	.p2align 4,,15
.globl op_maxps
	.type	op_maxps, @function
op_maxps:
.LFB1294:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	maxss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	maxss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	maxss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	maxss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1294:
	.size	op_maxps, .-op_maxps
	.p2align 4,,15
.globl op_maxss
	.type	op_maxss, @function
op_maxss:
.LFB1295:
	movss	__op_param1(%rbp), %xmm0
	maxss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1295:
	.size	op_maxss, .-op_maxss
	.p2align 4,,15
.globl op_maxpd
	.type	op_maxpd, @function
op_maxpd:
.LFB1296:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	maxsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	maxsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1296:
	.size	op_maxpd, .-op_maxpd
	.p2align 4,,15
.globl op_maxsd
	.type	op_maxsd, @function
op_maxsd:
.LFB1297:
	movlpd	__op_param1(%rbp), %xmm0
	maxsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1297:
	.size	op_maxsd, .-op_maxsd
	.p2align 4,,15
.globl op_haddps
	.type	op_haddps, @function
op_haddps:
.LFB1328:
	leaq	__op_param1(%rbp), %rcx
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rax
	addss	4(%rcx), %xmm0
	movss	%xmm0, -24(%rsp)
	movss	8(%rcx), %xmm0
	addss	12(%rcx), %xmm0
	movss	%xmm0, -20(%rsp)
	movss	__op_param2(%rbp), %xmm0
	addss	4(%rax), %xmm0
	movss	%xmm0, -16(%rsp)
	movss	8(%rax), %xmm0
	addss	12(%rax), %xmm0
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movss	%xmm0, -12(%rsp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1328:
	.size	op_haddps, .-op_haddps
	.p2align 4,,15
.globl op_haddpd
	.type	op_haddpd, @function
op_haddpd:
.LFB1329:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	addsd	8(%rcx), %xmm0
	movsd	%xmm0, -24(%rsp)
	movq	-24(%rsp), %rax
	movlpd	__op_param2(%rbp), %xmm0
	addsd	__op_param2+8(%rbp), %xmm0
	movq	%rax, __op_param1(%rbp)
	movsd	%xmm0, -16(%rsp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1329:
	.size	op_haddpd, .-op_haddpd
	.p2align 4,,15
.globl op_hsubps
	.type	op_hsubps, @function
op_hsubps:
.LFB1330:
	leaq	__op_param1(%rbp), %rcx
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rax
	subss	4(%rcx), %xmm0
	movss	%xmm0, -24(%rsp)
	movss	8(%rcx), %xmm0
	subss	12(%rcx), %xmm0
	movss	%xmm0, -20(%rsp)
	movss	__op_param2(%rbp), %xmm0
	subss	4(%rax), %xmm0
	movss	%xmm0, -16(%rsp)
	movss	8(%rax), %xmm0
	subss	12(%rax), %xmm0
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movss	%xmm0, -12(%rsp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1330:
	.size	op_hsubps, .-op_hsubps
	.p2align 4,,15
.globl op_hsubpd
	.type	op_hsubpd, @function
op_hsubpd:
.LFB1331:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	subsd	8(%rcx), %xmm0
	movsd	%xmm0, -24(%rsp)
	movq	-24(%rsp), %rax
	movlpd	__op_param2(%rbp), %xmm0
	subsd	__op_param2+8(%rbp), %xmm0
	movq	%rax, __op_param1(%rbp)
	movsd	%xmm0, -16(%rsp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1331:
	.size	op_hsubpd, .-op_hsubpd
	.p2align 4,,15
.globl op_addsubps
	.type	op_addsubps, @function
op_addsubps:
.LFB1332:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	subss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	addss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	subss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	addss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1332:
	.size	op_addsubps, .-op_addsubps
	.p2align 4,,15
.globl op_addsubpd
	.type	op_addsubpd, @function
op_addsubpd:
.LFB1333:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	subsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	addsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1333:
	.size	op_addsubpd, .-op_addsubpd
	.p2align 4,,15
.globl op_movmskps
	.type	op_movmskps, @function
op_movmskps:
.LFB1370:
	leaq	__op_param1(%rbp), %rcx
	movl	4(%rcx), %eax
	movl	(%rcx), %esi
	movl	8(%rcx), %edx
	shrl	$31, %eax
	shrl	$31, %esi
	addl	%eax, %eax
	shrl	$31, %edx
	orl	%eax, %esi
	movl	12(%rcx), %eax
	sall	$2, %edx
	movl	%esi, %ebx
	shrl	$31, %eax
	sall	$3, %eax
	orl	%eax, %edx
	orl	%edx, %ebx
	ret
.LFE1370:
	.size	op_movmskps, .-op_movmskps
	.p2align 4,,15
.globl op_movmskpd
	.type	op_movmskpd, @function
op_movmskpd:
.LFB1371:
	leaq	__op_param1(%rbp), %rax
	movl	4(%rax), %edx
	movl	12(%rax), %eax
	shrl	$31, %edx
	shrl	$31, %eax
	addl	%eax, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	ret
.LFE1371:
	.size	op_movmskpd, .-op_movmskpd
	.p2align 4,,15
.globl op_pmovmskb_xmm
	.type	op_pmovmskb_xmm, @function
op_pmovmskb_xmm:
.LFB1372:
	leaq	__op_param1(%rbp), %rsi
	xorl	%ebx, %ebx
	movzbl	(%rsi), %edx
	movzbl	1(%rsi), %eax
	movzbl	5(%rsi), %ecx
	shrb	$7, %dl
	shrb	$6, %al
	movzbl	%dl, %edx
	andl	$2, %eax
	shrb	$2, %cl
	orl	%edx, %eax
	movl	%edx, %ebx
	movzbl	2(%rsi), %edx
	andl	$32, %ecx
	shrb	$5, %dl
	andl	$4, %edx
	orl	%eax, %edx
	movzbl	3(%rsi), %eax
	shrb	$4, %al
	andl	$8, %eax
	orl	%edx, %eax
	movzbl	4(%rsi), %edx
	shrb	$3, %dl
	andl	$16, %edx
	orl	%eax, %edx
	movzbl	6(%rsi), %eax
	orl	%edx, %ecx
	movzbl	7(%rsi), %edx
	shrb	%al
	andl	$64, %eax
	andl	$128, %edx
	orl	%ecx, %eax
	movzbl	14(%rsi), %ecx
	orl	%eax, %edx
	movzbl	8(%rsi), %eax
	addl	%eax, %eax
	andl	$256, %eax
	orl	%edx, %eax
	movzbl	9(%rsi), %edx
	sall	$2, %edx
	andl	$512, %edx
	orl	%eax, %edx
	movzbl	10(%rsi), %eax
	sall	$3, %eax
	andl	$1024, %eax
	orl	%edx, %eax
	movzbl	11(%rsi), %edx
	sall	$4, %edx
	andl	$2048, %edx
	orl	%eax, %edx
	movzbl	12(%rsi), %eax
	sall	$7, %ecx
	andl	$16384, %ecx
	sall	$5, %eax
	andl	$4096, %eax
	orl	%edx, %eax
	movzbl	13(%rsi), %edx
	sall	$6, %edx
	andl	$8192, %edx
	orl	%eax, %edx
	movzbl	15(%rsi), %eax
	orl	%edx, %ecx
	movl	%ecx, %ebx
	sall	$8, %eax
	andl	$32768, %eax
	orl	%eax, %ebx
	ret
.LFE1372:
	.size	op_pmovmskb_xmm, .-op_pmovmskb_xmm
	.p2align 4,,15
.globl op_pinsrw_xmm
	.type	op_pinsrw_xmm, @function
op_pinsrw_xmm:
.LFB1373:
	movl	$__op_param2, %eax
	cltq
	movw	%bx, __op_param1(%rbp,%rax,2)
	ret
.LFE1373:
	.size	op_pinsrw_xmm, .-op_pinsrw_xmm
	.p2align 4,,15
.globl op_pextrw_xmm
	.type	op_pextrw_xmm, @function
op_pextrw_xmm:
.LFB1374:
	movl	$__op_param2, %eax
	cltq
	movzwl	__op_param1(%rbp,%rax,2), %ebx
	ret
.LFE1374:
	.size	op_pextrw_xmm, .-op_pextrw_xmm
	.p2align 4,,15
.globl op_punpcklbw_xmm
	.type	op_punpcklbw_xmm, @function
op_punpcklbw_xmm:
.LFB1378:
	movzbl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdx
	movb	%al, -24(%rsp)
	movzbl	__op_param2(%rbp), %eax
	movb	%al, -23(%rsp)
	movzbl	1(%rcx), %eax
	movb	%al, -22(%rsp)
	movzbl	1(%rdx), %eax
	movb	%al, -21(%rsp)
	movzbl	2(%rcx), %eax
	movb	%al, -20(%rsp)
	movzbl	2(%rdx), %eax
	movb	%al, -19(%rsp)
	movzbl	3(%rcx), %eax
	movb	%al, -18(%rsp)
	movzbl	3(%rdx), %eax
	movb	%al, -17(%rsp)
	movzbl	4(%rcx), %eax
	movb	%al, -16(%rsp)
	movzbl	4(%rdx), %eax
	movb	%al, -15(%rsp)
	movzbl	5(%rcx), %eax
	movb	%al, -14(%rsp)
	movzbl	5(%rdx), %eax
	movb	%al, -13(%rsp)
	movzbl	6(%rcx), %eax
	movb	%al, -12(%rsp)
	movzbl	6(%rdx), %eax
	movb	%al, -11(%rsp)
	movzbl	7(%rcx), %eax
	movb	%al, -10(%rsp)
	movzbl	7(%rdx), %eax
	movb	%al, -9(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1378:
	.size	op_punpcklbw_xmm, .-op_punpcklbw_xmm
	.p2align 4,,15
.globl op_punpcklwd_xmm
	.type	op_punpcklwd_xmm, @function
op_punpcklwd_xmm:
.LFB1379:
	movl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rcx
	movw	%ax, -24(%rsp)
	movl	__op_param2(%rbp), %eax
	movw	%ax, -22(%rsp)
	movzwl	2(%rsi), %eax
	movw	%ax, -20(%rsp)
	movzwl	2(%rcx), %eax
	movw	%ax, -18(%rsp)
	movl	4(%rsi), %eax
	movw	%ax, -16(%rsp)
	movl	4(%rcx), %eax
	movw	%ax, -14(%rsp)
	movzwl	6(%rsi), %eax
	movw	%ax, -12(%rsp)
	movzwl	6(%rcx), %eax
	movw	%ax, -10(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rsi)
	ret
.LFE1379:
	.size	op_punpcklwd_xmm, .-op_punpcklwd_xmm
	.p2align 4,,15
.globl op_punpckldq_xmm
	.type	op_punpckldq_xmm, @function
op_punpckldq_xmm:
.LFB1380:
	movl	__op_param1(%rbp), %eax
	leaq	__op_param1(%rbp), %rcx
	movl	%eax, -24(%rsp)
	movl	__op_param2(%rbp), %eax
	movl	%eax, -20(%rsp)
	movl	4(%rcx), %eax
	movl	%eax, -16(%rsp)
	movl	__op_param2+4(%rbp), %eax
	movl	%eax, -12(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1380:
	.size	op_punpckldq_xmm, .-op_punpckldq_xmm
	.p2align 4,,15
.globl op_punpcklqdq_xmm
	.type	op_punpcklqdq_xmm, @function
op_punpcklqdq_xmm:
.LFB1381:
	movq	__op_param2(%rbp), %rax
	movq	%rax, __op_param1+8(%rbp)
	ret
.LFE1381:
	.size	op_punpcklqdq_xmm, .-op_punpcklqdq_xmm
	.p2align 4,,15
.globl op_punpckhbw_xmm
	.type	op_punpckhbw_xmm, @function
op_punpckhbw_xmm:
.LFB1382:
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdx
	movzbl	8(%rcx), %eax
	movb	%al, -24(%rsp)
	movzbl	8(%rdx), %eax
	movb	%al, -23(%rsp)
	movzbl	9(%rcx), %eax
	movb	%al, -22(%rsp)
	movzbl	9(%rdx), %eax
	movb	%al, -21(%rsp)
	movzbl	10(%rcx), %eax
	movb	%al, -20(%rsp)
	movzbl	10(%rdx), %eax
	movb	%al, -19(%rsp)
	movzbl	11(%rcx), %eax
	movb	%al, -18(%rsp)
	movzbl	11(%rdx), %eax
	movb	%al, -17(%rsp)
	movzbl	12(%rcx), %eax
	movb	%al, -16(%rsp)
	movzbl	12(%rdx), %eax
	movb	%al, -15(%rsp)
	movzbl	13(%rcx), %eax
	movb	%al, -14(%rsp)
	movzbl	13(%rdx), %eax
	movb	%al, -13(%rsp)
	movzbl	14(%rcx), %eax
	movb	%al, -12(%rsp)
	movzbl	14(%rdx), %eax
	movb	%al, -11(%rsp)
	movzbl	15(%rcx), %eax
	movb	%al, -10(%rsp)
	movzbl	15(%rdx), %eax
	movb	%al, -9(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1382:
	.size	op_punpckhbw_xmm, .-op_punpckhbw_xmm
	.p2align 4,,15
.globl op_punpckhwd_xmm
	.type	op_punpckhwd_xmm, @function
op_punpckhwd_xmm:
.LFB1383:
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rdx
	movl	8(%rcx), %eax
	movw	%ax, -24(%rsp)
	movl	8(%rdx), %eax
	movw	%ax, -22(%rsp)
	movzwl	10(%rcx), %eax
	movw	%ax, -20(%rsp)
	movzwl	10(%rdx), %eax
	movw	%ax, -18(%rsp)
	movl	12(%rcx), %eax
	movw	%ax, -16(%rsp)
	movl	12(%rdx), %eax
	movw	%ax, -14(%rsp)
	movzwl	14(%rcx), %eax
	movw	%ax, -12(%rsp)
	movzwl	14(%rdx), %eax
	movw	%ax, -10(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1383:
	.size	op_punpckhwd_xmm, .-op_punpckhwd_xmm
	.p2align 4,,15
.globl op_punpckhdq_xmm
	.type	op_punpckhdq_xmm, @function
op_punpckhdq_xmm:
.LFB1384:
	leaq	__op_param1(%rbp), %rcx
	leaq	__op_param2(%rbp), %rsi
	movl	8(%rcx), %eax
	movl	%eax, -24(%rsp)
	movl	8(%rsi), %eax
	movl	%eax, -20(%rsp)
	movl	12(%rcx), %eax
	movl	%eax, -16(%rsp)
	movl	12(%rsi), %eax
	movl	%eax, -12(%rsp)
	movq	-24(%rsp), %rax
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1384:
	.size	op_punpckhdq_xmm, .-op_punpckhdq_xmm
	.p2align 4,,15
.globl op_punpckhqdq_xmm
	.type	op_punpckhqdq_xmm, @function
op_punpckhqdq_xmm:
.LFB1385:
	leaq	__op_param1(%rbp), %rsi
	movq	__op_param2+8(%rbp), %rcx
	movq	8(%rsi), %rdx
	movq	%rdx, __op_param1(%rbp)
	movq	%rcx, 8(%rsi)
	ret
.LFE1385:
	.size	op_punpckhqdq_xmm, .-op_punpckhqdq_xmm
	.p2align 4,,15
.globl op_comisd
	.type	op_comisd, @function
op_comisd:
.LFB1369:
	subq	$8, %rsp
.LCFI27:
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm1
	movlpd	__op_param1(%rbp), %xmm0
	call	float64_compare
	cltq
	movl	comis_eflags+4(,%rax,4), %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1369:
	.size	op_comisd, .-op_comisd
	.p2align 4,,15
.globl op_ucomisd
	.type	op_ucomisd, @function
op_ucomisd:
.LFB1368:
	subq	$8, %rsp
.LCFI28:
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm1
	movlpd	__op_param1(%rbp), %xmm0
	call	float64_compare_quiet
	cltq
	movl	comis_eflags+4(,%rax,4), %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1368:
	.size	op_ucomisd, .-op_ucomisd
	.p2align 4,,15
.globl op_comiss
	.type	op_comiss, @function
op_comiss:
.LFB1367:
	subq	$8, %rsp
.LCFI29:
	leaq	472(%rbp), %rdi
	movss	__op_param2(%rbp), %xmm1
	movss	__op_param1(%rbp), %xmm0
	call	float32_compare
	cltq
	movl	comis_eflags+4(,%rax,4), %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1367:
	.size	op_comiss, .-op_comiss
	.p2align 4,,15
.globl op_ucomiss
	.type	op_ucomiss, @function
op_ucomiss:
.LFB1366:
	subq	$8, %rsp
.LCFI30:
	leaq	472(%rbp), %rdi
	movss	__op_param2(%rbp), %xmm1
	movss	__op_param1(%rbp), %xmm0
	call	float32_compare_quiet
	cltq
	movl	comis_eflags+4(,%rax,4), %eax
	movl	%eax, 40(%rbp)
	addq	$8, %rsp
	ret
.LFE1366:
	.size	op_ucomiss, .-op_ucomiss
	.p2align 4,,15
.globl op_rcpss
	.type	op_rcpss, @function
op_rcpss:
.LFB1327:
	pushq	%r14
.LCFI31:
	movss	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	call	approx_rcp
	movss	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1327:
	.size	op_rcpss, .-op_rcpss
	.p2align 4,,15
.globl op_rcpps
	.type	op_rcpps, @function
op_rcpps:
.LFB1326:
	leaq	__op_param1(%rbp), %rax
	movq	%r14, -16(%rsp)
.LCFI32:
	movq	%r15, -8(%rsp)
.LCFI33:
	subq	$24, %rsp
.LCFI34:
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	approx_rcp
	movss	%xmm0, __op_param1(%r14)
	movss	4(%r15), %xmm0
	call	approx_rcp
	movq	(%rsp), %rax
	movss	%xmm0, 4(%rax)
	movss	8(%r15), %xmm0
	call	approx_rcp
	movq	(%rsp), %rax
	movss	%xmm0, 8(%rax)
	movss	12(%r15), %xmm0
	call	approx_rcp
	movq	(%rsp), %rax
	movss	%xmm0, 12(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1326:
	.size	op_rcpps, .-op_rcpps
	.p2align 4,,15
.globl op_rsqrtss
	.type	op_rsqrtss, @function
op_rsqrtss:
.LFB1325:
	pushq	%r14
.LCFI35:
	movss	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	call	approx_rsqrt
	movss	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1325:
	.size	op_rsqrtss, .-op_rsqrtss
	.p2align 4,,15
.globl op_rsqrtps
	.type	op_rsqrtps, @function
op_rsqrtps:
.LFB1324:
	leaq	__op_param1(%rbp), %rax
	movq	%r14, -16(%rsp)
.LCFI36:
	movq	%r15, -8(%rsp)
.LCFI37:
	subq	$24, %rsp
.LCFI38:
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	approx_rsqrt
	movss	%xmm0, __op_param1(%r14)
	movss	4(%r15), %xmm0
	call	approx_rsqrt
	movq	(%rsp), %rax
	movss	%xmm0, 4(%rax)
	movss	8(%r15), %xmm0
	call	approx_rsqrt
	movq	(%rsp), %rax
	movss	%xmm0, 8(%rax)
	movss	12(%r15), %xmm0
	call	approx_rsqrt
	movq	(%rsp), %rax
	movss	%xmm0, 12(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1324:
	.size	op_rsqrtps, .-op_rsqrtps
	.p2align 4,,15
.globl op_cvttsd2si
	.type	op_cvttsd2si, @function
op_cvttsd2si:
.LFB1323:
	subq	$8, %rsp
.LCFI39:
	leaq	472(%rbp), %rdi
	movlpd	__op_param1(%rbp), %xmm0
	call	float64_to_int32_round_to_zero
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1323:
	.size	op_cvttsd2si, .-op_cvttsd2si
	.p2align 4,,15
.globl op_cvttpd2pi
	.type	op_cvttpd2pi, @function
op_cvttpd2pi:
.LFB1321:
	movq	%r14, -16(%rsp)
.LCFI40:
	movq	%r15, -8(%rsp)
.LCFI41:
	subq	$24, %rsp
.LCFI42:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movq	%rax, (%rsp)
	call	float64_to_int32_round_to_zero
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%r15), %xmm0
	call	float64_to_int32_round_to_zero
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1321:
	.size	op_cvttpd2pi, .-op_cvttpd2pi
	.p2align 4,,15
.globl op_cvttpd2dq
	.type	op_cvttpd2dq, @function
op_cvttpd2dq:
.LFB1319:
	movq	%r14, -16(%rsp)
.LCFI43:
	movq	%r15, -8(%rsp)
.LCFI44:
	subq	$24, %rsp
.LCFI45:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movq	%rax, (%rsp)
	call	float64_to_int32_round_to_zero
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%r15), %xmm0
	call	float64_to_int32_round_to_zero
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	$0, 8(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1319:
	.size	op_cvttpd2dq, .-op_cvttpd2dq
	.p2align 4,,15
.globl op_cvttss2si
	.type	op_cvttss2si, @function
op_cvttss2si:
.LFB1322:
	subq	$8, %rsp
.LCFI46:
	leaq	472(%rbp), %rdi
	movss	__op_param1(%rbp), %xmm0
	call	float32_to_int32_round_to_zero
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1322:
	.size	op_cvttss2si, .-op_cvttss2si
	.p2align 4,,15
.globl op_cvttps2pi
	.type	op_cvttps2pi, @function
op_cvttps2pi:
.LFB1320:
	movq	%r14, -16(%rsp)
.LCFI47:
	movq	%r15, -8(%rsp)
.LCFI48:
	leaq	__op_param1(%rbp), %rax
	subq	$24, %rsp
.LCFI49:
	leaq	472(%rbp), %rdi
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	float32_to_int32_round_to_zero
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movss	4(%r15), %xmm0
	call	float32_to_int32_round_to_zero
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1320:
	.size	op_cvttps2pi, .-op_cvttps2pi
	.p2align 4,,15
.globl op_cvttps2dq
	.type	op_cvttps2dq, @function
op_cvttps2dq:
.LFB1318:
	pushq	%r15
.LCFI50:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	leaq	__op_param2(%rbp), %r15
	pushq	%r14
.LCFI51:
	movq	%rbp, %r14
	subq	$8, %rsp
.LCFI52:
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	float32_to_int32_round_to_zero
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movss	4(%r15), %xmm0
	call	float32_to_int32_round_to_zero
	movq	(%rsp), %rdx
	leaq	472(%rbp), %rdi
	movl	%eax, 4(%rdx)
	movss	8(%r15), %xmm0
	call	float32_to_int32_round_to_zero
	movq	(%rsp), %rdx
	leaq	472(%rbp), %rdi
	movl	%eax, 8(%rdx)
	movss	12(%r15), %xmm0
	call	float32_to_int32_round_to_zero
	movq	(%rsp), %rdx
	movl	%eax, 12(%rdx)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1318:
	.size	op_cvttps2dq, .-op_cvttps2dq
	.p2align 4,,15
.globl op_cvtsd2si
	.type	op_cvtsd2si, @function
op_cvtsd2si:
.LFB1317:
	subq	$8, %rsp
.LCFI53:
	leaq	472(%rbp), %rdi
	movlpd	__op_param1(%rbp), %xmm0
	call	float64_to_int32
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1317:
	.size	op_cvtsd2si, .-op_cvtsd2si
	.p2align 4,,15
.globl op_cvtpd2pi
	.type	op_cvtpd2pi, @function
op_cvtpd2pi:
.LFB1315:
	movq	%r14, -16(%rsp)
.LCFI54:
	movq	%r15, -8(%rsp)
.LCFI55:
	subq	$24, %rsp
.LCFI56:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movq	%rax, (%rsp)
	call	float64_to_int32
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%r15), %xmm0
	call	float64_to_int32
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1315:
	.size	op_cvtpd2pi, .-op_cvtpd2pi
	.p2align 4,,15
.globl op_cvtpd2dq
	.type	op_cvtpd2dq, @function
op_cvtpd2dq:
.LFB1313:
	movq	%r14, -16(%rsp)
.LCFI57:
	movq	%r15, -8(%rsp)
.LCFI58:
	subq	$24, %rsp
.LCFI59:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movq	%rax, (%rsp)
	call	float64_to_int32
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%r15), %xmm0
	call	float64_to_int32
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	$0, 8(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1313:
	.size	op_cvtpd2dq, .-op_cvtpd2dq
	.p2align 4,,15
.globl op_cvtss2si
	.type	op_cvtss2si, @function
op_cvtss2si:
.LFB1316:
	subq	$8, %rsp
.LCFI60:
	leaq	472(%rbp), %rdi
	movss	__op_param1(%rbp), %xmm0
	call	float32_to_int32
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE1316:
	.size	op_cvtss2si, .-op_cvtss2si
	.p2align 4,,15
.globl op_cvtps2pi
	.type	op_cvtps2pi, @function
op_cvtps2pi:
.LFB1314:
	movq	%r14, -16(%rsp)
.LCFI61:
	movq	%r15, -8(%rsp)
.LCFI62:
	leaq	__op_param1(%rbp), %rax
	subq	$24, %rsp
.LCFI63:
	leaq	472(%rbp), %rdi
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	float32_to_int32
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movss	4(%r15), %xmm0
	call	float32_to_int32
	movq	(%rsp), %rdx
	movl	%eax, 4(%rdx)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1314:
	.size	op_cvtps2pi, .-op_cvtps2pi
	.p2align 4,,15
.globl op_cvtps2dq
	.type	op_cvtps2dq, @function
op_cvtps2dq:
.LFB1312:
	pushq	%r15
.LCFI64:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	leaq	__op_param2(%rbp), %r15
	pushq	%r14
.LCFI65:
	movq	%rbp, %r14
	subq	$8, %rsp
.LCFI66:
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	float32_to_int32
	movl	%eax, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movss	4(%r15), %xmm0
	call	float32_to_int32
	movq	(%rsp), %rdx
	leaq	472(%rbp), %rdi
	movl	%eax, 4(%rdx)
	movss	8(%r15), %xmm0
	call	float32_to_int32
	movq	(%rsp), %rdx
	leaq	472(%rbp), %rdi
	movl	%eax, 8(%rdx)
	movss	12(%r15), %xmm0
	call	float32_to_int32
	movq	(%rsp), %rdx
	movl	%eax, 12(%rdx)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1312:
	.size	op_cvtps2dq, .-op_cvtps2dq
	.p2align 4,,15
.globl op_cvtsi2sd
	.type	op_cvtsi2sd, @function
op_cvtsi2sd:
.LFB1311:
	pushq	%r14
.LCFI67:
	leaq	472(%rbp), %rsi
	movq	%rbp, %r14
	movl	%ebx, %edi
	call	int32_to_float64
	movsd	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1311:
	.size	op_cvtsi2sd, .-op_cvtsi2sd
	.p2align 4,,15
.globl op_cvtpi2pd
	.type	op_cvtpi2pd, @function
op_cvtpi2pd:
.LFB1309:
	movq	%r14, -16(%rsp)
.LCFI68:
	movq	%r15, -8(%rsp)
.LCFI69:
	subq	$24, %rsp
.LCFI70:
	movl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %r15
	leaq	472(%rbp), %rsi
	movq	%rbp, %r14
	movq	%rax, (%rsp)
	call	int32_to_float64
	movsd	%xmm0, __op_param1(%r14)
	movl	4(%r15), %edi
	leaq	472(%rbp), %rsi
	call	int32_to_float64
	movq	(%rsp), %rax
	movsd	%xmm0, 8(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1309:
	.size	op_cvtpi2pd, .-op_cvtpi2pd
	.p2align 4,,15
.globl op_cvtdq2pd
	.type	op_cvtdq2pd, @function
op_cvtdq2pd:
.LFB1307:
	movq	%r14, -16(%rsp)
.LCFI71:
	movq	%r15, -8(%rsp)
.LCFI72:
	subq	$24, %rsp
.LCFI73:
	movl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rax
	movl	__op_param2+4(%rbp), %r15d
	leaq	472(%rbp), %rsi
	movq	%rbp, %r14
	movq	%rax, (%rsp)
	call	int32_to_float64
	leaq	472(%rbp), %rsi
	movl	%r15d, %edi
	movsd	%xmm0, __op_param1(%r14)
	call	int32_to_float64
	movq	(%rsp), %rax
	movsd	%xmm0, 8(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1307:
	.size	op_cvtdq2pd, .-op_cvtdq2pd
	.p2align 4,,15
.globl op_cvtsi2ss
	.type	op_cvtsi2ss, @function
op_cvtsi2ss:
.LFB1310:
	pushq	%r14
.LCFI74:
	leaq	472(%rbp), %rsi
	movq	%rbp, %r14
	movl	%ebx, %edi
	call	int32_to_float32
	movss	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1310:
	.size	op_cvtsi2ss, .-op_cvtsi2ss
	.p2align 4,,15
.globl op_cvtpi2ps
	.type	op_cvtpi2ps, @function
op_cvtpi2ps:
.LFB1308:
	movq	%r14, -16(%rsp)
.LCFI75:
	movq	%r15, -8(%rsp)
.LCFI76:
	subq	$24, %rsp
.LCFI77:
	movl	__op_param2(%rbp), %edi
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %r15
	leaq	472(%rbp), %rsi
	movq	%rbp, %r14
	movq	%rax, (%rsp)
	call	int32_to_float32
	movss	%xmm0, __op_param1(%r14)
	movl	4(%r15), %edi
	leaq	472(%rbp), %rsi
	call	int32_to_float32
	movq	(%rsp), %rax
	movss	%xmm0, 4(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1308:
	.size	op_cvtpi2ps, .-op_cvtpi2ps
	.p2align 4,,15
.globl op_cvtdq2ps
	.type	op_cvtdq2ps, @function
op_cvtdq2ps:
.LFB1306:
	pushq	%r15
.LCFI78:
	leaq	__op_param1(%rbp), %rax
	leaq	__op_param2(%rbp), %r15
	leaq	472(%rbp), %rsi
	pushq	%r14
.LCFI79:
	movq	%rbp, %r14
	subq	$8, %rsp
.LCFI80:
	movl	__op_param2(%rbp), %edi
	movq	%rax, (%rsp)
	call	int32_to_float32
	movss	%xmm0, __op_param1(%r14)
	movl	4(%r15), %edi
	leaq	472(%rbp), %rsi
	call	int32_to_float32
	movq	(%rsp), %rax
	leaq	472(%rbp), %rsi
	movss	%xmm0, 4(%rax)
	movl	8(%r15), %edi
	call	int32_to_float32
	movq	(%rsp), %rax
	leaq	472(%rbp), %rsi
	movss	%xmm0, 8(%rax)
	movl	12(%r15), %edi
	call	int32_to_float32
	movq	(%rsp), %rax
	movss	%xmm0, 12(%rax)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1306:
	.size	op_cvtdq2ps, .-op_cvtdq2ps
	.p2align 4,,15
.globl op_cvtsd2ss
	.type	op_cvtsd2ss, @function
op_cvtsd2ss:
.LFB1305:
	pushq	%r14
.LCFI81:
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	call	float64_to_float32
	movss	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1305:
	.size	op_cvtsd2ss, .-op_cvtsd2ss
	.p2align 4,,15
.globl op_cvtpd2ps
	.type	op_cvtpd2ps, @function
op_cvtpd2ps:
.LFB1303:
	movq	%r14, -16(%rsp)
.LCFI82:
	movq	%r15, -8(%rsp)
.LCFI83:
	subq	$24, %rsp
.LCFI84:
	leaq	__op_param2(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	leaq	__op_param1(%rbp), %r15
	movq	%rbp, %r14
	movq	%rax, (%rsp)
	call	float64_to_float32
	movq	(%rsp), %rax
	movss	%xmm0, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%rax), %xmm0
	call	float64_to_float32
	movq	$0, 8(%r15)
	movss	%xmm0, 4(%r15)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1303:
	.size	op_cvtpd2ps, .-op_cvtpd2ps
	.p2align 4,,15
.globl op_cvtss2sd
	.type	op_cvtss2sd, @function
op_cvtss2sd:
.LFB1304:
	pushq	%r14
.LCFI85:
	leaq	472(%rbp), %rdi
	movq	%rbp, %r14
	movss	__op_param2(%rbp), %xmm0
	call	float32_to_float64
	movsd	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1304:
	.size	op_cvtss2sd, .-op_cvtss2sd
	.p2align 4,,15
.globl op_cvtps2pd
	.type	op_cvtps2pd, @function
op_cvtps2pd:
.LFB1302:
	movq	%r14, -16(%rsp)
.LCFI86:
	movq	%r15, -8(%rsp)
.LCFI87:
	subq	$40, %rsp
.LCFI88:
	movl	__op_param2+4(%rbp), %r15d
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movss	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	movq	%rax, 16(%rsp)
	call	float32_to_float64
	movl	%r15d, 12(%rsp)
	leaq	472(%rbp), %rdi
	movsd	%xmm0, __op_param1(%r14)
	movss	12(%rsp), %xmm0
	call	float32_to_float64
	movq	16(%rsp), %rax
	movsd	%xmm0, 8(%rax)
	movq	24(%rsp), %r14
	movq	32(%rsp), %r15
	addq	$40, %rsp
	ret
.LFE1302:
	.size	op_cvtps2pd, .-op_cvtps2pd
	.p2align 4,,15
.globl op_sqrtsd
	.type	op_sqrtsd, @function
op_sqrtsd:
.LFB1301:
	pushq	%r14
.LCFI89:
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	call	float64_sqrt
	movsd	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1301:
	.size	op_sqrtsd, .-op_sqrtsd
	.p2align 4,,15
.globl op_sqrtpd
	.type	op_sqrtpd, @function
op_sqrtpd:
.LFB1300:
	movq	%r14, -16(%rsp)
.LCFI90:
	movq	%r15, -8(%rsp)
.LCFI91:
	subq	$24, %rsp
.LCFI92:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	movlpd	__op_param2(%rbp), %xmm0
	movq	%rbp, %r14
	leaq	__op_param2(%rbp), %r15
	movq	%rax, (%rsp)
	call	float64_sqrt
	movsd	%xmm0, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movlpd	8(%r15), %xmm0
	call	float64_sqrt
	movq	(%rsp), %rax
	movsd	%xmm0, 8(%rax)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE1300:
	.size	op_sqrtpd, .-op_sqrtpd
	.p2align 4,,15
.globl op_sqrtss
	.type	op_sqrtss, @function
op_sqrtss:
.LFB1299:
	pushq	%r14
.LCFI93:
	leaq	472(%rbp), %rdi
	movq	%rbp, %r14
	movss	__op_param2(%rbp), %xmm0
	call	float32_sqrt
	movss	%xmm0, __op_param1(%r14)
	popq	%r14
	ret
.LFE1299:
	.size	op_sqrtss, .-op_sqrtss
	.p2align 4,,15
.globl op_sqrtps
	.type	op_sqrtps, @function
op_sqrtps:
.LFB1298:
	pushq	%r15
.LCFI94:
	leaq	__op_param1(%rbp), %rax
	leaq	472(%rbp), %rdi
	leaq	__op_param2(%rbp), %r15
	pushq	%r14
.LCFI95:
	movq	%rbp, %r14
	subq	$8, %rsp
.LCFI96:
	movss	__op_param2(%rbp), %xmm0
	movq	%rax, (%rsp)
	call	float32_sqrt
	movss	%xmm0, __op_param1(%r14)
	leaq	472(%rbp), %rdi
	movss	4(%r15), %xmm0
	call	float32_sqrt
	movq	(%rsp), %rax
	leaq	472(%rbp), %rdi
	movss	%xmm0, 4(%rax)
	movss	8(%r15), %xmm0
	call	float32_sqrt
	movq	(%rsp), %rax
	leaq	472(%rbp), %rdi
	movss	%xmm0, 8(%rax)
	movss	12(%r15), %xmm0
	call	float32_sqrt
	movq	(%rsp), %rax
	movss	%xmm0, 12(%rax)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1298:
	.size	op_sqrtps, .-op_sqrtps
	.p2align 4,,15
.globl op_maskmov_xmm
	.type	op_maskmov_xmm, @function
op_maskmov_xmm:
.LFB1266:
	pushq	%r15
.LCFI97:
	leaq	__op_param1(%rbp), %rdx
	leaq	__op_param2(%rbp), %r15
	pushq	%r14
.LCFI98:
	xorl	%r14d, %r14d
	subq	$8, %rsp
.LCFI99:
	movq	%rdx, (%rsp)
	.p2align 4,,7
.L1784:
	cmpb	$0, (%r14,%r15)
	jns	.L1785
	movq	(%rsp), %rax
	leal	(%r13,%r14), %edi
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movzbl	(%r14,%rax), %r9d
	movl	%edi, %eax
	andl	$3, %edx
	shrl	$12, %eax
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	movslq	%r8d,%rdx
	salq	$5, %rax
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L1787
	movzbl	%r9b, %esi
	movl	%r8d, %edx
	call	__stb_mmu
	jmp	.L1785
	.p2align 4,,7
.L1787:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	movzbl	%r9b, %esi
	call	remR3PhysWriteU8
	.p2align 4,,7
.L1785:
	incq	%r14
	cmpq	$16, %r14
	jne	.L1784
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1266:
	.size	op_maskmov_xmm, .-op_maskmov_xmm
	.p2align 4,,15
.globl op_sbbb_user_T0_T1_cc
	.type	op_sbbb_user_T0_T1_cc, @function
op_sbbb_user_T0_T1_cc:
.LFB716:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI100:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %ecx
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %ecx
	subl	%eax, %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%ecx, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L1794
	movzbl	%cl, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L1796
	.p2align 4,,7
.L1794:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%cl, %esi
	call	remR3PhysWriteU8
.L1796:
	leal	14(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE716:
	.size	op_sbbb_user_T0_T1_cc, .-op_sbbb_user_T0_T1_cc
	.p2align 4,,15
.globl op_fxrstor_A0
	.type	op_fxrstor_A0, @function
op_fxrstor_A0:
.LFB1148:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI101:
	movl	%r13d, %edi
	call	helper_fxrstor
	addq	$8, %rsp
	ret
.LFE1148:
	.size	op_fxrstor_A0, .-op_fxrstor_A0
	.p2align 4,,15
.globl op_fxsave_A0
	.type	op_fxsave_A0, @function
op_fxsave_A0:
.LFB1147:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI102:
	movl	%r13d, %edi
	call	helper_fxsave
	addq	$8, %rsp
	ret
.LFE1147:
	.size	op_fxsave_A0, .-op_fxsave_A0
	.p2align 4,,15
.globl op_unlock
	.type	op_unlock, @function
op_unlock:
.LFB1141:
	subq	$8, %rsp
.LCFI103:
	call	cpu_unlock
	addq	$8, %rsp
	ret
.LFE1141:
	.size	op_unlock, .-op_unlock
	.p2align 4,,15
.globl op_lock
	.type	op_lock, @function
op_lock:
.LFB1140:
	subq	$8, %rsp
.LCFI104:
	call	cpu_lock
	addq	$8, %rsp
	ret
.LFE1140:
	.size	op_lock, .-op_lock
	.p2align 4,,15
.globl op_frstor_A0
	.type	op_frstor_A0, @function
op_frstor_A0:
.LFB1139:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI105:
	movl	%r13d, %edi
	call	helper_frstor
	addq	$8, %rsp
	ret
.LFE1139:
	.size	op_frstor_A0, .-op_frstor_A0
	.p2align 4,,15
.globl op_fnsave_A0
	.type	op_fnsave_A0, @function
op_fnsave_A0:
.LFB1138:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI106:
	movl	%r13d, %edi
	call	helper_fsave
	addq	$8, %rsp
	ret
.LFE1138:
	.size	op_fnsave_A0, .-op_fnsave_A0
	.p2align 4,,15
.globl op_fldenv_A0
	.type	op_fldenv_A0, @function
op_fldenv_A0:
.LFB1137:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI107:
	movl	%r13d, %edi
	call	helper_fldenv
	addq	$8, %rsp
	ret
.LFE1137:
	.size	op_fldenv_A0, .-op_fldenv_A0
	.p2align 4,,15
.globl op_fnstenv_A0
	.type	op_fnstenv_A0, @function
op_fnstenv_A0:
.LFB1136:
	leal	__op_param1(%rip), %esi
	subq	$8, %rsp
.LCFI108:
	movl	%r13d, %edi
	call	helper_fstenv
	addq	$8, %rsp
	ret
.LFE1136:
	.size	op_fnstenv_A0, .-op_fnstenv_A0
	.p2align 4,,15
.globl op_fwait
	.type	op_fwait, @function
op_fwait:
.LFB1134:
	subq	$8, %rsp
.LCFI109:
	cmpb	$0, 288(%rbp)
	jns	.L1815
	call	fpu_raise_exception
.L1815:
	addq	$8, %rsp
	ret
.LFE1134:
	.size	op_fwait, .-op_fwait
	.p2align 4,,15
.globl op_fldcw_A0
	.type	op_fldcw_A0, @function
op_fldcw_A0:
.LFB1132:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	pushq	%r14
.LCFI110:
	movl	%r13d, %edi
	movzbl	%al, %eax
	movq	%rbp, %r14
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L1819
	call	__ldw_mmu
	movzwl	%ax, %eax
	jmp	.L1821
	.p2align 4,,7
.L1819:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU16
	movzwl	%ax, %eax
.L1821:
	movl	%eax, 292(%r14)
	call	update_fp_status
	popq	%r14
	ret
.LFE1132:
	.size	op_fldcw_A0, .-op_fldcw_A0
	.p2align 4,,15
.globl op_lduw_user_T0_A0
	.type	op_lduw_user_T0_A0, @function
op_lduw_user_T0_A0:
.LFB601:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI111:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L1824
	movl	$1, %esi
	call	__ldw_mmu
	movzwl	%ax, %eax
	jmp	.L1826
	.p2align 4,,7
.L1824:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU16
	movzwl	%ax, %eax
.L1826:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE601:
	.size	op_lduw_user_T0_A0, .-op_lduw_user_T0_A0
	.p2align 4,,15
.globl op_lduw_kernel_T0_A0
	.type	op_lduw_kernel_T0_A0, @function
op_lduw_kernel_T0_A0:
.LFB582:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI112:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L1829
	xorl	%esi, %esi
	call	__ldw_mmu
	movzwl	%ax, %eax
	jmp	.L1831
	.p2align 4,,7
.L1829:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU16
	movzwl	%ax, %eax
.L1831:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE582:
	.size	op_lduw_kernel_T0_A0, .-op_lduw_kernel_T0_A0
	.p2align 4,,15
.globl op_lduw_raw_T1_A0
	.type	op_lduw_raw_T1_A0, @function
op_lduw_raw_T1_A0:
.LFB568:
	subq	$8, %rsp
.LCFI113:
	mov	%r13d, %edi
	call	remR3PhysReadU16
	addq	$8, %rsp
	movzwl	%ax, %r12d
	ret
.LFE568:
	.size	op_lduw_raw_T1_A0, .-op_lduw_raw_T1_A0
	.p2align 4,,15
.globl op_fnstsw_A0
	.type	op_fnstsw_A0, @function
op_fnstsw_A0:
.LFB1129:
	subq	$8, %rsp
.LCFI114:
	movl	284(%rbp), %eax
	movl	288(%rbp), %r8d
	movl	56(%rbp), %edx
	xorl	%r9d, %r9d
	movl	%r13d, %edi
	andl	$7, %eax
	andl	$-14337, %r8d
	sall	$11, %eax
	andl	$3, %edx
	orl	%eax, %r8d
	movl	%r13d, %eax
	shrl	$12, %eax
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r9b
	leaq	0(,%rax,8), %rcx
	movslq	%r9d,%rdx
	salq	$5, %rax
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L1836
	movzwl	%r8w, %esi
	movl	%r9d, %edx
	call	__stw_mmu
	jmp	.L1838
	.p2align 4,,7
.L1836:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%r8w, %esi
	call	remR3PhysWriteU16
.L1838:
	addq	$8, %rsp
	ret
.LFE1129:
	.size	op_fnstsw_A0, .-op_fnstsw_A0
	.p2align 4,,15
.globl op_rorw_user_T0_T1
	.type	op_rorw_user_T0_T1, @function
op_rorw_user_T0_T1:
.LFB812:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI115:
	andl	$15, %ecx
	je	.L1841
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L1843
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L1841
	.p2align 4,,7
.L1843:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L1841:
	addq	$8, %rsp
	ret
.LFE812:
	.size	op_rorw_user_T0_T1, .-op_rorw_user_T0_T1
	.p2align 4,,15
.globl op_rolw_kernel_T0_T1
	.type	op_rolw_kernel_T0_T1, @function
op_rolw_kernel_T0_T1:
.LFB795:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI116:
	andl	$15, %ecx
	je	.L1847
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L1849
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L1847
	.p2align 4,,7
.L1849:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L1847:
	addq	$8, %rsp
	ret
.LFE795:
	.size	op_rolw_kernel_T0_T1, .-op_rolw_kernel_T0_T1
	.p2align 4,,15
.globl op_shldw_raw_T0_T1_im_cc
	.type	op_shldw_raw_T0_T1_im_cc, @function
op_shldw_raw_T0_T1_im_cc:
.LFB786:
	pushq	%r15
.LCFI117:
	movzwl	%r12w,%eax
	leal	__op_param1(%rip), %r15d
	movl	%eax, %r12d
	pushq	%r14
.LCFI118:
	movl	%ebx, %r14d
	movl	%r15d, %ecx
	sall	$16, %r14d
	orl	%eax, %r14d
	subq	$8, %rsp
.LCFI119:
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L1853
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1853:
	movl	%edx, %esi
	mov	%r13d, %edi
	shrl	$16, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE786:
	.size	op_shldw_raw_T0_T1_im_cc, .-op_shldw_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_fcos
	.type	op_fcos, @function
op_fcos:
.LFB1128:
	subq	$8, %rsp
.LCFI120:
	call	helper_fcos
	addq	$8, %rsp
	ret
.LFE1128:
	.size	op_fcos, .-op_fcos
	.p2align 4,,15
.globl op_fsin
	.type	op_fsin, @function
op_fsin:
.LFB1127:
	subq	$8, %rsp
.LCFI121:
	call	helper_fsin
	addq	$8, %rsp
	ret
.LFE1127:
	.size	op_fsin, .-op_fsin
	.p2align 4,,15
.globl op_fscale
	.type	op_fscale, @function
op_fscale:
.LFB1126:
	subq	$8, %rsp
.LCFI122:
	call	helper_fscale
	addq	$8, %rsp
	ret
.LFE1126:
	.size	op_fscale, .-op_fscale
	.p2align 4,,15
.globl op_frndint
	.type	op_frndint, @function
op_frndint:
.LFB1125:
	subq	$8, %rsp
.LCFI123:
	call	helper_frndint
	addq	$8, %rsp
	ret
.LFE1125:
	.size	op_frndint, .-op_frndint
	.p2align 4,,15
.globl op_fsincos
	.type	op_fsincos, @function
op_fsincos:
.LFB1124:
	subq	$8, %rsp
.LCFI124:
	call	helper_fsincos
	addq	$8, %rsp
	ret
.LFE1124:
	.size	op_fsincos, .-op_fsincos
	.p2align 4,,15
.globl op_fsqrt
	.type	op_fsqrt, @function
op_fsqrt:
.LFB1123:
	subq	$8, %rsp
.LCFI125:
	call	helper_fsqrt
	addq	$8, %rsp
	ret
.LFE1123:
	.size	op_fsqrt, .-op_fsqrt
	.p2align 4,,15
.globl op_fyl2xp1
	.type	op_fyl2xp1, @function
op_fyl2xp1:
.LFB1122:
	subq	$8, %rsp
.LCFI126:
	call	helper_fyl2xp1
	addq	$8, %rsp
	ret
.LFE1122:
	.size	op_fyl2xp1, .-op_fyl2xp1
	.p2align 4,,15
.globl op_fprem
	.type	op_fprem, @function
op_fprem:
.LFB1121:
	subq	$8, %rsp
.LCFI127:
	call	helper_fprem
	addq	$8, %rsp
	ret
.LFE1121:
	.size	op_fprem, .-op_fprem
	.p2align 4,,15
.globl op_fprem1
	.type	op_fprem1, @function
op_fprem1:
.LFB1120:
	subq	$8, %rsp
.LCFI128:
	call	helper_fprem1
	addq	$8, %rsp
	ret
.LFE1120:
	.size	op_fprem1, .-op_fprem1
	.p2align 4,,15
.globl op_fxtract
	.type	op_fxtract, @function
op_fxtract:
.LFB1119:
	subq	$8, %rsp
.LCFI129:
	call	helper_fxtract
	addq	$8, %rsp
	ret
.LFE1119:
	.size	op_fxtract, .-op_fxtract
	.p2align 4,,15
.globl op_fpatan
	.type	op_fpatan, @function
op_fpatan:
.LFB1118:
	subq	$8, %rsp
.LCFI130:
	call	helper_fpatan
	addq	$8, %rsp
	ret
.LFE1118:
	.size	op_fpatan, .-op_fpatan
	.p2align 4,,15
.globl op_fptan
	.type	op_fptan, @function
op_fptan:
.LFB1117:
	subq	$8, %rsp
.LCFI131:
	call	helper_fptan
	addq	$8, %rsp
	ret
.LFE1117:
	.size	op_fptan, .-op_fptan
	.p2align 4,,15
.globl op_fyl2x
	.type	op_fyl2x, @function
op_fyl2x:
.LFB1116:
	subq	$8, %rsp
.LCFI132:
	call	helper_fyl2x
	addq	$8, %rsp
	ret
.LFE1116:
	.size	op_fyl2x, .-op_fyl2x
	.p2align 4,,15
.globl op_f2xm1
	.type	op_f2xm1, @function
op_f2xm1:
.LFB1115:
	subq	$8, %rsp
.LCFI133:
	call	helper_f2xm1
	addq	$8, %rsp
	ret
.LFE1115:
	.size	op_f2xm1, .-op_f2xm1
	.p2align 4,,15
.globl op_fxam_ST0
	.type	op_fxam_ST0, @function
op_fxam_ST0:
.LFB1106:
	subq	$8, %rsp
.LCFI134:
	call	helper_fxam_ST0
	addq	$8, %rsp
	ret
.LFE1106:
	.size	op_fxam_ST0, .-op_fxam_ST0
	.p2align 4,,15
.globl op_fabs_ST0
	.type	op_fabs_ST0, @function
op_fabs_ST0:
.LFB1105:
	pushq	%r14
.LCFI135:
	subq	$16, %rsp
.LCFI136:
	mov	284(%rbp), %r14d
	salq	$4, %r14
	addq	%rbp, %r14
	movq	304(%r14), %rax
	movl	312(%r14), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	nocrt_fabsl
	fstpt	304(%r14)
	addq	$16, %rsp
	popq	%r14
	ret
.LFE1105:
	.size	op_fabs_ST0, .-op_fabs_ST0
	.p2align 4,,15
.globl op_fdivr_STN_ST0
	.type	op_fdivr_STN_ST0, @function
op_fdivr_STN_ST0:
.LFB1103:
	pushq	%r14
.LCFI137:
	subq	$32, %rsp
.LCFI138:
	mov	284(%rbp), %ecx
	leaq	__op_param1(%rcx), %r14
	salq	$4, %rcx
	andl	$7, %r14d
	salq	$4, %r14
	leaq	304(%r14,%rbp), %r14
	movq	(%r14), %rax
	movl	8(%r14), %edx
	movq	%rax, 16(%rsp)
	movl	%edx, 24(%rsp)
	movq	304(%rcx,%rbp), %rax
	movl	312(%rcx,%rbp), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	helper_fdiv
	fstpt	(%r14)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1103:
	.size	op_fdivr_STN_ST0, .-op_fdivr_STN_ST0
	.p2align 4,,15
.globl op_fdiv_STN_ST0
	.type	op_fdiv_STN_ST0, @function
op_fdiv_STN_ST0:
.LFB1102:
	pushq	%r14
.LCFI139:
	subq	$32, %rsp
.LCFI140:
	mov	284(%rbp), %eax
	leaq	__op_param1(%rax), %r14
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	andl	$7, %r14d
	movq	(%rdx), %rax
	salq	$4, %r14
	movl	8(%rdx), %edx
	leaq	304(%r14,%rbp), %r14
	movq	%rax, 16(%rsp)
	movl	%edx, 24(%rsp)
	movq	(%r14), %rax
	movl	8(%r14), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	helper_fdiv
	fstpt	(%r14)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1102:
	.size	op_fdiv_STN_ST0, .-op_fdiv_STN_ST0
	.p2align 4,,15
.globl op_fdivr_ST0_FT0
	.type	op_fdivr_ST0_FT0, @function
op_fdivr_ST0_FT0:
.LFB1097:
	pushq	%r14
.LCFI141:
	subq	$32, %rsp
.LCFI142:
	mov	284(%rbp), %r14d
	salq	$4, %r14
	addq	%rbp, %r14
	movq	304(%r14), %rax
	movl	312(%r14), %edx
	movq	%rax, 16(%rsp)
	movl	%edx, 24(%rsp)
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	helper_fdiv
	fstpt	304(%r14)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1097:
	.size	op_fdivr_ST0_FT0, .-op_fdivr_ST0_FT0
	.p2align 4,,15
.globl op_fdiv_ST0_FT0
	.type	op_fdiv_ST0_FT0, @function
op_fdiv_ST0_FT0:
.LFB1096:
	pushq	%r14
.LCFI143:
	subq	$32, %rsp
.LCFI144:
	mov	284(%rbp), %r14d
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	movq	%rax, 16(%rsp)
	salq	$4, %r14
	movl	%edx, 24(%rsp)
	addq	%rbp, %r14
	movq	304(%r14), %rax
	movl	312(%r14), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	helper_fdiv
	fstpt	304(%r14)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1096:
	.size	op_fdiv_ST0_FT0, .-op_fdiv_ST0_FT0
	.p2align 4,,15
.globl op_fucomi_ST0_FT0
	.type	op_fucomi_ST0_FT0, @function
op_fucomi_ST0_FT0:
.LFB1090:
	pushq	%r14
.LCFI145:
	leaq	432(%rbp), %rdi
	subq	$32, %rsp
.LCFI146:
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	movq	%rax, 16(%rsp)
	mov	284(%rbp), %eax
	movl	%edx, 24(%rsp)
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 8(%rsp)
	movq	%rax, (%rsp)
	call	floatx80_compare_quiet
	movl	%eax, %r14d
	mov	48(%rbp), %eax
	movslq	%r14d,%r14
	salq	$4, %rax
	call	*cc_table(%rax)
	andl	$-70, %eax
	orl	fcomi_ccval+4(,%r14,4), %eax
	movl	%eax, 40(%rbp)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1090:
	.size	op_fucomi_ST0_FT0, .-op_fucomi_ST0_FT0
	.p2align 4,,15
.globl op_fucom_ST0_FT0
	.type	op_fucom_ST0_FT0, @function
op_fucom_ST0_FT0:
.LFB1088:
	subq	$40, %rsp
.LCFI147:
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	leaq	432(%rbp), %rdi
	movq	%rax, 16(%rsp)
	mov	284(%rbp), %eax
	movl	%edx, 24(%rsp)
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 8(%rsp)
	movq	%rax, (%rsp)
	call	floatx80_compare_quiet
	movl	288(%rbp), %edx
	cltq
	andb	$186, %dh
	orl	fcom_ccval+4(,%rax,4), %edx
	movl	%edx, 288(%rbp)
	addq	$40, %rsp
	ret
.LFE1088:
	.size	op_fucom_ST0_FT0, .-op_fucom_ST0_FT0
	.p2align 4,,15
.globl op_fcomi_ST0_FT0
	.type	op_fcomi_ST0_FT0, @function
op_fcomi_ST0_FT0:
.LFB1089:
	pushq	%r14
.LCFI148:
	leaq	432(%rbp), %rdi
	subq	$32, %rsp
.LCFI149:
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	movq	%rax, 16(%rsp)
	mov	284(%rbp), %eax
	movl	%edx, 24(%rsp)
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 8(%rsp)
	movq	%rax, (%rsp)
	call	floatx80_compare
	movl	%eax, %r14d
	mov	48(%rbp), %eax
	movslq	%r14d,%r14
	salq	$4, %rax
	call	*cc_table(%rax)
	andl	$-70, %eax
	orl	fcomi_ccval+4(,%r14,4), %eax
	movl	%eax, 40(%rbp)
	addq	$32, %rsp
	popq	%r14
	ret
.LFE1089:
	.size	op_fcomi_ST0_FT0, .-op_fcomi_ST0_FT0
	.p2align 4,,15
.globl op_fcom_ST0_FT0
	.type	op_fcom_ST0_FT0, @function
op_fcom_ST0_FT0:
.LFB1087:
	subq	$40, %rsp
.LCFI150:
	movq	448(%rbp), %rax
	movl	456(%rbp), %edx
	leaq	432(%rbp), %rdi
	movq	%rax, 16(%rsp)
	mov	284(%rbp), %eax
	movl	%edx, 24(%rsp)
	salq	$4, %rax
	movl	312(%rax,%rbp), %edx
	movq	304(%rax,%rbp), %rax
	movl	%edx, 8(%rsp)
	movq	%rax, (%rsp)
	call	floatx80_compare
	movl	288(%rbp), %edx
	cltq
	andb	$186, %dh
	orl	fcom_ccval+4(,%rax,4), %edx
	movl	%edx, 288(%rbp)
	addq	$40, %rsp
	ret
.LFE1087:
	.size	op_fcom_ST0_FT0, .-op_fcom_ST0_FT0
	.p2align 4,,15
.globl op_fbst_ST0_A0
	.type	op_fbst_ST0_A0, @function
op_fbst_ST0_A0:
.LFB1076:
	subq	$8, %rsp
.LCFI151:
	call	helper_fbst_ST0_A0
	addq	$8, %rsp
	ret
.LFE1076:
	.size	op_fbst_ST0_A0, .-op_fbst_ST0_A0
	.p2align 4,,15
.globl op_fbld_ST0_A0
	.type	op_fbld_ST0_A0, @function
op_fbld_ST0_A0:
.LFB1075:
	subq	$8, %rsp
.LCFI152:
	call	helper_fbld_ST0_A0
	addq	$8, %rsp
	ret
.LFE1075:
	.size	op_fbld_ST0_A0, .-op_fbld_ST0_A0
	.p2align 4,,15
.globl op_fstl_ST0_A0
	.type	op_fstl_ST0_A0, @function
op_fstl_ST0_A0:
.LFB1067:
	subq	$8, %rsp
.LCFI153:
	mov	284(%rbp), %eax
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	andl	$3, %edx
	salq	$4, %rax
	fldt	304(%rax,%rbp)
	movl	%r13d, %eax
	shrl	$12, %eax
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	movslq	%r8d,%rdx
	salq	$5, %rax
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	fstpl	(%rsp)
	cmpl	%eax, 828(%rdx)
	movq	(%rsp), %r9
	je	.L1909
	movl	%r8d, %edx
	movq	%r9, %rsi
	call	__stq_mmu
	jmp	.L1911
	.p2align 4,,7
.L1909:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movq	%r9, %rsi
	call	remR3PhysWriteU64
.L1911:
	addq	$8, %rsp
	ret
.LFE1067:
	.size	op_fstl_ST0_A0, .-op_fstl_ST0_A0
	.p2align 4,,15
.globl op_stq_raw_env_A0
	.type	op_stq_raw_env_A0, @function
op_stq_raw_env_A0:
.LFB577:
	subq	$8, %rsp
.LCFI154:
	movq	__op_param1(%rbp), %rsi
	mov	%r13d, %edi
	call	remR3PhysWriteU64
	addq	$8, %rsp
	ret
.LFE577:
	.size	op_stq_raw_env_A0, .-op_stq_raw_env_A0
	.p2align 4,,15
.globl op_fsts_ST0_A0
	.type	op_fsts_ST0_A0, @function
op_fsts_ST0_A0:
.LFB1066:
	subq	$8, %rsp
.LCFI155:
	mov	284(%rbp), %eax
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	andl	$3, %edx
	salq	$4, %rax
	fldt	304(%rax,%rbp)
	movl	%r13d, %eax
	shrl	$12, %eax
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	movslq	%r8d,%rdx
	salq	$5, %rax
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	fstps	4(%rsp)
	cmpl	%eax, 828(%rdx)
	movl	4(%rsp), %r9d
	je	.L1916
	movl	%r8d, %edx
	movl	%r9d, %esi
	call	__stl_mmu
	jmp	.L1918
	.p2align 4,,7
.L1916:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movl	%r9d, %esi
	call	remR3PhysWriteU32
.L1918:
	addq	$8, %rsp
	ret
.LFE1066:
	.size	op_fsts_ST0_A0, .-op_fsts_ST0_A0
	.p2align 4,,15
.globl op_stl_user_T1_A0
	.type	op_stl_user_T1_A0, @function
op_stl_user_T1_A0:
.LFB613:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI156:
	movl	%r12d, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L1921
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L1923
	.p2align 4,,7
.L1921:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L1923:
	addq	$8, %rsp
	ret
.LFE613:
	.size	op_stl_user_T1_A0, .-op_stl_user_T1_A0
	.p2align 4,,15
.globl op_sbbl_kernel_T0_T1_cc
	.type	op_sbbl_kernel_T0_T1_cc, @function
op_sbbl_kernel_T0_T1_cc:
.LFB924:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI157:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %esi
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %esi
	subl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L1926
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L1928
	.p2align 4,,7
.L1926:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L1928:
	leal	16(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE924:
	.size	op_sbbl_kernel_T0_T1_cc, .-op_sbbl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_stl_raw_T0_A0
	.type	op_stl_raw_T0_A0, @function
op_stl_raw_T0_A0:
.LFB573:
	subq	$8, %rsp
.LCFI158:
	mov	%r13d, %edi
	movl	%ebx, %esi
	call	remR3PhysWriteU32
	addq	$8, %rsp
	ret
.LFE573:
	.size	op_stl_raw_T0_A0, .-op_stl_raw_T0_A0
	.p2align 4,,15
.globl op_fstt_ST0_A0
	.type	op_fstt_ST0_A0, @function
op_fstt_ST0_A0:
.LFB1068:
	subq	$8, %rsp
.LCFI159:
	call	helper_fstt_ST0_A0
	addq	$8, %rsp
	ret
.LFE1068:
	.size	op_fstt_ST0_A0, .-op_fstt_ST0_A0
	.p2align 4,,15
.globl op_ldq_user_env_A0
	.type	op_ldq_user_env_A0, @function
op_ldq_user_env_A0:
.LFB614:
	movl	%r13d, %eax
	pushq	%r14
.LCFI160:
	movl	%r13d, %edi
	shrl	$12, %eax
	movq	%rbp, %r14
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L1935
	movl	$1, %esi
	call	__ldq_mmu
	jmp	.L1937
	.p2align 4,,7
.L1935:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU64
.L1937:
	movq	%rax, __op_param1(%r14)
	popq	%r14
	ret
.LFE614:
	.size	op_ldq_user_env_A0, .-op_ldq_user_env_A0
	.p2align 4,,15
.globl op_ldq_raw_env_A0
	.type	op_ldq_raw_env_A0, @function
op_ldq_raw_env_A0:
.LFB576:
	pushq	%r14
.LCFI161:
	leaq	__op_param1(%rbp), %r14
	mov	%r13d, %edi
	call	remR3PhysReadU64
	movq	%rax, (%r14)
	popq	%r14
	ret
.LFE576:
	.size	op_ldq_raw_env_A0, .-op_ldq_raw_env_A0
	.p2align 4,,15
.globl op_ldl_user_T0_A0
	.type	op_ldl_user_T0_A0, @function
op_ldl_user_T0_A0:
.LFB603:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI162:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L1942
	movl	$1, %esi
	call	__ldl_mmu
	jmp	.L1944
	.p2align 4,,7
.L1942:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU32
.L1944:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE603:
	.size	op_ldl_user_T0_A0, .-op_ldl_user_T0_A0
	.p2align 4,,15
.globl op_ldl_kernel_T0_A0
	.type	op_ldl_kernel_T0_A0, @function
op_ldl_kernel_T0_A0:
.LFB584:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI163:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L1947
	xorl	%esi, %esi
	call	__ldl_mmu
	jmp	.L1949
	.p2align 4,,7
.L1947:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
.L1949:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE584:
	.size	op_ldl_kernel_T0_A0, .-op_ldl_kernel_T0_A0
	.p2align 4,,15
.globl op_ldl_raw_T1_A0
	.type	op_ldl_raw_T1_A0, @function
op_ldl_raw_T1_A0:
.LFB570:
	subq	$8, %rsp
.LCFI164:
	mov	%r13d, %edi
	call	remR3PhysReadU32
	addq	$8, %rsp
	movl	%eax, %r12d
	ret
.LFE570:
	.size	op_ldl_raw_T1_A0, .-op_ldl_raw_T1_A0
	.p2align 4,,15
.globl op_ldsw_user_T0_A0
	.type	op_ldsw_user_T0_A0, @function
op_ldsw_user_T0_A0:
.LFB602:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI165:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L1954
	movl	$1, %esi
	call	__ldw_mmu
	cwtl
	jmp	.L1956
	.p2align 4,,7
.L1954:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadS16
	cwtl
.L1956:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE602:
	.size	op_ldsw_user_T0_A0, .-op_ldsw_user_T0_A0
	.p2align 4,,15
.globl op_ldsw_kernel_T0_A0
	.type	op_ldsw_kernel_T0_A0, @function
op_ldsw_kernel_T0_A0:
.LFB583:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI166:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L1959
	xorl	%esi, %esi
	call	__ldw_mmu
	cwtl
	jmp	.L1961
	.p2align 4,,7
.L1959:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	cwtl
.L1961:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE583:
	.size	op_ldsw_kernel_T0_A0, .-op_ldsw_kernel_T0_A0
	.p2align 4,,15
.globl op_ldsw_raw_T0_A0
	.type	op_ldsw_raw_T0_A0, @function
op_ldsw_raw_T0_A0:
.LFB564:
	subq	$8, %rsp
.LCFI167:
	mov	%r13d, %edi
	call	remR3PhysReadS16
	addq	$8, %rsp
	movswl	%ax,%ebx
	ret
.LFE564:
	.size	op_ldsw_raw_T0_A0, .-op_ldsw_raw_T0_A0
	.p2align 4,,15
.globl op_fldt_ST0_A0
	.type	op_fldt_ST0_A0, @function
op_fldt_ST0_A0:
.LFB1062:
	subq	$8, %rsp
.LCFI168:
	call	helper_fldt_ST0_A0
	addq	$8, %rsp
	ret
.LFE1062:
	.size	op_fldt_ST0_A0, .-op_fldt_ST0_A0
	.p2align 4,,15
.globl op_invlpg_A0
	.type	op_invlpg_A0, @function
op_invlpg_A0:
.LFB1014:
	subq	$8, %rsp
.LCFI169:
	movl	%r13d, %edi
	call	helper_invlpg
	addq	$8, %rsp
	ret
.LFE1014:
	.size	op_invlpg_A0, .-op_invlpg_A0
	.p2align 4,,15
.globl op_lmsw_T0
	.type	op_lmsw_T0, @function
op_lmsw_T0:
.LFB1013:
	subq	$8, %rsp
.LCFI170:
	movl	260(%rbp), %edx
	movl	%ebx, %eax
	andl	$15, %eax
	xorl	%edi, %edi
	andl	$-15, %edx
	movl	%edx, %ebx
	orl	%eax, %ebx
	call	helper_movl_crN_T0
	addq	$8, %rsp
	ret
.LFE1013:
	.size	op_lmsw_T0, .-op_lmsw_T0
	.p2align 4,,15
.globl op_movl_crN_T0
	.type	op_movl_crN_T0, @function
op_movl_crN_T0:
.LFB1010:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI171:
	call	helper_movl_crN_T0
	addq	$8, %rsp
	ret
.LFE1010:
	.size	op_movl_crN_T0, .-op_movl_crN_T0
	.p2align 4,,15
.globl op_movl_drN_T0
	.type	op_movl_drN_T0, @function
op_movl_drN_T0:
.LFB1012:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI172:
	call	helper_movl_drN_T0
	addq	$8, %rsp
	ret
.LFE1012:
	.size	op_movl_drN_T0, .-op_movl_drN_T0
	.p2align 4,,15
.globl op_movtl_T0_cr8
	.type	op_movtl_T0_cr8, @function
op_movtl_T0_cr8:
.LFB1011:
	subq	$8, %rsp
.LCFI173:
	movq	%rbp, %rdi
	call	cpu_get_apic_tpr
	addq	$8, %rsp
	movzbl	%al, %ebx
	ret
.LFE1011:
	.size	op_movtl_T0_cr8, .-op_movtl_T0_cr8
	.p2align 4,,15
.globl op_ltr_T0
	.type	op_ltr_T0, @function
op_ltr_T0:
.LFB1009:
	subq	$8, %rsp
.LCFI174:
	call	helper_ltr_T0
	addq	$8, %rsp
	ret
.LFE1009:
	.size	op_ltr_T0, .-op_ltr_T0
	.p2align 4,,15
.globl op_lldt_T0
	.type	op_lldt_T0, @function
op_lldt_T0:
.LFB1008:
	subq	$8, %rsp
.LCFI175:
	call	helper_lldt_T0
	addq	$8, %rsp
	ret
.LFE1008:
	.size	op_lldt_T0, .-op_lldt_T0
	.p2align 4,,15
.globl op_lret_protected
	.type	op_lret_protected, @function
op_lret_protected:
.LFB1007:
	leal	__op_param2(%rip), %esi
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI176:
	call	helper_lret_protected
	addq	$8, %rsp
	ret
.LFE1007:
	.size	op_lret_protected, .-op_lret_protected
	.p2align 4,,15
.globl op_iret_protected
	.type	op_iret_protected, @function
op_iret_protected:
.LFB1006:
	leal	__op_param2(%rip), %esi
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI177:
	call	helper_iret_protected
	addq	$8, %rsp
	ret
.LFE1006:
	.size	op_iret_protected, .-op_iret_protected
	.p2align 4,,15
.globl op_iret_real
	.type	op_iret_real, @function
op_iret_real:
.LFB1005:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI178:
	call	helper_iret_real
	addq	$8, %rsp
	ret
.LFE1005:
	.size	op_iret_real, .-op_iret_real
	.p2align 4,,15
.globl op_lcall_protected_T0_T1
	.type	op_lcall_protected_T0_T1, @function
op_lcall_protected_T0_T1:
.LFB1004:
	leal	__op_param2(%rip), %esi
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI179:
	call	helper_lcall_protected_T0_T1
	addq	$8, %rsp
	ret
.LFE1004:
	.size	op_lcall_protected_T0_T1, .-op_lcall_protected_T0_T1
	.p2align 4,,15
.globl op_lcall_real_T0_T1
	.type	op_lcall_real_T0_T1, @function
op_lcall_real_T0_T1:
.LFB1003:
	leal	__op_param2(%rip), %esi
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI180:
	call	helper_lcall_real_T0_T1
	addq	$8, %rsp
	ret
.LFE1003:
	.size	op_lcall_real_T0_T1, .-op_lcall_real_T0_T1
	.p2align 4,,15
.globl op_ljmp_protected_T0_T1
	.type	op_ljmp_protected_T0_T1, @function
op_ljmp_protected_T0_T1:
.LFB1002:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI181:
	call	helper_ljmp_protected_T0_T1
	addq	$8, %rsp
	ret
.LFE1002:
	.size	op_ljmp_protected_T0_T1, .-op_ljmp_protected_T0_T1
	.p2align 4,,15
.globl op_verw
	.type	op_verw, @function
op_verw:
.LFB999:
	subq	$8, %rsp
.LCFI182:
	call	helper_verw
	addq	$8, %rsp
	ret
.LFE999:
	.size	op_verw, .-op_verw
	.p2align 4,,15
.globl op_verr
	.type	op_verr, @function
op_verr:
.LFB998:
	subq	$8, %rsp
.LCFI183:
	call	helper_verr
	addq	$8, %rsp
	ret
.LFE998:
	.size	op_verr, .-op_verr
	.p2align 4,,15
.globl op_lar
	.type	op_lar, @function
op_lar:
.LFB997:
	subq	$8, %rsp
.LCFI184:
	call	helper_lar
	addq	$8, %rsp
	ret
.LFE997:
	.size	op_lar, .-op_lar
	.p2align 4,,15
.globl op_lsl
	.type	op_lsl, @function
op_lsl:
.LFB996:
	subq	$8, %rsp
.LCFI185:
	call	helper_lsl
	addq	$8, %rsp
	ret
.LFE996:
	.size	op_lsl, .-op_lsl
	.p2align 4,,15
.globl op_movl_seg_T0
	.type	op_movl_seg_T0, @function
op_movl_seg_T0:
.LFB993:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI186:
	movl	%ebx, %esi
	call	load_seg
	addq	$8, %rsp
	ret
.LFE993:
	.size	op_movl_seg_T0, .-op_movl_seg_T0
	.p2align 4,,15
.globl op_wrmsr
	.type	op_wrmsr, @function
op_wrmsr:
.LFB986:
	subq	$8, %rsp
.LCFI187:
	call	helper_wrmsr
	addq	$8, %rsp
	ret
.LFE986:
	.size	op_wrmsr, .-op_wrmsr
	.p2align 4,,15
.globl op_rdmsr
	.type	op_rdmsr, @function
op_rdmsr:
.LFB985:
	subq	$8, %rsp
.LCFI188:
	call	helper_rdmsr
	addq	$8, %rsp
	ret
.LFE985:
	.size	op_rdmsr, .-op_rdmsr
	.p2align 4,,15
.globl op_sysexit
	.type	op_sysexit, @function
op_sysexit:
.LFB984:
	subq	$8, %rsp
.LCFI189:
	call	helper_sysexit
	addq	$8, %rsp
	ret
.LFE984:
	.size	op_sysexit, .-op_sysexit
	.p2align 4,,15
.globl op_sysenter
	.type	op_sysenter, @function
op_sysenter:
.LFB983:
	subq	$8, %rsp
.LCFI190:
	call	helper_sysenter
	addq	$8, %rsp
	ret
.LFE983:
	.size	op_sysenter, .-op_sysenter
	.p2align 4,,15
.globl op_enter_level
	.type	op_enter_level, @function
op_enter_level:
.LFB982:
	leal	__op_param2(%rip), %esi
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI191:
	call	helper_enter_level
	addq	$8, %rsp
	ret
.LFE982:
	.size	op_enter_level, .-op_enter_level
	.p2align 4,,15
.globl op_cpuid
	.type	op_cpuid, @function
op_cpuid:
.LFB981:
	subq	$8, %rsp
.LCFI192:
	call	helper_cpuid
	addq	$8, %rsp
	ret
.LFE981:
	.size	op_cpuid, .-op_cpuid
	.p2align 4,,15
.globl op_rdtsc
	.type	op_rdtsc, @function
op_rdtsc:
.LFB980:
	subq	$8, %rsp
.LCFI193:
	call	helper_rdtsc
	addq	$8, %rsp
	ret
.LFE980:
	.size	op_rdtsc, .-op_rdtsc
	.p2align 4,,15
.globl op_check_iol_DX
	.type	op_check_iol_DX, @function
op_check_iol_DX:
.LFB956:
	subq	$8, %rsp
.LCFI194:
	call	check_iol_DX
	addq	$8, %rsp
	ret
.LFE956:
	.size	op_check_iol_DX, .-op_check_iol_DX
	.p2align 4,,15
.globl op_check_iol_T0
	.type	op_check_iol_T0, @function
op_check_iol_T0:
.LFB955:
	subq	$8, %rsp
.LCFI195:
	call	check_iol_T0
	addq	$8, %rsp
	ret
.LFE955:
	.size	op_check_iol_T0, .-op_check_iol_T0
	.p2align 4,,15
.globl op_outl_DX_T0
	.type	op_outl_DX_T0, @function
op_outl_DX_T0:
.LFB954:
	subq	$8, %rsp
.LCFI196:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	movl	%ebx, %edx
	call	cpu_outl
	addq	$8, %rsp
	ret
.LFE954:
	.size	op_outl_DX_T0, .-op_outl_DX_T0
	.p2align 4,,15
.globl op_outl_T0_T1
	.type	op_outl_T0_T1, @function
op_outl_T0_T1:
.LFB951:
	subq	$8, %rsp
.LCFI197:
	movl	%r12d, %edx
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_outl
	addq	$8, %rsp
	ret
.LFE951:
	.size	op_outl_T0_T1, .-op_outl_T0_T1
	.p2align 4,,15
.globl op_inl_DX_T0
	.type	op_inl_DX_T0, @function
op_inl_DX_T0:
.LFB953:
	subq	$8, %rsp
.LCFI198:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	call	cpu_inl
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE953:
	.size	op_inl_DX_T0, .-op_inl_DX_T0
	.p2align 4,,15
.globl op_inl_T0_T1
	.type	op_inl_T0_T1, @function
op_inl_T0_T1:
.LFB952:
	subq	$8, %rsp
.LCFI199:
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_inl
	addq	$8, %rsp
	movl	%eax, %r12d
	ret
.LFE952:
	.size	op_inl_T0_T1, .-op_inl_T0_T1
	.p2align 4,,15
.globl op_check_iow_DX
	.type	op_check_iow_DX, @function
op_check_iow_DX:
.LFB838:
	subq	$8, %rsp
.LCFI200:
	call	check_iow_DX
	addq	$8, %rsp
	ret
.LFE838:
	.size	op_check_iow_DX, .-op_check_iow_DX
	.p2align 4,,15
.globl op_check_iow_T0
	.type	op_check_iow_T0, @function
op_check_iow_T0:
.LFB837:
	subq	$8, %rsp
.LCFI201:
	call	check_iow_T0
	addq	$8, %rsp
	ret
.LFE837:
	.size	op_check_iow_T0, .-op_check_iow_T0
	.p2align 4,,15
.globl op_outw_DX_T0
	.type	op_outw_DX_T0, @function
op_outw_DX_T0:
.LFB836:
	subq	$8, %rsp
.LCFI202:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	movl	%ebx, %edx
	call	cpu_outw
	addq	$8, %rsp
	ret
.LFE836:
	.size	op_outw_DX_T0, .-op_outw_DX_T0
	.p2align 4,,15
.globl op_outw_T0_T1
	.type	op_outw_T0_T1, @function
op_outw_T0_T1:
.LFB833:
	subq	$8, %rsp
.LCFI203:
	movzwl	%r12w,%edx
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_outw
	addq	$8, %rsp
	ret
.LFE833:
	.size	op_outw_T0_T1, .-op_outw_T0_T1
	.p2align 4,,15
.globl op_inw_DX_T0
	.type	op_inw_DX_T0, @function
op_inw_DX_T0:
.LFB835:
	subq	$8, %rsp
.LCFI204:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	call	cpu_inw
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE835:
	.size	op_inw_DX_T0, .-op_inw_DX_T0
	.p2align 4,,15
.globl op_inw_T0_T1
	.type	op_inw_T0_T1, @function
op_inw_T0_T1:
.LFB834:
	subq	$8, %rsp
.LCFI205:
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_inw
	addq	$8, %rsp
	movl	%eax, %r12d
	ret
.LFE834:
	.size	op_inw_T0_T1, .-op_inw_T0_T1
	.p2align 4,,15
.globl op_check_iob_DX
	.type	op_check_iob_DX, @function
op_check_iob_DX:
.LFB724:
	subq	$8, %rsp
.LCFI206:
	call	check_iob_DX
	addq	$8, %rsp
	ret
.LFE724:
	.size	op_check_iob_DX, .-op_check_iob_DX
	.p2align 4,,15
.globl op_check_iob_T0
	.type	op_check_iob_T0, @function
op_check_iob_T0:
.LFB723:
	subq	$8, %rsp
.LCFI207:
	call	check_iob_T0
	addq	$8, %rsp
	ret
.LFE723:
	.size	op_check_iob_T0, .-op_check_iob_T0
	.p2align 4,,15
.globl op_outb_DX_T0
	.type	op_outb_DX_T0, @function
op_outb_DX_T0:
.LFB722:
	subq	$8, %rsp
.LCFI208:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	movl	%ebx, %edx
	call	cpu_outb
	addq	$8, %rsp
	ret
.LFE722:
	.size	op_outb_DX_T0, .-op_outb_DX_T0
	.p2align 4,,15
.globl op_outb_T0_T1
	.type	op_outb_T0_T1, @function
op_outb_T0_T1:
.LFB719:
	subq	$8, %rsp
.LCFI209:
	movzbl	%r12b,%edx
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_outb
	addq	$8, %rsp
	ret
.LFE719:
	.size	op_outb_T0_T1, .-op_outb_T0_T1
	.p2align 4,,15
.globl op_inb_DX_T0
	.type	op_inb_DX_T0, @function
op_inb_DX_T0:
.LFB721:
	subq	$8, %rsp
.LCFI210:
	movzwl	8(%rbp),%esi
	movq	%rbp, %rdi
	call	cpu_inb
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE721:
	.size	op_inb_DX_T0, .-op_inb_DX_T0
	.p2align 4,,15
.globl op_inb_T0_T1
	.type	op_inb_T0_T1, @function
op_inb_T0_T1:
.LFB720:
	subq	$8, %rsp
.LCFI211:
	movl	%ebx, %esi
	movq	%rbp, %rdi
	call	cpu_inb
	addq	$8, %rsp
	movl	%eax, %r12d
	ret
.LFE720:
	.size	op_inb_T0_T1, .-op_inb_T0_T1
	.p2align 4,,15
.globl op_check_external_event
	.type	op_check_external_event, @function
op_check_external_event:
.LFB636:
	subq	$8, %rsp
.LCFI212:
	movl	788(%rbp), %eax
	testw	$-12288, %ax
	jne	.L2054
	testb	$32, %ah
	je	.L2059
	testb	$2, 37(%rbp)
	je	.L2059
	testb	$8, 56(%rbp)
	.p2align 4,,5
	jne	.L2059
.L2054:
	.p2align 4,,7
	call	helper_external_event
.L2059:
	addq	$8, %rsp
	.p2align 4,,4
	ret
.LFE636:
	.size	op_check_external_event, .-op_check_external_event
	.p2align 4,,15
.globl op_cmpxchg8b
	.type	op_cmpxchg8b, @function
op_cmpxchg8b:
.LFB634:
	subq	$8, %rsp
.LCFI213:
	call	helper_cmpxchg8b
	addq	$8, %rsp
	ret
.LFE634:
	.size	op_cmpxchg8b, .-op_cmpxchg8b
	.p2align 4,,15
.globl op_raise_exception
	.type	op_raise_exception, @function
op_raise_exception:
.LFB625:
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI214:
	call	raise_exception
	addq	$8, %rsp
	ret
.LFE625:
	.size	op_raise_exception, .-op_raise_exception
	.p2align 4,,15
.globl op_idivw_AX_T0
	.type	op_idivw_AX_T0, @function
op_idivw_AX_T0:
.LFB542:
	movq	%r14, -16(%rsp)
.LCFI215:
	movq	%r15, -8(%rsp)
.LCFI216:
	subq	$24, %rsp
.LCFI217:
	movzwl	8(%rbp),%eax
	movzwl	(%rbp),%r14d
	movswl	%bx,%r15d
	sall	$16, %eax
	orl	%eax, %r14d
	testw	%bx, %bx
	jne	.L2065
	xorl	%edi, %edi
	call	raise_exception
	.p2align 4,,7
.L2065:
	movl	%r14d, %eax
	cltd
	idivl	%r15d
	movl	%eax, %r14d
	cwtl
	movl	%edx, %r15d
	cmpl	%eax, %r14d
	je	.L2067
	xorl	%edi, %edi
	call	raise_exception
.L2067:
	movw	%r14w, (%rbp)
	movw	%r15w, 8(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE542:
	.size	op_idivw_AX_T0, .-op_idivw_AX_T0
	.p2align 4,,15
.globl op_divw_AX_T0
	.type	op_divw_AX_T0, @function
op_divw_AX_T0:
.LFB541:
	movq	%r14, -16(%rsp)
.LCFI218:
	movq	%r15, -8(%rsp)
.LCFI219:
	subq	$24, %rsp
.LCFI220:
	movzwl	8(%rbp),%eax
	movzwl	(%rbp),%r14d
	movl	%ebx, %r15d
	sall	$16, %eax
	orl	%eax, %r14d
	andl	$65535, %r15d
	jne	.L2071
	xorl	%edi, %edi
	call	raise_exception
	.p2align 4,,7
.L2071:
	movl	%r14d, %eax
	xorl	%edx, %edx
	divl	%r15d
	cmpl	$65535, %eax
	movl	%edx, %r15d
	movl	%eax, %r14d
	jbe	.L2073
	xorl	%edi, %edi
	call	raise_exception
.L2073:
	movw	%r14w, (%rbp)
	movw	%r15w, 8(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE541:
	.size	op_divw_AX_T0, .-op_divw_AX_T0
	.p2align 4,,15
.globl op_idivb_AL_T0
	.type	op_idivb_AL_T0, @function
op_idivb_AL_T0:
.LFB540:
	movq	%r14, -16(%rsp)
.LCFI221:
	movq	%r15, -8(%rsp)
.LCFI222:
	subq	$24, %rsp
.LCFI223:
	testb	%bl, %bl
	movswl	(%rbp),%r15d
	movsbl	%bl,%r14d
	jne	.L2077
	xorl	%edi, %edi
	call	raise_exception
	.p2align 4,,7
.L2077:
	movl	%r15d, %eax
	cltd
	idivl	%r14d
	movl	%eax, %r14d
	movsbl	%al,%eax
	movl	%edx, %r15d
	cmpl	%eax, %r14d
	je	.L2079
	xorl	%edi, %edi
	call	raise_exception
.L2079:
	movl	(%rbp), %eax
	movzbl	%r15b,%edx
	movzbl	%r14b,%ecx
	sall	$8, %edx
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	xorw	%ax, %ax
	orl	%ecx, %eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
	addq	$24, %rsp
	ret
.LFE540:
	.size	op_idivb_AL_T0, .-op_idivb_AL_T0
	.p2align 4,,15
.globl op_divb_AL_T0
	.type	op_divb_AL_T0, @function
op_divb_AL_T0:
.LFB539:
	movq	%r14, -16(%rsp)
.LCFI224:
	movl	%ebx, %r14d
	movq	%r15, -8(%rsp)
.LCFI225:
	subq	$24, %rsp
.LCFI226:
	movzwl	(%rbp),%r15d
	andl	$255, %r14d
	jne	.L2083
	xorl	%edi, %edi
	call	raise_exception
	.p2align 4,,7
.L2083:
	movl	%r15d, %eax
	xorl	%edx, %edx
	divl	%r14d
	cmpl	$255, %eax
	movl	%edx, %r14d
	movl	%eax, %r15d
	jbe	.L2085
	xorl	%edi, %edi
	call	raise_exception
.L2085:
	movl	(%rbp), %ecx
	movzbl	%r15b,%edx
	sall	$8, %r14d
	movq	16(%rsp), %r15
	xorw	%cx, %cx
	orl	%ecx, %edx
	orl	%edx, %r14d
	movl	%r14d, (%rbp)
	movq	8(%rsp), %r14
	addq	$24, %rsp
	ret
.LFE539:
	.size	op_divb_AL_T0, .-op_divb_AL_T0
	.p2align 4,,15
.globl op_rsm
	.type	op_rsm, @function
op_rsm:
.LFB631:
	subq	$8, %rsp
.LCFI227:
	call	helper_rsm
	addq	$8, %rsp
	ret
.LFE631:
	.size	op_rsm, .-op_rsm
	.p2align 4,,15
.globl op_into
	.type	op_into, @function
op_into:
.LFB626:
	subq	$8, %rsp
.LCFI228:
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	testb	$8, %ah
	je	.L2091
	leal	__op_param1(%rip), %ecx
	xorl	%edx, %edx
	movl	$1, %esi
	movl	$4, %edi
	call	raise_interrupt
.L2091:
	addq	$8, %rsp
	ret
.LFE626:
	.size	op_into, .-op_into
	.p2align 4,,15
.globl op_raise_interrupt
	.type	op_raise_interrupt, @function
op_raise_interrupt:
.LFB624:
	leal	__op_param2(%rip), %ecx
	leal	__op_param1(%rip), %edi
	subq	$8, %rsp
.LCFI229:
	xorl	%edx, %edx
	movl	$1, %esi
	call	raise_interrupt
	addq	$8, %rsp
	ret
.LFE624:
	.size	op_raise_interrupt, .-op_raise_interrupt
	.p2align 4,,15
.globl op_debug
	.type	op_debug, @function
op_debug:
.LFB623:
	subq	$8, %rsp
.LCFI230:
	movl	$65538, 736(%rbp)
	call	cpu_loop_exit
	addq	$8, %rsp
	ret
.LFE623:
	.size	op_debug, .-op_debug
	.p2align 4,,15
.globl op_mwait
	.type	op_mwait, @function
op_mwait:
.LFB622:
	subq	$8, %rsp
.LCFI231:
	call	helper_mwait
	addq	$8, %rsp
	ret
.LFE622:
	.size	op_mwait, .-op_mwait
	.p2align 4,,15
.globl op_monitor
	.type	op_monitor, @function
op_monitor:
.LFB621:
	subq	$8, %rsp
.LCFI232:
	call	helper_monitor
	addq	$8, %rsp
	ret
.LFE621:
	.size	op_monitor, .-op_monitor
	.p2align 4,,15
.globl op_hlt
	.type	op_hlt, @function
op_hlt:
.LFB620:
	subq	$8, %rsp
.LCFI233:
	call	helper_hlt
	addq	$8, %rsp
	ret
.LFE620:
	.size	op_hlt, .-op_hlt
	.p2align 4,,15
.globl op_ldsb_user_T0_A0
	.type	op_ldsb_user_T0_A0, @function
op_ldsb_user_T0_A0:
.LFB600:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI234:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L2105
	movl	$1, %esi
	call	__ldb_mmu
	movsbl	%al,%eax
	jmp	.L2107
	.p2align 4,,7
.L2105:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadS8
	movsbl	%al,%eax
.L2107:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE600:
	.size	op_ldsb_user_T0_A0, .-op_ldsb_user_T0_A0
	.p2align 4,,15
.globl op_ldsb_kernel_T0_A0
	.type	op_ldsb_kernel_T0_A0, @function
op_ldsb_kernel_T0_A0:
.LFB581:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI235:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 824(%rdx)
	je	.L2110
	xorl	%esi, %esi
	call	__ldb_mmu
	movsbl	%al,%eax
	jmp	.L2112
	.p2align 4,,7
.L2110:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS8
	movsbl	%al,%eax
.L2112:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE581:
	.size	op_ldsb_kernel_T0_A0, .-op_ldsb_kernel_T0_A0
	.p2align 4,,15
.globl op_ldsb_raw_T0_A0
	.type	op_ldsb_raw_T0_A0, @function
op_ldsb_raw_T0_A0:
.LFB562:
	subq	$8, %rsp
.LCFI236:
	mov	%r13d, %edi
	call	remR3PhysReadS8
	addq	$8, %rsp
	movsbl	%al,%ebx
	ret
.LFE562:
	.size	op_ldsb_raw_T0_A0, .-op_ldsb_raw_T0_A0
	.p2align 4,,15
.globl op_ldub_user_T0_A0
	.type	op_ldub_user_T0_A0, @function
op_ldub_user_T0_A0:
.LFB599:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI237:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L2117
	movl	$1, %esi
	call	__ldb_mmu
	movzbl	%al, %eax
	jmp	.L2119
	.p2align 4,,7
.L2117:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU8
	movzbl	%al, %eax
.L2119:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE599:
	.size	op_ldub_user_T0_A0, .-op_ldub_user_T0_A0
	.p2align 4,,15
.globl op_ldub_kernel_T1_A0
	.type	op_ldub_kernel_T1_A0, @function
op_ldub_kernel_T1_A0:
.LFB585:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI238:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 824(%rdx)
	je	.L2122
	xorl	%esi, %esi
	call	__ldb_mmu
	movzbl	%al, %eax
	jmp	.L2124
	.p2align 4,,7
.L2122:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU8
	movzbl	%al, %eax
.L2124:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE585:
	.size	op_ldub_kernel_T1_A0, .-op_ldub_kernel_T1_A0
	.p2align 4,,15
.globl op_ldub_raw_T0_A0
	.type	op_ldub_raw_T0_A0, @function
op_ldub_raw_T0_A0:
.LFB561:
	subq	$8, %rsp
.LCFI239:
	mov	%r13d, %edi
	call	remR3PhysReadU8
	addq	$8, %rsp
	movzbl	%al, %ebx
	ret
.LFE561:
	.size	op_ldub_raw_T0_A0, .-op_ldub_raw_T0_A0
	.p2align 4,,15
.globl op_addl_A0_seg
	.type	op_addl_A0_seg, @function
op_addl_A0_seg:
.LFB558:
	movl	$__op_param1-64, %ecx
	movabsq	$-3689348814741910323, %rax
	pushq	%r14
.LCFI240:
	mulq	%rcx
	movq	%rbp, %rdi
	movq	%rdx, %rcx
	shrq	$4, %rcx
	mov	%ecx, %r14d
	movq	%r14, %rax
	salq	$4, %rax
	leaq	(%rax,%r14,4), %rax
	movl	76(%rax,%rbp), %edx
	testl	%edx, %edx
	je	.L2129
	testb	$2, 38(%rbp)
	jne	.L2129
	movl	%ecx, %esi
	call	sync_seg
	.p2align 4,,7
.L2129:
	movq	%r14, %rax
	salq	$4, %rax
	leaq	(%rax,%r14,4), %rax
	popq	%r14
	addl	64(%rax,%rbp), %r13d
	ret
.LFE558:
	.size	op_addl_A0_seg, .-op_addl_A0_seg
	.p2align 4,,15
.globl op_movl_A0_seg
	.type	op_movl_A0_seg, @function
op_movl_A0_seg:
.LFB557:
	movl	$__op_param1-64, %ecx
	movabsq	$-3689348814741910323, %rax
	pushq	%r14
.LCFI241:
	mulq	%rcx
	movq	%rbp, %rdi
	movq	%rdx, %rcx
	shrq	$4, %rcx
	mov	%ecx, %r14d
	movq	%r14, %rax
	salq	$4, %rax
	leaq	(%rax,%r14,4), %rax
	movl	76(%rax,%rbp), %edx
	testl	%edx, %edx
	je	.L2134
	testb	$2, 38(%rbp)
	jne	.L2134
	movl	%ecx, %esi
	call	sync_seg
	.p2align 4,,7
.L2134:
	movq	%r14, %rax
	salq	$4, %rax
	leaq	(%rax,%r14,4), %rax
	popq	%r14
	movl	64(%rax,%rbp), %r13d
	ret
.LFE557:
	.size	op_movl_A0_seg, .-op_movl_A0_seg
	.p2align 4,,15
.globl op_idivl_EAX_T0
	.type	op_idivl_EAX_T0, @function
op_idivl_EAX_T0:
.LFB544:
	subq	$8, %rsp
.LCFI242:
	call	helper_idivl_EAX_T0
	addq	$8, %rsp
	ret
.LFE544:
	.size	op_idivl_EAX_T0, .-op_idivl_EAX_T0
	.p2align 4,,15
.globl op_divl_EAX_T0
	.type	op_divl_EAX_T0, @function
op_divl_EAX_T0:
.LFB543:
	subq	$8, %rsp
.LCFI243:
	call	helper_divl_EAX_T0
	addq	$8, %rsp
	ret
.LFE543:
	.size	op_divl_EAX_T0, .-op_divl_EAX_T0
	.p2align 4,,15
	.type	compute_all_addb, @function
compute_all_addb:
.LFB638:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r8d
	movl	%eax, %edi
	movl	%eax, %r10d
	movl	%eax, %edx
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	cmpb	$1, %al
	movl	%edi, %esi
	sbbl	%r9d, %r9d
	movzbl	%al, %ecx
	xorl	%r10d, %esi
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpb	%r8b, %dl
	setb	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sall	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE638:
	.size	compute_all_addb, .-compute_all_addb
	.p2align 4,,15
.globl op_rcrl_T0_T1_cc
	.type	op_rcrl_T0_T1_cc, @function
op_rcrl_T0_T1_cc:
.LFB883:
	pushq	%r14
.LCFI244:
	movl	%r12d, %r14d
	andl	$31, %r14d
	je	.L2148
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	%r14d, %ecx
	movl	%ebx, %edx
	movl	%eax, %edi
	shrl	%cl, %edx
	movl	$32, %ecx
	andl	$1, %eax
	subl	%r14d, %ecx
	movl	%ebx, %esi
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2150
	movl	$33, %ecx
	movl	%ebx, %eax
	subl	%r14d, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L2150:
	movl	%edx, %ebx
	leal	-1(%r14), %ecx
	xorl	%esi, %edx
	sarl	$20, %edx
	andl	$-2050, %edi
	movl	$1, 48(%rbp)
	andl	$2048, %edx
	shrl	%cl, %esi
	orl	%edx, %edi
	andl	$1, %esi
	orl	%esi, %edi
	movl	%edi, 40(%rbp)
.L2148:
	popq	%r14
	ret
.LFE883:
	.size	op_rcrl_T0_T1_cc, .-op_rcrl_T0_T1_cc
	.p2align 4,,15
.globl op_rcll_T0_T1_cc
	.type	op_rcll_T0_T1_cc, @function
op_rcll_T0_T1_cc:
.LFB882:
	pushq	%r14
.LCFI245:
	movl	%r12d, %r14d
	andl	$31, %r14d
	je	.L2154
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movl	%r14d, %ecx
	movl	%ebx, %edx
	movl	%eax, %edi
	sall	%cl, %edx
	leal	-1(%r14), %ecx
	andl	$1, %eax
	movl	%ebx, %esi
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2156
	movl	$33, %ecx
	movl	%ebx, %eax
	subl	%r14d, %ecx
	shrl	%cl, %eax
	orl	%eax, %edx
.L2156:
	movl	%edx, %ebx
	movl	$32, %ecx
	xorl	%esi, %edx
	sarl	$20, %edx
	subl	%r14d, %ecx
	andl	$-2050, %edi
	andl	$2048, %edx
	shrl	%cl, %esi
	movl	$1, 48(%rbp)
	orl	%edx, %edi
	andl	$1, %esi
	orl	%esi, %edi
	movl	%edi, 40(%rbp)
.L2154:
	popq	%r14
	ret
.LFE882:
	.size	op_rcll_T0_T1_cc, .-op_rcll_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_T0_T1_cc
	.type	op_rorl_T0_T1_cc, @function
op_rorl_T0_T1_cc:
.LFB879:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI246:
	movq	%r15, -8(%rsp)
.LCFI247:
	subq	$24, %rsp
.LCFI248:
	andl	$31, %ecx
	je	.L2160
	movl	%ebx, %eax
	movl	%ebx, %r14d
	rorl	%cl, %eax
	movl	%eax, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r14d
	shrl	$31, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2160:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE879:
	.size	op_rorl_T0_T1_cc, .-op_rorl_T0_T1_cc
	.p2align 4,,15
.globl op_roll_T0_T1_cc
	.type	op_roll_T0_T1_cc, @function
op_roll_T0_T1_cc:
.LFB878:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI249:
	movq	%r15, -8(%rsp)
.LCFI250:
	subq	$24, %rsp
.LCFI251:
	andl	$31, %ecx
	je	.L2164
	movl	%ebx, %eax
	movl	%ebx, %r14d
	roll	%cl, %eax
	movl	%eax, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2164:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE878:
	.size	op_roll_T0_T1_cc, .-op_roll_T0_T1_cc
	.p2align 4,,15
.globl op_sets_T0_subl
	.type	op_sets_T0_subl, @function
op_sets_T0_subl:
.LFB872:
	movl	44(%rbp), %eax
	movl	%eax, %ebx
	shrl	$31, %ebx
	ret
.LFE872:
	.size	op_sets_T0_subl, .-op_sets_T0_subl
	.p2align 4,,15
	.type	compute_all_mull, @function
compute_all_mull:
.LFB857:
	movl	44(%rbp), %eax
	xorl	%esi, %esi
	cmpl	$0, 40(%rbp)
	movzbl	%al, %edx
	movzbl	parity_table(%rdx), %edx
	setne	%sil
	cmpl	$1, %eax
	sbbl	%ecx, %ecx
	sarl	$24, %eax
	andl	$64, %ecx
	andl	$128, %eax
	orl	%esi, %edx
	sall	$11, %esi
	orl	%edx, %eax
	orl	%esi, %ecx
	orl	%ecx, %eax
	ret
.LFE857:
	.size	compute_all_mull, .-compute_all_mull
	.p2align 4,,15
	.type	compute_all_sarl, @function
compute_all_sarl:
.LFB855:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpl	$1, %r8d
	movl	%r8d, %eax
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	movl	%esi, %edx
	xorl	%r8d, %esi
	sarl	$24, %eax
	andl	$1, %edx
	sarl	$20, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%ecx, %edx
	orl	%esi, %edi
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE855:
	.size	compute_all_sarl, .-compute_all_sarl
	.p2align 4,,15
	.type	compute_all_shll, @function
compute_all_shll:
.LFB852:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpl	$1, %r8d
	movl	%r8d, %eax
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	movl	%esi, %edx
	xorl	%r8d, %esi
	sarl	$24, %eax
	shrl	$31, %edx
	sarl	$20, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%ecx, %edx
	orl	%esi, %edi
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE852:
	.size	compute_all_shll, .-compute_all_shll
	.p2align 4,,15
	.type	compute_all_decl, @function
compute_all_decl:
.LFB851:
	movl	44(%rbp), %edx
	xorl	%ecx, %ecx
	movl	40(%rbp), %esi
	movl	$64, %edi
	movzbl	%dl, %eax
	testl	%edx, %edx
	movzbl	parity_table(%rax), %r8d
	je	.L2187
	movl	%edx, %ecx
	xorb	%dil, %dil
	sarl	$24, %ecx
	andl	$128, %ecx
	cmpl	$2147483647, %edx
	jne	.L2187
	orb	$8, %ch
	.p2align 4,,7
.L2187:
	leal	1(%rdx), %eax
	xorl	%eax, %edx
	movzbl	%r8b, %eax
	orl	%eax, %esi
	andl	$16, %edx
	orl	%esi, %edx
	orl	%edx, %edi
	orl	%edi, %ecx
	movl	%ecx, %eax
	ret
.LFE851:
	.size	compute_all_decl, .-compute_all_decl
	.p2align 4,,15
	.type	compute_all_incl, @function
compute_all_incl:
.LFB849:
	movl	44(%rbp), %edx
	xorl	%ecx, %ecx
	movl	40(%rbp), %esi
	movl	$64, %edi
	movzbl	%dl, %eax
	testl	%edx, %edx
	movzbl	parity_table(%rax), %r8d
	je	.L2194
	movl	%edx, %ecx
	xorb	%dil, %dil
	sarl	$24, %ecx
	andl	$128, %ecx
	cmpl	$-2147483648, %edx
	jne	.L2194
	orb	$8, %ch
	.p2align 4,,7
.L2194:
	leal	-1(%rdx), %eax
	xorl	%eax, %edx
	movzbl	%r8b, %eax
	orl	%eax, %esi
	andl	$16, %edx
	orl	%esi, %edx
	orl	%edx, %edi
	orl	%edi, %ecx
	movl	%ecx, %eax
	ret
.LFE849:
	.size	compute_all_incl, .-compute_all_incl
	.p2align 4,,15
	.type	compute_all_logicl, @function
compute_all_logicl:
.LFB847:
	movl	44(%rbp), %edx
	movzbl	%dl, %ecx
	cmpl	$1, %edx
	movzbl	parity_table(%rcx), %ecx
	sbbl	%eax, %eax
	sarl	$24, %edx
	andl	$128, %edx
	andl	$64, %eax
	orl	%edx, %ecx
	orl	%ecx, %eax
	ret
.LFE847:
	.size	compute_all_logicl, .-compute_all_logicl
	.p2align 4,,15
	.type	compute_all_sbbl, @function
compute_all_sbbl:
.LFB845:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	1(%rax,%r10), %r8d
	movl	%eax, %edi
	movzbl	%al, %ecx
	movl	%r10d, %esi
	movzbl	parity_table(%rcx), %ecx
	xorl	%r8d, %edi
	cmpl	$1, %eax
	sbbl	%r9d, %r9d
	xorl	%edi, %esi
	sarl	$24, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	xorl	%edx, %edx
	cmpl	%r10d, %r8d
	setbe	%dl
	xorl	%r10d, %r8d
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$20, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE845:
	.size	compute_all_sbbl, .-compute_all_sbbl
	.p2align 4,,15
	.type	compute_all_subl, @function
compute_all_subl:
.LFB843:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	(%rax,%r10), %r8d
	movl	%eax, %edi
	movzbl	%al, %ecx
	movl	%r10d, %esi
	movzbl	parity_table(%rcx), %ecx
	xorl	%r8d, %edi
	cmpl	$1, %eax
	sbbl	%r9d, %r9d
	xorl	%edi, %esi
	sarl	$24, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	xorl	%edx, %edx
	cmpl	%r10d, %r8d
	setb	%dl
	xorl	%r10d, %r8d
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$20, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE843:
	.size	compute_all_subl, .-compute_all_subl
	.p2align 4,,15
	.type	compute_all_adcl, @function
compute_all_adcl:
.LFB841:
	movl	44(%rbp), %edx
	movl	40(%rbp), %r8d
	movl	%edx, %r10d
	movl	%edx, %edi
	movl	%edx, %eax
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	movzbl	%dl, %ecx
	decl	%r10d
	movl	%edi, %esi
	cmpl	$1, %edx
	sbbl	%r9d, %r9d
	xorl	%r10d, %esi
	sarl	$24, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpl	%r8d, %edx
	setbe	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$20, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE841:
	.size	compute_all_adcl, .-compute_all_adcl
	.p2align 4,,15
	.type	compute_all_addl, @function
compute_all_addl:
.LFB839:
	movl	44(%rbp), %edx
	movl	40(%rbp), %r8d
	movl	%edx, %edi
	movl	%edx, %r10d
	movl	%edx, %eax
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	cmpl	$1, %edx
	movl	%edi, %esi
	sbbl	%r9d, %r9d
	movzbl	%dl, %ecx
	xorl	%r10d, %esi
	sarl	$24, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpl	%r8d, %edx
	setb	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$20, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE839:
	.size	compute_all_addl, .-compute_all_addl
	.p2align 4,,15
.globl op_rcrw_T0_T1_cc
	.type	op_rcrw_T0_T1_cc, @function
op_rcrw_T0_T1_cc:
.LFB766:
	movq	%r12, %rax
	pushq	%r14
.LCFI252:
	andl	$31, %eax
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L2224
	movzbl	%al, %r14d
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzwl	%bx,%esi
	movl	%r14d, %ecx
	movl	%eax, %edi
	movl	%esi, %edx
	andl	$1, %eax
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%r14d, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2226
	movl	$17, %ecx
	movl	%esi, %eax
	subl	%r14d, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L2226:
	movl	%edx, %ebx
	leal	-1(%r14), %ecx
	xorl	%esi, %edx
	sarl	$4, %edx
	andl	$-2050, %edi
	movl	$1, 48(%rbp)
	andl	$2048, %edx
	shrl	%cl, %esi
	orl	%edi, %edx
	andl	$1, %esi
	orl	%edx, %esi
	movl	%esi, 40(%rbp)
.L2224:
	popq	%r14
	ret
.LFE766:
	.size	op_rcrw_T0_T1_cc, .-op_rcrw_T0_T1_cc
	.p2align 4,,15
.globl op_rclw_T0_T1_cc
	.type	op_rclw_T0_T1_cc, @function
op_rclw_T0_T1_cc:
.LFB765:
	movq	%r12, %rax
	pushq	%r14
.LCFI253:
	andl	$31, %eax
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L2230
	movzbl	%al, %r14d
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzwl	%bx,%esi
	movl	%r14d, %ecx
	movl	%eax, %edi
	movl	%esi, %edx
	andl	$1, %eax
	sall	%cl, %edx
	leal	-1(%r14), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2232
	movl	$17, %ecx
	movl	%esi, %eax
	subl	%r14d, %ecx
	shrl	%cl, %eax
	orl	%eax, %edx
.L2232:
	movl	%edx, %ebx
	movl	$16, %ecx
	xorl	%esi, %edx
	sarl	$4, %edx
	subl	%r14d, %ecx
	andl	$-2050, %edi
	andl	$2048, %edx
	shrl	%cl, %esi
	movl	$1, 48(%rbp)
	orl	%edi, %edx
	andl	$1, %esi
	orl	%edx, %esi
	movl	%esi, 40(%rbp)
.L2230:
	popq	%r14
	ret
.LFE765:
	.size	op_rclw_T0_T1_cc, .-op_rclw_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_T0_T1_cc
	.type	op_rorw_T0_T1_cc, @function
op_rorw_T0_T1_cc:
.LFB762:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI254:
	movq	%r15, -8(%rsp)
.LCFI255:
	subq	$24, %rsp
.LCFI256:
	testb	$31, %al
	je	.L2236
	movl	%r12d, %esi
	movzwl	%bx,%eax
	movl	%ebx, %r14d
	andl	$15, %esi
	movl	%eax, %edx
	movl	%esi, %ecx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%esi, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r14d
	shrl	$15, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	andl	$1, %edx
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2236:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE762:
	.size	op_rorw_T0_T1_cc, .-op_rorw_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_T0_T1_cc
	.type	op_rolw_T0_T1_cc, @function
op_rolw_T0_T1_cc:
.LFB761:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI257:
	movq	%r15, -8(%rsp)
.LCFI258:
	subq	$24, %rsp
.LCFI259:
	testb	$31, %al
	je	.L2240
	movl	%r12d, %esi
	movzwl	%bx,%eax
	movl	%ebx, %r14d
	andl	$15, %esi
	movl	%eax, %edx
	movl	%esi, %ecx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%esi, %ecx
	shrl	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2240:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE761:
	.size	op_rolw_T0_T1_cc, .-op_rolw_T0_T1_cc
	.p2align 4,,15
.globl op_sets_T0_subw
	.type	op_sets_T0_subw, @function
op_sets_T0_subw:
.LFB755:
	movl	44(%rbp), %eax
	sarl	$15, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE755:
	.size	op_sets_T0_subw, .-op_sets_T0_subw
	.p2align 4,,15
	.type	compute_all_mulw, @function
compute_all_mulw:
.LFB740:
	movl	44(%rbp), %eax
	xorl	%esi, %esi
	cmpl	$0, 40(%rbp)
	movzbl	%al, %edi
	movzbl	parity_table(%rdi), %edx
	setne	%sil
	cmpw	$1, %ax
	sbbl	%ecx, %ecx
	sarl	$8, %eax
	andl	$64, %ecx
	andl	$128, %eax
	orl	%esi, %edx
	sall	$11, %esi
	orl	%edx, %eax
	orl	%esi, %ecx
	orl	%ecx, %eax
	ret
.LFE740:
	.size	compute_all_mulw, .-compute_all_mulw
	.p2align 4,,15
	.type	compute_all_sarw, @function
compute_all_sarw:
.LFB739:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpw	$1, %r8w
	movl	%r8d, %eax
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	movl	%esi, %edx
	xorl	%r8d, %esi
	sarl	$8, %eax
	andl	$1, %edx
	sarl	$4, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%ecx, %edx
	orl	%esi, %edi
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE739:
	.size	compute_all_sarw, .-compute_all_sarw
	.p2align 4,,15
	.type	compute_all_shlw, @function
compute_all_shlw:
.LFB737:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpw	$1, %r8w
	movl	%esi, %edx
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	movl	%r8d, %eax
	shrl	$15, %edx
	xorl	%r8d, %esi
	sarl	$8, %eax
	andl	$1, %edx
	sarl	$4, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%ecx, %edx
	orl	%esi, %edi
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE737:
	.size	compute_all_shlw, .-compute_all_shlw
	.p2align 4,,15
	.type	compute_all_decw, @function
compute_all_decw:
.LFB736:
	movl	44(%rbp), %ecx
	movl	$0, %eax
	movl	$2048, %esi
	cmpw	$1, %cx
	leal	1(%rcx), %edx
	movzbl	%cl, %r9d
	sbbl	%edi, %edi
	andl	$64, %edi
	cmpw	$32767, %cx
	cmovne	%eax, %esi
	movl	%ecx, %eax
	xorl	%edx, %ecx
	movzbl	parity_table(%r9), %edx
	sarl	$8, %eax
	andl	$16, %ecx
	orl	40(%rbp), %edx
	andl	$128, %eax
	orl	%edi, %esi
	orl	%edx, %ecx
	orl	%ecx, %eax
	orl	%esi, %eax
	ret
.LFE736:
	.size	compute_all_decw, .-compute_all_decw
	.p2align 4,,15
	.type	compute_all_incw, @function
compute_all_incw:
.LFB735:
	movl	44(%rbp), %ecx
	movl	$0, %eax
	movl	$2048, %esi
	cmpw	$1, %cx
	leal	-1(%rcx), %edx
	movzbl	%cl, %r9d
	sbbl	%edi, %edi
	andl	$64, %edi
	cmpw	$-32768, %cx
	cmovne	%eax, %esi
	movl	%ecx, %eax
	xorl	%edx, %ecx
	movzbl	parity_table(%r9), %edx
	sarl	$8, %eax
	andl	$16, %ecx
	orl	40(%rbp), %edx
	andl	$128, %eax
	orl	%edi, %esi
	orl	%edx, %ecx
	orl	%ecx, %eax
	orl	%esi, %eax
	ret
.LFE735:
	.size	compute_all_incw, .-compute_all_incw
	.p2align 4,,15
	.type	compute_all_logicw, @function
compute_all_logicw:
.LFB733:
	movl	44(%rbp), %edx
	movzbl	%dl, %ecx
	cmpw	$1, %dx
	movzbl	parity_table(%rcx), %ecx
	sbbl	%eax, %eax
	sarl	$8, %edx
	andl	$128, %edx
	andl	$64, %eax
	orl	%edx, %ecx
	orl	%ecx, %eax
	ret
.LFE733:
	.size	compute_all_logicw, .-compute_all_logicw
	.p2align 4,,15
	.type	compute_all_sbbw, @function
compute_all_sbbw:
.LFB731:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	1(%rax,%r10), %r9d
	movl	%eax, %edi
	movzbl	%al, %ecx
	movzbl	parity_table(%rcx), %ecx
	xorl	%r9d, %edi
	cmpw	$1, %ax
	movl	%edi, %esi
	sbbl	%r8d, %r8d
	sarl	$8, %eax
	xorl	%r10d, %esi
	andl	$64, %r8d
	andl	$128, %eax
	andl	$16, %esi
	xorl	%edx, %edx
	cmpw	%r10w, %r9w
	setbe	%dl
	xorl	%r10d, %r9d
	andl	%r9d, %edi
	orl	%ecx, %edx
	sarl	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r8d
	orl	%r8d, %eax
	ret
.LFE731:
	.size	compute_all_sbbw, .-compute_all_sbbw
	.p2align 4,,15
	.type	compute_all_subw, @function
compute_all_subw:
.LFB729:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	(%rax,%r10), %r9d
	movl	%eax, %edi
	movzbl	%al, %ecx
	movzbl	parity_table(%rcx), %ecx
	xorl	%r9d, %edi
	cmpw	$1, %ax
	movl	%edi, %esi
	sbbl	%r8d, %r8d
	sarl	$8, %eax
	xorl	%r10d, %esi
	andl	$64, %r8d
	andl	$128, %eax
	andl	$16, %esi
	xorl	%edx, %edx
	cmpw	%r10w, %r9w
	setb	%dl
	xorl	%r10d, %r9d
	andl	%r9d, %edi
	orl	%ecx, %edx
	sarl	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r8d
	orl	%r8d, %eax
	ret
.LFE729:
	.size	compute_all_subw, .-compute_all_subw
	.p2align 4,,15
	.type	compute_all_adcw, @function
compute_all_adcw:
.LFB727:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r8d
	movl	%eax, %r10d
	movl	%eax, %edi
	movl	%eax, %edx
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	movzbl	%al, %ecx
	decl	%r10d
	movl	%edi, %esi
	cmpw	$1, %ax
	sbbl	%r9d, %r9d
	xorl	%r10d, %esi
	sarl	$8, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpw	%r8w, %dx
	setbe	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE727:
	.size	compute_all_adcw, .-compute_all_adcw
	.p2align 4,,15
	.type	compute_all_addw, @function
compute_all_addw:
.LFB725:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r8d
	movl	%eax, %edi
	movl	%eax, %r10d
	movl	%eax, %edx
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	cmpw	$1, %ax
	movl	%edi, %esi
	sbbl	%r9d, %r9d
	movzbl	%al, %ecx
	xorl	%r10d, %esi
	sarl	$8, %eax
	andl	$64, %r9d
	andl	$128, %eax
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpw	%r8w, %dx
	setb	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sarl	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE725:
	.size	compute_all_addw, .-compute_all_addw
	.p2align 4,,15
.globl op_rorb_kernel_T0_T1_cc
	.type	op_rorb_kernel_T0_T1_cc, @function
op_rorb_kernel_T0_T1_cc:
.LFB695:
	pushq	%r15
.LCFI260:
	movl	%r12d, %eax
	pushq	%r14
.LCFI261:
	subq	$8, %rsp
.LCFI262:
	testb	$31, %al
	je	.L2302
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r15d
	andl	$7, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L2304
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L2306
	.p2align 4,,7
.L2304:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L2306:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r15d
	shrl	$7, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	andl	$1, %edx
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L2302:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE695:
	.size	op_rorb_kernel_T0_T1_cc, .-op_rorb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rclb_raw_T0_T1_cc
	.type	op_rclb_raw_T0_T1_cc, @function
op_rclb_raw_T0_T1_cc:
.LFB686:
	pushq	%r15
.LCFI263:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI264:
	subq	$8, %rsp
.LCFI265:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L2309
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %edx
	andl	$1, %eax
	sall	%cl, %edx
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 4(%rsp)
	jle	.L2311
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L2311:
	movl	%edx, %ebx
	movzbl	%dl, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU8
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L2309:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE686:
	.size	op_rclb_raw_T0_T1_cc, .-op_rclb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_T0_T1_cc
	.type	op_rcrb_T0_T1_cc, @function
op_rcrb_T0_T1_cc:
.LFB675:
	movq	%r12, %rax
	pushq	%r14
.LCFI266:
	andl	$31, %eax
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L2315
	movzbl	%al, %r14d
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	%bl,%esi
	movl	%r14d, %ecx
	movl	%eax, %edi
	movl	%esi, %edx
	andl	$1, %eax
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%r14d, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2317
	movl	$9, %ecx
	movl	%esi, %eax
	subl	%r14d, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L2317:
	movl	%edx, %ebx
	leal	-1(%r14), %ecx
	xorl	%esi, %edx
	sall	$4, %edx
	andl	$-2050, %edi
	movl	$1, 48(%rbp)
	andl	$2048, %edx
	shrl	%cl, %esi
	orl	%edi, %edx
	andl	$1, %esi
	orl	%edx, %esi
	movl	%esi, 40(%rbp)
.L2315:
	popq	%r14
	ret
.LFE675:
	.size	op_rcrb_T0_T1_cc, .-op_rcrb_T0_T1_cc
	.p2align 4,,15
.globl op_rclb_T0_T1_cc
	.type	op_rclb_T0_T1_cc, @function
op_rclb_T0_T1_cc:
.LFB674:
	movq	%r12, %rax
	pushq	%r14
.LCFI267:
	andl	$31, %eax
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L2321
	movzbl	%al, %r14d
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	%bl,%esi
	movl	%r14d, %ecx
	movl	%eax, %edi
	movl	%esi, %edx
	andl	$1, %eax
	sall	%cl, %edx
	leal	-1(%r14), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, %r14d
	jle	.L2323
	movl	$9, %ecx
	movl	%esi, %eax
	subl	%r14d, %ecx
	shrl	%cl, %eax
	orl	%eax, %edx
.L2323:
	movl	%edx, %ebx
	movl	$8, %ecx
	xorl	%esi, %edx
	sall	$4, %edx
	subl	%r14d, %ecx
	andl	$-2050, %edi
	andl	$2048, %edx
	shrl	%cl, %esi
	movl	$1, 48(%rbp)
	orl	%edi, %edx
	andl	$1, %esi
	orl	%edx, %esi
	movl	%esi, 40(%rbp)
.L2321:
	popq	%r14
	ret
.LFE674:
	.size	op_rclb_T0_T1_cc, .-op_rclb_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_T0_T1_cc
	.type	op_rorb_T0_T1_cc, @function
op_rorb_T0_T1_cc:
.LFB671:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI268:
	movq	%r15, -8(%rsp)
.LCFI269:
	subq	$24, %rsp
.LCFI270:
	testb	$31, %al
	je	.L2327
	movl	%r12d, %esi
	movzbl	%bl,%eax
	movl	%ebx, %r14d
	andl	$7, %esi
	movl	%eax, %edx
	movl	%esi, %ecx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%esi, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r14d
	shrl	$7, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	andl	$1, %edx
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2327:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE671:
	.size	op_rorb_T0_T1_cc, .-op_rorb_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_T0_T1_cc
	.type	op_rolb_T0_T1_cc, @function
op_rolb_T0_T1_cc:
.LFB670:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI271:
	movq	%r15, -8(%rsp)
.LCFI272:
	subq	$24, %rsp
.LCFI273:
	testb	$31, %al
	je	.L2331
	movl	%r12d, %esi
	movzbl	%bl,%eax
	movl	%ebx, %r14d
	andl	$7, %esi
	movl	%eax, %edx
	movl	%esi, %ecx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%esi, %ecx
	shrl	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %ebx
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2331:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE670:
	.size	op_rolb_T0_T1_cc, .-op_rolb_T0_T1_cc
	.p2align 4,,15
.globl op_sets_T0_subb
	.type	op_sets_T0_subb, @function
op_sets_T0_subb:
.LFB664:
	movl	44(%rbp), %eax
	sarl	$7, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
.LFE664:
	.size	op_sets_T0_subb, .-op_sets_T0_subb
	.p2align 4,,15
	.type	compute_all_mulb, @function
compute_all_mulb:
.LFB653:
	movl	44(%rbp), %eax
	xorl	%esi, %esi
	cmpl	$0, 40(%rbp)
	movzbl	%al, %edi
	movzbl	parity_table(%rdi), %edx
	setne	%sil
	cmpb	$1, %al
	sbbl	%ecx, %ecx
	andl	$128, %eax
	andl	$64, %ecx
	orl	%esi, %edx
	sall	$11, %esi
	orl	%edx, %eax
	orl	%esi, %ecx
	orl	%ecx, %eax
	ret
.LFE653:
	.size	compute_all_mulb, .-compute_all_mulb
	.p2align 4,,15
	.type	compute_all_sarb, @function
compute_all_sarb:
.LFB652:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpb	$1, %r8b
	movl	%esi, %edx
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	xorl	%r8d, %esi
	movl	%r8d, %eax
	andl	$1, %edx
	sall	$4, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%esi, %edi
	orl	%ecx, %edx
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE652:
	.size	compute_all_sarb, .-compute_all_sarb
	.p2align 4,,15
	.type	compute_all_shlb, @function
compute_all_shlb:
.LFB650:
	movl	44(%rbp), %r8d
	movl	40(%rbp), %esi
	movzbl	%r8b, %ecx
	cmpb	$1, %r8b
	movl	%esi, %edx
	movzbl	parity_table(%rcx), %ecx
	sbbl	%edi, %edi
	shrl	$7, %edx
	xorl	%r8d, %esi
	movl	%r8d, %eax
	andl	$1, %edx
	sall	$4, %esi
	andl	$64, %edi
	andl	$128, %eax
	andl	$2048, %esi
	orl	%ecx, %edx
	orl	%esi, %edi
	orl	%edx, %eax
	orl	%edi, %eax
	ret
.LFE650:
	.size	compute_all_shlb, .-compute_all_shlb
	.p2align 4,,15
	.type	compute_all_decb, @function
compute_all_decb:
.LFB649:
	movl	44(%rbp), %ecx
	movl	$0, %eax
	movl	$2048, %edi
	cmpb	$1, %cl
	leal	1(%rcx), %edx
	movzbl	%cl, %r9d
	sbbl	%esi, %esi
	andl	$64, %esi
	cmpb	$127, %cl
	cmovne	%eax, %edi
	movl	%ecx, %eax
	xorl	%edx, %ecx
	movzbl	parity_table(%r9), %edx
	andl	$16, %ecx
	andl	$128, %eax
	orl	40(%rbp), %edx
	orl	%edi, %esi
	orl	%edx, %ecx
	orl	%ecx, %eax
	orl	%esi, %eax
	ret
.LFE649:
	.size	compute_all_decb, .-compute_all_decb
	.p2align 4,,15
	.type	compute_all_incb, @function
compute_all_incb:
.LFB648:
	movl	44(%rbp), %ecx
	movl	$0, %eax
	movl	$2048, %edi
	cmpb	$1, %cl
	leal	-1(%rcx), %edx
	movzbl	%cl, %r9d
	sbbl	%esi, %esi
	andl	$64, %esi
	cmpb	$-128, %cl
	cmovne	%eax, %edi
	movl	%ecx, %eax
	xorl	%edx, %ecx
	movzbl	parity_table(%r9), %edx
	andl	$16, %ecx
	andl	$128, %eax
	orl	40(%rbp), %edx
	orl	%edi, %esi
	orl	%edx, %ecx
	orl	%ecx, %eax
	orl	%esi, %eax
	ret
.LFE648:
	.size	compute_all_incb, .-compute_all_incb
	.p2align 4,,15
	.type	compute_all_logicb, @function
compute_all_logicb:
.LFB646:
	movl	44(%rbp), %ecx
	movzbl	%cl, %edx
	cmpb	$1, %cl
	movzbl	parity_table(%rdx), %edx
	sbbl	%eax, %eax
	andl	$128, %ecx
	andl	$64, %eax
	orl	%ecx, %edx
	orl	%edx, %eax
	ret
.LFE646:
	.size	compute_all_logicb, .-compute_all_logicb
	.p2align 4,,15
	.type	compute_all_sbbb, @function
compute_all_sbbb:
.LFB644:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	1(%rax,%r10), %r9d
	movl	%eax, %esi
	movzbl	%al, %ecx
	movzbl	parity_table(%rcx), %ecx
	xorl	%r9d, %esi
	cmpb	$1, %al
	movl	%esi, %edi
	sbbl	%r8d, %r8d
	andl	$128, %eax
	xorl	%r10d, %edi
	andl	$64, %r8d
	xorl	%edx, %edx
	andl	$16, %edi
	cmpb	%r10b, %r9b
	setbe	%dl
	xorl	%r10d, %r9d
	andl	%r9d, %esi
	orl	%ecx, %edx
	sall	$4, %esi
	orl	%edx, %edi
	andl	$2048, %esi
	orl	%edi, %eax
	orl	%esi, %r8d
	orl	%r8d, %eax
	ret
.LFE644:
	.size	compute_all_sbbb, .-compute_all_sbbb
	.p2align 4,,15
	.type	compute_all_subb, @function
compute_all_subb:
.LFB642:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r10d
	leal	(%rax,%r10), %r9d
	movl	%eax, %esi
	movzbl	%al, %ecx
	movzbl	parity_table(%rcx), %ecx
	xorl	%r9d, %esi
	cmpb	$1, %al
	movl	%esi, %edi
	sbbl	%r8d, %r8d
	andl	$128, %eax
	xorl	%r10d, %edi
	andl	$64, %r8d
	xorl	%edx, %edx
	andl	$16, %edi
	cmpb	%r10b, %r9b
	setb	%dl
	xorl	%r10d, %r9d
	andl	%r9d, %esi
	orl	%ecx, %edx
	sall	$4, %esi
	orl	%edx, %edi
	andl	$2048, %esi
	orl	%edi, %eax
	orl	%esi, %r8d
	orl	%r8d, %eax
	ret
.LFE642:
	.size	compute_all_subb, .-compute_all_subb
	.p2align 4,,15
	.type	compute_all_adcb, @function
compute_all_adcb:
.LFB640:
	movl	44(%rbp), %eax
	movl	40(%rbp), %r8d
	movl	%eax, %r10d
	movl	%eax, %edi
	movl	%eax, %edx
	xorl	%r8d, %edi
	subl	%r8d, %r10d
	movzbl	%al, %ecx
	decl	%r10d
	movl	%edi, %esi
	cmpb	$1, %al
	sbbl	%r9d, %r9d
	xorl	%r10d, %esi
	andl	$128, %eax
	andl	$64, %r9d
	andl	$16, %esi
	movzbl	parity_table(%rcx), %ecx
	cmpb	%r8b, %dl
	setbe	%dl
	xorl	%r10d, %r8d
	notl	%r8d
	movzbl	%dl, %edx
	andl	%r8d, %edi
	orl	%ecx, %edx
	sall	$4, %edi
	orl	%edx, %esi
	andl	$2048, %edi
	orl	%esi, %eax
	orl	%edi, %r9d
	orl	%r9d, %eax
	ret
.LFE640:
	.size	compute_all_adcb, .-compute_all_adcb
	.p2align 4,,15
.globl op_packuswb_xmm
	.type	op_packuswb_xmm, @function
op_packuswb_xmm:
.LFB1376:
	movswl	__op_param1(%rbp),%eax
	movl	$255, %ecx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	xorl	%edx, %edx
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -24(%rsp)
	movswl	2(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -23(%rsp)
	movswl	4(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -22(%rsp)
	movswl	6(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -21(%rsp)
	movswl	8(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -20(%rsp)
	movswl	10(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -19(%rsp)
	movswl	12(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -18(%rsp)
	movswl	14(%rsi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -17(%rsp)
	movswl	__op_param2(%rbp),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -16(%rsp)
	movswl	2(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -15(%rsp)
	movswl	4(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -14(%rsp)
	movswl	6(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -13(%rsp)
	movswl	8(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -12(%rsp)
	movswl	10(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -11(%rsp)
	movswl	12(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovs	%edx, %eax
	movb	%al, -10(%rsp)
	movswl	14(%rdi),%eax
	cmpl	$255, %eax
	cmovg	%ecx, %eax
	testl	%eax, %eax
	cmovns	%eax, %edx
	movq	-24(%rsp), %rax
	movb	%dl, -9(%rsp)
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rsi)
	ret
.LFE1376:
	.size	op_packuswb_xmm, .-op_packuswb_xmm
	.p2align 4,,15
.globl op_psubusb_xmm
	.type	op_psubusb_xmm, @function
op_psubusb_xmm:
.LFB1238:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edx
	movl	$255, %r8d
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	xorl	%edi, %edi
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%r9), %edx
	movzbl	1(%rsi), %eax
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 7(%rsi)
	movzbl	8(%rsi), %eax
	movzbl	8(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 8(%rsi)
	movzbl	9(%rsi), %eax
	movzbl	9(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 9(%rsi)
	movzbl	10(%rsi), %eax
	movzbl	10(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 10(%rsi)
	movzbl	11(%rsi), %eax
	movzbl	11(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 11(%rsi)
	movzbl	12(%r9), %edx
	movzbl	12(%rsi), %eax
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 12(%rsi)
	movzbl	13(%rsi), %eax
	movzbl	13(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 13(%rsi)
	movzbl	14(%rsi), %eax
	movzbl	14(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 14(%rsi)
	movzbl	15(%rsi), %eax
	movzbl	15(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovns	%eax, %edi
	movb	%dil, 15(%rsi)
	ret
.LFE1238:
	.size	op_psubusb_xmm, .-op_psubusb_xmm
	.p2align 4,,15
.globl op_paddusb_xmm
	.type	op_paddusb_xmm, @function
op_paddusb_xmm:
.LFB1236:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rsi
	movl	$255, %edi
	leaq	__op_param2(%rbp), %r8
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%r8), %edx
	movzbl	1(%rsi), %eax
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 7(%rsi)
	movzbl	8(%rsi), %eax
	movzbl	8(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 8(%rsi)
	movzbl	9(%rsi), %eax
	movzbl	9(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 9(%rsi)
	movzbl	10(%rsi), %eax
	movzbl	10(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 10(%rsi)
	movzbl	11(%rsi), %eax
	movzbl	11(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 11(%rsi)
	movzbl	12(%r8), %edx
	movzbl	12(%rsi), %eax
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 12(%rsi)
	movzbl	13(%rsi), %eax
	movzbl	13(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 13(%rsi)
	movzbl	14(%rsi), %eax
	movzbl	14(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 14(%rsi)
	movzbl	15(%rsi), %eax
	movzbl	15(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovle	%eax, %edi
	movb	%dil, 15(%rsi)
	ret
.LFE1236:
	.size	op_paddusb_xmm, .-op_paddusb_xmm
	.p2align 4,,15
.globl op_packuswb_mmx
	.type	op_packuswb_mmx, @function
op_packuswb_mmx:
.LFB1210:
	movswl	__op_param1(%rbp),%eax
	movl	$255, %edi
	xorl	%ecx, %ecx
	leaq	__op_param1(%rbp), %rdx
	leaq	__op_param2(%rbp), %r9
	cmpl	$255, %eax
	cmovg	%edi, %eax
	xorl	%esi, %esi
	testl	%eax, %eax
	cmovs	%esi, %eax
	movb	%al, %cl
	movswl	2(%rdx),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	movb	%al, %ch
	movswl	4(%rdx),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	andq	$-16711681, %rcx
	movzbl	%al, %eax
	salq	$16, %rax
	orq	%rax, %rcx
	movswl	6(%rdx),%eax
	movabsq	$-4278190081, %rdx
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-1095216660481, %rdx
	movzbl	%al, %eax
	salq	$24, %rax
	orq	%rax, %rcx
	movswl	__op_param2(%rbp),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-280375465082881, %rdx
	movzbl	%al, %eax
	salq	$32, %rax
	orq	%rax, %rcx
	movswl	2(%r9),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-71776119061217281, %rdx
	movzbl	%al, %eax
	salq	$40, %rax
	orq	%rax, %rcx
	movswl	4(%r9),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	andq	%rdx, %rcx
	movzbl	%al, %eax
	salq	$48, %rax
	orq	%rax, %rcx
	movswl	6(%r9),%eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	testl	%eax, %eax
	cmovns	%eax, %esi
	movabsq	$72057594037927935, %rax
	salq	$56, %rsi
	andq	%rax, %rcx
	orq	%rsi, %rcx
	movq	%rcx, __op_param1(%rbp)
	ret
.LFE1210:
	.size	op_packuswb_mmx, .-op_packuswb_mmx
	.p2align 4,,15
.globl op_psubusb_mmx
	.type	op_psubusb_mmx, @function
op_psubusb_mmx:
.LFB1173:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edx
	movl	$255, %r8d
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	xorl	%edi, %edi
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%r9), %edx
	movzbl	1(%rsi), %eax
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%r9), %edx
	subl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovns	%eax, %edi
	movb	%dil, 7(%rsi)
	ret
.LFE1173:
	.size	op_psubusb_mmx, .-op_psubusb_mmx
	.p2align 4,,15
.globl op_paddusb_mmx
	.type	op_paddusb_mmx, @function
op_paddusb_mmx:
.LFB1171:
	movzbl	__op_param1(%rbp), %eax
	movzbl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rsi
	movl	$255, %edi
	leaq	__op_param2(%rbp), %r8
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movzbl	1(%r8), %edx
	movzbl	1(%rsi), %eax
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 1(%rsi)
	movzbl	2(%rsi), %eax
	movzbl	2(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 2(%rsi)
	movzbl	3(%rsi), %eax
	movzbl	3(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 3(%rsi)
	movzbl	4(%rsi), %eax
	movzbl	4(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 4(%rsi)
	movzbl	5(%rsi), %eax
	movzbl	5(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 5(%rsi)
	movzbl	6(%rsi), %eax
	movzbl	6(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovg	%edi, %eax
	movb	%al, 6(%rsi)
	movzbl	7(%rsi), %eax
	movzbl	7(%r8), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	cmovle	%eax, %edi
	movb	%dil, 7(%rsi)
	ret
.LFE1171:
	.size	op_paddusb_mmx, .-op_paddusb_mmx
	.p2align 4,,15
.globl op_packsswb_xmm
	.type	op_packsswb_xmm, @function
op_packsswb_xmm:
.LFB1375:
	movswl	__op_param1(%rbp),%eax
	movl	$127, %ecx
	movl	$-128, %edx
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %rdi
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -24(%rsp)
	movswl	2(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -23(%rsp)
	movswl	4(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -22(%rsp)
	movswl	6(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -21(%rsp)
	movswl	8(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -20(%rsp)
	movswl	10(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -19(%rsp)
	movswl	12(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -18(%rsp)
	movswl	14(%rsi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -17(%rsp)
	movswl	__op_param2(%rbp),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -16(%rsp)
	movswl	2(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -15(%rsp)
	movswl	4(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -14(%rsp)
	movswl	6(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -13(%rsp)
	movswl	8(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -12(%rsp)
	movswl	10(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -11(%rsp)
	movswl	12(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movb	%al, -10(%rsp)
	movswl	14(%rdi),%eax
	cmpl	$127, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovge	%eax, %edx
	movq	-24(%rsp), %rax
	movb	%dl, -9(%rsp)
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rsi)
	ret
.LFE1375:
	.size	op_packsswb_xmm, .-op_packsswb_xmm
	.p2align 4,,15
.globl op_psubsb_xmm
	.type	op_psubsb_xmm, @function
op_psubsb_xmm:
.LFB1239:
	movsbl	__op_param1(%rbp),%eax
	movsbl	__op_param2(%rbp),%edx
	movl	$127, %r8d
	movl	$-128, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movsbl	1(%r9),%edx
	movsbl	1(%rsi),%eax
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 1(%rsi)
	movsbl	2(%rsi),%eax
	movsbl	2(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 2(%rsi)
	movsbl	3(%rsi),%eax
	movsbl	3(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 3(%rsi)
	movsbl	4(%rsi),%eax
	movsbl	4(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 4(%rsi)
	movsbl	5(%rsi),%eax
	movsbl	5(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 5(%rsi)
	movsbl	6(%rsi),%eax
	movsbl	6(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 6(%rsi)
	movsbl	7(%rsi),%eax
	movsbl	7(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 7(%rsi)
	movsbl	8(%rsi),%eax
	movsbl	8(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 8(%rsi)
	movsbl	9(%rsi),%eax
	movsbl	9(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 9(%rsi)
	movsbl	10(%rsi),%eax
	movsbl	10(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 10(%rsi)
	movsbl	11(%rsi),%eax
	movsbl	11(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 11(%rsi)
	movsbl	12(%r9),%edx
	movsbl	12(%rsi),%eax
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 12(%rsi)
	movsbl	13(%rsi),%eax
	movsbl	13(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 13(%rsi)
	movsbl	14(%rsi),%eax
	movsbl	14(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 14(%rsi)
	movsbl	15(%rsi),%eax
	movsbl	15(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movb	%dil, 15(%rsi)
	ret
.LFE1239:
	.size	op_psubsb_xmm, .-op_psubsb_xmm
	.p2align 4,,15
.globl op_paddsb_xmm
	.type	op_paddsb_xmm, @function
op_paddsb_xmm:
.LFB1237:
	movsbl	__op_param1(%rbp),%eax
	movsbl	__op_param2(%rbp),%edx
	movl	$127, %r8d
	movl	$-128, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movsbl	1(%r9),%edx
	movsbl	1(%rsi),%eax
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 1(%rsi)
	movsbl	2(%rsi),%eax
	movsbl	2(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 2(%rsi)
	movsbl	3(%rsi),%eax
	movsbl	3(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 3(%rsi)
	movsbl	4(%rsi),%eax
	movsbl	4(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 4(%rsi)
	movsbl	5(%rsi),%eax
	movsbl	5(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 5(%rsi)
	movsbl	6(%rsi),%eax
	movsbl	6(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 6(%rsi)
	movsbl	7(%rsi),%eax
	movsbl	7(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 7(%rsi)
	movsbl	8(%rsi),%eax
	movsbl	8(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 8(%rsi)
	movsbl	9(%rsi),%eax
	movsbl	9(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 9(%rsi)
	movsbl	10(%rsi),%eax
	movsbl	10(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 10(%rsi)
	movsbl	11(%rsi),%eax
	movsbl	11(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 11(%rsi)
	movsbl	12(%r9),%edx
	movsbl	12(%rsi),%eax
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 12(%rsi)
	movsbl	13(%rsi),%eax
	movsbl	13(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 13(%rsi)
	movsbl	14(%rsi),%eax
	movsbl	14(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 14(%rsi)
	movsbl	15(%rsi),%eax
	movsbl	15(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movb	%dil, 15(%rsi)
	ret
.LFE1237:
	.size	op_paddsb_xmm, .-op_paddsb_xmm
	.p2align 4,,15
.globl op_packsswb_mmx
	.type	op_packsswb_mmx, @function
op_packsswb_mmx:
.LFB1209:
	movswl	__op_param1(%rbp),%eax
	movl	$127, %edi
	xorl	%ecx, %ecx
	movl	$-128, %esi
	leaq	__op_param1(%rbp), %rdx
	leaq	__op_param2(%rbp), %r9
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movb	%al, %cl
	movswl	2(%rdx),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movb	%al, %ch
	movswl	4(%rdx),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	andq	$-16711681, %rcx
	movzbl	%al, %eax
	salq	$16, %rax
	orq	%rax, %rcx
	movswl	6(%rdx),%eax
	movabsq	$-4278190081, %rdx
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-1095216660481, %rdx
	movzbl	%al, %eax
	salq	$24, %rax
	orq	%rax, %rcx
	movswl	__op_param2(%rbp),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-280375465082881, %rdx
	movzbl	%al, %eax
	salq	$32, %rax
	orq	%rax, %rcx
	movswl	2(%r9),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	andq	%rdx, %rcx
	movabsq	$-71776119061217281, %rdx
	movzbl	%al, %eax
	salq	$40, %rax
	orq	%rax, %rcx
	movswl	4(%r9),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	andq	%rdx, %rcx
	movzbl	%al, %eax
	salq	$48, %rax
	orq	%rax, %rcx
	movswl	6(%r9),%eax
	cmpl	$127, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovge	%eax, %esi
	movabsq	$72057594037927935, %rax
	salq	$56, %rsi
	andq	%rax, %rcx
	orq	%rsi, %rcx
	movq	%rcx, __op_param1(%rbp)
	ret
.LFE1209:
	.size	op_packsswb_mmx, .-op_packsswb_mmx
	.p2align 4,,15
.globl op_psubsb_mmx
	.type	op_psubsb_mmx, @function
op_psubsb_mmx:
.LFB1174:
	movsbl	__op_param1(%rbp),%eax
	movsbl	__op_param2(%rbp),%edx
	movl	$127, %r8d
	movl	$-128, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movsbl	1(%r9),%edx
	movsbl	1(%rsi),%eax
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 1(%rsi)
	movsbl	2(%rsi),%eax
	movsbl	2(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 2(%rsi)
	movsbl	3(%rsi),%eax
	movsbl	3(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 3(%rsi)
	movsbl	4(%rsi),%eax
	movsbl	4(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 4(%rsi)
	movsbl	5(%rsi),%eax
	movsbl	5(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 5(%rsi)
	movsbl	6(%rsi),%eax
	movsbl	6(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 6(%rsi)
	movsbl	7(%rsi),%eax
	movsbl	7(%r9),%edx
	subl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movb	%dil, 7(%rsi)
	ret
.LFE1174:
	.size	op_psubsb_mmx, .-op_psubsb_mmx
	.p2align 4,,15
.globl op_paddsb_mmx
	.type	op_paddsb_mmx, @function
op_paddsb_mmx:
.LFB1172:
	movsbl	__op_param1(%rbp),%eax
	movsbl	__op_param2(%rbp),%edx
	movl	$127, %r8d
	movl	$-128, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, __op_param1(%rbp)
	movsbl	1(%r9),%edx
	movsbl	1(%rsi),%eax
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 1(%rsi)
	movsbl	2(%rsi),%eax
	movsbl	2(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 2(%rsi)
	movsbl	3(%rsi),%eax
	movsbl	3(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 3(%rsi)
	movsbl	4(%rsi),%eax
	movsbl	4(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 4(%rsi)
	movsbl	5(%rsi),%eax
	movsbl	5(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 5(%rsi)
	movsbl	6(%rsi),%eax
	movsbl	6(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movb	%al, 6(%rsi)
	movsbl	7(%rsi),%eax
	movsbl	7(%r9),%edx
	addl	%edx, %eax
	cmpl	$127, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movb	%dil, 7(%rsi)
	ret
.LFE1172:
	.size	op_paddsb_mmx, .-op_paddsb_mmx
	.p2align 4,,15
.globl op_packssdw_xmm
	.type	op_packssdw_xmm, @function
op_packssdw_xmm:
.LFB1377:
	movl	__op_param1(%rbp), %eax
	movl	$32767, %ecx
	movl	$-32768, %edx
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r8
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -24(%rsp)
	movl	4(%rdi), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -22(%rsp)
	movl	8(%rdi), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -20(%rsp)
	movl	12(%rdi), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -18(%rsp)
	movl	__op_param2(%rbp), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -16(%rsp)
	movl	4(%r8), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -14(%rsp)
	movl	8(%r8), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovl	%edx, %eax
	movw	%ax, -12(%rsp)
	movl	12(%r8), %eax
	cmpl	$32767, %eax
	cmovg	%ecx, %eax
	cmpl	%edx, %eax
	cmovge	%eax, %edx
	movq	-24(%rsp), %rax
	movw	%dx, -10(%rsp)
	movq	%rax, __op_param1(%rbp)
	movq	-16(%rsp), %rax
	movq	%rax, 8(%rdi)
	ret
.LFE1377:
	.size	op_packssdw_xmm, .-op_packssdw_xmm
	.p2align 4,,15
.globl op_psubsw_xmm
	.type	op_psubsw_xmm, @function
op_psubsw_xmm:
.LFB1243:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	movl	$32767, %r8d
	movl	$-32768, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%r9),%edx
	movswl	2(%rsi),%eax
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 2(%rsi)
	movswl	4(%rsi),%eax
	movswl	4(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 4(%rsi)
	movswl	6(%rsi),%eax
	movswl	6(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 6(%rsi)
	movswl	8(%rsi),%eax
	movswl	8(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 8(%rsi)
	movswl	10(%rsi),%eax
	movswl	10(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 10(%rsi)
	movswl	12(%rsi),%eax
	movswl	12(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 12(%rsi)
	movswl	14(%rsi),%eax
	movswl	14(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movw	%di, 14(%rsi)
	ret
.LFE1243:
	.size	op_psubsw_xmm, .-op_psubsw_xmm
	.p2align 4,,15
.globl op_paddsw_xmm
	.type	op_paddsw_xmm, @function
op_paddsw_xmm:
.LFB1241:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	movl	$32767, %r8d
	movl	$-32768, %edi
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%r9),%edx
	movswl	2(%rsi),%eax
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 2(%rsi)
	movswl	4(%rsi),%eax
	movswl	4(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 4(%rsi)
	movswl	6(%rsi),%eax
	movswl	6(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 6(%rsi)
	movswl	8(%rsi),%eax
	movswl	8(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 8(%rsi)
	movswl	10(%rsi),%eax
	movswl	10(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 10(%rsi)
	movswl	12(%rsi),%eax
	movswl	12(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovl	%edi, %eax
	movw	%ax, 12(%rsi)
	movswl	14(%rsi),%eax
	movswl	14(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%edi, %eax
	cmovge	%eax, %edi
	movw	%di, 14(%rsi)
	ret
.LFE1241:
	.size	op_paddsw_xmm, .-op_paddsw_xmm
	.p2align 4,,15
.globl op_packssdw_mmx
	.type	op_packssdw_mmx, @function
op_packssdw_mmx:
.LFB1211:
	movl	__op_param1(%rbp), %ecx
	movl	$32767, %edi
	movl	__op_param1+4(%rbp), %edx
	movl	$-32768, %esi
	movl	__op_param2(%rbp), %eax
	cmpl	$32767, %ecx
	cmovg	%edi, %ecx
	cmpl	%esi, %ecx
	cmovl	%esi, %ecx
	cmpl	$32767, %edx
	cmovg	%edi, %edx
	movzwl	%cx, %ecx
	cmpl	%esi, %edx
	cmovl	%esi, %edx
	movzwl	%dx, %edx
	salq	$16, %rdx
	cmpl	$32767, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	orq	%rdx, %rcx
	movzwl	%ax, %eax
	salq	$32, %rax
	orq	%rax, %rcx
	movl	__op_param2+4(%rbp), %eax
	cmpl	$32767, %eax
	cmovg	%edi, %eax
	cmpl	%esi, %eax
	cmovge	%eax, %esi
	movabsq	$281474976710655, %rax
	salq	$48, %rsi
	andq	%rax, %rcx
	orq	%rsi, %rcx
	movq	%rcx, __op_param1(%rbp)
	ret
.LFE1211:
	.size	op_packssdw_mmx, .-op_packssdw_mmx
	.p2align 4,,15
.globl op_psubsw_mmx
	.type	op_psubsw_mmx, @function
op_psubsw_mmx:
.LFB1178:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	movl	$32767, %r8d
	movl	$-32768, %esi
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%r9),%edx
	movswl	2(%rdi),%eax
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, 2(%rdi)
	movswl	4(%rdi),%eax
	movswl	4(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, 4(%rdi)
	movswl	6(%rdi),%eax
	movswl	6(%r9),%edx
	subl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovge	%eax, %esi
	movw	%si, 6(%rdi)
	ret
.LFE1178:
	.size	op_psubsw_mmx, .-op_psubsw_mmx
	.p2align 4,,15
.globl op_paddsw_mmx
	.type	op_paddsw_mmx, @function
op_paddsw_mmx:
.LFB1176:
	movswl	__op_param1(%rbp),%eax
	movswl	__op_param2(%rbp),%edx
	movl	$32767, %r8d
	movl	$-32768, %esi
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r9
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, __op_param1(%rbp)
	movswl	2(%r9),%edx
	movswl	2(%rdi),%eax
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, 2(%rdi)
	movswl	4(%rdi),%eax
	movswl	4(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovl	%esi, %eax
	movw	%ax, 4(%rdi)
	movswl	6(%rdi),%eax
	movswl	6(%r9),%edx
	addl	%edx, %eax
	cmpl	$32767, %eax
	cmovg	%r8d, %eax
	cmpl	%esi, %eax
	cmovge	%eax, %esi
	movw	%si, 6(%rdi)
	ret
.LFE1176:
	.size	op_paddsw_mmx, .-op_paddsw_mmx
	.p2align 4,,15
.globl op_psadbw_xmm
	.type	op_psadbw_xmm, @function
op_psadbw_xmm:
.LFB1265:
	leaq	__op_param1(%rbp), %r8
	leaq	__op_param2(%rbp), %rdi
	movzbl	__op_param1(%rbp), %ecx
	movzbl	__op_param2(%rbp), %eax
	movzbl	1(%rdi), %edx
	movzbl	1(%r8), %esi
	movzbl	3(%r8), %r10d
	subl	%eax, %ecx
	movl	%ecx, %eax
	subl	%edx, %esi
	cltd
	movl	%esi, %eax
	xorl	%edx, %ecx
	subl	%edx, %ecx
	cltd
	movzbl	2(%rdi), %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	movzbl	3(%rdi), %edx
	addl	%esi, %ecx
	movzbl	2(%r8), %esi
	subl	%edx, %r10d
	subl	%eax, %esi
	movl	%esi, %eax
	cltd
	movl	%r10d, %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	cltd
	movzbl	4(%r8), %eax
	xorl	%edx, %r10d
	subl	%edx, %r10d
	movzbl	4(%rdi), %edx
	addl	%r10d, %esi
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	5(%rdi), %edx
	addl	%eax, %ecx
	movzbl	5(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	6(%rdi), %edx
	addl	%eax, %esi
	movzbl	6(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	7(%rdi), %edx
	addl	%eax, %ecx
	movzbl	7(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %esi
	addl	%esi, %ecx
	movq	%rcx, __op_param1(%rbp)
	movzbl	8(%rdi), %eax
	movzbl	8(%r8), %ecx
	movzbl	9(%rdi), %edx
	movzbl	9(%r8), %esi
	movzbl	11(%r8), %r9d
	subl	%eax, %ecx
	movl	%ecx, %eax
	subl	%edx, %esi
	cltd
	movl	%esi, %eax
	xorl	%edx, %ecx
	subl	%edx, %ecx
	cltd
	movzbl	10(%rdi), %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	movzbl	11(%rdi), %edx
	addl	%esi, %ecx
	movzbl	10(%r8), %esi
	subl	%edx, %r9d
	subl	%eax, %esi
	movl	%esi, %eax
	cltd
	movl	%r9d, %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	cltd
	movzbl	12(%r8), %eax
	xorl	%edx, %r9d
	subl	%edx, %r9d
	movzbl	12(%rdi), %edx
	addl	%r9d, %esi
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	13(%rdi), %edx
	addl	%eax, %ecx
	movzbl	13(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	14(%rdi), %edx
	addl	%eax, %esi
	movzbl	14(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	15(%rdi), %edx
	addl	%eax, %ecx
	movzbl	15(%r8), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %esi
	addl	%esi, %ecx
	movq	%rcx, 8(%r8)
	ret
.LFE1265:
	.size	op_psadbw_xmm, .-op_psadbw_xmm
	.p2align 4,,15
.globl op_psadbw_mmx
	.type	op_psadbw_mmx, @function
op_psadbw_mmx:
.LFB1201:
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r8
	movzbl	__op_param1(%rbp), %ecx
	movzbl	__op_param2(%rbp), %eax
	movzbl	1(%r8), %edx
	movzbl	1(%rdi), %esi
	movzbl	3(%rdi), %r10d
	subl	%eax, %ecx
	movl	%ecx, %eax
	subl	%edx, %esi
	cltd
	movl	%esi, %eax
	xorl	%edx, %ecx
	subl	%edx, %ecx
	cltd
	movzbl	2(%r8), %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	movzbl	3(%r8), %edx
	addl	%esi, %ecx
	movzbl	2(%rdi), %esi
	subl	%edx, %r10d
	subl	%eax, %esi
	movl	%esi, %eax
	cltd
	movl	%r10d, %eax
	xorl	%edx, %esi
	subl	%edx, %esi
	cltd
	movzbl	4(%rdi), %eax
	xorl	%edx, %r10d
	subl	%edx, %r10d
	movzbl	4(%r8), %edx
	addl	%r10d, %esi
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	5(%r8), %edx
	addl	%eax, %ecx
	movzbl	5(%rdi), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	6(%r8), %edx
	addl	%eax, %esi
	movzbl	6(%rdi), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movzbl	7(%r8), %edx
	addl	%eax, %ecx
	movzbl	7(%rdi), %eax
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %esi
	addl	%esi, %ecx
	movq	%rcx, __op_param1(%rbp)
	ret
.LFE1201:
	.size	op_psadbw_mmx, .-op_psadbw_mmx
	.p2align 4,,15
.globl op_psubusw_xmm
	.type	op_psubusw_xmm, @function
op_psubusw_xmm:
.LFB1242:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	movl	$65535, %r8d
	leaq	__op_param1(%rbp), %rsi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	xorl	%edi, %edi
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%r9), %edx
	movzwl	2(%rsi), %eax
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	movzwl	4(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	movzwl	6(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 6(%rsi)
	movzwl	8(%rsi), %eax
	movzwl	8(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 8(%rsi)
	movzwl	10(%rsi), %eax
	movzwl	10(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 10(%rsi)
	movzwl	12(%rsi), %eax
	movzwl	12(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%edi, %eax
	movw	%ax, 12(%rsi)
	movzwl	14(%rsi), %eax
	movzwl	14(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovns	%eax, %edi
	movw	%di, 14(%rsi)
	ret
.LFE1242:
	.size	op_psubusw_xmm, .-op_psubusw_xmm
	.p2align 4,,15
.globl op_paddusw_mmx
	.type	op_paddusw_mmx, @function
op_paddusw_mmx:
.LFB1175:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rdi
	movl	$65535, %esi
	leaq	__op_param2(%rbp), %r8
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%esi, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%r8), %edx
	movzwl	2(%rdi), %eax
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%esi, %eax
	movw	%ax, 2(%rdi)
	movzwl	4(%rdi), %eax
	movzwl	4(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%esi, %eax
	movw	%ax, 4(%rdi)
	movzwl	6(%rdi), %eax
	movzwl	6(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovle	%eax, %esi
	movw	%si, 6(%rdi)
	ret
.LFE1175:
	.size	op_paddusw_mmx, .-op_paddusw_mmx
	.p2align 4,,15
.globl op_paddusw_xmm
	.type	op_paddusw_xmm, @function
op_paddusw_xmm:
.LFB1240:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	leaq	__op_param1(%rbp), %rsi
	movl	$65535, %edi
	leaq	__op_param2(%rbp), %r8
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%r8), %edx
	movzwl	2(%rsi), %eax
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 2(%rsi)
	movzwl	4(%rsi), %eax
	movzwl	4(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 4(%rsi)
	movzwl	6(%rsi), %eax
	movzwl	6(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 6(%rsi)
	movzwl	8(%rsi), %eax
	movzwl	8(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 8(%rsi)
	movzwl	10(%rsi), %eax
	movzwl	10(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 10(%rsi)
	movzwl	12(%rsi), %eax
	movzwl	12(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%edi, %eax
	movw	%ax, 12(%rsi)
	movzwl	14(%rsi), %eax
	movzwl	14(%r8), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	cmovle	%eax, %edi
	movw	%di, 14(%rsi)
	ret
.LFE1240:
	.size	op_paddusw_xmm, .-op_paddusw_xmm
	.p2align 4,,15
.globl op_psubusw_mmx
	.type	op_psubusw_mmx, @function
op_psubusw_mmx:
.LFB1177:
	movzwl	__op_param1(%rbp), %eax
	movzwl	__op_param2(%rbp), %edx
	movl	$65535, %r8d
	leaq	__op_param1(%rbp), %rdi
	leaq	__op_param2(%rbp), %r9
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	xorl	%esi, %esi
	testl	%eax, %eax
	cmovs	%esi, %eax
	movw	%ax, __op_param1(%rbp)
	movzwl	2(%r9), %edx
	movzwl	2(%rdi), %eax
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	movw	%ax, 2(%rdi)
	movzwl	4(%rdi), %eax
	movzwl	4(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovs	%esi, %eax
	movw	%ax, 4(%rdi)
	movzwl	6(%rdi), %eax
	movzwl	6(%r9), %edx
	subl	%edx, %eax
	cmpl	$65535, %eax
	cmovg	%r8d, %eax
	testl	%eax, %eax
	cmovns	%eax, %esi
	movw	%si, 6(%rdi)
	ret
.LFE1177:
	.size	op_psubusw_mmx, .-op_psubusw_mmx
	.p2align 4,,15
.globl op_cmpneqss
	.type	op_cmpneqss, @function
op_cmpneqss:
.LFB1351:
	movss	__op_param2(%rbp), %xmm0
	ucomiss	__op_param1(%rbp), %xmm0
	sete	%al
	setnp	%dl
	andl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1351:
	.size	op_cmpneqss, .-op_cmpneqss
	.p2align 4,,15
.globl op_cmpneqps
	.type	op_cmpneqps, @function
op_cmpneqps:
.LFB1350:
	movss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rdi
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rsi
	setnp	%dl
	sete	%al
	andl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movss	4(%rdi), %xmm0
	ucomiss	4(%rsi), %xmm0
	setnp	%dl
	sete	%al
	andl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rsi)
	movss	8(%rdi), %xmm0
	ucomiss	8(%rsi), %xmm0
	setnp	%dl
	sete	%al
	andl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%rsi)
	movss	12(%rdi), %xmm0
	ucomiss	12(%rsi), %xmm0
	sete	%al
	setnp	%dl
	andl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%rsi)
	ret
.LFE1350:
	.size	op_cmpneqps, .-op_cmpneqps
	.p2align 4,,15
.globl op_cmpeqss
	.type	op_cmpeqss, @function
op_cmpeqss:
.LFB1335:
	movss	__op_param2(%rbp), %xmm0
	ucomiss	__op_param1(%rbp), %xmm0
	setne	%al
	setp	%dl
	orl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1335:
	.size	op_cmpeqss, .-op_cmpeqss
	.p2align 4,,15
.globl op_cmpeqps
	.type	op_cmpeqps, @function
op_cmpeqps:
.LFB1334:
	movss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rdi
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rsi
	setp	%dl
	setne	%al
	orl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	movss	4(%rdi), %xmm0
	ucomiss	4(%rsi), %xmm0
	setp	%dl
	setne	%al
	orl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%rsi)
	movss	8(%rdi), %xmm0
	ucomiss	8(%rsi), %xmm0
	setp	%dl
	setne	%al
	orl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%rsi)
	movss	12(%rdi), %xmm0
	ucomiss	12(%rsi), %xmm0
	setne	%al
	setp	%dl
	orl	%edx, %eax
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%rsi)
	ret
.LFE1334:
	.size	op_cmpeqps, .-op_cmpeqps
	.p2align 4,,15
.globl op_cmpnless
	.type	op_cmpnless, @function
op_cmpnless:
.LFB1359:
	movss	__op_param2(%rbp), %xmm0
	ucomiss	__op_param1(%rbp), %xmm0
	sbbl	%edx, %edx
	movl	%edx, __op_param1(%rbp)
	ret
.LFE1359:
	.size	op_cmpnless, .-op_cmpnless
	.p2align 4,,15
.globl op_cmpnleps
	.type	op_cmpnleps, @function
op_cmpnleps:
.LFB1358:
	movss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	sbbl	%edx, %edx
	movl	%edx, __op_param1(%rbp)
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	sbbl	%eax, %eax
	movl	%eax, 4(%rcx)
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	sbbl	%eax, %eax
	movl	%eax, 8(%rcx)
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	sbbl	%eax, %eax
	movl	%eax, 12(%rcx)
	ret
.LFE1358:
	.size	op_cmpnleps, .-op_cmpnleps
	.p2align 4,,15
.globl op_cmpless
	.type	op_cmpless, @function
op_cmpless:
.LFB1343:
	movss	__op_param2(%rbp), %xmm0
	ucomiss	__op_param1(%rbp), %xmm0
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1343:
	.size	op_cmpless, .-op_cmpless
	.p2align 4,,15
.globl op_cmpleps
	.type	op_cmpleps, @function
op_cmpleps:
.LFB1342:
	movss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, __op_param1(%rbp)
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 4(%rcx)
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 8(%rcx)
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1342:
	.size	op_cmpleps, .-op_cmpleps
	.p2align 4,,15
.globl op_cmpnltss
	.type	op_cmpnltss, @function
op_cmpnltss:
.LFB1355:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	seta	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1355:
	.size	op_cmpnltss, .-op_cmpnltss
	.p2align 4,,15
.globl op_cmpnltps
	.type	op_cmpnltps, @function
op_cmpnltps:
.LFB1354:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	leaq	__op_param1(%rbp), %rcx
	seta	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	xorl	%eax, %eax
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	seta	%al
	decl	%eax
	movl	%eax, 4(%rcx)
	xorl	%eax, %eax
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	seta	%al
	decl	%eax
	movl	%eax, 8(%rcx)
	xorl	%eax, %eax
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	seta	%al
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1354:
	.size	op_cmpnltps, .-op_cmpnltps
	.p2align 4,,15
.globl op_cmpltss
	.type	op_cmpltss, @function
op_cmpltss:
.LFB1339:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	setbe	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1339:
	.size	op_cmpltss, .-op_cmpltss
	.p2align 4,,15
.globl op_cmpltps
	.type	op_cmpltps, @function
op_cmpltps:
.LFB1338:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	leaq	__op_param1(%rbp), %rcx
	setbe	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	xorl	%eax, %eax
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	setbe	%al
	decl	%eax
	movl	%eax, 4(%rcx)
	xorl	%eax, %eax
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	setbe	%al
	decl	%eax
	movl	%eax, 8(%rcx)
	xorl	%eax, %eax
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	setbe	%al
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1338:
	.size	op_cmpltps, .-op_cmpltps
	.p2align 4,,15
.globl op_cmpordss
	.type	op_cmpordss, @function
op_cmpordss:
.LFB1363:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	setp	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1363:
	.size	op_cmpordss, .-op_cmpordss
	.p2align 4,,15
.globl op_cmpordps
	.type	op_cmpordps, @function
op_cmpordps:
.LFB1362:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	leaq	__op_param1(%rbp), %rcx
	setp	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	xorl	%eax, %eax
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	setp	%al
	decl	%eax
	movl	%eax, 4(%rcx)
	xorl	%eax, %eax
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	setp	%al
	decl	%eax
	movl	%eax, 8(%rcx)
	xorl	%eax, %eax
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	setp	%al
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1362:
	.size	op_cmpordps, .-op_cmpordps
	.p2align 4,,15
.globl op_cmpunordss
	.type	op_cmpunordss, @function
op_cmpunordss:
.LFB1347:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	setnp	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	ret
.LFE1347:
	.size	op_cmpunordss, .-op_cmpunordss
	.p2align 4,,15
.globl op_cmpunordps
	.type	op_cmpunordps, @function
op_cmpunordps:
.LFB1346:
	movss	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomiss	__op_param1(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rsi
	leaq	__op_param1(%rbp), %rcx
	setnp	%al
	decl	%eax
	movl	%eax, __op_param1(%rbp)
	xorl	%eax, %eax
	movss	4(%rsi), %xmm0
	ucomiss	4(%rcx), %xmm0
	setnp	%al
	decl	%eax
	movl	%eax, 4(%rcx)
	xorl	%eax, %eax
	movss	8(%rsi), %xmm0
	ucomiss	8(%rcx), %xmm0
	setnp	%al
	decl	%eax
	movl	%eax, 8(%rcx)
	xorl	%eax, %eax
	movss	12(%rsi), %xmm0
	ucomiss	12(%rcx), %xmm0
	setnp	%al
	decl	%eax
	movl	%eax, 12(%rcx)
	ret
.LFE1346:
	.size	op_cmpunordps, .-op_cmpunordps
	.p2align 4,,15
.globl op_cmpneqsd
	.type	op_cmpneqsd, @function
op_cmpneqsd:
.LFB1353:
	movlpd	__op_param2(%rbp), %xmm0
	ucomisd	__op_param1(%rbp), %xmm0
	sete	%al
	setnp	%dl
	andl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1353:
	.size	op_cmpneqsd, .-op_cmpneqsd
	.p2align 4,,15
.globl op_cmpneqpd
	.type	op_cmpneqpd, @function
op_cmpneqpd:
.LFB1352:
	movlpd	__op_param2(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rsi
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	setnp	%dl
	sete	%al
	andl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ucomisd	8(%rsi), %xmm0
	sete	%al
	setnp	%dl
	andl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, 8(%rsi)
	ret
.LFE1352:
	.size	op_cmpneqpd, .-op_cmpneqpd
	.p2align 4,,15
.globl op_cmpeqsd
	.type	op_cmpeqsd, @function
op_cmpeqsd:
.LFB1337:
	movlpd	__op_param2(%rbp), %xmm0
	ucomisd	__op_param1(%rbp), %xmm0
	setne	%al
	setp	%dl
	orl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1337:
	.size	op_cmpeqsd, .-op_cmpeqsd
	.p2align 4,,15
.globl op_cmpeqpd
	.type	op_cmpeqpd, @function
op_cmpeqpd:
.LFB1336:
	movlpd	__op_param2(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rsi
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	setp	%dl
	setne	%al
	orl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ucomisd	8(%rsi), %xmm0
	setne	%al
	setp	%dl
	orl	%edx, %eax
	movzbl	%al, %eax
	decq	%rax
	movq	%rax, 8(%rsi)
	ret
.LFE1336:
	.size	op_cmpeqpd, .-op_cmpeqpd
	.p2align 4,,15
.globl op_cmpnlesd
	.type	op_cmpnlesd, @function
op_cmpnlesd:
.LFB1361:
	movlpd	__op_param2(%rbp), %xmm0
	ucomisd	__op_param1(%rbp), %xmm0
	sbbq	%rdx, %rdx
	movq	%rdx, __op_param1(%rbp)
	ret
.LFE1361:
	.size	op_cmpnlesd, .-op_cmpnlesd
	.p2align 4,,15
.globl op_cmpnlepd
	.type	op_cmpnlepd, @function
op_cmpnlepd:
.LFB1360:
	movlpd	__op_param2(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	sbbq	%rdx, %rdx
	movq	%rdx, __op_param1(%rbp)
	ucomisd	8(%rcx), %xmm0
	sbbq	%rax, %rax
	movq	%rax, 8(%rcx)
	ret
.LFE1360:
	.size	op_cmpnlepd, .-op_cmpnlepd
	.p2align 4,,15
.globl op_cmplesd
	.type	op_cmplesd, @function
op_cmplesd:
.LFB1345:
	movlpd	__op_param2(%rbp), %xmm0
	ucomisd	__op_param1(%rbp), %xmm0
	sbbq	%rax, %rax
	notq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1345:
	.size	op_cmplesd, .-op_cmplesd
	.p2align 4,,15
.globl op_cmplepd
	.type	op_cmplepd, @function
op_cmplepd:
.LFB1344:
	movlpd	__op_param2(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	sbbq	%rax, %rax
	notq	%rax
	movq	%rax, __op_param1(%rbp)
	ucomisd	8(%rcx), %xmm0
	sbbq	%rax, %rax
	notq	%rax
	movq	%rax, 8(%rcx)
	ret
.LFE1344:
	.size	op_cmplepd, .-op_cmplepd
	.p2align 4,,15
.globl op_cmpnltsd
	.type	op_cmpnltsd, @function
op_cmpnltsd:
.LFB1357:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomisd	__op_param1(%rbp), %xmm0
	seta	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1357:
	.size	op_cmpnltsd, .-op_cmpnltsd
	.p2align 4,,15
.globl op_cmpnltpd
	.type	op_cmpnltpd, @function
op_cmpnltpd:
.LFB1356:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	seta	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	xorl	%eax, %eax
	ucomisd	8(%rcx), %xmm0
	seta	%al
	decq	%rax
	movq	%rax, 8(%rcx)
	ret
.LFE1356:
	.size	op_cmpnltpd, .-op_cmpnltpd
	.p2align 4,,15
.globl op_cmpltsd
	.type	op_cmpltsd, @function
op_cmpltsd:
.LFB1341:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomisd	__op_param1(%rbp), %xmm0
	setbe	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1341:
	.size	op_cmpltsd, .-op_cmpltsd
	.p2align 4,,15
.globl op_cmpltpd
	.type	op_cmpltpd, @function
op_cmpltpd:
.LFB1340:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	setbe	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	xorl	%eax, %eax
	ucomisd	8(%rcx), %xmm0
	setbe	%al
	decq	%rax
	movq	%rax, 8(%rcx)
	ret
.LFE1340:
	.size	op_cmpltpd, .-op_cmpltpd
	.p2align 4,,15
.globl op_cmpordsd
	.type	op_cmpordsd, @function
op_cmpordsd:
.LFB1365:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomisd	__op_param1(%rbp), %xmm0
	setp	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1365:
	.size	op_cmpordsd, .-op_cmpordsd
	.p2align 4,,15
.globl op_cmpordpd
	.type	op_cmpordpd, @function
op_cmpordpd:
.LFB1364:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	setp	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	xorl	%eax, %eax
	ucomisd	8(%rcx), %xmm0
	setp	%al
	decq	%rax
	movq	%rax, 8(%rcx)
	ret
.LFE1364:
	.size	op_cmpordpd, .-op_cmpordpd
	.p2align 4,,15
.globl op_cmpunordsd
	.type	op_cmpunordsd, @function
op_cmpunordsd:
.LFB1349:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	ucomisd	__op_param1(%rbp), %xmm0
	setnp	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	ret
.LFE1349:
	.size	op_cmpunordsd, .-op_cmpunordsd
	.p2align 4,,15
.globl op_cmpunordpd
	.type	op_cmpunordpd, @function
op_cmpunordpd:
.LFB1348:
	movlpd	__op_param2(%rbp), %xmm0
	xorl	%eax, %eax
	leaq	__op_param1(%rbp), %rcx
	ucomisd	__op_param1(%rbp), %xmm0
	movlpd	__op_param2+8(%rbp), %xmm0
	setnp	%al
	decq	%rax
	movq	%rax, __op_param1(%rbp)
	xorl	%eax, %eax
	ucomisd	8(%rcx), %xmm0
	setnp	%al
	decq	%rax
	movq	%rax, 8(%rcx)
	ret
.LFE1348:
	.size	op_cmpunordpd, .-op_cmpunordpd
	.p2align 4,,15
.globl op_addss
	.type	op_addss, @function
op_addss:
.LFB1275:
	movss	__op_param1(%rbp), %xmm0
	addss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1275:
	.size	op_addss, .-op_addss
	.p2align 4,,15
.globl op_addps
	.type	op_addps, @function
op_addps:
.LFB1274:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	addss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	addss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	addss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	addss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1274:
	.size	op_addps, .-op_addps
	.p2align 4,,15
.globl op_subss
	.type	op_subss, @function
op_subss:
.LFB1279:
	movss	__op_param1(%rbp), %xmm0
	subss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1279:
	.size	op_subss, .-op_subss
	.p2align 4,,15
.globl op_subps
	.type	op_subps, @function
op_subps:
.LFB1278:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	subss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	subss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	subss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	subss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1278:
	.size	op_subps, .-op_subps
	.p2align 4,,15
.globl op_mulss
	.type	op_mulss, @function
op_mulss:
.LFB1283:
	movss	__op_param1(%rbp), %xmm0
	mulss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1283:
	.size	op_mulss, .-op_mulss
	.p2align 4,,15
.globl op_mulps
	.type	op_mulps, @function
op_mulps:
.LFB1282:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	mulss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	mulss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	mulss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	mulss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1282:
	.size	op_mulps, .-op_mulps
	.p2align 4,,15
.globl op_addsd
	.type	op_addsd, @function
op_addsd:
.LFB1277:
	movlpd	__op_param1(%rbp), %xmm0
	addsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1277:
	.size	op_addsd, .-op_addsd
	.p2align 4,,15
.globl op_addpd
	.type	op_addpd, @function
op_addpd:
.LFB1276:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	addsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	addsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1276:
	.size	op_addpd, .-op_addpd
	.p2align 4,,15
.globl op_subsd
	.type	op_subsd, @function
op_subsd:
.LFB1281:
	movlpd	__op_param1(%rbp), %xmm0
	subsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1281:
	.size	op_subsd, .-op_subsd
	.p2align 4,,15
.globl op_subpd
	.type	op_subpd, @function
op_subpd:
.LFB1280:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	subsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	subsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1280:
	.size	op_subpd, .-op_subpd
	.p2align 4,,15
.globl op_mulsd
	.type	op_mulsd, @function
op_mulsd:
.LFB1285:
	movlpd	__op_param1(%rbp), %xmm0
	mulsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1285:
	.size	op_mulsd, .-op_mulsd
	.p2align 4,,15
.globl op_mulpd
	.type	op_mulpd, @function
op_mulpd:
.LFB1284:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	mulsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	mulsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1284:
	.size	op_mulpd, .-op_mulpd
	.p2align 4,,15
.globl op_divss
	.type	op_divss, @function
op_divss:
.LFB1287:
	movss	__op_param1(%rbp), %xmm0
	divss	__op_param2(%rbp), %xmm0
	movss	%xmm0, __op_param1(%rbp)
	ret
.LFE1287:
	.size	op_divss, .-op_divss
	.p2align 4,,15
.globl op_divps
	.type	op_divps, @function
op_divps:
.LFB1286:
	movss	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	divss	__op_param2(%rbp), %xmm0
	leaq	__op_param2(%rbp), %rcx
	movss	%xmm0, __op_param1(%rbp)
	movss	4(%rdx), %xmm0
	divss	4(%rcx), %xmm0
	movss	%xmm0, 4(%rdx)
	movss	8(%rdx), %xmm0
	divss	8(%rcx), %xmm0
	movss	%xmm0, 8(%rdx)
	movss	12(%rdx), %xmm0
	divss	12(%rcx), %xmm0
	movss	%xmm0, 12(%rdx)
	ret
.LFE1286:
	.size	op_divps, .-op_divps
	.p2align 4,,15
.globl op_cmpxchgl_raw_T0_T1_EAX_cc
	.type	op_cmpxchgl_raw_T0_T1_EAX_cc, @function
op_cmpxchgl_raw_T0_T1_EAX_cc:
.LFB909:
	movq	%r14, -16(%rsp)
.LCFI274:
	movq	%r15, -8(%rsp)
.LCFI275:
	subq	$24, %rsp
.LCFI276:
	movl	(%rbp), %eax
	movl	%ebx, %r14d
	movq	%rbp, %rdx
	movl	%eax, %r15d
	subl	%ebx, %r15d
	jne	.L2784
	movl	%r12d, %esi
	mov	%r13d, %edi
	movl	%r12d, %ebx
	call	remR3PhysWriteU32
	jmp	.L2786
	.p2align 4,,7
.L2784:
	movl	%ebx, (%rdx)
.L2786:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE909:
	.size	op_cmpxchgl_raw_T0_T1_EAX_cc, .-op_cmpxchgl_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_sbbl_raw_T0_T1_cc
	.type	op_sbbl_raw_T0_T1_cc, @function
op_sbbl_raw_T0_T1_cc:
.LFB908:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI277:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	subl	%r12d, %esi
	subl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	16(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE908:
	.size	op_sbbl_raw_T0_T1_cc, .-op_sbbl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_adcl_raw_T0_T1_cc
	.type	op_adcl_raw_T0_T1_cc, @function
op_adcl_raw_T0_T1_cc:
.LFB907:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI278:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	leal	(%rbx,%r12), %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	addl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	8(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE907:
	.size	op_adcl_raw_T0_T1_cc, .-op_adcl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrdl_raw_T0_T1_ECX_cc
	.type	op_shrdl_raw_T0_T1_ECX_cc, @function
op_shrdl_raw_T0_T1_ECX_cc:
.LFB906:
	movq	%r15, -8(%rsp)
.LCFI279:
	movq	%r14, -16(%rsp)
.LCFI280:
	subq	$24, %rsp
.LCFI281:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L2793
	movl	%ebx, %esi
	movl	%r15d, %ecx
	movl	%r12d, %eax
	shrl	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r14d
	subl	%r15d, %ecx
	mov	%r13d, %edi
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2793:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE906:
	.size	op_shrdl_raw_T0_T1_ECX_cc, .-op_shrdl_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_raw_T0_T1_im_cc
	.type	op_shrdl_raw_T0_T1_im_cc, @function
op_shrdl_raw_T0_T1_im_cc:
.LFB905:
	movq	%r14, -16(%rsp)
.LCFI282:
	leal	__op_param1(%rip), %r14d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movq	%r15, -8(%rsp)
.LCFI283:
	mov	%r13d, %edi
	movl	%r14d, %ecx
	subq	$24, %rsp
.LCFI284:
	movl	%ebx, %r15d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	%r14d, %ecx
	decl	%r14d
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	movl	%r14d, %ecx
	movq	8(%rsp), %r14
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r15d
	movl	%r15d, 40(%rbp)
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE905:
	.size	op_shrdl_raw_T0_T1_im_cc, .-op_shrdl_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_raw_T0_T1_ECX_cc
	.type	op_shldl_raw_T0_T1_ECX_cc, @function
op_shldl_raw_T0_T1_ECX_cc:
.LFB904:
	movq	%r15, -8(%rsp)
.LCFI285:
	movq	%r14, -16(%rsp)
.LCFI286:
	subq	$24, %rsp
.LCFI287:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L2799
	movl	%ebx, %esi
	movl	%r15d, %ecx
	movl	%r12d, %eax
	sall	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r14d
	subl	%r15d, %ecx
	mov	%r13d, %edi
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2799:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE904:
	.size	op_shldl_raw_T0_T1_ECX_cc, .-op_shldl_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shldl_raw_T0_T1_im_cc
	.type	op_shldl_raw_T0_T1_im_cc, @function
op_shldl_raw_T0_T1_im_cc:
.LFB903:
	movq	%r14, -16(%rsp)
.LCFI288:
	leal	__op_param1(%rip), %r14d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movq	%r15, -8(%rsp)
.LCFI289:
	mov	%r13d, %edi
	movl	%r14d, %ecx
	subq	$24, %rsp
.LCFI290:
	movl	%ebx, %r15d
	sall	%cl, %esi
	movl	$32, %ecx
	subl	%r14d, %ecx
	decl	%r14d
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	movl	%r14d, %ecx
	movq	8(%rsp), %r14
	movl	%ebx, 44(%rbp)
	sall	%cl, %r15d
	movl	%r15d, 40(%rbp)
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE903:
	.size	op_shldl_raw_T0_T1_im_cc, .-op_shldl_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_sarl_raw_T0_T1_cc
	.type	op_sarl_raw_T0_T1_cc, @function
op_sarl_raw_T0_T1_cc:
.LFB902:
	movq	%r15, -8(%rsp)
.LCFI291:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI292:
	subq	$24, %rsp
.LCFI293:
	andl	$31, %r15d
	je	.L2805
	movl	%r15d, %ecx
	movl	%ebx, %esi
	movl	%ebx, %r14d
	sarl	%cl, %esi
	mov	%r13d, %edi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2805:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE902:
	.size	op_sarl_raw_T0_T1_cc, .-op_sarl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_raw_T0_T1_cc
	.type	op_shrl_raw_T0_T1_cc, @function
op_shrl_raw_T0_T1_cc:
.LFB901:
	movq	%r15, -8(%rsp)
.LCFI294:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI295:
	subq	$24, %rsp
.LCFI296:
	andl	$31, %r15d
	je	.L2809
	movl	%r15d, %ecx
	movl	%ebx, %esi
	movl	%ebx, %r14d
	shrl	%cl, %esi
	mov	%r13d, %edi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2809:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE901:
	.size	op_shrl_raw_T0_T1_cc, .-op_shrl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shll_raw_T0_T1_cc
	.type	op_shll_raw_T0_T1_cc, @function
op_shll_raw_T0_T1_cc:
.LFB900:
	movq	%r15, -8(%rsp)
.LCFI297:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI298:
	subq	$24, %rsp
.LCFI299:
	andl	$31, %r15d
	je	.L2813
	movl	%r15d, %ecx
	movl	%ebx, %esi
	movl	%ebx, %r14d
	sall	%cl, %esi
	mov	%r13d, %edi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2813:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE900:
	.size	op_shll_raw_T0_T1_cc, .-op_shll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_raw_T0_T1_cc
	.type	op_rcrl_raw_T0_T1_cc, @function
op_rcrl_raw_T0_T1_cc:
.LFB899:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI300:
	movq	%r15, -8(%rsp)
.LCFI301:
	subq	$24, %rsp
.LCFI302:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L2817
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L2819
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L2819:
	movl	%esi, %ebx
	mov	%r13d, %edi
	andl	$-2050, %r15d
	call	remR3PhysWriteU32
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	movl	$1, 48(%rbp)
	xorl	%ebx, %eax
	sarl	$20, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L2817:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE899:
	.size	op_rcrl_raw_T0_T1_cc, .-op_rcrl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcll_raw_T0_T1_cc
	.type	op_rcll_raw_T0_T1_cc, @function
op_rcll_raw_T0_T1_cc:
.LFB898:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI303:
	movq	%r15, -8(%rsp)
.LCFI304:
	subq	$24, %rsp
.LCFI305:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L2823
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L2825
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L2825:
	movl	%esi, %ebx
	mov	%r13d, %edi
	andl	$-2050, %r15d
	call	remR3PhysWriteU32
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L2823:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE898:
	.size	op_rcll_raw_T0_T1_cc, .-op_rcll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_raw_T0_T1
	.type	op_rorl_raw_T0_T1, @function
op_rorl_raw_T0_T1:
.LFB897:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI306:
	andl	$31, %ecx
	je	.L2829
	movl	%ebx, %esi
	mov	%r13d, %edi
	rorl	%cl, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
.L2829:
	addq	$8, %rsp
	ret
.LFE897:
	.size	op_rorl_raw_T0_T1, .-op_rorl_raw_T0_T1
	.p2align 4,,15
.globl op_roll_raw_T0_T1
	.type	op_roll_raw_T0_T1, @function
op_roll_raw_T0_T1:
.LFB896:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI307:
	andl	$31, %ecx
	je	.L2833
	movl	%ebx, %esi
	mov	%r13d, %edi
	roll	%cl, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
.L2833:
	addq	$8, %rsp
	ret
.LFE896:
	.size	op_roll_raw_T0_T1, .-op_roll_raw_T0_T1
	.p2align 4,,15
.globl op_rorl_raw_T0_T1_cc
	.type	op_rorl_raw_T0_T1_cc, @function
op_rorl_raw_T0_T1_cc:
.LFB895:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI308:
	movq	%r15, -8(%rsp)
.LCFI309:
	subq	$24, %rsp
.LCFI310:
	andl	$31, %ecx
	je	.L2837
	movl	%ebx, %esi
	movl	%ebx, %r14d
	mov	%r13d, %edi
	rorl	%cl, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r14d
	shrl	$31, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2837:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE895:
	.size	op_rorl_raw_T0_T1_cc, .-op_rorl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_roll_raw_T0_T1_cc
	.type	op_roll_raw_T0_T1_cc, @function
op_roll_raw_T0_T1_cc:
.LFB894:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI311:
	movq	%r15, -8(%rsp)
.LCFI312:
	subq	$24, %rsp
.LCFI313:
	andl	$31, %ecx
	je	.L2841
	movl	%ebx, %esi
	movl	%ebx, %r14d
	mov	%r13d, %edi
	roll	%cl, %esi
	movl	%esi, %ebx
	call	remR3PhysWriteU32
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2841:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE894:
	.size	op_roll_raw_T0_T1_cc, .-op_roll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_stl_raw_T1_A0
	.type	op_stl_raw_T1_A0, @function
op_stl_raw_T1_A0:
.LFB575:
	subq	$8, %rsp
.LCFI314:
	mov	%r13d, %edi
	movl	%r12d, %esi
	call	remR3PhysWriteU32
	addq	$8, %rsp
	ret
.LFE575:
	.size	op_stl_raw_T1_A0, .-op_stl_raw_T1_A0
	.p2align 4,,15
.globl op_movw_eflags_T0
	.type	op_movw_eflags_T0, @function
op_movw_eflags_T0:
.LFB1040:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$16640, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andb	$190, %ah
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1040:
	.size	op_movw_eflags_T0, .-op_movw_eflags_T0
	.p2align 4,,15
.globl op_movl_eflags_T0
	.type	op_movl_eflags_T0, @function
op_movl_eflags_T0:
.LFB1039:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$2375936, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andl	$-2375937, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1039:
	.size	op_movl_eflags_T0, .-op_movl_eflags_T0
	.p2align 4,,15
.globl op_movw_eflags_T0_cpl0
	.type	op_movw_eflags_T0_cpl0, @function
op_movw_eflags_T0_cpl0:
.LFB1044:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$29440, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andb	$140, %ah
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1044:
	.size	op_movw_eflags_T0_cpl0, .-op_movw_eflags_T0_cpl0
	.p2align 4,,15
.globl op_movl_eflags_T0_cpl0
	.type	op_movl_eflags_T0_cpl0, @function
op_movl_eflags_T0_cpl0:
.LFB1043:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$2388736, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andl	$-2388737, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1043:
	.size	op_movl_eflags_T0_cpl0, .-op_movl_eflags_T0_cpl0
	.p2align 4,,15
.globl op_movw_eflags_T0_io
	.type	op_movw_eflags_T0_io, @function
op_movw_eflags_T0_io:
.LFB1042:
	movl	%ebx, %eax
	movl	%ebx, %edx
	movl	%ebx, %ecx
	andl	$2261, %eax
	shrl	$9, %edx
	andl	$17152, %ecx
	movl	%eax, 40(%rbp)
	andl	$2, %edx
	movl	$1, %eax
	subl	%edx, %eax
	movl	%eax, 52(%rbp)
	movl	36(%rbp), %eax
	andb	$188, %ah
	orl	%ecx, %eax
	movl	%eax, 36(%rbp)
	ret
.LFE1042:
	.size	op_movw_eflags_T0_io, .-op_movw_eflags_T0_io
	.p2align 4,,15
.globl op_divsd
	.type	op_divsd, @function
op_divsd:
.LFB1289:
	movlpd	__op_param1(%rbp), %xmm0
	divsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	ret
.LFE1289:
	.size	op_divsd, .-op_divsd
	.p2align 4,,15
.globl op_divpd
	.type	op_divpd, @function
op_divpd:
.LFB1288:
	movlpd	__op_param1(%rbp), %xmm0
	leaq	__op_param1(%rbp), %rdx
	divsd	__op_param2(%rbp), %xmm0
	movsd	%xmm0, __op_param1(%rbp)
	movlpd	8(%rdx), %xmm0
	divsd	__op_param2+8(%rbp), %xmm0
	movsd	%xmm0, 8(%rdx)
	ret
.LFE1288:
	.size	op_divpd, .-op_divpd
	.p2align 4,,15
.globl op_rolw_raw_T0_T1_cc
	.type	op_rolw_raw_T0_T1_cc, @function
op_rolw_raw_T0_T1_cc:
.LFB777:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI315:
	movq	%r15, -8(%rsp)
.LCFI316:
	subq	$24, %rsp
.LCFI317:
	testb	$31, %al
	je	.L2861
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r14d
	andl	$15, %edx
	movl	%eax, %esi
	mov	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2861:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE777:
	.size	op_rolw_raw_T0_T1_cc, .-op_rolw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_stw_raw_T1_A0
	.type	op_stw_raw_T1_A0, @function
op_stw_raw_T1_A0:
.LFB574:
	subq	$8, %rsp
.LCFI318:
	movzwl	%r12w, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	addq	$8, %rsp
	ret
.LFE574:
	.size	op_stw_raw_T1_A0, .-op_stw_raw_T1_A0
	.p2align 4,,15
.globl op_stw_raw_T0_A0
	.type	op_stw_raw_T0_A0, @function
op_stw_raw_T0_A0:
.LFB572:
	subq	$8, %rsp
.LCFI319:
	movzwl	%bx, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	addq	$8, %rsp
	ret
.LFE572:
	.size	op_stw_raw_T0_A0, .-op_stw_raw_T0_A0
	.p2align 4,,15
.globl op_sarw_raw_T0_T1_cc
	.type	op_sarw_raw_T0_T1_cc, @function
op_sarw_raw_T0_T1_cc:
.LFB785:
	movq	%r15, -8(%rsp)
.LCFI320:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI321:
	subq	$24, %rsp
.LCFI322:
	andl	$31, %r15d
	je	.L2869
	movswl	%bx,%r14d
	movl	%r15d, %ecx
	mov	%r13d, %edi
	movl	%r14d, %esi
	sarl	%cl, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2869:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE785:
	.size	op_sarw_raw_T0_T1_cc, .-op_sarw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_raw_T0_T1_cc
	.type	op_shrw_raw_T0_T1_cc, @function
op_shrw_raw_T0_T1_cc:
.LFB784:
	movq	%r15, -8(%rsp)
.LCFI323:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI324:
	subq	$24, %rsp
.LCFI325:
	andl	$31, %r15d
	je	.L2873
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	mov	%r13d, %edi
	movl	%r14d, %esi
	shrl	%cl, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2873:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE784:
	.size	op_shrw_raw_T0_T1_cc, .-op_shrw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_raw_T0_T1_cc
	.type	op_shlw_raw_T0_T1_cc, @function
op_shlw_raw_T0_T1_cc:
.LFB783:
	movq	%r14, -16(%rsp)
.LCFI326:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI327:
	subq	$24, %rsp
.LCFI328:
	andl	$31, %r14d
	je	.L2877
	movl	%r14d, %ecx
	movl	%ebx, %esi
	movl	%ebx, %r15d
	sall	%cl, %esi
	mov	%r13d, %edi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	leal	-1(%r14), %ecx
	movzwl	%r15w, %eax
	movl	%ebx, 44(%rbp)
	movl	$35, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L2877:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE783:
	.size	op_shlw_raw_T0_T1_cc, .-op_shlw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_raw_T0_T1_cc
	.type	op_rcrw_raw_T0_T1_cc, @function
op_rcrw_raw_T0_T1_cc:
.LFB782:
	pushq	%r15
.LCFI329:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI330:
	subq	$8, %rsp
.LCFI331:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L2881
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %edx
	andl	$1, %eax
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 4(%rsp)
	jle	.L2883
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L2883:
	movl	%edx, %ebx
	movzwl	%dx, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L2881:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE782:
	.size	op_rcrw_raw_T0_T1_cc, .-op_rcrw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rclw_raw_T0_T1_cc
	.type	op_rclw_raw_T0_T1_cc, @function
op_rclw_raw_T0_T1_cc:
.LFB781:
	pushq	%r15
.LCFI332:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI333:
	subq	$8, %rsp
.LCFI334:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L2887
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %edx
	andl	$1, %eax
	sall	%cl, %edx
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 4(%rsp)
	jle	.L2889
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L2889:
	movl	%edx, %ebx
	movzwl	%dx, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L2887:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE781:
	.size	op_rclw_raw_T0_T1_cc, .-op_rclw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_raw_T0_T1
	.type	op_rorw_raw_T0_T1, @function
op_rorw_raw_T0_T1:
.LFB780:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI335:
	andl	$15, %ecx
	je	.L2893
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	mov	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L2893:
	addq	$8, %rsp
	ret
.LFE780:
	.size	op_rorw_raw_T0_T1, .-op_rorw_raw_T0_T1
	.p2align 4,,15
.globl op_rolw_raw_T0_T1
	.type	op_rolw_raw_T0_T1, @function
op_rolw_raw_T0_T1:
.LFB779:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI336:
	andl	$15, %ecx
	je	.L2897
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	mov	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L2897:
	addq	$8, %rsp
	ret
.LFE779:
	.size	op_rolw_raw_T0_T1, .-op_rolw_raw_T0_T1
	.p2align 4,,15
.globl op_rorw_raw_T0_T1_cc
	.type	op_rorw_raw_T0_T1_cc, @function
op_rorw_raw_T0_T1_cc:
.LFB778:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI337:
	movq	%r15, -8(%rsp)
.LCFI338:
	subq	$24, %rsp
.LCFI339:
	testb	$31, %al
	je	.L2901
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r14d
	andl	$15, %edx
	movl	%eax, %esi
	mov	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r14d
	shrl	$15, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	andl	$1, %edx
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2901:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE778:
	.size	op_rorw_raw_T0_T1_cc, .-op_rorw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_raw_T0_T1_EAX_cc
	.type	op_cmpxchgw_raw_T0_T1_EAX_cc, @function
op_cmpxchgw_raw_T0_T1_EAX_cc:
.LFB792:
	movq	%r14, -16(%rsp)
.LCFI340:
	movq	%r15, -8(%rsp)
.LCFI341:
	subq	$24, %rsp
.LCFI342:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testw	%r15w, %r15w
	jne	.L2905
	movzwl	%r12w, %esi
	mov	%r13d, %edi
	movl	%r12d, %ebx
	call	remR3PhysWriteU16
	jmp	.L2907
	.p2align 4,,7
.L2905:
	xorw	%dx, %dx
	movzwl	%bx,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L2907:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE792:
	.size	op_cmpxchgw_raw_T0_T1_EAX_cc, .-op_cmpxchgw_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_sbbw_raw_T0_T1_cc
	.type	op_sbbw_raw_T0_T1_cc, @function
op_sbbw_raw_T0_T1_cc:
.LFB791:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI343:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	subl	%r12d, %esi
	subl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	leal	15(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE791:
	.size	op_sbbw_raw_T0_T1_cc, .-op_sbbw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_adcw_raw_T0_T1_cc
	.type	op_adcw_raw_T0_T1_cc, @function
op_adcw_raw_T0_T1_cc:
.LFB790:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI344:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	leal	(%rbx,%r12), %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	addl	%eax, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	leal	7(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE790:
	.size	op_adcw_raw_T0_T1_cc, .-op_adcw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrdw_raw_T0_T1_ECX_cc
	.type	op_shrdw_raw_T0_T1_ECX_cc, @function
op_shrdw_raw_T0_T1_ECX_cc:
.LFB789:
	movq	%r15, -8(%rsp)
.LCFI345:
	movq	%r14, -16(%rsp)
.LCFI346:
	subq	$24, %rsp
.LCFI347:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L2914
	movl	%r12d, %eax
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	sall	$16, %eax
	movl	%r12d, %edx
	orl	%eax, %r14d
	movl	%r14d, %eax
	shrl	%cl, %eax
	cmpl	$16, %r15d
	jle	.L2916
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %eax
.L2916:
	movl	%eax, %ebx
	movzwl	%ax, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2914:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE789:
	.size	op_shrdw_raw_T0_T1_ECX_cc, .-op_shrdw_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_raw_T0_T1_im_cc
	.type	op_shrdw_raw_T0_T1_im_cc, @function
op_shrdw_raw_T0_T1_im_cc:
.LFB788:
	pushq	%r15
.LCFI348:
	movl	%r12d, %eax
	leal	__op_param1(%rip), %r15d
	sall	$16, %eax
	movl	%r12d, %edx
	pushq	%r14
.LCFI349:
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	orl	%eax, %r14d
	movl	%r14d, %eax
	subq	$8, %rsp
.LCFI350:
	shrl	%cl, %eax
	cmpl	$16, %r15d
	jle	.L2920
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %eax
.L2920:
	movl	%eax, %ebx
	movzwl	%ax, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU16
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE788:
	.size	op_shrdw_raw_T0_T1_im_cc, .-op_shrdw_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_raw_T0_T1_ECX_cc
	.type	op_shldw_raw_T0_T1_ECX_cc, @function
op_shldw_raw_T0_T1_ECX_cc:
.LFB787:
	movq	%r15, -8(%rsp)
.LCFI351:
	movq	%r14, -16(%rsp)
.LCFI352:
	subq	$24, %rsp
.LCFI353:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L2924
	movl	%ebx, %r14d
	movzwl	%r12w,%eax
	movl	%r15d, %ecx
	sall	$16, %r14d
	movl	%eax, %r12d
	orl	%eax, %r14d
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L2926
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L2926:
	movl	%edx, %esi
	mov	%r13d, %edi
	shrl	$16, %esi
	movl	%esi, %ebx
	movzwl	%si, %esi
	call	remR3PhysWriteU16
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2924:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE787:
	.size	op_shldw_raw_T0_T1_ECX_cc, .-op_shldw_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_rorb_raw_T0_T1
	.type	op_rorb_raw_T0_T1, @function
op_rorb_raw_T0_T1:
.LFB685:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI354:
	andl	$7, %ecx
	je	.L2930
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	mov	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L2930:
	addq	$8, %rsp
	ret
.LFE685:
	.size	op_rorb_raw_T0_T1, .-op_rorb_raw_T0_T1
	.p2align 4,,15
.globl op_rolb_raw_T0_T1
	.type	op_rolb_raw_T0_T1, @function
op_rolb_raw_T0_T1:
.LFB684:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI355:
	andl	$7, %ecx
	je	.L2934
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	mov	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L2934:
	addq	$8, %rsp
	ret
.LFE684:
	.size	op_rolb_raw_T0_T1, .-op_rolb_raw_T0_T1
	.p2align 4,,15
.globl op_rorb_raw_T0_T1_cc
	.type	op_rorb_raw_T0_T1_cc, @function
op_rorb_raw_T0_T1_cc:
.LFB683:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI356:
	movq	%r15, -8(%rsp)
.LCFI357:
	subq	$24, %rsp
.LCFI358:
	testb	$31, %al
	je	.L2938
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r14d
	andl	$7, %edx
	movl	%eax, %esi
	mov	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r14d
	shrl	$7, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	andl	$1, %edx
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2938:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE683:
	.size	op_rorb_raw_T0_T1_cc, .-op_rorb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_raw_T0_T1_cc
	.type	op_rolb_raw_T0_T1_cc, @function
op_rolb_raw_T0_T1_cc:
.LFB682:
	movl	%r12d, %eax
	movq	%r14, -16(%rsp)
.LCFI359:
	movq	%r15, -8(%rsp)
.LCFI360:
	subq	$24, %rsp
.LCFI361:
	testb	$31, %al
	je	.L2942
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r14d
	andl	$7, %edx
	movl	%eax, %esi
	mov	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	mov	48(%rbp), %eax
	movq	%rbp, %r15
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r14d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r14d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r14d
	orl	%r14d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r15)
.L2942:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE682:
	.size	op_rolb_raw_T0_T1_cc, .-op_rolb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_stb_raw_T0_A0
	.type	op_stb_raw_T0_A0, @function
op_stb_raw_T0_A0:
.LFB571:
	subq	$8, %rsp
.LCFI362:
	movzbl	%bl, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU8
	addq	$8, %rsp
	ret
.LFE571:
	.size	op_stb_raw_T0_A0, .-op_stb_raw_T0_A0
	.p2align 4,,15
.globl op_cmpxchgb_raw_T0_T1_EAX_cc
	.type	op_cmpxchgb_raw_T0_T1_EAX_cc, @function
op_cmpxchgb_raw_T0_T1_EAX_cc:
.LFB693:
	movq	%r14, -16(%rsp)
.LCFI363:
	movq	%r15, -8(%rsp)
.LCFI364:
	subq	$24, %rsp
.LCFI365:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testb	%r15b, %r15b
	jne	.L2948
	movzbl	%r12b, %esi
	mov	%r13d, %edi
	movl	%r12d, %ebx
	call	remR3PhysWriteU8
	jmp	.L2950
	.p2align 4,,7
.L2948:
	xorb	%dl, %dl
	movzbl	%bl,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L2950:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE693:
	.size	op_cmpxchgb_raw_T0_T1_EAX_cc, .-op_cmpxchgb_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_sbbb_raw_T0_T1_cc
	.type	op_sbbb_raw_T0_T1_cc, @function
op_sbbb_raw_T0_T1_cc:
.LFB692:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI366:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	subl	%r12d, %esi
	subl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	leal	14(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE692:
	.size	op_sbbb_raw_T0_T1_cc, .-op_sbbb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_raw_T0_T1_cc
	.type	op_adcb_raw_T0_T1_cc, @function
op_adcb_raw_T0_T1_cc:
.LFB691:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI367:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	leal	(%rbx,%r12), %esi
	movl	%eax, %r14d
	mov	%r13d, %edi
	addl	%eax, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	leal	6(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE691:
	.size	op_adcb_raw_T0_T1_cc, .-op_adcb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_raw_T0_T1_cc
	.type	op_sarb_raw_T0_T1_cc, @function
op_sarb_raw_T0_T1_cc:
.LFB690:
	movq	%r15, -8(%rsp)
.LCFI368:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI369:
	subq	$24, %rsp
.LCFI370:
	andl	$31, %r15d
	je	.L2957
	movsbl	%bl,%r14d
	movl	%r15d, %ecx
	mov	%r13d, %edi
	movl	%r14d, %esi
	sarl	%cl, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2957:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE690:
	.size	op_sarb_raw_T0_T1_cc, .-op_sarb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_raw_T0_T1_cc
	.type	op_shrb_raw_T0_T1_cc, @function
op_shrb_raw_T0_T1_cc:
.LFB689:
	movq	%r15, -8(%rsp)
.LCFI371:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI372:
	subq	$24, %rsp
.LCFI373:
	andl	$31, %r15d
	je	.L2961
	movzbl	%bl,%r14d
	movl	%r15d, %ecx
	mov	%r13d, %edi
	movl	%r14d, %esi
	shrl	%cl, %esi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L2961:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE689:
	.size	op_shrb_raw_T0_T1_cc, .-op_shrb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_raw_T0_T1_cc
	.type	op_shlb_raw_T0_T1_cc, @function
op_shlb_raw_T0_T1_cc:
.LFB688:
	movq	%r14, -16(%rsp)
.LCFI374:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI375:
	subq	$24, %rsp
.LCFI376:
	andl	$31, %r14d
	je	.L2965
	movl	%r14d, %ecx
	movl	%ebx, %esi
	movl	%ebx, %r15d
	sall	%cl, %esi
	mov	%r13d, %edi
	movl	%esi, %ebx
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
	leal	-1(%r14), %ecx
	movzbl	%r15b, %eax
	movl	%ebx, 44(%rbp)
	movl	$34, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L2965:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE688:
	.size	op_shlb_raw_T0_T1_cc, .-op_shlb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_raw_T0_T1_cc
	.type	op_rcrb_raw_T0_T1_cc, @function
op_rcrb_raw_T0_T1_cc:
.LFB687:
	pushq	%r15
.LCFI377:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI378:
	subq	$8, %rsp
.LCFI379:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L2969
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %edx
	andl	$1, %eax
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 4(%rsp)
	jle	.L2971
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L2971:
	movl	%edx, %ebx
	movzbl	%dl, %esi
	mov	%r13d, %edi
	call	remR3PhysWriteU8
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sall	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L2969:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE687:
	.size	op_rcrb_raw_T0_T1_cc, .-op_rcrb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sto_raw_env_A0
	.type	op_sto_raw_env_A0, @function
op_sto_raw_env_A0:
.LFB579:
	pushq	%r14
.LCFI380:
	leaq	__op_param1(%rbp), %r14
	mov	%r13d, %edi
	movq	(%r14), %rsi
	call	remR3PhysWriteU64
	movq	8(%r14), %rsi
	leal	8(%r13), %edi
	call	remR3PhysWriteU64
	popq	%r14
	ret
.LFE579:
	.size	op_sto_raw_env_A0, .-op_sto_raw_env_A0
	.p2align 4,,15
.globl op_ldo_raw_env_A0
	.type	op_ldo_raw_env_A0, @function
op_ldo_raw_env_A0:
.LFB578:
	pushq	%r14
.LCFI381:
	mov	%r13d, %edi
	leaq	__op_param1(%rbp), %r14
	call	remR3PhysReadU64
	leal	8(%r13), %edi
	movq	%rax, (%r14)
	call	remR3PhysReadU64
	movq	%rax, 8(%r14)
	popq	%r14
	ret
.LFE578:
	.size	op_ldo_raw_env_A0, .-op_ldo_raw_env_A0
	.p2align 4,,15
.globl op_lduw_raw_T0_A0
	.type	op_lduw_raw_T0_A0, @function
op_lduw_raw_T0_A0:
.LFB563:
	subq	$8, %rsp
.LCFI382:
	mov	%r13d, %edi
	call	remR3PhysReadU16
	addq	$8, %rsp
	movzwl	%ax, %ebx
	ret
.LFE563:
	.size	op_lduw_raw_T0_A0, .-op_lduw_raw_T0_A0
	.p2align 4,,15
.globl op_ldsw_raw_T1_A0
	.type	op_ldsw_raw_T1_A0, @function
op_ldsw_raw_T1_A0:
.LFB569:
	subq	$8, %rsp
.LCFI383:
	mov	%r13d, %edi
	call	remR3PhysReadS16
	addq	$8, %rsp
	movswl	%ax,%r12d
	ret
.LFE569:
	.size	op_ldsw_raw_T1_A0, .-op_ldsw_raw_T1_A0
	.p2align 4,,15
.globl op_ldl_raw_T0_A0
	.type	op_ldl_raw_T0_A0, @function
op_ldl_raw_T0_A0:
.LFB565:
	subq	$8, %rsp
.LCFI384:
	mov	%r13d, %edi
	call	remR3PhysReadU32
	addq	$8, %rsp
	movl	%eax, %ebx
	ret
.LFE565:
	.size	op_ldl_raw_T0_A0, .-op_ldl_raw_T0_A0
	.p2align 4,,15
.globl op_ldub_raw_T1_A0
	.type	op_ldub_raw_T1_A0, @function
op_ldub_raw_T1_A0:
.LFB566:
	subq	$8, %rsp
.LCFI385:
	mov	%r13d, %edi
	call	remR3PhysReadU8
	addq	$8, %rsp
	movzbl	%al, %r12d
	ret
.LFE566:
	.size	op_ldub_raw_T1_A0, .-op_ldub_raw_T1_A0
	.p2align 4,,15
.globl op_ldsb_raw_T1_A0
	.type	op_ldsb_raw_T1_A0, @function
op_ldsb_raw_T1_A0:
.LFB567:
	subq	$8, %rsp
.LCFI386:
	mov	%r13d, %edi
	call	remR3PhysReadS8
	addq	$8, %rsp
	movsbl	%al,%r12d
	ret
.LFE567:
	.size	op_ldsb_raw_T1_A0, .-op_ldsb_raw_T1_A0
	.p2align 4,,15
.globl op_fldl_ST0_A0
	.type	op_fldl_ST0_A0, @function
op_fldl_ST0_A0:
.LFB1061:
	pushq	%r15
.LCFI387:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	pushq	%r14
.LCFI388:
	movzbl	%al, %eax
	movq	%rbp, %r14
	leaq	0(,%rax,8), %rcx
	subq	$8, %rsp
.LCFI389:
	movl	284(%rbp), %r15d
	movl	56(%rbp), %edx
	decl	%r15d
	andl	$3, %edx
	andl	$7, %r15d
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L2989
	call	__ldq_mmu
	movq	%rax, (%rsp)
	jmp	.L2991
	.p2align 4,,7
.L2989:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
	movq	%rax, (%rsp)
.L2991:
	movslq	%r15d,%rdx
	fldl	(%rsp)
	movq	%rdx, %rax
	salq	$4, %rax
	fstpt	304(%rax,%r14)
	movl	%r15d, 284(%rbp)
	movb	$0, 296(%rdx,%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1061:
	.size	op_fldl_ST0_A0, .-op_fldl_ST0_A0
	.p2align 4,,15
.globl op_flds_ST0_A0
	.type	op_flds_ST0_A0, @function
op_flds_ST0_A0:
.LFB1060:
	pushq	%r15
.LCFI390:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	pushq	%r14
.LCFI391:
	movzbl	%al, %eax
	movq	%rbp, %r14
	leaq	0(,%rax,8), %rcx
	subq	$8, %rsp
.LCFI392:
	movl	284(%rbp), %r15d
	movl	56(%rbp), %edx
	decl	%r15d
	andl	$3, %edx
	andl	$7, %r15d
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L2994
	call	__ldl_mmu
	movl	%eax, 4(%rsp)
	jmp	.L2996
	.p2align 4,,7
.L2994:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
	movl	%eax, 4(%rsp)
.L2996:
	movslq	%r15d,%rdx
	flds	4(%rsp)
	movq	%rdx, %rax
	salq	$4, %rax
	fstpt	304(%rax,%r14)
	movl	%r15d, 284(%rbp)
	movb	$0, 296(%rdx,%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1060:
	.size	op_flds_ST0_A0, .-op_flds_ST0_A0
	.p2align 4,,15
.globl op_ldub_kernel_T0_A0
	.type	op_ldub_kernel_T0_A0, @function
op_ldub_kernel_T0_A0:
.LFB580:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI393:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 824(%rdx)
	je	.L2999
	xorl	%esi, %esi
	call	__ldb_mmu
	movzbl	%al, %eax
	jmp	.L3001
	.p2align 4,,7
.L2999:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU8
	movzbl	%al, %eax
.L3001:
	movl	%eax, %ebx
	addq	$8, %rsp
	ret
.LFE580:
	.size	op_ldub_kernel_T0_A0, .-op_ldub_kernel_T0_A0
	.p2align 4,,15
.globl op_ldsb_kernel_T1_A0
	.type	op_ldsb_kernel_T1_A0, @function
op_ldsb_kernel_T1_A0:
.LFB586:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI394:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3004
	xorl	%esi, %esi
	call	__ldb_mmu
	movsbl	%al,%eax
	jmp	.L3006
	.p2align 4,,7
.L3004:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS8
	movsbl	%al,%eax
.L3006:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE586:
	.size	op_ldsb_kernel_T1_A0, .-op_ldsb_kernel_T1_A0
	.p2align 4,,15
.globl op_lduw_user_T1_A0
	.type	op_lduw_user_T1_A0, @function
op_lduw_user_T1_A0:
.LFB606:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI395:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3009
	movl	$1, %esi
	call	__ldw_mmu
	movzwl	%ax, %eax
	jmp	.L3011
	.p2align 4,,7
.L3009:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU16
	movzwl	%ax, %eax
.L3011:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE606:
	.size	op_lduw_user_T1_A0, .-op_lduw_user_T1_A0
	.p2align 4,,15
.globl op_ldsw_user_T1_A0
	.type	op_ldsw_user_T1_A0, @function
op_ldsw_user_T1_A0:
.LFB607:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI396:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3014
	movl	$1, %esi
	call	__ldw_mmu
	cwtl
	jmp	.L3016
	.p2align 4,,7
.L3014:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadS16
	cwtl
.L3016:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE607:
	.size	op_ldsw_user_T1_A0, .-op_ldsw_user_T1_A0
	.p2align 4,,15
.globl op_ldl_user_T1_A0
	.type	op_ldl_user_T1_A0, @function
op_ldl_user_T1_A0:
.LFB608:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI397:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3019
	movl	$1, %esi
	call	__ldl_mmu
	jmp	.L3021
	.p2align 4,,7
.L3019:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU32
.L3021:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE608:
	.size	op_ldl_user_T1_A0, .-op_ldl_user_T1_A0
	.p2align 4,,15
.globl op_lduw_kernel_T1_A0
	.type	op_lduw_kernel_T1_A0, @function
op_lduw_kernel_T1_A0:
.LFB587:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI398:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3024
	xorl	%esi, %esi
	call	__ldw_mmu
	movzwl	%ax, %eax
	jmp	.L3026
	.p2align 4,,7
.L3024:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU16
	movzwl	%ax, %eax
.L3026:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE587:
	.size	op_lduw_kernel_T1_A0, .-op_lduw_kernel_T1_A0
	.p2align 4,,15
.globl op_ldsw_kernel_T1_A0
	.type	op_ldsw_kernel_T1_A0, @function
op_ldsw_kernel_T1_A0:
.LFB588:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI399:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3029
	xorl	%esi, %esi
	call	__ldw_mmu
	cwtl
	jmp	.L3031
	.p2align 4,,7
.L3029:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	cwtl
.L3031:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE588:
	.size	op_ldsw_kernel_T1_A0, .-op_ldsw_kernel_T1_A0
	.p2align 4,,15
.globl op_ldl_kernel_T1_A0
	.type	op_ldl_kernel_T1_A0, @function
op_ldl_kernel_T1_A0:
.LFB589:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI400:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3034
	xorl	%esi, %esi
	call	__ldl_mmu
	jmp	.L3036
	.p2align 4,,7
.L3034:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
.L3036:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE589:
	.size	op_ldl_kernel_T1_A0, .-op_ldl_kernel_T1_A0
	.p2align 4,,15
.globl op_ldub_user_T1_A0
	.type	op_ldub_user_T1_A0, @function
op_ldub_user_T1_A0:
.LFB604:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI401:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3039
	movl	$1, %esi
	call	__ldb_mmu
	movzbl	%al, %eax
	jmp	.L3041
	.p2align 4,,7
.L3039:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU8
	movzbl	%al, %eax
.L3041:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE604:
	.size	op_ldub_user_T1_A0, .-op_ldub_user_T1_A0
	.p2align 4,,15
.globl op_ldsb_user_T1_A0
	.type	op_ldsb_user_T1_A0, @function
op_ldsb_user_T1_A0:
.LFB605:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI402:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3044
	movl	$1, %esi
	call	__ldb_mmu
	movsbl	%al,%eax
	jmp	.L3046
	.p2align 4,,7
.L3044:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadS8
	movsbl	%al,%eax
.L3046:
	movl	%eax, %r12d
	addq	$8, %rsp
	ret
.LFE605:
	.size	op_ldsb_user_T1_A0, .-op_ldsb_user_T1_A0
	.p2align 4,,15
.globl op_maskmov_mmx
	.type	op_maskmov_mmx, @function
op_maskmov_mmx:
.LFB1202:
	pushq	%r15
.LCFI403:
	leaq	__op_param1(%rbp), %rdx
	leaq	__op_param2(%rbp), %r15
	pushq	%r14
.LCFI404:
	xorl	%r14d, %r14d
	subq	$8, %rsp
.LCFI405:
	movq	%rdx, (%rsp)
	.p2align 4,,7
.L3049:
	cmpb	$0, (%r14,%r15)
	jns	.L3050
	movq	(%rsp), %rax
	leal	(%r13,%r14), %edi
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movzbl	(%r14,%rax), %r9d
	movl	%edi, %eax
	andl	$3, %edx
	shrl	$12, %eax
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	movslq	%r8d,%rdx
	salq	$5, %rax
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3052
	movzbl	%r9b, %esi
	movl	%r8d, %edx
	call	__stb_mmu
	jmp	.L3050
	.p2align 4,,7
.L3052:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	movzbl	%r9b, %esi
	call	remR3PhysWriteU8
	.p2align 4,,7
.L3050:
	incq	%r14
	cmpq	$8, %r14
	jne	.L3049
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1202:
	.size	op_maskmov_mmx, .-op_maskmov_mmx
	.p2align 4,,15
.globl op_stq_user_env_A0
	.type	op_stq_user_env_A0, @function
op_stq_user_env_A0:
.LFB615:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI406:
	movl	%r13d, %edi
	shrl	$12, %eax
	movq	__op_param1(%rbp), %rsi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3059
	movl	$1, %edx
	call	__stq_mmu
	jmp	.L3061
	.p2align 4,,7
.L3059:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU64
.L3061:
	addq	$8, %rsp
	ret
.LFE615:
	.size	op_stq_user_env_A0, .-op_stq_user_env_A0
	.p2align 4,,15
.globl op_sto_user_env_A0
	.type	op_sto_user_env_A0, @function
op_sto_user_env_A0:
.LFB617:
	movl	%r13d, %eax
	pushq	%r14
.LCFI407:
	movq	__op_param1(%rbp), %rsi
	shrl	$12, %eax
	leaq	__op_param1(%rbp), %r14
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3064
	movl	$1, %edx
	call	__stq_mmu
	jmp	.L3066
	.p2align 4,,7
.L3064:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU64
.L3066:
	leal	8(%r13), %edi
	movq	8(%r14), %rsi
	movl	%edi, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3067
	movl	$1, %edx
	call	__stq_mmu
	jmp	.L3069
	.p2align 4,,7
.L3067:
	mov	%edi, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU64
.L3069:
	popq	%r14
	ret
.LFE617:
	.size	op_sto_user_env_A0, .-op_sto_user_env_A0
	.p2align 4,,15
.globl op_sto_kernel_env_A0
	.type	op_sto_kernel_env_A0, @function
op_sto_kernel_env_A0:
.LFB598:
	movl	%r13d, %eax
	pushq	%r14
.LCFI408:
	movq	__op_param1(%rbp), %rsi
	shrl	$12, %eax
	leaq	__op_param1(%rbp), %r14
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3072
	xorl	%edx, %edx
	call	__stq_mmu
	jmp	.L3074
	.p2align 4,,7
.L3072:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU64
.L3074:
	leal	8(%r13), %edi
	movq	8(%r14), %rsi
	movl	%edi, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4089, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3075
	xorl	%edx, %edx
	call	__stq_mmu
	jmp	.L3077
	.p2align 4,,7
.L3075:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU64
.L3077:
	popq	%r14
	ret
.LFE598:
	.size	op_sto_kernel_env_A0, .-op_sto_kernel_env_A0
	.p2align 4,,15
.globl op_stq_kernel_env_A0
	.type	op_stq_kernel_env_A0, @function
op_stq_kernel_env_A0:
.LFB596:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI409:
	movl	%r13d, %edi
	shrl	$12, %eax
	movq	__op_param1(%rbp), %rsi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3080
	xorl	%edx, %edx
	call	__stq_mmu
	jmp	.L3082
	.p2align 4,,7
.L3080:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU64
.L3082:
	addq	$8, %rsp
	ret
.LFE596:
	.size	op_stq_kernel_env_A0, .-op_stq_kernel_env_A0
	.p2align 4,,15
.globl op_ldq_kernel_env_A0
	.type	op_ldq_kernel_env_A0, @function
op_ldq_kernel_env_A0:
.LFB595:
	movl	%r13d, %eax
	pushq	%r14
.LCFI410:
	movl	%r13d, %edi
	shrl	$12, %eax
	movq	%rbp, %r14
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3085
	xorl	%esi, %esi
	call	__ldq_mmu
	jmp	.L3087
	.p2align 4,,7
.L3085:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
.L3087:
	movq	%rax, __op_param1(%r14)
	popq	%r14
	ret
.LFE595:
	.size	op_ldq_kernel_env_A0, .-op_ldq_kernel_env_A0
	.p2align 4,,15
.globl op_ldo_kernel_env_A0
	.type	op_ldo_kernel_env_A0, @function
op_ldo_kernel_env_A0:
.LFB597:
	movl	%r13d, %eax
	pushq	%r14
.LCFI411:
	movl	%r13d, %edi
	shrl	$12, %eax
	leaq	__op_param1(%rbp), %r14
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3090
	xorl	%esi, %esi
	call	__ldq_mmu
	jmp	.L3092
	.p2align 4,,7
.L3090:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
.L3092:
	leal	8(%r13), %edi
	movq	%rax, (%r14)
	movl	%edi, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3093
	xorl	%esi, %esi
	call	__ldq_mmu
	jmp	.L3095
	.p2align 4,,7
.L3093:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
.L3095:
	movq	%rax, 8(%r14)
	popq	%r14
	ret
.LFE597:
	.size	op_ldo_kernel_env_A0, .-op_ldo_kernel_env_A0
	.p2align 4,,15
.globl op_ldo_user_env_A0
	.type	op_ldo_user_env_A0, @function
op_ldo_user_env_A0:
.LFB616:
	movl	%r13d, %eax
	pushq	%r14
.LCFI412:
	movl	%r13d, %edi
	shrl	$12, %eax
	leaq	__op_param1(%rbp), %r14
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3098
	movl	$1, %esi
	call	__ldq_mmu
	jmp	.L3100
	.p2align 4,,7
.L3098:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU64
.L3100:
	leal	8(%r13), %edi
	movq	%rax, (%r14)
	movl	%edi, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4089, %eax
	cmpl	%eax, 6968(%rdx)
	je	.L3101
	movl	$1, %esi
	call	__ldq_mmu
	jmp	.L3103
	.p2align 4,,7
.L3101:
	mov	%edi, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysReadU64
.L3103:
	movq	%rax, 8(%r14)
	popq	%r14
	ret
.LFE616:
	.size	op_ldo_user_env_A0, .-op_ldo_user_env_A0
	.p2align 4,,15
.globl op_fisttl_ST0_A0
	.type	op_fisttl_ST0_A0, @function
op_fisttl_ST0_A0:
.LFB1073:
	subq	$24, %rsp
.LCFI413:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int32_round_to_zero
	movl	56(%rbp), %edx
	movl	%eax, %r9d
	movl	%r13d, %eax
	shrl	$12, %eax
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3106
	movl	%r8d, %edx
	movl	%r9d, %esi
	call	__stl_mmu
	jmp	.L3108
	.p2align 4,,7
.L3106:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movl	%r9d, %esi
	call	remR3PhysWriteU32
.L3108:
	addq	$24, %rsp
	ret
.LFE1073:
	.size	op_fisttl_ST0_A0, .-op_fisttl_ST0_A0
	.p2align 4,,15
.globl op_fistl_ST0_A0
	.type	op_fistl_ST0_A0, @function
op_fistl_ST0_A0:
.LFB1070:
	subq	$24, %rsp
.LCFI414:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int32
	movl	56(%rbp), %edx
	movl	%eax, %r9d
	movl	%r13d, %eax
	shrl	$12, %eax
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3111
	movl	%r8d, %edx
	movl	%r9d, %esi
	call	__stl_mmu
	jmp	.L3113
	.p2align 4,,7
.L3111:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movl	%r9d, %esi
	call	remR3PhysWriteU32
.L3113:
	addq	$24, %rsp
	ret
.LFE1070:
	.size	op_fistl_ST0_A0, .-op_fistl_ST0_A0
	.p2align 4,,15
.globl op_fisttll_ST0_A0
	.type	op_fisttll_ST0_A0, @function
op_fisttll_ST0_A0:
.LFB1074:
	subq	$24, %rsp
.LCFI415:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int64_round_to_zero
	movl	56(%rbp), %edx
	movq	%rax, %r9
	movl	%r13d, %eax
	shrl	$12, %eax
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3116
	movl	%r8d, %edx
	movq	%r9, %rsi
	call	__stq_mmu
	jmp	.L3118
	.p2align 4,,7
.L3116:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movq	%r9, %rsi
	call	remR3PhysWriteU64
.L3118:
	addq	$24, %rsp
	ret
.LFE1074:
	.size	op_fisttll_ST0_A0, .-op_fisttll_ST0_A0
	.p2align 4,,15
.globl op_fistll_ST0_A0
	.type	op_fistll_ST0_A0, @function
op_fistll_ST0_A0:
.LFB1071:
	subq	$24, %rsp
.LCFI416:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int64
	movl	56(%rbp), %edx
	movq	%rax, %r9
	movl	%r13d, %eax
	shrl	$12, %eax
	xorl	%r8d, %r8d
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3121
	movl	%r8d, %edx
	movq	%r9, %rsi
	call	__stq_mmu
	jmp	.L3123
	.p2align 4,,7
.L3121:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movq	%r9, %rsi
	call	remR3PhysWriteU64
.L3123:
	addq	$24, %rsp
	ret
.LFE1071:
	.size	op_fistll_ST0_A0, .-op_fistll_ST0_A0
	.p2align 4,,15
.globl op_fildll_ST0_A0
	.type	op_fildll_ST0_A0, @function
op_fildll_ST0_A0:
.LFB1065:
	pushq	%r15
.LCFI417:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	pushq	%r14
.LCFI418:
	movzbl	%al, %eax
	movq	%rbp, %r14
	leaq	0(,%rax,8), %rcx
	subq	$8, %rsp
.LCFI419:
	movl	284(%rbp), %r15d
	movl	56(%rbp), %edx
	decl	%r15d
	andl	$3, %edx
	andl	$7, %r15d
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3126
	call	__ldq_mmu
	movq	%rax, %rcx
	jmp	.L3128
	.p2align 4,,7
.L3126:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
	movq	%rax, %rcx
.L3128:
	movq	%rcx, -128(%rsp)
	movslq	%r15d,%rdx
	fildll	-128(%rsp)
	movq	%rdx, %rax
	salq	$4, %rax
	fstpt	304(%rax,%r14)
	movl	%r15d, 284(%rbp)
	movb	$0, 296(%rdx,%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1065:
	.size	op_fildll_ST0_A0, .-op_fildll_ST0_A0
	.p2align 4,,15
.globl op_fildll_FT0_A0
	.type	op_fildll_FT0_A0, @function
op_fildll_FT0_A0:
.LFB1059:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	pushq	%r14
.LCFI420:
	movl	%r13d, %edi
	movzbl	%al, %eax
	movq	%rbp, %r14
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3131
	call	__ldq_mmu
	jmp	.L3133
	.p2align 4,,7
.L3131:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
.L3133:
	movq	%rax, -128(%rsp)
	fildll	-128(%rsp)
	fstpt	448(%r14)
	popq	%r14
	ret
.LFE1059:
	.size	op_fildll_FT0_A0, .-op_fildll_FT0_A0
	.p2align 4,,15
.globl op_fldl_FT0_A0
	.type	op_fldl_FT0_A0, @function
op_fldl_FT0_A0:
.LFB1056:
	pushq	%r14
.LCFI421:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movq	%rbp, %r14
	movl	%r13d, %edi
	subq	$16, %rsp
.LCFI422:
	movl	56(%rbp), %edx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rcx
	andl	$3, %edx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4089, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3136
	call	__ldq_mmu
	jmp	.L3138
	.p2align 4,,7
.L3136:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU64
.L3138:
	movq	%rax, 8(%rsp)
	fldl	8(%rsp)
	fstpt	448(%r14)
	addq	$16, %rsp
	popq	%r14
	ret
.LFE1056:
	.size	op_fldl_FT0_A0, .-op_fldl_FT0_A0
	.p2align 4,,15
.globl op_fild_ST0_A0
	.type	op_fild_ST0_A0, @function
op_fild_ST0_A0:
.LFB1063:
	pushq	%r15
.LCFI423:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	pushq	%r14
.LCFI424:
	movzbl	%al, %eax
	movq	%rbp, %r14
	leaq	0(,%rax,8), %rcx
	subq	$8, %rsp
.LCFI425:
	movl	284(%rbp), %r15d
	movl	56(%rbp), %edx
	decl	%r15d
	andl	$3, %edx
	andl	$7, %r15d
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3141
	call	__ldw_mmu
	movswl	%ax,%ecx
	jmp	.L3143
	.p2align 4,,7
.L3141:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	movswl	%ax,%ecx
.L3143:
	movl	%ecx, -128(%rsp)
	movslq	%r15d,%rdx
	fildl	-128(%rsp)
	movq	%rdx, %rax
	salq	$4, %rax
	fstpt	304(%rax,%r14)
	movl	%r15d, 284(%rbp)
	movb	$0, 296(%rdx,%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1063:
	.size	op_fild_ST0_A0, .-op_fild_ST0_A0
	.p2align 4,,15
.globl op_fild_FT0_A0
	.type	op_fild_FT0_A0, @function
op_fild_FT0_A0:
.LFB1057:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	pushq	%r14
.LCFI426:
	movl	%r13d, %edi
	movzbl	%al, %eax
	movq	%rbp, %r14
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3146
	call	__ldw_mmu
	cwtl
	jmp	.L3148
	.p2align 4,,7
.L3146:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	cwtl
.L3148:
	movl	%eax, -128(%rsp)
	fildl	-128(%rsp)
	fstpt	448(%r14)
	popq	%r14
	ret
.LFE1057:
	.size	op_fild_FT0_A0, .-op_fild_FT0_A0
	.p2align 4,,15
.globl op_boundw
	.type	op_boundw, @function
op_boundw:
.LFB632:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%r8d, %r8d
	shrl	$12, %eax
	pushq	%r14
.LCFI427:
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3151
	movl	%r8d, %esi
	call	__ldw_mmu
	movswl	%ax,%r14d
	jmp	.L3153
	.p2align 4,,7
.L3151:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	movswl	%ax,%r14d
.L3153:
	leal	2(%r13), %edi
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$3, %edx
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4095, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3154
	movl	%r8d, %esi
	call	__ldw_mmu
	movswl	%ax,%ecx
	jmp	.L3156
	.p2align 4,,7
.L3154:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadS16
	movswl	%ax,%ecx
.L3156:
	movswl	%bx,%eax
	cmpl	%eax, %r14d
	setg	%dl
	cmpl	%eax, %ecx
	setl	%al
	orb	%al, %dl
	je	.L3157
	movl	$5, %edi
	call	raise_exception
.L3157:
	popq	%r14
	ret
.LFE632:
	.size	op_boundw, .-op_boundw
	.p2align 4,,15
.globl op_fistt_ST0_A0
	.type	op_fistt_ST0_A0, @function
op_fistt_ST0_A0:
.LFB1072:
	subq	$24, %rsp
.LCFI428:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int32_round_to_zero
	movswl	%ax,%edx
	movzwl	%ax, %ecx
	movl	$32768, %r8d
	cmpl	%edx, %eax
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	cmove	%ecx, %r8d
	shrl	$12, %eax
	xorl	%r9d, %r9d
	movzbl	%al, %eax
	movl	%r13d, %edi
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r9b
	salq	$5, %rax
	movslq	%r9d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3164
	movl	%r9d, %edx
	movl	%r8d, %esi
	call	__stw_mmu
	jmp	.L3166
	.p2align 4,,7
.L3164:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movl	%r8d, %esi
	call	remR3PhysWriteU16
.L3166:
	addq	$24, %rsp
	ret
.LFE1072:
	.size	op_fistt_ST0_A0, .-op_fistt_ST0_A0
	.p2align 4,,15
.globl op_fist_ST0_A0
	.type	op_fist_ST0_A0, @function
op_fist_ST0_A0:
.LFB1069:
	subq	$24, %rsp
.LCFI429:
	mov	284(%rbp), %eax
	leaq	432(%rbp), %rdi
	salq	$4, %rax
	leaq	304(%rax,%rbp), %rdx
	movq	(%rdx), %rax
	movl	8(%rdx), %edx
	movq	%rax, (%rsp)
	movl	%edx, 8(%rsp)
	call	floatx80_to_int32
	movswl	%ax,%edx
	movzwl	%ax, %ecx
	movl	$32768, %r8d
	cmpl	%edx, %eax
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	cmove	%ecx, %r8d
	shrl	$12, %eax
	xorl	%r9d, %r9d
	movzbl	%al, %eax
	movl	%r13d, %edi
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r9b
	salq	$5, %rax
	movslq	%r9d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3172
	movl	%r9d, %edx
	movl	%r8d, %esi
	call	__stw_mmu
	jmp	.L3174
	.p2align 4,,7
.L3172:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movl	%r8d, %esi
	call	remR3PhysWriteU16
.L3174:
	addq	$24, %rsp
	ret
.LFE1069:
	.size	op_fist_ST0_A0, .-op_fist_ST0_A0
	.p2align 4,,15
.globl op_fnstcw_A0
	.type	op_fnstcw_A0, @function
op_fnstcw_A0:
.LFB1131:
	subq	$8, %rsp
.LCFI430:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	shrl	$12, %eax
	xorl	%r8d, %r8d
	movl	292(%rbp), %r9d
	movzbl	%al, %eax
	movl	%r13d, %edi
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3177
	movzwl	%r9w, %esi
	movl	%r8d, %edx
	call	__stw_mmu
	jmp	.L3179
	.p2align 4,,7
.L3177:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%r9w, %esi
	call	remR3PhysWriteU16
.L3179:
	addq	$8, %rsp
	ret
.LFE1131:
	.size	op_fnstcw_A0, .-op_fnstcw_A0
	.p2align 4,,15
.globl op_boundl
	.type	op_boundl, @function
op_boundl:
.LFB633:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%r8d, %r8d
	shrl	$12, %eax
	pushq	%r14
.LCFI431:
	movl	%r13d, %edi
	movzbl	%al, %eax
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%r8b
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3182
	movl	%r8d, %esi
	call	__ldl_mmu
	movl	%eax, %r14d
	jmp	.L3184
	.p2align 4,,7
.L3182:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
	movl	%eax, %r14d
.L3184:
	leal	4(%r13), %edi
	movl	56(%rbp), %edx
	xorl	%r8d, %r8d
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$3, %edx
	cmpl	$3, %edx
	movzbl	%al, %eax
	sete	%r8b
	leaq	0(,%rax,8), %rcx
	salq	$5, %rax
	movslq	%r8d,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%edi, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3185
	movl	%r8d, %esi
	call	__ldl_mmu
	movl	%eax, %ecx
	jmp	.L3187
	.p2align 4,,7
.L3185:
	mov	%edi, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
	movl	%eax, %ecx
.L3187:
	cmpl	%ebx, %r14d
	setg	%dl
	cmpl	%ebx, %ecx
	setl	%al
	orb	%al, %dl
	je	.L3188
	movl	$5, %edi
	call	raise_exception
.L3188:
	popq	%r14
	ret
.LFE633:
	.size	op_boundl, .-op_boundl
	.p2align 4,,15
.globl op_fildl_ST0_A0
	.type	op_fildl_ST0_A0, @function
op_fildl_ST0_A0:
.LFB1064:
	pushq	%r15
.LCFI432:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	pushq	%r14
.LCFI433:
	movzbl	%al, %eax
	movq	%rbp, %r14
	leaq	0(,%rax,8), %rcx
	subq	$8, %rsp
.LCFI434:
	movl	284(%rbp), %r15d
	movl	56(%rbp), %edx
	decl	%r15d
	andl	$3, %edx
	andl	$7, %r15d
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3192
	call	__ldl_mmu
	movl	%eax, %ecx
	jmp	.L3194
	.p2align 4,,7
.L3192:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
	movl	%eax, %ecx
.L3194:
	movl	%ecx, -128(%rsp)
	movslq	%r15d,%rdx
	fildl	-128(%rsp)
	movq	%rdx, %rax
	salq	$4, %rax
	fstpt	304(%rax,%r14)
	movl	%r15d, 284(%rbp)
	movb	$0, 296(%rdx,%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE1064:
	.size	op_fildl_ST0_A0, .-op_fildl_ST0_A0
	.p2align 4,,15
.globl op_fildl_FT0_A0
	.type	op_fildl_FT0_A0, @function
op_fildl_FT0_A0:
.LFB1058:
	movl	56(%rbp), %edx
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	pushq	%r14
.LCFI435:
	movl	%r13d, %edi
	movzbl	%al, %eax
	movq	%rbp, %r14
	andl	$3, %edx
	leaq	0(,%rax,8), %rcx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3197
	call	__ldl_mmu
	jmp	.L3199
	.p2align 4,,7
.L3197:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
.L3199:
	movl	%eax, -128(%rsp)
	fildl	-128(%rsp)
	fstpt	448(%r14)
	popq	%r14
	ret
.LFE1058:
	.size	op_fildl_FT0_A0, .-op_fildl_FT0_A0
	.p2align 4,,15
.globl op_flds_FT0_A0
	.type	op_flds_FT0_A0, @function
op_flds_FT0_A0:
.LFB1055:
	pushq	%r14
.LCFI436:
	movl	%r13d, %eax
	xorl	%esi, %esi
	shrl	$12, %eax
	movq	%rbp, %r14
	movl	%r13d, %edi
	subq	$16, %rsp
.LCFI437:
	movl	56(%rbp), %edx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rcx
	andl	$3, %edx
	cmpl	$3, %edx
	sete	%sil
	salq	$5, %rax
	movslq	%esi,%rdx
	subq	%rcx, %rax
	movq	%rdx, %rcx
	salq	$13, %rdx
	salq	$11, %rcx
	subq	%rcx, %rdx
	addq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 824(%rdx)
	je	.L3202
	call	__ldl_mmu
	jmp	.L3204
	.p2align 4,,7
.L3202:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysReadU32
.L3204:
	movl	%eax, 12(%rsp)
	flds	12(%rsp)
	fstpt	448(%r14)
	addq	$16, %rsp
	popq	%r14
	ret
.LFE1055:
	.size	op_flds_FT0_A0, .-op_flds_FT0_A0
	.p2align 4,,15
.globl op_sbbb_kernel_T0_T1_cc
	.type	op_sbbb_kernel_T0_T1_cc, @function
op_sbbb_kernel_T0_T1_cc:
.LFB704:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI438:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %ecx
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %ecx
	subl	%eax, %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%ecx, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3207
	movzbl	%cl, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3209
	.p2align 4,,7
.L3207:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%cl, %esi
	call	remR3PhysWriteU8
.L3209:
	leal	14(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE704:
	.size	op_sbbb_kernel_T0_T1_cc, .-op_sbbb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_kernel_T0_T1_cc
	.type	op_adcb_kernel_T0_T1_cc, @function
op_adcb_kernel_T0_T1_cc:
.LFB703:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI439:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3212
	movzbl	%cl, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3214
	.p2align 4,,7
.L3212:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%cl, %esi
	call	remR3PhysWriteU8
.L3214:
	leal	6(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE703:
	.size	op_adcb_kernel_T0_T1_cc, .-op_adcb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_user_T0_T1_cc
	.type	op_adcb_user_T0_T1_cc, @function
op_adcb_user_T0_T1_cc:
.LFB715:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI440:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3217
	movzbl	%cl, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3219
	.p2align 4,,7
.L3217:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%cl, %esi
	call	remR3PhysWriteU8
.L3219:
	leal	6(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE715:
	.size	op_adcb_user_T0_T1_cc, .-op_adcb_user_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_user_T0_T1_cc
	.type	op_sarb_user_T0_T1_cc, @function
op_sarb_user_T0_T1_cc:
.LFB714:
	movq	%r15, -8(%rsp)
.LCFI441:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI442:
	subq	$24, %rsp
.LCFI443:
	andl	$31, %r15d
	je	.L3222
	movl	%r13d, %eax
	movsbl	%bl,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	sarl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3224
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3226
	.p2align 4,,7
.L3224:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3226:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3222:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE714:
	.size	op_sarb_user_T0_T1_cc, .-op_sarb_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_user_T0_T1_cc
	.type	op_shrb_user_T0_T1_cc, @function
op_shrb_user_T0_T1_cc:
.LFB713:
	movq	%r15, -8(%rsp)
.LCFI444:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI445:
	subq	$24, %rsp
.LCFI446:
	andl	$31, %r15d
	je	.L3229
	movl	%r13d, %eax
	movzbl	%bl,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	shrl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3231
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3233
	.p2align 4,,7
.L3231:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3233:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3229:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE713:
	.size	op_shrb_user_T0_T1_cc, .-op_shrb_user_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_user_T0_T1_cc
	.type	op_shlb_user_T0_T1_cc, @function
op_shlb_user_T0_T1_cc:
.LFB712:
	movq	%r14, -16(%rsp)
.LCFI447:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI448:
	subq	$24, %rsp
.LCFI449:
	andl	$31, %r14d
	je	.L3236
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r14d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r15d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3238
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3240
	.p2align 4,,7
.L3238:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3240:
	leal	-1(%r14), %ecx
	movzbl	%r15b, %eax
	movl	%ebx, 44(%rbp)
	movl	$34, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L3236:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE712:
	.size	op_shlb_user_T0_T1_cc, .-op_shlb_user_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_kernel_T0_T1_cc
	.type	op_sarb_kernel_T0_T1_cc, @function
op_sarb_kernel_T0_T1_cc:
.LFB702:
	movq	%r15, -8(%rsp)
.LCFI450:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI451:
	subq	$24, %rsp
.LCFI452:
	andl	$31, %r15d
	je	.L3243
	movl	%r13d, %eax
	movsbl	%bl,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	sarl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3245
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3247
	.p2align 4,,7
.L3245:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3247:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3243:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE702:
	.size	op_sarb_kernel_T0_T1_cc, .-op_sarb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_kernel_T0_T1_cc
	.type	op_shrb_kernel_T0_T1_cc, @function
op_shrb_kernel_T0_T1_cc:
.LFB701:
	movq	%r15, -8(%rsp)
.LCFI453:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI454:
	subq	$24, %rsp
.LCFI455:
	andl	$31, %r15d
	je	.L3250
	movl	%r13d, %eax
	movzbl	%bl,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	shrl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3252
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3254
	.p2align 4,,7
.L3252:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3254:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$38, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3250:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE701:
	.size	op_shrb_kernel_T0_T1_cc, .-op_shrb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_kernel_T0_T1_cc
	.type	op_shlb_kernel_T0_T1_cc, @function
op_shlb_kernel_T0_T1_cc:
.LFB700:
	movq	%r14, -16(%rsp)
.LCFI456:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI457:
	subq	$24, %rsp
.LCFI458:
	andl	$31, %r14d
	je	.L3257
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r14d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r15d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3259
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3261
	.p2align 4,,7
.L3259:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3261:
	leal	-1(%r14), %ecx
	movzbl	%r15b, %eax
	movl	%ebx, 44(%rbp)
	movl	$34, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L3257:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE700:
	.size	op_shlb_kernel_T0_T1_cc, .-op_shlb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_kernel_T0_T1_cc
	.type	op_rcrb_kernel_T0_T1_cc, @function
op_rcrb_kernel_T0_T1_cc:
.LFB699:
	pushq	%r15
.LCFI459:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI460:
	subq	$8, %rsp
.LCFI461:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L3264
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	shrl	%cl, %esi
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3266
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3266:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3268
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3270
	.p2align 4,,7
.L3268:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3270:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sall	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3264:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE699:
	.size	op_rcrb_kernel_T0_T1_cc, .-op_rcrb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_stb_user_T0_A0
	.type	op_stb_user_T0_A0, @function
op_stb_user_T0_A0:
.LFB609:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI462:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3273
	movzbl	%bl, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3275
	.p2align 4,,7
.L3273:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%bl, %esi
	call	remR3PhysWriteU8
.L3275:
	addq	$8, %rsp
	ret
.LFE609:
	.size	op_stb_user_T0_A0, .-op_stb_user_T0_A0
	.p2align 4,,15
.globl op_rorb_user_T0_T1_cc
	.type	op_rorb_user_T0_T1_cc, @function
op_rorb_user_T0_T1_cc:
.LFB707:
	pushq	%r15
.LCFI463:
	movl	%r12d, %eax
	pushq	%r14
.LCFI464:
	subq	$8, %rsp
.LCFI465:
	testb	$31, %al
	je	.L3278
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r15d
	andl	$7, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3280
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3282
	.p2align 4,,7
.L3280:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3282:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r15d
	shrl	$7, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	andl	$1, %edx
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3278:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE707:
	.size	op_rorb_user_T0_T1_cc, .-op_rorb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_user_T0_T1_cc
	.type	op_rolb_user_T0_T1_cc, @function
op_rolb_user_T0_T1_cc:
.LFB706:
	pushq	%r15
.LCFI466:
	movl	%r12d, %eax
	pushq	%r14
.LCFI467:
	subq	$8, %rsp
.LCFI468:
	testb	$31, %al
	je	.L3285
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r15d
	andl	$7, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3287
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3289
	.p2align 4,,7
.L3287:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3289:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3285:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE706:
	.size	op_rolb_user_T0_T1_cc, .-op_rolb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_user_T0_T1_cc
	.type	op_rcrb_user_T0_T1_cc, @function
op_rcrb_user_T0_T1_cc:
.LFB711:
	pushq	%r15
.LCFI469:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI470:
	subq	$8, %rsp
.LCFI471:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L3292
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	shrl	%cl, %esi
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3294
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3294:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3296
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3298
	.p2align 4,,7
.L3296:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3298:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sall	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3292:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE711:
	.size	op_rcrb_user_T0_T1_cc, .-op_rcrb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rclb_user_T0_T1_cc
	.type	op_rclb_user_T0_T1_cc, @function
op_rclb_user_T0_T1_cc:
.LFB710:
	pushq	%r15
.LCFI472:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI473:
	subq	$8, %rsp
.LCFI474:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L3301
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3303
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3303:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3305
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3307
	.p2align 4,,7
.L3305:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3307:
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3301:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE710:
	.size	op_rclb_user_T0_T1_cc, .-op_rclb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_user_T0_T1
	.type	op_rorb_user_T0_T1, @function
op_rorb_user_T0_T1:
.LFB709:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI475:
	andl	$7, %ecx
	je	.L3310
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3312
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3310
	.p2align 4,,7
.L3312:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3310:
	addq	$8, %rsp
	ret
.LFE709:
	.size	op_rorb_user_T0_T1, .-op_rorb_user_T0_T1
	.p2align 4,,15
.globl op_rolb_user_T0_T1
	.type	op_rolb_user_T0_T1, @function
op_rolb_user_T0_T1:
.LFB708:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI476:
	andl	$7, %ecx
	je	.L3316
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3318
	movzbl	%sil, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3316
	.p2align 4,,7
.L3318:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3316:
	addq	$8, %rsp
	ret
.LFE708:
	.size	op_rolb_user_T0_T1, .-op_rolb_user_T0_T1
	.p2align 4,,15
.globl op_cmpxchgb_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgb_kernel_T0_T1_EAX_cc, @function
op_cmpxchgb_kernel_T0_T1_EAX_cc:
.LFB705:
	movq	%r14, -16(%rsp)
.LCFI477:
	movq	%r15, -8(%rsp)
.LCFI478:
	subq	$24, %rsp
.LCFI479:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testb	%r15b, %r15b
	jne	.L3322
	movl	%r13d, %eax
	movl	%r12d, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3324
	movzbl	%r12b, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3326
	.p2align 4,,7
.L3324:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%r12b, %esi
	call	remR3PhysWriteU8
	jmp	.L3326
	.p2align 4,,7
.L3322:
	xorb	%dl, %dl
	movzbl	%bl,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L3326:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE705:
	.size	op_cmpxchgb_kernel_T0_T1_EAX_cc, .-op_cmpxchgb_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_cmpxchgb_user_T0_T1_EAX_cc
	.type	op_cmpxchgb_user_T0_T1_EAX_cc, @function
op_cmpxchgb_user_T0_T1_EAX_cc:
.LFB717:
	movq	%r14, -16(%rsp)
.LCFI480:
	movq	%r15, -8(%rsp)
.LCFI481:
	subq	$24, %rsp
.LCFI482:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testb	%r15b, %r15b
	jne	.L3329
	movl	%r13d, %eax
	movl	%r12d, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3331
	movzbl	%r12b, %esi
	movl	$1, %edx
	call	__stb_mmu
	jmp	.L3333
	.p2align 4,,7
.L3331:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzbl	%r12b, %esi
	call	remR3PhysWriteU8
	jmp	.L3333
	.p2align 4,,7
.L3329:
	xorb	%dl, %dl
	movzbl	%bl,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L3333:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE717:
	.size	op_cmpxchgb_user_T0_T1_EAX_cc, .-op_cmpxchgb_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolb_kernel_T0_T1_cc
	.type	op_rolb_kernel_T0_T1_cc, @function
op_rolb_kernel_T0_T1_cc:
.LFB694:
	pushq	%r15
.LCFI483:
	movl	%r12d, %eax
	pushq	%r14
.LCFI484:
	subq	$8, %rsp
.LCFI485:
	testb	$31, %al
	je	.L3336
	movl	%r12d, %edx
	movzbl	%bl,%eax
	movl	%ebx, %r15d
	andl	$7, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$8, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3338
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3340
	.p2align 4,,7
.L3338:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3340:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sall	$4, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3336:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE694:
	.size	op_rolb_kernel_T0_T1_cc, .-op_rolb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_stb_kernel_T0_A0
	.type	op_stb_kernel_T0_A0, @function
op_stb_kernel_T0_A0:
.LFB590:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI486:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3343
	movzbl	%bl, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3345
	.p2align 4,,7
.L3343:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%bl, %esi
	call	remR3PhysWriteU8
.L3345:
	addq	$8, %rsp
	ret
.LFE590:
	.size	op_stb_kernel_T0_A0, .-op_stb_kernel_T0_A0
	.p2align 4,,15
.globl op_rclb_kernel_T0_T1_cc
	.type	op_rclb_kernel_T0_T1_cc, @function
op_rclb_kernel_T0_T1_cc:
.LFB698:
	pushq	%r15
.LCFI487:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI488:
	subq	$8, %rsp
.LCFI489:
	movzbl	rclb_table(%rax), %eax
	testb	%al, %al
	je	.L3348
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzbl	%bl,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3350
	movl	$9, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3350:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3352
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3354
	.p2align 4,,7
.L3352:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3354:
	movl	$8, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3348:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE698:
	.size	op_rclb_kernel_T0_T1_cc, .-op_rclb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_kernel_T0_T1
	.type	op_rorb_kernel_T0_T1, @function
op_rorb_kernel_T0_T1:
.LFB697:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI490:
	andl	$7, %ecx
	je	.L3357
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3359
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3357
	.p2align 4,,7
.L3359:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3357:
	addq	$8, %rsp
	ret
.LFE697:
	.size	op_rorb_kernel_T0_T1, .-op_rorb_kernel_T0_T1
	.p2align 4,,15
.globl op_rolb_kernel_T0_T1
	.type	op_rolb_kernel_T0_T1, @function
op_rolb_kernel_T0_T1:
.LFB696:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI491:
	andl	$7, %ecx
	je	.L3363
	movzbl	%bl,%eax
	movl	$8, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4096, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3365
	movzbl	%sil, %esi
	xorl	%edx, %edx
	call	__stb_mmu
	jmp	.L3363
	.p2align 4,,7
.L3365:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzbl	%sil, %esi
	call	remR3PhysWriteU8
.L3363:
	addq	$8, %rsp
	ret
.LFE696:
	.size	op_rolb_kernel_T0_T1, .-op_rolb_kernel_T0_T1
	.p2align 4,,15
.globl op_stl_user_T0_A0
	.type	op_stl_user_T0_A0, @function
op_stl_user_T0_A0:
.LFB611:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI492:
	movl	%ebx, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3369
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3371
	.p2align 4,,7
.L3369:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3371:
	addq	$8, %rsp
	ret
.LFE611:
	.size	op_stl_user_T0_A0, .-op_stl_user_T0_A0
	.p2align 4,,15
.globl op_cmpxchgl_user_T0_T1_EAX_cc
	.type	op_cmpxchgl_user_T0_T1_EAX_cc, @function
op_cmpxchgl_user_T0_T1_EAX_cc:
.LFB941:
	movq	%r14, -16(%rsp)
.LCFI493:
	movq	%r15, -8(%rsp)
.LCFI494:
	subq	$24, %rsp
.LCFI495:
	movl	(%rbp), %eax
	movl	%ebx, %r14d
	movq	%rbp, %rcx
	movl	%eax, %r15d
	subl	%ebx, %r15d
	jne	.L3374
	movl	%r13d, %eax
	movl	%r12d, %esi
	movl	%r12d, %ebx
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3376
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3378
	.p2align 4,,7
.L3376:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
	jmp	.L3378
	.p2align 4,,7
.L3374:
	movl	%ebx, (%rcx)
.L3378:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE941:
	.size	op_cmpxchgl_user_T0_T1_EAX_cc, .-op_cmpxchgl_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_sbbl_user_T0_T1_cc
	.type	op_sbbl_user_T0_T1_cc, @function
op_sbbl_user_T0_T1_cc:
.LFB940:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI496:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %esi
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %esi
	subl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3381
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3383
	.p2align 4,,7
.L3381:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3383:
	leal	16(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE940:
	.size	op_sbbl_user_T0_T1_cc, .-op_sbbl_user_T0_T1_cc
	.p2align 4,,15
.globl op_adcl_user_T0_T1_cc
	.type	op_adcl_user_T0_T1_cc, @function
op_adcl_user_T0_T1_cc:
.LFB939:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI497:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3386
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3388
	.p2align 4,,7
.L3386:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3388:
	leal	8(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE939:
	.size	op_adcl_user_T0_T1_cc, .-op_adcl_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrdl_user_T0_T1_ECX_cc
	.type	op_shrdl_user_T0_T1_ECX_cc, @function
op_shrdl_user_T0_T1_ECX_cc:
.LFB938:
	movq	%r14, -16(%rsp)
.LCFI498:
	movq	%r15, -8(%rsp)
.LCFI499:
	subq	$24, %rsp
.LCFI500:
	movl	4(%rbp), %r14d
	andl	$31, %r14d
	je	.L3391
	movl	%ebx, %esi
	movl	%r14d, %ecx
	movl	%r12d, %eax
	shrl	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r15d
	subl	%r14d, %ecx
	movl	%r13d, %edi
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3393
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3395
	.p2align 4,,7
.L3393:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3395:
	leal	-1(%r14), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r15d
	movl	%r15d, 40(%rbp)
.L3391:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE938:
	.size	op_shrdl_user_T0_T1_ECX_cc, .-op_shrdl_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_user_T0_T1_im_cc
	.type	op_shrdl_user_T0_T1_im_cc, @function
op_shrdl_user_T0_T1_im_cc:
.LFB937:
	pushq	%r15
.LCFI501:
	leal	__op_param1(%rip), %r15d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movl	%r13d, %edi
	movl	%r15d, %ecx
	pushq	%r14
.LCFI502:
	movl	%ebx, %r14d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	%r15d, %ecx
	subq	$8, %rsp
.LCFI503:
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3398
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3400
	.p2align 4,,7
.L3398:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3400:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE937:
	.size	op_shrdl_user_T0_T1_im_cc, .-op_shrdl_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_user_T0_T1_ECX_cc
	.type	op_shldl_user_T0_T1_ECX_cc, @function
op_shldl_user_T0_T1_ECX_cc:
.LFB936:
	movq	%r14, -16(%rsp)
.LCFI504:
	movq	%r15, -8(%rsp)
.LCFI505:
	subq	$24, %rsp
.LCFI506:
	movl	4(%rbp), %r14d
	andl	$31, %r14d
	je	.L3403
	movl	%ebx, %esi
	movl	%r14d, %ecx
	movl	%r12d, %eax
	sall	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r15d
	subl	%r14d, %ecx
	movl	%r13d, %edi
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3405
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3407
	.p2align 4,,7
.L3405:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3407:
	leal	-1(%r14), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r15d
	movl	%r15d, 40(%rbp)
.L3403:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE936:
	.size	op_shldl_user_T0_T1_ECX_cc, .-op_shldl_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shldl_user_T0_T1_im_cc
	.type	op_shldl_user_T0_T1_im_cc, @function
op_shldl_user_T0_T1_im_cc:
.LFB935:
	pushq	%r15
.LCFI507:
	leal	__op_param1(%rip), %r15d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movl	%r13d, %edi
	movl	%r15d, %ecx
	pushq	%r14
.LCFI508:
	movl	%ebx, %r14d
	sall	%cl, %esi
	movl	$32, %ecx
	subl	%r15d, %ecx
	subq	$8, %rsp
.LCFI509:
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3410
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3412
	.p2align 4,,7
.L3410:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3412:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE935:
	.size	op_shldl_user_T0_T1_im_cc, .-op_shldl_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_sarl_user_T0_T1_cc
	.type	op_sarl_user_T0_T1_cc, @function
op_sarl_user_T0_T1_cc:
.LFB934:
	movq	%r15, -8(%rsp)
.LCFI510:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI511:
	subq	$24, %rsp
.LCFI512:
	andl	$31, %r15d
	je	.L3415
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	sarl	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3417
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3419
	.p2align 4,,7
.L3417:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3419:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3415:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE934:
	.size	op_sarl_user_T0_T1_cc, .-op_sarl_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_user_T0_T1_cc
	.type	op_shrl_user_T0_T1_cc, @function
op_shrl_user_T0_T1_cc:
.LFB933:
	movq	%r15, -8(%rsp)
.LCFI513:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI514:
	subq	$24, %rsp
.LCFI515:
	andl	$31, %r15d
	je	.L3422
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	shrl	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3424
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3426
	.p2align 4,,7
.L3424:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3426:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3422:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE933:
	.size	op_shrl_user_T0_T1_cc, .-op_shrl_user_T0_T1_cc
	.p2align 4,,15
.globl op_shll_user_T0_T1_cc
	.type	op_shll_user_T0_T1_cc, @function
op_shll_user_T0_T1_cc:
.LFB932:
	movq	%r15, -8(%rsp)
.LCFI516:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI517:
	subq	$24, %rsp
.LCFI518:
	andl	$31, %r15d
	je	.L3429
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3431
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3433
	.p2align 4,,7
.L3431:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3433:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3429:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE932:
	.size	op_shll_user_T0_T1_cc, .-op_shll_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_user_T0_T1_cc
	.type	op_rcrl_user_T0_T1_cc, @function
op_rcrl_user_T0_T1_cc:
.LFB931:
	pushq	%r15
.LCFI519:
	movl	%r12d, %eax
	pushq	%r14
.LCFI520:
	subq	$8, %rsp
.LCFI521:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L3436
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3438
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3438:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3440
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3442
	.p2align 4,,7
.L3440:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3442:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$20, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L3436:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE931:
	.size	op_rcrl_user_T0_T1_cc, .-op_rcrl_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcll_user_T0_T1_cc
	.type	op_rcll_user_T0_T1_cc, @function
op_rcll_user_T0_T1_cc:
.LFB930:
	pushq	%r15
.LCFI522:
	movl	%r12d, %eax
	pushq	%r14
.LCFI523:
	subq	$8, %rsp
.LCFI524:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L3445
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3447
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3447:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3449
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3451
	.p2align 4,,7
.L3449:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3451:
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L3445:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE930:
	.size	op_rcll_user_T0_T1_cc, .-op_rcll_user_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_user_T0_T1
	.type	op_rorl_user_T0_T1, @function
op_rorl_user_T0_T1:
.LFB929:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI525:
	andl	$31, %ecx
	je	.L3454
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r13d, %edi
	shrl	$12, %eax
	rorl	%cl, %esi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3456
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3454
	.p2align 4,,7
.L3456:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3454:
	addq	$8, %rsp
	ret
.LFE929:
	.size	op_rorl_user_T0_T1, .-op_rorl_user_T0_T1
	.p2align 4,,15
.globl op_roll_user_T0_T1
	.type	op_roll_user_T0_T1, @function
op_roll_user_T0_T1:
.LFB928:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI526:
	andl	$31, %ecx
	je	.L3460
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r13d, %edi
	shrl	$12, %eax
	roll	%cl, %esi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3462
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3460
	.p2align 4,,7
.L3462:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3460:
	addq	$8, %rsp
	ret
.LFE928:
	.size	op_roll_user_T0_T1, .-op_roll_user_T0_T1
	.p2align 4,,15
.globl op_rorl_user_T0_T1_cc
	.type	op_rorl_user_T0_T1_cc, @function
op_rorl_user_T0_T1_cc:
.LFB927:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI527:
	movq	%r15, -8(%rsp)
.LCFI528:
	subq	$24, %rsp
.LCFI529:
	andl	$31, %ecx
	je	.L3466
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%ebx, %r15d
	shrl	$12, %eax
	rorl	%cl, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3468
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3470
	.p2align 4,,7
.L3468:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3470:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r15d
	shrl	$31, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3466:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE927:
	.size	op_rorl_user_T0_T1_cc, .-op_rorl_user_T0_T1_cc
	.p2align 4,,15
.globl op_roll_user_T0_T1_cc
	.type	op_roll_user_T0_T1_cc, @function
op_roll_user_T0_T1_cc:
.LFB926:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI530:
	movq	%r15, -8(%rsp)
.LCFI531:
	subq	$24, %rsp
.LCFI532:
	andl	$31, %ecx
	je	.L3473
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%ebx, %r15d
	shrl	$12, %eax
	roll	%cl, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3475
	movl	$1, %edx
	call	__stl_mmu
	jmp	.L3477
	.p2align 4,,7
.L3475:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	call	remR3PhysWriteU32
.L3477:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3473:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE926:
	.size	op_roll_user_T0_T1_cc, .-op_roll_user_T0_T1_cc
	.p2align 4,,15
.globl op_adcl_kernel_T0_T1_cc
	.type	op_adcl_kernel_T0_T1_cc, @function
op_adcl_kernel_T0_T1_cc:
.LFB923:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI533:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3480
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3482
	.p2align 4,,7
.L3480:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3482:
	leal	8(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE923:
	.size	op_adcl_kernel_T0_T1_cc, .-op_adcl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrdl_kernel_T0_T1_ECX_cc
	.type	op_shrdl_kernel_T0_T1_ECX_cc, @function
op_shrdl_kernel_T0_T1_ECX_cc:
.LFB922:
	movq	%r14, -16(%rsp)
.LCFI534:
	movq	%r15, -8(%rsp)
.LCFI535:
	subq	$24, %rsp
.LCFI536:
	movl	4(%rbp), %r14d
	andl	$31, %r14d
	je	.L3485
	movl	%ebx, %esi
	movl	%r14d, %ecx
	movl	%r12d, %eax
	shrl	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r15d
	subl	%r14d, %ecx
	movl	%r13d, %edi
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3487
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3489
	.p2align 4,,7
.L3487:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3489:
	leal	-1(%r14), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r15d
	movl	%r15d, 40(%rbp)
.L3485:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE922:
	.size	op_shrdl_kernel_T0_T1_ECX_cc, .-op_shrdl_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_kernel_T0_T1_im_cc
	.type	op_shrdl_kernel_T0_T1_im_cc, @function
op_shrdl_kernel_T0_T1_im_cc:
.LFB921:
	pushq	%r15
.LCFI537:
	leal	__op_param1(%rip), %r15d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movl	%r13d, %edi
	movl	%r15d, %ecx
	pushq	%r14
.LCFI538:
	movl	%ebx, %r14d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	%r15d, %ecx
	subq	$8, %rsp
.LCFI539:
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3492
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3494
	.p2align 4,,7
.L3492:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3494:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE921:
	.size	op_shrdl_kernel_T0_T1_im_cc, .-op_shrdl_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_kernel_T0_T1_ECX_cc
	.type	op_shldl_kernel_T0_T1_ECX_cc, @function
op_shldl_kernel_T0_T1_ECX_cc:
.LFB920:
	movq	%r14, -16(%rsp)
.LCFI540:
	movq	%r15, -8(%rsp)
.LCFI541:
	subq	$24, %rsp
.LCFI542:
	movl	4(%rbp), %r14d
	andl	$31, %r14d
	je	.L3497
	movl	%ebx, %esi
	movl	%r14d, %ecx
	movl	%r12d, %eax
	sall	%cl, %esi
	movl	$32, %ecx
	movl	%ebx, %r15d
	subl	%r14d, %ecx
	movl	%r13d, %edi
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3499
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3501
	.p2align 4,,7
.L3499:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3501:
	leal	-1(%r14), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r15d
	movl	%r15d, 40(%rbp)
.L3497:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE920:
	.size	op_shldl_kernel_T0_T1_ECX_cc, .-op_shldl_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shldl_kernel_T0_T1_im_cc
	.type	op_shldl_kernel_T0_T1_im_cc, @function
op_shldl_kernel_T0_T1_im_cc:
.LFB919:
	pushq	%r15
.LCFI543:
	leal	__op_param1(%rip), %r15d
	movl	%ebx, %esi
	movl	%r12d, %eax
	movl	%r13d, %edi
	movl	%r15d, %ecx
	pushq	%r14
.LCFI544:
	movl	%ebx, %r14d
	sall	%cl, %esi
	movl	$32, %ecx
	subl	%r15d, %ecx
	subq	$8, %rsp
.LCFI545:
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3504
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3506
	.p2align 4,,7
.L3504:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3506:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE919:
	.size	op_shldl_kernel_T0_T1_im_cc, .-op_shldl_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_sarl_kernel_T0_T1_cc
	.type	op_sarl_kernel_T0_T1_cc, @function
op_sarl_kernel_T0_T1_cc:
.LFB918:
	movq	%r15, -8(%rsp)
.LCFI546:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI547:
	subq	$24, %rsp
.LCFI548:
	andl	$31, %r15d
	je	.L3509
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	sarl	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3511
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3513
	.p2align 4,,7
.L3511:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3513:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3509:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE918:
	.size	op_sarl_kernel_T0_T1_cc, .-op_sarl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_kernel_T0_T1_cc
	.type	op_shrl_kernel_T0_T1_cc, @function
op_shrl_kernel_T0_T1_cc:
.LFB917:
	movq	%r15, -8(%rsp)
.LCFI549:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI550:
	subq	$24, %rsp
.LCFI551:
	andl	$31, %r15d
	je	.L3516
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	shrl	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3518
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3520
	.p2align 4,,7
.L3518:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3520:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$40, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3516:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE917:
	.size	op_shrl_kernel_T0_T1_cc, .-op_shrl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shll_kernel_T0_T1_cc
	.type	op_shll_kernel_T0_T1_cc, @function
op_shll_kernel_T0_T1_cc:
.LFB916:
	movq	%r15, -8(%rsp)
.LCFI552:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI553:
	subq	$24, %rsp
.LCFI554:
	andl	$31, %r15d
	je	.L3523
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r15d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r14d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3525
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3527
	.p2align 4,,7
.L3525:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3527:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$36, 48(%rbp)
	sall	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3523:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE916:
	.size	op_shll_kernel_T0_T1_cc, .-op_shll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_kernel_T0_T1_cc
	.type	op_rcrl_kernel_T0_T1_cc, @function
op_rcrl_kernel_T0_T1_cc:
.LFB915:
	pushq	%r15
.LCFI555:
	movl	%r12d, %eax
	pushq	%r14
.LCFI556:
	subq	$8, %rsp
.LCFI557:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L3530
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	shrl	%cl, %esi
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3532
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3532:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3534
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3536
	.p2align 4,,7
.L3534:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3536:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$20, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L3530:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE915:
	.size	op_rcrl_kernel_T0_T1_cc, .-op_rcrl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcll_kernel_T0_T1_cc
	.type	op_rcll_kernel_T0_T1_cc, @function
op_rcll_kernel_T0_T1_cc:
.LFB914:
	pushq	%r15
.LCFI558:
	movl	%r12d, %eax
	pushq	%r14
.LCFI559:
	subq	$8, %rsp
.LCFI560:
	andl	$31, %eax
	movl	%eax, 4(%rsp)
	je	.L3539
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movl	%ebx, %esi
	movl	%eax, %r15d
	andl	$1, %eax
	movl	%ebx, %r14d
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3541
	movl	$33, %ecx
	subl	4(%rsp), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3541:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3543
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3545
	.p2align 4,,7
.L3543:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3545:
	movl	$32, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r14d, %r15d
	movl	%r15d, 40(%rbp)
.L3539:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE914:
	.size	op_rcll_kernel_T0_T1_cc, .-op_rcll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_kernel_T0_T1
	.type	op_rorl_kernel_T0_T1, @function
op_rorl_kernel_T0_T1:
.LFB913:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI561:
	andl	$31, %ecx
	je	.L3548
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r13d, %edi
	shrl	$12, %eax
	rorl	%cl, %esi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3550
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3548
	.p2align 4,,7
.L3550:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3548:
	addq	$8, %rsp
	ret
.LFE913:
	.size	op_rorl_kernel_T0_T1, .-op_rorl_kernel_T0_T1
	.p2align 4,,15
.globl op_roll_kernel_T0_T1
	.type	op_roll_kernel_T0_T1, @function
op_roll_kernel_T0_T1:
.LFB912:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI562:
	andl	$31, %ecx
	je	.L3554
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r13d, %edi
	shrl	$12, %eax
	roll	%cl, %esi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3556
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3554
	.p2align 4,,7
.L3556:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3554:
	addq	$8, %rsp
	ret
.LFE912:
	.size	op_roll_kernel_T0_T1, .-op_roll_kernel_T0_T1
	.p2align 4,,15
.globl op_rorl_kernel_T0_T1_cc
	.type	op_rorl_kernel_T0_T1_cc, @function
op_rorl_kernel_T0_T1_cc:
.LFB911:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI563:
	movq	%r15, -8(%rsp)
.LCFI564:
	subq	$24, %rsp
.LCFI565:
	andl	$31, %ecx
	je	.L3560
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%ebx, %r15d
	shrl	$12, %eax
	rorl	%cl, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3562
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3564
	.p2align 4,,7
.L3562:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3564:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r15d
	shrl	$31, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3560:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE911:
	.size	op_rorl_kernel_T0_T1_cc, .-op_rorl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_roll_kernel_T0_T1_cc
	.type	op_roll_kernel_T0_T1_cc, @function
op_roll_kernel_T0_T1_cc:
.LFB910:
	movl	%r12d, %ecx
	movq	%r14, -16(%rsp)
.LCFI566:
	movq	%r15, -8(%rsp)
.LCFI567:
	subq	$24, %rsp
.LCFI568:
	andl	$31, %ecx
	je	.L3567
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%ebx, %r15d
	shrl	$12, %eax
	roll	%cl, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	movl	%esi, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3569
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3571
	.p2align 4,,7
.L3569:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3571:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$20, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3567:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE910:
	.size	op_roll_kernel_T0_T1_cc, .-op_roll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_stl_kernel_T1_A0
	.type	op_stl_kernel_T1_A0, @function
op_stl_kernel_T1_A0:
.LFB594:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI569:
	movl	%r12d, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3574
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3576
	.p2align 4,,7
.L3574:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3576:
	addq	$8, %rsp
	ret
.LFE594:
	.size	op_stl_kernel_T1_A0, .-op_stl_kernel_T1_A0
	.p2align 4,,15
.globl op_stl_kernel_T0_A0
	.type	op_stl_kernel_T0_A0, @function
op_stl_kernel_T0_A0:
.LFB592:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI570:
	movl	%ebx, %esi
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3579
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3581
	.p2align 4,,7
.L3579:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
.L3581:
	addq	$8, %rsp
	ret
.LFE592:
	.size	op_stl_kernel_T0_A0, .-op_stl_kernel_T0_A0
	.p2align 4,,15
.globl op_cmpxchgl_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgl_kernel_T0_T1_EAX_cc, @function
op_cmpxchgl_kernel_T0_T1_EAX_cc:
.LFB925:
	movq	%r14, -16(%rsp)
.LCFI571:
	movq	%r15, -8(%rsp)
.LCFI572:
	subq	$24, %rsp
.LCFI573:
	movl	(%rbp), %eax
	movl	%ebx, %r14d
	movq	%rbp, %rcx
	movl	%eax, %r15d
	subl	%ebx, %r15d
	jne	.L3584
	movl	%r13d, %eax
	movl	%r12d, %esi
	movl	%r12d, %ebx
	shrl	$12, %eax
	movl	%r13d, %edi
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4093, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3586
	xorl	%edx, %edx
	call	__stl_mmu
	jmp	.L3588
	.p2align 4,,7
.L3586:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	call	remR3PhysWriteU32
	jmp	.L3588
	.p2align 4,,7
.L3584:
	movl	%ebx, (%rcx)
.L3588:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE925:
	.size	op_cmpxchgl_kernel_T0_T1_EAX_cc, .-op_cmpxchgl_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_adcw_kernel_T0_T1_cc
	.type	op_adcw_kernel_T0_T1_cc, @function
op_adcw_kernel_T0_T1_cc:
.LFB806:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI574:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3591
	movzwl	%cx, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3593
	.p2align 4,,7
.L3591:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3593:
	leal	7(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE806:
	.size	op_adcw_kernel_T0_T1_cc, .-op_adcw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrdw_kernel_T0_T1_ECX_cc
	.type	op_shrdw_kernel_T0_T1_ECX_cc, @function
op_shrdw_kernel_T0_T1_ECX_cc:
.LFB805:
	pushq	%r15
.LCFI575:
	movq	%rbp, %r8
	pushq	%r14
.LCFI576:
	subq	$8, %rsp
.LCFI577:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L3596
	movl	%r12d, %eax
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	sall	$16, %eax
	movl	%r12d, %edx
	orl	%eax, %r14d
	movl	%r14d, %esi
	shrl	%cl, %esi
	cmpl	$16, %r15d
	jle	.L3598
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %esi
.L3598:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%r8), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3600
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3602
	.p2align 4,,7
.L3600:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3602:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3596:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE805:
	.size	op_shrdw_kernel_T0_T1_ECX_cc, .-op_shrdw_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_kernel_T0_T1_im_cc
	.type	op_shrdw_kernel_T0_T1_im_cc, @function
op_shrdw_kernel_T0_T1_im_cc:
.LFB804:
	pushq	%r15
.LCFI578:
	movl	%r12d, %eax
	leal	__op_param1(%rip), %r15d
	sall	$16, %eax
	movl	%r12d, %edx
	pushq	%r14
.LCFI579:
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	orl	%eax, %r14d
	movl	%r14d, %esi
	subq	$8, %rsp
.LCFI580:
	shrl	%cl, %esi
	cmpl	$16, %r15d
	jle	.L3605
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %esi
.L3605:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3607
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3609
	.p2align 4,,7
.L3607:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3609:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE804:
	.size	op_shrdw_kernel_T0_T1_im_cc, .-op_shrdw_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_kernel_T0_T1_ECX_cc
	.type	op_shldw_kernel_T0_T1_ECX_cc, @function
op_shldw_kernel_T0_T1_ECX_cc:
.LFB803:
	pushq	%r15
.LCFI581:
	movq	%rbp, %r8
	pushq	%r14
.LCFI582:
	subq	$8, %rsp
.LCFI583:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L3612
	movl	%ebx, %r14d
	movzwl	%r12w,%eax
	movl	%r15d, %ecx
	sall	$16, %r14d
	movl	%eax, %r12d
	orl	%eax, %r14d
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L3614
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L3614:
	movl	%r13d, %eax
	movl	%edx, %ecx
	movl	%r13d, %edi
	shrl	$12, %eax
	shrl	$16, %ecx
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%r8), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3616
	movzwl	%cx, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3618
	.p2align 4,,7
.L3616:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3618:
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3612:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE803:
	.size	op_shldw_kernel_T0_T1_ECX_cc, .-op_shldw_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_cmpxchgw_user_T0_T1_EAX_cc
	.type	op_cmpxchgw_user_T0_T1_EAX_cc, @function
op_cmpxchgw_user_T0_T1_EAX_cc:
.LFB824:
	movq	%r14, -16(%rsp)
.LCFI584:
	movq	%r15, -8(%rsp)
.LCFI585:
	subq	$24, %rsp
.LCFI586:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testw	%r15w, %r15w
	jne	.L3621
	movl	%r13d, %eax
	movl	%r12d, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3623
	movzwl	%r12w, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3625
	.p2align 4,,7
.L3623:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%r12w, %esi
	call	remR3PhysWriteU16
	jmp	.L3625
	.p2align 4,,7
.L3621:
	xorw	%dx, %dx
	movzwl	%bx,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L3625:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE824:
	.size	op_cmpxchgw_user_T0_T1_EAX_cc, .-op_cmpxchgw_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_stw_user_T0_A0
	.type	op_stw_user_T0_A0, @function
op_stw_user_T0_A0:
.LFB610:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI587:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3628
	movzwl	%bx, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3630
	.p2align 4,,7
.L3628:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%bx, %esi
	call	remR3PhysWriteU16
.L3630:
	addq	$8, %rsp
	ret
.LFE610:
	.size	op_stw_user_T0_A0, .-op_stw_user_T0_A0
	.p2align 4,,15
.globl op_cmpxchgw_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgw_kernel_T0_T1_EAX_cc, @function
op_cmpxchgw_kernel_T0_T1_EAX_cc:
.LFB808:
	movq	%r14, -16(%rsp)
.LCFI588:
	movq	%r15, -8(%rsp)
.LCFI589:
	subq	$24, %rsp
.LCFI590:
	movl	(%rbp), %edx
	movl	%ebx, %r14d
	movl	%edx, %r15d
	subl	%ebx, %r15d
	testw	%r15w, %r15w
	jne	.L3633
	movl	%r13d, %eax
	movl	%r12d, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3635
	movzwl	%r12w, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3637
	.p2align 4,,7
.L3635:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%r12w, %esi
	call	remR3PhysWriteU16
	jmp	.L3637
	.p2align 4,,7
.L3633:
	xorw	%dx, %dx
	movzwl	%bx,%eax
	orl	%eax, %edx
	movl	%edx, (%rbp)
.L3637:
	movl	%r14d, 40(%rbp)
	movl	%r15d, 44(%rbp)
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE808:
	.size	op_cmpxchgw_kernel_T0_T1_EAX_cc, .-op_cmpxchgw_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_sbbw_kernel_T0_T1_cc
	.type	op_sbbw_kernel_T0_T1_cc, @function
op_sbbw_kernel_T0_T1_cc:
.LFB807:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI591:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %ecx
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %ecx
	subl	%eax, %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%ecx, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3640
	movzwl	%cx, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3642
	.p2align 4,,7
.L3640:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3642:
	leal	15(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE807:
	.size	op_sbbw_kernel_T0_T1_cc, .-op_sbbw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_kernel_T0_T1_cc
	.type	op_rorw_kernel_T0_T1_cc, @function
op_rorw_kernel_T0_T1_cc:
.LFB794:
	pushq	%r15
.LCFI592:
	movl	%r12d, %eax
	pushq	%r14
.LCFI593:
	subq	$8, %rsp
.LCFI594:
	testb	$31, %al
	je	.L3645
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r15d
	andl	$15, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3647
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3649
	.p2align 4,,7
.L3647:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3649:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r15d
	shrl	$15, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	andl	$1, %edx
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3645:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE794:
	.size	op_rorw_kernel_T0_T1_cc, .-op_rorw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_kernel_T0_T1_cc
	.type	op_rolw_kernel_T0_T1_cc, @function
op_rolw_kernel_T0_T1_cc:
.LFB793:
	pushq	%r15
.LCFI595:
	movl	%r12d, %eax
	pushq	%r14
.LCFI596:
	subq	$8, %rsp
.LCFI597:
	testb	$31, %al
	je	.L3652
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r15d
	andl	$15, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3654
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3656
	.p2align 4,,7
.L3654:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3656:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3652:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE793:
	.size	op_rolw_kernel_T0_T1_cc, .-op_rolw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_stw_kernel_T1_A0
	.type	op_stw_kernel_T1_A0, @function
op_stw_kernel_T1_A0:
.LFB593:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI598:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3659
	movzwl	%r12w, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3661
	.p2align 4,,7
.L3659:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%r12w, %esi
	call	remR3PhysWriteU16
.L3661:
	addq	$8, %rsp
	ret
.LFE593:
	.size	op_stw_kernel_T1_A0, .-op_stw_kernel_T1_A0
	.p2align 4,,15
.globl op_stw_kernel_T0_A0
	.type	op_stw_kernel_T0_A0, @function
op_stw_kernel_T0_A0:
.LFB591:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI599:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3664
	movzwl	%bx, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3666
	.p2align 4,,7
.L3664:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%bx, %esi
	call	remR3PhysWriteU16
.L3666:
	addq	$8, %rsp
	ret
.LFE591:
	.size	op_stw_kernel_T0_A0, .-op_stw_kernel_T0_A0
	.p2align 4,,15
.globl op_shldw_kernel_T0_T1_im_cc
	.type	op_shldw_kernel_T0_T1_im_cc, @function
op_shldw_kernel_T0_T1_im_cc:
.LFB802:
	pushq	%r15
.LCFI600:
	movzwl	%r12w,%eax
	leal	__op_param1(%rip), %r15d
	movl	%eax, %r12d
	pushq	%r14
.LCFI601:
	movl	%ebx, %r14d
	movl	%r15d, %ecx
	sall	$16, %r14d
	orl	%eax, %r14d
	subq	$8, %rsp
.LCFI602:
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L3669
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L3669:
	movl	%r13d, %eax
	movl	%edx, %ecx
	movl	%r13d, %edi
	shrl	$12, %eax
	shrl	$16, %ecx
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3671
	movzwl	%cx, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3673
	.p2align 4,,7
.L3671:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3673:
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE802:
	.size	op_shldw_kernel_T0_T1_im_cc, .-op_shldw_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_sarw_kernel_T0_T1_cc
	.type	op_sarw_kernel_T0_T1_cc, @function
op_sarw_kernel_T0_T1_cc:
.LFB801:
	movq	%r15, -8(%rsp)
.LCFI603:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI604:
	subq	$24, %rsp
.LCFI605:
	andl	$31, %r15d
	je	.L3676
	movl	%r13d, %eax
	movswl	%bx,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	sarl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3678
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3680
	.p2align 4,,7
.L3678:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3680:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3676:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE801:
	.size	op_sarw_kernel_T0_T1_cc, .-op_sarw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_kernel_T0_T1_cc
	.type	op_shrw_kernel_T0_T1_cc, @function
op_shrw_kernel_T0_T1_cc:
.LFB800:
	movq	%r15, -8(%rsp)
.LCFI606:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI607:
	subq	$24, %rsp
.LCFI608:
	andl	$31, %r15d
	je	.L3683
	movl	%r13d, %eax
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	shrl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3685
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3687
	.p2align 4,,7
.L3685:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3687:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3683:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE800:
	.size	op_shrw_kernel_T0_T1_cc, .-op_shrw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_kernel_T0_T1_cc
	.type	op_shlw_kernel_T0_T1_cc, @function
op_shlw_kernel_T0_T1_cc:
.LFB799:
	movq	%r14, -16(%rsp)
.LCFI609:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI610:
	subq	$24, %rsp
.LCFI611:
	andl	$31, %r14d
	je	.L3690
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r14d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r15d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3692
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3694
	.p2align 4,,7
.L3692:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3694:
	leal	-1(%r14), %ecx
	movzwl	%r15w, %eax
	movl	%ebx, 44(%rbp)
	movl	$35, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L3690:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE799:
	.size	op_shlw_kernel_T0_T1_cc, .-op_shlw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_kernel_T0_T1_cc
	.type	op_rcrw_kernel_T0_T1_cc, @function
op_rcrw_kernel_T0_T1_cc:
.LFB798:
	pushq	%r15
.LCFI612:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI613:
	subq	$8, %rsp
.LCFI614:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L3697
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	shrl	%cl, %esi
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3699
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3699:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3701
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3703
	.p2align 4,,7
.L3701:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3703:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3697:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE798:
	.size	op_rcrw_kernel_T0_T1_cc, .-op_rcrw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rclw_kernel_T0_T1_cc
	.type	op_rclw_kernel_T0_T1_cc, @function
op_rclw_kernel_T0_T1_cc:
.LFB797:
	pushq	%r15
.LCFI615:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI616:
	subq	$8, %rsp
.LCFI617:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L3706
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3708
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3708:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3710
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3712
	.p2align 4,,7
.L3710:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3712:
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3706:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE797:
	.size	op_rclw_kernel_T0_T1_cc, .-op_rclw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_kernel_T0_T1
	.type	op_rorw_kernel_T0_T1, @function
op_rorw_kernel_T0_T1:
.LFB796:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI618:
	andl	$15, %ecx
	je	.L3715
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	shrl	%cl, %esi
	movzbl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 828(%rdx)
	je	.L3717
	movzwl	%si, %esi
	xorl	%edx, %edx
	call	__stw_mmu
	jmp	.L3715
	.p2align 4,,7
.L3717:
	mov	%r13d, %edi
	addq	840(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3715:
	addq	$8, %rsp
	ret
.LFE796:
	.size	op_rorw_kernel_T0_T1, .-op_rorw_kernel_T0_T1
	.p2align 4,,15
.globl op_sbbw_user_T0_T1_cc
	.type	op_sbbw_user_T0_T1_cc, @function
op_sbbw_user_T0_T1_cc:
.LFB823:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI619:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%ebx, %ecx
	movl	%eax, %r14d
	movl	%r13d, %edi
	subl	%r12d, %ecx
	subl	%eax, %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%ecx, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3721
	movzwl	%cx, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3723
	.p2align 4,,7
.L3721:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3723:
	leal	15(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE823:
	.size	op_sbbw_user_T0_T1_cc, .-op_sbbw_user_T0_T1_cc
	.p2align 4,,15
.globl op_adcw_user_T0_T1_cc
	.type	op_adcw_user_T0_T1_cc, @function
op_adcw_user_T0_T1_cc:
.LFB822:
	mov	48(%rbp), %eax
	pushq	%r14
.LCFI620:
	salq	$4, %rax
	call	*cc_table+8(%rax)
	movl	%eax, %r14d
	leal	(%rbx,%r12), %eax
	movl	%r13d, %edi
	leal	(%rax,%r14), %ecx
	movl	%r13d, %eax
	shrl	$12, %eax
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3726
	movzwl	%cx, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3728
	.p2align 4,,7
.L3726:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3728:
	leal	7(,%r14,4), %eax
	movl	%r12d, 40(%rbp)
	movl	%ebx, 44(%rbp)
	popq	%r14
	movl	%eax, 48(%rbp)
	ret
.LFE822:
	.size	op_adcw_user_T0_T1_cc, .-op_adcw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrdw_user_T0_T1_ECX_cc
	.type	op_shrdw_user_T0_T1_ECX_cc, @function
op_shrdw_user_T0_T1_ECX_cc:
.LFB821:
	pushq	%r15
.LCFI621:
	movq	%rbp, %r8
	pushq	%r14
.LCFI622:
	subq	$8, %rsp
.LCFI623:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L3731
	movl	%r12d, %eax
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	sall	$16, %eax
	movl	%r12d, %edx
	orl	%eax, %r14d
	movl	%r14d, %esi
	shrl	%cl, %esi
	cmpl	$16, %r15d
	jle	.L3733
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %esi
.L3733:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%r8), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3735
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3737
	.p2align 4,,7
.L3735:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3737:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3731:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE821:
	.size	op_shrdw_user_T0_T1_ECX_cc, .-op_shrdw_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_user_T0_T1_im_cc
	.type	op_shrdw_user_T0_T1_im_cc, @function
op_shrdw_user_T0_T1_im_cc:
.LFB820:
	pushq	%r15
.LCFI624:
	movl	%r12d, %eax
	leal	__op_param1(%rip), %r15d
	sall	$16, %eax
	movl	%r12d, %edx
	pushq	%r14
.LCFI625:
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	orl	%eax, %r14d
	movl	%r14d, %esi
	subq	$8, %rsp
.LCFI626:
	shrl	%cl, %esi
	cmpl	$16, %r15d
	jle	.L3740
	movl	$32, %ecx
	subl	%r15d, %ecx
	sall	%cl, %edx
	orl	%edx, %esi
.L3740:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3742
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3744
	.p2align 4,,7
.L3742:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3744:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE820:
	.size	op_shrdw_user_T0_T1_im_cc, .-op_shrdw_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_rolw_user_T0_T1
	.type	op_rolw_user_T0_T1, @function
op_rolw_user_T0_T1:
.LFB811:
	movl	%r12d, %ecx
	subq	$8, %rsp
.LCFI627:
	andl	$15, %ecx
	je	.L3747
	movzwl	%bx,%eax
	movl	$16, 4(%rsp)
	subl	%ecx, 4(%rsp)
	movl	%eax, %esi
	movl	%r13d, %edi
	sall	%cl, %esi
	movzbl	4(%rsp), %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3749
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3747
	.p2align 4,,7
.L3749:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3747:
	addq	$8, %rsp
	ret
.LFE811:
	.size	op_rolw_user_T0_T1, .-op_rolw_user_T0_T1
	.p2align 4,,15
.globl op_rorw_user_T0_T1_cc
	.type	op_rorw_user_T0_T1_cc, @function
op_rorw_user_T0_T1_cc:
.LFB810:
	pushq	%r15
.LCFI628:
	movl	%r12d, %eax
	pushq	%r14
.LCFI629:
	subq	$8, %rsp
.LCFI630:
	testb	$31, %al
	je	.L3753
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r15d
	andl	$15, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	shrl	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3755
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3757
	.p2align 4,,7
.L3755:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3757:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r15d
	shrl	$15, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	andl	$1, %edx
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3753:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE810:
	.size	op_rorw_user_T0_T1_cc, .-op_rorw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_user_T0_T1_cc
	.type	op_rolw_user_T0_T1_cc, @function
op_rolw_user_T0_T1_cc:
.LFB809:
	pushq	%r15
.LCFI631:
	movl	%r12d, %eax
	pushq	%r14
.LCFI632:
	subq	$8, %rsp
.LCFI633:
	testb	$31, %al
	je	.L3760
	movl	%r12d, %edx
	movzwl	%bx,%eax
	movl	%ebx, %r15d
	andl	$15, %edx
	movl	%eax, %esi
	movl	%r13d, %edi
	movl	%edx, %ecx
	sall	%cl, %esi
	movl	$16, %ecx
	subl	%edx, %ecx
	shrl	%cl, %eax
	orl	%eax, %esi
	movl	%r13d, %eax
	shrl	$12, %eax
	movl	%esi, %ebx
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3762
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3764
	.p2align 4,,7
.L3762:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3764:
	mov	48(%rbp), %eax
	movq	%rbp, %r14
	salq	$4, %rax
	call	*cc_table(%rax)
	xorl	%ebx, %r15d
	movl	%ebx, %edx
	andl	$-2050, %eax
	sarl	$4, %r15d
	andl	$1, %edx
	movl	$1, 48(%rbp)
	andl	$2048, %r15d
	orl	%r15d, %eax
	orl	%eax, %edx
	movl	%edx, 40(%r14)
.L3760:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE809:
	.size	op_rolw_user_T0_T1_cc, .-op_rolw_user_T0_T1_cc
	.p2align 4,,15
.globl op_stw_user_T1_A0
	.type	op_stw_user_T1_A0, @function
op_stw_user_T1_A0:
.LFB612:
	movl	%r13d, %eax
	subq	$8, %rsp
.LCFI634:
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3767
	movzwl	%r12w, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3769
	.p2align 4,,7
.L3767:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%r12w, %esi
	call	remR3PhysWriteU16
.L3769:
	addq	$8, %rsp
	ret
.LFE612:
	.size	op_stw_user_T1_A0, .-op_stw_user_T1_A0
	.p2align 4,,15
.globl op_shldw_user_T0_T1_ECX_cc
	.type	op_shldw_user_T0_T1_ECX_cc, @function
op_shldw_user_T0_T1_ECX_cc:
.LFB819:
	pushq	%r15
.LCFI635:
	movq	%rbp, %r8
	pushq	%r14
.LCFI636:
	subq	$8, %rsp
.LCFI637:
	movl	4(%rbp), %r15d
	andl	$31, %r15d
	je	.L3772
	movl	%ebx, %r14d
	movzwl	%r12w,%eax
	movl	%r15d, %ecx
	sall	$16, %r14d
	movl	%eax, %r12d
	orl	%eax, %r14d
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L3774
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L3774:
	movl	%r13d, %eax
	movl	%edx, %ecx
	movl	%r13d, %edi
	shrl	$12, %eax
	shrl	$16, %ecx
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%r8), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3776
	movzwl	%cx, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3778
	.p2align 4,,7
.L3776:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3778:
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3772:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE819:
	.size	op_shldw_user_T0_T1_ECX_cc, .-op_shldw_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shldw_user_T0_T1_im_cc
	.type	op_shldw_user_T0_T1_im_cc, @function
op_shldw_user_T0_T1_im_cc:
.LFB818:
	pushq	%r15
.LCFI638:
	movzwl	%r12w,%eax
	leal	__op_param1(%rip), %r15d
	movl	%eax, %r12d
	pushq	%r14
.LCFI639:
	movl	%ebx, %r14d
	movl	%r15d, %ecx
	sall	$16, %r14d
	orl	%eax, %r14d
	subq	$8, %rsp
.LCFI640:
	movl	%r14d, %edx
	sall	%cl, %edx
	cmpl	$16, %r15d
	jle	.L3781
	leal	-16(%r15), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L3781:
	movl	%r13d, %eax
	movl	%edx, %ecx
	movl	%r13d, %edi
	shrl	$12, %eax
	shrl	$16, %ecx
	movzbl	%al, %eax
	movl	%ecx, %ebx
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3783
	movzwl	%cx, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3785
	.p2align 4,,7
.L3783:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%cx, %esi
	call	remR3PhysWriteU16
.L3785:
	movl	$32, %ecx
	movl	%ebx, 44(%rbp)
	subl	%r15d, %ecx
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE818:
	.size	op_shldw_user_T0_T1_im_cc, .-op_shldw_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_sarw_user_T0_T1_cc
	.type	op_sarw_user_T0_T1_cc, @function
op_sarw_user_T0_T1_cc:
.LFB817:
	movq	%r15, -8(%rsp)
.LCFI641:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI642:
	subq	$24, %rsp
.LCFI643:
	andl	$31, %r15d
	je	.L3788
	movl	%r13d, %eax
	movswl	%bx,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	sarl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3790
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3792
	.p2align 4,,7
.L3790:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3792:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	sarl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3788:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE817:
	.size	op_sarw_user_T0_T1_cc, .-op_sarw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_user_T0_T1_cc
	.type	op_shrw_user_T0_T1_cc, @function
op_shrw_user_T0_T1_cc:
.LFB816:
	movq	%r15, -8(%rsp)
.LCFI644:
	movl	%r12d, %r15d
	movq	%r14, -16(%rsp)
.LCFI645:
	subq	$24, %rsp
.LCFI646:
	andl	$31, %r15d
	je	.L3795
	movl	%r13d, %eax
	movzwl	%bx,%r14d
	movl	%r15d, %ecx
	shrl	$12, %eax
	movl	%r14d, %esi
	movl	%r13d, %edi
	movzbl	%al, %eax
	shrl	%cl, %esi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	movl	%esi, %ebx
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3797
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3799
	.p2align 4,,7
.L3797:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3799:
	leal	-1(%r15), %ecx
	movl	%ebx, 44(%rbp)
	movl	$39, 48(%rbp)
	shrl	%cl, %r14d
	movl	%r14d, 40(%rbp)
.L3795:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE816:
	.size	op_shrw_user_T0_T1_cc, .-op_shrw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_user_T0_T1_cc
	.type	op_shlw_user_T0_T1_cc, @function
op_shlw_user_T0_T1_cc:
.LFB815:
	movq	%r14, -16(%rsp)
.LCFI647:
	movl	%r12d, %r14d
	movq	%r15, -8(%rsp)
.LCFI648:
	subq	$24, %rsp
.LCFI649:
	andl	$31, %r14d
	je	.L3802
	movl	%r13d, %eax
	movl	%ebx, %esi
	movl	%r14d, %ecx
	shrl	$12, %eax
	sall	%cl, %esi
	movl	%ebx, %r15d
	movzbl	%al, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3804
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3806
	.p2align 4,,7
.L3804:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3806:
	leal	-1(%r14), %ecx
	movzwl	%r15w, %eax
	movl	%ebx, 44(%rbp)
	movl	$35, 48(%rbp)
	sall	%cl, %eax
	movl	%eax, 40(%rbp)
.L3802:
	movq	8(%rsp), %r14
	movq	16(%rsp), %r15
	addq	$24, %rsp
	ret
.LFE815:
	.size	op_shlw_user_T0_T1_cc, .-op_shlw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_user_T0_T1_cc
	.type	op_rcrw_user_T0_T1_cc, @function
op_rcrw_user_T0_T1_cc:
.LFB814:
	pushq	%r15
.LCFI650:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI651:
	subq	$8, %rsp
.LCFI652:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L3809
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	shrl	%cl, %esi
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3811
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	sall	%cl, %eax
	orl	%eax, %esi
.L3811:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3813
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3815
	.p2align 4,,7
.L3813:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3815:
	movl	4(%rsp), %ecx
	movl	%r14d, %eax
	andl	$-2050, %r15d
	xorl	%ebx, %eax
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3809:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE814:
	.size	op_rcrw_user_T0_T1_cc, .-op_rcrw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rclw_user_T0_T1_cc
	.type	op_rclw_user_T0_T1_cc, @function
op_rclw_user_T0_T1_cc:
.LFB813:
	pushq	%r15
.LCFI653:
	movq	%r12, %rax
	andl	$31, %eax
	pushq	%r14
.LCFI654:
	subq	$8, %rsp
.LCFI655:
	movzbl	rclw_table(%rax), %eax
	testb	%al, %al
	je	.L3818
	movzbl	%al, %eax
	movl	%eax, 4(%rsp)
	mov	48(%rbp), %eax
	salq	$4, %rax
	call	*cc_table(%rax)
	movzbl	4(%rsp), %ecx
	movzwl	%bx,%r14d
	movl	%eax, %r15d
	movl	%r14d, %esi
	andl	$1, %eax
	sall	%cl, %esi
	movl	4(%rsp), %ecx
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %esi
	cmpl	$1, 4(%rsp)
	jle	.L3820
	movl	$17, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	shrl	%cl, %eax
	orl	%eax, %esi
.L3820:
	movl	%r13d, %eax
	movl	%esi, %ebx
	movl	%r13d, %edi
	shrl	$12, %eax
	movzbl	%al, %eax
	leaq	0(,%rax,8), %rdx
	salq	$5, %rax
	subq	%rdx, %rax
	leaq	(%rax,%rbp), %rdx
	movl	%r13d, %eax
	andl	$-4095, %eax
	cmpl	%eax, 6972(%rdx)
	je	.L3822
	movzwl	%si, %esi
	movl	$1, %edx
	call	__stw_mmu
	jmp	.L3824
	.p2align 4,,7
.L3822:
	mov	%r13d, %edi
	addq	6984(%rdx), %rdi
	movzwl	%si, %esi
	call	remR3PhysWriteU16
.L3824:
	movl	$16, %ecx
	subl	4(%rsp), %ecx
	movl	%r14d, %eax
	xorl	%ebx, %eax
	andl	$-2050, %r15d
	movl	$1, 48(%rbp)
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, %r14d
	orl	%eax, %r15d
	andl	$1, %r14d
	orl	%r15d, %r14d
	movl	%r14d, 40(%rbp)
.L3818:
	addq	$8, %rsp
	popq	%r14
	popq	%r15
	ret
.LFE813:
	.size	op_rclw_user_T0_T1_cc, .-op_rclw_user_T0_T1_cc
.globl cc_table
	.data
	.align 32
	.type	cc_table, @object
	.size	cc_table, 672
cc_table:
	.zero	16
	.quad	compute_all_eflags
	.quad	compute_c_eflags
	.quad	compute_all_mulb
	.quad	compute_c_mull
	.quad	compute_all_mulw
	.quad	compute_c_mull
	.quad	compute_all_mull
	.quad	compute_c_mull
	.zero	16
	.quad	compute_all_addb
	.quad	compute_c_addb
	.quad	compute_all_addw
	.quad	compute_c_addw
	.quad	compute_all_addl
	.quad	compute_c_addl
	.zero	16
	.quad	compute_all_adcb
	.quad	compute_c_adcb
	.quad	compute_all_adcw
	.quad	compute_c_adcw
	.quad	compute_all_adcl
	.quad	compute_c_adcl
	.zero	16
	.quad	compute_all_subb
	.quad	compute_c_subb
	.quad	compute_all_subw
	.quad	compute_c_subw
	.quad	compute_all_subl
	.quad	compute_c_subl
	.zero	16
	.quad	compute_all_sbbb
	.quad	compute_c_sbbb
	.quad	compute_all_sbbw
	.quad	compute_c_sbbw
	.quad	compute_all_sbbl
	.quad	compute_c_sbbl
	.zero	16
	.quad	compute_all_logicb
	.quad	compute_c_logicb
	.quad	compute_all_logicw
	.quad	compute_c_logicw
	.quad	compute_all_logicl
	.quad	compute_c_logicl
	.zero	16
	.quad	compute_all_incb
	.quad	compute_c_incl
	.quad	compute_all_incw
	.quad	compute_c_incl
	.quad	compute_all_incl
	.quad	compute_c_incl
	.zero	16
	.quad	compute_all_decb
	.quad	compute_c_incl
	.quad	compute_all_decw
	.quad	compute_c_incl
	.quad	compute_all_decl
	.quad	compute_c_incl
	.zero	16
	.quad	compute_all_shlb
	.quad	compute_c_shlb
	.quad	compute_all_shlw
	.quad	compute_c_shlw
	.quad	compute_all_shll
	.quad	compute_c_shll
	.zero	16
	.quad	compute_all_sarb
	.quad	compute_c_sarl
	.quad	compute_all_sarw
	.quad	compute_c_sarl
	.quad	compute_all_sarl
	.quad	compute_c_sarl
	.zero	16
.globl fcom_ccval
	.section	.rodata
	.align 16
	.type	fcom_ccval, @object
	.size	fcom_ccval, 16
fcom_ccval:
	.long	256
	.long	16384
	.long	0
	.long	17664
.globl fcomi_ccval
	.align 16
	.type	fcomi_ccval, @object
	.size	fcomi_ccval, 16
fcomi_ccval:
	.long	1
	.long	64
	.long	0
	.long	69
.globl comis_eflags
	.align 16
	.type	comis_eflags, @object
	.size	comis_eflags, 16
comis_eflags:
	.long	1
	.long	64
	.long	0
	.long	69
.globl g_aiOpStruct_Misc
	.align 32
	.type	g_aiOpStruct_Misc, @object
	.size	g_aiOpStruct_Misc, 88
g_aiOpStruct_Misc:
	.long	1
	.long	2
	.long	4
	.long	8
	.long	4
	.long	8
	.long	16
	.long	8
	.long	8
	.long	8
	.long	8
	.long	8
	.long	1
	.long	2
	.long	4
	.long	8
	.long	8
	.long	4
	.long	8
	.long	8
	.long	8
	.long	1107296257
.globl g_aiOpStruct_TLB
	.align 16
	.type	g_aiOpStruct_TLB, @object
	.size	g_aiOpStruct_TLB, 24
g_aiOpStruct_TLB:
	.long	24
	.long	0
	.long	4
	.long	8
	.long	16
	.long	1107296280
.globl g_aiOpStruct_SegmentCache
	.align 16
	.type	g_aiOpStruct_SegmentCache, @object
	.size	g_aiOpStruct_SegmentCache, 28
g_aiOpStruct_SegmentCache:
	.long	20
	.long	0
	.long	4
	.long	8
	.long	12
	.long	16
	.long	1107296276
.globl g_aiOpStruct_XMMReg
	.align 32
	.type	g_aiOpStruct_XMMReg, @object
	.size	g_aiOpStruct_XMMReg, 32
g_aiOpStruct_XMMReg:
	.long	16
	.long	1
	.long	2
	.long	4
	.long	8
	.long	4
	.long	8
	.long	1107296272
.globl g_aiOpStruct_MMXReg
	.align 16
	.type	g_aiOpStruct_MMXReg, @object
	.size	g_aiOpStruct_MMXReg, 24
g_aiOpStruct_MMXReg:
	.long	8
	.long	1
	.long	2
	.long	4
	.long	0
	.long	1107296264
.globl g_aiOpStruct_float_status
	.align 16
	.type	g_aiOpStruct_float_status, @object
	.size	g_aiOpStruct_float_status, 16
g_aiOpStruct_float_status:
	.long	2
	.long	0
	.long	1
	.long	1107296258
.globl g_aiOpStruct_float32u
	.align 16
	.type	g_aiOpStruct_float32u, @object
	.size	g_aiOpStruct_float32u, 24
g_aiOpStruct_float32u:
	.long	4
	.long	0
	.long	4
	.long	0
	.long	4
	.long	1107296260
.globl g_aiOpStruct_float64u
	.align 16
	.type	g_aiOpStruct_float64u, @object
	.size	g_aiOpStruct_float64u, 24
g_aiOpStruct_float64u:
	.long	8
	.long	0
	.long	8
	.long	0
	.long	8
	.long	1107296264
.globl g_aiOpStruct_floatx80u
	.align 32
	.type	g_aiOpStruct_floatx80u, @object
	.size	g_aiOpStruct_floatx80u, 40
g_aiOpStruct_floatx80u:
	.long	16
	.long	0
	.long	16
	.long	0
	.long	16
	.long	0
	.long	8
	.long	8
	.long	2
	.long	1107296272
.globl g_aiOpStruct_CPUState
	.align 32
	.type	g_aiOpStruct_CPUState, @object
	.size	g_aiOpStruct_CPUState, 368
g_aiOpStruct_CPUState:
	.long	46096
	.long	0
	.long	0
	.long	12
	.long	4
	.long	8
	.long	24
	.long	28
	.long	20
	.long	16
	.long	32
	.long	36
	.long	40
	.long	44
	.long	48
	.long	52
	.long	56
	.long	60
	.long	100
	.long	80
	.long	120
	.long	60
	.long	140
	.long	160
	.long	180
	.long	200
	.long	220
	.long	240
	.long	260
	.long	20
	.long	264
	.long	280
	.long	284
	.long	288
	.long	292
	.long	296
	.long	8
	.long	297
	.long	304
	.long	128
	.long	320
	.long	432
	.long	448
	.long	16
	.long	464
	.long	8
	.long	472
	.long	476
	.long	480
	.long	128
	.long	496
	.long	624
	.long	632
	.long	636
	.long	640
	.long	648
	.long	656
	.long	664
	.long	672
	.long	64
	.long	736
	.long	740
	.long	744
	.long	748
	.long	752
	.long	32
	.long	756
	.long	784
	.long	788
	.long	792
	.long	46040
	.long	46048
	.long	46056
	.long	46064
	.long	46068
	.long	46072
	.long	46076
	.long	800
	.long	808
	.long	816
	.long	824
	.long	12288
	.long	13112
	.long	32768
	.long	45880
	.long	128
	.long	46008
	.long	46012
	.long	46016
	.long	46024
	.long	46032
	.long	1107342352
	.align 8
	.type	__op_label1.op_goto_tb1, @object
	.size	__op_label1.op_goto_tb1, 8
__op_label1.op_goto_tb1:
	.quad	.L982
	.align 8
	.type	dummy1.19115, @object
	.size	dummy1.19115, 8
dummy1.19115:
	.quad	.L983
	.align 8
	.type	__op_label0.op_goto_tb0, @object
	.size	__op_label0.op_goto_tb0, 8
__op_label0.op_goto_tb0:
	.quad	.L978
	.align 8
	.type	dummy0.19103, @object
	.size	dummy0.19103, 8
dummy0.19103:
	.quad	.L979
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0x0
	.byte	0x1
	.string	"zR"
	.uleb128 0x1
	.sleb128 -8
	.byte	0x10
	.uleb128 0x1
	.byte	0x3
	.byte	0xc
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1
.LASFDE1:
	.long	.LASFDE1-.Lframe1
	.long	.LFB347
	.long	.LFE347-.LFB347
	.uleb128 0x0
	.align 8
.LEFDE1:
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB348
	.long	.LFE348-.LFB348
	.uleb128 0x0
	.align 8
.LEFDE3:
.LSFDE5:
	.long	.LEFDE5-.LASFDE5
.LASFDE5:
	.long	.LASFDE5-.Lframe1
	.long	.LFB349
	.long	.LFE349-.LFB349
	.uleb128 0x0
	.align 8
.LEFDE5:
.LSFDE7:
	.long	.LEFDE7-.LASFDE7
.LASFDE7:
	.long	.LASFDE7-.Lframe1
	.long	.LFB350
	.long	.LFE350-.LFB350
	.uleb128 0x0
	.align 8
.LEFDE7:
.LSFDE9:
	.long	.LEFDE9-.LASFDE9
.LASFDE9:
	.long	.LASFDE9-.Lframe1
	.long	.LFB351
	.long	.LFE351-.LFB351
	.uleb128 0x0
	.align 8
.LEFDE9:
.LSFDE11:
	.long	.LEFDE11-.LASFDE11
.LASFDE11:
	.long	.LASFDE11-.Lframe1
	.long	.LFB352
	.long	.LFE352-.LFB352
	.uleb128 0x0
	.align 8
.LEFDE11:
.LSFDE13:
	.long	.LEFDE13-.LASFDE13
.LASFDE13:
	.long	.LASFDE13-.Lframe1
	.long	.LFB353
	.long	.LFE353-.LFB353
	.uleb128 0x0
	.align 8
.LEFDE13:
.LSFDE15:
	.long	.LEFDE15-.LASFDE15
.LASFDE15:
	.long	.LASFDE15-.Lframe1
	.long	.LFB354
	.long	.LFE354-.LFB354
	.uleb128 0x0
	.align 8
.LEFDE15:
.LSFDE17:
	.long	.LEFDE17-.LASFDE17
.LASFDE17:
	.long	.LASFDE17-.Lframe1
	.long	.LFB355
	.long	.LFE355-.LFB355
	.uleb128 0x0
	.align 8
.LEFDE17:
.LSFDE19:
	.long	.LEFDE19-.LASFDE19
.LASFDE19:
	.long	.LASFDE19-.Lframe1
	.long	.LFB356
	.long	.LFE356-.LFB356
	.uleb128 0x0
	.align 8
.LEFDE19:
.LSFDE21:
	.long	.LEFDE21-.LASFDE21
.LASFDE21:
	.long	.LASFDE21-.Lframe1
	.long	.LFB357
	.long	.LFE357-.LFB357
	.uleb128 0x0
	.align 8
.LEFDE21:
.LSFDE23:
	.long	.LEFDE23-.LASFDE23
.LASFDE23:
	.long	.LASFDE23-.Lframe1
	.long	.LFB358
	.long	.LFE358-.LFB358
	.uleb128 0x0
	.align 8
.LEFDE23:
.LSFDE25:
	.long	.LEFDE25-.LASFDE25
.LASFDE25:
	.long	.LASFDE25-.Lframe1
	.long	.LFB359
	.long	.LFE359-.LFB359
	.uleb128 0x0
	.align 8
.LEFDE25:
.LSFDE27:
	.long	.LEFDE27-.LASFDE27
.LASFDE27:
	.long	.LASFDE27-.Lframe1
	.long	.LFB360
	.long	.LFE360-.LFB360
	.uleb128 0x0
	.align 8
.LEFDE27:
.LSFDE29:
	.long	.LEFDE29-.LASFDE29
.LASFDE29:
	.long	.LASFDE29-.Lframe1
	.long	.LFB361
	.long	.LFE361-.LFB361
	.uleb128 0x0
	.align 8
.LEFDE29:
.LSFDE31:
	.long	.LEFDE31-.LASFDE31
.LASFDE31:
	.long	.LASFDE31-.Lframe1
	.long	.LFB362
	.long	.LFE362-.LFB362
	.uleb128 0x0
	.align 8
.LEFDE31:
.LSFDE33:
	.long	.LEFDE33-.LASFDE33
.LASFDE33:
	.long	.LASFDE33-.Lframe1
	.long	.LFB363
	.long	.LFE363-.LFB363
	.uleb128 0x0
	.align 8
.LEFDE33:
.LSFDE35:
	.long	.LEFDE35-.LASFDE35
.LASFDE35:
	.long	.LASFDE35-.Lframe1
	.long	.LFB364
	.long	.LFE364-.LFB364
	.uleb128 0x0
	.align 8
.LEFDE35:
.LSFDE37:
	.long	.LEFDE37-.LASFDE37
.LASFDE37:
	.long	.LASFDE37-.Lframe1
	.long	.LFB365
	.long	.LFE365-.LFB365
	.uleb128 0x0
	.align 8
.LEFDE37:
.LSFDE39:
	.long	.LEFDE39-.LASFDE39
.LASFDE39:
	.long	.LASFDE39-.Lframe1
	.long	.LFB366
	.long	.LFE366-.LFB366
	.uleb128 0x0
	.align 8
.LEFDE39:
.LSFDE41:
	.long	.LEFDE41-.LASFDE41
.LASFDE41:
	.long	.LASFDE41-.Lframe1
	.long	.LFB367
	.long	.LFE367-.LFB367
	.uleb128 0x0
	.align 8
.LEFDE41:
.LSFDE43:
	.long	.LEFDE43-.LASFDE43
.LASFDE43:
	.long	.LASFDE43-.Lframe1
	.long	.LFB368
	.long	.LFE368-.LFB368
	.uleb128 0x0
	.align 8
.LEFDE43:
.LSFDE45:
	.long	.LEFDE45-.LASFDE45
.LASFDE45:
	.long	.LASFDE45-.Lframe1
	.long	.LFB369
	.long	.LFE369-.LFB369
	.uleb128 0x0
	.align 8
.LEFDE45:
.LSFDE47:
	.long	.LEFDE47-.LASFDE47
.LASFDE47:
	.long	.LASFDE47-.Lframe1
	.long	.LFB370
	.long	.LFE370-.LFB370
	.uleb128 0x0
	.align 8
.LEFDE47:
.LSFDE49:
	.long	.LEFDE49-.LASFDE49
.LASFDE49:
	.long	.LASFDE49-.Lframe1
	.long	.LFB371
	.long	.LFE371-.LFB371
	.uleb128 0x0
	.align 8
.LEFDE49:
.LSFDE51:
	.long	.LEFDE51-.LASFDE51
.LASFDE51:
	.long	.LASFDE51-.Lframe1
	.long	.LFB372
	.long	.LFE372-.LFB372
	.uleb128 0x0
	.align 8
.LEFDE51:
.LSFDE53:
	.long	.LEFDE53-.LASFDE53
.LASFDE53:
	.long	.LASFDE53-.Lframe1
	.long	.LFB373
	.long	.LFE373-.LFB373
	.uleb128 0x0
	.align 8
.LEFDE53:
.LSFDE55:
	.long	.LEFDE55-.LASFDE55
.LASFDE55:
	.long	.LASFDE55-.Lframe1
	.long	.LFB374
	.long	.LFE374-.LFB374
	.uleb128 0x0
	.align 8
.LEFDE55:
.LSFDE57:
	.long	.LEFDE57-.LASFDE57
.LASFDE57:
	.long	.LASFDE57-.Lframe1
	.long	.LFB375
	.long	.LFE375-.LFB375
	.uleb128 0x0
	.align 8
.LEFDE57:
.LSFDE59:
	.long	.LEFDE59-.LASFDE59
.LASFDE59:
	.long	.LASFDE59-.Lframe1
	.long	.LFB376
	.long	.LFE376-.LFB376
	.uleb128 0x0
	.align 8
.LEFDE59:
.LSFDE61:
	.long	.LEFDE61-.LASFDE61
.LASFDE61:
	.long	.LASFDE61-.Lframe1
	.long	.LFB377
	.long	.LFE377-.LFB377
	.uleb128 0x0
	.align 8
.LEFDE61:
.LSFDE63:
	.long	.LEFDE63-.LASFDE63
.LASFDE63:
	.long	.LASFDE63-.Lframe1
	.long	.LFB378
	.long	.LFE378-.LFB378
	.uleb128 0x0
	.align 8
.LEFDE63:
.LSFDE65:
	.long	.LEFDE65-.LASFDE65
.LASFDE65:
	.long	.LASFDE65-.Lframe1
	.long	.LFB379
	.long	.LFE379-.LFB379
	.uleb128 0x0
	.align 8
.LEFDE65:
.LSFDE67:
	.long	.LEFDE67-.LASFDE67
.LASFDE67:
	.long	.LASFDE67-.Lframe1
	.long	.LFB380
	.long	.LFE380-.LFB380
	.uleb128 0x0
	.align 8
.LEFDE67:
.LSFDE69:
	.long	.LEFDE69-.LASFDE69
.LASFDE69:
	.long	.LASFDE69-.Lframe1
	.long	.LFB381
	.long	.LFE381-.LFB381
	.uleb128 0x0
	.align 8
.LEFDE69:
.LSFDE71:
	.long	.LEFDE71-.LASFDE71
.LASFDE71:
	.long	.LASFDE71-.Lframe1
	.long	.LFB382
	.long	.LFE382-.LFB382
	.uleb128 0x0
	.align 8
.LEFDE71:
.LSFDE73:
	.long	.LEFDE73-.LASFDE73
.LASFDE73:
	.long	.LASFDE73-.Lframe1
	.long	.LFB383
	.long	.LFE383-.LFB383
	.uleb128 0x0
	.align 8
.LEFDE73:
.LSFDE75:
	.long	.LEFDE75-.LASFDE75
.LASFDE75:
	.long	.LASFDE75-.Lframe1
	.long	.LFB384
	.long	.LFE384-.LFB384
	.uleb128 0x0
	.align 8
.LEFDE75:
.LSFDE77:
	.long	.LEFDE77-.LASFDE77
.LASFDE77:
	.long	.LASFDE77-.Lframe1
	.long	.LFB385
	.long	.LFE385-.LFB385
	.uleb128 0x0
	.align 8
.LEFDE77:
.LSFDE79:
	.long	.LEFDE79-.LASFDE79
.LASFDE79:
	.long	.LASFDE79-.Lframe1
	.long	.LFB386
	.long	.LFE386-.LFB386
	.uleb128 0x0
	.align 8
.LEFDE79:
.LSFDE81:
	.long	.LEFDE81-.LASFDE81
.LASFDE81:
	.long	.LASFDE81-.Lframe1
	.long	.LFB387
	.long	.LFE387-.LFB387
	.uleb128 0x0
	.align 8
.LEFDE81:
.LSFDE83:
	.long	.LEFDE83-.LASFDE83
.LASFDE83:
	.long	.LASFDE83-.Lframe1
	.long	.LFB388
	.long	.LFE388-.LFB388
	.uleb128 0x0
	.align 8
.LEFDE83:
.LSFDE85:
	.long	.LEFDE85-.LASFDE85
.LASFDE85:
	.long	.LASFDE85-.Lframe1
	.long	.LFB389
	.long	.LFE389-.LFB389
	.uleb128 0x0
	.align 8
.LEFDE85:
.LSFDE87:
	.long	.LEFDE87-.LASFDE87
.LASFDE87:
	.long	.LASFDE87-.Lframe1
	.long	.LFB390
	.long	.LFE390-.LFB390
	.uleb128 0x0
	.align 8
.LEFDE87:
.LSFDE89:
	.long	.LEFDE89-.LASFDE89
.LASFDE89:
	.long	.LASFDE89-.Lframe1
	.long	.LFB391
	.long	.LFE391-.LFB391
	.uleb128 0x0
	.align 8
.LEFDE89:
.LSFDE91:
	.long	.LEFDE91-.LASFDE91
.LASFDE91:
	.long	.LASFDE91-.Lframe1
	.long	.LFB392
	.long	.LFE392-.LFB392
	.uleb128 0x0
	.align 8
.LEFDE91:
.LSFDE93:
	.long	.LEFDE93-.LASFDE93
.LASFDE93:
	.long	.LASFDE93-.Lframe1
	.long	.LFB393
	.long	.LFE393-.LFB393
	.uleb128 0x0
	.align 8
.LEFDE93:
.LSFDE95:
	.long	.LEFDE95-.LASFDE95
.LASFDE95:
	.long	.LASFDE95-.Lframe1
	.long	.LFB394
	.long	.LFE394-.LFB394
	.uleb128 0x0
	.align 8
.LEFDE95:
.LSFDE97:
	.long	.LEFDE97-.LASFDE97
.LASFDE97:
	.long	.LASFDE97-.Lframe1
	.long	.LFB395
	.long	.LFE395-.LFB395
	.uleb128 0x0
	.align 8
.LEFDE97:
.LSFDE99:
	.long	.LEFDE99-.LASFDE99
.LASFDE99:
	.long	.LASFDE99-.Lframe1
	.long	.LFB396
	.long	.LFE396-.LFB396
	.uleb128 0x0
	.align 8
.LEFDE99:
.LSFDE101:
	.long	.LEFDE101-.LASFDE101
.LASFDE101:
	.long	.LASFDE101-.Lframe1
	.long	.LFB397
	.long	.LFE397-.LFB397
	.uleb128 0x0
	.align 8
.LEFDE101:
.LSFDE103:
	.long	.LEFDE103-.LASFDE103
.LASFDE103:
	.long	.LASFDE103-.Lframe1
	.long	.LFB398
	.long	.LFE398-.LFB398
	.uleb128 0x0
	.align 8
.LEFDE103:
.LSFDE105:
	.long	.LEFDE105-.LASFDE105
.LASFDE105:
	.long	.LASFDE105-.Lframe1
	.long	.LFB399
	.long	.LFE399-.LFB399
	.uleb128 0x0
	.align 8
.LEFDE105:
.LSFDE107:
	.long	.LEFDE107-.LASFDE107
.LASFDE107:
	.long	.LASFDE107-.Lframe1
	.long	.LFB400
	.long	.LFE400-.LFB400
	.uleb128 0x0
	.align 8
.LEFDE107:
.LSFDE109:
	.long	.LEFDE109-.LASFDE109
.LASFDE109:
	.long	.LASFDE109-.Lframe1
	.long	.LFB401
	.long	.LFE401-.LFB401
	.uleb128 0x0
	.align 8
.LEFDE109:
.LSFDE111:
	.long	.LEFDE111-.LASFDE111
.LASFDE111:
	.long	.LASFDE111-.Lframe1
	.long	.LFB402
	.long	.LFE402-.LFB402
	.uleb128 0x0
	.align 8
.LEFDE111:
.LSFDE113:
	.long	.LEFDE113-.LASFDE113
.LASFDE113:
	.long	.LASFDE113-.Lframe1
	.long	.LFB403
	.long	.LFE403-.LFB403
	.uleb128 0x0
	.align 8
.LEFDE113:
.LSFDE115:
	.long	.LEFDE115-.LASFDE115
.LASFDE115:
	.long	.LASFDE115-.Lframe1
	.long	.LFB404
	.long	.LFE404-.LFB404
	.uleb128 0x0
	.align 8
.LEFDE115:
.LSFDE117:
	.long	.LEFDE117-.LASFDE117
.LASFDE117:
	.long	.LASFDE117-.Lframe1
	.long	.LFB405
	.long	.LFE405-.LFB405
	.uleb128 0x0
	.align 8
.LEFDE117:
.LSFDE119:
	.long	.LEFDE119-.LASFDE119
.LASFDE119:
	.long	.LASFDE119-.Lframe1
	.long	.LFB406
	.long	.LFE406-.LFB406
	.uleb128 0x0
	.align 8
.LEFDE119:
.LSFDE121:
	.long	.LEFDE121-.LASFDE121
.LASFDE121:
	.long	.LASFDE121-.Lframe1
	.long	.LFB407
	.long	.LFE407-.LFB407
	.uleb128 0x0
	.align 8
.LEFDE121:
.LSFDE123:
	.long	.LEFDE123-.LASFDE123
.LASFDE123:
	.long	.LASFDE123-.Lframe1
	.long	.LFB408
	.long	.LFE408-.LFB408
	.uleb128 0x0
	.align 8
.LEFDE123:
.LSFDE125:
	.long	.LEFDE125-.LASFDE125
.LASFDE125:
	.long	.LASFDE125-.Lframe1
	.long	.LFB409
	.long	.LFE409-.LFB409
	.uleb128 0x0
	.align 8
.LEFDE125:
.LSFDE127:
	.long	.LEFDE127-.LASFDE127
.LASFDE127:
	.long	.LASFDE127-.Lframe1
	.long	.LFB410
	.long	.LFE410-.LFB410
	.uleb128 0x0
	.align 8
.LEFDE127:
.LSFDE129:
	.long	.LEFDE129-.LASFDE129
.LASFDE129:
	.long	.LASFDE129-.Lframe1
	.long	.LFB411
	.long	.LFE411-.LFB411
	.uleb128 0x0
	.align 8
.LEFDE129:
.LSFDE131:
	.long	.LEFDE131-.LASFDE131
.LASFDE131:
	.long	.LASFDE131-.Lframe1
	.long	.LFB412
	.long	.LFE412-.LFB412
	.uleb128 0x0
	.align 8
.LEFDE131:
.LSFDE133:
	.long	.LEFDE133-.LASFDE133
.LASFDE133:
	.long	.LASFDE133-.Lframe1
	.long	.LFB413
	.long	.LFE413-.LFB413
	.uleb128 0x0
	.align 8
.LEFDE133:
.LSFDE135:
	.long	.LEFDE135-.LASFDE135
.LASFDE135:
	.long	.LASFDE135-.Lframe1
	.long	.LFB414
	.long	.LFE414-.LFB414
	.uleb128 0x0
	.align 8
.LEFDE135:
.LSFDE137:
	.long	.LEFDE137-.LASFDE137
.LASFDE137:
	.long	.LASFDE137-.Lframe1
	.long	.LFB415
	.long	.LFE415-.LFB415
	.uleb128 0x0
	.align 8
.LEFDE137:
.LSFDE139:
	.long	.LEFDE139-.LASFDE139
.LASFDE139:
	.long	.LASFDE139-.Lframe1
	.long	.LFB416
	.long	.LFE416-.LFB416
	.uleb128 0x0
	.align 8
.LEFDE139:
.LSFDE141:
	.long	.LEFDE141-.LASFDE141
.LASFDE141:
	.long	.LASFDE141-.Lframe1
	.long	.LFB417
	.long	.LFE417-.LFB417
	.uleb128 0x0
	.align 8
.LEFDE141:
.LSFDE143:
	.long	.LEFDE143-.LASFDE143
.LASFDE143:
	.long	.LASFDE143-.Lframe1
	.long	.LFB418
	.long	.LFE418-.LFB418
	.uleb128 0x0
	.align 8
.LEFDE143:
.LSFDE145:
	.long	.LEFDE145-.LASFDE145
.LASFDE145:
	.long	.LASFDE145-.Lframe1
	.long	.LFB419
	.long	.LFE419-.LFB419
	.uleb128 0x0
	.align 8
.LEFDE145:
.LSFDE147:
	.long	.LEFDE147-.LASFDE147
.LASFDE147:
	.long	.LASFDE147-.Lframe1
	.long	.LFB420
	.long	.LFE420-.LFB420
	.uleb128 0x0
	.align 8
.LEFDE147:
.LSFDE149:
	.long	.LEFDE149-.LASFDE149
.LASFDE149:
	.long	.LASFDE149-.Lframe1
	.long	.LFB421
	.long	.LFE421-.LFB421
	.uleb128 0x0
	.align 8
.LEFDE149:
.LSFDE151:
	.long	.LEFDE151-.LASFDE151
.LASFDE151:
	.long	.LASFDE151-.Lframe1
	.long	.LFB422
	.long	.LFE422-.LFB422
	.uleb128 0x0
	.align 8
.LEFDE151:
.LSFDE153:
	.long	.LEFDE153-.LASFDE153
.LASFDE153:
	.long	.LASFDE153-.Lframe1
	.long	.LFB423
	.long	.LFE423-.LFB423
	.uleb128 0x0
	.align 8
.LEFDE153:
.LSFDE155:
	.long	.LEFDE155-.LASFDE155
.LASFDE155:
	.long	.LASFDE155-.Lframe1
	.long	.LFB424
	.long	.LFE424-.LFB424
	.uleb128 0x0
	.align 8
.LEFDE155:
.LSFDE157:
	.long	.LEFDE157-.LASFDE157
.LASFDE157:
	.long	.LASFDE157-.Lframe1
	.long	.LFB425
	.long	.LFE425-.LFB425
	.uleb128 0x0
	.align 8
.LEFDE157:
.LSFDE159:
	.long	.LEFDE159-.LASFDE159
.LASFDE159:
	.long	.LASFDE159-.Lframe1
	.long	.LFB426
	.long	.LFE426-.LFB426
	.uleb128 0x0
	.align 8
.LEFDE159:
.LSFDE161:
	.long	.LEFDE161-.LASFDE161
.LASFDE161:
	.long	.LASFDE161-.Lframe1
	.long	.LFB427
	.long	.LFE427-.LFB427
	.uleb128 0x0
	.align 8
.LEFDE161:
.LSFDE163:
	.long	.LEFDE163-.LASFDE163
.LASFDE163:
	.long	.LASFDE163-.Lframe1
	.long	.LFB428
	.long	.LFE428-.LFB428
	.uleb128 0x0
	.align 8
.LEFDE163:
.LSFDE165:
	.long	.LEFDE165-.LASFDE165
.LASFDE165:
	.long	.LASFDE165-.Lframe1
	.long	.LFB429
	.long	.LFE429-.LFB429
	.uleb128 0x0
	.align 8
.LEFDE165:
.LSFDE167:
	.long	.LEFDE167-.LASFDE167
.LASFDE167:
	.long	.LASFDE167-.Lframe1
	.long	.LFB430
	.long	.LFE430-.LFB430
	.uleb128 0x0
	.align 8
.LEFDE167:
.LSFDE169:
	.long	.LEFDE169-.LASFDE169
.LASFDE169:
	.long	.LASFDE169-.Lframe1
	.long	.LFB431
	.long	.LFE431-.LFB431
	.uleb128 0x0
	.align 8
.LEFDE169:
.LSFDE171:
	.long	.LEFDE171-.LASFDE171
.LASFDE171:
	.long	.LASFDE171-.Lframe1
	.long	.LFB432
	.long	.LFE432-.LFB432
	.uleb128 0x0
	.align 8
.LEFDE171:
.LSFDE173:
	.long	.LEFDE173-.LASFDE173
.LASFDE173:
	.long	.LASFDE173-.Lframe1
	.long	.LFB433
	.long	.LFE433-.LFB433
	.uleb128 0x0
	.align 8
.LEFDE173:
.LSFDE175:
	.long	.LEFDE175-.LASFDE175
.LASFDE175:
	.long	.LASFDE175-.Lframe1
	.long	.LFB434
	.long	.LFE434-.LFB434
	.uleb128 0x0
	.align 8
.LEFDE175:
.LSFDE177:
	.long	.LEFDE177-.LASFDE177
.LASFDE177:
	.long	.LASFDE177-.Lframe1
	.long	.LFB435
	.long	.LFE435-.LFB435
	.uleb128 0x0
	.align 8
.LEFDE177:
.LSFDE179:
	.long	.LEFDE179-.LASFDE179
.LASFDE179:
	.long	.LASFDE179-.Lframe1
	.long	.LFB436
	.long	.LFE436-.LFB436
	.uleb128 0x0
	.align 8
.LEFDE179:
.LSFDE181:
	.long	.LEFDE181-.LASFDE181
.LASFDE181:
	.long	.LASFDE181-.Lframe1
	.long	.LFB437
	.long	.LFE437-.LFB437
	.uleb128 0x0
	.align 8
.LEFDE181:
.LSFDE183:
	.long	.LEFDE183-.LASFDE183
.LASFDE183:
	.long	.LASFDE183-.Lframe1
	.long	.LFB438
	.long	.LFE438-.LFB438
	.uleb128 0x0
	.align 8
.LEFDE183:
.LSFDE185:
	.long	.LEFDE185-.LASFDE185
.LASFDE185:
	.long	.LASFDE185-.Lframe1
	.long	.LFB439
	.long	.LFE439-.LFB439
	.uleb128 0x0
	.align 8
.LEFDE185:
.LSFDE187:
	.long	.LEFDE187-.LASFDE187
.LASFDE187:
	.long	.LASFDE187-.Lframe1
	.long	.LFB440
	.long	.LFE440-.LFB440
	.uleb128 0x0
	.align 8
.LEFDE187:
.LSFDE189:
	.long	.LEFDE189-.LASFDE189
.LASFDE189:
	.long	.LASFDE189-.Lframe1
	.long	.LFB441
	.long	.LFE441-.LFB441
	.uleb128 0x0
	.align 8
.LEFDE189:
.LSFDE191:
	.long	.LEFDE191-.LASFDE191
.LASFDE191:
	.long	.LASFDE191-.Lframe1
	.long	.LFB442
	.long	.LFE442-.LFB442
	.uleb128 0x0
	.align 8
.LEFDE191:
.LSFDE193:
	.long	.LEFDE193-.LASFDE193
.LASFDE193:
	.long	.LASFDE193-.Lframe1
	.long	.LFB443
	.long	.LFE443-.LFB443
	.uleb128 0x0
	.align 8
.LEFDE193:
.LSFDE195:
	.long	.LEFDE195-.LASFDE195
.LASFDE195:
	.long	.LASFDE195-.Lframe1
	.long	.LFB444
	.long	.LFE444-.LFB444
	.uleb128 0x0
	.align 8
.LEFDE195:
.LSFDE197:
	.long	.LEFDE197-.LASFDE197
.LASFDE197:
	.long	.LASFDE197-.Lframe1
	.long	.LFB445
	.long	.LFE445-.LFB445
	.uleb128 0x0
	.align 8
.LEFDE197:
.LSFDE199:
	.long	.LEFDE199-.LASFDE199
.LASFDE199:
	.long	.LASFDE199-.Lframe1
	.long	.LFB446
	.long	.LFE446-.LFB446
	.uleb128 0x0
	.align 8
.LEFDE199:
.LSFDE201:
	.long	.LEFDE201-.LASFDE201
.LASFDE201:
	.long	.LASFDE201-.Lframe1
	.long	.LFB447
	.long	.LFE447-.LFB447
	.uleb128 0x0
	.align 8
.LEFDE201:
.LSFDE203:
	.long	.LEFDE203-.LASFDE203
.LASFDE203:
	.long	.LASFDE203-.Lframe1
	.long	.LFB448
	.long	.LFE448-.LFB448
	.uleb128 0x0
	.align 8
.LEFDE203:
.LSFDE205:
	.long	.LEFDE205-.LASFDE205
.LASFDE205:
	.long	.LASFDE205-.Lframe1
	.long	.LFB449
	.long	.LFE449-.LFB449
	.uleb128 0x0
	.align 8
.LEFDE205:
.LSFDE207:
	.long	.LEFDE207-.LASFDE207
.LASFDE207:
	.long	.LASFDE207-.Lframe1
	.long	.LFB450
	.long	.LFE450-.LFB450
	.uleb128 0x0
	.align 8
.LEFDE207:
.LSFDE209:
	.long	.LEFDE209-.LASFDE209
.LASFDE209:
	.long	.LASFDE209-.Lframe1
	.long	.LFB451
	.long	.LFE451-.LFB451
	.uleb128 0x0
	.align 8
.LEFDE209:
.LSFDE211:
	.long	.LEFDE211-.LASFDE211
.LASFDE211:
	.long	.LASFDE211-.Lframe1
	.long	.LFB452
	.long	.LFE452-.LFB452
	.uleb128 0x0
	.align 8
.LEFDE211:
.LSFDE213:
	.long	.LEFDE213-.LASFDE213
.LASFDE213:
	.long	.LASFDE213-.Lframe1
	.long	.LFB453
	.long	.LFE453-.LFB453
	.uleb128 0x0
	.align 8
.LEFDE213:
.LSFDE215:
	.long	.LEFDE215-.LASFDE215
.LASFDE215:
	.long	.LASFDE215-.Lframe1
	.long	.LFB454
	.long	.LFE454-.LFB454
	.uleb128 0x0
	.align 8
.LEFDE215:
.LSFDE217:
	.long	.LEFDE217-.LASFDE217
.LASFDE217:
	.long	.LASFDE217-.Lframe1
	.long	.LFB455
	.long	.LFE455-.LFB455
	.uleb128 0x0
	.align 8
.LEFDE217:
.LSFDE219:
	.long	.LEFDE219-.LASFDE219
.LASFDE219:
	.long	.LASFDE219-.Lframe1
	.long	.LFB456
	.long	.LFE456-.LFB456
	.uleb128 0x0
	.align 8
.LEFDE219:
.LSFDE221:
	.long	.LEFDE221-.LASFDE221
.LASFDE221:
	.long	.LASFDE221-.Lframe1
	.long	.LFB457
	.long	.LFE457-.LFB457
	.uleb128 0x0
	.align 8
.LEFDE221:
.LSFDE223:
	.long	.LEFDE223-.LASFDE223
.LASFDE223:
	.long	.LASFDE223-.Lframe1
	.long	.LFB458
	.long	.LFE458-.LFB458
	.uleb128 0x0
	.align 8
.LEFDE223:
.LSFDE225:
	.long	.LEFDE225-.LASFDE225
.LASFDE225:
	.long	.LASFDE225-.Lframe1
	.long	.LFB459
	.long	.LFE459-.LFB459
	.uleb128 0x0
	.align 8
.LEFDE225:
.LSFDE227:
	.long	.LEFDE227-.LASFDE227
.LASFDE227:
	.long	.LASFDE227-.Lframe1
	.long	.LFB460
	.long	.LFE460-.LFB460
	.uleb128 0x0
	.align 8
.LEFDE227:
.LSFDE229:
	.long	.LEFDE229-.LASFDE229
.LASFDE229:
	.long	.LASFDE229-.Lframe1
	.long	.LFB461
	.long	.LFE461-.LFB461
	.uleb128 0x0
	.align 8
.LEFDE229:
.LSFDE231:
	.long	.LEFDE231-.LASFDE231
.LASFDE231:
	.long	.LASFDE231-.Lframe1
	.long	.LFB462
	.long	.LFE462-.LFB462
	.uleb128 0x0
	.align 8
.LEFDE231:
.LSFDE233:
	.long	.LEFDE233-.LASFDE233
.LASFDE233:
	.long	.LASFDE233-.Lframe1
	.long	.LFB463
	.long	.LFE463-.LFB463
	.uleb128 0x0
	.align 8
.LEFDE233:
.LSFDE235:
	.long	.LEFDE235-.LASFDE235
.LASFDE235:
	.long	.LASFDE235-.Lframe1
	.long	.LFB464
	.long	.LFE464-.LFB464
	.uleb128 0x0
	.align 8
.LEFDE235:
.LSFDE237:
	.long	.LEFDE237-.LASFDE237
.LASFDE237:
	.long	.LASFDE237-.Lframe1
	.long	.LFB465
	.long	.LFE465-.LFB465
	.uleb128 0x0
	.align 8
.LEFDE237:
.LSFDE239:
	.long	.LEFDE239-.LASFDE239
.LASFDE239:
	.long	.LASFDE239-.Lframe1
	.long	.LFB466
	.long	.LFE466-.LFB466
	.uleb128 0x0
	.align 8
.LEFDE239:
.LSFDE241:
	.long	.LEFDE241-.LASFDE241
.LASFDE241:
	.long	.LASFDE241-.Lframe1
	.long	.LFB467
	.long	.LFE467-.LFB467
	.uleb128 0x0
	.align 8
.LEFDE241:
.LSFDE243:
	.long	.LEFDE243-.LASFDE243
.LASFDE243:
	.long	.LASFDE243-.Lframe1
	.long	.LFB468
	.long	.LFE468-.LFB468
	.uleb128 0x0
	.align 8
.LEFDE243:
.LSFDE245:
	.long	.LEFDE245-.LASFDE245
.LASFDE245:
	.long	.LASFDE245-.Lframe1
	.long	.LFB469
	.long	.LFE469-.LFB469
	.uleb128 0x0
	.align 8
.LEFDE245:
.LSFDE247:
	.long	.LEFDE247-.LASFDE247
.LASFDE247:
	.long	.LASFDE247-.Lframe1
	.long	.LFB470
	.long	.LFE470-.LFB470
	.uleb128 0x0
	.align 8
.LEFDE247:
.LSFDE249:
	.long	.LEFDE249-.LASFDE249
.LASFDE249:
	.long	.LASFDE249-.Lframe1
	.long	.LFB471
	.long	.LFE471-.LFB471
	.uleb128 0x0
	.align 8
.LEFDE249:
.LSFDE251:
	.long	.LEFDE251-.LASFDE251
.LASFDE251:
	.long	.LASFDE251-.Lframe1
	.long	.LFB472
	.long	.LFE472-.LFB472
	.uleb128 0x0
	.align 8
.LEFDE251:
.LSFDE253:
	.long	.LEFDE253-.LASFDE253
.LASFDE253:
	.long	.LASFDE253-.Lframe1
	.long	.LFB473
	.long	.LFE473-.LFB473
	.uleb128 0x0
	.align 8
.LEFDE253:
.LSFDE255:
	.long	.LEFDE255-.LASFDE255
.LASFDE255:
	.long	.LASFDE255-.Lframe1
	.long	.LFB474
	.long	.LFE474-.LFB474
	.uleb128 0x0
	.align 8
.LEFDE255:
.LSFDE257:
	.long	.LEFDE257-.LASFDE257
.LASFDE257:
	.long	.LASFDE257-.Lframe1
	.long	.LFB475
	.long	.LFE475-.LFB475
	.uleb128 0x0
	.align 8
.LEFDE257:
.LSFDE259:
	.long	.LEFDE259-.LASFDE259
.LASFDE259:
	.long	.LASFDE259-.Lframe1
	.long	.LFB476
	.long	.LFE476-.LFB476
	.uleb128 0x0
	.align 8
.LEFDE259:
.LSFDE261:
	.long	.LEFDE261-.LASFDE261
.LASFDE261:
	.long	.LASFDE261-.Lframe1
	.long	.LFB477
	.long	.LFE477-.LFB477
	.uleb128 0x0
	.align 8
.LEFDE261:
.LSFDE263:
	.long	.LEFDE263-.LASFDE263
.LASFDE263:
	.long	.LASFDE263-.Lframe1
	.long	.LFB478
	.long	.LFE478-.LFB478
	.uleb128 0x0
	.align 8
.LEFDE263:
.LSFDE265:
	.long	.LEFDE265-.LASFDE265
.LASFDE265:
	.long	.LASFDE265-.Lframe1
	.long	.LFB479
	.long	.LFE479-.LFB479
	.uleb128 0x0
	.align 8
.LEFDE265:
.LSFDE267:
	.long	.LEFDE267-.LASFDE267
.LASFDE267:
	.long	.LASFDE267-.Lframe1
	.long	.LFB480
	.long	.LFE480-.LFB480
	.uleb128 0x0
	.align 8
.LEFDE267:
.LSFDE269:
	.long	.LEFDE269-.LASFDE269
.LASFDE269:
	.long	.LASFDE269-.Lframe1
	.long	.LFB481
	.long	.LFE481-.LFB481
	.uleb128 0x0
	.align 8
.LEFDE269:
.LSFDE271:
	.long	.LEFDE271-.LASFDE271
.LASFDE271:
	.long	.LASFDE271-.Lframe1
	.long	.LFB482
	.long	.LFE482-.LFB482
	.uleb128 0x0
	.align 8
.LEFDE271:
.LSFDE273:
	.long	.LEFDE273-.LASFDE273
.LASFDE273:
	.long	.LASFDE273-.Lframe1
	.long	.LFB483
	.long	.LFE483-.LFB483
	.uleb128 0x0
	.align 8
.LEFDE273:
.LSFDE275:
	.long	.LEFDE275-.LASFDE275
.LASFDE275:
	.long	.LASFDE275-.Lframe1
	.long	.LFB484
	.long	.LFE484-.LFB484
	.uleb128 0x0
	.align 8
.LEFDE275:
.LSFDE277:
	.long	.LEFDE277-.LASFDE277
.LASFDE277:
	.long	.LASFDE277-.Lframe1
	.long	.LFB485
	.long	.LFE485-.LFB485
	.uleb128 0x0
	.align 8
.LEFDE277:
.LSFDE279:
	.long	.LEFDE279-.LASFDE279
.LASFDE279:
	.long	.LASFDE279-.Lframe1
	.long	.LFB486
	.long	.LFE486-.LFB486
	.uleb128 0x0
	.align 8
.LEFDE279:
.LSFDE281:
	.long	.LEFDE281-.LASFDE281
.LASFDE281:
	.long	.LASFDE281-.Lframe1
	.long	.LFB487
	.long	.LFE487-.LFB487
	.uleb128 0x0
	.align 8
.LEFDE281:
.LSFDE283:
	.long	.LEFDE283-.LASFDE283
.LASFDE283:
	.long	.LASFDE283-.Lframe1
	.long	.LFB488
	.long	.LFE488-.LFB488
	.uleb128 0x0
	.align 8
.LEFDE283:
.LSFDE285:
	.long	.LEFDE285-.LASFDE285
.LASFDE285:
	.long	.LASFDE285-.Lframe1
	.long	.LFB489
	.long	.LFE489-.LFB489
	.uleb128 0x0
	.align 8
.LEFDE285:
.LSFDE287:
	.long	.LEFDE287-.LASFDE287
.LASFDE287:
	.long	.LASFDE287-.Lframe1
	.long	.LFB490
	.long	.LFE490-.LFB490
	.uleb128 0x0
	.align 8
.LEFDE287:
.LSFDE289:
	.long	.LEFDE289-.LASFDE289
.LASFDE289:
	.long	.LASFDE289-.Lframe1
	.long	.LFB491
	.long	.LFE491-.LFB491
	.uleb128 0x0
	.align 8
.LEFDE289:
.LSFDE291:
	.long	.LEFDE291-.LASFDE291
.LASFDE291:
	.long	.LASFDE291-.Lframe1
	.long	.LFB492
	.long	.LFE492-.LFB492
	.uleb128 0x0
	.align 8
.LEFDE291:
.LSFDE293:
	.long	.LEFDE293-.LASFDE293
.LASFDE293:
	.long	.LASFDE293-.Lframe1
	.long	.LFB493
	.long	.LFE493-.LFB493
	.uleb128 0x0
	.align 8
.LEFDE293:
.LSFDE295:
	.long	.LEFDE295-.LASFDE295
.LASFDE295:
	.long	.LASFDE295-.Lframe1
	.long	.LFB494
	.long	.LFE494-.LFB494
	.uleb128 0x0
	.align 8
.LEFDE295:
.LSFDE297:
	.long	.LEFDE297-.LASFDE297
.LASFDE297:
	.long	.LASFDE297-.Lframe1
	.long	.LFB495
	.long	.LFE495-.LFB495
	.uleb128 0x0
	.align 8
.LEFDE297:
.LSFDE299:
	.long	.LEFDE299-.LASFDE299
.LASFDE299:
	.long	.LASFDE299-.Lframe1
	.long	.LFB496
	.long	.LFE496-.LFB496
	.uleb128 0x0
	.align 8
.LEFDE299:
.LSFDE301:
	.long	.LEFDE301-.LASFDE301
.LASFDE301:
	.long	.LASFDE301-.Lframe1
	.long	.LFB497
	.long	.LFE497-.LFB497
	.uleb128 0x0
	.align 8
.LEFDE301:
.LSFDE303:
	.long	.LEFDE303-.LASFDE303
.LASFDE303:
	.long	.LASFDE303-.Lframe1
	.long	.LFB498
	.long	.LFE498-.LFB498
	.uleb128 0x0
	.align 8
.LEFDE303:
.LSFDE305:
	.long	.LEFDE305-.LASFDE305
.LASFDE305:
	.long	.LASFDE305-.Lframe1
	.long	.LFB499
	.long	.LFE499-.LFB499
	.uleb128 0x0
	.align 8
.LEFDE305:
.LSFDE307:
	.long	.LEFDE307-.LASFDE307
.LASFDE307:
	.long	.LASFDE307-.Lframe1
	.long	.LFB500
	.long	.LFE500-.LFB500
	.uleb128 0x0
	.align 8
.LEFDE307:
.LSFDE309:
	.long	.LEFDE309-.LASFDE309
.LASFDE309:
	.long	.LASFDE309-.Lframe1
	.long	.LFB501
	.long	.LFE501-.LFB501
	.uleb128 0x0
	.align 8
.LEFDE309:
.LSFDE311:
	.long	.LEFDE311-.LASFDE311
.LASFDE311:
	.long	.LASFDE311-.Lframe1
	.long	.LFB502
	.long	.LFE502-.LFB502
	.uleb128 0x0
	.align 8
.LEFDE311:
.LSFDE313:
	.long	.LEFDE313-.LASFDE313
.LASFDE313:
	.long	.LASFDE313-.Lframe1
	.long	.LFB503
	.long	.LFE503-.LFB503
	.uleb128 0x0
	.align 8
.LEFDE313:
.LSFDE315:
	.long	.LEFDE315-.LASFDE315
.LASFDE315:
	.long	.LASFDE315-.Lframe1
	.long	.LFB504
	.long	.LFE504-.LFB504
	.uleb128 0x0
	.align 8
.LEFDE315:
.LSFDE317:
	.long	.LEFDE317-.LASFDE317
.LASFDE317:
	.long	.LASFDE317-.Lframe1
	.long	.LFB505
	.long	.LFE505-.LFB505
	.uleb128 0x0
	.align 8
.LEFDE317:
.LSFDE319:
	.long	.LEFDE319-.LASFDE319
.LASFDE319:
	.long	.LASFDE319-.Lframe1
	.long	.LFB506
	.long	.LFE506-.LFB506
	.uleb128 0x0
	.align 8
.LEFDE319:
.LSFDE321:
	.long	.LEFDE321-.LASFDE321
.LASFDE321:
	.long	.LASFDE321-.Lframe1
	.long	.LFB507
	.long	.LFE507-.LFB507
	.uleb128 0x0
	.align 8
.LEFDE321:
.LSFDE323:
	.long	.LEFDE323-.LASFDE323
.LASFDE323:
	.long	.LASFDE323-.Lframe1
	.long	.LFB508
	.long	.LFE508-.LFB508
	.uleb128 0x0
	.align 8
.LEFDE323:
.LSFDE325:
	.long	.LEFDE325-.LASFDE325
.LASFDE325:
	.long	.LASFDE325-.Lframe1
	.long	.LFB509
	.long	.LFE509-.LFB509
	.uleb128 0x0
	.align 8
.LEFDE325:
.LSFDE327:
	.long	.LEFDE327-.LASFDE327
.LASFDE327:
	.long	.LASFDE327-.Lframe1
	.long	.LFB510
	.long	.LFE510-.LFB510
	.uleb128 0x0
	.align 8
.LEFDE327:
.LSFDE329:
	.long	.LEFDE329-.LASFDE329
.LASFDE329:
	.long	.LASFDE329-.Lframe1
	.long	.LFB511
	.long	.LFE511-.LFB511
	.uleb128 0x0
	.align 8
.LEFDE329:
.LSFDE331:
	.long	.LEFDE331-.LASFDE331
.LASFDE331:
	.long	.LASFDE331-.Lframe1
	.long	.LFB512
	.long	.LFE512-.LFB512
	.uleb128 0x0
	.align 8
.LEFDE331:
.LSFDE333:
	.long	.LEFDE333-.LASFDE333
.LASFDE333:
	.long	.LASFDE333-.Lframe1
	.long	.LFB513
	.long	.LFE513-.LFB513
	.uleb128 0x0
	.align 8
.LEFDE333:
.LSFDE335:
	.long	.LEFDE335-.LASFDE335
.LASFDE335:
	.long	.LASFDE335-.Lframe1
	.long	.LFB514
	.long	.LFE514-.LFB514
	.uleb128 0x0
	.align 8
.LEFDE335:
.LSFDE337:
	.long	.LEFDE337-.LASFDE337
.LASFDE337:
	.long	.LASFDE337-.Lframe1
	.long	.LFB515
	.long	.LFE515-.LFB515
	.uleb128 0x0
	.align 8
.LEFDE337:
.LSFDE339:
	.long	.LEFDE339-.LASFDE339
.LASFDE339:
	.long	.LASFDE339-.Lframe1
	.long	.LFB516
	.long	.LFE516-.LFB516
	.uleb128 0x0
	.align 8
.LEFDE339:
.LSFDE341:
	.long	.LEFDE341-.LASFDE341
.LASFDE341:
	.long	.LASFDE341-.Lframe1
	.long	.LFB517
	.long	.LFE517-.LFB517
	.uleb128 0x0
	.align 8
.LEFDE341:
.LSFDE343:
	.long	.LEFDE343-.LASFDE343
.LASFDE343:
	.long	.LASFDE343-.Lframe1
	.long	.LFB518
	.long	.LFE518-.LFB518
	.uleb128 0x0
	.align 8
.LEFDE343:
.LSFDE345:
	.long	.LEFDE345-.LASFDE345
.LASFDE345:
	.long	.LASFDE345-.Lframe1
	.long	.LFB519
	.long	.LFE519-.LFB519
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI0-.LFB519
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE345:
.LSFDE347:
	.long	.LEFDE347-.LASFDE347
.LASFDE347:
	.long	.LASFDE347-.Lframe1
	.long	.LFB520
	.long	.LFE520-.LFB520
	.uleb128 0x0
	.align 8
.LEFDE347:
.LSFDE349:
	.long	.LEFDE349-.LASFDE349
.LASFDE349:
	.long	.LASFDE349-.Lframe1
	.long	.LFB521
	.long	.LFE521-.LFB521
	.uleb128 0x0
	.align 8
.LEFDE349:
.LSFDE351:
	.long	.LEFDE351-.LASFDE351
.LASFDE351:
	.long	.LASFDE351-.Lframe1
	.long	.LFB522
	.long	.LFE522-.LFB522
	.uleb128 0x0
	.align 8
.LEFDE351:
.LSFDE353:
	.long	.LEFDE353-.LASFDE353
.LASFDE353:
	.long	.LASFDE353-.Lframe1
	.long	.LFB523
	.long	.LFE523-.LFB523
	.uleb128 0x0
	.align 8
.LEFDE353:
.LSFDE355:
	.long	.LEFDE355-.LASFDE355
.LASFDE355:
	.long	.LASFDE355-.Lframe1
	.long	.LFB524
	.long	.LFE524-.LFB524
	.uleb128 0x0
	.align 8
.LEFDE355:
.LSFDE357:
	.long	.LEFDE357-.LASFDE357
.LASFDE357:
	.long	.LASFDE357-.Lframe1
	.long	.LFB525
	.long	.LFE525-.LFB525
	.uleb128 0x0
	.align 8
.LEFDE357:
.LSFDE359:
	.long	.LEFDE359-.LASFDE359
.LASFDE359:
	.long	.LASFDE359-.Lframe1
	.long	.LFB526
	.long	.LFE526-.LFB526
	.uleb128 0x0
	.align 8
.LEFDE359:
.LSFDE361:
	.long	.LEFDE361-.LASFDE361
.LASFDE361:
	.long	.LASFDE361-.Lframe1
	.long	.LFB527
	.long	.LFE527-.LFB527
	.uleb128 0x0
	.align 8
.LEFDE361:
.LSFDE363:
	.long	.LEFDE363-.LASFDE363
.LASFDE363:
	.long	.LASFDE363-.Lframe1
	.long	.LFB528
	.long	.LFE528-.LFB528
	.uleb128 0x0
	.align 8
.LEFDE363:
.LSFDE365:
	.long	.LEFDE365-.LASFDE365
.LASFDE365:
	.long	.LASFDE365-.Lframe1
	.long	.LFB529
	.long	.LFE529-.LFB529
	.uleb128 0x0
	.align 8
.LEFDE365:
.LSFDE367:
	.long	.LEFDE367-.LASFDE367
.LASFDE367:
	.long	.LASFDE367-.Lframe1
	.long	.LFB530
	.long	.LFE530-.LFB530
	.uleb128 0x0
	.align 8
.LEFDE367:
.LSFDE369:
	.long	.LEFDE369-.LASFDE369
.LASFDE369:
	.long	.LASFDE369-.Lframe1
	.long	.LFB531
	.long	.LFE531-.LFB531
	.uleb128 0x0
	.align 8
.LEFDE369:
.LSFDE371:
	.long	.LEFDE371-.LASFDE371
.LASFDE371:
	.long	.LASFDE371-.Lframe1
	.long	.LFB532
	.long	.LFE532-.LFB532
	.uleb128 0x0
	.align 8
.LEFDE371:
.LSFDE373:
	.long	.LEFDE373-.LASFDE373
.LASFDE373:
	.long	.LASFDE373-.Lframe1
	.long	.LFB533
	.long	.LFE533-.LFB533
	.uleb128 0x0
	.align 8
.LEFDE373:
.LSFDE375:
	.long	.LEFDE375-.LASFDE375
.LASFDE375:
	.long	.LASFDE375-.Lframe1
	.long	.LFB534
	.long	.LFE534-.LFB534
	.uleb128 0x0
	.align 8
.LEFDE375:
.LSFDE377:
	.long	.LEFDE377-.LASFDE377
.LASFDE377:
	.long	.LASFDE377-.Lframe1
	.long	.LFB535
	.long	.LFE535-.LFB535
	.uleb128 0x0
	.align 8
.LEFDE377:
.LSFDE379:
	.long	.LEFDE379-.LASFDE379
.LASFDE379:
	.long	.LASFDE379-.Lframe1
	.long	.LFB536
	.long	.LFE536-.LFB536
	.uleb128 0x0
	.align 8
.LEFDE379:
.LSFDE381:
	.long	.LEFDE381-.LASFDE381
.LASFDE381:
	.long	.LASFDE381-.Lframe1
	.long	.LFB537
	.long	.LFE537-.LFB537
	.uleb128 0x0
	.align 8
.LEFDE381:
.LSFDE383:
	.long	.LEFDE383-.LASFDE383
.LASFDE383:
	.long	.LASFDE383-.Lframe1
	.long	.LFB538
	.long	.LFE538-.LFB538
	.uleb128 0x0
	.align 8
.LEFDE383:
.LSFDE385:
	.long	.LEFDE385-.LASFDE385
.LASFDE385:
	.long	.LASFDE385-.Lframe1
	.long	.LFB545
	.long	.LFE545-.LFB545
	.uleb128 0x0
	.align 8
.LEFDE385:
.LSFDE387:
	.long	.LEFDE387-.LASFDE387
.LASFDE387:
	.long	.LASFDE387-.Lframe1
	.long	.LFB546
	.long	.LFE546-.LFB546
	.uleb128 0x0
	.align 8
.LEFDE387:
.LSFDE389:
	.long	.LEFDE389-.LASFDE389
.LASFDE389:
	.long	.LASFDE389-.Lframe1
	.long	.LFB547
	.long	.LFE547-.LFB547
	.uleb128 0x0
	.align 8
.LEFDE389:
.LSFDE391:
	.long	.LEFDE391-.LASFDE391
.LASFDE391:
	.long	.LASFDE391-.Lframe1
	.long	.LFB548
	.long	.LFE548-.LFB548
	.uleb128 0x0
	.align 8
.LEFDE391:
.LSFDE393:
	.long	.LEFDE393-.LASFDE393
.LASFDE393:
	.long	.LASFDE393-.Lframe1
	.long	.LFB549
	.long	.LFE549-.LFB549
	.uleb128 0x0
	.align 8
.LEFDE393:
.LSFDE395:
	.long	.LEFDE395-.LASFDE395
.LASFDE395:
	.long	.LASFDE395-.Lframe1
	.long	.LFB550
	.long	.LFE550-.LFB550
	.uleb128 0x0
	.align 8
.LEFDE395:
.LSFDE397:
	.long	.LEFDE397-.LASFDE397
.LASFDE397:
	.long	.LASFDE397-.Lframe1
	.long	.LFB551
	.long	.LFE551-.LFB551
	.uleb128 0x0
	.align 8
.LEFDE397:
.LSFDE399:
	.long	.LEFDE399-.LASFDE399
.LASFDE399:
	.long	.LASFDE399-.Lframe1
	.long	.LFB552
	.long	.LFE552-.LFB552
	.uleb128 0x0
	.align 8
.LEFDE399:
.LSFDE401:
	.long	.LEFDE401-.LASFDE401
.LASFDE401:
	.long	.LASFDE401-.Lframe1
	.long	.LFB553
	.long	.LFE553-.LFB553
	.uleb128 0x0
	.align 8
.LEFDE401:
.LSFDE403:
	.long	.LEFDE403-.LASFDE403
.LASFDE403:
	.long	.LASFDE403-.Lframe1
	.long	.LFB554
	.long	.LFE554-.LFB554
	.uleb128 0x0
	.align 8
.LEFDE403:
.LSFDE405:
	.long	.LEFDE405-.LASFDE405
.LASFDE405:
	.long	.LASFDE405-.Lframe1
	.long	.LFB555
	.long	.LFE555-.LFB555
	.uleb128 0x0
	.align 8
.LEFDE405:
.LSFDE407:
	.long	.LEFDE407-.LASFDE407
.LASFDE407:
	.long	.LASFDE407-.Lframe1
	.long	.LFB556
	.long	.LFE556-.LFB556
	.uleb128 0x0
	.align 8
.LEFDE407:
.LSFDE409:
	.long	.LEFDE409-.LASFDE409
.LASFDE409:
	.long	.LASFDE409-.Lframe1
	.long	.LFB559
	.long	.LFE559-.LFB559
	.uleb128 0x0
	.align 8
.LEFDE409:
.LSFDE411:
	.long	.LEFDE411-.LASFDE411
.LASFDE411:
	.long	.LASFDE411-.Lframe1
	.long	.LFB560
	.long	.LFE560-.LFB560
	.uleb128 0x0
	.align 8
.LEFDE411:
.LSFDE413:
	.long	.LEFDE413-.LASFDE413
.LASFDE413:
	.long	.LASFDE413-.Lframe1
	.long	.LFB618
	.long	.LFE618-.LFB618
	.uleb128 0x0
	.align 8
.LEFDE413:
.LSFDE415:
	.long	.LEFDE415-.LASFDE415
.LASFDE415:
	.long	.LASFDE415-.Lframe1
	.long	.LFB619
	.long	.LFE619-.LFB619
	.uleb128 0x0
	.align 8
.LEFDE415:
.LSFDE417:
	.long	.LEFDE417-.LASFDE417
.LASFDE417:
	.long	.LASFDE417-.Lframe1
	.long	.LFB627
	.long	.LFE627-.LFB627
	.uleb128 0x0
	.align 8
.LEFDE417:
.LSFDE419:
	.long	.LEFDE419-.LASFDE419
.LASFDE419:
	.long	.LASFDE419-.Lframe1
	.long	.LFB628
	.long	.LFE628-.LFB628
	.uleb128 0x0
	.align 8
.LEFDE419:
.LSFDE421:
	.long	.LEFDE421-.LASFDE421
.LASFDE421:
	.long	.LASFDE421-.Lframe1
	.long	.LFB629
	.long	.LFE629-.LFB629
	.uleb128 0x0
	.align 8
.LEFDE421:
.LSFDE423:
	.long	.LEFDE423-.LASFDE423
.LASFDE423:
	.long	.LASFDE423-.Lframe1
	.long	.LFB630
	.long	.LFE630-.LFB630
	.uleb128 0x0
	.align 8
.LEFDE423:
.LSFDE425:
	.long	.LEFDE425-.LASFDE425
.LASFDE425:
	.long	.LASFDE425-.Lframe1
	.long	.LFB635
	.long	.LFE635-.LFB635
	.uleb128 0x0
	.align 8
.LEFDE425:
.LSFDE427:
	.long	.LEFDE427-.LASFDE427
.LASFDE427:
	.long	.LASFDE427-.Lframe1
	.long	.LFB637
	.long	.LFE637-.LFB637
	.uleb128 0x0
	.align 8
.LEFDE427:
.LSFDE429:
	.long	.LEFDE429-.LASFDE429
.LASFDE429:
	.long	.LASFDE429-.Lframe1
	.long	.LFB639
	.long	.LFE639-.LFB639
	.uleb128 0x0
	.align 8
.LEFDE429:
.LSFDE431:
	.long	.LEFDE431-.LASFDE431
.LASFDE431:
	.long	.LASFDE431-.Lframe1
	.long	.LFB641
	.long	.LFE641-.LFB641
	.uleb128 0x0
	.align 8
.LEFDE431:
.LSFDE433:
	.long	.LEFDE433-.LASFDE433
.LASFDE433:
	.long	.LASFDE433-.Lframe1
	.long	.LFB643
	.long	.LFE643-.LFB643
	.uleb128 0x0
	.align 8
.LEFDE433:
.LSFDE435:
	.long	.LEFDE435-.LASFDE435
.LASFDE435:
	.long	.LASFDE435-.Lframe1
	.long	.LFB645
	.long	.LFE645-.LFB645
	.uleb128 0x0
	.align 8
.LEFDE435:
.LSFDE437:
	.long	.LEFDE437-.LASFDE437
.LASFDE437:
	.long	.LASFDE437-.Lframe1
	.long	.LFB647
	.long	.LFE647-.LFB647
	.uleb128 0x0
	.align 8
.LEFDE437:
.LSFDE439:
	.long	.LEFDE439-.LASFDE439
.LASFDE439:
	.long	.LASFDE439-.Lframe1
	.long	.LFB651
	.long	.LFE651-.LFB651
	.uleb128 0x0
	.align 8
.LEFDE439:
.LSFDE441:
	.long	.LEFDE441-.LASFDE441
.LASFDE441:
	.long	.LASFDE441-.Lframe1
	.long	.LFB654
	.long	.LFE654-.LFB654
	.uleb128 0x0
	.align 8
.LEFDE441:
.LSFDE443:
	.long	.LEFDE443-.LASFDE443
.LASFDE443:
	.long	.LASFDE443-.Lframe1
	.long	.LFB655
	.long	.LFE655-.LFB655
	.uleb128 0x0
	.align 8
.LEFDE443:
.LSFDE445:
	.long	.LEFDE445-.LASFDE445
.LASFDE445:
	.long	.LASFDE445-.Lframe1
	.long	.LFB656
	.long	.LFE656-.LFB656
	.uleb128 0x0
	.align 8
.LEFDE445:
.LSFDE447:
	.long	.LEFDE447-.LASFDE447
.LASFDE447:
	.long	.LASFDE447-.Lframe1
	.long	.LFB657
	.long	.LFE657-.LFB657
	.uleb128 0x0
	.align 8
.LEFDE447:
.LSFDE449:
	.long	.LEFDE449-.LASFDE449
.LASFDE449:
	.long	.LASFDE449-.Lframe1
	.long	.LFB658
	.long	.LFE658-.LFB658
	.uleb128 0x0
	.align 8
.LEFDE449:
.LSFDE451:
	.long	.LEFDE451-.LASFDE451
.LASFDE451:
	.long	.LASFDE451-.Lframe1
	.long	.LFB659
	.long	.LFE659-.LFB659
	.uleb128 0x0
	.align 8
.LEFDE451:
.LSFDE453:
	.long	.LEFDE453-.LASFDE453
.LASFDE453:
	.long	.LASFDE453-.Lframe1
	.long	.LFB660
	.long	.LFE660-.LFB660
	.uleb128 0x0
	.align 8
.LEFDE453:
.LSFDE455:
	.long	.LEFDE455-.LASFDE455
.LASFDE455:
	.long	.LASFDE455-.Lframe1
	.long	.LFB661
	.long	.LFE661-.LFB661
	.uleb128 0x0
	.align 8
.LEFDE455:
.LSFDE457:
	.long	.LEFDE457-.LASFDE457
.LASFDE457:
	.long	.LASFDE457-.Lframe1
	.long	.LFB662
	.long	.LFE662-.LFB662
	.uleb128 0x0
	.align 8
.LEFDE457:
.LSFDE459:
	.long	.LEFDE459-.LASFDE459
.LASFDE459:
	.long	.LASFDE459-.Lframe1
	.long	.LFB663
	.long	.LFE663-.LFB663
	.uleb128 0x0
	.align 8
.LEFDE459:
.LSFDE461:
	.long	.LEFDE461-.LASFDE461
.LASFDE461:
	.long	.LASFDE461-.Lframe1
	.long	.LFB665
	.long	.LFE665-.LFB665
	.uleb128 0x0
	.align 8
.LEFDE461:
.LSFDE463:
	.long	.LEFDE463-.LASFDE463
.LASFDE463:
	.long	.LASFDE463-.Lframe1
	.long	.LFB666
	.long	.LFE666-.LFB666
	.uleb128 0x0
	.align 8
.LEFDE463:
.LSFDE465:
	.long	.LEFDE465-.LASFDE465
.LASFDE465:
	.long	.LASFDE465-.Lframe1
	.long	.LFB667
	.long	.LFE667-.LFB667
	.uleb128 0x0
	.align 8
.LEFDE465:
.LSFDE467:
	.long	.LEFDE467-.LASFDE467
.LASFDE467:
	.long	.LASFDE467-.Lframe1
	.long	.LFB668
	.long	.LFE668-.LFB668
	.uleb128 0x0
	.align 8
.LEFDE467:
.LSFDE469:
	.long	.LEFDE469-.LASFDE469
.LASFDE469:
	.long	.LASFDE469-.Lframe1
	.long	.LFB669
	.long	.LFE669-.LFB669
	.uleb128 0x0
	.align 8
.LEFDE469:
.LSFDE471:
	.long	.LEFDE471-.LASFDE471
.LASFDE471:
	.long	.LASFDE471-.Lframe1
	.long	.LFB672
	.long	.LFE672-.LFB672
	.uleb128 0x0
	.align 8
.LEFDE471:
.LSFDE473:
	.long	.LEFDE473-.LASFDE473
.LASFDE473:
	.long	.LASFDE473-.Lframe1
	.long	.LFB673
	.long	.LFE673-.LFB673
	.uleb128 0x0
	.align 8
.LEFDE473:
.LSFDE475:
	.long	.LEFDE475-.LASFDE475
.LASFDE475:
	.long	.LASFDE475-.Lframe1
	.long	.LFB676
	.long	.LFE676-.LFB676
	.uleb128 0x0
	.align 8
.LEFDE475:
.LSFDE477:
	.long	.LEFDE477-.LASFDE477
.LASFDE477:
	.long	.LASFDE477-.Lframe1
	.long	.LFB677
	.long	.LFE677-.LFB677
	.uleb128 0x0
	.align 8
.LEFDE477:
.LSFDE479:
	.long	.LEFDE479-.LASFDE479
.LASFDE479:
	.long	.LASFDE479-.Lframe1
	.long	.LFB678
	.long	.LFE678-.LFB678
	.uleb128 0x0
	.align 8
.LEFDE479:
.LSFDE481:
	.long	.LEFDE481-.LASFDE481
.LASFDE481:
	.long	.LASFDE481-.Lframe1
	.long	.LFB679
	.long	.LFE679-.LFB679
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI1-.LFB679
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE481:
.LSFDE483:
	.long	.LEFDE483-.LASFDE483
.LASFDE483:
	.long	.LASFDE483-.Lframe1
	.long	.LFB680
	.long	.LFE680-.LFB680
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI2-.LFB680
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE483:
.LSFDE485:
	.long	.LEFDE485-.LASFDE485
.LASFDE485:
	.long	.LASFDE485-.Lframe1
	.long	.LFB681
	.long	.LFE681-.LFB681
	.uleb128 0x0
	.align 8
.LEFDE485:
.LSFDE487:
	.long	.LEFDE487-.LASFDE487
.LASFDE487:
	.long	.LASFDE487-.Lframe1
	.long	.LFB718
	.long	.LFE718-.LFB718
	.uleb128 0x0
	.align 8
.LEFDE487:
.LSFDE489:
	.long	.LEFDE489-.LASFDE489
.LASFDE489:
	.long	.LASFDE489-.Lframe1
	.long	.LFB726
	.long	.LFE726-.LFB726
	.uleb128 0x0
	.align 8
.LEFDE489:
.LSFDE491:
	.long	.LEFDE491-.LASFDE491
.LASFDE491:
	.long	.LASFDE491-.Lframe1
	.long	.LFB728
	.long	.LFE728-.LFB728
	.uleb128 0x0
	.align 8
.LEFDE491:
.LSFDE493:
	.long	.LEFDE493-.LASFDE493
.LASFDE493:
	.long	.LASFDE493-.Lframe1
	.long	.LFB730
	.long	.LFE730-.LFB730
	.uleb128 0x0
	.align 8
.LEFDE493:
.LSFDE495:
	.long	.LEFDE495-.LASFDE495
.LASFDE495:
	.long	.LASFDE495-.Lframe1
	.long	.LFB732
	.long	.LFE732-.LFB732
	.uleb128 0x0
	.align 8
.LEFDE495:
.LSFDE497:
	.long	.LEFDE497-.LASFDE497
.LASFDE497:
	.long	.LASFDE497-.Lframe1
	.long	.LFB734
	.long	.LFE734-.LFB734
	.uleb128 0x0
	.align 8
.LEFDE497:
.LSFDE499:
	.long	.LEFDE499-.LASFDE499
.LASFDE499:
	.long	.LASFDE499-.Lframe1
	.long	.LFB738
	.long	.LFE738-.LFB738
	.uleb128 0x0
	.align 8
.LEFDE499:
.LSFDE501:
	.long	.LEFDE501-.LASFDE501
.LASFDE501:
	.long	.LASFDE501-.Lframe1
	.long	.LFB741
	.long	.LFE741-.LFB741
	.uleb128 0x0
	.align 8
.LEFDE501:
.LSFDE503:
	.long	.LEFDE503-.LASFDE503
.LASFDE503:
	.long	.LASFDE503-.Lframe1
	.long	.LFB742
	.long	.LFE742-.LFB742
	.uleb128 0x0
	.align 8
.LEFDE503:
.LSFDE505:
	.long	.LEFDE505-.LASFDE505
.LASFDE505:
	.long	.LASFDE505-.Lframe1
	.long	.LFB743
	.long	.LFE743-.LFB743
	.uleb128 0x0
	.align 8
.LEFDE505:
.LSFDE507:
	.long	.LEFDE507-.LASFDE507
.LASFDE507:
	.long	.LASFDE507-.Lframe1
	.long	.LFB744
	.long	.LFE744-.LFB744
	.uleb128 0x0
	.align 8
.LEFDE507:
.LSFDE509:
	.long	.LEFDE509-.LASFDE509
.LASFDE509:
	.long	.LASFDE509-.Lframe1
	.long	.LFB745
	.long	.LFE745-.LFB745
	.uleb128 0x0
	.align 8
.LEFDE509:
.LSFDE511:
	.long	.LEFDE511-.LASFDE511
.LASFDE511:
	.long	.LASFDE511-.Lframe1
	.long	.LFB746
	.long	.LFE746-.LFB746
	.uleb128 0x0
	.align 8
.LEFDE511:
.LSFDE513:
	.long	.LEFDE513-.LASFDE513
.LASFDE513:
	.long	.LASFDE513-.Lframe1
	.long	.LFB747
	.long	.LFE747-.LFB747
	.uleb128 0x0
	.align 8
.LEFDE513:
.LSFDE515:
	.long	.LEFDE515-.LASFDE515
.LASFDE515:
	.long	.LASFDE515-.Lframe1
	.long	.LFB748
	.long	.LFE748-.LFB748
	.uleb128 0x0
	.align 8
.LEFDE515:
.LSFDE517:
	.long	.LEFDE517-.LASFDE517
.LASFDE517:
	.long	.LASFDE517-.Lframe1
	.long	.LFB749
	.long	.LFE749-.LFB749
	.uleb128 0x0
	.align 8
.LEFDE517:
.LSFDE519:
	.long	.LEFDE519-.LASFDE519
.LASFDE519:
	.long	.LASFDE519-.Lframe1
	.long	.LFB750
	.long	.LFE750-.LFB750
	.uleb128 0x0
	.align 8
.LEFDE519:
.LSFDE521:
	.long	.LEFDE521-.LASFDE521
.LASFDE521:
	.long	.LASFDE521-.Lframe1
	.long	.LFB751
	.long	.LFE751-.LFB751
	.uleb128 0x0
	.align 8
.LEFDE521:
.LSFDE523:
	.long	.LEFDE523-.LASFDE523
.LASFDE523:
	.long	.LASFDE523-.Lframe1
	.long	.LFB752
	.long	.LFE752-.LFB752
	.uleb128 0x0
	.align 8
.LEFDE523:
.LSFDE525:
	.long	.LEFDE525-.LASFDE525
.LASFDE525:
	.long	.LASFDE525-.Lframe1
	.long	.LFB753
	.long	.LFE753-.LFB753
	.uleb128 0x0
	.align 8
.LEFDE525:
.LSFDE527:
	.long	.LEFDE527-.LASFDE527
.LASFDE527:
	.long	.LASFDE527-.Lframe1
	.long	.LFB754
	.long	.LFE754-.LFB754
	.uleb128 0x0
	.align 8
.LEFDE527:
.LSFDE529:
	.long	.LEFDE529-.LASFDE529
.LASFDE529:
	.long	.LASFDE529-.Lframe1
	.long	.LFB756
	.long	.LFE756-.LFB756
	.uleb128 0x0
	.align 8
.LEFDE529:
.LSFDE531:
	.long	.LEFDE531-.LASFDE531
.LASFDE531:
	.long	.LASFDE531-.Lframe1
	.long	.LFB757
	.long	.LFE757-.LFB757
	.uleb128 0x0
	.align 8
.LEFDE531:
.LSFDE533:
	.long	.LEFDE533-.LASFDE533
.LASFDE533:
	.long	.LASFDE533-.Lframe1
	.long	.LFB758
	.long	.LFE758-.LFB758
	.uleb128 0x0
	.align 8
.LEFDE533:
.LSFDE535:
	.long	.LEFDE535-.LASFDE535
.LASFDE535:
	.long	.LASFDE535-.Lframe1
	.long	.LFB759
	.long	.LFE759-.LFB759
	.uleb128 0x0
	.align 8
.LEFDE535:
.LSFDE537:
	.long	.LEFDE537-.LASFDE537
.LASFDE537:
	.long	.LASFDE537-.Lframe1
	.long	.LFB760
	.long	.LFE760-.LFB760
	.uleb128 0x0
	.align 8
.LEFDE537:
.LSFDE539:
	.long	.LEFDE539-.LASFDE539
.LASFDE539:
	.long	.LASFDE539-.Lframe1
	.long	.LFB763
	.long	.LFE763-.LFB763
	.uleb128 0x0
	.align 8
.LEFDE539:
.LSFDE541:
	.long	.LEFDE541-.LASFDE541
.LASFDE541:
	.long	.LASFDE541-.Lframe1
	.long	.LFB764
	.long	.LFE764-.LFB764
	.uleb128 0x0
	.align 8
.LEFDE541:
.LSFDE543:
	.long	.LEFDE543-.LASFDE543
.LASFDE543:
	.long	.LASFDE543-.Lframe1
	.long	.LFB767
	.long	.LFE767-.LFB767
	.uleb128 0x0
	.align 8
.LEFDE543:
.LSFDE545:
	.long	.LEFDE545-.LASFDE545
.LASFDE545:
	.long	.LASFDE545-.Lframe1
	.long	.LFB768
	.long	.LFE768-.LFB768
	.uleb128 0x0
	.align 8
.LEFDE545:
.LSFDE547:
	.long	.LEFDE547-.LASFDE547
.LASFDE547:
	.long	.LASFDE547-.Lframe1
	.long	.LFB769
	.long	.LFE769-.LFB769
	.uleb128 0x0
	.align 8
.LEFDE547:
.LSFDE549:
	.long	.LEFDE549-.LASFDE549
.LASFDE549:
	.long	.LASFDE549-.Lframe1
	.long	.LFB770
	.long	.LFE770-.LFB770
	.uleb128 0x0
	.align 8
.LEFDE549:
.LSFDE551:
	.long	.LEFDE551-.LASFDE551
.LASFDE551:
	.long	.LASFDE551-.Lframe1
	.long	.LFB771
	.long	.LFE771-.LFB771
	.uleb128 0x0
	.align 8
.LEFDE551:
.LSFDE553:
	.long	.LEFDE553-.LASFDE553
.LASFDE553:
	.long	.LASFDE553-.Lframe1
	.long	.LFB772
	.long	.LFE772-.LFB772
	.uleb128 0x0
	.align 8
.LEFDE553:
.LSFDE555:
	.long	.LEFDE555-.LASFDE555
.LASFDE555:
	.long	.LASFDE555-.Lframe1
	.long	.LFB773
	.long	.LFE773-.LFB773
	.uleb128 0x0
	.align 8
.LEFDE555:
.LSFDE557:
	.long	.LEFDE557-.LASFDE557
.LASFDE557:
	.long	.LASFDE557-.Lframe1
	.long	.LFB774
	.long	.LFE774-.LFB774
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI3-.LFB774
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE557:
.LSFDE559:
	.long	.LEFDE559-.LASFDE559
.LASFDE559:
	.long	.LASFDE559-.Lframe1
	.long	.LFB775
	.long	.LFE775-.LFB775
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI4-.LFB775
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE559:
.LSFDE561:
	.long	.LEFDE561-.LASFDE561
.LASFDE561:
	.long	.LASFDE561-.Lframe1
	.long	.LFB776
	.long	.LFE776-.LFB776
	.uleb128 0x0
	.align 8
.LEFDE561:
.LSFDE563:
	.long	.LEFDE563-.LASFDE563
.LASFDE563:
	.long	.LASFDE563-.Lframe1
	.long	.LFB825
	.long	.LFE825-.LFB825
	.uleb128 0x0
	.align 8
.LEFDE563:
.LSFDE565:
	.long	.LEFDE565-.LASFDE565
.LASFDE565:
	.long	.LASFDE565-.Lframe1
	.long	.LFB826
	.long	.LFE826-.LFB826
	.uleb128 0x0
	.align 8
.LEFDE565:
.LSFDE567:
	.long	.LEFDE567-.LASFDE567
.LASFDE567:
	.long	.LASFDE567-.Lframe1
	.long	.LFB827
	.long	.LFE827-.LFB827
	.uleb128 0x0
	.align 8
.LEFDE567:
.LSFDE569:
	.long	.LEFDE569-.LASFDE569
.LASFDE569:
	.long	.LASFDE569-.Lframe1
	.long	.LFB828
	.long	.LFE828-.LFB828
	.uleb128 0x0
	.align 8
.LEFDE569:
.LSFDE571:
	.long	.LEFDE571-.LASFDE571
.LASFDE571:
	.long	.LASFDE571-.Lframe1
	.long	.LFB829
	.long	.LFE829-.LFB829
	.uleb128 0x0
	.align 8
.LEFDE571:
.LSFDE573:
	.long	.LEFDE573-.LASFDE573
.LASFDE573:
	.long	.LASFDE573-.Lframe1
	.long	.LFB830
	.long	.LFE830-.LFB830
	.uleb128 0x0
	.align 8
.LEFDE573:
.LSFDE575:
	.long	.LEFDE575-.LASFDE575
.LASFDE575:
	.long	.LASFDE575-.Lframe1
	.long	.LFB831
	.long	.LFE831-.LFB831
	.uleb128 0x0
	.align 8
.LEFDE575:
.LSFDE577:
	.long	.LEFDE577-.LASFDE577
.LASFDE577:
	.long	.LASFDE577-.Lframe1
	.long	.LFB832
	.long	.LFE832-.LFB832
	.uleb128 0x0
	.align 8
.LEFDE577:
.LSFDE579:
	.long	.LEFDE579-.LASFDE579
.LASFDE579:
	.long	.LASFDE579-.Lframe1
	.long	.LFB840
	.long	.LFE840-.LFB840
	.uleb128 0x0
	.align 8
.LEFDE579:
.LSFDE581:
	.long	.LEFDE581-.LASFDE581
.LASFDE581:
	.long	.LASFDE581-.Lframe1
	.long	.LFB842
	.long	.LFE842-.LFB842
	.uleb128 0x0
	.align 8
.LEFDE581:
.LSFDE583:
	.long	.LEFDE583-.LASFDE583
.LASFDE583:
	.long	.LASFDE583-.Lframe1
	.long	.LFB844
	.long	.LFE844-.LFB844
	.uleb128 0x0
	.align 8
.LEFDE583:
.LSFDE585:
	.long	.LEFDE585-.LASFDE585
.LASFDE585:
	.long	.LASFDE585-.Lframe1
	.long	.LFB846
	.long	.LFE846-.LFB846
	.uleb128 0x0
	.align 8
.LEFDE585:
.LSFDE587:
	.long	.LEFDE587-.LASFDE587
.LASFDE587:
	.long	.LASFDE587-.Lframe1
	.long	.LFB848
	.long	.LFE848-.LFB848
	.uleb128 0x0
	.align 8
.LEFDE587:
.LSFDE589:
	.long	.LEFDE589-.LASFDE589
.LASFDE589:
	.long	.LASFDE589-.Lframe1
	.long	.LFB850
	.long	.LFE850-.LFB850
	.uleb128 0x0
	.align 8
.LEFDE589:
.LSFDE591:
	.long	.LEFDE591-.LASFDE591
.LASFDE591:
	.long	.LASFDE591-.Lframe1
	.long	.LFB853
	.long	.LFE853-.LFB853
	.uleb128 0x0
	.align 8
.LEFDE591:
.LSFDE593:
	.long	.LEFDE593-.LASFDE593
.LASFDE593:
	.long	.LASFDE593-.Lframe1
	.long	.LFB854
	.long	.LFE854-.LFB854
	.uleb128 0x0
	.align 8
.LEFDE593:
.LSFDE595:
	.long	.LEFDE595-.LASFDE595
.LASFDE595:
	.long	.LASFDE595-.Lframe1
	.long	.LFB856
	.long	.LFE856-.LFB856
	.uleb128 0x0
	.align 8
.LEFDE595:
.LSFDE597:
	.long	.LEFDE597-.LASFDE597
.LASFDE597:
	.long	.LASFDE597-.Lframe1
	.long	.LFB858
	.long	.LFE858-.LFB858
	.uleb128 0x0
	.align 8
.LEFDE597:
.LSFDE599:
	.long	.LEFDE599-.LASFDE599
.LASFDE599:
	.long	.LASFDE599-.Lframe1
	.long	.LFB859
	.long	.LFE859-.LFB859
	.uleb128 0x0
	.align 8
.LEFDE599:
.LSFDE601:
	.long	.LEFDE601-.LASFDE601
.LASFDE601:
	.long	.LASFDE601-.Lframe1
	.long	.LFB860
	.long	.LFE860-.LFB860
	.uleb128 0x0
	.align 8
.LEFDE601:
.LSFDE603:
	.long	.LEFDE603-.LASFDE603
.LASFDE603:
	.long	.LASFDE603-.Lframe1
	.long	.LFB861
	.long	.LFE861-.LFB861
	.uleb128 0x0
	.align 8
.LEFDE603:
.LSFDE605:
	.long	.LEFDE605-.LASFDE605
.LASFDE605:
	.long	.LASFDE605-.Lframe1
	.long	.LFB862
	.long	.LFE862-.LFB862
	.uleb128 0x0
	.align 8
.LEFDE605:
.LSFDE607:
	.long	.LEFDE607-.LASFDE607
.LASFDE607:
	.long	.LASFDE607-.Lframe1
	.long	.LFB863
	.long	.LFE863-.LFB863
	.uleb128 0x0
	.align 8
.LEFDE607:
.LSFDE609:
	.long	.LEFDE609-.LASFDE609
.LASFDE609:
	.long	.LASFDE609-.Lframe1
	.long	.LFB864
	.long	.LFE864-.LFB864
	.uleb128 0x0
	.align 8
.LEFDE609:
.LSFDE611:
	.long	.LEFDE611-.LASFDE611
.LASFDE611:
	.long	.LASFDE611-.Lframe1
	.long	.LFB865
	.long	.LFE865-.LFB865
	.uleb128 0x0
	.align 8
.LEFDE611:
.LSFDE613:
	.long	.LEFDE613-.LASFDE613
.LASFDE613:
	.long	.LASFDE613-.Lframe1
	.long	.LFB866
	.long	.LFE866-.LFB866
	.uleb128 0x0
	.align 8
.LEFDE613:
.LSFDE615:
	.long	.LEFDE615-.LASFDE615
.LASFDE615:
	.long	.LASFDE615-.Lframe1
	.long	.LFB867
	.long	.LFE867-.LFB867
	.uleb128 0x0
	.align 8
.LEFDE615:
.LSFDE617:
	.long	.LEFDE617-.LASFDE617
.LASFDE617:
	.long	.LASFDE617-.Lframe1
	.long	.LFB868
	.long	.LFE868-.LFB868
	.uleb128 0x0
	.align 8
.LEFDE617:
.LSFDE619:
	.long	.LEFDE619-.LASFDE619
.LASFDE619:
	.long	.LASFDE619-.Lframe1
	.long	.LFB869
	.long	.LFE869-.LFB869
	.uleb128 0x0
	.align 8
.LEFDE619:
.LSFDE621:
	.long	.LEFDE621-.LASFDE621
.LASFDE621:
	.long	.LASFDE621-.Lframe1
	.long	.LFB870
	.long	.LFE870-.LFB870
	.uleb128 0x0
	.align 8
.LEFDE621:
.LSFDE623:
	.long	.LEFDE623-.LASFDE623
.LASFDE623:
	.long	.LASFDE623-.Lframe1
	.long	.LFB871
	.long	.LFE871-.LFB871
	.uleb128 0x0
	.align 8
.LEFDE623:
.LSFDE625:
	.long	.LEFDE625-.LASFDE625
.LASFDE625:
	.long	.LASFDE625-.Lframe1
	.long	.LFB873
	.long	.LFE873-.LFB873
	.uleb128 0x0
	.align 8
.LEFDE625:
.LSFDE627:
	.long	.LEFDE627-.LASFDE627
.LASFDE627:
	.long	.LASFDE627-.Lframe1
	.long	.LFB874
	.long	.LFE874-.LFB874
	.uleb128 0x0
	.align 8
.LEFDE627:
.LSFDE629:
	.long	.LEFDE629-.LASFDE629
.LASFDE629:
	.long	.LASFDE629-.Lframe1
	.long	.LFB875
	.long	.LFE875-.LFB875
	.uleb128 0x0
	.align 8
.LEFDE629:
.LSFDE631:
	.long	.LEFDE631-.LASFDE631
.LASFDE631:
	.long	.LASFDE631-.Lframe1
	.long	.LFB876
	.long	.LFE876-.LFB876
	.uleb128 0x0
	.align 8
.LEFDE631:
.LSFDE633:
	.long	.LEFDE633-.LASFDE633
.LASFDE633:
	.long	.LASFDE633-.Lframe1
	.long	.LFB877
	.long	.LFE877-.LFB877
	.uleb128 0x0
	.align 8
.LEFDE633:
.LSFDE635:
	.long	.LEFDE635-.LASFDE635
.LASFDE635:
	.long	.LASFDE635-.Lframe1
	.long	.LFB880
	.long	.LFE880-.LFB880
	.uleb128 0x0
	.align 8
.LEFDE635:
.LSFDE637:
	.long	.LEFDE637-.LASFDE637
.LASFDE637:
	.long	.LASFDE637-.Lframe1
	.long	.LFB881
	.long	.LFE881-.LFB881
	.uleb128 0x0
	.align 8
.LEFDE637:
.LSFDE639:
	.long	.LEFDE639-.LASFDE639
.LASFDE639:
	.long	.LASFDE639-.Lframe1
	.long	.LFB884
	.long	.LFE884-.LFB884
	.uleb128 0x0
	.align 8
.LEFDE639:
.LSFDE641:
	.long	.LEFDE641-.LASFDE641
.LASFDE641:
	.long	.LASFDE641-.Lframe1
	.long	.LFB885
	.long	.LFE885-.LFB885
	.uleb128 0x0
	.align 8
.LEFDE641:
.LSFDE643:
	.long	.LEFDE643-.LASFDE643
.LASFDE643:
	.long	.LASFDE643-.Lframe1
	.long	.LFB886
	.long	.LFE886-.LFB886
	.uleb128 0x0
	.align 8
.LEFDE643:
.LSFDE645:
	.long	.LEFDE645-.LASFDE645
.LASFDE645:
	.long	.LASFDE645-.Lframe1
	.long	.LFB887
	.long	.LFE887-.LFB887
	.uleb128 0x0
	.align 8
.LEFDE645:
.LSFDE647:
	.long	.LEFDE647-.LASFDE647
.LASFDE647:
	.long	.LASFDE647-.Lframe1
	.long	.LFB888
	.long	.LFE888-.LFB888
	.uleb128 0x0
	.align 8
.LEFDE647:
.LSFDE649:
	.long	.LEFDE649-.LASFDE649
.LASFDE649:
	.long	.LASFDE649-.Lframe1
	.long	.LFB889
	.long	.LFE889-.LFB889
	.uleb128 0x0
	.align 8
.LEFDE649:
.LSFDE651:
	.long	.LEFDE651-.LASFDE651
.LASFDE651:
	.long	.LASFDE651-.Lframe1
	.long	.LFB890
	.long	.LFE890-.LFB890
	.uleb128 0x0
	.align 8
.LEFDE651:
.LSFDE653:
	.long	.LEFDE653-.LASFDE653
.LASFDE653:
	.long	.LASFDE653-.Lframe1
	.long	.LFB891
	.long	.LFE891-.LFB891
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI5-.LFB891
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE653:
.LSFDE655:
	.long	.LEFDE655-.LASFDE655
.LASFDE655:
	.long	.LASFDE655-.Lframe1
	.long	.LFB892
	.long	.LFE892-.LFB892
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI6-.LFB892
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE655:
.LSFDE657:
	.long	.LEFDE657-.LASFDE657
.LASFDE657:
	.long	.LASFDE657-.Lframe1
	.long	.LFB893
	.long	.LFE893-.LFB893
	.uleb128 0x0
	.align 8
.LEFDE657:
.LSFDE659:
	.long	.LEFDE659-.LASFDE659
.LASFDE659:
	.long	.LASFDE659-.Lframe1
	.long	.LFB942
	.long	.LFE942-.LFB942
	.uleb128 0x0
	.align 8
.LEFDE659:
.LSFDE661:
	.long	.LEFDE661-.LASFDE661
.LASFDE661:
	.long	.LASFDE661-.Lframe1
	.long	.LFB943
	.long	.LFE943-.LFB943
	.uleb128 0x0
	.align 8
.LEFDE661:
.LSFDE663:
	.long	.LEFDE663-.LASFDE663
.LASFDE663:
	.long	.LASFDE663-.Lframe1
	.long	.LFB944
	.long	.LFE944-.LFB944
	.uleb128 0x0
	.align 8
.LEFDE663:
.LSFDE665:
	.long	.LEFDE665-.LASFDE665
.LASFDE665:
	.long	.LASFDE665-.Lframe1
	.long	.LFB945
	.long	.LFE945-.LFB945
	.uleb128 0x0
	.align 8
.LEFDE665:
.LSFDE667:
	.long	.LEFDE667-.LASFDE667
.LASFDE667:
	.long	.LASFDE667-.Lframe1
	.long	.LFB946
	.long	.LFE946-.LFB946
	.uleb128 0x0
	.align 8
.LEFDE667:
.LSFDE669:
	.long	.LEFDE669-.LASFDE669
.LASFDE669:
	.long	.LASFDE669-.Lframe1
	.long	.LFB947
	.long	.LFE947-.LFB947
	.uleb128 0x0
	.align 8
.LEFDE669:
.LSFDE671:
	.long	.LEFDE671-.LASFDE671
.LASFDE671:
	.long	.LASFDE671-.Lframe1
	.long	.LFB948
	.long	.LFE948-.LFB948
	.uleb128 0x0
	.align 8
.LEFDE671:
.LSFDE673:
	.long	.LEFDE673-.LASFDE673
.LASFDE673:
	.long	.LASFDE673-.Lframe1
	.long	.LFB949
	.long	.LFE949-.LFB949
	.uleb128 0x0
	.align 8
.LEFDE673:
.LSFDE675:
	.long	.LEFDE675-.LASFDE675
.LASFDE675:
	.long	.LASFDE675-.Lframe1
	.long	.LFB950
	.long	.LFE950-.LFB950
	.uleb128 0x0
	.align 8
.LEFDE675:
.LSFDE677:
	.long	.LEFDE677-.LASFDE677
.LASFDE677:
	.long	.LASFDE677-.Lframe1
	.long	.LFB957
	.long	.LFE957-.LFB957
	.uleb128 0x0
	.align 8
.LEFDE677:
.LSFDE679:
	.long	.LEFDE679-.LASFDE679
.LASFDE679:
	.long	.LASFDE679-.Lframe1
	.long	.LFB958
	.long	.LFE958-.LFB958
	.uleb128 0x0
	.align 8
.LEFDE679:
.LSFDE681:
	.long	.LEFDE681-.LASFDE681
.LASFDE681:
	.long	.LASFDE681-.Lframe1
	.long	.LFB959
	.long	.LFE959-.LFB959
	.uleb128 0x0
	.align 8
.LEFDE681:
.LSFDE683:
	.long	.LEFDE683-.LASFDE683
.LASFDE683:
	.long	.LASFDE683-.Lframe1
	.long	.LFB960
	.long	.LFE960-.LFB960
	.uleb128 0x0
	.align 8
.LEFDE683:
.LSFDE685:
	.long	.LEFDE685-.LASFDE685
.LASFDE685:
	.long	.LASFDE685-.Lframe1
	.long	.LFB961
	.long	.LFE961-.LFB961
	.uleb128 0x0
	.align 8
.LEFDE685:
.LSFDE687:
	.long	.LEFDE687-.LASFDE687
.LASFDE687:
	.long	.LASFDE687-.Lframe1
	.long	.LFB962
	.long	.LFE962-.LFB962
	.uleb128 0x0
	.align 8
.LEFDE687:
.LSFDE689:
	.long	.LEFDE689-.LASFDE689
.LASFDE689:
	.long	.LASFDE689-.Lframe1
	.long	.LFB963
	.long	.LFE963-.LFB963
	.uleb128 0x0
	.align 8
.LEFDE689:
.LSFDE691:
	.long	.LEFDE691-.LASFDE691
.LASFDE691:
	.long	.LASFDE691-.Lframe1
	.long	.LFB964
	.long	.LFE964-.LFB964
	.uleb128 0x0
	.align 8
.LEFDE691:
.LSFDE693:
	.long	.LEFDE693-.LASFDE693
.LASFDE693:
	.long	.LASFDE693-.Lframe1
	.long	.LFB965
	.long	.LFE965-.LFB965
	.uleb128 0x0
	.align 8
.LEFDE693:
.LSFDE695:
	.long	.LEFDE695-.LASFDE695
.LASFDE695:
	.long	.LASFDE695-.Lframe1
	.long	.LFB966
	.long	.LFE966-.LFB966
	.uleb128 0x0
	.align 8
.LEFDE695:
.LSFDE697:
	.long	.LEFDE697-.LASFDE697
.LASFDE697:
	.long	.LASFDE697-.Lframe1
	.long	.LFB967
	.long	.LFE967-.LFB967
	.uleb128 0x0
	.align 8
.LEFDE697:
.LSFDE699:
	.long	.LEFDE699-.LASFDE699
.LASFDE699:
	.long	.LASFDE699-.Lframe1
	.long	.LFB968
	.long	.LFE968-.LFB968
	.uleb128 0x0
	.align 8
.LEFDE699:
.LSFDE701:
	.long	.LEFDE701-.LASFDE701
.LASFDE701:
	.long	.LASFDE701-.Lframe1
	.long	.LFB969
	.long	.LFE969-.LFB969
	.uleb128 0x0
	.align 8
.LEFDE701:
.LSFDE703:
	.long	.LEFDE703-.LASFDE703
.LASFDE703:
	.long	.LASFDE703-.Lframe1
	.long	.LFB970
	.long	.LFE970-.LFB970
	.uleb128 0x0
	.align 8
.LEFDE703:
.LSFDE705:
	.long	.LEFDE705-.LASFDE705
.LASFDE705:
	.long	.LASFDE705-.Lframe1
	.long	.LFB971
	.long	.LFE971-.LFB971
	.uleb128 0x0
	.align 8
.LEFDE705:
.LSFDE707:
	.long	.LEFDE707-.LASFDE707
.LASFDE707:
	.long	.LASFDE707-.Lframe1
	.long	.LFB972
	.long	.LFE972-.LFB972
	.uleb128 0x0
	.align 8
.LEFDE707:
.LSFDE709:
	.long	.LEFDE709-.LASFDE709
.LASFDE709:
	.long	.LASFDE709-.Lframe1
	.long	.LFB973
	.long	.LFE973-.LFB973
	.uleb128 0x0
	.align 8
.LEFDE709:
.LSFDE711:
	.long	.LEFDE711-.LASFDE711
.LASFDE711:
	.long	.LASFDE711-.Lframe1
	.long	.LFB974
	.long	.LFE974-.LFB974
	.uleb128 0x0
	.align 8
.LEFDE711:
.LSFDE713:
	.long	.LEFDE713-.LASFDE713
.LASFDE713:
	.long	.LASFDE713-.Lframe1
	.long	.LFB975
	.long	.LFE975-.LFB975
	.uleb128 0x0
	.align 8
.LEFDE713:
.LSFDE715:
	.long	.LEFDE715-.LASFDE715
.LASFDE715:
	.long	.LASFDE715-.Lframe1
	.long	.LFB976
	.long	.LFE976-.LFB976
	.uleb128 0x0
	.align 8
.LEFDE715:
.LSFDE717:
	.long	.LEFDE717-.LASFDE717
.LASFDE717:
	.long	.LASFDE717-.Lframe1
	.long	.LFB977
	.long	.LFE977-.LFB977
	.uleb128 0x0
	.align 8
.LEFDE717:
.LSFDE719:
	.long	.LEFDE719-.LASFDE719
.LASFDE719:
	.long	.LASFDE719-.Lframe1
	.long	.LFB978
	.long	.LFE978-.LFB978
	.uleb128 0x0
	.align 8
.LEFDE719:
.LSFDE721:
	.long	.LEFDE721-.LASFDE721
.LASFDE721:
	.long	.LASFDE721-.Lframe1
	.long	.LFB979
	.long	.LFE979-.LFB979
	.uleb128 0x0
	.align 8
.LEFDE721:
.LSFDE723:
	.long	.LEFDE723-.LASFDE723
.LASFDE723:
	.long	.LASFDE723-.Lframe1
	.long	.LFB987
	.long	.LFE987-.LFB987
	.uleb128 0x0
	.align 8
.LEFDE723:
.LSFDE725:
	.long	.LEFDE725-.LASFDE725
.LASFDE725:
	.long	.LASFDE725-.Lframe1
	.long	.LFB988
	.long	.LFE988-.LFB988
	.uleb128 0x0
	.align 8
.LEFDE725:
.LSFDE727:
	.long	.LEFDE727-.LASFDE727
.LASFDE727:
	.long	.LASFDE727-.Lframe1
	.long	.LFB989
	.long	.LFE989-.LFB989
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI7-.LFB989
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE727:
.LSFDE729:
	.long	.LEFDE729-.LASFDE729
.LASFDE729:
	.long	.LASFDE729-.Lframe1
	.long	.LFB990
	.long	.LFE990-.LFB990
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI8-.LFB990
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE729:
.LSFDE731:
	.long	.LEFDE731-.LASFDE731
.LASFDE731:
	.long	.LASFDE731-.Lframe1
	.long	.LFB991
	.long	.LFE991-.LFB991
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI9-.LFB991
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE731:
.LSFDE733:
	.long	.LEFDE733-.LASFDE733
.LASFDE733:
	.long	.LASFDE733-.Lframe1
	.long	.LFB992
	.long	.LFE992-.LFB992
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI10-.LFB992
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE733:
.LSFDE735:
	.long	.LEFDE735-.LASFDE735
.LASFDE735:
	.long	.LASFDE735-.Lframe1
	.long	.LFB994
	.long	.LFE994-.LFB994
	.uleb128 0x0
	.align 8
.LEFDE735:
.LSFDE737:
	.long	.LEFDE737-.LASFDE737
.LASFDE737:
	.long	.LASFDE737-.Lframe1
	.long	.LFB995
	.long	.LFE995-.LFB995
	.uleb128 0x0
	.align 8
.LEFDE737:
.LSFDE739:
	.long	.LEFDE739-.LASFDE739
.LASFDE739:
	.long	.LASFDE739-.Lframe1
	.long	.LFB1000
	.long	.LFE1000-.LFB1000
	.uleb128 0x0
	.align 8
.LEFDE739:
.LSFDE741:
	.long	.LEFDE741-.LASFDE741
.LASFDE741:
	.long	.LASFDE741-.Lframe1
	.long	.LFB1001
	.long	.LFE1001-.LFB1001
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI11-.LFB1001
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE741:
.LSFDE743:
	.long	.LEFDE743-.LASFDE743
.LASFDE743:
	.long	.LASFDE743-.Lframe1
	.long	.LFB1015
	.long	.LFE1015-.LFB1015
	.uleb128 0x0
	.align 8
.LEFDE743:
.LSFDE745:
	.long	.LEFDE745-.LASFDE745
.LASFDE745:
	.long	.LASFDE745-.Lframe1
	.long	.LFB1016
	.long	.LFE1016-.LFB1016
	.uleb128 0x0
	.align 8
.LEFDE745:
.LSFDE747:
	.long	.LEFDE747-.LASFDE747
.LASFDE747:
	.long	.LASFDE747-.Lframe1
	.long	.LFB1017
	.long	.LFE1017-.LFB1017
	.uleb128 0x0
	.align 8
.LEFDE747:
.LSFDE749:
	.long	.LEFDE749-.LASFDE749
.LASFDE749:
	.long	.LASFDE749-.Lframe1
	.long	.LFB1018
	.long	.LFE1018-.LFB1018
	.uleb128 0x0
	.align 8
.LEFDE749:
.LSFDE751:
	.long	.LEFDE751-.LASFDE751
.LASFDE751:
	.long	.LASFDE751-.Lframe1
	.long	.LFB1019
	.long	.LFE1019-.LFB1019
	.uleb128 0x0
	.align 8
.LEFDE751:
.LSFDE753:
	.long	.LEFDE753-.LASFDE753
.LASFDE753:
	.long	.LASFDE753-.Lframe1
	.long	.LFB1020
	.long	.LFE1020-.LFB1020
	.uleb128 0x0
	.align 8
.LEFDE753:
.LSFDE755:
	.long	.LEFDE755-.LASFDE755
.LASFDE755:
	.long	.LASFDE755-.Lframe1
	.long	.LFB1021
	.long	.LFE1021-.LFB1021
	.uleb128 0x0
	.align 8
.LEFDE755:
.LSFDE757:
	.long	.LEFDE757-.LASFDE757
.LASFDE757:
	.long	.LASFDE757-.Lframe1
	.long	.LFB1022
	.long	.LFE1022-.LFB1022
	.uleb128 0x0
	.align 8
.LEFDE757:
.LSFDE759:
	.long	.LEFDE759-.LASFDE759
.LASFDE759:
	.long	.LASFDE759-.Lframe1
	.long	.LFB1023
	.long	.LFE1023-.LFB1023
	.uleb128 0x0
	.align 8
.LEFDE759:
.LSFDE761:
	.long	.LEFDE761-.LASFDE761
.LASFDE761:
	.long	.LASFDE761-.Lframe1
	.long	.LFB1024
	.long	.LFE1024-.LFB1024
	.uleb128 0x0
	.align 8
.LEFDE761:
.LSFDE763:
	.long	.LEFDE763-.LASFDE763
.LASFDE763:
	.long	.LASFDE763-.Lframe1
	.long	.LFB1025
	.long	.LFE1025-.LFB1025
	.uleb128 0x0
	.align 8
.LEFDE763:
.LSFDE765:
	.long	.LEFDE765-.LASFDE765
.LASFDE765:
	.long	.LASFDE765-.Lframe1
	.long	.LFB1026
	.long	.LFE1026-.LFB1026
	.uleb128 0x0
	.align 8
.LEFDE765:
.LSFDE767:
	.long	.LEFDE767-.LASFDE767
.LASFDE767:
	.long	.LASFDE767-.Lframe1
	.long	.LFB1027
	.long	.LFE1027-.LFB1027
	.uleb128 0x0
	.align 8
.LEFDE767:
.LSFDE769:
	.long	.LEFDE769-.LASFDE769
.LASFDE769:
	.long	.LASFDE769-.Lframe1
	.long	.LFB1028
	.long	.LFE1028-.LFB1028
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI12-.LFB1028
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE769:
.LSFDE771:
	.long	.LEFDE771-.LASFDE771
.LASFDE771:
	.long	.LASFDE771-.Lframe1
	.long	.LFB1029
	.long	.LFE1029-.LFB1029
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI13-.LFB1029
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE771:
.LSFDE773:
	.long	.LEFDE773-.LASFDE773
.LASFDE773:
	.long	.LASFDE773-.Lframe1
	.long	.LFB1030
	.long	.LFE1030-.LFB1030
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI14-.LFB1030
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE773:
.LSFDE775:
	.long	.LEFDE775-.LASFDE775
.LASFDE775:
	.long	.LASFDE775-.Lframe1
	.long	.LFB1031
	.long	.LFE1031-.LFB1031
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI15-.LFB1031
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE775:
.LSFDE777:
	.long	.LEFDE777-.LASFDE777
.LASFDE777:
	.long	.LASFDE777-.Lframe1
	.long	.LFB1032
	.long	.LFE1032-.LFB1032
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI16-.LFB1032
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE777:
.LSFDE779:
	.long	.LEFDE779-.LASFDE779
.LASFDE779:
	.long	.LASFDE779-.Lframe1
	.long	.LFB1033
	.long	.LFE1033-.LFB1033
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI17-.LFB1033
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE779:
.LSFDE781:
	.long	.LEFDE781-.LASFDE781
.LASFDE781:
	.long	.LASFDE781-.Lframe1
	.long	.LFB1034
	.long	.LFE1034-.LFB1034
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI18-.LFB1034
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE781:
.LSFDE783:
	.long	.LEFDE783-.LASFDE783
.LASFDE783:
	.long	.LASFDE783-.Lframe1
	.long	.LFB1035
	.long	.LFE1035-.LFB1035
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI19-.LFB1035
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE783:
.LSFDE785:
	.long	.LEFDE785-.LASFDE785
.LASFDE785:
	.long	.LASFDE785-.Lframe1
	.long	.LFB1036
	.long	.LFE1036-.LFB1036
	.uleb128 0x0
	.align 8
.LEFDE785:
.LSFDE787:
	.long	.LEFDE787-.LASFDE787
.LASFDE787:
	.long	.LASFDE787-.Lframe1
	.long	.LFB1037
	.long	.LFE1037-.LFB1037
	.uleb128 0x0
	.align 8
.LEFDE787:
.LSFDE789:
	.long	.LEFDE789-.LASFDE789
.LASFDE789:
	.long	.LASFDE789-.Lframe1
	.long	.LFB1038
	.long	.LFE1038-.LFB1038
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI20-.LFB1038
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE789:
.LSFDE791:
	.long	.LEFDE791-.LASFDE791
.LASFDE791:
	.long	.LASFDE791-.Lframe1
	.long	.LFB1041
	.long	.LFE1041-.LFB1041
	.uleb128 0x0
	.align 8
.LEFDE791:
.LSFDE793:
	.long	.LEFDE793-.LASFDE793
.LASFDE793:
	.long	.LASFDE793-.Lframe1
	.long	.LFB1045
	.long	.LFE1045-.LFB1045
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI21-.LFB1045
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE793:
.LSFDE795:
	.long	.LEFDE795-.LASFDE795
.LASFDE795:
	.long	.LASFDE795-.Lframe1
	.long	.LFB1046
	.long	.LFE1046-.LFB1046
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI22-.LFB1046
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE795:
.LSFDE797:
	.long	.LEFDE797-.LASFDE797
.LASFDE797:
	.long	.LASFDE797-.Lframe1
	.long	.LFB1047
	.long	.LFE1047-.LFB1047
	.uleb128 0x0
	.align 8
.LEFDE797:
.LSFDE799:
	.long	.LEFDE799-.LASFDE799
.LASFDE799:
	.long	.LASFDE799-.Lframe1
	.long	.LFB1048
	.long	.LFE1048-.LFB1048
	.uleb128 0x0
	.align 8
.LEFDE799:
.LSFDE801:
	.long	.LEFDE801-.LASFDE801
.LASFDE801:
	.long	.LASFDE801-.Lframe1
	.long	.LFB1049
	.long	.LFE1049-.LFB1049
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI23-.LFB1049
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE801:
.LSFDE803:
	.long	.LEFDE803-.LASFDE803
.LASFDE803:
	.long	.LASFDE803-.Lframe1
	.long	.LFB1050
	.long	.LFE1050-.LFB1050
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI24-.LFB1050
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE803:
.LSFDE805:
	.long	.LEFDE805-.LASFDE805
.LASFDE805:
	.long	.LASFDE805-.Lframe1
	.long	.LFB1051
	.long	.LFE1051-.LFB1051
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI25-.LFB1051
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE805:
.LSFDE807:
	.long	.LEFDE807-.LASFDE807
.LASFDE807:
	.long	.LASFDE807-.Lframe1
	.long	.LFB1052
	.long	.LFE1052-.LFB1052
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI26-.LFB1052
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE807:
.LSFDE809:
	.long	.LEFDE809-.LASFDE809
.LASFDE809:
	.long	.LASFDE809-.Lframe1
	.long	.LFB1053
	.long	.LFE1053-.LFB1053
	.uleb128 0x0
	.align 8
.LEFDE809:
.LSFDE811:
	.long	.LEFDE811-.LASFDE811
.LASFDE811:
	.long	.LASFDE811-.Lframe1
	.long	.LFB1054
	.long	.LFE1054-.LFB1054
	.uleb128 0x0
	.align 8
.LEFDE811:
.LSFDE813:
	.long	.LEFDE813-.LASFDE813
.LASFDE813:
	.long	.LASFDE813-.Lframe1
	.long	.LFB1077
	.long	.LFE1077-.LFB1077
	.uleb128 0x0
	.align 8
.LEFDE813:
.LSFDE815:
	.long	.LEFDE815-.LASFDE815
.LASFDE815:
	.long	.LASFDE815-.Lframe1
	.long	.LFB1078
	.long	.LFE1078-.LFB1078
	.uleb128 0x0
	.align 8
.LEFDE815:
.LSFDE817:
	.long	.LEFDE817-.LASFDE817
.LASFDE817:
	.long	.LASFDE817-.Lframe1
	.long	.LFB1079
	.long	.LFE1079-.LFB1079
	.uleb128 0x0
	.align 8
.LEFDE817:
.LSFDE819:
	.long	.LEFDE819-.LASFDE819
.LASFDE819:
	.long	.LASFDE819-.Lframe1
	.long	.LFB1080
	.long	.LFE1080-.LFB1080
	.uleb128 0x0
	.align 8
.LEFDE819:
.LSFDE821:
	.long	.LEFDE821-.LASFDE821
.LASFDE821:
	.long	.LASFDE821-.Lframe1
	.long	.LFB1081
	.long	.LFE1081-.LFB1081
	.uleb128 0x0
	.align 8
.LEFDE821:
.LSFDE823:
	.long	.LEFDE823-.LASFDE823
.LASFDE823:
	.long	.LASFDE823-.Lframe1
	.long	.LFB1082
	.long	.LFE1082-.LFB1082
	.uleb128 0x0
	.align 8
.LEFDE823:
.LSFDE825:
	.long	.LEFDE825-.LASFDE825
.LASFDE825:
	.long	.LASFDE825-.Lframe1
	.long	.LFB1083
	.long	.LFE1083-.LFB1083
	.uleb128 0x0
	.align 8
.LEFDE825:
.LSFDE827:
	.long	.LEFDE827-.LASFDE827
.LASFDE827:
	.long	.LASFDE827-.Lframe1
	.long	.LFB1084
	.long	.LFE1084-.LFB1084
	.uleb128 0x0
	.align 8
.LEFDE827:
.LSFDE829:
	.long	.LEFDE829-.LASFDE829
.LASFDE829:
	.long	.LASFDE829-.Lframe1
	.long	.LFB1085
	.long	.LFE1085-.LFB1085
	.uleb128 0x0
	.align 8
.LEFDE829:
.LSFDE831:
	.long	.LEFDE831-.LASFDE831
.LASFDE831:
	.long	.LASFDE831-.Lframe1
	.long	.LFB1086
	.long	.LFE1086-.LFB1086
	.uleb128 0x0
	.align 8
.LEFDE831:
.LSFDE833:
	.long	.LEFDE833-.LASFDE833
.LASFDE833:
	.long	.LASFDE833-.Lframe1
	.long	.LFB1091
	.long	.LFE1091-.LFB1091
	.uleb128 0x0
	.align 8
.LEFDE833:
.LSFDE835:
	.long	.LEFDE835-.LASFDE835
.LASFDE835:
	.long	.LASFDE835-.Lframe1
	.long	.LFB1092
	.long	.LFE1092-.LFB1092
	.uleb128 0x0
	.align 8
.LEFDE835:
.LSFDE837:
	.long	.LEFDE837-.LASFDE837
.LASFDE837:
	.long	.LASFDE837-.Lframe1
	.long	.LFB1093
	.long	.LFE1093-.LFB1093
	.uleb128 0x0
	.align 8
.LEFDE837:
.LSFDE839:
	.long	.LEFDE839-.LASFDE839
.LASFDE839:
	.long	.LASFDE839-.Lframe1
	.long	.LFB1094
	.long	.LFE1094-.LFB1094
	.uleb128 0x0
	.align 8
.LEFDE839:
.LSFDE841:
	.long	.LEFDE841-.LASFDE841
.LASFDE841:
	.long	.LASFDE841-.Lframe1
	.long	.LFB1095
	.long	.LFE1095-.LFB1095
	.uleb128 0x0
	.align 8
.LEFDE841:
.LSFDE843:
	.long	.LEFDE843-.LASFDE843
.LASFDE843:
	.long	.LASFDE843-.Lframe1
	.long	.LFB1098
	.long	.LFE1098-.LFB1098
	.uleb128 0x0
	.align 8
.LEFDE843:
.LSFDE845:
	.long	.LEFDE845-.LASFDE845
.LASFDE845:
	.long	.LASFDE845-.Lframe1
	.long	.LFB1099
	.long	.LFE1099-.LFB1099
	.uleb128 0x0
	.align 8
.LEFDE845:
.LSFDE847:
	.long	.LEFDE847-.LASFDE847
.LASFDE847:
	.long	.LASFDE847-.Lframe1
	.long	.LFB1100
	.long	.LFE1100-.LFB1100
	.uleb128 0x0
	.align 8
.LEFDE847:
.LSFDE849:
	.long	.LEFDE849-.LASFDE849
.LASFDE849:
	.long	.LASFDE849-.Lframe1
	.long	.LFB1101
	.long	.LFE1101-.LFB1101
	.uleb128 0x0
	.align 8
.LEFDE849:
.LSFDE851:
	.long	.LEFDE851-.LASFDE851
.LASFDE851:
	.long	.LASFDE851-.Lframe1
	.long	.LFB1104
	.long	.LFE1104-.LFB1104
	.uleb128 0x0
	.align 8
.LEFDE851:
.LSFDE853:
	.long	.LEFDE853-.LASFDE853
.LASFDE853:
	.long	.LASFDE853-.Lframe1
	.long	.LFB1107
	.long	.LFE1107-.LFB1107
	.uleb128 0x0
	.align 8
.LEFDE853:
.LSFDE855:
	.long	.LEFDE855-.LASFDE855
.LASFDE855:
	.long	.LASFDE855-.Lframe1
	.long	.LFB1108
	.long	.LFE1108-.LFB1108
	.uleb128 0x0
	.align 8
.LEFDE855:
.LSFDE857:
	.long	.LEFDE857-.LASFDE857
.LASFDE857:
	.long	.LASFDE857-.Lframe1
	.long	.LFB1109
	.long	.LFE1109-.LFB1109
	.uleb128 0x0
	.align 8
.LEFDE857:
.LSFDE859:
	.long	.LEFDE859-.LASFDE859
.LASFDE859:
	.long	.LASFDE859-.Lframe1
	.long	.LFB1110
	.long	.LFE1110-.LFB1110
	.uleb128 0x0
	.align 8
.LEFDE859:
.LSFDE861:
	.long	.LEFDE861-.LASFDE861
.LASFDE861:
	.long	.LASFDE861-.Lframe1
	.long	.LFB1111
	.long	.LFE1111-.LFB1111
	.uleb128 0x0
	.align 8
.LEFDE861:
.LSFDE863:
	.long	.LEFDE863-.LASFDE863
.LASFDE863:
	.long	.LASFDE863-.Lframe1
	.long	.LFB1112
	.long	.LFE1112-.LFB1112
	.uleb128 0x0
	.align 8
.LEFDE863:
.LSFDE865:
	.long	.LEFDE865-.LASFDE865
.LASFDE865:
	.long	.LASFDE865-.Lframe1
	.long	.LFB1113
	.long	.LFE1113-.LFB1113
	.uleb128 0x0
	.align 8
.LEFDE865:
.LSFDE867:
	.long	.LEFDE867-.LASFDE867
.LASFDE867:
	.long	.LASFDE867-.Lframe1
	.long	.LFB1114
	.long	.LFE1114-.LFB1114
	.uleb128 0x0
	.align 8
.LEFDE867:
.LSFDE869:
	.long	.LEFDE869-.LASFDE869
.LASFDE869:
	.long	.LASFDE869-.Lframe1
	.long	.LFB1130
	.long	.LFE1130-.LFB1130
	.uleb128 0x0
	.align 8
.LEFDE869:
.LSFDE871:
	.long	.LEFDE871-.LASFDE871
.LASFDE871:
	.long	.LASFDE871-.Lframe1
	.long	.LFB1133
	.long	.LFE1133-.LFB1133
	.uleb128 0x0
	.align 8
.LEFDE871:
.LSFDE873:
	.long	.LEFDE873-.LASFDE873
.LASFDE873:
	.long	.LASFDE873-.Lframe1
	.long	.LFB1135
	.long	.LFE1135-.LFB1135
	.uleb128 0x0
	.align 8
.LEFDE873:
.LSFDE875:
	.long	.LEFDE875-.LASFDE875
.LASFDE875:
	.long	.LASFDE875-.Lframe1
	.long	.LFB1143
	.long	.LFE1143-.LFB1143
	.uleb128 0x0
	.align 8
.LEFDE875:
.LSFDE877:
	.long	.LEFDE877-.LASFDE877
.LASFDE877:
	.long	.LASFDE877-.Lframe1
	.long	.LFB1144
	.long	.LFE1144-.LFB1144
	.uleb128 0x0
	.align 8
.LEFDE877:
.LSFDE879:
	.long	.LEFDE879-.LASFDE879
.LASFDE879:
	.long	.LASFDE879-.Lframe1
	.long	.LFB1145
	.long	.LFE1145-.LFB1145
	.uleb128 0x0
	.align 8
.LEFDE879:
.LSFDE881:
	.long	.LEFDE881-.LASFDE881
.LASFDE881:
	.long	.LASFDE881-.Lframe1
	.long	.LFB1146
	.long	.LFE1146-.LFB1146
	.uleb128 0x0
	.align 8
.LEFDE881:
.LSFDE883:
	.long	.LEFDE883-.LASFDE883
.LASFDE883:
	.long	.LASFDE883-.Lframe1
	.long	.LFB1149
	.long	.LFE1149-.LFB1149
	.uleb128 0x0
	.align 8
.LEFDE883:
.LSFDE885:
	.long	.LEFDE885-.LASFDE885
.LASFDE885:
	.long	.LASFDE885-.Lframe1
	.long	.LFB1150
	.long	.LFE1150-.LFB1150
	.uleb128 0x0
	.align 8
.LEFDE885:
.LSFDE887:
	.long	.LEFDE887-.LASFDE887
.LASFDE887:
	.long	.LASFDE887-.Lframe1
	.long	.LFB1151
	.long	.LFE1151-.LFB1151
	.uleb128 0x0
	.align 8
.LEFDE887:
.LSFDE889:
	.long	.LEFDE889-.LASFDE889
.LASFDE889:
	.long	.LASFDE889-.Lframe1
	.long	.LFB1152
	.long	.LFE1152-.LFB1152
	.uleb128 0x0
	.align 8
.LEFDE889:
.LSFDE891:
	.long	.LEFDE891-.LASFDE891
.LASFDE891:
	.long	.LASFDE891-.Lframe1
	.long	.LFB1153
	.long	.LFE1153-.LFB1153
	.uleb128 0x0
	.align 8
.LEFDE891:
.LSFDE893:
	.long	.LEFDE893-.LASFDE893
.LASFDE893:
	.long	.LASFDE893-.Lframe1
	.long	.LFB1154
	.long	.LFE1154-.LFB1154
	.uleb128 0x0
	.align 8
.LEFDE893:
.LSFDE895:
	.long	.LEFDE895-.LASFDE895
.LASFDE895:
	.long	.LASFDE895-.Lframe1
	.long	.LFB1155
	.long	.LFE1155-.LFB1155
	.uleb128 0x0
	.align 8
.LEFDE895:
.LSFDE897:
	.long	.LEFDE897-.LASFDE897
.LASFDE897:
	.long	.LASFDE897-.Lframe1
	.long	.LFB1156
	.long	.LFE1156-.LFB1156
	.uleb128 0x0
	.align 8
.LEFDE897:
.LSFDE899:
	.long	.LEFDE899-.LASFDE899
.LASFDE899:
	.long	.LASFDE899-.Lframe1
	.long	.LFB1157
	.long	.LFE1157-.LFB1157
	.uleb128 0x0
	.align 8
.LEFDE899:
.LSFDE901:
	.long	.LEFDE901-.LASFDE901
.LASFDE901:
	.long	.LASFDE901-.Lframe1
	.long	.LFB1158
	.long	.LFE1158-.LFB1158
	.uleb128 0x0
	.align 8
.LEFDE901:
.LSFDE903:
	.long	.LEFDE903-.LASFDE903
.LASFDE903:
	.long	.LASFDE903-.Lframe1
	.long	.LFB1163
	.long	.LFE1163-.LFB1163
	.uleb128 0x0
	.align 8
.LEFDE903:
.LSFDE905:
	.long	.LEFDE905-.LASFDE905
.LASFDE905:
	.long	.LASFDE905-.Lframe1
	.long	.LFB1164
	.long	.LFE1164-.LFB1164
	.uleb128 0x0
	.align 8
.LEFDE905:
.LSFDE907:
	.long	.LEFDE907-.LASFDE907
.LASFDE907:
	.long	.LASFDE907-.Lframe1
	.long	.LFB1165
	.long	.LFE1165-.LFB1165
	.uleb128 0x0
	.align 8
.LEFDE907:
.LSFDE909:
	.long	.LEFDE909-.LASFDE909
.LASFDE909:
	.long	.LASFDE909-.Lframe1
	.long	.LFB1166
	.long	.LFE1166-.LFB1166
	.uleb128 0x0
	.align 8
.LEFDE909:
.LSFDE911:
	.long	.LEFDE911-.LASFDE911
.LASFDE911:
	.long	.LASFDE911-.Lframe1
	.long	.LFB1167
	.long	.LFE1167-.LFB1167
	.uleb128 0x0
	.align 8
.LEFDE911:
.LSFDE913:
	.long	.LEFDE913-.LASFDE913
.LASFDE913:
	.long	.LASFDE913-.Lframe1
	.long	.LFB1168
	.long	.LFE1168-.LFB1168
	.uleb128 0x0
	.align 8
.LEFDE913:
.LSFDE915:
	.long	.LEFDE915-.LASFDE915
.LASFDE915:
	.long	.LASFDE915-.Lframe1
	.long	.LFB1169
	.long	.LFE1169-.LFB1169
	.uleb128 0x0
	.align 8
.LEFDE915:
.LSFDE917:
	.long	.LEFDE917-.LASFDE917
.LASFDE917:
	.long	.LASFDE917-.Lframe1
	.long	.LFB1170
	.long	.LFE1170-.LFB1170
	.uleb128 0x0
	.align 8
.LEFDE917:
.LSFDE919:
	.long	.LEFDE919-.LASFDE919
.LASFDE919:
	.long	.LASFDE919-.Lframe1
	.long	.LFB1179
	.long	.LFE1179-.LFB1179
	.uleb128 0x0
	.align 8
.LEFDE919:
.LSFDE921:
	.long	.LEFDE921-.LASFDE921
.LASFDE921:
	.long	.LASFDE921-.Lframe1
	.long	.LFB1180
	.long	.LFE1180-.LFB1180
	.uleb128 0x0
	.align 8
.LEFDE921:
.LSFDE923:
	.long	.LEFDE923-.LASFDE923
.LASFDE923:
	.long	.LASFDE923-.Lframe1
	.long	.LFB1181
	.long	.LFE1181-.LFB1181
	.uleb128 0x0
	.align 8
.LEFDE923:
.LSFDE925:
	.long	.LEFDE925-.LASFDE925
.LASFDE925:
	.long	.LASFDE925-.Lframe1
	.long	.LFB1182
	.long	.LFE1182-.LFB1182
	.uleb128 0x0
	.align 8
.LEFDE925:
.LSFDE927:
	.long	.LEFDE927-.LASFDE927
.LASFDE927:
	.long	.LASFDE927-.Lframe1
	.long	.LFB1183
	.long	.LFE1183-.LFB1183
	.uleb128 0x0
	.align 8
.LEFDE927:
.LSFDE929:
	.long	.LEFDE929-.LASFDE929
.LASFDE929:
	.long	.LASFDE929-.Lframe1
	.long	.LFB1184
	.long	.LFE1184-.LFB1184
	.uleb128 0x0
	.align 8
.LEFDE929:
.LSFDE931:
	.long	.LEFDE931-.LASFDE931
.LASFDE931:
	.long	.LASFDE931-.Lframe1
	.long	.LFB1185
	.long	.LFE1185-.LFB1185
	.uleb128 0x0
	.align 8
.LEFDE931:
.LSFDE933:
	.long	.LEFDE933-.LASFDE933
.LASFDE933:
	.long	.LASFDE933-.Lframe1
	.long	.LFB1186
	.long	.LFE1186-.LFB1186
	.uleb128 0x0
	.align 8
.LEFDE933:
.LSFDE935:
	.long	.LEFDE935-.LASFDE935
.LASFDE935:
	.long	.LASFDE935-.Lframe1
	.long	.LFB1187
	.long	.LFE1187-.LFB1187
	.uleb128 0x0
	.align 8
.LEFDE935:
.LSFDE937:
	.long	.LEFDE937-.LASFDE937
.LASFDE937:
	.long	.LASFDE937-.Lframe1
	.long	.LFB1188
	.long	.LFE1188-.LFB1188
	.uleb128 0x0
	.align 8
.LEFDE937:
.LSFDE939:
	.long	.LEFDE939-.LASFDE939
.LASFDE939:
	.long	.LASFDE939-.Lframe1
	.long	.LFB1189
	.long	.LFE1189-.LFB1189
	.uleb128 0x0
	.align 8
.LEFDE939:
.LSFDE941:
	.long	.LEFDE941-.LASFDE941
.LASFDE941:
	.long	.LASFDE941-.Lframe1
	.long	.LFB1190
	.long	.LFE1190-.LFB1190
	.uleb128 0x0
	.align 8
.LEFDE941:
.LSFDE943:
	.long	.LEFDE943-.LASFDE943
.LASFDE943:
	.long	.LASFDE943-.Lframe1
	.long	.LFB1191
	.long	.LFE1191-.LFB1191
	.uleb128 0x0
	.align 8
.LEFDE943:
.LSFDE945:
	.long	.LEFDE945-.LASFDE945
.LASFDE945:
	.long	.LASFDE945-.Lframe1
	.long	.LFB1192
	.long	.LFE1192-.LFB1192
	.uleb128 0x0
	.align 8
.LEFDE945:
.LSFDE947:
	.long	.LEFDE947-.LASFDE947
.LASFDE947:
	.long	.LASFDE947-.Lframe1
	.long	.LFB1193
	.long	.LFE1193-.LFB1193
	.uleb128 0x0
	.align 8
.LEFDE947:
.LSFDE949:
	.long	.LEFDE949-.LASFDE949
.LASFDE949:
	.long	.LASFDE949-.Lframe1
	.long	.LFB1194
	.long	.LFE1194-.LFB1194
	.uleb128 0x0
	.align 8
.LEFDE949:
.LSFDE951:
	.long	.LEFDE951-.LASFDE951
.LASFDE951:
	.long	.LASFDE951-.Lframe1
	.long	.LFB1195
	.long	.LFE1195-.LFB1195
	.uleb128 0x0
	.align 8
.LEFDE951:
.LSFDE953:
	.long	.LEFDE953-.LASFDE953
.LASFDE953:
	.long	.LASFDE953-.Lframe1
	.long	.LFB1196
	.long	.LFE1196-.LFB1196
	.uleb128 0x0
	.align 8
.LEFDE953:
.LSFDE955:
	.long	.LEFDE955-.LASFDE955
.LASFDE955:
	.long	.LASFDE955-.Lframe1
	.long	.LFB1197
	.long	.LFE1197-.LFB1197
	.uleb128 0x0
	.align 8
.LEFDE955:
.LSFDE957:
	.long	.LEFDE957-.LASFDE957
.LASFDE957:
	.long	.LASFDE957-.Lframe1
	.long	.LFB1198
	.long	.LFE1198-.LFB1198
	.uleb128 0x0
	.align 8
.LEFDE957:
.LSFDE959:
	.long	.LEFDE959-.LASFDE959
.LASFDE959:
	.long	.LASFDE959-.Lframe1
	.long	.LFB1199
	.long	.LFE1199-.LFB1199
	.uleb128 0x0
	.align 8
.LEFDE959:
.LSFDE961:
	.long	.LEFDE961-.LASFDE961
.LASFDE961:
	.long	.LASFDE961-.Lframe1
	.long	.LFB1203
	.long	.LFE1203-.LFB1203
	.uleb128 0x0
	.align 8
.LEFDE961:
.LSFDE963:
	.long	.LEFDE963-.LASFDE963
.LASFDE963:
	.long	.LASFDE963-.Lframe1
	.long	.LFB1204
	.long	.LFE1204-.LFB1204
	.uleb128 0x0
	.align 8
.LEFDE963:
.LSFDE965:
	.long	.LEFDE965-.LASFDE965
.LASFDE965:
	.long	.LASFDE965-.Lframe1
	.long	.LFB1205
	.long	.LFE1205-.LFB1205
	.uleb128 0x0
	.align 8
.LEFDE965:
.LSFDE967:
	.long	.LEFDE967-.LASFDE967
.LASFDE967:
	.long	.LASFDE967-.Lframe1
	.long	.LFB1206
	.long	.LFE1206-.LFB1206
	.uleb128 0x0
	.align 8
.LEFDE967:
.LSFDE969:
	.long	.LEFDE969-.LASFDE969
.LASFDE969:
	.long	.LASFDE969-.Lframe1
	.long	.LFB1207
	.long	.LFE1207-.LFB1207
	.uleb128 0x0
	.align 8
.LEFDE969:
.LSFDE971:
	.long	.LEFDE971-.LASFDE971
.LASFDE971:
	.long	.LASFDE971-.Lframe1
	.long	.LFB1208
	.long	.LFE1208-.LFB1208
	.uleb128 0x0
	.align 8
.LEFDE971:
.LSFDE973:
	.long	.LEFDE973-.LASFDE973
.LASFDE973:
	.long	.LASFDE973-.Lframe1
	.long	.LFB1212
	.long	.LFE1212-.LFB1212
	.uleb128 0x0
	.align 8
.LEFDE973:
.LSFDE975:
	.long	.LEFDE975-.LASFDE975
.LASFDE975:
	.long	.LASFDE975-.Lframe1
	.long	.LFB1213
	.long	.LFE1213-.LFB1213
	.uleb128 0x0
	.align 8
.LEFDE975:
.LSFDE977:
	.long	.LEFDE977-.LASFDE977
.LASFDE977:
	.long	.LASFDE977-.Lframe1
	.long	.LFB1214
	.long	.LFE1214-.LFB1214
	.uleb128 0x0
	.align 8
.LEFDE977:
.LSFDE979:
	.long	.LEFDE979-.LASFDE979
.LASFDE979:
	.long	.LASFDE979-.Lframe1
	.long	.LFB1215
	.long	.LFE1215-.LFB1215
	.uleb128 0x0
	.align 8
.LEFDE979:
.LSFDE981:
	.long	.LEFDE981-.LASFDE981
.LASFDE981:
	.long	.LASFDE981-.Lframe1
	.long	.LFB1216
	.long	.LFE1216-.LFB1216
	.uleb128 0x0
	.align 8
.LEFDE981:
.LSFDE983:
	.long	.LEFDE983-.LASFDE983
.LASFDE983:
	.long	.LASFDE983-.Lframe1
	.long	.LFB1217
	.long	.LFE1217-.LFB1217
	.uleb128 0x0
	.align 8
.LEFDE983:
.LSFDE985:
	.long	.LEFDE985-.LASFDE985
.LASFDE985:
	.long	.LASFDE985-.Lframe1
	.long	.LFB1218
	.long	.LFE1218-.LFB1218
	.uleb128 0x0
	.align 8
.LEFDE985:
.LSFDE987:
	.long	.LEFDE987-.LASFDE987
.LASFDE987:
	.long	.LASFDE987-.Lframe1
	.long	.LFB1219
	.long	.LFE1219-.LFB1219
	.uleb128 0x0
	.align 8
.LEFDE987:
.LSFDE989:
	.long	.LEFDE989-.LASFDE989
.LASFDE989:
	.long	.LASFDE989-.Lframe1
	.long	.LFB1220
	.long	.LFE1220-.LFB1220
	.uleb128 0x0
	.align 8
.LEFDE989:
.LSFDE991:
	.long	.LEFDE991-.LASFDE991
.LASFDE991:
	.long	.LASFDE991-.Lframe1
	.long	.LFB1221
	.long	.LFE1221-.LFB1221
	.uleb128 0x0
	.align 8
.LEFDE991:
.LSFDE993:
	.long	.LEFDE993-.LASFDE993
.LASFDE993:
	.long	.LASFDE993-.Lframe1
	.long	.LFB1222
	.long	.LFE1222-.LFB1222
	.uleb128 0x0
	.align 8
.LEFDE993:
.LSFDE995:
	.long	.LEFDE995-.LASFDE995
.LASFDE995:
	.long	.LASFDE995-.Lframe1
	.long	.LFB1223
	.long	.LFE1223-.LFB1223
	.uleb128 0x0
	.align 8
.LEFDE995:
.LSFDE997:
	.long	.LEFDE997-.LASFDE997
.LASFDE997:
	.long	.LASFDE997-.Lframe1
	.long	.LFB1224
	.long	.LFE1224-.LFB1224
	.uleb128 0x0
	.align 8
.LEFDE997:
.LSFDE999:
	.long	.LEFDE999-.LASFDE999
.LASFDE999:
	.long	.LASFDE999-.Lframe1
	.long	.LFB1225
	.long	.LFE1225-.LFB1225
	.uleb128 0x0
	.align 8
.LEFDE999:
.LSFDE1001:
	.long	.LEFDE1001-.LASFDE1001
.LASFDE1001:
	.long	.LASFDE1001-.Lframe1
	.long	.LFB1226
	.long	.LFE1226-.LFB1226
	.uleb128 0x0
	.align 8
.LEFDE1001:
.LSFDE1003:
	.long	.LEFDE1003-.LASFDE1003
.LASFDE1003:
	.long	.LASFDE1003-.Lframe1
	.long	.LFB1227
	.long	.LFE1227-.LFB1227
	.uleb128 0x0
	.align 8
.LEFDE1003:
.LSFDE1005:
	.long	.LEFDE1005-.LASFDE1005
.LASFDE1005:
	.long	.LASFDE1005-.Lframe1
	.long	.LFB1228
	.long	.LFE1228-.LFB1228
	.uleb128 0x0
	.align 8
.LEFDE1005:
.LSFDE1007:
	.long	.LEFDE1007-.LASFDE1007
.LASFDE1007:
	.long	.LASFDE1007-.Lframe1
	.long	.LFB1229
	.long	.LFE1229-.LFB1229
	.uleb128 0x0
	.align 8
.LEFDE1007:
.LSFDE1009:
	.long	.LEFDE1009-.LASFDE1009
.LASFDE1009:
	.long	.LASFDE1009-.Lframe1
	.long	.LFB1230
	.long	.LFE1230-.LFB1230
	.uleb128 0x0
	.align 8
.LEFDE1009:
.LSFDE1011:
	.long	.LEFDE1011-.LASFDE1011
.LASFDE1011:
	.long	.LASFDE1011-.Lframe1
	.long	.LFB1231
	.long	.LFE1231-.LFB1231
	.uleb128 0x0
	.align 8
.LEFDE1011:
.LSFDE1013:
	.long	.LEFDE1013-.LASFDE1013
.LASFDE1013:
	.long	.LASFDE1013-.Lframe1
	.long	.LFB1232
	.long	.LFE1232-.LFB1232
	.uleb128 0x0
	.align 8
.LEFDE1013:
.LSFDE1015:
	.long	.LEFDE1015-.LASFDE1015
.LASFDE1015:
	.long	.LASFDE1015-.Lframe1
	.long	.LFB1233
	.long	.LFE1233-.LFB1233
	.uleb128 0x0
	.align 8
.LEFDE1015:
.LSFDE1017:
	.long	.LEFDE1017-.LASFDE1017
.LASFDE1017:
	.long	.LASFDE1017-.Lframe1
	.long	.LFB1234
	.long	.LFE1234-.LFB1234
	.uleb128 0x0
	.align 8
.LEFDE1017:
.LSFDE1019:
	.long	.LEFDE1019-.LASFDE1019
.LASFDE1019:
	.long	.LASFDE1019-.Lframe1
	.long	.LFB1235
	.long	.LFE1235-.LFB1235
	.uleb128 0x0
	.align 8
.LEFDE1019:
.LSFDE1021:
	.long	.LEFDE1021-.LASFDE1021
.LASFDE1021:
	.long	.LASFDE1021-.Lframe1
	.long	.LFB1244
	.long	.LFE1244-.LFB1244
	.uleb128 0x0
	.align 8
.LEFDE1021:
.LSFDE1023:
	.long	.LEFDE1023-.LASFDE1023
.LASFDE1023:
	.long	.LASFDE1023-.Lframe1
	.long	.LFB1245
	.long	.LFE1245-.LFB1245
	.uleb128 0x0
	.align 8
.LEFDE1023:
.LSFDE1025:
	.long	.LEFDE1025-.LASFDE1025
.LASFDE1025:
	.long	.LASFDE1025-.Lframe1
	.long	.LFB1246
	.long	.LFE1246-.LFB1246
	.uleb128 0x0
	.align 8
.LEFDE1025:
.LSFDE1027:
	.long	.LEFDE1027-.LASFDE1027
.LASFDE1027:
	.long	.LASFDE1027-.Lframe1
	.long	.LFB1247
	.long	.LFE1247-.LFB1247
	.uleb128 0x0
	.align 8
.LEFDE1027:
.LSFDE1029:
	.long	.LEFDE1029-.LASFDE1029
.LASFDE1029:
	.long	.LASFDE1029-.Lframe1
	.long	.LFB1248
	.long	.LFE1248-.LFB1248
	.uleb128 0x0
	.align 8
.LEFDE1029:
.LSFDE1031:
	.long	.LEFDE1031-.LASFDE1031
.LASFDE1031:
	.long	.LASFDE1031-.Lframe1
	.long	.LFB1249
	.long	.LFE1249-.LFB1249
	.uleb128 0x0
	.align 8
.LEFDE1031:
.LSFDE1033:
	.long	.LEFDE1033-.LASFDE1033
.LASFDE1033:
	.long	.LASFDE1033-.Lframe1
	.long	.LFB1250
	.long	.LFE1250-.LFB1250
	.uleb128 0x0
	.align 8
.LEFDE1033:
.LSFDE1035:
	.long	.LEFDE1035-.LASFDE1035
.LASFDE1035:
	.long	.LASFDE1035-.Lframe1
	.long	.LFB1251
	.long	.LFE1251-.LFB1251
	.uleb128 0x0
	.align 8
.LEFDE1035:
.LSFDE1037:
	.long	.LEFDE1037-.LASFDE1037
.LASFDE1037:
	.long	.LASFDE1037-.Lframe1
	.long	.LFB1252
	.long	.LFE1252-.LFB1252
	.uleb128 0x0
	.align 8
.LEFDE1037:
.LSFDE1039:
	.long	.LEFDE1039-.LASFDE1039
.LASFDE1039:
	.long	.LASFDE1039-.Lframe1
	.long	.LFB1253
	.long	.LFE1253-.LFB1253
	.uleb128 0x0
	.align 8
.LEFDE1039:
.LSFDE1041:
	.long	.LEFDE1041-.LASFDE1041
.LASFDE1041:
	.long	.LASFDE1041-.Lframe1
	.long	.LFB1254
	.long	.LFE1254-.LFB1254
	.uleb128 0x0
	.align 8
.LEFDE1041:
.LSFDE1043:
	.long	.LEFDE1043-.LASFDE1043
.LASFDE1043:
	.long	.LASFDE1043-.Lframe1
	.long	.LFB1255
	.long	.LFE1255-.LFB1255
	.uleb128 0x0
	.align 8
.LEFDE1043:
.LSFDE1045:
	.long	.LEFDE1045-.LASFDE1045
.LASFDE1045:
	.long	.LASFDE1045-.Lframe1
	.long	.LFB1256
	.long	.LFE1256-.LFB1256
	.uleb128 0x0
	.align 8
.LEFDE1045:
.LSFDE1047:
	.long	.LEFDE1047-.LASFDE1047
.LASFDE1047:
	.long	.LASFDE1047-.Lframe1
	.long	.LFB1257
	.long	.LFE1257-.LFB1257
	.uleb128 0x0
	.align 8
.LEFDE1047:
.LSFDE1049:
	.long	.LEFDE1049-.LASFDE1049
.LASFDE1049:
	.long	.LASFDE1049-.Lframe1
	.long	.LFB1258
	.long	.LFE1258-.LFB1258
	.uleb128 0x0
	.align 8
.LEFDE1049:
.LSFDE1051:
	.long	.LEFDE1051-.LASFDE1051
.LASFDE1051:
	.long	.LASFDE1051-.Lframe1
	.long	.LFB1259
	.long	.LFE1259-.LFB1259
	.uleb128 0x0
	.align 8
.LEFDE1051:
.LSFDE1053:
	.long	.LEFDE1053-.LASFDE1053
.LASFDE1053:
	.long	.LASFDE1053-.Lframe1
	.long	.LFB1260
	.long	.LFE1260-.LFB1260
	.uleb128 0x0
	.align 8
.LEFDE1053:
.LSFDE1055:
	.long	.LEFDE1055-.LASFDE1055
.LASFDE1055:
	.long	.LASFDE1055-.Lframe1
	.long	.LFB1261
	.long	.LFE1261-.LFB1261
	.uleb128 0x0
	.align 8
.LEFDE1055:
.LSFDE1057:
	.long	.LEFDE1057-.LASFDE1057
.LASFDE1057:
	.long	.LASFDE1057-.Lframe1
	.long	.LFB1262
	.long	.LFE1262-.LFB1262
	.uleb128 0x0
	.align 8
.LEFDE1057:
.LSFDE1059:
	.long	.LEFDE1059-.LASFDE1059
.LASFDE1059:
	.long	.LASFDE1059-.Lframe1
	.long	.LFB1263
	.long	.LFE1263-.LFB1263
	.uleb128 0x0
	.align 8
.LEFDE1059:
.LSFDE1061:
	.long	.LEFDE1061-.LASFDE1061
.LASFDE1061:
	.long	.LASFDE1061-.Lframe1
	.long	.LFB1264
	.long	.LFE1264-.LFB1264
	.uleb128 0x0
	.align 8
.LEFDE1061:
.LSFDE1063:
	.long	.LEFDE1063-.LASFDE1063
.LASFDE1063:
	.long	.LASFDE1063-.Lframe1
	.long	.LFB1267
	.long	.LFE1267-.LFB1267
	.uleb128 0x0
	.align 8
.LEFDE1063:
.LSFDE1065:
	.long	.LEFDE1065-.LASFDE1065
.LASFDE1065:
	.long	.LASFDE1065-.Lframe1
	.long	.LFB1268
	.long	.LFE1268-.LFB1268
	.uleb128 0x0
	.align 8
.LEFDE1065:
.LSFDE1067:
	.long	.LEFDE1067-.LASFDE1067
.LASFDE1067:
	.long	.LASFDE1067-.Lframe1
	.long	.LFB1269
	.long	.LFE1269-.LFB1269
	.uleb128 0x0
	.align 8
.LEFDE1067:
.LSFDE1069:
	.long	.LEFDE1069-.LASFDE1069
.LASFDE1069:
	.long	.LASFDE1069-.Lframe1
	.long	.LFB1270
	.long	.LFE1270-.LFB1270
	.uleb128 0x0
	.align 8
.LEFDE1069:
.LSFDE1071:
	.long	.LEFDE1071-.LASFDE1071
.LASFDE1071:
	.long	.LASFDE1071-.Lframe1
	.long	.LFB1271
	.long	.LFE1271-.LFB1271
	.uleb128 0x0
	.align 8
.LEFDE1071:
.LSFDE1073:
	.long	.LEFDE1073-.LASFDE1073
.LASFDE1073:
	.long	.LASFDE1073-.Lframe1
	.long	.LFB1272
	.long	.LFE1272-.LFB1272
	.uleb128 0x0
	.align 8
.LEFDE1073:
.LSFDE1075:
	.long	.LEFDE1075-.LASFDE1075
.LASFDE1075:
	.long	.LASFDE1075-.Lframe1
	.long	.LFB1273
	.long	.LFE1273-.LFB1273
	.uleb128 0x0
	.align 8
.LEFDE1075:
.LSFDE1077:
	.long	.LEFDE1077-.LASFDE1077
.LASFDE1077:
	.long	.LASFDE1077-.Lframe1
	.long	.LFB1290
	.long	.LFE1290-.LFB1290
	.uleb128 0x0
	.align 8
.LEFDE1077:
.LSFDE1079:
	.long	.LEFDE1079-.LASFDE1079
.LASFDE1079:
	.long	.LASFDE1079-.Lframe1
	.long	.LFB1291
	.long	.LFE1291-.LFB1291
	.uleb128 0x0
	.align 8
.LEFDE1079:
.LSFDE1081:
	.long	.LEFDE1081-.LASFDE1081
.LASFDE1081:
	.long	.LASFDE1081-.Lframe1
	.long	.LFB1292
	.long	.LFE1292-.LFB1292
	.uleb128 0x0
	.align 8
.LEFDE1081:
.LSFDE1083:
	.long	.LEFDE1083-.LASFDE1083
.LASFDE1083:
	.long	.LASFDE1083-.Lframe1
	.long	.LFB1293
	.long	.LFE1293-.LFB1293
	.uleb128 0x0
	.align 8
.LEFDE1083:
.LSFDE1085:
	.long	.LEFDE1085-.LASFDE1085
.LASFDE1085:
	.long	.LASFDE1085-.Lframe1
	.long	.LFB1294
	.long	.LFE1294-.LFB1294
	.uleb128 0x0
	.align 8
.LEFDE1085:
.LSFDE1087:
	.long	.LEFDE1087-.LASFDE1087
.LASFDE1087:
	.long	.LASFDE1087-.Lframe1
	.long	.LFB1295
	.long	.LFE1295-.LFB1295
	.uleb128 0x0
	.align 8
.LEFDE1087:
.LSFDE1089:
	.long	.LEFDE1089-.LASFDE1089
.LASFDE1089:
	.long	.LASFDE1089-.Lframe1
	.long	.LFB1296
	.long	.LFE1296-.LFB1296
	.uleb128 0x0
	.align 8
.LEFDE1089:
.LSFDE1091:
	.long	.LEFDE1091-.LASFDE1091
.LASFDE1091:
	.long	.LASFDE1091-.Lframe1
	.long	.LFB1297
	.long	.LFE1297-.LFB1297
	.uleb128 0x0
	.align 8
.LEFDE1091:
.LSFDE1093:
	.long	.LEFDE1093-.LASFDE1093
.LASFDE1093:
	.long	.LASFDE1093-.Lframe1
	.long	.LFB1328
	.long	.LFE1328-.LFB1328
	.uleb128 0x0
	.align 8
.LEFDE1093:
.LSFDE1095:
	.long	.LEFDE1095-.LASFDE1095
.LASFDE1095:
	.long	.LASFDE1095-.Lframe1
	.long	.LFB1329
	.long	.LFE1329-.LFB1329
	.uleb128 0x0
	.align 8
.LEFDE1095:
.LSFDE1097:
	.long	.LEFDE1097-.LASFDE1097
.LASFDE1097:
	.long	.LASFDE1097-.Lframe1
	.long	.LFB1330
	.long	.LFE1330-.LFB1330
	.uleb128 0x0
	.align 8
.LEFDE1097:
.LSFDE1099:
	.long	.LEFDE1099-.LASFDE1099
.LASFDE1099:
	.long	.LASFDE1099-.Lframe1
	.long	.LFB1331
	.long	.LFE1331-.LFB1331
	.uleb128 0x0
	.align 8
.LEFDE1099:
.LSFDE1101:
	.long	.LEFDE1101-.LASFDE1101
.LASFDE1101:
	.long	.LASFDE1101-.Lframe1
	.long	.LFB1332
	.long	.LFE1332-.LFB1332
	.uleb128 0x0
	.align 8
.LEFDE1101:
.LSFDE1103:
	.long	.LEFDE1103-.LASFDE1103
.LASFDE1103:
	.long	.LASFDE1103-.Lframe1
	.long	.LFB1333
	.long	.LFE1333-.LFB1333
	.uleb128 0x0
	.align 8
.LEFDE1103:
.LSFDE1105:
	.long	.LEFDE1105-.LASFDE1105
.LASFDE1105:
	.long	.LASFDE1105-.Lframe1
	.long	.LFB1370
	.long	.LFE1370-.LFB1370
	.uleb128 0x0
	.align 8
.LEFDE1105:
.LSFDE1107:
	.long	.LEFDE1107-.LASFDE1107
.LASFDE1107:
	.long	.LASFDE1107-.Lframe1
	.long	.LFB1371
	.long	.LFE1371-.LFB1371
	.uleb128 0x0
	.align 8
.LEFDE1107:
.LSFDE1109:
	.long	.LEFDE1109-.LASFDE1109
.LASFDE1109:
	.long	.LASFDE1109-.Lframe1
	.long	.LFB1372
	.long	.LFE1372-.LFB1372
	.uleb128 0x0
	.align 8
.LEFDE1109:
.LSFDE1111:
	.long	.LEFDE1111-.LASFDE1111
.LASFDE1111:
	.long	.LASFDE1111-.Lframe1
	.long	.LFB1373
	.long	.LFE1373-.LFB1373
	.uleb128 0x0
	.align 8
.LEFDE1111:
.LSFDE1113:
	.long	.LEFDE1113-.LASFDE1113
.LASFDE1113:
	.long	.LASFDE1113-.Lframe1
	.long	.LFB1374
	.long	.LFE1374-.LFB1374
	.uleb128 0x0
	.align 8
.LEFDE1113:
.LSFDE1115:
	.long	.LEFDE1115-.LASFDE1115
.LASFDE1115:
	.long	.LASFDE1115-.Lframe1
	.long	.LFB1378
	.long	.LFE1378-.LFB1378
	.uleb128 0x0
	.align 8
.LEFDE1115:
.LSFDE1117:
	.long	.LEFDE1117-.LASFDE1117
.LASFDE1117:
	.long	.LASFDE1117-.Lframe1
	.long	.LFB1379
	.long	.LFE1379-.LFB1379
	.uleb128 0x0
	.align 8
.LEFDE1117:
.LSFDE1119:
	.long	.LEFDE1119-.LASFDE1119
.LASFDE1119:
	.long	.LASFDE1119-.Lframe1
	.long	.LFB1380
	.long	.LFE1380-.LFB1380
	.uleb128 0x0
	.align 8
.LEFDE1119:
.LSFDE1121:
	.long	.LEFDE1121-.LASFDE1121
.LASFDE1121:
	.long	.LASFDE1121-.Lframe1
	.long	.LFB1381
	.long	.LFE1381-.LFB1381
	.uleb128 0x0
	.align 8
.LEFDE1121:
.LSFDE1123:
	.long	.LEFDE1123-.LASFDE1123
.LASFDE1123:
	.long	.LASFDE1123-.Lframe1
	.long	.LFB1382
	.long	.LFE1382-.LFB1382
	.uleb128 0x0
	.align 8
.LEFDE1123:
.LSFDE1125:
	.long	.LEFDE1125-.LASFDE1125
.LASFDE1125:
	.long	.LASFDE1125-.Lframe1
	.long	.LFB1383
	.long	.LFE1383-.LFB1383
	.uleb128 0x0
	.align 8
.LEFDE1125:
.LSFDE1127:
	.long	.LEFDE1127-.LASFDE1127
.LASFDE1127:
	.long	.LASFDE1127-.Lframe1
	.long	.LFB1384
	.long	.LFE1384-.LFB1384
	.uleb128 0x0
	.align 8
.LEFDE1127:
.LSFDE1129:
	.long	.LEFDE1129-.LASFDE1129
.LASFDE1129:
	.long	.LASFDE1129-.Lframe1
	.long	.LFB1385
	.long	.LFE1385-.LFB1385
	.uleb128 0x0
	.align 8
.LEFDE1129:
.LSFDE1131:
	.long	.LEFDE1131-.LASFDE1131
.LASFDE1131:
	.long	.LASFDE1131-.Lframe1
	.long	.LFB1369
	.long	.LFE1369-.LFB1369
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI27-.LFB1369
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1131:
.LSFDE1133:
	.long	.LEFDE1133-.LASFDE1133
.LASFDE1133:
	.long	.LASFDE1133-.Lframe1
	.long	.LFB1368
	.long	.LFE1368-.LFB1368
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI28-.LFB1368
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1133:
.LSFDE1135:
	.long	.LEFDE1135-.LASFDE1135
.LASFDE1135:
	.long	.LASFDE1135-.Lframe1
	.long	.LFB1367
	.long	.LFE1367-.LFB1367
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI29-.LFB1367
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1135:
.LSFDE1137:
	.long	.LEFDE1137-.LASFDE1137
.LASFDE1137:
	.long	.LASFDE1137-.Lframe1
	.long	.LFB1366
	.long	.LFE1366-.LFB1366
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI30-.LFB1366
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1137:
.LSFDE1139:
	.long	.LEFDE1139-.LASFDE1139
.LASFDE1139:
	.long	.LASFDE1139-.Lframe1
	.long	.LFB1327
	.long	.LFE1327-.LFB1327
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI31-.LFB1327
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1139:
.LSFDE1141:
	.long	.LEFDE1141-.LASFDE1141
.LASFDE1141:
	.long	.LASFDE1141-.Lframe1
	.long	.LFB1326
	.long	.LFE1326-.LFB1326
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI34-.LFB1326
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1141:
.LSFDE1143:
	.long	.LEFDE1143-.LASFDE1143
.LASFDE1143:
	.long	.LASFDE1143-.Lframe1
	.long	.LFB1325
	.long	.LFE1325-.LFB1325
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI35-.LFB1325
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1143:
.LSFDE1145:
	.long	.LEFDE1145-.LASFDE1145
.LASFDE1145:
	.long	.LASFDE1145-.Lframe1
	.long	.LFB1324
	.long	.LFE1324-.LFB1324
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI38-.LFB1324
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1145:
.LSFDE1147:
	.long	.LEFDE1147-.LASFDE1147
.LASFDE1147:
	.long	.LASFDE1147-.Lframe1
	.long	.LFB1323
	.long	.LFE1323-.LFB1323
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI39-.LFB1323
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1147:
.LSFDE1149:
	.long	.LEFDE1149-.LASFDE1149
.LASFDE1149:
	.long	.LASFDE1149-.Lframe1
	.long	.LFB1321
	.long	.LFE1321-.LFB1321
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI42-.LFB1321
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1149:
.LSFDE1151:
	.long	.LEFDE1151-.LASFDE1151
.LASFDE1151:
	.long	.LASFDE1151-.Lframe1
	.long	.LFB1319
	.long	.LFE1319-.LFB1319
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI45-.LFB1319
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1151:
.LSFDE1153:
	.long	.LEFDE1153-.LASFDE1153
.LASFDE1153:
	.long	.LASFDE1153-.Lframe1
	.long	.LFB1322
	.long	.LFE1322-.LFB1322
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI46-.LFB1322
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1153:
.LSFDE1155:
	.long	.LEFDE1155-.LASFDE1155
.LASFDE1155:
	.long	.LASFDE1155-.Lframe1
	.long	.LFB1320
	.long	.LFE1320-.LFB1320
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI49-.LFB1320
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1155:
.LSFDE1157:
	.long	.LEFDE1157-.LASFDE1157
.LASFDE1157:
	.long	.LASFDE1157-.Lframe1
	.long	.LFB1318
	.long	.LFE1318-.LFB1318
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI50-.LFB1318
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI51-.LCFI50
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI52-.LCFI51
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1157:
.LSFDE1159:
	.long	.LEFDE1159-.LASFDE1159
.LASFDE1159:
	.long	.LASFDE1159-.Lframe1
	.long	.LFB1317
	.long	.LFE1317-.LFB1317
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI53-.LFB1317
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1159:
.LSFDE1161:
	.long	.LEFDE1161-.LASFDE1161
.LASFDE1161:
	.long	.LASFDE1161-.Lframe1
	.long	.LFB1315
	.long	.LFE1315-.LFB1315
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI56-.LFB1315
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1161:
.LSFDE1163:
	.long	.LEFDE1163-.LASFDE1163
.LASFDE1163:
	.long	.LASFDE1163-.Lframe1
	.long	.LFB1313
	.long	.LFE1313-.LFB1313
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI59-.LFB1313
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1163:
.LSFDE1165:
	.long	.LEFDE1165-.LASFDE1165
.LASFDE1165:
	.long	.LASFDE1165-.Lframe1
	.long	.LFB1316
	.long	.LFE1316-.LFB1316
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI60-.LFB1316
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1165:
.LSFDE1167:
	.long	.LEFDE1167-.LASFDE1167
.LASFDE1167:
	.long	.LASFDE1167-.Lframe1
	.long	.LFB1314
	.long	.LFE1314-.LFB1314
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI63-.LFB1314
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1167:
.LSFDE1169:
	.long	.LEFDE1169-.LASFDE1169
.LASFDE1169:
	.long	.LASFDE1169-.Lframe1
	.long	.LFB1312
	.long	.LFE1312-.LFB1312
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI64-.LFB1312
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI65-.LCFI64
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI66-.LCFI65
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1169:
.LSFDE1171:
	.long	.LEFDE1171-.LASFDE1171
.LASFDE1171:
	.long	.LASFDE1171-.Lframe1
	.long	.LFB1311
	.long	.LFE1311-.LFB1311
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI67-.LFB1311
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1171:
.LSFDE1173:
	.long	.LEFDE1173-.LASFDE1173
.LASFDE1173:
	.long	.LASFDE1173-.Lframe1
	.long	.LFB1309
	.long	.LFE1309-.LFB1309
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI70-.LFB1309
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1173:
.LSFDE1175:
	.long	.LEFDE1175-.LASFDE1175
.LASFDE1175:
	.long	.LASFDE1175-.Lframe1
	.long	.LFB1307
	.long	.LFE1307-.LFB1307
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI73-.LFB1307
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1175:
.LSFDE1177:
	.long	.LEFDE1177-.LASFDE1177
.LASFDE1177:
	.long	.LASFDE1177-.Lframe1
	.long	.LFB1310
	.long	.LFE1310-.LFB1310
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI74-.LFB1310
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1177:
.LSFDE1179:
	.long	.LEFDE1179-.LASFDE1179
.LASFDE1179:
	.long	.LASFDE1179-.Lframe1
	.long	.LFB1308
	.long	.LFE1308-.LFB1308
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI77-.LFB1308
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1179:
.LSFDE1181:
	.long	.LEFDE1181-.LASFDE1181
.LASFDE1181:
	.long	.LASFDE1181-.Lframe1
	.long	.LFB1306
	.long	.LFE1306-.LFB1306
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI78-.LFB1306
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI79-.LCFI78
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI80-.LCFI79
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1181:
.LSFDE1183:
	.long	.LEFDE1183-.LASFDE1183
.LASFDE1183:
	.long	.LASFDE1183-.Lframe1
	.long	.LFB1305
	.long	.LFE1305-.LFB1305
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI81-.LFB1305
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1183:
.LSFDE1185:
	.long	.LEFDE1185-.LASFDE1185
.LASFDE1185:
	.long	.LASFDE1185-.Lframe1
	.long	.LFB1303
	.long	.LFE1303-.LFB1303
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI84-.LFB1303
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1185:
.LSFDE1187:
	.long	.LEFDE1187-.LASFDE1187
.LASFDE1187:
	.long	.LASFDE1187-.Lframe1
	.long	.LFB1304
	.long	.LFE1304-.LFB1304
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI85-.LFB1304
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1187:
.LSFDE1189:
	.long	.LEFDE1189-.LASFDE1189
.LASFDE1189:
	.long	.LASFDE1189-.Lframe1
	.long	.LFB1302
	.long	.LFE1302-.LFB1302
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI88-.LFB1302
	.byte	0xe
	.uleb128 0x30
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1189:
.LSFDE1191:
	.long	.LEFDE1191-.LASFDE1191
.LASFDE1191:
	.long	.LASFDE1191-.Lframe1
	.long	.LFB1301
	.long	.LFE1301-.LFB1301
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI89-.LFB1301
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1191:
.LSFDE1193:
	.long	.LEFDE1193-.LASFDE1193
.LASFDE1193:
	.long	.LASFDE1193-.Lframe1
	.long	.LFB1300
	.long	.LFE1300-.LFB1300
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI92-.LFB1300
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1193:
.LSFDE1195:
	.long	.LEFDE1195-.LASFDE1195
.LASFDE1195:
	.long	.LASFDE1195-.Lframe1
	.long	.LFB1299
	.long	.LFE1299-.LFB1299
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI93-.LFB1299
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1195:
.LSFDE1197:
	.long	.LEFDE1197-.LASFDE1197
.LASFDE1197:
	.long	.LASFDE1197-.Lframe1
	.long	.LFB1298
	.long	.LFE1298-.LFB1298
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI94-.LFB1298
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI95-.LCFI94
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI96-.LCFI95
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1197:
.LSFDE1199:
	.long	.LEFDE1199-.LASFDE1199
.LASFDE1199:
	.long	.LASFDE1199-.Lframe1
	.long	.LFB1266
	.long	.LFE1266-.LFB1266
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI97-.LFB1266
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI98-.LCFI97
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI99-.LCFI98
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1199:
.LSFDE1201:
	.long	.LEFDE1201-.LASFDE1201
.LASFDE1201:
	.long	.LASFDE1201-.Lframe1
	.long	.LFB716
	.long	.LFE716-.LFB716
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI100-.LFB716
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1201:
.LSFDE1203:
	.long	.LEFDE1203-.LASFDE1203
.LASFDE1203:
	.long	.LASFDE1203-.Lframe1
	.long	.LFB1148
	.long	.LFE1148-.LFB1148
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI101-.LFB1148
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1203:
.LSFDE1205:
	.long	.LEFDE1205-.LASFDE1205
.LASFDE1205:
	.long	.LASFDE1205-.Lframe1
	.long	.LFB1147
	.long	.LFE1147-.LFB1147
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI102-.LFB1147
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1205:
.LSFDE1207:
	.long	.LEFDE1207-.LASFDE1207
.LASFDE1207:
	.long	.LASFDE1207-.Lframe1
	.long	.LFB1141
	.long	.LFE1141-.LFB1141
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI103-.LFB1141
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1207:
.LSFDE1209:
	.long	.LEFDE1209-.LASFDE1209
.LASFDE1209:
	.long	.LASFDE1209-.Lframe1
	.long	.LFB1140
	.long	.LFE1140-.LFB1140
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI104-.LFB1140
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1209:
.LSFDE1211:
	.long	.LEFDE1211-.LASFDE1211
.LASFDE1211:
	.long	.LASFDE1211-.Lframe1
	.long	.LFB1139
	.long	.LFE1139-.LFB1139
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI105-.LFB1139
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1211:
.LSFDE1213:
	.long	.LEFDE1213-.LASFDE1213
.LASFDE1213:
	.long	.LASFDE1213-.Lframe1
	.long	.LFB1138
	.long	.LFE1138-.LFB1138
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI106-.LFB1138
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1213:
.LSFDE1215:
	.long	.LEFDE1215-.LASFDE1215
.LASFDE1215:
	.long	.LASFDE1215-.Lframe1
	.long	.LFB1137
	.long	.LFE1137-.LFB1137
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI107-.LFB1137
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1215:
.LSFDE1217:
	.long	.LEFDE1217-.LASFDE1217
.LASFDE1217:
	.long	.LASFDE1217-.Lframe1
	.long	.LFB1136
	.long	.LFE1136-.LFB1136
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI108-.LFB1136
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1217:
.LSFDE1219:
	.long	.LEFDE1219-.LASFDE1219
.LASFDE1219:
	.long	.LASFDE1219-.Lframe1
	.long	.LFB1134
	.long	.LFE1134-.LFB1134
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI109-.LFB1134
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1219:
.LSFDE1221:
	.long	.LEFDE1221-.LASFDE1221
.LASFDE1221:
	.long	.LASFDE1221-.Lframe1
	.long	.LFB1132
	.long	.LFE1132-.LFB1132
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI110-.LFB1132
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1221:
.LSFDE1223:
	.long	.LEFDE1223-.LASFDE1223
.LASFDE1223:
	.long	.LASFDE1223-.Lframe1
	.long	.LFB601
	.long	.LFE601-.LFB601
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI111-.LFB601
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1223:
.LSFDE1225:
	.long	.LEFDE1225-.LASFDE1225
.LASFDE1225:
	.long	.LASFDE1225-.Lframe1
	.long	.LFB582
	.long	.LFE582-.LFB582
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI112-.LFB582
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1225:
.LSFDE1227:
	.long	.LEFDE1227-.LASFDE1227
.LASFDE1227:
	.long	.LASFDE1227-.Lframe1
	.long	.LFB568
	.long	.LFE568-.LFB568
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI113-.LFB568
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1227:
.LSFDE1229:
	.long	.LEFDE1229-.LASFDE1229
.LASFDE1229:
	.long	.LASFDE1229-.Lframe1
	.long	.LFB1129
	.long	.LFE1129-.LFB1129
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI114-.LFB1129
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1229:
.LSFDE1231:
	.long	.LEFDE1231-.LASFDE1231
.LASFDE1231:
	.long	.LASFDE1231-.Lframe1
	.long	.LFB812
	.long	.LFE812-.LFB812
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI115-.LFB812
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1231:
.LSFDE1233:
	.long	.LEFDE1233-.LASFDE1233
.LASFDE1233:
	.long	.LASFDE1233-.Lframe1
	.long	.LFB795
	.long	.LFE795-.LFB795
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI116-.LFB795
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1233:
.LSFDE1235:
	.long	.LEFDE1235-.LASFDE1235
.LASFDE1235:
	.long	.LASFDE1235-.Lframe1
	.long	.LFB786
	.long	.LFE786-.LFB786
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI117-.LFB786
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI118-.LCFI117
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI119-.LCFI118
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1235:
.LSFDE1237:
	.long	.LEFDE1237-.LASFDE1237
.LASFDE1237:
	.long	.LASFDE1237-.Lframe1
	.long	.LFB1128
	.long	.LFE1128-.LFB1128
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI120-.LFB1128
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1237:
.LSFDE1239:
	.long	.LEFDE1239-.LASFDE1239
.LASFDE1239:
	.long	.LASFDE1239-.Lframe1
	.long	.LFB1127
	.long	.LFE1127-.LFB1127
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI121-.LFB1127
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1239:
.LSFDE1241:
	.long	.LEFDE1241-.LASFDE1241
.LASFDE1241:
	.long	.LASFDE1241-.Lframe1
	.long	.LFB1126
	.long	.LFE1126-.LFB1126
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI122-.LFB1126
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1241:
.LSFDE1243:
	.long	.LEFDE1243-.LASFDE1243
.LASFDE1243:
	.long	.LASFDE1243-.Lframe1
	.long	.LFB1125
	.long	.LFE1125-.LFB1125
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI123-.LFB1125
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1243:
.LSFDE1245:
	.long	.LEFDE1245-.LASFDE1245
.LASFDE1245:
	.long	.LASFDE1245-.Lframe1
	.long	.LFB1124
	.long	.LFE1124-.LFB1124
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI124-.LFB1124
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1245:
.LSFDE1247:
	.long	.LEFDE1247-.LASFDE1247
.LASFDE1247:
	.long	.LASFDE1247-.Lframe1
	.long	.LFB1123
	.long	.LFE1123-.LFB1123
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI125-.LFB1123
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1247:
.LSFDE1249:
	.long	.LEFDE1249-.LASFDE1249
.LASFDE1249:
	.long	.LASFDE1249-.Lframe1
	.long	.LFB1122
	.long	.LFE1122-.LFB1122
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI126-.LFB1122
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1249:
.LSFDE1251:
	.long	.LEFDE1251-.LASFDE1251
.LASFDE1251:
	.long	.LASFDE1251-.Lframe1
	.long	.LFB1121
	.long	.LFE1121-.LFB1121
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI127-.LFB1121
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1251:
.LSFDE1253:
	.long	.LEFDE1253-.LASFDE1253
.LASFDE1253:
	.long	.LASFDE1253-.Lframe1
	.long	.LFB1120
	.long	.LFE1120-.LFB1120
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI128-.LFB1120
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1253:
.LSFDE1255:
	.long	.LEFDE1255-.LASFDE1255
.LASFDE1255:
	.long	.LASFDE1255-.Lframe1
	.long	.LFB1119
	.long	.LFE1119-.LFB1119
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI129-.LFB1119
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1255:
.LSFDE1257:
	.long	.LEFDE1257-.LASFDE1257
.LASFDE1257:
	.long	.LASFDE1257-.Lframe1
	.long	.LFB1118
	.long	.LFE1118-.LFB1118
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI130-.LFB1118
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1257:
.LSFDE1259:
	.long	.LEFDE1259-.LASFDE1259
.LASFDE1259:
	.long	.LASFDE1259-.Lframe1
	.long	.LFB1117
	.long	.LFE1117-.LFB1117
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI131-.LFB1117
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1259:
.LSFDE1261:
	.long	.LEFDE1261-.LASFDE1261
.LASFDE1261:
	.long	.LASFDE1261-.Lframe1
	.long	.LFB1116
	.long	.LFE1116-.LFB1116
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI132-.LFB1116
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1261:
.LSFDE1263:
	.long	.LEFDE1263-.LASFDE1263
.LASFDE1263:
	.long	.LASFDE1263-.Lframe1
	.long	.LFB1115
	.long	.LFE1115-.LFB1115
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI133-.LFB1115
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1263:
.LSFDE1265:
	.long	.LEFDE1265-.LASFDE1265
.LASFDE1265:
	.long	.LASFDE1265-.Lframe1
	.long	.LFB1106
	.long	.LFE1106-.LFB1106
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI134-.LFB1106
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1265:
.LSFDE1267:
	.long	.LEFDE1267-.LASFDE1267
.LASFDE1267:
	.long	.LASFDE1267-.Lframe1
	.long	.LFB1105
	.long	.LFE1105-.LFB1105
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI135-.LFB1105
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI136-.LCFI135
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1267:
.LSFDE1269:
	.long	.LEFDE1269-.LASFDE1269
.LASFDE1269:
	.long	.LASFDE1269-.Lframe1
	.long	.LFB1103
	.long	.LFE1103-.LFB1103
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI137-.LFB1103
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI138-.LCFI137
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1269:
.LSFDE1271:
	.long	.LEFDE1271-.LASFDE1271
.LASFDE1271:
	.long	.LASFDE1271-.Lframe1
	.long	.LFB1102
	.long	.LFE1102-.LFB1102
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI139-.LFB1102
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI140-.LCFI139
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1271:
.LSFDE1273:
	.long	.LEFDE1273-.LASFDE1273
.LASFDE1273:
	.long	.LASFDE1273-.Lframe1
	.long	.LFB1097
	.long	.LFE1097-.LFB1097
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI141-.LFB1097
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI142-.LCFI141
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1273:
.LSFDE1275:
	.long	.LEFDE1275-.LASFDE1275
.LASFDE1275:
	.long	.LASFDE1275-.Lframe1
	.long	.LFB1096
	.long	.LFE1096-.LFB1096
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI143-.LFB1096
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI144-.LCFI143
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1275:
.LSFDE1277:
	.long	.LEFDE1277-.LASFDE1277
.LASFDE1277:
	.long	.LASFDE1277-.Lframe1
	.long	.LFB1090
	.long	.LFE1090-.LFB1090
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI145-.LFB1090
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI146-.LCFI145
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1277:
.LSFDE1279:
	.long	.LEFDE1279-.LASFDE1279
.LASFDE1279:
	.long	.LASFDE1279-.Lframe1
	.long	.LFB1088
	.long	.LFE1088-.LFB1088
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI147-.LFB1088
	.byte	0xe
	.uleb128 0x30
	.align 8
.LEFDE1279:
.LSFDE1281:
	.long	.LEFDE1281-.LASFDE1281
.LASFDE1281:
	.long	.LASFDE1281-.Lframe1
	.long	.LFB1089
	.long	.LFE1089-.LFB1089
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI148-.LFB1089
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI149-.LCFI148
	.byte	0xe
	.uleb128 0x30
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1281:
.LSFDE1283:
	.long	.LEFDE1283-.LASFDE1283
.LASFDE1283:
	.long	.LASFDE1283-.Lframe1
	.long	.LFB1087
	.long	.LFE1087-.LFB1087
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI150-.LFB1087
	.byte	0xe
	.uleb128 0x30
	.align 8
.LEFDE1283:
.LSFDE1285:
	.long	.LEFDE1285-.LASFDE1285
.LASFDE1285:
	.long	.LASFDE1285-.Lframe1
	.long	.LFB1076
	.long	.LFE1076-.LFB1076
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI151-.LFB1076
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1285:
.LSFDE1287:
	.long	.LEFDE1287-.LASFDE1287
.LASFDE1287:
	.long	.LASFDE1287-.Lframe1
	.long	.LFB1075
	.long	.LFE1075-.LFB1075
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI152-.LFB1075
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1287:
.LSFDE1289:
	.long	.LEFDE1289-.LASFDE1289
.LASFDE1289:
	.long	.LASFDE1289-.Lframe1
	.long	.LFB1067
	.long	.LFE1067-.LFB1067
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI153-.LFB1067
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1289:
.LSFDE1291:
	.long	.LEFDE1291-.LASFDE1291
.LASFDE1291:
	.long	.LASFDE1291-.Lframe1
	.long	.LFB577
	.long	.LFE577-.LFB577
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI154-.LFB577
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1291:
.LSFDE1293:
	.long	.LEFDE1293-.LASFDE1293
.LASFDE1293:
	.long	.LASFDE1293-.Lframe1
	.long	.LFB1066
	.long	.LFE1066-.LFB1066
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI155-.LFB1066
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1293:
.LSFDE1295:
	.long	.LEFDE1295-.LASFDE1295
.LASFDE1295:
	.long	.LASFDE1295-.Lframe1
	.long	.LFB613
	.long	.LFE613-.LFB613
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI156-.LFB613
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1295:
.LSFDE1297:
	.long	.LEFDE1297-.LASFDE1297
.LASFDE1297:
	.long	.LASFDE1297-.Lframe1
	.long	.LFB924
	.long	.LFE924-.LFB924
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI157-.LFB924
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1297:
.LSFDE1299:
	.long	.LEFDE1299-.LASFDE1299
.LASFDE1299:
	.long	.LASFDE1299-.Lframe1
	.long	.LFB573
	.long	.LFE573-.LFB573
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI158-.LFB573
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1299:
.LSFDE1301:
	.long	.LEFDE1301-.LASFDE1301
.LASFDE1301:
	.long	.LASFDE1301-.Lframe1
	.long	.LFB1068
	.long	.LFE1068-.LFB1068
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI159-.LFB1068
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1301:
.LSFDE1303:
	.long	.LEFDE1303-.LASFDE1303
.LASFDE1303:
	.long	.LASFDE1303-.Lframe1
	.long	.LFB614
	.long	.LFE614-.LFB614
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI160-.LFB614
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1303:
.LSFDE1305:
	.long	.LEFDE1305-.LASFDE1305
.LASFDE1305:
	.long	.LASFDE1305-.Lframe1
	.long	.LFB576
	.long	.LFE576-.LFB576
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI161-.LFB576
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1305:
.LSFDE1307:
	.long	.LEFDE1307-.LASFDE1307
.LASFDE1307:
	.long	.LASFDE1307-.Lframe1
	.long	.LFB603
	.long	.LFE603-.LFB603
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI162-.LFB603
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1307:
.LSFDE1309:
	.long	.LEFDE1309-.LASFDE1309
.LASFDE1309:
	.long	.LASFDE1309-.Lframe1
	.long	.LFB584
	.long	.LFE584-.LFB584
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI163-.LFB584
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1309:
.LSFDE1311:
	.long	.LEFDE1311-.LASFDE1311
.LASFDE1311:
	.long	.LASFDE1311-.Lframe1
	.long	.LFB570
	.long	.LFE570-.LFB570
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI164-.LFB570
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1311:
.LSFDE1313:
	.long	.LEFDE1313-.LASFDE1313
.LASFDE1313:
	.long	.LASFDE1313-.Lframe1
	.long	.LFB602
	.long	.LFE602-.LFB602
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI165-.LFB602
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1313:
.LSFDE1315:
	.long	.LEFDE1315-.LASFDE1315
.LASFDE1315:
	.long	.LASFDE1315-.Lframe1
	.long	.LFB583
	.long	.LFE583-.LFB583
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI166-.LFB583
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1315:
.LSFDE1317:
	.long	.LEFDE1317-.LASFDE1317
.LASFDE1317:
	.long	.LASFDE1317-.Lframe1
	.long	.LFB564
	.long	.LFE564-.LFB564
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI167-.LFB564
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1317:
.LSFDE1319:
	.long	.LEFDE1319-.LASFDE1319
.LASFDE1319:
	.long	.LASFDE1319-.Lframe1
	.long	.LFB1062
	.long	.LFE1062-.LFB1062
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI168-.LFB1062
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1319:
.LSFDE1321:
	.long	.LEFDE1321-.LASFDE1321
.LASFDE1321:
	.long	.LASFDE1321-.Lframe1
	.long	.LFB1014
	.long	.LFE1014-.LFB1014
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI169-.LFB1014
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1321:
.LSFDE1323:
	.long	.LEFDE1323-.LASFDE1323
.LASFDE1323:
	.long	.LASFDE1323-.Lframe1
	.long	.LFB1013
	.long	.LFE1013-.LFB1013
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI170-.LFB1013
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1323:
.LSFDE1325:
	.long	.LEFDE1325-.LASFDE1325
.LASFDE1325:
	.long	.LASFDE1325-.Lframe1
	.long	.LFB1010
	.long	.LFE1010-.LFB1010
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI171-.LFB1010
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1325:
.LSFDE1327:
	.long	.LEFDE1327-.LASFDE1327
.LASFDE1327:
	.long	.LASFDE1327-.Lframe1
	.long	.LFB1012
	.long	.LFE1012-.LFB1012
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI172-.LFB1012
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1327:
.LSFDE1329:
	.long	.LEFDE1329-.LASFDE1329
.LASFDE1329:
	.long	.LASFDE1329-.Lframe1
	.long	.LFB1011
	.long	.LFE1011-.LFB1011
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI173-.LFB1011
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1329:
.LSFDE1331:
	.long	.LEFDE1331-.LASFDE1331
.LASFDE1331:
	.long	.LASFDE1331-.Lframe1
	.long	.LFB1009
	.long	.LFE1009-.LFB1009
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI174-.LFB1009
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1331:
.LSFDE1333:
	.long	.LEFDE1333-.LASFDE1333
.LASFDE1333:
	.long	.LASFDE1333-.Lframe1
	.long	.LFB1008
	.long	.LFE1008-.LFB1008
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI175-.LFB1008
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1333:
.LSFDE1335:
	.long	.LEFDE1335-.LASFDE1335
.LASFDE1335:
	.long	.LASFDE1335-.Lframe1
	.long	.LFB1007
	.long	.LFE1007-.LFB1007
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI176-.LFB1007
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1335:
.LSFDE1337:
	.long	.LEFDE1337-.LASFDE1337
.LASFDE1337:
	.long	.LASFDE1337-.Lframe1
	.long	.LFB1006
	.long	.LFE1006-.LFB1006
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI177-.LFB1006
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1337:
.LSFDE1339:
	.long	.LEFDE1339-.LASFDE1339
.LASFDE1339:
	.long	.LASFDE1339-.Lframe1
	.long	.LFB1005
	.long	.LFE1005-.LFB1005
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI178-.LFB1005
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1339:
.LSFDE1341:
	.long	.LEFDE1341-.LASFDE1341
.LASFDE1341:
	.long	.LASFDE1341-.Lframe1
	.long	.LFB1004
	.long	.LFE1004-.LFB1004
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI179-.LFB1004
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1341:
.LSFDE1343:
	.long	.LEFDE1343-.LASFDE1343
.LASFDE1343:
	.long	.LASFDE1343-.Lframe1
	.long	.LFB1003
	.long	.LFE1003-.LFB1003
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI180-.LFB1003
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1343:
.LSFDE1345:
	.long	.LEFDE1345-.LASFDE1345
.LASFDE1345:
	.long	.LASFDE1345-.Lframe1
	.long	.LFB1002
	.long	.LFE1002-.LFB1002
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI181-.LFB1002
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1345:
.LSFDE1347:
	.long	.LEFDE1347-.LASFDE1347
.LASFDE1347:
	.long	.LASFDE1347-.Lframe1
	.long	.LFB999
	.long	.LFE999-.LFB999
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI182-.LFB999
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1347:
.LSFDE1349:
	.long	.LEFDE1349-.LASFDE1349
.LASFDE1349:
	.long	.LASFDE1349-.Lframe1
	.long	.LFB998
	.long	.LFE998-.LFB998
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI183-.LFB998
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1349:
.LSFDE1351:
	.long	.LEFDE1351-.LASFDE1351
.LASFDE1351:
	.long	.LASFDE1351-.Lframe1
	.long	.LFB997
	.long	.LFE997-.LFB997
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI184-.LFB997
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1351:
.LSFDE1353:
	.long	.LEFDE1353-.LASFDE1353
.LASFDE1353:
	.long	.LASFDE1353-.Lframe1
	.long	.LFB996
	.long	.LFE996-.LFB996
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI185-.LFB996
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1353:
.LSFDE1355:
	.long	.LEFDE1355-.LASFDE1355
.LASFDE1355:
	.long	.LASFDE1355-.Lframe1
	.long	.LFB993
	.long	.LFE993-.LFB993
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI186-.LFB993
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1355:
.LSFDE1357:
	.long	.LEFDE1357-.LASFDE1357
.LASFDE1357:
	.long	.LASFDE1357-.Lframe1
	.long	.LFB986
	.long	.LFE986-.LFB986
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI187-.LFB986
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1357:
.LSFDE1359:
	.long	.LEFDE1359-.LASFDE1359
.LASFDE1359:
	.long	.LASFDE1359-.Lframe1
	.long	.LFB985
	.long	.LFE985-.LFB985
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI188-.LFB985
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1359:
.LSFDE1361:
	.long	.LEFDE1361-.LASFDE1361
.LASFDE1361:
	.long	.LASFDE1361-.Lframe1
	.long	.LFB984
	.long	.LFE984-.LFB984
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI189-.LFB984
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1361:
.LSFDE1363:
	.long	.LEFDE1363-.LASFDE1363
.LASFDE1363:
	.long	.LASFDE1363-.Lframe1
	.long	.LFB983
	.long	.LFE983-.LFB983
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI190-.LFB983
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1363:
.LSFDE1365:
	.long	.LEFDE1365-.LASFDE1365
.LASFDE1365:
	.long	.LASFDE1365-.Lframe1
	.long	.LFB982
	.long	.LFE982-.LFB982
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI191-.LFB982
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1365:
.LSFDE1367:
	.long	.LEFDE1367-.LASFDE1367
.LASFDE1367:
	.long	.LASFDE1367-.Lframe1
	.long	.LFB981
	.long	.LFE981-.LFB981
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI192-.LFB981
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1367:
.LSFDE1369:
	.long	.LEFDE1369-.LASFDE1369
.LASFDE1369:
	.long	.LASFDE1369-.Lframe1
	.long	.LFB980
	.long	.LFE980-.LFB980
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI193-.LFB980
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1369:
.LSFDE1371:
	.long	.LEFDE1371-.LASFDE1371
.LASFDE1371:
	.long	.LASFDE1371-.Lframe1
	.long	.LFB956
	.long	.LFE956-.LFB956
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI194-.LFB956
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1371:
.LSFDE1373:
	.long	.LEFDE1373-.LASFDE1373
.LASFDE1373:
	.long	.LASFDE1373-.Lframe1
	.long	.LFB955
	.long	.LFE955-.LFB955
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI195-.LFB955
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1373:
.LSFDE1375:
	.long	.LEFDE1375-.LASFDE1375
.LASFDE1375:
	.long	.LASFDE1375-.Lframe1
	.long	.LFB954
	.long	.LFE954-.LFB954
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI196-.LFB954
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1375:
.LSFDE1377:
	.long	.LEFDE1377-.LASFDE1377
.LASFDE1377:
	.long	.LASFDE1377-.Lframe1
	.long	.LFB951
	.long	.LFE951-.LFB951
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI197-.LFB951
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1377:
.LSFDE1379:
	.long	.LEFDE1379-.LASFDE1379
.LASFDE1379:
	.long	.LASFDE1379-.Lframe1
	.long	.LFB953
	.long	.LFE953-.LFB953
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI198-.LFB953
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1379:
.LSFDE1381:
	.long	.LEFDE1381-.LASFDE1381
.LASFDE1381:
	.long	.LASFDE1381-.Lframe1
	.long	.LFB952
	.long	.LFE952-.LFB952
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI199-.LFB952
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1381:
.LSFDE1383:
	.long	.LEFDE1383-.LASFDE1383
.LASFDE1383:
	.long	.LASFDE1383-.Lframe1
	.long	.LFB838
	.long	.LFE838-.LFB838
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI200-.LFB838
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1383:
.LSFDE1385:
	.long	.LEFDE1385-.LASFDE1385
.LASFDE1385:
	.long	.LASFDE1385-.Lframe1
	.long	.LFB837
	.long	.LFE837-.LFB837
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI201-.LFB837
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1385:
.LSFDE1387:
	.long	.LEFDE1387-.LASFDE1387
.LASFDE1387:
	.long	.LASFDE1387-.Lframe1
	.long	.LFB836
	.long	.LFE836-.LFB836
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI202-.LFB836
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1387:
.LSFDE1389:
	.long	.LEFDE1389-.LASFDE1389
.LASFDE1389:
	.long	.LASFDE1389-.Lframe1
	.long	.LFB833
	.long	.LFE833-.LFB833
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI203-.LFB833
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1389:
.LSFDE1391:
	.long	.LEFDE1391-.LASFDE1391
.LASFDE1391:
	.long	.LASFDE1391-.Lframe1
	.long	.LFB835
	.long	.LFE835-.LFB835
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI204-.LFB835
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1391:
.LSFDE1393:
	.long	.LEFDE1393-.LASFDE1393
.LASFDE1393:
	.long	.LASFDE1393-.Lframe1
	.long	.LFB834
	.long	.LFE834-.LFB834
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI205-.LFB834
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1393:
.LSFDE1395:
	.long	.LEFDE1395-.LASFDE1395
.LASFDE1395:
	.long	.LASFDE1395-.Lframe1
	.long	.LFB724
	.long	.LFE724-.LFB724
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI206-.LFB724
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1395:
.LSFDE1397:
	.long	.LEFDE1397-.LASFDE1397
.LASFDE1397:
	.long	.LASFDE1397-.Lframe1
	.long	.LFB723
	.long	.LFE723-.LFB723
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI207-.LFB723
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1397:
.LSFDE1399:
	.long	.LEFDE1399-.LASFDE1399
.LASFDE1399:
	.long	.LASFDE1399-.Lframe1
	.long	.LFB722
	.long	.LFE722-.LFB722
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI208-.LFB722
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1399:
.LSFDE1401:
	.long	.LEFDE1401-.LASFDE1401
.LASFDE1401:
	.long	.LASFDE1401-.Lframe1
	.long	.LFB719
	.long	.LFE719-.LFB719
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI209-.LFB719
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1401:
.LSFDE1403:
	.long	.LEFDE1403-.LASFDE1403
.LASFDE1403:
	.long	.LASFDE1403-.Lframe1
	.long	.LFB721
	.long	.LFE721-.LFB721
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI210-.LFB721
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1403:
.LSFDE1405:
	.long	.LEFDE1405-.LASFDE1405
.LASFDE1405:
	.long	.LASFDE1405-.Lframe1
	.long	.LFB720
	.long	.LFE720-.LFB720
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI211-.LFB720
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1405:
.LSFDE1407:
	.long	.LEFDE1407-.LASFDE1407
.LASFDE1407:
	.long	.LASFDE1407-.Lframe1
	.long	.LFB636
	.long	.LFE636-.LFB636
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI212-.LFB636
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1407:
.LSFDE1409:
	.long	.LEFDE1409-.LASFDE1409
.LASFDE1409:
	.long	.LASFDE1409-.Lframe1
	.long	.LFB634
	.long	.LFE634-.LFB634
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI213-.LFB634
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1409:
.LSFDE1411:
	.long	.LEFDE1411-.LASFDE1411
.LASFDE1411:
	.long	.LASFDE1411-.Lframe1
	.long	.LFB625
	.long	.LFE625-.LFB625
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI214-.LFB625
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1411:
.LSFDE1413:
	.long	.LEFDE1413-.LASFDE1413
.LASFDE1413:
	.long	.LASFDE1413-.Lframe1
	.long	.LFB542
	.long	.LFE542-.LFB542
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI217-.LFB542
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1413:
.LSFDE1415:
	.long	.LEFDE1415-.LASFDE1415
.LASFDE1415:
	.long	.LASFDE1415-.Lframe1
	.long	.LFB541
	.long	.LFE541-.LFB541
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI220-.LFB541
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1415:
.LSFDE1417:
	.long	.LEFDE1417-.LASFDE1417
.LASFDE1417:
	.long	.LASFDE1417-.Lframe1
	.long	.LFB540
	.long	.LFE540-.LFB540
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI223-.LFB540
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1417:
.LSFDE1419:
	.long	.LEFDE1419-.LASFDE1419
.LASFDE1419:
	.long	.LASFDE1419-.Lframe1
	.long	.LFB539
	.long	.LFE539-.LFB539
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI224-.LFB539
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI226-.LCFI224
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1419:
.LSFDE1421:
	.long	.LEFDE1421-.LASFDE1421
.LASFDE1421:
	.long	.LASFDE1421-.Lframe1
	.long	.LFB631
	.long	.LFE631-.LFB631
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI227-.LFB631
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1421:
.LSFDE1423:
	.long	.LEFDE1423-.LASFDE1423
.LASFDE1423:
	.long	.LASFDE1423-.Lframe1
	.long	.LFB626
	.long	.LFE626-.LFB626
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI228-.LFB626
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1423:
.LSFDE1425:
	.long	.LEFDE1425-.LASFDE1425
.LASFDE1425:
	.long	.LASFDE1425-.Lframe1
	.long	.LFB624
	.long	.LFE624-.LFB624
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI229-.LFB624
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1425:
.LSFDE1427:
	.long	.LEFDE1427-.LASFDE1427
.LASFDE1427:
	.long	.LASFDE1427-.Lframe1
	.long	.LFB623
	.long	.LFE623-.LFB623
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI230-.LFB623
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1427:
.LSFDE1429:
	.long	.LEFDE1429-.LASFDE1429
.LASFDE1429:
	.long	.LASFDE1429-.Lframe1
	.long	.LFB622
	.long	.LFE622-.LFB622
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI231-.LFB622
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1429:
.LSFDE1431:
	.long	.LEFDE1431-.LASFDE1431
.LASFDE1431:
	.long	.LASFDE1431-.Lframe1
	.long	.LFB621
	.long	.LFE621-.LFB621
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI232-.LFB621
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1431:
.LSFDE1433:
	.long	.LEFDE1433-.LASFDE1433
.LASFDE1433:
	.long	.LASFDE1433-.Lframe1
	.long	.LFB620
	.long	.LFE620-.LFB620
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI233-.LFB620
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1433:
.LSFDE1435:
	.long	.LEFDE1435-.LASFDE1435
.LASFDE1435:
	.long	.LASFDE1435-.Lframe1
	.long	.LFB600
	.long	.LFE600-.LFB600
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI234-.LFB600
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1435:
.LSFDE1437:
	.long	.LEFDE1437-.LASFDE1437
.LASFDE1437:
	.long	.LASFDE1437-.Lframe1
	.long	.LFB581
	.long	.LFE581-.LFB581
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI235-.LFB581
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1437:
.LSFDE1439:
	.long	.LEFDE1439-.LASFDE1439
.LASFDE1439:
	.long	.LASFDE1439-.Lframe1
	.long	.LFB562
	.long	.LFE562-.LFB562
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI236-.LFB562
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1439:
.LSFDE1441:
	.long	.LEFDE1441-.LASFDE1441
.LASFDE1441:
	.long	.LASFDE1441-.Lframe1
	.long	.LFB599
	.long	.LFE599-.LFB599
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI237-.LFB599
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1441:
.LSFDE1443:
	.long	.LEFDE1443-.LASFDE1443
.LASFDE1443:
	.long	.LASFDE1443-.Lframe1
	.long	.LFB585
	.long	.LFE585-.LFB585
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI238-.LFB585
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1443:
.LSFDE1445:
	.long	.LEFDE1445-.LASFDE1445
.LASFDE1445:
	.long	.LASFDE1445-.Lframe1
	.long	.LFB561
	.long	.LFE561-.LFB561
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI239-.LFB561
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1445:
.LSFDE1447:
	.long	.LEFDE1447-.LASFDE1447
.LASFDE1447:
	.long	.LASFDE1447-.Lframe1
	.long	.LFB558
	.long	.LFE558-.LFB558
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI240-.LFB558
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1447:
.LSFDE1449:
	.long	.LEFDE1449-.LASFDE1449
.LASFDE1449:
	.long	.LASFDE1449-.Lframe1
	.long	.LFB557
	.long	.LFE557-.LFB557
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI241-.LFB557
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1449:
.LSFDE1451:
	.long	.LEFDE1451-.LASFDE1451
.LASFDE1451:
	.long	.LASFDE1451-.Lframe1
	.long	.LFB544
	.long	.LFE544-.LFB544
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI242-.LFB544
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1451:
.LSFDE1453:
	.long	.LEFDE1453-.LASFDE1453
.LASFDE1453:
	.long	.LASFDE1453-.Lframe1
	.long	.LFB543
	.long	.LFE543-.LFB543
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI243-.LFB543
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1453:
.LSFDE1455:
	.long	.LEFDE1455-.LASFDE1455
.LASFDE1455:
	.long	.LASFDE1455-.Lframe1
	.long	.LFB638
	.long	.LFE638-.LFB638
	.uleb128 0x0
	.align 8
.LEFDE1455:
.LSFDE1457:
	.long	.LEFDE1457-.LASFDE1457
.LASFDE1457:
	.long	.LASFDE1457-.Lframe1
	.long	.LFB883
	.long	.LFE883-.LFB883
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI244-.LFB883
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1457:
.LSFDE1459:
	.long	.LEFDE1459-.LASFDE1459
.LASFDE1459:
	.long	.LASFDE1459-.Lframe1
	.long	.LFB882
	.long	.LFE882-.LFB882
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI245-.LFB882
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1459:
.LSFDE1461:
	.long	.LEFDE1461-.LASFDE1461
.LASFDE1461:
	.long	.LASFDE1461-.Lframe1
	.long	.LFB879
	.long	.LFE879-.LFB879
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI248-.LFB879
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1461:
.LSFDE1463:
	.long	.LEFDE1463-.LASFDE1463
.LASFDE1463:
	.long	.LASFDE1463-.Lframe1
	.long	.LFB878
	.long	.LFE878-.LFB878
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI251-.LFB878
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1463:
.LSFDE1465:
	.long	.LEFDE1465-.LASFDE1465
.LASFDE1465:
	.long	.LASFDE1465-.Lframe1
	.long	.LFB872
	.long	.LFE872-.LFB872
	.uleb128 0x0
	.align 8
.LEFDE1465:
.LSFDE1467:
	.long	.LEFDE1467-.LASFDE1467
.LASFDE1467:
	.long	.LASFDE1467-.Lframe1
	.long	.LFB857
	.long	.LFE857-.LFB857
	.uleb128 0x0
	.align 8
.LEFDE1467:
.LSFDE1469:
	.long	.LEFDE1469-.LASFDE1469
.LASFDE1469:
	.long	.LASFDE1469-.Lframe1
	.long	.LFB855
	.long	.LFE855-.LFB855
	.uleb128 0x0
	.align 8
.LEFDE1469:
.LSFDE1471:
	.long	.LEFDE1471-.LASFDE1471
.LASFDE1471:
	.long	.LASFDE1471-.Lframe1
	.long	.LFB852
	.long	.LFE852-.LFB852
	.uleb128 0x0
	.align 8
.LEFDE1471:
.LSFDE1473:
	.long	.LEFDE1473-.LASFDE1473
.LASFDE1473:
	.long	.LASFDE1473-.Lframe1
	.long	.LFB851
	.long	.LFE851-.LFB851
	.uleb128 0x0
	.align 8
.LEFDE1473:
.LSFDE1475:
	.long	.LEFDE1475-.LASFDE1475
.LASFDE1475:
	.long	.LASFDE1475-.Lframe1
	.long	.LFB849
	.long	.LFE849-.LFB849
	.uleb128 0x0
	.align 8
.LEFDE1475:
.LSFDE1477:
	.long	.LEFDE1477-.LASFDE1477
.LASFDE1477:
	.long	.LASFDE1477-.Lframe1
	.long	.LFB847
	.long	.LFE847-.LFB847
	.uleb128 0x0
	.align 8
.LEFDE1477:
.LSFDE1479:
	.long	.LEFDE1479-.LASFDE1479
.LASFDE1479:
	.long	.LASFDE1479-.Lframe1
	.long	.LFB845
	.long	.LFE845-.LFB845
	.uleb128 0x0
	.align 8
.LEFDE1479:
.LSFDE1481:
	.long	.LEFDE1481-.LASFDE1481
.LASFDE1481:
	.long	.LASFDE1481-.Lframe1
	.long	.LFB843
	.long	.LFE843-.LFB843
	.uleb128 0x0
	.align 8
.LEFDE1481:
.LSFDE1483:
	.long	.LEFDE1483-.LASFDE1483
.LASFDE1483:
	.long	.LASFDE1483-.Lframe1
	.long	.LFB841
	.long	.LFE841-.LFB841
	.uleb128 0x0
	.align 8
.LEFDE1483:
.LSFDE1485:
	.long	.LEFDE1485-.LASFDE1485
.LASFDE1485:
	.long	.LASFDE1485-.Lframe1
	.long	.LFB839
	.long	.LFE839-.LFB839
	.uleb128 0x0
	.align 8
.LEFDE1485:
.LSFDE1487:
	.long	.LEFDE1487-.LASFDE1487
.LASFDE1487:
	.long	.LASFDE1487-.Lframe1
	.long	.LFB766
	.long	.LFE766-.LFB766
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI252-.LFB766
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1487:
.LSFDE1489:
	.long	.LEFDE1489-.LASFDE1489
.LASFDE1489:
	.long	.LASFDE1489-.Lframe1
	.long	.LFB765
	.long	.LFE765-.LFB765
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI253-.LFB765
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1489:
.LSFDE1491:
	.long	.LEFDE1491-.LASFDE1491
.LASFDE1491:
	.long	.LASFDE1491-.Lframe1
	.long	.LFB762
	.long	.LFE762-.LFB762
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI256-.LFB762
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1491:
.LSFDE1493:
	.long	.LEFDE1493-.LASFDE1493
.LASFDE1493:
	.long	.LASFDE1493-.Lframe1
	.long	.LFB761
	.long	.LFE761-.LFB761
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI259-.LFB761
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1493:
.LSFDE1495:
	.long	.LEFDE1495-.LASFDE1495
.LASFDE1495:
	.long	.LASFDE1495-.Lframe1
	.long	.LFB755
	.long	.LFE755-.LFB755
	.uleb128 0x0
	.align 8
.LEFDE1495:
.LSFDE1497:
	.long	.LEFDE1497-.LASFDE1497
.LASFDE1497:
	.long	.LASFDE1497-.Lframe1
	.long	.LFB740
	.long	.LFE740-.LFB740
	.uleb128 0x0
	.align 8
.LEFDE1497:
.LSFDE1499:
	.long	.LEFDE1499-.LASFDE1499
.LASFDE1499:
	.long	.LASFDE1499-.Lframe1
	.long	.LFB739
	.long	.LFE739-.LFB739
	.uleb128 0x0
	.align 8
.LEFDE1499:
.LSFDE1501:
	.long	.LEFDE1501-.LASFDE1501
.LASFDE1501:
	.long	.LASFDE1501-.Lframe1
	.long	.LFB737
	.long	.LFE737-.LFB737
	.uleb128 0x0
	.align 8
.LEFDE1501:
.LSFDE1503:
	.long	.LEFDE1503-.LASFDE1503
.LASFDE1503:
	.long	.LASFDE1503-.Lframe1
	.long	.LFB736
	.long	.LFE736-.LFB736
	.uleb128 0x0
	.align 8
.LEFDE1503:
.LSFDE1505:
	.long	.LEFDE1505-.LASFDE1505
.LASFDE1505:
	.long	.LASFDE1505-.Lframe1
	.long	.LFB735
	.long	.LFE735-.LFB735
	.uleb128 0x0
	.align 8
.LEFDE1505:
.LSFDE1507:
	.long	.LEFDE1507-.LASFDE1507
.LASFDE1507:
	.long	.LASFDE1507-.Lframe1
	.long	.LFB733
	.long	.LFE733-.LFB733
	.uleb128 0x0
	.align 8
.LEFDE1507:
.LSFDE1509:
	.long	.LEFDE1509-.LASFDE1509
.LASFDE1509:
	.long	.LASFDE1509-.Lframe1
	.long	.LFB731
	.long	.LFE731-.LFB731
	.uleb128 0x0
	.align 8
.LEFDE1509:
.LSFDE1511:
	.long	.LEFDE1511-.LASFDE1511
.LASFDE1511:
	.long	.LASFDE1511-.Lframe1
	.long	.LFB729
	.long	.LFE729-.LFB729
	.uleb128 0x0
	.align 8
.LEFDE1511:
.LSFDE1513:
	.long	.LEFDE1513-.LASFDE1513
.LASFDE1513:
	.long	.LASFDE1513-.Lframe1
	.long	.LFB727
	.long	.LFE727-.LFB727
	.uleb128 0x0
	.align 8
.LEFDE1513:
.LSFDE1515:
	.long	.LEFDE1515-.LASFDE1515
.LASFDE1515:
	.long	.LASFDE1515-.Lframe1
	.long	.LFB725
	.long	.LFE725-.LFB725
	.uleb128 0x0
	.align 8
.LEFDE1515:
.LSFDE1517:
	.long	.LEFDE1517-.LASFDE1517
.LASFDE1517:
	.long	.LASFDE1517-.Lframe1
	.long	.LFB695
	.long	.LFE695-.LFB695
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI260-.LFB695
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI261-.LCFI260
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI262-.LCFI261
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1517:
.LSFDE1519:
	.long	.LEFDE1519-.LASFDE1519
.LASFDE1519:
	.long	.LASFDE1519-.Lframe1
	.long	.LFB686
	.long	.LFE686-.LFB686
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI263-.LFB686
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI264-.LCFI263
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI265-.LCFI264
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1519:
.LSFDE1521:
	.long	.LEFDE1521-.LASFDE1521
.LASFDE1521:
	.long	.LASFDE1521-.Lframe1
	.long	.LFB675
	.long	.LFE675-.LFB675
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI266-.LFB675
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1521:
.LSFDE1523:
	.long	.LEFDE1523-.LASFDE1523
.LASFDE1523:
	.long	.LASFDE1523-.Lframe1
	.long	.LFB674
	.long	.LFE674-.LFB674
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI267-.LFB674
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1523:
.LSFDE1525:
	.long	.LEFDE1525-.LASFDE1525
.LASFDE1525:
	.long	.LASFDE1525-.Lframe1
	.long	.LFB671
	.long	.LFE671-.LFB671
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI270-.LFB671
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1525:
.LSFDE1527:
	.long	.LEFDE1527-.LASFDE1527
.LASFDE1527:
	.long	.LASFDE1527-.Lframe1
	.long	.LFB670
	.long	.LFE670-.LFB670
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI273-.LFB670
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1527:
.LSFDE1529:
	.long	.LEFDE1529-.LASFDE1529
.LASFDE1529:
	.long	.LASFDE1529-.Lframe1
	.long	.LFB664
	.long	.LFE664-.LFB664
	.uleb128 0x0
	.align 8
.LEFDE1529:
.LSFDE1531:
	.long	.LEFDE1531-.LASFDE1531
.LASFDE1531:
	.long	.LASFDE1531-.Lframe1
	.long	.LFB653
	.long	.LFE653-.LFB653
	.uleb128 0x0
	.align 8
.LEFDE1531:
.LSFDE1533:
	.long	.LEFDE1533-.LASFDE1533
.LASFDE1533:
	.long	.LASFDE1533-.Lframe1
	.long	.LFB652
	.long	.LFE652-.LFB652
	.uleb128 0x0
	.align 8
.LEFDE1533:
.LSFDE1535:
	.long	.LEFDE1535-.LASFDE1535
.LASFDE1535:
	.long	.LASFDE1535-.Lframe1
	.long	.LFB650
	.long	.LFE650-.LFB650
	.uleb128 0x0
	.align 8
.LEFDE1535:
.LSFDE1537:
	.long	.LEFDE1537-.LASFDE1537
.LASFDE1537:
	.long	.LASFDE1537-.Lframe1
	.long	.LFB649
	.long	.LFE649-.LFB649
	.uleb128 0x0
	.align 8
.LEFDE1537:
.LSFDE1539:
	.long	.LEFDE1539-.LASFDE1539
.LASFDE1539:
	.long	.LASFDE1539-.Lframe1
	.long	.LFB648
	.long	.LFE648-.LFB648
	.uleb128 0x0
	.align 8
.LEFDE1539:
.LSFDE1541:
	.long	.LEFDE1541-.LASFDE1541
.LASFDE1541:
	.long	.LASFDE1541-.Lframe1
	.long	.LFB646
	.long	.LFE646-.LFB646
	.uleb128 0x0
	.align 8
.LEFDE1541:
.LSFDE1543:
	.long	.LEFDE1543-.LASFDE1543
.LASFDE1543:
	.long	.LASFDE1543-.Lframe1
	.long	.LFB644
	.long	.LFE644-.LFB644
	.uleb128 0x0
	.align 8
.LEFDE1543:
.LSFDE1545:
	.long	.LEFDE1545-.LASFDE1545
.LASFDE1545:
	.long	.LASFDE1545-.Lframe1
	.long	.LFB642
	.long	.LFE642-.LFB642
	.uleb128 0x0
	.align 8
.LEFDE1545:
.LSFDE1547:
	.long	.LEFDE1547-.LASFDE1547
.LASFDE1547:
	.long	.LASFDE1547-.Lframe1
	.long	.LFB640
	.long	.LFE640-.LFB640
	.uleb128 0x0
	.align 8
.LEFDE1547:
.LSFDE1549:
	.long	.LEFDE1549-.LASFDE1549
.LASFDE1549:
	.long	.LASFDE1549-.Lframe1
	.long	.LFB1376
	.long	.LFE1376-.LFB1376
	.uleb128 0x0
	.align 8
.LEFDE1549:
.LSFDE1551:
	.long	.LEFDE1551-.LASFDE1551
.LASFDE1551:
	.long	.LASFDE1551-.Lframe1
	.long	.LFB1238
	.long	.LFE1238-.LFB1238
	.uleb128 0x0
	.align 8
.LEFDE1551:
.LSFDE1553:
	.long	.LEFDE1553-.LASFDE1553
.LASFDE1553:
	.long	.LASFDE1553-.Lframe1
	.long	.LFB1236
	.long	.LFE1236-.LFB1236
	.uleb128 0x0
	.align 8
.LEFDE1553:
.LSFDE1555:
	.long	.LEFDE1555-.LASFDE1555
.LASFDE1555:
	.long	.LASFDE1555-.Lframe1
	.long	.LFB1210
	.long	.LFE1210-.LFB1210
	.uleb128 0x0
	.align 8
.LEFDE1555:
.LSFDE1557:
	.long	.LEFDE1557-.LASFDE1557
.LASFDE1557:
	.long	.LASFDE1557-.Lframe1
	.long	.LFB1173
	.long	.LFE1173-.LFB1173
	.uleb128 0x0
	.align 8
.LEFDE1557:
.LSFDE1559:
	.long	.LEFDE1559-.LASFDE1559
.LASFDE1559:
	.long	.LASFDE1559-.Lframe1
	.long	.LFB1171
	.long	.LFE1171-.LFB1171
	.uleb128 0x0
	.align 8
.LEFDE1559:
.LSFDE1561:
	.long	.LEFDE1561-.LASFDE1561
.LASFDE1561:
	.long	.LASFDE1561-.Lframe1
	.long	.LFB1375
	.long	.LFE1375-.LFB1375
	.uleb128 0x0
	.align 8
.LEFDE1561:
.LSFDE1563:
	.long	.LEFDE1563-.LASFDE1563
.LASFDE1563:
	.long	.LASFDE1563-.Lframe1
	.long	.LFB1239
	.long	.LFE1239-.LFB1239
	.uleb128 0x0
	.align 8
.LEFDE1563:
.LSFDE1565:
	.long	.LEFDE1565-.LASFDE1565
.LASFDE1565:
	.long	.LASFDE1565-.Lframe1
	.long	.LFB1237
	.long	.LFE1237-.LFB1237
	.uleb128 0x0
	.align 8
.LEFDE1565:
.LSFDE1567:
	.long	.LEFDE1567-.LASFDE1567
.LASFDE1567:
	.long	.LASFDE1567-.Lframe1
	.long	.LFB1209
	.long	.LFE1209-.LFB1209
	.uleb128 0x0
	.align 8
.LEFDE1567:
.LSFDE1569:
	.long	.LEFDE1569-.LASFDE1569
.LASFDE1569:
	.long	.LASFDE1569-.Lframe1
	.long	.LFB1174
	.long	.LFE1174-.LFB1174
	.uleb128 0x0
	.align 8
.LEFDE1569:
.LSFDE1571:
	.long	.LEFDE1571-.LASFDE1571
.LASFDE1571:
	.long	.LASFDE1571-.Lframe1
	.long	.LFB1172
	.long	.LFE1172-.LFB1172
	.uleb128 0x0
	.align 8
.LEFDE1571:
.LSFDE1573:
	.long	.LEFDE1573-.LASFDE1573
.LASFDE1573:
	.long	.LASFDE1573-.Lframe1
	.long	.LFB1377
	.long	.LFE1377-.LFB1377
	.uleb128 0x0
	.align 8
.LEFDE1573:
.LSFDE1575:
	.long	.LEFDE1575-.LASFDE1575
.LASFDE1575:
	.long	.LASFDE1575-.Lframe1
	.long	.LFB1243
	.long	.LFE1243-.LFB1243
	.uleb128 0x0
	.align 8
.LEFDE1575:
.LSFDE1577:
	.long	.LEFDE1577-.LASFDE1577
.LASFDE1577:
	.long	.LASFDE1577-.Lframe1
	.long	.LFB1241
	.long	.LFE1241-.LFB1241
	.uleb128 0x0
	.align 8
.LEFDE1577:
.LSFDE1579:
	.long	.LEFDE1579-.LASFDE1579
.LASFDE1579:
	.long	.LASFDE1579-.Lframe1
	.long	.LFB1211
	.long	.LFE1211-.LFB1211
	.uleb128 0x0
	.align 8
.LEFDE1579:
.LSFDE1581:
	.long	.LEFDE1581-.LASFDE1581
.LASFDE1581:
	.long	.LASFDE1581-.Lframe1
	.long	.LFB1178
	.long	.LFE1178-.LFB1178
	.uleb128 0x0
	.align 8
.LEFDE1581:
.LSFDE1583:
	.long	.LEFDE1583-.LASFDE1583
.LASFDE1583:
	.long	.LASFDE1583-.Lframe1
	.long	.LFB1176
	.long	.LFE1176-.LFB1176
	.uleb128 0x0
	.align 8
.LEFDE1583:
.LSFDE1585:
	.long	.LEFDE1585-.LASFDE1585
.LASFDE1585:
	.long	.LASFDE1585-.Lframe1
	.long	.LFB1265
	.long	.LFE1265-.LFB1265
	.uleb128 0x0
	.align 8
.LEFDE1585:
.LSFDE1587:
	.long	.LEFDE1587-.LASFDE1587
.LASFDE1587:
	.long	.LASFDE1587-.Lframe1
	.long	.LFB1201
	.long	.LFE1201-.LFB1201
	.uleb128 0x0
	.align 8
.LEFDE1587:
.LSFDE1589:
	.long	.LEFDE1589-.LASFDE1589
.LASFDE1589:
	.long	.LASFDE1589-.Lframe1
	.long	.LFB1242
	.long	.LFE1242-.LFB1242
	.uleb128 0x0
	.align 8
.LEFDE1589:
.LSFDE1591:
	.long	.LEFDE1591-.LASFDE1591
.LASFDE1591:
	.long	.LASFDE1591-.Lframe1
	.long	.LFB1175
	.long	.LFE1175-.LFB1175
	.uleb128 0x0
	.align 8
.LEFDE1591:
.LSFDE1593:
	.long	.LEFDE1593-.LASFDE1593
.LASFDE1593:
	.long	.LASFDE1593-.Lframe1
	.long	.LFB1240
	.long	.LFE1240-.LFB1240
	.uleb128 0x0
	.align 8
.LEFDE1593:
.LSFDE1595:
	.long	.LEFDE1595-.LASFDE1595
.LASFDE1595:
	.long	.LASFDE1595-.Lframe1
	.long	.LFB1177
	.long	.LFE1177-.LFB1177
	.uleb128 0x0
	.align 8
.LEFDE1595:
.LSFDE1597:
	.long	.LEFDE1597-.LASFDE1597
.LASFDE1597:
	.long	.LASFDE1597-.Lframe1
	.long	.LFB1351
	.long	.LFE1351-.LFB1351
	.uleb128 0x0
	.align 8
.LEFDE1597:
.LSFDE1599:
	.long	.LEFDE1599-.LASFDE1599
.LASFDE1599:
	.long	.LASFDE1599-.Lframe1
	.long	.LFB1350
	.long	.LFE1350-.LFB1350
	.uleb128 0x0
	.align 8
.LEFDE1599:
.LSFDE1601:
	.long	.LEFDE1601-.LASFDE1601
.LASFDE1601:
	.long	.LASFDE1601-.Lframe1
	.long	.LFB1335
	.long	.LFE1335-.LFB1335
	.uleb128 0x0
	.align 8
.LEFDE1601:
.LSFDE1603:
	.long	.LEFDE1603-.LASFDE1603
.LASFDE1603:
	.long	.LASFDE1603-.Lframe1
	.long	.LFB1334
	.long	.LFE1334-.LFB1334
	.uleb128 0x0
	.align 8
.LEFDE1603:
.LSFDE1605:
	.long	.LEFDE1605-.LASFDE1605
.LASFDE1605:
	.long	.LASFDE1605-.Lframe1
	.long	.LFB1359
	.long	.LFE1359-.LFB1359
	.uleb128 0x0
	.align 8
.LEFDE1605:
.LSFDE1607:
	.long	.LEFDE1607-.LASFDE1607
.LASFDE1607:
	.long	.LASFDE1607-.Lframe1
	.long	.LFB1358
	.long	.LFE1358-.LFB1358
	.uleb128 0x0
	.align 8
.LEFDE1607:
.LSFDE1609:
	.long	.LEFDE1609-.LASFDE1609
.LASFDE1609:
	.long	.LASFDE1609-.Lframe1
	.long	.LFB1343
	.long	.LFE1343-.LFB1343
	.uleb128 0x0
	.align 8
.LEFDE1609:
.LSFDE1611:
	.long	.LEFDE1611-.LASFDE1611
.LASFDE1611:
	.long	.LASFDE1611-.Lframe1
	.long	.LFB1342
	.long	.LFE1342-.LFB1342
	.uleb128 0x0
	.align 8
.LEFDE1611:
.LSFDE1613:
	.long	.LEFDE1613-.LASFDE1613
.LASFDE1613:
	.long	.LASFDE1613-.Lframe1
	.long	.LFB1355
	.long	.LFE1355-.LFB1355
	.uleb128 0x0
	.align 8
.LEFDE1613:
.LSFDE1615:
	.long	.LEFDE1615-.LASFDE1615
.LASFDE1615:
	.long	.LASFDE1615-.Lframe1
	.long	.LFB1354
	.long	.LFE1354-.LFB1354
	.uleb128 0x0
	.align 8
.LEFDE1615:
.LSFDE1617:
	.long	.LEFDE1617-.LASFDE1617
.LASFDE1617:
	.long	.LASFDE1617-.Lframe1
	.long	.LFB1339
	.long	.LFE1339-.LFB1339
	.uleb128 0x0
	.align 8
.LEFDE1617:
.LSFDE1619:
	.long	.LEFDE1619-.LASFDE1619
.LASFDE1619:
	.long	.LASFDE1619-.Lframe1
	.long	.LFB1338
	.long	.LFE1338-.LFB1338
	.uleb128 0x0
	.align 8
.LEFDE1619:
.LSFDE1621:
	.long	.LEFDE1621-.LASFDE1621
.LASFDE1621:
	.long	.LASFDE1621-.Lframe1
	.long	.LFB1363
	.long	.LFE1363-.LFB1363
	.uleb128 0x0
	.align 8
.LEFDE1621:
.LSFDE1623:
	.long	.LEFDE1623-.LASFDE1623
.LASFDE1623:
	.long	.LASFDE1623-.Lframe1
	.long	.LFB1362
	.long	.LFE1362-.LFB1362
	.uleb128 0x0
	.align 8
.LEFDE1623:
.LSFDE1625:
	.long	.LEFDE1625-.LASFDE1625
.LASFDE1625:
	.long	.LASFDE1625-.Lframe1
	.long	.LFB1347
	.long	.LFE1347-.LFB1347
	.uleb128 0x0
	.align 8
.LEFDE1625:
.LSFDE1627:
	.long	.LEFDE1627-.LASFDE1627
.LASFDE1627:
	.long	.LASFDE1627-.Lframe1
	.long	.LFB1346
	.long	.LFE1346-.LFB1346
	.uleb128 0x0
	.align 8
.LEFDE1627:
.LSFDE1629:
	.long	.LEFDE1629-.LASFDE1629
.LASFDE1629:
	.long	.LASFDE1629-.Lframe1
	.long	.LFB1353
	.long	.LFE1353-.LFB1353
	.uleb128 0x0
	.align 8
.LEFDE1629:
.LSFDE1631:
	.long	.LEFDE1631-.LASFDE1631
.LASFDE1631:
	.long	.LASFDE1631-.Lframe1
	.long	.LFB1352
	.long	.LFE1352-.LFB1352
	.uleb128 0x0
	.align 8
.LEFDE1631:
.LSFDE1633:
	.long	.LEFDE1633-.LASFDE1633
.LASFDE1633:
	.long	.LASFDE1633-.Lframe1
	.long	.LFB1337
	.long	.LFE1337-.LFB1337
	.uleb128 0x0
	.align 8
.LEFDE1633:
.LSFDE1635:
	.long	.LEFDE1635-.LASFDE1635
.LASFDE1635:
	.long	.LASFDE1635-.Lframe1
	.long	.LFB1336
	.long	.LFE1336-.LFB1336
	.uleb128 0x0
	.align 8
.LEFDE1635:
.LSFDE1637:
	.long	.LEFDE1637-.LASFDE1637
.LASFDE1637:
	.long	.LASFDE1637-.Lframe1
	.long	.LFB1361
	.long	.LFE1361-.LFB1361
	.uleb128 0x0
	.align 8
.LEFDE1637:
.LSFDE1639:
	.long	.LEFDE1639-.LASFDE1639
.LASFDE1639:
	.long	.LASFDE1639-.Lframe1
	.long	.LFB1360
	.long	.LFE1360-.LFB1360
	.uleb128 0x0
	.align 8
.LEFDE1639:
.LSFDE1641:
	.long	.LEFDE1641-.LASFDE1641
.LASFDE1641:
	.long	.LASFDE1641-.Lframe1
	.long	.LFB1345
	.long	.LFE1345-.LFB1345
	.uleb128 0x0
	.align 8
.LEFDE1641:
.LSFDE1643:
	.long	.LEFDE1643-.LASFDE1643
.LASFDE1643:
	.long	.LASFDE1643-.Lframe1
	.long	.LFB1344
	.long	.LFE1344-.LFB1344
	.uleb128 0x0
	.align 8
.LEFDE1643:
.LSFDE1645:
	.long	.LEFDE1645-.LASFDE1645
.LASFDE1645:
	.long	.LASFDE1645-.Lframe1
	.long	.LFB1357
	.long	.LFE1357-.LFB1357
	.uleb128 0x0
	.align 8
.LEFDE1645:
.LSFDE1647:
	.long	.LEFDE1647-.LASFDE1647
.LASFDE1647:
	.long	.LASFDE1647-.Lframe1
	.long	.LFB1356
	.long	.LFE1356-.LFB1356
	.uleb128 0x0
	.align 8
.LEFDE1647:
.LSFDE1649:
	.long	.LEFDE1649-.LASFDE1649
.LASFDE1649:
	.long	.LASFDE1649-.Lframe1
	.long	.LFB1341
	.long	.LFE1341-.LFB1341
	.uleb128 0x0
	.align 8
.LEFDE1649:
.LSFDE1651:
	.long	.LEFDE1651-.LASFDE1651
.LASFDE1651:
	.long	.LASFDE1651-.Lframe1
	.long	.LFB1340
	.long	.LFE1340-.LFB1340
	.uleb128 0x0
	.align 8
.LEFDE1651:
.LSFDE1653:
	.long	.LEFDE1653-.LASFDE1653
.LASFDE1653:
	.long	.LASFDE1653-.Lframe1
	.long	.LFB1365
	.long	.LFE1365-.LFB1365
	.uleb128 0x0
	.align 8
.LEFDE1653:
.LSFDE1655:
	.long	.LEFDE1655-.LASFDE1655
.LASFDE1655:
	.long	.LASFDE1655-.Lframe1
	.long	.LFB1364
	.long	.LFE1364-.LFB1364
	.uleb128 0x0
	.align 8
.LEFDE1655:
.LSFDE1657:
	.long	.LEFDE1657-.LASFDE1657
.LASFDE1657:
	.long	.LASFDE1657-.Lframe1
	.long	.LFB1349
	.long	.LFE1349-.LFB1349
	.uleb128 0x0
	.align 8
.LEFDE1657:
.LSFDE1659:
	.long	.LEFDE1659-.LASFDE1659
.LASFDE1659:
	.long	.LASFDE1659-.Lframe1
	.long	.LFB1348
	.long	.LFE1348-.LFB1348
	.uleb128 0x0
	.align 8
.LEFDE1659:
.LSFDE1661:
	.long	.LEFDE1661-.LASFDE1661
.LASFDE1661:
	.long	.LASFDE1661-.Lframe1
	.long	.LFB1275
	.long	.LFE1275-.LFB1275
	.uleb128 0x0
	.align 8
.LEFDE1661:
.LSFDE1663:
	.long	.LEFDE1663-.LASFDE1663
.LASFDE1663:
	.long	.LASFDE1663-.Lframe1
	.long	.LFB1274
	.long	.LFE1274-.LFB1274
	.uleb128 0x0
	.align 8
.LEFDE1663:
.LSFDE1665:
	.long	.LEFDE1665-.LASFDE1665
.LASFDE1665:
	.long	.LASFDE1665-.Lframe1
	.long	.LFB1279
	.long	.LFE1279-.LFB1279
	.uleb128 0x0
	.align 8
.LEFDE1665:
.LSFDE1667:
	.long	.LEFDE1667-.LASFDE1667
.LASFDE1667:
	.long	.LASFDE1667-.Lframe1
	.long	.LFB1278
	.long	.LFE1278-.LFB1278
	.uleb128 0x0
	.align 8
.LEFDE1667:
.LSFDE1669:
	.long	.LEFDE1669-.LASFDE1669
.LASFDE1669:
	.long	.LASFDE1669-.Lframe1
	.long	.LFB1283
	.long	.LFE1283-.LFB1283
	.uleb128 0x0
	.align 8
.LEFDE1669:
.LSFDE1671:
	.long	.LEFDE1671-.LASFDE1671
.LASFDE1671:
	.long	.LASFDE1671-.Lframe1
	.long	.LFB1282
	.long	.LFE1282-.LFB1282
	.uleb128 0x0
	.align 8
.LEFDE1671:
.LSFDE1673:
	.long	.LEFDE1673-.LASFDE1673
.LASFDE1673:
	.long	.LASFDE1673-.Lframe1
	.long	.LFB1277
	.long	.LFE1277-.LFB1277
	.uleb128 0x0
	.align 8
.LEFDE1673:
.LSFDE1675:
	.long	.LEFDE1675-.LASFDE1675
.LASFDE1675:
	.long	.LASFDE1675-.Lframe1
	.long	.LFB1276
	.long	.LFE1276-.LFB1276
	.uleb128 0x0
	.align 8
.LEFDE1675:
.LSFDE1677:
	.long	.LEFDE1677-.LASFDE1677
.LASFDE1677:
	.long	.LASFDE1677-.Lframe1
	.long	.LFB1281
	.long	.LFE1281-.LFB1281
	.uleb128 0x0
	.align 8
.LEFDE1677:
.LSFDE1679:
	.long	.LEFDE1679-.LASFDE1679
.LASFDE1679:
	.long	.LASFDE1679-.Lframe1
	.long	.LFB1280
	.long	.LFE1280-.LFB1280
	.uleb128 0x0
	.align 8
.LEFDE1679:
.LSFDE1681:
	.long	.LEFDE1681-.LASFDE1681
.LASFDE1681:
	.long	.LASFDE1681-.Lframe1
	.long	.LFB1285
	.long	.LFE1285-.LFB1285
	.uleb128 0x0
	.align 8
.LEFDE1681:
.LSFDE1683:
	.long	.LEFDE1683-.LASFDE1683
.LASFDE1683:
	.long	.LASFDE1683-.Lframe1
	.long	.LFB1284
	.long	.LFE1284-.LFB1284
	.uleb128 0x0
	.align 8
.LEFDE1683:
.LSFDE1685:
	.long	.LEFDE1685-.LASFDE1685
.LASFDE1685:
	.long	.LASFDE1685-.Lframe1
	.long	.LFB1287
	.long	.LFE1287-.LFB1287
	.uleb128 0x0
	.align 8
.LEFDE1685:
.LSFDE1687:
	.long	.LEFDE1687-.LASFDE1687
.LASFDE1687:
	.long	.LASFDE1687-.Lframe1
	.long	.LFB1286
	.long	.LFE1286-.LFB1286
	.uleb128 0x0
	.align 8
.LEFDE1687:
.LSFDE1689:
	.long	.LEFDE1689-.LASFDE1689
.LASFDE1689:
	.long	.LASFDE1689-.Lframe1
	.long	.LFB909
	.long	.LFE909-.LFB909
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI276-.LFB909
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1689:
.LSFDE1691:
	.long	.LEFDE1691-.LASFDE1691
.LASFDE1691:
	.long	.LASFDE1691-.Lframe1
	.long	.LFB908
	.long	.LFE908-.LFB908
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI277-.LFB908
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1691:
.LSFDE1693:
	.long	.LEFDE1693-.LASFDE1693
.LASFDE1693:
	.long	.LASFDE1693-.Lframe1
	.long	.LFB907
	.long	.LFE907-.LFB907
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI278-.LFB907
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1693:
.LSFDE1695:
	.long	.LEFDE1695-.LASFDE1695
.LASFDE1695:
	.long	.LASFDE1695-.Lframe1
	.long	.LFB906
	.long	.LFE906-.LFB906
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI281-.LFB906
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1695:
.LSFDE1697:
	.long	.LEFDE1697-.LASFDE1697
.LASFDE1697:
	.long	.LASFDE1697-.Lframe1
	.long	.LFB905
	.long	.LFE905-.LFB905
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI282-.LFB905
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI284-.LCFI282
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1697:
.LSFDE1699:
	.long	.LEFDE1699-.LASFDE1699
.LASFDE1699:
	.long	.LASFDE1699-.Lframe1
	.long	.LFB904
	.long	.LFE904-.LFB904
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI287-.LFB904
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1699:
.LSFDE1701:
	.long	.LEFDE1701-.LASFDE1701
.LASFDE1701:
	.long	.LASFDE1701-.Lframe1
	.long	.LFB903
	.long	.LFE903-.LFB903
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI288-.LFB903
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI290-.LCFI288
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1701:
.LSFDE1703:
	.long	.LEFDE1703-.LASFDE1703
.LASFDE1703:
	.long	.LASFDE1703-.Lframe1
	.long	.LFB902
	.long	.LFE902-.LFB902
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI291-.LFB902
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI293-.LCFI291
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1703:
.LSFDE1705:
	.long	.LEFDE1705-.LASFDE1705
.LASFDE1705:
	.long	.LASFDE1705-.Lframe1
	.long	.LFB901
	.long	.LFE901-.LFB901
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI294-.LFB901
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI296-.LCFI294
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1705:
.LSFDE1707:
	.long	.LEFDE1707-.LASFDE1707
.LASFDE1707:
	.long	.LASFDE1707-.Lframe1
	.long	.LFB900
	.long	.LFE900-.LFB900
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI297-.LFB900
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI299-.LCFI297
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1707:
.LSFDE1709:
	.long	.LEFDE1709-.LASFDE1709
.LASFDE1709:
	.long	.LASFDE1709-.Lframe1
	.long	.LFB899
	.long	.LFE899-.LFB899
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI302-.LFB899
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1709:
.LSFDE1711:
	.long	.LEFDE1711-.LASFDE1711
.LASFDE1711:
	.long	.LASFDE1711-.Lframe1
	.long	.LFB898
	.long	.LFE898-.LFB898
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI305-.LFB898
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1711:
.LSFDE1713:
	.long	.LEFDE1713-.LASFDE1713
.LASFDE1713:
	.long	.LASFDE1713-.Lframe1
	.long	.LFB897
	.long	.LFE897-.LFB897
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI306-.LFB897
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1713:
.LSFDE1715:
	.long	.LEFDE1715-.LASFDE1715
.LASFDE1715:
	.long	.LASFDE1715-.Lframe1
	.long	.LFB896
	.long	.LFE896-.LFB896
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI307-.LFB896
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1715:
.LSFDE1717:
	.long	.LEFDE1717-.LASFDE1717
.LASFDE1717:
	.long	.LASFDE1717-.Lframe1
	.long	.LFB895
	.long	.LFE895-.LFB895
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI310-.LFB895
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1717:
.LSFDE1719:
	.long	.LEFDE1719-.LASFDE1719
.LASFDE1719:
	.long	.LASFDE1719-.Lframe1
	.long	.LFB894
	.long	.LFE894-.LFB894
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI313-.LFB894
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1719:
.LSFDE1721:
	.long	.LEFDE1721-.LASFDE1721
.LASFDE1721:
	.long	.LASFDE1721-.Lframe1
	.long	.LFB575
	.long	.LFE575-.LFB575
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI314-.LFB575
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1721:
.LSFDE1723:
	.long	.LEFDE1723-.LASFDE1723
.LASFDE1723:
	.long	.LASFDE1723-.Lframe1
	.long	.LFB1040
	.long	.LFE1040-.LFB1040
	.uleb128 0x0
	.align 8
.LEFDE1723:
.LSFDE1725:
	.long	.LEFDE1725-.LASFDE1725
.LASFDE1725:
	.long	.LASFDE1725-.Lframe1
	.long	.LFB1039
	.long	.LFE1039-.LFB1039
	.uleb128 0x0
	.align 8
.LEFDE1725:
.LSFDE1727:
	.long	.LEFDE1727-.LASFDE1727
.LASFDE1727:
	.long	.LASFDE1727-.Lframe1
	.long	.LFB1044
	.long	.LFE1044-.LFB1044
	.uleb128 0x0
	.align 8
.LEFDE1727:
.LSFDE1729:
	.long	.LEFDE1729-.LASFDE1729
.LASFDE1729:
	.long	.LASFDE1729-.Lframe1
	.long	.LFB1043
	.long	.LFE1043-.LFB1043
	.uleb128 0x0
	.align 8
.LEFDE1729:
.LSFDE1731:
	.long	.LEFDE1731-.LASFDE1731
.LASFDE1731:
	.long	.LASFDE1731-.Lframe1
	.long	.LFB1042
	.long	.LFE1042-.LFB1042
	.uleb128 0x0
	.align 8
.LEFDE1731:
.LSFDE1733:
	.long	.LEFDE1733-.LASFDE1733
.LASFDE1733:
	.long	.LASFDE1733-.Lframe1
	.long	.LFB1289
	.long	.LFE1289-.LFB1289
	.uleb128 0x0
	.align 8
.LEFDE1733:
.LSFDE1735:
	.long	.LEFDE1735-.LASFDE1735
.LASFDE1735:
	.long	.LASFDE1735-.Lframe1
	.long	.LFB1288
	.long	.LFE1288-.LFB1288
	.uleb128 0x0
	.align 8
.LEFDE1735:
.LSFDE1737:
	.long	.LEFDE1737-.LASFDE1737
.LASFDE1737:
	.long	.LASFDE1737-.Lframe1
	.long	.LFB777
	.long	.LFE777-.LFB777
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI317-.LFB777
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1737:
.LSFDE1739:
	.long	.LEFDE1739-.LASFDE1739
.LASFDE1739:
	.long	.LASFDE1739-.Lframe1
	.long	.LFB574
	.long	.LFE574-.LFB574
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI318-.LFB574
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1739:
.LSFDE1741:
	.long	.LEFDE1741-.LASFDE1741
.LASFDE1741:
	.long	.LASFDE1741-.Lframe1
	.long	.LFB572
	.long	.LFE572-.LFB572
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI319-.LFB572
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1741:
.LSFDE1743:
	.long	.LEFDE1743-.LASFDE1743
.LASFDE1743:
	.long	.LASFDE1743-.Lframe1
	.long	.LFB785
	.long	.LFE785-.LFB785
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI320-.LFB785
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI322-.LCFI320
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1743:
.LSFDE1745:
	.long	.LEFDE1745-.LASFDE1745
.LASFDE1745:
	.long	.LASFDE1745-.Lframe1
	.long	.LFB784
	.long	.LFE784-.LFB784
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI323-.LFB784
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI325-.LCFI323
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1745:
.LSFDE1747:
	.long	.LEFDE1747-.LASFDE1747
.LASFDE1747:
	.long	.LASFDE1747-.Lframe1
	.long	.LFB783
	.long	.LFE783-.LFB783
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI326-.LFB783
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI328-.LCFI326
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1747:
.LSFDE1749:
	.long	.LEFDE1749-.LASFDE1749
.LASFDE1749:
	.long	.LASFDE1749-.Lframe1
	.long	.LFB782
	.long	.LFE782-.LFB782
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI329-.LFB782
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI330-.LCFI329
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI331-.LCFI330
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1749:
.LSFDE1751:
	.long	.LEFDE1751-.LASFDE1751
.LASFDE1751:
	.long	.LASFDE1751-.Lframe1
	.long	.LFB781
	.long	.LFE781-.LFB781
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI332-.LFB781
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI333-.LCFI332
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI334-.LCFI333
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1751:
.LSFDE1753:
	.long	.LEFDE1753-.LASFDE1753
.LASFDE1753:
	.long	.LASFDE1753-.Lframe1
	.long	.LFB780
	.long	.LFE780-.LFB780
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI335-.LFB780
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1753:
.LSFDE1755:
	.long	.LEFDE1755-.LASFDE1755
.LASFDE1755:
	.long	.LASFDE1755-.Lframe1
	.long	.LFB779
	.long	.LFE779-.LFB779
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI336-.LFB779
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1755:
.LSFDE1757:
	.long	.LEFDE1757-.LASFDE1757
.LASFDE1757:
	.long	.LASFDE1757-.Lframe1
	.long	.LFB778
	.long	.LFE778-.LFB778
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI339-.LFB778
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1757:
.LSFDE1759:
	.long	.LEFDE1759-.LASFDE1759
.LASFDE1759:
	.long	.LASFDE1759-.Lframe1
	.long	.LFB792
	.long	.LFE792-.LFB792
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI342-.LFB792
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1759:
.LSFDE1761:
	.long	.LEFDE1761-.LASFDE1761
.LASFDE1761:
	.long	.LASFDE1761-.Lframe1
	.long	.LFB791
	.long	.LFE791-.LFB791
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI343-.LFB791
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1761:
.LSFDE1763:
	.long	.LEFDE1763-.LASFDE1763
.LASFDE1763:
	.long	.LASFDE1763-.Lframe1
	.long	.LFB790
	.long	.LFE790-.LFB790
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI344-.LFB790
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1763:
.LSFDE1765:
	.long	.LEFDE1765-.LASFDE1765
.LASFDE1765:
	.long	.LASFDE1765-.Lframe1
	.long	.LFB789
	.long	.LFE789-.LFB789
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI347-.LFB789
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1765:
.LSFDE1767:
	.long	.LEFDE1767-.LASFDE1767
.LASFDE1767:
	.long	.LASFDE1767-.Lframe1
	.long	.LFB788
	.long	.LFE788-.LFB788
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI348-.LFB788
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI349-.LCFI348
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI350-.LCFI349
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1767:
.LSFDE1769:
	.long	.LEFDE1769-.LASFDE1769
.LASFDE1769:
	.long	.LASFDE1769-.Lframe1
	.long	.LFB787
	.long	.LFE787-.LFB787
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI353-.LFB787
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1769:
.LSFDE1771:
	.long	.LEFDE1771-.LASFDE1771
.LASFDE1771:
	.long	.LASFDE1771-.Lframe1
	.long	.LFB685
	.long	.LFE685-.LFB685
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI354-.LFB685
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1771:
.LSFDE1773:
	.long	.LEFDE1773-.LASFDE1773
.LASFDE1773:
	.long	.LASFDE1773-.Lframe1
	.long	.LFB684
	.long	.LFE684-.LFB684
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI355-.LFB684
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1773:
.LSFDE1775:
	.long	.LEFDE1775-.LASFDE1775
.LASFDE1775:
	.long	.LASFDE1775-.Lframe1
	.long	.LFB683
	.long	.LFE683-.LFB683
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI358-.LFB683
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1775:
.LSFDE1777:
	.long	.LEFDE1777-.LASFDE1777
.LASFDE1777:
	.long	.LASFDE1777-.Lframe1
	.long	.LFB682
	.long	.LFE682-.LFB682
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI361-.LFB682
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1777:
.LSFDE1779:
	.long	.LEFDE1779-.LASFDE1779
.LASFDE1779:
	.long	.LASFDE1779-.Lframe1
	.long	.LFB571
	.long	.LFE571-.LFB571
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI362-.LFB571
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1779:
.LSFDE1781:
	.long	.LEFDE1781-.LASFDE1781
.LASFDE1781:
	.long	.LASFDE1781-.Lframe1
	.long	.LFB693
	.long	.LFE693-.LFB693
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI365-.LFB693
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1781:
.LSFDE1783:
	.long	.LEFDE1783-.LASFDE1783
.LASFDE1783:
	.long	.LASFDE1783-.Lframe1
	.long	.LFB692
	.long	.LFE692-.LFB692
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI366-.LFB692
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1783:
.LSFDE1785:
	.long	.LEFDE1785-.LASFDE1785
.LASFDE1785:
	.long	.LASFDE1785-.Lframe1
	.long	.LFB691
	.long	.LFE691-.LFB691
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI367-.LFB691
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1785:
.LSFDE1787:
	.long	.LEFDE1787-.LASFDE1787
.LASFDE1787:
	.long	.LASFDE1787-.Lframe1
	.long	.LFB690
	.long	.LFE690-.LFB690
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI368-.LFB690
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI370-.LCFI368
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1787:
.LSFDE1789:
	.long	.LEFDE1789-.LASFDE1789
.LASFDE1789:
	.long	.LASFDE1789-.Lframe1
	.long	.LFB689
	.long	.LFE689-.LFB689
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI371-.LFB689
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI373-.LCFI371
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1789:
.LSFDE1791:
	.long	.LEFDE1791-.LASFDE1791
.LASFDE1791:
	.long	.LASFDE1791-.Lframe1
	.long	.LFB688
	.long	.LFE688-.LFB688
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI374-.LFB688
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI376-.LCFI374
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1791:
.LSFDE1793:
	.long	.LEFDE1793-.LASFDE1793
.LASFDE1793:
	.long	.LASFDE1793-.Lframe1
	.long	.LFB687
	.long	.LFE687-.LFB687
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI377-.LFB687
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI378-.LCFI377
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI379-.LCFI378
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1793:
.LSFDE1795:
	.long	.LEFDE1795-.LASFDE1795
.LASFDE1795:
	.long	.LASFDE1795-.Lframe1
	.long	.LFB579
	.long	.LFE579-.LFB579
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI380-.LFB579
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1795:
.LSFDE1797:
	.long	.LEFDE1797-.LASFDE1797
.LASFDE1797:
	.long	.LASFDE1797-.Lframe1
	.long	.LFB578
	.long	.LFE578-.LFB578
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI381-.LFB578
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1797:
.LSFDE1799:
	.long	.LEFDE1799-.LASFDE1799
.LASFDE1799:
	.long	.LASFDE1799-.Lframe1
	.long	.LFB563
	.long	.LFE563-.LFB563
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI382-.LFB563
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1799:
.LSFDE1801:
	.long	.LEFDE1801-.LASFDE1801
.LASFDE1801:
	.long	.LASFDE1801-.Lframe1
	.long	.LFB569
	.long	.LFE569-.LFB569
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI383-.LFB569
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1801:
.LSFDE1803:
	.long	.LEFDE1803-.LASFDE1803
.LASFDE1803:
	.long	.LASFDE1803-.Lframe1
	.long	.LFB565
	.long	.LFE565-.LFB565
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI384-.LFB565
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1803:
.LSFDE1805:
	.long	.LEFDE1805-.LASFDE1805
.LASFDE1805:
	.long	.LASFDE1805-.Lframe1
	.long	.LFB566
	.long	.LFE566-.LFB566
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI385-.LFB566
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1805:
.LSFDE1807:
	.long	.LEFDE1807-.LASFDE1807
.LASFDE1807:
	.long	.LASFDE1807-.Lframe1
	.long	.LFB567
	.long	.LFE567-.LFB567
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI386-.LFB567
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1807:
.LSFDE1809:
	.long	.LEFDE1809-.LASFDE1809
.LASFDE1809:
	.long	.LASFDE1809-.Lframe1
	.long	.LFB1061
	.long	.LFE1061-.LFB1061
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI387-.LFB1061
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI388-.LCFI387
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI389-.LCFI388
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1809:
.LSFDE1811:
	.long	.LEFDE1811-.LASFDE1811
.LASFDE1811:
	.long	.LASFDE1811-.Lframe1
	.long	.LFB1060
	.long	.LFE1060-.LFB1060
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI390-.LFB1060
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI391-.LCFI390
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI392-.LCFI391
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1811:
.LSFDE1813:
	.long	.LEFDE1813-.LASFDE1813
.LASFDE1813:
	.long	.LASFDE1813-.Lframe1
	.long	.LFB580
	.long	.LFE580-.LFB580
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI393-.LFB580
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1813:
.LSFDE1815:
	.long	.LEFDE1815-.LASFDE1815
.LASFDE1815:
	.long	.LASFDE1815-.Lframe1
	.long	.LFB586
	.long	.LFE586-.LFB586
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI394-.LFB586
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1815:
.LSFDE1817:
	.long	.LEFDE1817-.LASFDE1817
.LASFDE1817:
	.long	.LASFDE1817-.Lframe1
	.long	.LFB606
	.long	.LFE606-.LFB606
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI395-.LFB606
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1817:
.LSFDE1819:
	.long	.LEFDE1819-.LASFDE1819
.LASFDE1819:
	.long	.LASFDE1819-.Lframe1
	.long	.LFB607
	.long	.LFE607-.LFB607
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI396-.LFB607
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1819:
.LSFDE1821:
	.long	.LEFDE1821-.LASFDE1821
.LASFDE1821:
	.long	.LASFDE1821-.Lframe1
	.long	.LFB608
	.long	.LFE608-.LFB608
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI397-.LFB608
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1821:
.LSFDE1823:
	.long	.LEFDE1823-.LASFDE1823
.LASFDE1823:
	.long	.LASFDE1823-.Lframe1
	.long	.LFB587
	.long	.LFE587-.LFB587
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI398-.LFB587
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1823:
.LSFDE1825:
	.long	.LEFDE1825-.LASFDE1825
.LASFDE1825:
	.long	.LASFDE1825-.Lframe1
	.long	.LFB588
	.long	.LFE588-.LFB588
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI399-.LFB588
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1825:
.LSFDE1827:
	.long	.LEFDE1827-.LASFDE1827
.LASFDE1827:
	.long	.LASFDE1827-.Lframe1
	.long	.LFB589
	.long	.LFE589-.LFB589
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI400-.LFB589
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1827:
.LSFDE1829:
	.long	.LEFDE1829-.LASFDE1829
.LASFDE1829:
	.long	.LASFDE1829-.Lframe1
	.long	.LFB604
	.long	.LFE604-.LFB604
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI401-.LFB604
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1829:
.LSFDE1831:
	.long	.LEFDE1831-.LASFDE1831
.LASFDE1831:
	.long	.LASFDE1831-.Lframe1
	.long	.LFB605
	.long	.LFE605-.LFB605
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI402-.LFB605
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1831:
.LSFDE1833:
	.long	.LEFDE1833-.LASFDE1833
.LASFDE1833:
	.long	.LASFDE1833-.Lframe1
	.long	.LFB1202
	.long	.LFE1202-.LFB1202
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI403-.LFB1202
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI404-.LCFI403
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI405-.LCFI404
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1833:
.LSFDE1835:
	.long	.LEFDE1835-.LASFDE1835
.LASFDE1835:
	.long	.LASFDE1835-.Lframe1
	.long	.LFB615
	.long	.LFE615-.LFB615
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI406-.LFB615
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1835:
.LSFDE1837:
	.long	.LEFDE1837-.LASFDE1837
.LASFDE1837:
	.long	.LASFDE1837-.Lframe1
	.long	.LFB617
	.long	.LFE617-.LFB617
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI407-.LFB617
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1837:
.LSFDE1839:
	.long	.LEFDE1839-.LASFDE1839
.LASFDE1839:
	.long	.LASFDE1839-.Lframe1
	.long	.LFB598
	.long	.LFE598-.LFB598
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI408-.LFB598
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1839:
.LSFDE1841:
	.long	.LEFDE1841-.LASFDE1841
.LASFDE1841:
	.long	.LASFDE1841-.Lframe1
	.long	.LFB596
	.long	.LFE596-.LFB596
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI409-.LFB596
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1841:
.LSFDE1843:
	.long	.LEFDE1843-.LASFDE1843
.LASFDE1843:
	.long	.LASFDE1843-.Lframe1
	.long	.LFB595
	.long	.LFE595-.LFB595
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI410-.LFB595
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1843:
.LSFDE1845:
	.long	.LEFDE1845-.LASFDE1845
.LASFDE1845:
	.long	.LASFDE1845-.Lframe1
	.long	.LFB597
	.long	.LFE597-.LFB597
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI411-.LFB597
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1845:
.LSFDE1847:
	.long	.LEFDE1847-.LASFDE1847
.LASFDE1847:
	.long	.LASFDE1847-.Lframe1
	.long	.LFB616
	.long	.LFE616-.LFB616
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI412-.LFB616
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1847:
.LSFDE1849:
	.long	.LEFDE1849-.LASFDE1849
.LASFDE1849:
	.long	.LASFDE1849-.Lframe1
	.long	.LFB1073
	.long	.LFE1073-.LFB1073
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI413-.LFB1073
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1849:
.LSFDE1851:
	.long	.LEFDE1851-.LASFDE1851
.LASFDE1851:
	.long	.LASFDE1851-.Lframe1
	.long	.LFB1070
	.long	.LFE1070-.LFB1070
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI414-.LFB1070
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1851:
.LSFDE1853:
	.long	.LEFDE1853-.LASFDE1853
.LASFDE1853:
	.long	.LASFDE1853-.Lframe1
	.long	.LFB1074
	.long	.LFE1074-.LFB1074
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI415-.LFB1074
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1853:
.LSFDE1855:
	.long	.LEFDE1855-.LASFDE1855
.LASFDE1855:
	.long	.LASFDE1855-.Lframe1
	.long	.LFB1071
	.long	.LFE1071-.LFB1071
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI416-.LFB1071
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1855:
.LSFDE1857:
	.long	.LEFDE1857-.LASFDE1857
.LASFDE1857:
	.long	.LASFDE1857-.Lframe1
	.long	.LFB1065
	.long	.LFE1065-.LFB1065
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI417-.LFB1065
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI418-.LCFI417
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI419-.LCFI418
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1857:
.LSFDE1859:
	.long	.LEFDE1859-.LASFDE1859
.LASFDE1859:
	.long	.LASFDE1859-.Lframe1
	.long	.LFB1059
	.long	.LFE1059-.LFB1059
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI420-.LFB1059
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1859:
.LSFDE1861:
	.long	.LEFDE1861-.LASFDE1861
.LASFDE1861:
	.long	.LASFDE1861-.Lframe1
	.long	.LFB1056
	.long	.LFE1056-.LFB1056
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI421-.LFB1056
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI422-.LCFI421
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1861:
.LSFDE1863:
	.long	.LEFDE1863-.LASFDE1863
.LASFDE1863:
	.long	.LASFDE1863-.Lframe1
	.long	.LFB1063
	.long	.LFE1063-.LFB1063
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI423-.LFB1063
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI424-.LCFI423
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI425-.LCFI424
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1863:
.LSFDE1865:
	.long	.LEFDE1865-.LASFDE1865
.LASFDE1865:
	.long	.LASFDE1865-.Lframe1
	.long	.LFB1057
	.long	.LFE1057-.LFB1057
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI426-.LFB1057
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1865:
.LSFDE1867:
	.long	.LEFDE1867-.LASFDE1867
.LASFDE1867:
	.long	.LASFDE1867-.Lframe1
	.long	.LFB632
	.long	.LFE632-.LFB632
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI427-.LFB632
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1867:
.LSFDE1869:
	.long	.LEFDE1869-.LASFDE1869
.LASFDE1869:
	.long	.LASFDE1869-.Lframe1
	.long	.LFB1072
	.long	.LFE1072-.LFB1072
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI428-.LFB1072
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1869:
.LSFDE1871:
	.long	.LEFDE1871-.LASFDE1871
.LASFDE1871:
	.long	.LASFDE1871-.Lframe1
	.long	.LFB1069
	.long	.LFE1069-.LFB1069
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI429-.LFB1069
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1871:
.LSFDE1873:
	.long	.LEFDE1873-.LASFDE1873
.LASFDE1873:
	.long	.LASFDE1873-.Lframe1
	.long	.LFB1131
	.long	.LFE1131-.LFB1131
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI430-.LFB1131
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1873:
.LSFDE1875:
	.long	.LEFDE1875-.LASFDE1875
.LASFDE1875:
	.long	.LASFDE1875-.Lframe1
	.long	.LFB633
	.long	.LFE633-.LFB633
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI431-.LFB633
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1875:
.LSFDE1877:
	.long	.LEFDE1877-.LASFDE1877
.LASFDE1877:
	.long	.LASFDE1877-.Lframe1
	.long	.LFB1064
	.long	.LFE1064-.LFB1064
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI432-.LFB1064
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI433-.LCFI432
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI434-.LCFI433
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1877:
.LSFDE1879:
	.long	.LEFDE1879-.LASFDE1879
.LASFDE1879:
	.long	.LASFDE1879-.Lframe1
	.long	.LFB1058
	.long	.LFE1058-.LFB1058
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI435-.LFB1058
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1879:
.LSFDE1881:
	.long	.LEFDE1881-.LASFDE1881
.LASFDE1881:
	.long	.LASFDE1881-.Lframe1
	.long	.LFB1055
	.long	.LFE1055-.LFB1055
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI436-.LFB1055
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI437-.LCFI436
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1881:
.LSFDE1883:
	.long	.LEFDE1883-.LASFDE1883
.LASFDE1883:
	.long	.LASFDE1883-.Lframe1
	.long	.LFB704
	.long	.LFE704-.LFB704
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI438-.LFB704
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1883:
.LSFDE1885:
	.long	.LEFDE1885-.LASFDE1885
.LASFDE1885:
	.long	.LASFDE1885-.Lframe1
	.long	.LFB703
	.long	.LFE703-.LFB703
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI439-.LFB703
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1885:
.LSFDE1887:
	.long	.LEFDE1887-.LASFDE1887
.LASFDE1887:
	.long	.LASFDE1887-.Lframe1
	.long	.LFB715
	.long	.LFE715-.LFB715
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI440-.LFB715
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1887:
.LSFDE1889:
	.long	.LEFDE1889-.LASFDE1889
.LASFDE1889:
	.long	.LASFDE1889-.Lframe1
	.long	.LFB714
	.long	.LFE714-.LFB714
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI441-.LFB714
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI443-.LCFI441
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1889:
.LSFDE1891:
	.long	.LEFDE1891-.LASFDE1891
.LASFDE1891:
	.long	.LASFDE1891-.Lframe1
	.long	.LFB713
	.long	.LFE713-.LFB713
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI444-.LFB713
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI446-.LCFI444
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1891:
.LSFDE1893:
	.long	.LEFDE1893-.LASFDE1893
.LASFDE1893:
	.long	.LASFDE1893-.Lframe1
	.long	.LFB712
	.long	.LFE712-.LFB712
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI447-.LFB712
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI449-.LCFI447
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1893:
.LSFDE1895:
	.long	.LEFDE1895-.LASFDE1895
.LASFDE1895:
	.long	.LASFDE1895-.Lframe1
	.long	.LFB702
	.long	.LFE702-.LFB702
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI450-.LFB702
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI452-.LCFI450
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1895:
.LSFDE1897:
	.long	.LEFDE1897-.LASFDE1897
.LASFDE1897:
	.long	.LASFDE1897-.Lframe1
	.long	.LFB701
	.long	.LFE701-.LFB701
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI453-.LFB701
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI455-.LCFI453
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1897:
.LSFDE1899:
	.long	.LEFDE1899-.LASFDE1899
.LASFDE1899:
	.long	.LASFDE1899-.Lframe1
	.long	.LFB700
	.long	.LFE700-.LFB700
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI456-.LFB700
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI458-.LCFI456
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1899:
.LSFDE1901:
	.long	.LEFDE1901-.LASFDE1901
.LASFDE1901:
	.long	.LASFDE1901-.Lframe1
	.long	.LFB699
	.long	.LFE699-.LFB699
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI459-.LFB699
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI460-.LCFI459
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI461-.LCFI460
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1901:
.LSFDE1903:
	.long	.LEFDE1903-.LASFDE1903
.LASFDE1903:
	.long	.LASFDE1903-.Lframe1
	.long	.LFB609
	.long	.LFE609-.LFB609
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI462-.LFB609
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1903:
.LSFDE1905:
	.long	.LEFDE1905-.LASFDE1905
.LASFDE1905:
	.long	.LASFDE1905-.Lframe1
	.long	.LFB707
	.long	.LFE707-.LFB707
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI463-.LFB707
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI464-.LCFI463
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI465-.LCFI464
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1905:
.LSFDE1907:
	.long	.LEFDE1907-.LASFDE1907
.LASFDE1907:
	.long	.LASFDE1907-.Lframe1
	.long	.LFB706
	.long	.LFE706-.LFB706
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI466-.LFB706
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI467-.LCFI466
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI468-.LCFI467
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1907:
.LSFDE1909:
	.long	.LEFDE1909-.LASFDE1909
.LASFDE1909:
	.long	.LASFDE1909-.Lframe1
	.long	.LFB711
	.long	.LFE711-.LFB711
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI469-.LFB711
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI470-.LCFI469
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI471-.LCFI470
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1909:
.LSFDE1911:
	.long	.LEFDE1911-.LASFDE1911
.LASFDE1911:
	.long	.LASFDE1911-.Lframe1
	.long	.LFB710
	.long	.LFE710-.LFB710
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI472-.LFB710
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI473-.LCFI472
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI474-.LCFI473
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1911:
.LSFDE1913:
	.long	.LEFDE1913-.LASFDE1913
.LASFDE1913:
	.long	.LASFDE1913-.Lframe1
	.long	.LFB709
	.long	.LFE709-.LFB709
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI475-.LFB709
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1913:
.LSFDE1915:
	.long	.LEFDE1915-.LASFDE1915
.LASFDE1915:
	.long	.LASFDE1915-.Lframe1
	.long	.LFB708
	.long	.LFE708-.LFB708
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI476-.LFB708
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1915:
.LSFDE1917:
	.long	.LEFDE1917-.LASFDE1917
.LASFDE1917:
	.long	.LASFDE1917-.Lframe1
	.long	.LFB705
	.long	.LFE705-.LFB705
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI479-.LFB705
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1917:
.LSFDE1919:
	.long	.LEFDE1919-.LASFDE1919
.LASFDE1919:
	.long	.LASFDE1919-.Lframe1
	.long	.LFB717
	.long	.LFE717-.LFB717
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI482-.LFB717
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1919:
.LSFDE1921:
	.long	.LEFDE1921-.LASFDE1921
.LASFDE1921:
	.long	.LASFDE1921-.Lframe1
	.long	.LFB694
	.long	.LFE694-.LFB694
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI483-.LFB694
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI484-.LCFI483
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI485-.LCFI484
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1921:
.LSFDE1923:
	.long	.LEFDE1923-.LASFDE1923
.LASFDE1923:
	.long	.LASFDE1923-.Lframe1
	.long	.LFB590
	.long	.LFE590-.LFB590
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI486-.LFB590
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1923:
.LSFDE1925:
	.long	.LEFDE1925-.LASFDE1925
.LASFDE1925:
	.long	.LASFDE1925-.Lframe1
	.long	.LFB698
	.long	.LFE698-.LFB698
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI487-.LFB698
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI488-.LCFI487
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI489-.LCFI488
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1925:
.LSFDE1927:
	.long	.LEFDE1927-.LASFDE1927
.LASFDE1927:
	.long	.LASFDE1927-.Lframe1
	.long	.LFB697
	.long	.LFE697-.LFB697
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI490-.LFB697
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1927:
.LSFDE1929:
	.long	.LEFDE1929-.LASFDE1929
.LASFDE1929:
	.long	.LASFDE1929-.Lframe1
	.long	.LFB696
	.long	.LFE696-.LFB696
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI491-.LFB696
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1929:
.LSFDE1931:
	.long	.LEFDE1931-.LASFDE1931
.LASFDE1931:
	.long	.LASFDE1931-.Lframe1
	.long	.LFB611
	.long	.LFE611-.LFB611
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI492-.LFB611
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1931:
.LSFDE1933:
	.long	.LEFDE1933-.LASFDE1933
.LASFDE1933:
	.long	.LASFDE1933-.Lframe1
	.long	.LFB941
	.long	.LFE941-.LFB941
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI495-.LFB941
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1933:
.LSFDE1935:
	.long	.LEFDE1935-.LASFDE1935
.LASFDE1935:
	.long	.LASFDE1935-.Lframe1
	.long	.LFB940
	.long	.LFE940-.LFB940
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI496-.LFB940
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1935:
.LSFDE1937:
	.long	.LEFDE1937-.LASFDE1937
.LASFDE1937:
	.long	.LASFDE1937-.Lframe1
	.long	.LFB939
	.long	.LFE939-.LFB939
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI497-.LFB939
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1937:
.LSFDE1939:
	.long	.LEFDE1939-.LASFDE1939
.LASFDE1939:
	.long	.LASFDE1939-.Lframe1
	.long	.LFB938
	.long	.LFE938-.LFB938
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI500-.LFB938
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1939:
.LSFDE1941:
	.long	.LEFDE1941-.LASFDE1941
.LASFDE1941:
	.long	.LASFDE1941-.Lframe1
	.long	.LFB937
	.long	.LFE937-.LFB937
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI501-.LFB937
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI502-.LCFI501
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI503-.LCFI502
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1941:
.LSFDE1943:
	.long	.LEFDE1943-.LASFDE1943
.LASFDE1943:
	.long	.LASFDE1943-.Lframe1
	.long	.LFB936
	.long	.LFE936-.LFB936
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI506-.LFB936
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1943:
.LSFDE1945:
	.long	.LEFDE1945-.LASFDE1945
.LASFDE1945:
	.long	.LASFDE1945-.Lframe1
	.long	.LFB935
	.long	.LFE935-.LFB935
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI507-.LFB935
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI508-.LCFI507
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI509-.LCFI508
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1945:
.LSFDE1947:
	.long	.LEFDE1947-.LASFDE1947
.LASFDE1947:
	.long	.LASFDE1947-.Lframe1
	.long	.LFB934
	.long	.LFE934-.LFB934
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI510-.LFB934
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI512-.LCFI510
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1947:
.LSFDE1949:
	.long	.LEFDE1949-.LASFDE1949
.LASFDE1949:
	.long	.LASFDE1949-.Lframe1
	.long	.LFB933
	.long	.LFE933-.LFB933
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI513-.LFB933
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI515-.LCFI513
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1949:
.LSFDE1951:
	.long	.LEFDE1951-.LASFDE1951
.LASFDE1951:
	.long	.LASFDE1951-.Lframe1
	.long	.LFB932
	.long	.LFE932-.LFB932
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI516-.LFB932
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI518-.LCFI516
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1951:
.LSFDE1953:
	.long	.LEFDE1953-.LASFDE1953
.LASFDE1953:
	.long	.LASFDE1953-.Lframe1
	.long	.LFB931
	.long	.LFE931-.LFB931
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI519-.LFB931
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI520-.LCFI519
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI521-.LCFI520
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1953:
.LSFDE1955:
	.long	.LEFDE1955-.LASFDE1955
.LASFDE1955:
	.long	.LASFDE1955-.Lframe1
	.long	.LFB930
	.long	.LFE930-.LFB930
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI522-.LFB930
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI523-.LCFI522
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI524-.LCFI523
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1955:
.LSFDE1957:
	.long	.LEFDE1957-.LASFDE1957
.LASFDE1957:
	.long	.LASFDE1957-.Lframe1
	.long	.LFB929
	.long	.LFE929-.LFB929
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI525-.LFB929
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1957:
.LSFDE1959:
	.long	.LEFDE1959-.LASFDE1959
.LASFDE1959:
	.long	.LASFDE1959-.Lframe1
	.long	.LFB928
	.long	.LFE928-.LFB928
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI526-.LFB928
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1959:
.LSFDE1961:
	.long	.LEFDE1961-.LASFDE1961
.LASFDE1961:
	.long	.LASFDE1961-.Lframe1
	.long	.LFB927
	.long	.LFE927-.LFB927
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI529-.LFB927
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1961:
.LSFDE1963:
	.long	.LEFDE1963-.LASFDE1963
.LASFDE1963:
	.long	.LASFDE1963-.Lframe1
	.long	.LFB926
	.long	.LFE926-.LFB926
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI532-.LFB926
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1963:
.LSFDE1965:
	.long	.LEFDE1965-.LASFDE1965
.LASFDE1965:
	.long	.LASFDE1965-.Lframe1
	.long	.LFB923
	.long	.LFE923-.LFB923
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI533-.LFB923
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1965:
.LSFDE1967:
	.long	.LEFDE1967-.LASFDE1967
.LASFDE1967:
	.long	.LASFDE1967-.Lframe1
	.long	.LFB922
	.long	.LFE922-.LFB922
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI536-.LFB922
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1967:
.LSFDE1969:
	.long	.LEFDE1969-.LASFDE1969
.LASFDE1969:
	.long	.LASFDE1969-.Lframe1
	.long	.LFB921
	.long	.LFE921-.LFB921
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI537-.LFB921
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI538-.LCFI537
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI539-.LCFI538
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1969:
.LSFDE1971:
	.long	.LEFDE1971-.LASFDE1971
.LASFDE1971:
	.long	.LASFDE1971-.Lframe1
	.long	.LFB920
	.long	.LFE920-.LFB920
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI542-.LFB920
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1971:
.LSFDE1973:
	.long	.LEFDE1973-.LASFDE1973
.LASFDE1973:
	.long	.LASFDE1973-.Lframe1
	.long	.LFB919
	.long	.LFE919-.LFB919
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI543-.LFB919
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI544-.LCFI543
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI545-.LCFI544
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE1973:
.LSFDE1975:
	.long	.LEFDE1975-.LASFDE1975
.LASFDE1975:
	.long	.LASFDE1975-.Lframe1
	.long	.LFB918
	.long	.LFE918-.LFB918
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI546-.LFB918
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI548-.LCFI546
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1975:
.LSFDE1977:
	.long	.LEFDE1977-.LASFDE1977
.LASFDE1977:
	.long	.LASFDE1977-.Lframe1
	.long	.LFB917
	.long	.LFE917-.LFB917
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI549-.LFB917
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI551-.LCFI549
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1977:
.LSFDE1979:
	.long	.LEFDE1979-.LASFDE1979
.LASFDE1979:
	.long	.LASFDE1979-.Lframe1
	.long	.LFB916
	.long	.LFE916-.LFB916
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI552-.LFB916
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI554-.LCFI552
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1979:
.LSFDE1981:
	.long	.LEFDE1981-.LASFDE1981
.LASFDE1981:
	.long	.LASFDE1981-.Lframe1
	.long	.LFB915
	.long	.LFE915-.LFB915
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI555-.LFB915
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI556-.LCFI555
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI557-.LCFI556
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1981:
.LSFDE1983:
	.long	.LEFDE1983-.LASFDE1983
.LASFDE1983:
	.long	.LASFDE1983-.Lframe1
	.long	.LFB914
	.long	.LFE914-.LFB914
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI558-.LFB914
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI559-.LCFI558
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI560-.LCFI559
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE1983:
.LSFDE1985:
	.long	.LEFDE1985-.LASFDE1985
.LASFDE1985:
	.long	.LASFDE1985-.Lframe1
	.long	.LFB913
	.long	.LFE913-.LFB913
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI561-.LFB913
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1985:
.LSFDE1987:
	.long	.LEFDE1987-.LASFDE1987
.LASFDE1987:
	.long	.LASFDE1987-.Lframe1
	.long	.LFB912
	.long	.LFE912-.LFB912
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI562-.LFB912
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1987:
.LSFDE1989:
	.long	.LEFDE1989-.LASFDE1989
.LASFDE1989:
	.long	.LASFDE1989-.Lframe1
	.long	.LFB911
	.long	.LFE911-.LFB911
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI565-.LFB911
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1989:
.LSFDE1991:
	.long	.LEFDE1991-.LASFDE1991
.LASFDE1991:
	.long	.LASFDE1991-.Lframe1
	.long	.LFB910
	.long	.LFE910-.LFB910
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI568-.LFB910
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1991:
.LSFDE1993:
	.long	.LEFDE1993-.LASFDE1993
.LASFDE1993:
	.long	.LASFDE1993-.Lframe1
	.long	.LFB594
	.long	.LFE594-.LFB594
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI569-.LFB594
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1993:
.LSFDE1995:
	.long	.LEFDE1995-.LASFDE1995
.LASFDE1995:
	.long	.LASFDE1995-.Lframe1
	.long	.LFB592
	.long	.LFE592-.LFB592
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI570-.LFB592
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE1995:
.LSFDE1997:
	.long	.LEFDE1997-.LASFDE1997
.LASFDE1997:
	.long	.LASFDE1997-.Lframe1
	.long	.LFB925
	.long	.LFE925-.LFB925
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI573-.LFB925
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE1997:
.LSFDE1999:
	.long	.LEFDE1999-.LASFDE1999
.LASFDE1999:
	.long	.LASFDE1999-.Lframe1
	.long	.LFB806
	.long	.LFE806-.LFB806
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI574-.LFB806
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE1999:
.LSFDE2001:
	.long	.LEFDE2001-.LASFDE2001
.LASFDE2001:
	.long	.LASFDE2001-.Lframe1
	.long	.LFB805
	.long	.LFE805-.LFB805
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI575-.LFB805
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI576-.LCFI575
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI577-.LCFI576
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2001:
.LSFDE2003:
	.long	.LEFDE2003-.LASFDE2003
.LASFDE2003:
	.long	.LASFDE2003-.Lframe1
	.long	.LFB804
	.long	.LFE804-.LFB804
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI578-.LFB804
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI579-.LCFI578
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI580-.LCFI579
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE2003:
.LSFDE2005:
	.long	.LEFDE2005-.LASFDE2005
.LASFDE2005:
	.long	.LASFDE2005-.Lframe1
	.long	.LFB803
	.long	.LFE803-.LFB803
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI581-.LFB803
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI582-.LCFI581
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI583-.LCFI582
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2005:
.LSFDE2007:
	.long	.LEFDE2007-.LASFDE2007
.LASFDE2007:
	.long	.LASFDE2007-.Lframe1
	.long	.LFB824
	.long	.LFE824-.LFB824
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI586-.LFB824
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2007:
.LSFDE2009:
	.long	.LEFDE2009-.LASFDE2009
.LASFDE2009:
	.long	.LASFDE2009-.Lframe1
	.long	.LFB610
	.long	.LFE610-.LFB610
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI587-.LFB610
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2009:
.LSFDE2011:
	.long	.LEFDE2011-.LASFDE2011
.LASFDE2011:
	.long	.LASFDE2011-.Lframe1
	.long	.LFB808
	.long	.LFE808-.LFB808
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI590-.LFB808
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2011:
.LSFDE2013:
	.long	.LEFDE2013-.LASFDE2013
.LASFDE2013:
	.long	.LASFDE2013-.Lframe1
	.long	.LFB807
	.long	.LFE807-.LFB807
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI591-.LFB807
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE2013:
.LSFDE2015:
	.long	.LEFDE2015-.LASFDE2015
.LASFDE2015:
	.long	.LASFDE2015-.Lframe1
	.long	.LFB794
	.long	.LFE794-.LFB794
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI592-.LFB794
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI593-.LCFI592
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI594-.LCFI593
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2015:
.LSFDE2017:
	.long	.LEFDE2017-.LASFDE2017
.LASFDE2017:
	.long	.LASFDE2017-.Lframe1
	.long	.LFB793
	.long	.LFE793-.LFB793
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI595-.LFB793
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI596-.LCFI595
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI597-.LCFI596
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2017:
.LSFDE2019:
	.long	.LEFDE2019-.LASFDE2019
.LASFDE2019:
	.long	.LASFDE2019-.Lframe1
	.long	.LFB593
	.long	.LFE593-.LFB593
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI598-.LFB593
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2019:
.LSFDE2021:
	.long	.LEFDE2021-.LASFDE2021
.LASFDE2021:
	.long	.LASFDE2021-.Lframe1
	.long	.LFB591
	.long	.LFE591-.LFB591
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI599-.LFB591
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2021:
.LSFDE2023:
	.long	.LEFDE2023-.LASFDE2023
.LASFDE2023:
	.long	.LASFDE2023-.Lframe1
	.long	.LFB802
	.long	.LFE802-.LFB802
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI600-.LFB802
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI601-.LCFI600
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI602-.LCFI601
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE2023:
.LSFDE2025:
	.long	.LEFDE2025-.LASFDE2025
.LASFDE2025:
	.long	.LASFDE2025-.Lframe1
	.long	.LFB801
	.long	.LFE801-.LFB801
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI603-.LFB801
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI605-.LCFI603
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2025:
.LSFDE2027:
	.long	.LEFDE2027-.LASFDE2027
.LASFDE2027:
	.long	.LASFDE2027-.Lframe1
	.long	.LFB800
	.long	.LFE800-.LFB800
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI606-.LFB800
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI608-.LCFI606
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2027:
.LSFDE2029:
	.long	.LEFDE2029-.LASFDE2029
.LASFDE2029:
	.long	.LASFDE2029-.Lframe1
	.long	.LFB799
	.long	.LFE799-.LFB799
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI609-.LFB799
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI611-.LCFI609
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2029:
.LSFDE2031:
	.long	.LEFDE2031-.LASFDE2031
.LASFDE2031:
	.long	.LASFDE2031-.Lframe1
	.long	.LFB798
	.long	.LFE798-.LFB798
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI612-.LFB798
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI613-.LCFI612
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI614-.LCFI613
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2031:
.LSFDE2033:
	.long	.LEFDE2033-.LASFDE2033
.LASFDE2033:
	.long	.LASFDE2033-.Lframe1
	.long	.LFB797
	.long	.LFE797-.LFB797
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI615-.LFB797
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI616-.LCFI615
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI617-.LCFI616
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2033:
.LSFDE2035:
	.long	.LEFDE2035-.LASFDE2035
.LASFDE2035:
	.long	.LASFDE2035-.Lframe1
	.long	.LFB796
	.long	.LFE796-.LFB796
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI618-.LFB796
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2035:
.LSFDE2037:
	.long	.LEFDE2037-.LASFDE2037
.LASFDE2037:
	.long	.LASFDE2037-.Lframe1
	.long	.LFB823
	.long	.LFE823-.LFB823
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI619-.LFB823
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE2037:
.LSFDE2039:
	.long	.LEFDE2039-.LASFDE2039
.LASFDE2039:
	.long	.LASFDE2039-.Lframe1
	.long	.LFB822
	.long	.LFE822-.LFB822
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI620-.LFB822
	.byte	0xe
	.uleb128 0x10
	.byte	0x8e
	.uleb128 0x2
	.align 8
.LEFDE2039:
.LSFDE2041:
	.long	.LEFDE2041-.LASFDE2041
.LASFDE2041:
	.long	.LASFDE2041-.Lframe1
	.long	.LFB821
	.long	.LFE821-.LFB821
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI621-.LFB821
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI622-.LCFI621
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI623-.LCFI622
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2041:
.LSFDE2043:
	.long	.LEFDE2043-.LASFDE2043
.LASFDE2043:
	.long	.LASFDE2043-.Lframe1
	.long	.LFB820
	.long	.LFE820-.LFB820
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI624-.LFB820
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI625-.LCFI624
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI626-.LCFI625
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE2043:
.LSFDE2045:
	.long	.LEFDE2045-.LASFDE2045
.LASFDE2045:
	.long	.LASFDE2045-.Lframe1
	.long	.LFB811
	.long	.LFE811-.LFB811
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI627-.LFB811
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2045:
.LSFDE2047:
	.long	.LEFDE2047-.LASFDE2047
.LASFDE2047:
	.long	.LASFDE2047-.Lframe1
	.long	.LFB810
	.long	.LFE810-.LFB810
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI628-.LFB810
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI629-.LCFI628
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI630-.LCFI629
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2047:
.LSFDE2049:
	.long	.LEFDE2049-.LASFDE2049
.LASFDE2049:
	.long	.LASFDE2049-.Lframe1
	.long	.LFB809
	.long	.LFE809-.LFB809
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI631-.LFB809
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI632-.LCFI631
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI633-.LCFI632
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2049:
.LSFDE2051:
	.long	.LEFDE2051-.LASFDE2051
.LASFDE2051:
	.long	.LASFDE2051-.Lframe1
	.long	.LFB612
	.long	.LFE612-.LFB612
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI634-.LFB612
	.byte	0xe
	.uleb128 0x10
	.align 8
.LEFDE2051:
.LSFDE2053:
	.long	.LEFDE2053-.LASFDE2053
.LASFDE2053:
	.long	.LASFDE2053-.Lframe1
	.long	.LFB819
	.long	.LFE819-.LFB819
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI635-.LFB819
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI636-.LCFI635
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI637-.LCFI636
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2053:
.LSFDE2055:
	.long	.LEFDE2055-.LASFDE2055
.LASFDE2055:
	.long	.LASFDE2055-.Lframe1
	.long	.LFB818
	.long	.LFE818-.LFB818
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI638-.LFB818
	.byte	0xe
	.uleb128 0x10
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI639-.LCFI638
	.byte	0xe
	.uleb128 0x18
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI640-.LCFI639
	.byte	0xe
	.uleb128 0x20
	.align 8
.LEFDE2055:
.LSFDE2057:
	.long	.LEFDE2057-.LASFDE2057
.LASFDE2057:
	.long	.LASFDE2057-.Lframe1
	.long	.LFB817
	.long	.LFE817-.LFB817
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI641-.LFB817
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI643-.LCFI641
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2057:
.LSFDE2059:
	.long	.LEFDE2059-.LASFDE2059
.LASFDE2059:
	.long	.LASFDE2059-.Lframe1
	.long	.LFB816
	.long	.LFE816-.LFB816
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI644-.LFB816
	.byte	0x8f
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI646-.LCFI644
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.align 8
.LEFDE2059:
.LSFDE2061:
	.long	.LEFDE2061-.LASFDE2061
.LASFDE2061:
	.long	.LASFDE2061-.Lframe1
	.long	.LFB815
	.long	.LFE815-.LFB815
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI647-.LFB815
	.byte	0x8e
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI649-.LCFI647
	.byte	0xe
	.uleb128 0x20
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2061:
.LSFDE2063:
	.long	.LEFDE2063-.LASFDE2063
.LASFDE2063:
	.long	.LASFDE2063-.Lframe1
	.long	.LFB814
	.long	.LFE814-.LFB814
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI650-.LFB814
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI651-.LCFI650
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI652-.LCFI651
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2063:
.LSFDE2065:
	.long	.LEFDE2065-.LASFDE2065
.LASFDE2065:
	.long	.LASFDE2065-.Lframe1
	.long	.LFB813
	.long	.LFE813-.LFB813
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI653-.LFB813
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI654-.LCFI653
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI655-.LCFI654
	.byte	0xe
	.uleb128 0x20
	.byte	0x8e
	.uleb128 0x3
	.byte	0x8f
	.uleb128 0x2
	.align 8
.LEFDE2065:
	.ident	"GCC: (GNU) 4.1.1 (Gentoo 4.1.1-r1)"
	.section	.note.GNU-stack,"",@progbits
