	.file	"op.c"
.globl comis_eflags
	.section	.rodata
	.align 4
	.type	comis_eflags, @object
	.size	comis_eflags, 16
comis_eflags:
	.long	1
	.long	64
	.long	0
	.long	69
.globl fcomi_ccval
	.align 4
	.type	fcomi_ccval, @object
	.size	fcomi_ccval, 16
fcomi_ccval:
	.long	1
	.long	64
	.long	0
	.long	69
.globl fcom_ccval
	.align 4
	.type	fcom_ccval, @object
	.size	fcom_ccval, 16
fcom_ccval:
	.long	256
	.long	16384
	.long	0
	.long	17664
.globl cc_table
	.data
	.align 32
	.type	cc_table, @object
	.size	cc_table, 336
cc_table:
	.zero	8
	.long	compute_all_eflags
	.long	compute_c_eflags
	.long	compute_all_mulb
	.long	compute_c_mull
	.long	compute_all_mulw
	.long	compute_c_mull
	.long	compute_all_mull
	.long	compute_c_mull
	.zero	8
	.long	compute_all_addb
	.long	compute_c_addb
	.long	compute_all_addw
	.long	compute_c_addw
	.long	compute_all_addl
	.long	compute_c_addl
	.zero	8
	.long	compute_all_adcb
	.long	compute_c_adcb
	.long	compute_all_adcw
	.long	compute_c_adcw
	.long	compute_all_adcl
	.long	compute_c_adcl
	.zero	8
	.long	compute_all_subb
	.long	compute_c_subb
	.long	compute_all_subw
	.long	compute_c_subw
	.long	compute_all_subl
	.long	compute_c_subl
	.zero	8
	.long	compute_all_sbbb
	.long	compute_c_sbbb
	.long	compute_all_sbbw
	.long	compute_c_sbbw
	.long	compute_all_sbbl
	.long	compute_c_sbbl
	.zero	8
	.long	compute_all_logicb
	.long	compute_c_logicb
	.long	compute_all_logicw
	.long	compute_c_logicw
	.long	compute_all_logicl
	.long	compute_c_logicl
	.zero	8
	.long	compute_all_incb
	.long	compute_c_incl
	.long	compute_all_incw
	.long	compute_c_incl
	.long	compute_all_incl
	.long	compute_c_incl
	.zero	8
	.long	compute_all_decb
	.long	compute_c_incl
	.long	compute_all_decw
	.long	compute_c_incl
	.long	compute_all_decl
	.long	compute_c_incl
	.zero	8
	.long	compute_all_shlb
	.long	compute_c_shlb
	.long	compute_all_shlw
	.long	compute_c_shlw
	.long	compute_all_shll
	.long	compute_c_shll
	.zero	8
	.long	compute_all_sarb
	.long	compute_c_sarl
	.long	compute_all_sarw
	.long	compute_c_sarl
	.long	compute_all_sarl
	.long	compute_c_sarl
	.zero	8
	.text
	.p2align 4,,15
.globl op_movl_A0_EAX
	.type	op_movl_A0_EAX, @function
op_movl_A0_EAX:
	movl	(%ebp), %edi
	ret
	.size	op_movl_A0_EAX, .-op_movl_A0_EAX
	.p2align 4,,15
.globl op_addl_A0_EAX
	.type	op_addl_A0_EAX, @function
op_addl_A0_EAX:
	movl	(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_EAX, .-op_addl_A0_EAX
	.p2align 4,,15
.globl op_addl_A0_EAX_s1
	.type	op_addl_A0_EAX_s1, @function
op_addl_A0_EAX_s1:
	movl	(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_EAX_s1, .-op_addl_A0_EAX_s1
	.p2align 4,,15
.globl op_addl_A0_EAX_s2
	.type	op_addl_A0_EAX_s2, @function
op_addl_A0_EAX_s2:
	movl	(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_EAX_s2, .-op_addl_A0_EAX_s2
	.p2align 4,,15
.globl op_addl_A0_EAX_s3
	.type	op_addl_A0_EAX_s3, @function
op_addl_A0_EAX_s3:
	movl	(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_EAX_s3, .-op_addl_A0_EAX_s3
	.p2align 4,,15
.globl op_movl_T0_EAX
	.type	op_movl_T0_EAX, @function
op_movl_T0_EAX:
	movl	(%ebp), %ebx
	ret
	.size	op_movl_T0_EAX, .-op_movl_T0_EAX
	.p2align 4,,15
.globl op_movl_T1_EAX
	.type	op_movl_T1_EAX, @function
op_movl_T1_EAX:
	movl	(%ebp), %esi
	ret
	.size	op_movl_T1_EAX, .-op_movl_T1_EAX
	.p2align 4,,15
.globl op_movh_T0_EAX
	.type	op_movh_T0_EAX, @function
op_movh_T0_EAX:
	movl	(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_EAX, .-op_movh_T0_EAX
	.p2align 4,,15
.globl op_movh_T1_EAX
	.type	op_movh_T1_EAX, @function
op_movh_T1_EAX:
	movl	(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_EAX, .-op_movh_T1_EAX
	.p2align 4,,15
.globl op_movl_EAX_T0
	.type	op_movl_EAX_T0, @function
op_movl_EAX_T0:
	movl	%ebx, (%ebp)
	ret
	.size	op_movl_EAX_T0, .-op_movl_EAX_T0
	.p2align 4,,15
.globl op_movl_EAX_T1
	.type	op_movl_EAX_T1, @function
op_movl_EAX_T1:
	movl	%esi, (%ebp)
	ret
	.size	op_movl_EAX_T1, .-op_movl_EAX_T1
	.p2align 4,,15
.globl op_movl_EAX_A0
	.type	op_movl_EAX_A0, @function
op_movl_EAX_A0:
	movl	%edi, (%ebp)
	ret
	.size	op_movl_EAX_A0, .-op_movl_EAX_A0
	.p2align 4,,15
.globl op_cmovw_EAX_T1_T0
	.type	op_cmovw_EAX_T1_T0, @function
op_cmovw_EAX_T1_T0:
	testl	%ebx, %ebx
	je	.L14
	movw	%si, (%ebp)
.L14:
	ret
	.size	op_cmovw_EAX_T1_T0, .-op_cmovw_EAX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EAX_T1_T0
	.type	op_cmovl_EAX_T1_T0, @function
op_cmovl_EAX_T1_T0:
	testl	%ebx, %ebx
	je	.L16
	movl	%esi, (%ebp)
.L16:
	ret
	.size	op_cmovl_EAX_T1_T0, .-op_cmovl_EAX_T1_T0
	.p2align 4,,15
.globl op_movw_EAX_T0
	.type	op_movw_EAX_T0, @function
op_movw_EAX_T0:
	movw	%bx, (%ebp)
	ret
	.size	op_movw_EAX_T0, .-op_movw_EAX_T0
	.p2align 4,,15
.globl op_movw_EAX_T1
	.type	op_movw_EAX_T1, @function
op_movw_EAX_T1:
	movw	%si, (%ebp)
	ret
	.size	op_movw_EAX_T1, .-op_movw_EAX_T1
	.p2align 4,,15
.globl op_movw_EAX_A0
	.type	op_movw_EAX_A0, @function
op_movw_EAX_A0:
	movw	%di, (%ebp)
	ret
	.size	op_movw_EAX_A0, .-op_movw_EAX_A0
	.p2align 4,,15
.globl op_movb_EAX_T0
	.type	op_movb_EAX_T0, @function
op_movb_EAX_T0:
	movb	%bl, (%ebp)
	ret
	.size	op_movb_EAX_T0, .-op_movb_EAX_T0
	.p2align 4,,15
.globl op_movh_EAX_T0
	.type	op_movh_EAX_T0, @function
op_movh_EAX_T0:
	movb	%bl, 1(%ebp)
	ret
	.size	op_movh_EAX_T0, .-op_movh_EAX_T0
	.p2align 4,,15
.globl op_movb_EAX_T1
	.type	op_movb_EAX_T1, @function
op_movb_EAX_T1:
	movl	%esi, %eax
	movb	%al, (%ebp)
	ret
	.size	op_movb_EAX_T1, .-op_movb_EAX_T1
	.p2align 4,,15
.globl op_movh_EAX_T1
	.type	op_movh_EAX_T1, @function
op_movh_EAX_T1:
	movl	%esi, %eax
	movb	%al, 1(%ebp)
	ret
	.size	op_movh_EAX_T1, .-op_movh_EAX_T1
	.p2align 4,,15
.globl op_movl_A0_ECX
	.type	op_movl_A0_ECX, @function
op_movl_A0_ECX:
	movl	4(%ebp), %edi
	ret
	.size	op_movl_A0_ECX, .-op_movl_A0_ECX
	.p2align 4,,15
.globl op_addl_A0_ECX
	.type	op_addl_A0_ECX, @function
op_addl_A0_ECX:
	movl	4(%ebp), %edx
	addl	%edx, %edi
	ret
	.size	op_addl_A0_ECX, .-op_addl_A0_ECX
	.p2align 4,,15
.globl op_addl_A0_ECX_s1
	.type	op_addl_A0_ECX_s1, @function
op_addl_A0_ECX_s1:
	movl	4(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_ECX_s1, .-op_addl_A0_ECX_s1
	.p2align 4,,15
.globl op_addl_A0_ECX_s2
	.type	op_addl_A0_ECX_s2, @function
op_addl_A0_ECX_s2:
	movl	4(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_ECX_s2, .-op_addl_A0_ECX_s2
	.p2align 4,,15
.globl op_addl_A0_ECX_s3
	.type	op_addl_A0_ECX_s3, @function
op_addl_A0_ECX_s3:
	movl	4(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_ECX_s3, .-op_addl_A0_ECX_s3
	.p2align 4,,15
.globl op_movl_T0_ECX
	.type	op_movl_T0_ECX, @function
op_movl_T0_ECX:
	movl	4(%ebp), %ebx
	ret
	.size	op_movl_T0_ECX, .-op_movl_T0_ECX
	.p2align 4,,15
.globl op_movl_T1_ECX
	.type	op_movl_T1_ECX, @function
op_movl_T1_ECX:
	movl	4(%ebp), %esi
	ret
	.size	op_movl_T1_ECX, .-op_movl_T1_ECX
	.p2align 4,,15
.globl op_movh_T0_ECX
	.type	op_movh_T0_ECX, @function
op_movh_T0_ECX:
	movl	4(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_ECX, .-op_movh_T0_ECX
	.p2align 4,,15
.globl op_movh_T1_ECX
	.type	op_movh_T1_ECX, @function
op_movh_T1_ECX:
	movl	4(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_ECX, .-op_movh_T1_ECX
	.p2align 4,,15
.globl op_movl_ECX_T0
	.type	op_movl_ECX_T0, @function
op_movl_ECX_T0:
	movl	%ebx, 4(%ebp)
	ret
	.size	op_movl_ECX_T0, .-op_movl_ECX_T0
	.p2align 4,,15
.globl op_movl_ECX_T1
	.type	op_movl_ECX_T1, @function
op_movl_ECX_T1:
	movl	%esi, 4(%ebp)
	ret
	.size	op_movl_ECX_T1, .-op_movl_ECX_T1
	.p2align 4,,15
.globl op_movl_ECX_A0
	.type	op_movl_ECX_A0, @function
op_movl_ECX_A0:
	movl	%edi, 4(%ebp)
	ret
	.size	op_movl_ECX_A0, .-op_movl_ECX_A0
	.p2align 4,,15
.globl op_cmovw_ECX_T1_T0
	.type	op_cmovw_ECX_T1_T0, @function
op_cmovw_ECX_T1_T0:
	testl	%ebx, %ebx
	je	.L37
	movw	%si, 4(%ebp)
.L37:
	ret
	.size	op_cmovw_ECX_T1_T0, .-op_cmovw_ECX_T1_T0
	.p2align 4,,15
.globl op_cmovl_ECX_T1_T0
	.type	op_cmovl_ECX_T1_T0, @function
op_cmovl_ECX_T1_T0:
	testl	%ebx, %ebx
	je	.L39
	movl	%esi, 4(%ebp)
.L39:
	ret
	.size	op_cmovl_ECX_T1_T0, .-op_cmovl_ECX_T1_T0
	.p2align 4,,15
.globl op_movw_ECX_T0
	.type	op_movw_ECX_T0, @function
op_movw_ECX_T0:
	movw	%bx, 4(%ebp)
	ret
	.size	op_movw_ECX_T0, .-op_movw_ECX_T0
	.p2align 4,,15
.globl op_movw_ECX_T1
	.type	op_movw_ECX_T1, @function
op_movw_ECX_T1:
	movw	%si, 4(%ebp)
	ret
	.size	op_movw_ECX_T1, .-op_movw_ECX_T1
	.p2align 4,,15
.globl op_movw_ECX_A0
	.type	op_movw_ECX_A0, @function
op_movw_ECX_A0:
	movw	%di, 4(%ebp)
	ret
	.size	op_movw_ECX_A0, .-op_movw_ECX_A0
	.p2align 4,,15
.globl op_movb_ECX_T0
	.type	op_movb_ECX_T0, @function
op_movb_ECX_T0:
	movb	%bl, 4(%ebp)
	ret
	.size	op_movb_ECX_T0, .-op_movb_ECX_T0
	.p2align 4,,15
.globl op_movh_ECX_T0
	.type	op_movh_ECX_T0, @function
op_movh_ECX_T0:
	movb	%bl, 5(%ebp)
	ret
	.size	op_movh_ECX_T0, .-op_movh_ECX_T0
	.p2align 4,,15
.globl op_movb_ECX_T1
	.type	op_movb_ECX_T1, @function
op_movb_ECX_T1:
	movl	%esi, %eax
	movb	%al, 4(%ebp)
	ret
	.size	op_movb_ECX_T1, .-op_movb_ECX_T1
	.p2align 4,,15
.globl op_movh_ECX_T1
	.type	op_movh_ECX_T1, @function
op_movh_ECX_T1:
	movl	%esi, %eax
	movb	%al, 5(%ebp)
	ret
	.size	op_movh_ECX_T1, .-op_movh_ECX_T1
	.p2align 4,,15
.globl op_movl_A0_EDX
	.type	op_movl_A0_EDX, @function
op_movl_A0_EDX:
	movl	8(%ebp), %edi
	ret
	.size	op_movl_A0_EDX, .-op_movl_A0_EDX
	.p2align 4,,15
.globl op_addl_A0_EDX
	.type	op_addl_A0_EDX, @function
op_addl_A0_EDX:
	movl	8(%ebp), %ecx
	addl	%ecx, %edi
	ret
	.size	op_addl_A0_EDX, .-op_addl_A0_EDX
	.p2align 4,,15
.globl op_addl_A0_EDX_s1
	.type	op_addl_A0_EDX_s1, @function
op_addl_A0_EDX_s1:
	movl	8(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_EDX_s1, .-op_addl_A0_EDX_s1
	.p2align 4,,15
.globl op_addl_A0_EDX_s2
	.type	op_addl_A0_EDX_s2, @function
op_addl_A0_EDX_s2:
	movl	8(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_EDX_s2, .-op_addl_A0_EDX_s2
	.p2align 4,,15
.globl op_addl_A0_EDX_s3
	.type	op_addl_A0_EDX_s3, @function
op_addl_A0_EDX_s3:
	movl	8(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_EDX_s3, .-op_addl_A0_EDX_s3
	.p2align 4,,15
.globl op_movl_T0_EDX
	.type	op_movl_T0_EDX, @function
op_movl_T0_EDX:
	movl	8(%ebp), %ebx
	ret
	.size	op_movl_T0_EDX, .-op_movl_T0_EDX
	.p2align 4,,15
.globl op_movl_T1_EDX
	.type	op_movl_T1_EDX, @function
op_movl_T1_EDX:
	movl	8(%ebp), %esi
	ret
	.size	op_movl_T1_EDX, .-op_movl_T1_EDX
	.p2align 4,,15
.globl op_movh_T0_EDX
	.type	op_movh_T0_EDX, @function
op_movh_T0_EDX:
	movl	8(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_EDX, .-op_movh_T0_EDX
	.p2align 4,,15
.globl op_movh_T1_EDX
	.type	op_movh_T1_EDX, @function
op_movh_T1_EDX:
	movl	8(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_EDX, .-op_movh_T1_EDX
	.p2align 4,,15
.globl op_movl_EDX_T0
	.type	op_movl_EDX_T0, @function
op_movl_EDX_T0:
	movl	%ebx, 8(%ebp)
	ret
	.size	op_movl_EDX_T0, .-op_movl_EDX_T0
	.p2align 4,,15
.globl op_movl_EDX_T1
	.type	op_movl_EDX_T1, @function
op_movl_EDX_T1:
	movl	%esi, 8(%ebp)
	ret
	.size	op_movl_EDX_T1, .-op_movl_EDX_T1
	.p2align 4,,15
.globl op_movl_EDX_A0
	.type	op_movl_EDX_A0, @function
op_movl_EDX_A0:
	movl	%edi, 8(%ebp)
	ret
	.size	op_movl_EDX_A0, .-op_movl_EDX_A0
	.p2align 4,,15
.globl op_cmovw_EDX_T1_T0
	.type	op_cmovw_EDX_T1_T0, @function
op_cmovw_EDX_T1_T0:
	testl	%ebx, %ebx
	je	.L60
	movw	%si, 8(%ebp)
.L60:
	ret
	.size	op_cmovw_EDX_T1_T0, .-op_cmovw_EDX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EDX_T1_T0
	.type	op_cmovl_EDX_T1_T0, @function
op_cmovl_EDX_T1_T0:
	testl	%ebx, %ebx
	je	.L62
	movl	%esi, 8(%ebp)
.L62:
	ret
	.size	op_cmovl_EDX_T1_T0, .-op_cmovl_EDX_T1_T0
	.p2align 4,,15
.globl op_movw_EDX_T0
	.type	op_movw_EDX_T0, @function
op_movw_EDX_T0:
	movw	%bx, 8(%ebp)
	ret
	.size	op_movw_EDX_T0, .-op_movw_EDX_T0
	.p2align 4,,15
.globl op_movw_EDX_T1
	.type	op_movw_EDX_T1, @function
op_movw_EDX_T1:
	movw	%si, 8(%ebp)
	ret
	.size	op_movw_EDX_T1, .-op_movw_EDX_T1
	.p2align 4,,15
.globl op_movw_EDX_A0
	.type	op_movw_EDX_A0, @function
op_movw_EDX_A0:
	movw	%di, 8(%ebp)
	ret
	.size	op_movw_EDX_A0, .-op_movw_EDX_A0
	.p2align 4,,15
.globl op_movb_EDX_T0
	.type	op_movb_EDX_T0, @function
op_movb_EDX_T0:
	movb	%bl, 8(%ebp)
	ret
	.size	op_movb_EDX_T0, .-op_movb_EDX_T0
	.p2align 4,,15
.globl op_movh_EDX_T0
	.type	op_movh_EDX_T0, @function
op_movh_EDX_T0:
	movb	%bl, 9(%ebp)
	ret
	.size	op_movh_EDX_T0, .-op_movh_EDX_T0
	.p2align 4,,15
.globl op_movb_EDX_T1
	.type	op_movb_EDX_T1, @function
op_movb_EDX_T1:
	movl	%esi, %eax
	movb	%al, 8(%ebp)
	ret
	.size	op_movb_EDX_T1, .-op_movb_EDX_T1
	.p2align 4,,15
.globl op_movh_EDX_T1
	.type	op_movh_EDX_T1, @function
op_movh_EDX_T1:
	movl	%esi, %eax
	movb	%al, 9(%ebp)
	ret
	.size	op_movh_EDX_T1, .-op_movh_EDX_T1
	.p2align 4,,15
.globl op_movl_A0_EBX
	.type	op_movl_A0_EBX, @function
op_movl_A0_EBX:
	movl	12(%ebp), %edi
	ret
	.size	op_movl_A0_EBX, .-op_movl_A0_EBX
	.p2align 4,,15
.globl op_addl_A0_EBX
	.type	op_addl_A0_EBX, @function
op_addl_A0_EBX:
	movl	12(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_EBX, .-op_addl_A0_EBX
	.p2align 4,,15
.globl op_addl_A0_EBX_s1
	.type	op_addl_A0_EBX_s1, @function
op_addl_A0_EBX_s1:
	movl	12(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_EBX_s1, .-op_addl_A0_EBX_s1
	.p2align 4,,15
.globl op_addl_A0_EBX_s2
	.type	op_addl_A0_EBX_s2, @function
op_addl_A0_EBX_s2:
	movl	12(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_EBX_s2, .-op_addl_A0_EBX_s2
	.p2align 4,,15
.globl op_addl_A0_EBX_s3
	.type	op_addl_A0_EBX_s3, @function
op_addl_A0_EBX_s3:
	movl	12(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_EBX_s3, .-op_addl_A0_EBX_s3
	.p2align 4,,15
.globl op_movl_T0_EBX
	.type	op_movl_T0_EBX, @function
op_movl_T0_EBX:
	movl	12(%ebp), %ebx
	ret
	.size	op_movl_T0_EBX, .-op_movl_T0_EBX
	.p2align 4,,15
.globl op_movl_T1_EBX
	.type	op_movl_T1_EBX, @function
op_movl_T1_EBX:
	movl	12(%ebp), %esi
	ret
	.size	op_movl_T1_EBX, .-op_movl_T1_EBX
	.p2align 4,,15
.globl op_movh_T0_EBX
	.type	op_movh_T0_EBX, @function
op_movh_T0_EBX:
	movl	12(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_EBX, .-op_movh_T0_EBX
	.p2align 4,,15
.globl op_movh_T1_EBX
	.type	op_movh_T1_EBX, @function
op_movh_T1_EBX:
	movl	12(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_EBX, .-op_movh_T1_EBX
	.p2align 4,,15
.globl op_movl_EBX_T0
	.type	op_movl_EBX_T0, @function
op_movl_EBX_T0:
	movl	%ebx, 12(%ebp)
	ret
	.size	op_movl_EBX_T0, .-op_movl_EBX_T0
	.p2align 4,,15
.globl op_movl_EBX_T1
	.type	op_movl_EBX_T1, @function
op_movl_EBX_T1:
	movl	%esi, 12(%ebp)
	ret
	.size	op_movl_EBX_T1, .-op_movl_EBX_T1
	.p2align 4,,15
.globl op_movl_EBX_A0
	.type	op_movl_EBX_A0, @function
op_movl_EBX_A0:
	movl	%edi, 12(%ebp)
	ret
	.size	op_movl_EBX_A0, .-op_movl_EBX_A0
	.p2align 4,,15
.globl op_cmovw_EBX_T1_T0
	.type	op_cmovw_EBX_T1_T0, @function
op_cmovw_EBX_T1_T0:
	testl	%ebx, %ebx
	je	.L83
	movw	%si, 12(%ebp)
.L83:
	ret
	.size	op_cmovw_EBX_T1_T0, .-op_cmovw_EBX_T1_T0
	.p2align 4,,15
.globl op_cmovl_EBX_T1_T0
	.type	op_cmovl_EBX_T1_T0, @function
op_cmovl_EBX_T1_T0:
	testl	%ebx, %ebx
	je	.L85
	movl	%esi, 12(%ebp)
.L85:
	ret
	.size	op_cmovl_EBX_T1_T0, .-op_cmovl_EBX_T1_T0
	.p2align 4,,15
.globl op_movw_EBX_T0
	.type	op_movw_EBX_T0, @function
op_movw_EBX_T0:
	movw	%bx, 12(%ebp)
	ret
	.size	op_movw_EBX_T0, .-op_movw_EBX_T0
	.p2align 4,,15
.globl op_movw_EBX_T1
	.type	op_movw_EBX_T1, @function
op_movw_EBX_T1:
	movw	%si, 12(%ebp)
	ret
	.size	op_movw_EBX_T1, .-op_movw_EBX_T1
	.p2align 4,,15
.globl op_movw_EBX_A0
	.type	op_movw_EBX_A0, @function
op_movw_EBX_A0:
	movw	%di, 12(%ebp)
	ret
	.size	op_movw_EBX_A0, .-op_movw_EBX_A0
	.p2align 4,,15
.globl op_movb_EBX_T0
	.type	op_movb_EBX_T0, @function
op_movb_EBX_T0:
	movb	%bl, 12(%ebp)
	ret
	.size	op_movb_EBX_T0, .-op_movb_EBX_T0
	.p2align 4,,15
.globl op_movh_EBX_T0
	.type	op_movh_EBX_T0, @function
op_movh_EBX_T0:
	movb	%bl, 13(%ebp)
	ret
	.size	op_movh_EBX_T0, .-op_movh_EBX_T0
	.p2align 4,,15
.globl op_movb_EBX_T1
	.type	op_movb_EBX_T1, @function
op_movb_EBX_T1:
	movl	%esi, %eax
	movb	%al, 12(%ebp)
	ret
	.size	op_movb_EBX_T1, .-op_movb_EBX_T1
	.p2align 4,,15
.globl op_movh_EBX_T1
	.type	op_movh_EBX_T1, @function
op_movh_EBX_T1:
	movl	%esi, %eax
	movb	%al, 13(%ebp)
	ret
	.size	op_movh_EBX_T1, .-op_movh_EBX_T1
	.p2align 4,,15
.globl op_movl_A0_ESP
	.type	op_movl_A0_ESP, @function
op_movl_A0_ESP:
	movl	16(%ebp), %edi
	ret
	.size	op_movl_A0_ESP, .-op_movl_A0_ESP
	.p2align 4,,15
.globl op_addl_A0_ESP
	.type	op_addl_A0_ESP, @function
op_addl_A0_ESP:
	movl	16(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_ESP, .-op_addl_A0_ESP
	.p2align 4,,15
.globl op_addl_A0_ESP_s1
	.type	op_addl_A0_ESP_s1, @function
op_addl_A0_ESP_s1:
	movl	16(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_ESP_s1, .-op_addl_A0_ESP_s1
	.p2align 4,,15
.globl op_addl_A0_ESP_s2
	.type	op_addl_A0_ESP_s2, @function
op_addl_A0_ESP_s2:
	movl	16(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_ESP_s2, .-op_addl_A0_ESP_s2
	.p2align 4,,15
.globl op_addl_A0_ESP_s3
	.type	op_addl_A0_ESP_s3, @function
op_addl_A0_ESP_s3:
	movl	16(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_ESP_s3, .-op_addl_A0_ESP_s3
	.p2align 4,,15
.globl op_movl_T0_ESP
	.type	op_movl_T0_ESP, @function
op_movl_T0_ESP:
	movl	16(%ebp), %ebx
	ret
	.size	op_movl_T0_ESP, .-op_movl_T0_ESP
	.p2align 4,,15
.globl op_movl_T1_ESP
	.type	op_movl_T1_ESP, @function
op_movl_T1_ESP:
	movl	16(%ebp), %esi
	ret
	.size	op_movl_T1_ESP, .-op_movl_T1_ESP
	.p2align 4,,15
.globl op_movh_T0_ESP
	.type	op_movh_T0_ESP, @function
op_movh_T0_ESP:
	movl	16(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_ESP, .-op_movh_T0_ESP
	.p2align 4,,15
.globl op_movh_T1_ESP
	.type	op_movh_T1_ESP, @function
op_movh_T1_ESP:
	movl	16(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_ESP, .-op_movh_T1_ESP
	.p2align 4,,15
.globl op_movl_ESP_T0
	.type	op_movl_ESP_T0, @function
op_movl_ESP_T0:
	movl	%ebx, 16(%ebp)
	ret
	.size	op_movl_ESP_T0, .-op_movl_ESP_T0
	.p2align 4,,15
.globl op_movl_ESP_T1
	.type	op_movl_ESP_T1, @function
op_movl_ESP_T1:
	movl	%esi, 16(%ebp)
	ret
	.size	op_movl_ESP_T1, .-op_movl_ESP_T1
	.p2align 4,,15
.globl op_movl_ESP_A0
	.type	op_movl_ESP_A0, @function
op_movl_ESP_A0:
	movl	%edi, 16(%ebp)
	ret
	.size	op_movl_ESP_A0, .-op_movl_ESP_A0
	.p2align 4,,15
.globl op_cmovw_ESP_T1_T0
	.type	op_cmovw_ESP_T1_T0, @function
op_cmovw_ESP_T1_T0:
	testl	%ebx, %ebx
	je	.L106
	movw	%si, 16(%ebp)
.L106:
	ret
	.size	op_cmovw_ESP_T1_T0, .-op_cmovw_ESP_T1_T0
	.p2align 4,,15
.globl op_cmovl_ESP_T1_T0
	.type	op_cmovl_ESP_T1_T0, @function
op_cmovl_ESP_T1_T0:
	testl	%ebx, %ebx
	je	.L108
	movl	%esi, 16(%ebp)
.L108:
	ret
	.size	op_cmovl_ESP_T1_T0, .-op_cmovl_ESP_T1_T0
	.p2align 4,,15
.globl op_movw_ESP_T0
	.type	op_movw_ESP_T0, @function
op_movw_ESP_T0:
	movw	%bx, 16(%ebp)
	ret
	.size	op_movw_ESP_T0, .-op_movw_ESP_T0
	.p2align 4,,15
.globl op_movw_ESP_T1
	.type	op_movw_ESP_T1, @function
op_movw_ESP_T1:
	movw	%si, 16(%ebp)
	ret
	.size	op_movw_ESP_T1, .-op_movw_ESP_T1
	.p2align 4,,15
.globl op_movw_ESP_A0
	.type	op_movw_ESP_A0, @function
op_movw_ESP_A0:
	movw	%di, 16(%ebp)
	ret
	.size	op_movw_ESP_A0, .-op_movw_ESP_A0
	.p2align 4,,15
.globl op_movb_ESP_T0
	.type	op_movb_ESP_T0, @function
op_movb_ESP_T0:
	movb	%bl, 16(%ebp)
	ret
	.size	op_movb_ESP_T0, .-op_movb_ESP_T0
	.p2align 4,,15
.globl op_movh_ESP_T0
	.type	op_movh_ESP_T0, @function
op_movh_ESP_T0:
	movb	%bl, 17(%ebp)
	ret
	.size	op_movh_ESP_T0, .-op_movh_ESP_T0
	.p2align 4,,15
.globl op_movb_ESP_T1
	.type	op_movb_ESP_T1, @function
op_movb_ESP_T1:
	movl	%esi, %eax
	movb	%al, 16(%ebp)
	ret
	.size	op_movb_ESP_T1, .-op_movb_ESP_T1
	.p2align 4,,15
.globl op_movh_ESP_T1
	.type	op_movh_ESP_T1, @function
op_movh_ESP_T1:
	movl	%esi, %eax
	movb	%al, 17(%ebp)
	ret
	.size	op_movh_ESP_T1, .-op_movh_ESP_T1
	.p2align 4,,15
.globl op_movl_A0_EBP
	.type	op_movl_A0_EBP, @function
op_movl_A0_EBP:
	movl	20(%ebp), %edi
	ret
	.size	op_movl_A0_EBP, .-op_movl_A0_EBP
	.p2align 4,,15
.globl op_addl_A0_EBP
	.type	op_addl_A0_EBP, @function
op_addl_A0_EBP:
	movl	20(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_EBP, .-op_addl_A0_EBP
	.p2align 4,,15
.globl op_addl_A0_EBP_s1
	.type	op_addl_A0_EBP_s1, @function
op_addl_A0_EBP_s1:
	movl	20(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_EBP_s1, .-op_addl_A0_EBP_s1
	.p2align 4,,15
.globl op_addl_A0_EBP_s2
	.type	op_addl_A0_EBP_s2, @function
op_addl_A0_EBP_s2:
	movl	20(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_EBP_s2, .-op_addl_A0_EBP_s2
	.p2align 4,,15
.globl op_addl_A0_EBP_s3
	.type	op_addl_A0_EBP_s3, @function
op_addl_A0_EBP_s3:
	movl	20(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_EBP_s3, .-op_addl_A0_EBP_s3
	.p2align 4,,15
.globl op_movl_T0_EBP
	.type	op_movl_T0_EBP, @function
op_movl_T0_EBP:
	movl	20(%ebp), %ebx
	ret
	.size	op_movl_T0_EBP, .-op_movl_T0_EBP
	.p2align 4,,15
.globl op_movl_T1_EBP
	.type	op_movl_T1_EBP, @function
op_movl_T1_EBP:
	movl	20(%ebp), %esi
	ret
	.size	op_movl_T1_EBP, .-op_movl_T1_EBP
	.p2align 4,,15
.globl op_movh_T0_EBP
	.type	op_movh_T0_EBP, @function
op_movh_T0_EBP:
	movl	20(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_EBP, .-op_movh_T0_EBP
	.p2align 4,,15
.globl op_movh_T1_EBP
	.type	op_movh_T1_EBP, @function
op_movh_T1_EBP:
	movl	20(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_EBP, .-op_movh_T1_EBP
	.p2align 4,,15
.globl op_movl_EBP_T0
	.type	op_movl_EBP_T0, @function
op_movl_EBP_T0:
	movl	%ebx, 20(%ebp)
	ret
	.size	op_movl_EBP_T0, .-op_movl_EBP_T0
	.p2align 4,,15
.globl op_movl_EBP_T1
	.type	op_movl_EBP_T1, @function
op_movl_EBP_T1:
	movl	%esi, 20(%ebp)
	ret
	.size	op_movl_EBP_T1, .-op_movl_EBP_T1
	.p2align 4,,15
.globl op_movl_EBP_A0
	.type	op_movl_EBP_A0, @function
op_movl_EBP_A0:
	movl	%edi, 20(%ebp)
	ret
	.size	op_movl_EBP_A0, .-op_movl_EBP_A0
	.p2align 4,,15
.globl op_cmovw_EBP_T1_T0
	.type	op_cmovw_EBP_T1_T0, @function
op_cmovw_EBP_T1_T0:
	testl	%ebx, %ebx
	je	.L129
	movw	%si, 20(%ebp)
.L129:
	ret
	.size	op_cmovw_EBP_T1_T0, .-op_cmovw_EBP_T1_T0
	.p2align 4,,15
.globl op_cmovl_EBP_T1_T0
	.type	op_cmovl_EBP_T1_T0, @function
op_cmovl_EBP_T1_T0:
	testl	%ebx, %ebx
	je	.L131
	movl	%esi, 20(%ebp)
.L131:
	ret
	.size	op_cmovl_EBP_T1_T0, .-op_cmovl_EBP_T1_T0
	.p2align 4,,15
.globl op_movw_EBP_T0
	.type	op_movw_EBP_T0, @function
op_movw_EBP_T0:
	movw	%bx, 20(%ebp)
	ret
	.size	op_movw_EBP_T0, .-op_movw_EBP_T0
	.p2align 4,,15
.globl op_movw_EBP_T1
	.type	op_movw_EBP_T1, @function
op_movw_EBP_T1:
	movw	%si, 20(%ebp)
	ret
	.size	op_movw_EBP_T1, .-op_movw_EBP_T1
	.p2align 4,,15
.globl op_movw_EBP_A0
	.type	op_movw_EBP_A0, @function
op_movw_EBP_A0:
	movw	%di, 20(%ebp)
	ret
	.size	op_movw_EBP_A0, .-op_movw_EBP_A0
	.p2align 4,,15
.globl op_movb_EBP_T0
	.type	op_movb_EBP_T0, @function
op_movb_EBP_T0:
	movb	%bl, 20(%ebp)
	ret
	.size	op_movb_EBP_T0, .-op_movb_EBP_T0
	.p2align 4,,15
.globl op_movh_EBP_T0
	.type	op_movh_EBP_T0, @function
op_movh_EBP_T0:
	movb	%bl, 21(%ebp)
	ret
	.size	op_movh_EBP_T0, .-op_movh_EBP_T0
	.p2align 4,,15
.globl op_movb_EBP_T1
	.type	op_movb_EBP_T1, @function
op_movb_EBP_T1:
	movl	%esi, %eax
	movb	%al, 20(%ebp)
	ret
	.size	op_movb_EBP_T1, .-op_movb_EBP_T1
	.p2align 4,,15
.globl op_movh_EBP_T1
	.type	op_movh_EBP_T1, @function
op_movh_EBP_T1:
	movl	%esi, %eax
	movb	%al, 21(%ebp)
	ret
	.size	op_movh_EBP_T1, .-op_movh_EBP_T1
	.p2align 4,,15
.globl op_movl_A0_ESI
	.type	op_movl_A0_ESI, @function
op_movl_A0_ESI:
	movl	24(%ebp), %edi
	ret
	.size	op_movl_A0_ESI, .-op_movl_A0_ESI
	.p2align 4,,15
.globl op_addl_A0_ESI
	.type	op_addl_A0_ESI, @function
op_addl_A0_ESI:
	movl	24(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_ESI, .-op_addl_A0_ESI
	.p2align 4,,15
.globl op_addl_A0_ESI_s1
	.type	op_addl_A0_ESI_s1, @function
op_addl_A0_ESI_s1:
	movl	24(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_ESI_s1, .-op_addl_A0_ESI_s1
	.p2align 4,,15
.globl op_addl_A0_ESI_s2
	.type	op_addl_A0_ESI_s2, @function
op_addl_A0_ESI_s2:
	movl	24(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_ESI_s2, .-op_addl_A0_ESI_s2
	.p2align 4,,15
.globl op_addl_A0_ESI_s3
	.type	op_addl_A0_ESI_s3, @function
op_addl_A0_ESI_s3:
	movl	24(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_ESI_s3, .-op_addl_A0_ESI_s3
	.p2align 4,,15
.globl op_movl_T0_ESI
	.type	op_movl_T0_ESI, @function
op_movl_T0_ESI:
	movl	24(%ebp), %ebx
	ret
	.size	op_movl_T0_ESI, .-op_movl_T0_ESI
	.p2align 4,,15
.globl op_movl_T1_ESI
	.type	op_movl_T1_ESI, @function
op_movl_T1_ESI:
	movl	24(%ebp), %esi
	ret
	.size	op_movl_T1_ESI, .-op_movl_T1_ESI
	.p2align 4,,15
.globl op_movh_T0_ESI
	.type	op_movh_T0_ESI, @function
op_movh_T0_ESI:
	movl	24(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_ESI, .-op_movh_T0_ESI
	.p2align 4,,15
.globl op_movh_T1_ESI
	.type	op_movh_T1_ESI, @function
op_movh_T1_ESI:
	movl	24(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_ESI, .-op_movh_T1_ESI
	.p2align 4,,15
.globl op_movl_ESI_T0
	.type	op_movl_ESI_T0, @function
op_movl_ESI_T0:
	movl	%ebx, 24(%ebp)
	ret
	.size	op_movl_ESI_T0, .-op_movl_ESI_T0
	.p2align 4,,15
.globl op_movl_ESI_T1
	.type	op_movl_ESI_T1, @function
op_movl_ESI_T1:
	movl	%esi, 24(%ebp)
	ret
	.size	op_movl_ESI_T1, .-op_movl_ESI_T1
	.p2align 4,,15
.globl op_movl_ESI_A0
	.type	op_movl_ESI_A0, @function
op_movl_ESI_A0:
	movl	%edi, 24(%ebp)
	ret
	.size	op_movl_ESI_A0, .-op_movl_ESI_A0
	.p2align 4,,15
.globl op_cmovw_ESI_T1_T0
	.type	op_cmovw_ESI_T1_T0, @function
op_cmovw_ESI_T1_T0:
	testl	%ebx, %ebx
	je	.L152
	movw	%si, 24(%ebp)
.L152:
	ret
	.size	op_cmovw_ESI_T1_T0, .-op_cmovw_ESI_T1_T0
	.p2align 4,,15
.globl op_cmovl_ESI_T1_T0
	.type	op_cmovl_ESI_T1_T0, @function
op_cmovl_ESI_T1_T0:
	testl	%ebx, %ebx
	je	.L154
	movl	%esi, 24(%ebp)
.L154:
	ret
	.size	op_cmovl_ESI_T1_T0, .-op_cmovl_ESI_T1_T0
	.p2align 4,,15
.globl op_movw_ESI_T0
	.type	op_movw_ESI_T0, @function
op_movw_ESI_T0:
	movw	%bx, 24(%ebp)
	ret
	.size	op_movw_ESI_T0, .-op_movw_ESI_T0
	.p2align 4,,15
.globl op_movw_ESI_T1
	.type	op_movw_ESI_T1, @function
op_movw_ESI_T1:
	movw	%si, 24(%ebp)
	ret
	.size	op_movw_ESI_T1, .-op_movw_ESI_T1
	.p2align 4,,15
.globl op_movw_ESI_A0
	.type	op_movw_ESI_A0, @function
op_movw_ESI_A0:
	movw	%di, 24(%ebp)
	ret
	.size	op_movw_ESI_A0, .-op_movw_ESI_A0
	.p2align 4,,15
.globl op_movb_ESI_T0
	.type	op_movb_ESI_T0, @function
op_movb_ESI_T0:
	movb	%bl, 24(%ebp)
	ret
	.size	op_movb_ESI_T0, .-op_movb_ESI_T0
	.p2align 4,,15
.globl op_movh_ESI_T0
	.type	op_movh_ESI_T0, @function
op_movh_ESI_T0:
	movb	%bl, 25(%ebp)
	ret
	.size	op_movh_ESI_T0, .-op_movh_ESI_T0
	.p2align 4,,15
.globl op_movb_ESI_T1
	.type	op_movb_ESI_T1, @function
op_movb_ESI_T1:
	movl	%esi, %eax
	movb	%al, 24(%ebp)
	ret
	.size	op_movb_ESI_T1, .-op_movb_ESI_T1
	.p2align 4,,15
.globl op_movh_ESI_T1
	.type	op_movh_ESI_T1, @function
op_movh_ESI_T1:
	movl	%esi, %eax
	movb	%al, 25(%ebp)
	ret
	.size	op_movh_ESI_T1, .-op_movh_ESI_T1
	.p2align 4,,15
.globl op_movl_A0_EDI
	.type	op_movl_A0_EDI, @function
op_movl_A0_EDI:
	movl	28(%ebp), %edi
	ret
	.size	op_movl_A0_EDI, .-op_movl_A0_EDI
	.p2align 4,,15
.globl op_addl_A0_EDI
	.type	op_addl_A0_EDI, @function
op_addl_A0_EDI:
	movl	28(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_EDI, .-op_addl_A0_EDI
	.p2align 4,,15
.globl op_addl_A0_EDI_s1
	.type	op_addl_A0_EDI_s1, @function
op_addl_A0_EDI_s1:
	movl	28(%ebp), %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_addl_A0_EDI_s1, .-op_addl_A0_EDI_s1
	.p2align 4,,15
.globl op_addl_A0_EDI_s2
	.type	op_addl_A0_EDI_s2, @function
op_addl_A0_EDI_s2:
	movl	28(%ebp), %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_addl_A0_EDI_s2, .-op_addl_A0_EDI_s2
	.p2align 4,,15
.globl op_addl_A0_EDI_s3
	.type	op_addl_A0_EDI_s3, @function
op_addl_A0_EDI_s3:
	movl	28(%ebp), %eax
	leal	(%edi,%eax,8), %edi
	ret
	.size	op_addl_A0_EDI_s3, .-op_addl_A0_EDI_s3
	.p2align 4,,15
.globl op_movl_T0_EDI
	.type	op_movl_T0_EDI, @function
op_movl_T0_EDI:
	movl	28(%ebp), %ebx
	ret
	.size	op_movl_T0_EDI, .-op_movl_T0_EDI
	.p2align 4,,15
.globl op_movl_T1_EDI
	.type	op_movl_T1_EDI, @function
op_movl_T1_EDI:
	movl	28(%ebp), %esi
	ret
	.size	op_movl_T1_EDI, .-op_movl_T1_EDI
	.p2align 4,,15
.globl op_movh_T0_EDI
	.type	op_movh_T0_EDI, @function
op_movh_T0_EDI:
	movl	28(%ebp), %eax
	movl	%eax, %ebx
	shrl	$8, %ebx
	ret
	.size	op_movh_T0_EDI, .-op_movh_T0_EDI
	.p2align 4,,15
.globl op_movh_T1_EDI
	.type	op_movh_T1_EDI, @function
op_movh_T1_EDI:
	movl	28(%ebp), %eax
	movl	%eax, %esi
	shrl	$8, %esi
	ret
	.size	op_movh_T1_EDI, .-op_movh_T1_EDI
	.p2align 4,,15
.globl op_movl_EDI_T0
	.type	op_movl_EDI_T0, @function
op_movl_EDI_T0:
	movl	%ebx, 28(%ebp)
	ret
	.size	op_movl_EDI_T0, .-op_movl_EDI_T0
	.p2align 4,,15
.globl op_movl_EDI_T1
	.type	op_movl_EDI_T1, @function
op_movl_EDI_T1:
	movl	%esi, 28(%ebp)
	ret
	.size	op_movl_EDI_T1, .-op_movl_EDI_T1
	.p2align 4,,15
.globl op_movl_EDI_A0
	.type	op_movl_EDI_A0, @function
op_movl_EDI_A0:
	movl	%edi, 28(%ebp)
	ret
	.size	op_movl_EDI_A0, .-op_movl_EDI_A0
	.p2align 4,,15
.globl op_cmovw_EDI_T1_T0
	.type	op_cmovw_EDI_T1_T0, @function
op_cmovw_EDI_T1_T0:
	testl	%ebx, %ebx
	je	.L175
	movw	%si, 28(%ebp)
.L175:
	ret
	.size	op_cmovw_EDI_T1_T0, .-op_cmovw_EDI_T1_T0
	.p2align 4,,15
.globl op_cmovl_EDI_T1_T0
	.type	op_cmovl_EDI_T1_T0, @function
op_cmovl_EDI_T1_T0:
	testl	%ebx, %ebx
	je	.L177
	movl	%esi, 28(%ebp)
.L177:
	ret
	.size	op_cmovl_EDI_T1_T0, .-op_cmovl_EDI_T1_T0
	.p2align 4,,15
.globl op_movw_EDI_T0
	.type	op_movw_EDI_T0, @function
op_movw_EDI_T0:
	movw	%bx, 28(%ebp)
	ret
	.size	op_movw_EDI_T0, .-op_movw_EDI_T0
	.p2align 4,,15
.globl op_movw_EDI_T1
	.type	op_movw_EDI_T1, @function
op_movw_EDI_T1:
	movw	%si, 28(%ebp)
	ret
	.size	op_movw_EDI_T1, .-op_movw_EDI_T1
	.p2align 4,,15
.globl op_movw_EDI_A0
	.type	op_movw_EDI_A0, @function
op_movw_EDI_A0:
	movw	%di, 28(%ebp)
	ret
	.size	op_movw_EDI_A0, .-op_movw_EDI_A0
	.p2align 4,,15
.globl op_movb_EDI_T0
	.type	op_movb_EDI_T0, @function
op_movb_EDI_T0:
	movb	%bl, 28(%ebp)
	ret
	.size	op_movb_EDI_T0, .-op_movb_EDI_T0
	.p2align 4,,15
.globl op_movh_EDI_T0
	.type	op_movh_EDI_T0, @function
op_movh_EDI_T0:
	movb	%bl, 29(%ebp)
	ret
	.size	op_movh_EDI_T0, .-op_movh_EDI_T0
	.p2align 4,,15
.globl op_movb_EDI_T1
	.type	op_movb_EDI_T1, @function
op_movb_EDI_T1:
	movl	%esi, %eax
	movb	%al, 28(%ebp)
	ret
	.size	op_movb_EDI_T1, .-op_movb_EDI_T1
	.p2align 4,,15
.globl op_movh_EDI_T1
	.type	op_movh_EDI_T1, @function
op_movh_EDI_T1:
	movl	%esi, %eax
	movb	%al, 29(%ebp)
	ret
	.size	op_movh_EDI_T1, .-op_movh_EDI_T1
	.p2align 4,,15
.globl op_update2_cc
	.type	op_update2_cc, @function
op_update2_cc:
	movl	%esi, 40(%ebp)
	movl	%ebx, 44(%ebp)
	ret
	.size	op_update2_cc, .-op_update2_cc
	.p2align 4,,15
.globl op_update1_cc
	.type	op_update1_cc, @function
op_update1_cc:
	movl	%ebx, 44(%ebp)
	ret
	.size	op_update1_cc, .-op_update1_cc
	.p2align 4,,15
.globl op_update_neg_cc
	.type	op_update_neg_cc, @function
op_update_neg_cc:
	movl	%ebx, 44(%ebp)
	movl	%ebx, %eax
	negl	%eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_update_neg_cc, .-op_update_neg_cc
	.p2align 4,,15
.globl op_cmpl_T0_T1_cc
	.type	op_cmpl_T0_T1_cc, @function
op_cmpl_T0_T1_cc:
	movl	%esi, 40(%ebp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%eax, 44(%ebp)
	ret
	.size	op_cmpl_T0_T1_cc, .-op_cmpl_T0_T1_cc
	.p2align 4,,15
.globl op_update_inc_cc
	.type	op_update_inc_cc, @function
op_update_inc_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
	ret
	.size	op_update_inc_cc, .-op_update_inc_cc
	.p2align 4,,15
.globl op_testl_T0_T1_cc
	.type	op_testl_T0_T1_cc, @function
op_testl_T0_T1_cc:
	movl	%ebx, %eax
	andl	%esi, %eax
	movl	%eax, 44(%ebp)
	ret
	.size	op_testl_T0_T1_cc, .-op_testl_T0_T1_cc
	.p2align 4,,15
.globl op_addl_T0_T1
	.type	op_addl_T0_T1, @function
op_addl_T0_T1:
	addl	%esi, %ebx
	ret
	.size	op_addl_T0_T1, .-op_addl_T0_T1
	.p2align 4,,15
.globl op_orl_T0_T1
	.type	op_orl_T0_T1, @function
op_orl_T0_T1:
	orl	%esi, %ebx
	ret
	.size	op_orl_T0_T1, .-op_orl_T0_T1
	.p2align 4,,15
.globl op_andl_T0_T1
	.type	op_andl_T0_T1, @function
op_andl_T0_T1:
	andl	%esi, %ebx
	ret
	.size	op_andl_T0_T1, .-op_andl_T0_T1
	.p2align 4,,15
.globl op_subl_T0_T1
	.type	op_subl_T0_T1, @function
op_subl_T0_T1:
	subl	%esi, %ebx
	ret
	.size	op_subl_T0_T1, .-op_subl_T0_T1
	.p2align 4,,15
.globl op_xorl_T0_T1
	.type	op_xorl_T0_T1, @function
op_xorl_T0_T1:
	xorl	%esi, %ebx
	ret
	.size	op_xorl_T0_T1, .-op_xorl_T0_T1
	.p2align 4,,15
.globl op_negl_T0
	.type	op_negl_T0, @function
op_negl_T0:
	negl	%ebx
	ret
	.size	op_negl_T0, .-op_negl_T0
	.p2align 4,,15
.globl op_incl_T0
	.type	op_incl_T0, @function
op_incl_T0:
	incl	%ebx
	ret
	.size	op_incl_T0, .-op_incl_T0
	.p2align 4,,15
.globl op_decl_T0
	.type	op_decl_T0, @function
op_decl_T0:
	decl	%ebx
	ret
	.size	op_decl_T0, .-op_decl_T0
	.p2align 4,,15
.globl op_notl_T0
	.type	op_notl_T0, @function
op_notl_T0:
	notl	%ebx
	ret
	.size	op_notl_T0, .-op_notl_T0
	.p2align 4,,15
.globl op_bswapl_T0
	.type	op_bswapl_T0, @function
op_bswapl_T0:
#APP
	rorw $8, %bx;rorl $16, %ebx;rorw $8, %bx
#NO_APP
	ret
	.size	op_bswapl_T0, .-op_bswapl_T0
	.p2align 4,,15
.globl op_mulb_AL_T0
	.type	op_mulb_AL_T0, @function
op_mulb_AL_T0:
	movzbl	(%ebp), %eax
	movzbl	%bl, %edx
	imull	%edx, %eax
	movw	%ax, (%ebp)
	movl	%eax, 44(%ebp)
	andl	$65280, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_mulb_AL_T0, .-op_mulb_AL_T0
	.p2align 4,,15
.globl op_imulb_AL_T0
	.type	op_imulb_AL_T0, @function
op_imulb_AL_T0:
	movsbl	(%ebp),%eax
	movsbl	%bl,%edx
	imull	%edx, %eax
	movw	%ax, (%ebp)
	movsbl	%al,%edx
	cmpl	%eax, %edx
	movl	%eax, 44(%ebp)
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_imulb_AL_T0, .-op_imulb_AL_T0
	.p2align 4,,15
.globl op_mulw_AX_T0
	.type	op_mulw_AX_T0, @function
op_mulw_AX_T0:
	movzwl	(%ebp), %eax
	movzwl	%bx, %edx
	imull	%edx, %eax
	movw	%ax, (%ebp)
	movl	%eax, %edx
	shrl	$16, %edx
	movw	%dx, 8(%ebp)
	movl	%eax, 44(%ebp)
	movl	%edx, 40(%ebp)
	ret
	.size	op_mulw_AX_T0, .-op_mulw_AX_T0
	.p2align 4,,15
.globl op_imulw_AX_T0
	.type	op_imulw_AX_T0, @function
op_imulw_AX_T0:
	movswl	(%ebp),%eax
	movswl	%bx,%edx
	imull	%edx, %eax
	movw	%ax, (%ebp)
	movl	%eax, %edx
	shrl	$16, %edx
	movw	%dx, 8(%ebp)
	movswl	%ax,%edx
	cmpl	%eax, %edx
	movl	%eax, 44(%ebp)
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_imulw_AX_T0, .-op_imulw_AX_T0
	.p2align 4,,15
.globl op_mull_EAX_T0
	.type	op_mull_EAX_T0, @function
op_mull_EAX_T0:
	subl	$16, %esp
	movl	(%ebp), %eax
	mull	%ebx
	movl	%eax, (%esp)
	movl	%eax, (%ebp)
	movl	%edx, %eax
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%esp), %edx
	movl	%eax, 8(%ebp)
	movl	8(%esp), %eax
	movl	%edx, 44(%ebp)
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_mull_EAX_T0, .-op_mull_EAX_T0
	.p2align 4,,15
.globl op_imull_EAX_T0
	.type	op_imull_EAX_T0, @function
op_imull_EAX_T0:
	subl	$8, %esp
	movl	(%ebp), %eax
	imull	%ebx
	movl	%eax, (%esp)
	movl	(%esp), %ecx
	movl	%edx, 4(%esp)
	movl	%edx, 8(%ebp)
	movl	(%esp), %edx
	movl	%eax, (%ebp)
	movl	%edx, %eax
	movl	%edx, 44(%ebp)
	cltd
	xorl	%ecx, %eax
	movl	4(%esp), %ecx
	xorl	%ecx, %edx
	orl	%edx, %eax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%ebp)
	addl	$8, %esp
	ret
	.size	op_imull_EAX_T0, .-op_imull_EAX_T0
	.p2align 4,,15
.globl op_imulw_T0_T1
	.type	op_imulw_T0_T1, @function
op_imulw_T0_T1:
	movswl	%bx,%eax
	movswl	%si,%edx
	imull	%edx, %eax
	movl	%eax, 44(%ebp)
	movswl	%ax,%edx
	movl	%eax, %ebx
	cmpl	%eax, %edx
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_imulw_T0_T1, .-op_imulw_T0_T1
	.p2align 4,,15
.globl op_imull_T0_T1
	.type	op_imull_T0_T1, @function
op_imull_T0_T1:
	movl	%ebx, %eax
	subl	$8, %esp
	imull	%esi
	movl	%eax, (%esp)
	movl	%eax, %ebx
	movl	(%esp), %ecx
	movl	%edx, 4(%esp)
	movl	(%esp), %edx
	movl	%edx, %eax
	movl	%edx, 44(%ebp)
	cltd
	xorl	%ecx, %eax
	movl	4(%esp), %ecx
	xorl	%ecx, %edx
	orl	%edx, %eax
	setne	%al
	movzbl	%al, %eax
	movl	%eax, 40(%ebp)
	addl	$8, %esp
	ret
	.size	op_imull_T0_T1, .-op_imull_T0_T1
	.p2align 4,,15
.globl op_divb_AL_T0
	.type	op_divb_AL_T0, @function
op_divb_AL_T0:
	subl	$20, %esp
	movl	%ebx, %edx
	andl	$255, %edx
	movzwl	(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 16(%esp)
	jne	.L213
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L213:
	xorl	%edx, %edx
	movl	16(%esp), %eax
	divl	12(%esp)
	cmpl	$255, %eax
	movl	%edx, 4(%esp)
	movl	%eax, 8(%esp)
	jbe	.L214
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L214:
	sall	$8, 4(%esp)
	movl	(%ebp), %eax
	movzbl	8(%esp),%edx
	movl	4(%esp), %ecx
	andl	$-65536, %eax
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, (%ebp)
	addl	$20, %esp
	ret
	.size	op_divb_AL_T0, .-op_divb_AL_T0
	.p2align 4,,15
.globl op_idivb_AL_T0
	.type	op_idivb_AL_T0, @function
op_idivb_AL_T0:
	subl	$20, %esp
	movsbl	%bl,%edx
	testl	%edx, %edx
	movswl	(%ebp),%eax
	movl	%edx, 12(%esp)
	movl	%eax, 16(%esp)
	jne	.L216
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L216:
	movl	16(%esp), %eax
	cltd
	idivl	12(%esp)
	movl	%eax, 8(%esp)
	movsbl	8(%esp),%eax
	movl	%edx, 4(%esp)
	cmpl	8(%esp), %eax
	je	.L217
	movl	$0, (%esp)
	call	raise_exception
.L217:
	movzbl	4(%esp),%edx
	movl	(%ebp), %eax
	movzbl	8(%esp),%ecx
	andl	$-65536, %eax
	sall	$8, %edx
	orl	%edx, %eax
	orl	%ecx, %eax
	movl	%eax, (%ebp)
	addl	$20, %esp
	ret
	.size	op_idivb_AL_T0, .-op_idivb_AL_T0
	.p2align 4,,15
.globl op_divw_AX_T0
	.type	op_divw_AX_T0, @function
op_divw_AX_T0:
	subl	$20, %esp
	movl	%ebx, %edx
	movzwl	(%ebp), %eax
	movl	%eax, 16(%esp)
	movzwl	8(%ebp), %eax
	sall	$16, %eax
	orl	%eax, 16(%esp)
	andl	$65535, %edx
	movl	%edx, 12(%esp)
	jne	.L219
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L219:
	xorl	%edx, %edx
	movl	16(%esp), %eax
	divl	12(%esp)
	cmpl	$65535, %eax
	movl	%edx, 4(%esp)
	movl	%eax, 8(%esp)
	jbe	.L220
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L220:
	movl	8(%esp), %eax
	movl	4(%esp), %edx
	movw	%ax, (%ebp)
	movw	%dx, 8(%ebp)
	addl	$20, %esp
	ret
	.size	op_divw_AX_T0, .-op_divw_AX_T0
	.p2align 4,,15
.globl op_idivw_AX_T0
	.type	op_idivw_AX_T0, @function
op_idivw_AX_T0:
	subl	$20, %esp
	movswl	%bx,%edx
	movzwl	(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 16(%esp)
	movzwl	8(%ebp), %eax
	sall	$16, %eax
	orl	%eax, 16(%esp)
	testl	%edx, %edx
	jne	.L222
	movl	$0, (%esp)
	call	raise_exception
	.p2align 4,,15
.L222:
	movl	16(%esp), %eax
	cltd
	idivl	12(%esp)
	movl	%eax, 8(%esp)
	movswl	8(%esp),%eax
	movl	%edx, 4(%esp)
	cmpl	8(%esp), %eax
	je	.L223
	movl	$0, (%esp)
	call	raise_exception
.L223:
	movl	8(%esp), %eax
	movl	4(%esp), %edx
	movw	%ax, (%ebp)
	movw	%dx, 8(%ebp)
	addl	$20, %esp
	ret
	.size	op_idivw_AX_T0, .-op_idivw_AX_T0
	.p2align 4,,15
.globl op_divl_EAX_T0
	.type	op_divl_EAX_T0, @function
op_divl_EAX_T0:
	call	helper_divl_EAX_T0
	ret
	.size	op_divl_EAX_T0, .-op_divl_EAX_T0
	.p2align 4,,15
.globl op_idivl_EAX_T0
	.type	op_idivl_EAX_T0, @function
op_idivl_EAX_T0:
	call	helper_idivl_EAX_T0
	ret
	.size	op_idivl_EAX_T0, .-op_idivl_EAX_T0
	.p2align 4,,15
.globl op_movl_T0_imu
	.type	op_movl_T0_imu, @function
op_movl_T0_imu:
	movl	$__op_param1, %ebx
	ret
	.size	op_movl_T0_imu, .-op_movl_T0_imu
	.p2align 4,,15
.globl op_movl_T0_im
	.type	op_movl_T0_im, @function
op_movl_T0_im:
	movl	$__op_param1, %ebx
	ret
	.size	op_movl_T0_im, .-op_movl_T0_im
	.p2align 4,,15
.globl op_addl_T0_im
	.type	op_addl_T0_im, @function
op_addl_T0_im:
	addl	$__op_param1, %ebx
	ret
	.size	op_addl_T0_im, .-op_addl_T0_im
	.p2align 4,,15
.globl op_andl_T0_ffff
	.type	op_andl_T0_ffff, @function
op_andl_T0_ffff:
	andl	$65535, %ebx
	ret
	.size	op_andl_T0_ffff, .-op_andl_T0_ffff
	.p2align 4,,15
.globl op_andl_T0_im
	.type	op_andl_T0_im, @function
op_andl_T0_im:
	andl	$__op_param1, %ebx
	ret
	.size	op_andl_T0_im, .-op_andl_T0_im
	.p2align 4,,15
.globl op_movl_T0_T1
	.type	op_movl_T0_T1, @function
op_movl_T0_T1:
	movl	%esi, %ebx
	ret
	.size	op_movl_T0_T1, .-op_movl_T0_T1
	.p2align 4,,15
.globl op_movl_T1_imu
	.type	op_movl_T1_imu, @function
op_movl_T1_imu:
	movl	$__op_param1, %esi
	ret
	.size	op_movl_T1_imu, .-op_movl_T1_imu
	.p2align 4,,15
.globl op_movl_T1_im
	.type	op_movl_T1_im, @function
op_movl_T1_im:
	movl	$__op_param1, %esi
	ret
	.size	op_movl_T1_im, .-op_movl_T1_im
	.p2align 4,,15
.globl op_addl_T1_im
	.type	op_addl_T1_im, @function
op_addl_T1_im:
	addl	$__op_param1, %esi
	ret
	.size	op_addl_T1_im, .-op_addl_T1_im
	.p2align 4,,15
.globl op_movl_T1_A0
	.type	op_movl_T1_A0, @function
op_movl_T1_A0:
	movl	%edi, %esi
	ret
	.size	op_movl_T1_A0, .-op_movl_T1_A0
	.p2align 4,,15
.globl op_movl_A0_im
	.type	op_movl_A0_im, @function
op_movl_A0_im:
	movl	$__op_param1, %edi
	ret
	.size	op_movl_A0_im, .-op_movl_A0_im
	.p2align 4,,15
.globl op_addl_A0_im
	.type	op_addl_A0_im, @function
op_addl_A0_im:
	addl	$__op_param1, %edi
	ret
	.size	op_addl_A0_im, .-op_addl_A0_im
	.p2align 4,,15
.globl op_movl_A0_seg
	.type	op_movl_A0_seg, @function
op_movl_A0_seg:
	movl	$__op_param1-64, %eax
	movl	$-858993459, %ecx
	subl	$16, %esp
	mull	%ecx
	shrl	$4, %edx
	leal	(%edx,%edx,4), %eax
	movl	%edx, 12(%esp)
	sall	$2, %eax
	movl	76(%eax,%ebp), %ecx
	testl	%ecx, %ecx
	je	.L239
	testb	$2, 38(%ebp)
	jne	.L239
	movl	%edx, 4(%esp)
	movl	76(%eax,%ebp), %eax
	movl	%ebp, (%esp)
	movl	%eax, 8(%esp)
	call	sync_seg
	.p2align 4,,15
.L239:
	movl	12(%esp), %edx
	leal	(%edx,%edx,4), %eax
	movl	64(%ebp,%eax,4), %edi
	addl	$16, %esp
	ret
	.size	op_movl_A0_seg, .-op_movl_A0_seg
	.p2align 4,,15
.globl op_addl_A0_seg
	.type	op_addl_A0_seg, @function
op_addl_A0_seg:
	movl	$__op_param1-64, %eax
	movl	$-858993459, %ecx
	subl	$16, %esp
	mull	%ecx
	shrl	$4, %edx
	leal	(%edx,%edx,4), %eax
	movl	%edx, 12(%esp)
	sall	$2, %eax
	movl	76(%eax,%ebp), %ecx
	testl	%ecx, %ecx
	je	.L241
	testb	$2, 38(%ebp)
	jne	.L241
	movl	%edx, 4(%esp)
	movl	76(%eax,%ebp), %eax
	movl	%ebp, (%esp)
	movl	%eax, 8(%esp)
	call	sync_seg
	.p2align 4,,15
.L241:
	movl	12(%esp), %edx
	leal	(%edx,%edx,4), %eax
	movl	64(%ebp,%eax,4), %edx
	addl	$16, %esp
	addl	%edx, %edi
	ret
	.size	op_addl_A0_seg, .-op_addl_A0_seg
	.p2align 4,,15
.globl op_addl_A0_AL
	.type	op_addl_A0_AL, @function
op_addl_A0_AL:
	movzbl	(%ebp), %eax
	leal	(%eax,%edi), %edi
	ret
	.size	op_addl_A0_AL, .-op_addl_A0_AL
	.p2align 4,,15
.globl op_andl_A0_ffff
	.type	op_andl_A0_ffff, @function
op_andl_A0_ffff:
	andl	$65535, %edi
	ret
	.size	op_andl_A0_ffff, .-op_andl_A0_ffff
	.p2align 4,,15
.globl op_ldub_raw_T0_A0
	.type	op_ldub_raw_T0_A0, @function
op_ldub_raw_T0_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU8
	movzbl	%al, %ebx
	popl	%eax
	ret
	.size	op_ldub_raw_T0_A0, .-op_ldub_raw_T0_A0
	.p2align 4,,15
.globl op_ldsb_raw_T0_A0
	.type	op_ldsb_raw_T0_A0, @function
op_ldsb_raw_T0_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadS8
	movsbl	%al,%ebx
	popl	%eax
	ret
	.size	op_ldsb_raw_T0_A0, .-op_ldsb_raw_T0_A0
	.p2align 4,,15
.globl op_lduw_raw_T0_A0
	.type	op_lduw_raw_T0_A0, @function
op_lduw_raw_T0_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU16
	movzwl	%ax, %ebx
	popl	%eax
	ret
	.size	op_lduw_raw_T0_A0, .-op_lduw_raw_T0_A0
	.p2align 4,,15
.globl op_ldsw_raw_T0_A0
	.type	op_ldsw_raw_T0_A0, @function
op_ldsw_raw_T0_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadS16
	movswl	%ax,%ebx
	popl	%eax
	ret
	.size	op_ldsw_raw_T0_A0, .-op_ldsw_raw_T0_A0
	.p2align 4,,15
.globl op_ldl_raw_T0_A0
	.type	op_ldl_raw_T0_A0, @function
op_ldl_raw_T0_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU32
	movl	%eax, %ebx
	popl	%eax
	ret
	.size	op_ldl_raw_T0_A0, .-op_ldl_raw_T0_A0
	.p2align 4,,15
.globl op_ldub_raw_T1_A0
	.type	op_ldub_raw_T1_A0, @function
op_ldub_raw_T1_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU8
	movzbl	%al, %esi
	popl	%eax
	ret
	.size	op_ldub_raw_T1_A0, .-op_ldub_raw_T1_A0
	.p2align 4,,15
.globl op_ldsb_raw_T1_A0
	.type	op_ldsb_raw_T1_A0, @function
op_ldsb_raw_T1_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadS8
	popl	%edx
	movsbl	%al,%esi
	ret
	.size	op_ldsb_raw_T1_A0, .-op_ldsb_raw_T1_A0
	.p2align 4,,15
.globl op_lduw_raw_T1_A0
	.type	op_lduw_raw_T1_A0, @function
op_lduw_raw_T1_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU16
	popl	%ecx
	movzwl	%ax, %esi
	ret
	.size	op_lduw_raw_T1_A0, .-op_lduw_raw_T1_A0
	.p2align 4,,15
.globl op_ldsw_raw_T1_A0
	.type	op_ldsw_raw_T1_A0, @function
op_ldsw_raw_T1_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadS16
	movswl	%ax,%esi
	popl	%eax
	ret
	.size	op_ldsw_raw_T1_A0, .-op_ldsw_raw_T1_A0
	.p2align 4,,15
.globl op_ldl_raw_T1_A0
	.type	op_ldl_raw_T1_A0, @function
op_ldl_raw_T1_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	remR3PhysReadU32
	movl	%eax, %esi
	popl	%eax
	ret
	.size	op_ldl_raw_T1_A0, .-op_ldl_raw_T1_A0
	.p2align 4,,15
.globl op_stb_raw_T0_A0
	.type	op_stb_raw_T0_A0, @function
op_stb_raw_T0_A0:
	subl	$8, %esp
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU8
	addl	$8, %esp
	ret
	.size	op_stb_raw_T0_A0, .-op_stb_raw_T0_A0
	.p2align 4,,15
.globl op_stw_raw_T0_A0
	.type	op_stw_raw_T0_A0, @function
op_stw_raw_T0_A0:
	subl	$8, %esp
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU16
	addl	$8, %esp
	ret
	.size	op_stw_raw_T0_A0, .-op_stw_raw_T0_A0
	.p2align 4,,15
.globl op_stl_raw_T0_A0
	.type	op_stl_raw_T0_A0, @function
op_stl_raw_T0_A0:
	subl	$8, %esp
	movl	%ebx, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	addl	$8, %esp
	ret
	.size	op_stl_raw_T0_A0, .-op_stl_raw_T0_A0
	.p2align 4,,15
.globl op_stw_raw_T1_A0
	.type	op_stw_raw_T1_A0, @function
op_stw_raw_T1_A0:
	subl	$8, %esp
	movzwl	%si, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU16
	addl	$8, %esp
	ret
	.size	op_stw_raw_T1_A0, .-op_stw_raw_T1_A0
	.p2align 4,,15
.globl op_stl_raw_T1_A0
	.type	op_stl_raw_T1_A0, @function
op_stl_raw_T1_A0:
	subl	$8, %esp
	movl	%esi, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	addl	$8, %esp
	ret
	.size	op_stl_raw_T1_A0, .-op_stl_raw_T1_A0
	.p2align 4,,15
.globl op_ldq_raw_env_A0
	.type	op_ldq_raw_env_A0, @function
op_ldq_raw_env_A0:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysReadU64
	movl	4(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	addl	$8, %esp
	ret
	.size	op_ldq_raw_env_A0, .-op_ldq_raw_env_A0
	.p2align 4,,15
.globl op_stq_raw_env_A0
	.type	op_stq_raw_env_A0, @function
op_stq_raw_env_A0:
	subl	$12, %esp
	movl	__op_param1(%ebp), %eax
	movl	%edi, (%esp)
	movl	__op_param1+4(%ebp), %edx
	movl	%eax, 4(%esp)
	movl	%edx, 8(%esp)
	call	remR3PhysWriteU64
	addl	$12, %esp
	ret
	.size	op_stq_raw_env_A0, .-op_stq_raw_env_A0
	.p2align 4,,15
.globl op_ldo_raw_env_A0
	.type	op_ldo_raw_env_A0, @function
op_ldo_raw_env_A0:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysReadU64
	movl	4(%esp), %ecx
	movl	%eax, (%ecx)
	leal	8(%edi), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%esp)
	call	remR3PhysReadU64
	movl	4(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$8, %esp
	ret
	.size	op_ldo_raw_env_A0, .-op_ldo_raw_env_A0
	.p2align 4,,15
.globl op_sto_raw_env_A0
	.type	op_sto_raw_env_A0, @function
op_sto_raw_env_A0:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, %ecx
	movl	%eax, 12(%esp)
	movl	4(%ecx), %edx
	movl	(%eax), %eax
	movl	%edi, (%esp)
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
	movl	12(%esp), %edx
	leal	8(%edi), %ecx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	movl	%edx, 8(%esp)
	call	remR3PhysWriteU64
	addl	$16, %esp
	ret
	.size	op_sto_raw_env_A0, .-op_sto_raw_env_A0
	.p2align 4,,15
.globl op_ldub_kernel_T0_A0
	.type	op_ldub_kernel_T0_A0, @function
op_ldub_kernel_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldb_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movzbl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldub_kernel_T0_A0, .-op_ldub_kernel_T0_A0
	.p2align 4,,15
.globl op_ldsb_kernel_T0_A0
	.type	op_ldsb_kernel_T0_A0, @function
op_ldsb_kernel_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldb_mmu
popl %edx
movsbl %al, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movsbl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldsb_kernel_T0_A0, .-op_ldsb_kernel_T0_A0
	.p2align 4,,15
.globl op_lduw_kernel_T0_A0
	.type	op_lduw_kernel_T0_A0, @function
op_lduw_kernel_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldw_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movzwl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_lduw_kernel_T0_A0, .-op_lduw_kernel_T0_A0
	.p2align 4,,15
.globl op_ldsw_kernel_T0_A0
	.type	op_ldsw_kernel_T0_A0, @function
op_ldsw_kernel_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldw_mmu
popl %edx
movswl %ax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movswl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldsw_kernel_T0_A0, .-op_ldsw_kernel_T0_A0
	.p2align 4,,15
.globl op_ldl_kernel_T0_A0
	.type	op_ldl_kernel_T0_A0, @function
op_ldl_kernel_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4093, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldl_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldl_kernel_T0_A0, .-op_ldl_kernel_T0_A0
	.p2align 4,,15
.globl op_ldub_kernel_T1_A0
	.type	op_ldub_kernel_T1_A0, @function
op_ldub_kernel_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldb_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movzbl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldub_kernel_T1_A0, .-op_ldub_kernel_T1_A0
	.p2align 4,,15
.globl op_ldsb_kernel_T1_A0
	.type	op_ldsb_kernel_T1_A0, @function
op_ldsb_kernel_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldb_mmu
popl %edx
movsbl %al, %esi
jmp 2f
1:
addl 12(%edx), %eax
movsbl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldsb_kernel_T1_A0, .-op_ldsb_kernel_T1_A0
	.p2align 4,,15
.globl op_lduw_kernel_T1_A0
	.type	op_lduw_kernel_T1_A0, @function
op_lduw_kernel_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldw_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movzwl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_lduw_kernel_T1_A0, .-op_lduw_kernel_T1_A0
	.p2align 4,,15
.globl op_ldsw_kernel_T1_A0
	.type	op_ldsw_kernel_T1_A0, @function
op_ldsw_kernel_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldw_mmu
popl %edx
movswl %ax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movswl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldsw_kernel_T1_A0, .-op_ldsw_kernel_T1_A0
	.p2align 4,,15
.globl op_ldl_kernel_T1_A0
	.type	op_ldl_kernel_T1_A0, @function
op_ldl_kernel_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4093, %eax
andl $4080, %edx
leal 884(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $0
call __ldl_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldl_kernel_T1_A0, .-op_ldl_kernel_T1_A0
	.p2align 4,,15
.globl op_stb_kernel_T0_A0
	.type	op_stb_kernel_T0_A0, @function
op_stb_kernel_T0_A0:
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	subl	$12, %esp
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edi, 8(%esp)
	je	.L305
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L308
	.p2align 4,,7
.L305:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
.L308:
	addl	$12, %esp
	ret
	.size	op_stb_kernel_T0_A0, .-op_stb_kernel_T0_A0
	.p2align 4,,15
.globl op_stw_kernel_T0_A0
	.type	op_stw_kernel_T0_A0, @function
op_stw_kernel_T0_A0:
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	subl	$12, %esp
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edi, 8(%esp)
	je	.L310
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L313
	.p2align 4,,7
.L310:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L313:
	addl	$12, %esp
	ret
	.size	op_stw_kernel_T0_A0, .-op_stw_kernel_T0_A0
	.p2align 4,,15
.globl op_stl_kernel_T0_A0
	.type	op_stl_kernel_T0_A0, @function
op_stl_kernel_T0_A0:
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	subl	$12, %esp
	andl	$4080, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edi, 8(%esp)
	movl	%ebx, %edx
	je	.L315
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L318
	.p2align 4,,7
.L315:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L318:
	addl	$12, %esp
	ret
	.size	op_stl_kernel_T0_A0, .-op_stl_kernel_T0_A0
	.p2align 4,,15
.globl op_stw_kernel_T1_A0
	.type	op_stw_kernel_T1_A0, @function
op_stw_kernel_T1_A0:
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	subl	$12, %esp
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edi, 8(%esp)
	je	.L320
	movl	$0, (%esp)
	movzwl	%si, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L323
	.p2align 4,,7
.L320:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%si, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L323:
	addl	$12, %esp
	ret
	.size	op_stw_kernel_T1_A0, .-op_stw_kernel_T1_A0
	.p2align 4,,15
.globl op_stl_kernel_T1_A0
	.type	op_stl_kernel_T1_A0, @function
op_stl_kernel_T1_A0:
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	subl	$12, %esp
	andl	$4080, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edi, 8(%esp)
	movl	%esi, %edx
	je	.L325
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L328
	.p2align 4,,7
.L325:
	movl	%esi, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L328:
	addl	$12, %esp
	ret
	.size	op_stl_kernel_T1_A0, .-op_stl_kernel_T1_A0
	.p2align 4,,15
.globl op_ldq_kernel_env_A0
	.type	op_ldq_kernel_env_A0, @function
op_ldq_kernel_env_A0:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%edi, %edx
	movl	%eax, 8(%esp)
	shrl	$8, %edx
	movl	%edi, %eax
	movl	%edi, 4(%esp)
	andl	$4080, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L330
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L331
	.p2align 4,,7
.L330:
	movl	896(%edx,%ebp), %ecx
	addl	%ecx, 4(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L331:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	addl	$12, %esp
	ret
	.size	op_ldq_kernel_env_A0, .-op_ldq_kernel_env_A0
	.p2align 4,,15
.globl op_stq_kernel_env_A0
	.type	op_stq_kernel_env_A0, @function
op_stq_kernel_env_A0:
	subl	$20, %esp
	movl	%edi, %ecx
	movl	__op_param1+4(%ebp), %edx
	movl	__op_param1(%ebp), %eax
	movl	%edx, 16(%esp)
	movl	%edi, %edx
	shrl	$8, %edx
	movl	%eax, 12(%esp)
	movl	%edi, %eax
	andl	$4080, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L335
	xorl	%eax, %eax
	movl	16(%esp), %edx
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L338
	.p2align 4,,7
.L335:
	movl	896(%edx,%ebp), %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L338:
	addl	$20, %esp
	ret
	.size	op_stq_kernel_env_A0, .-op_stq_kernel_env_A0
	.p2align 4,,15
.globl op_ldo_kernel_env_A0
	.type	op_ldo_kernel_env_A0, @function
op_ldo_kernel_env_A0:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%edi, %edx
	movl	%eax, 8(%esp)
	shrl	$8, %edx
	movl	%edi, %eax
	movl	%edi, 4(%esp)
	andl	$4080, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L340
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L341
	.p2align 4,,7
.L340:
	movl	896(%edx,%ebp), %ecx
	addl	%ecx, 4(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L341:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	leal	8(%edi), %ecx
	movl	%ecx, %edx
	shrl	$8, %edx
	movl	%ecx, %eax
	andl	$4080, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L344
	movl	$0, (%esp)
	movl	%ecx, %eax
	call	__ldq_mmu
	jmp	.L345
	.p2align 4,,7
.L344:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU64
.L345:
	movl	8(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$12, %esp
	ret
	.size	op_ldo_kernel_env_A0, .-op_ldo_kernel_env_A0
	.p2align 4,,15
.globl op_sto_kernel_env_A0
	.type	op_sto_kernel_env_A0, @function
op_sto_kernel_env_A0:
	subl	$32, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, %edx
	movl	%eax, 28(%esp)
	movl	%edi, %ecx
	movl	4(%edx), %edx
	movl	(%eax), %eax
	movl	%edx, 24(%esp)
	movl	%edi, %edx
	shrl	$8, %edx
	movl	%eax, 20(%esp)
	movl	%edi, %eax
	andl	$4080, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L349
	xorl	%eax, %eax
	movl	24(%esp), %edx
	movl	%eax, 8(%esp)
	movl	20(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L352
	.p2align 4,,7
.L349:
	movl	896(%edx,%ebp), %eax
	movl	24(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L352:
	movl	28(%esp), %edx
	leal	8(%edi), %ecx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 12(%esp)
	movl	%ecx, %eax
	andl	$-4089, %eax
	movl	%edx, 16(%esp)
	movl	%ecx, %edx
	shrl	$8, %edx
	andl	$4080, %edx
	cmpl	%eax, 888(%edx,%ebp)
	je	.L353
	xorl	%edx, %edx
	movl	12(%esp), %eax
	movl	%edx, 8(%esp)
	movl	16(%esp), %edx
	movl	%eax, (%esp)
	movl	%ecx, %eax
	movl	%edx, 4(%esp)
	call	__stq_mmu
	jmp	.L356
	.p2align 4,,7
.L353:
	movl	896(%edx,%ebp), %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L356:
	addl	$32, %esp
	ret
	.size	op_sto_kernel_env_A0, .-op_sto_kernel_env_A0
	.p2align 4,,15
.globl op_ldub_user_T0_A0
	.type	op_ldub_user_T0_A0, @function
op_ldub_user_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldb_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movzbl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldub_user_T0_A0, .-op_ldub_user_T0_A0
	.p2align 4,,15
.globl op_ldsb_user_T0_A0
	.type	op_ldsb_user_T0_A0, @function
op_ldsb_user_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldb_mmu
popl %edx
movsbl %al, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movsbl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldsb_user_T0_A0, .-op_ldsb_user_T0_A0
	.p2align 4,,15
.globl op_lduw_user_T0_A0
	.type	op_lduw_user_T0_A0, @function
op_lduw_user_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldw_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movzwl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_lduw_user_T0_A0, .-op_lduw_user_T0_A0
	.p2align 4,,15
.globl op_ldsw_user_T0_A0
	.type	op_ldsw_user_T0_A0, @function
op_ldsw_user_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldw_mmu
popl %edx
movswl %ax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movswl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldsw_user_T0_A0, .-op_ldsw_user_T0_A0
	.p2align 4,,15
.globl op_ldl_user_T0_A0
	.type	op_ldl_user_T0_A0, @function
op_ldl_user_T0_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4093, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldl_mmu
popl %edx
movl %eax, %ebx
jmp 2f
1:
addl 12(%edx), %eax
movl (%eax), %ebx
2:

#NO_APP
	ret
	.size	op_ldl_user_T0_A0, .-op_ldl_user_T0_A0
	.p2align 4,,15
.globl op_ldub_user_T1_A0
	.type	op_ldub_user_T1_A0, @function
op_ldub_user_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldb_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movzbl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldub_user_T1_A0, .-op_ldub_user_T1_A0
	.p2align 4,,15
.globl op_ldsb_user_T1_A0
	.type	op_ldsb_user_T1_A0, @function
op_ldsb_user_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4096, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldb_mmu
popl %edx
movsbl %al, %esi
jmp 2f
1:
addl 12(%edx), %eax
movsbl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldsb_user_T1_A0, .-op_ldsb_user_T1_A0
	.p2align 4,,15
.globl op_lduw_user_T1_A0
	.type	op_lduw_user_T1_A0, @function
op_lduw_user_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldw_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movzwl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_lduw_user_T1_A0, .-op_lduw_user_T1_A0
	.p2align 4,,15
.globl op_ldsw_user_T1_A0
	.type	op_ldsw_user_T1_A0, @function
op_ldsw_user_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4095, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldw_mmu
popl %edx
movswl %ax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movswl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldsw_user_T1_A0, .-op_ldsw_user_T1_A0
	.p2align 4,,15
.globl op_ldl_user_T1_A0
	.type	op_ldl_user_T1_A0, @function
op_ldl_user_T1_A0:
#APP
	movl %edi, %edx
movl %edi, %eax
shrl $8, %edx
andl $-4093, %eax
andl $4080, %edx
leal 4980(%edx, %ebp), %edx
cmpl (%edx), %eax
movl %edi, %eax
je 1f
pushl $1
call __ldl_mmu
popl %edx
movl %eax, %esi
jmp 2f
1:
addl 12(%edx), %eax
movl (%eax), %esi
2:

#NO_APP
	ret
	.size	op_ldl_user_T1_A0, .-op_ldl_user_T1_A0
	.p2align 4,,15
.globl op_stb_user_T0_A0
	.type	op_stb_user_T0_A0, @function
op_stb_user_T0_A0:
	movl	%edi, %eax
	subl	$12, %esp
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L378
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L381
	.p2align 4,,7
.L378:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
.L381:
	addl	$12, %esp
	ret
	.size	op_stb_user_T0_A0, .-op_stb_user_T0_A0
	.p2align 4,,15
.globl op_stw_user_T0_A0
	.type	op_stw_user_T0_A0, @function
op_stw_user_T0_A0:
	movl	%edi, %eax
	subl	$12, %esp
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L383
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L386
	.p2align 4,,7
.L383:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L386:
	addl	$12, %esp
	ret
	.size	op_stw_user_T0_A0, .-op_stw_user_T0_A0
	.p2align 4,,15
.globl op_stl_user_T0_A0
	.type	op_stl_user_T0_A0, @function
op_stl_user_T0_A0:
	movl	%edi, %eax
	subl	$12, %esp
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	andl	$255, %eax
	movl	%ebx, %edx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L388
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L391
	.p2align 4,,7
.L388:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L391:
	addl	$12, %esp
	ret
	.size	op_stl_user_T0_A0, .-op_stl_user_T0_A0
	.p2align 4,,15
.globl op_stw_user_T1_A0
	.type	op_stw_user_T1_A0, @function
op_stw_user_T1_A0:
	movl	%edi, %eax
	subl	$12, %esp
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L393
	movl	$1, (%esp)
	movzwl	%si, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L396
	.p2align 4,,7
.L393:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%si, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L396:
	addl	$12, %esp
	ret
	.size	op_stw_user_T1_A0, .-op_stw_user_T1_A0
	.p2align 4,,15
.globl op_stl_user_T1_A0
	.type	op_stl_user_T1_A0, @function
op_stl_user_T1_A0:
	movl	%edi, %eax
	subl	$12, %esp
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	andl	$255, %eax
	movl	%esi, %edx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L398
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L401
	.p2align 4,,7
.L398:
	movl	%esi, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L401:
	addl	$12, %esp
	ret
	.size	op_stl_user_T1_A0, .-op_stl_user_T1_A0
	.p2align 4,,15
.globl op_ldq_user_env_A0
	.type	op_ldq_user_env_A0, @function
op_ldq_user_env_A0:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%edi, 4(%esp)
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L403
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L404
	.p2align 4,,7
.L403:
	movl	896(%edx,%ebp), %ecx
	addl	%ecx, 4(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L404:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	addl	$12, %esp
	ret
	.size	op_ldq_user_env_A0, .-op_ldq_user_env_A0
	.p2align 4,,15
.globl op_stq_user_env_A0
	.type	op_stq_user_env_A0, @function
op_stq_user_env_A0:
	subl	$20, %esp
	movl	%edi, %ecx
	movl	__op_param1(%ebp), %eax
	movl	__op_param1+4(%ebp), %edx
	movl	%eax, 12(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%edx, 16(%esp)
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L408
	movl	$1, %eax
	movl	16(%esp), %edx
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L411
	.p2align 4,,7
.L408:
	movl	896(%edx,%ebp), %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L411:
	addl	$20, %esp
	ret
	.size	op_stq_user_env_A0, .-op_stq_user_env_A0
	.p2align 4,,15
.globl op_ldo_user_env_A0
	.type	op_ldo_user_env_A0, @function
op_ldo_user_env_A0:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%edi, 4(%esp)
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L413
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L414
	.p2align 4,,7
.L413:
	movl	896(%edx,%ebp), %ecx
	addl	%ecx, 4(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L414:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	leal	8(%edi), %ecx
	movl	%ecx, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L417
	movl	$1, (%esp)
	movl	%ecx, %eax
	call	__ldq_mmu
	jmp	.L418
	.p2align 4,,7
.L417:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU64
.L418:
	movl	8(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$12, %esp
	ret
	.size	op_ldo_user_env_A0, .-op_ldo_user_env_A0
	.p2align 4,,15
.globl op_sto_user_env_A0
	.type	op_sto_user_env_A0, @function
op_sto_user_env_A0:
	subl	$32, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, %edx
	movl	%eax, 28(%esp)
	movl	%edi, %ecx
	movl	(%eax), %eax
	movl	4(%edx), %edx
	movl	%eax, 20(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%edx, 24(%esp)
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L422
	movl	$1, %eax
	movl	24(%esp), %edx
	movl	%eax, 8(%esp)
	movl	20(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L425
	.p2align 4,,7
.L422:
	movl	896(%edx,%ebp), %eax
	movl	24(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L425:
	movl	28(%esp), %edx
	leal	8(%edi), %ecx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 12(%esp)
	movl	%ecx, %eax
	shrl	$12, %eax
	movl	%edx, 16(%esp)
	andl	$255, %eax
	leal	256(%eax), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L426
	movl	12(%esp), %eax
	movl	$1, %edx
	movl	%edx, 8(%esp)
	movl	16(%esp), %edx
	movl	%eax, (%esp)
	movl	%ecx, %eax
	movl	%edx, 4(%esp)
	call	__stq_mmu
	jmp	.L429
	.p2align 4,,7
.L426:
	movl	896(%edx,%ebp), %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L429:
	addl	$32, %esp
	ret
	.size	op_sto_user_env_A0, .-op_sto_user_env_A0
	.p2align 4,,15
.globl op_jmp_T0
	.type	op_jmp_T0, @function
op_jmp_T0:
	movl	%ebx, 32(%ebp)
	ret
	.size	op_jmp_T0, .-op_jmp_T0
	.p2align 4,,15
.globl op_movl_eip_im
	.type	op_movl_eip_im, @function
op_movl_eip_im:
	movl	$__op_param1, 32(%ebp)
	ret
	.size	op_movl_eip_im, .-op_movl_eip_im
	.p2align 4,,15
.globl op_hlt
	.type	op_hlt, @function
op_hlt:
	call	helper_hlt
	ret
	.size	op_hlt, .-op_hlt
	.p2align 4,,15
.globl op_monitor
	.type	op_monitor, @function
op_monitor:
	call	helper_monitor
	ret
	.size	op_monitor, .-op_monitor
	.p2align 4,,15
.globl op_mwait
	.type	op_mwait, @function
op_mwait:
	call	helper_mwait
	ret
	.size	op_mwait, .-op_mwait
	.p2align 4,,15
.globl op_debug
	.type	op_debug, @function
op_debug:
	movl	$65538, %eax
	movl	%eax, 812(%ebp)
	call	cpu_loop_exit
	ret
	.size	op_debug, .-op_debug
	.p2align 4,,15
.globl op_raise_interrupt
	.type	op_raise_interrupt, @function
op_raise_interrupt:
	subl	$16, %esp
	movl	$__op_param2, %eax
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	$1, %eax
	movl	%eax, 4(%esp)
	movl	$__op_param1, (%esp)
	call	raise_interrupt
	addl	$16, %esp
	ret
	.size	op_raise_interrupt, .-op_raise_interrupt
	.p2align 4,,15
.globl op_raise_exception
	.type	op_raise_exception, @function
op_raise_exception:
	subl	$4, %esp
	movl	$__op_param1, (%esp)
	call	raise_exception
	popl	%edx
	ret
	.size	op_raise_exception, .-op_raise_exception
	.p2align 4,,15
.globl op_into
	.type	op_into, @function
op_into:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	testb	$8, %ah
	je	.L439
	movl	$4, (%esp)
	movl	$__op_param1, %eax
	movl	$1, %ecx
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	%ecx, 4(%esp)
	call	raise_interrupt
	.p2align 4,,15
.L439:
	addl	$16, %esp
	ret
	.size	op_into, .-op_into
	.p2align 4,,15
.globl op_cli
	.type	op_cli, @function
op_cli:
	andl	$-513, 36(%ebp)
	ret
	.size	op_cli, .-op_cli
	.p2align 4,,15
.globl op_sti
	.type	op_sti, @function
op_sti:
	orl	$512, 36(%ebp)
	ret
	.size	op_sti, .-op_sti
	.p2align 4,,15
.globl op_set_inhibit_irq
	.type	op_set_inhibit_irq, @function
op_set_inhibit_irq:
	orl	$8, 56(%ebp)
	ret
	.size	op_set_inhibit_irq, .-op_set_inhibit_irq
	.p2align 4,,15
.globl op_reset_inhibit_irq
	.type	op_reset_inhibit_irq, @function
op_reset_inhibit_irq:
	andl	$-9, 56(%ebp)
	ret
	.size	op_reset_inhibit_irq, .-op_reset_inhibit_irq
	.p2align 4,,15
.globl op_rsm
	.type	op_rsm, @function
op_rsm:
	call	helper_rsm
	ret
	.size	op_rsm, .-op_rsm
	.p2align 4,,15
.globl op_boundw
	.type	op_boundw, @function
op_boundw:
	subl	$20, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 12(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L446
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldw_mmu
	jmp	.L455
	.p2align 4,,7
.L446:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 12(%esp)
	movl	12(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadS16
.L455:
	cwtl
	leal	2(%edi), %ecx
	movl	%ecx, %edx
	movl	%eax, 16(%esp)
	movl	56(%ebp), %eax
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L450
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%ecx, %eax
	call	__ldw_mmu
	jmp	.L456
	.p2align 4,,7
.L450:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadS16
.L456:
	movswl	%ax,%ecx
	movswl	%bx,%eax
	cmpl	16(%esp), %eax
	setl	%dl
	cmpl	%ecx, %eax
	setg	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L454
	movl	$5, (%esp)
	call	raise_exception
	.p2align 4,,15
.L454:
	addl	$20, %esp
	ret
	.size	op_boundw, .-op_boundw
	.p2align 4,,15
.globl op_boundl
	.type	op_boundl, @function
op_boundl:
	subl	$20, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 12(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L458
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldl_mmu
	jmp	.L459
	.p2align 4,,7
.L458:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 12(%esp)
	movl	12(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU32
.L459:
	movl	%eax, 16(%esp)
	movl	56(%ebp), %eax
	leal	4(%edi), %ecx
	movl	%ecx, %edx
	shrl	$12, %edx
	andl	$3, %eax
	andl	$255, %edx
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L462
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%ecx, %eax
	call	__ldl_mmu
	jmp	.L467
	.p2align 4,,7
.L462:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU32
.L467:
	cmpl	16(%esp), %ebx
	movl	%eax, %ecx
	setl	%dl
	cmpl	%ecx, %ebx
	setg	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L466
	movl	$5, (%esp)
	call	raise_exception
	.p2align 4,,15
.L466:
	addl	$20, %esp
	ret
	.size	op_boundl, .-op_boundl
	.p2align 4,,15
.globl op_cmpxchg8b
	.type	op_cmpxchg8b, @function
op_cmpxchg8b:
	call	helper_cmpxchg8b
	ret
	.size	op_cmpxchg8b, .-op_cmpxchg8b
	.p2align 4,,15
.globl op_movl_T0_0
	.type	op_movl_T0_0, @function
op_movl_T0_0:
	xorl	%ebx, %ebx
	ret
	.size	op_movl_T0_0, .-op_movl_T0_0
	.p2align 4,,15
.globl op_check_external_event
	.type	op_check_external_event, @function
op_check_external_event:
	testl	$53248, 864(%ebp)
	jne	.L472
	testb	$32, 865(%ebp)
	je	.L470
	testb	$2, 37(%ebp)
	je	.L470
	testb	$8, 56(%ebp)
	jne	.L470
	.p2align 4,,15
.L472:
	call	helper_external_event
.L470:
	ret
	.size	op_check_external_event, .-op_check_external_event
	.p2align 4,,15
.globl op_exit_tb
	.type	op_exit_tb, @function
op_exit_tb:
#APP
	ret
#NO_APP
	ret
	.size	op_exit_tb, .-op_exit_tb
	.p2align 4,,15
	.type	compute_all_addb, @function
compute_all_addb:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	subl	%eax, %ecx
	cmpb	%al, 44(%ebp)
	movzbl	parity_table(%edx), %edx
	setb	%al
	movzbl	%al, %eax
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movzbl	44(%ebp), %edx
	cmpb	$1, %dl
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	$128, %edx
	movl	%edx, 12(%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	notl	%ecx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sall	$4, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_addb, .-compute_all_addb
	.p2align 4,,15
	.type	compute_c_addb, @function
compute_c_addb:
	movl	44(%ebp), %eax
	cmpb	%al, 40(%ebp)
	seta	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_addb, .-compute_c_addb
	.p2align 4,,15
	.type	compute_all_adcb, @function
compute_all_adcb:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	movzbl	44(%ebp), %ecx
	movl	%eax, 8(%esp)
	subl	%eax, %edx
	decl	%edx
	movzbl	parity_table(%ecx), %ecx
	cmpb	%al, 44(%ebp)
	movl	%ecx, 16(%esp)
	movl	8(%esp), %ecx
	setbe	%al
	xorl	44(%ebp), %ecx
	movzbl	%al, %eax
	xorl	%edx, %ecx
	andl	$16, %ecx
	movl	%ecx, 4(%esp)
	movzbl	44(%ebp), %ecx
	cmpb	$1, %cl
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	movl	%ecx, (%esp)
	movl	44(%ebp), %ecx
	andl	$128, %ecx
	movl	%ecx, 12(%esp)
	movl	8(%esp), %ecx
	xorl	%ecx, %edx
	movl	44(%ebp), %ecx
	notl	%edx
	xorl	%ecx, 8(%esp)
	movl	8(%esp), %ecx
	andl	%ecx, %edx
	movl	16(%esp), %ecx
	sall	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	orl	%ecx, %eax
	movl	12(%esp), %ecx
	addl	$20, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_adcb, .-compute_all_adcb
	.p2align 4,,15
	.type	compute_c_adcb, @function
compute_c_adcb:
	movl	44(%ebp), %eax
	cmpb	%al, 40(%ebp)
	setae	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_adcb, .-compute_c_adcb
	.p2align 4,,15
	.type	compute_all_subb, @function
compute_all_subb:
	subl	$20, %esp
	movl	44(%ebp), %edx
	movl	40(%ebp), %eax
	movl	40(%ebp), %ecx
	addl	%edx, %eax
	cmpb	%cl, %al
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	setb	%al
	movzbl	%al, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movzbl	44(%ebp), %edx
	cmpb	$1, %dl
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	$128, %edx
	movl	%edx, 12(%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sall	$4, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_subb, .-compute_all_subb
	.p2align 4,,15
	.type	compute_c_subb, @function
compute_c_subb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpb	%al, 40(%ebp)
	seta	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_subb, .-compute_c_subb
	.p2align 4,,15
	.type	compute_all_sbbb, @function
compute_all_sbbb:
	subl	$20, %esp
	movl	44(%ebp), %eax
	movl	40(%ebp), %ecx
	movzbl	44(%ebp), %edx
	addl	%eax, %ecx
	movl	40(%ebp), %eax
	movzbl	parity_table(%edx), %edx
	incl	%ecx
	cmpb	%al, %cl
	movl	%eax, 8(%esp)
	setbe	%al
	movzbl	%al, %eax
	movl	%edx, 16(%esp)
	movl	44(%ebp), %edx
	xorl	%ecx, %edx
	movl	%edx, 4(%esp)
	movl	8(%esp), %edx
	xorl	%edx, 4(%esp)
	movzbl	44(%ebp), %edx
	andl	$16, 4(%esp)
	cmpb	$1, %dl
	sbbl	%edx, %edx
	xorl	%ecx, 8(%esp)
	andl	$64, %edx
	xorl	44(%ebp), %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	%ecx, 8(%esp)
	movl	16(%esp), %ecx
	andl	$128, %edx
	sall	$4, 8(%esp)
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	andl	$2048, 8(%esp)
	orl	%ecx, %eax
	movl	(%esp), %ecx
	orl	%ecx, %eax
	movl	8(%esp), %ecx
	orl	%edx, %eax
	addl	$20, %esp
	orl	%ecx, %eax
	ret
	.size	compute_all_sbbb, .-compute_all_sbbb
	.p2align 4,,15
	.type	compute_c_sbbb, @function
compute_c_sbbb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	incl	%eax
	cmpb	%al, 40(%ebp)
	setae	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_sbbb, .-compute_c_sbbb
	.p2align 4,,15
	.type	compute_all_logicb, @function
compute_all_logicb:
	movzbl	44(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	movzbl	parity_table(%eax), %eax
	cmpb	$1, %dl
	sbbl	%edx, %edx
	andl	$64, %edx
	andl	$128, %ecx
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_logicb, .-compute_all_logicb
	.p2align 4,,15
	.type	compute_c_logicb, @function
compute_c_logicb:
	xorl	%eax, %eax
	ret
	.size	compute_c_logicb, .-compute_c_logicb
	.p2align 4,,15
	.type	compute_all_incb, @function
compute_all_incb:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	decl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzbl	44(%ebp), %edx
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpb	$1, %dl
	movl	44(%ebp), %edx
	movzbl	parity_table(%eax), %eax
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	andl	$128, %edx
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	cmpb	$-128, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_incb, .-compute_all_incb
	.p2align 4,,15
	.type	compute_all_decb, @function
compute_all_decb:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	incl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzbl	44(%ebp), %edx
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpb	$1, %dl
	movl	44(%ebp), %edx
	movzbl	parity_table(%eax), %eax
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	andl	$128, %edx
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	cmpb	$127, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_decb, .-compute_all_decb
	.p2align 4,,15
	.type	compute_all_shlb, @function
compute_all_shlb:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movzbl	44(%ebp), %edx
	shrl	$7, %eax
	andl	$1, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movzbl	44(%ebp), %edx
	cmpb	$1, %dl
	movl	44(%ebp), %edx
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	andl	$128, %edx
	orl	4(%esp), %eax
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	xorl	40(%ebp), %edx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	addl	$8, %esp
	sall	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_shlb, .-compute_all_shlb
	.p2align 4,,15
	.type	compute_c_shlb, @function
compute_c_shlb:
	movl	40(%ebp), %eax
	shrl	$7, %eax
	andl	$1, %eax
	ret
	.size	compute_c_shlb, .-compute_c_shlb
	.p2align 4,,15
	.type	compute_all_sarb, @function
compute_all_sarb:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movzbl	44(%ebp), %edx
	andl	$1, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movzbl	44(%ebp), %edx
	cmpb	$1, %dl
	movl	44(%ebp), %edx
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	andl	$128, %edx
	orl	4(%esp), %eax
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	xorl	40(%ebp), %edx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	addl	$8, %esp
	sall	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_sarb, .-compute_all_sarb
	.p2align 4,,15
	.type	compute_all_mulb, @function
compute_all_mulb:
	subl	$12, %esp
	xorl	%eax, %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	cmpl	$0, 40(%ebp)
	movzbl	parity_table(%edx), %edx
	setne	%al
	movl	%edx, 8(%esp)
	movzbl	44(%ebp), %edx
	cmpb	$1, %dl
	sbbl	%edx, %edx
	andl	$128, %ecx
	andl	$64, %edx
	movl	%ecx, (%esp)
	movl	%eax, %ecx
	sall	$11, %ecx
	orl	8(%esp), %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	addl	$12, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_mulb, .-compute_all_mulb
	.p2align 4,,15
.globl op_jb_subb
	.type	op_jb_subb, @function
op_jb_subb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpb	%al, 40(%ebp)
	jbe	.L563
#APP
	jmp __op_gen_label1
#NO_APP
.L563:
	ret
	.size	op_jb_subb, .-op_jb_subb
	.p2align 4,,15
.globl op_jz_subb
	.type	op_jz_subb, @function
op_jz_subb:
	cmpb	$0, 44(%ebp)
	jne	.L565
#APP
	jmp __op_gen_label1
#NO_APP
.L565:
	ret
	.size	op_jz_subb, .-op_jz_subb
	.p2align 4,,15
.globl op_jnz_subb
	.type	op_jnz_subb, @function
op_jnz_subb:
	cmpb	$0, 44(%ebp)
	je	.L567
#APP
	jmp __op_gen_label1
#NO_APP
.L567:
	ret
	.size	op_jnz_subb, .-op_jnz_subb
	.p2align 4,,15
.globl op_jbe_subb
	.type	op_jbe_subb, @function
op_jbe_subb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpb	%al, 40(%ebp)
	jb	.L569
#APP
	jmp __op_gen_label1
#NO_APP
.L569:
	ret
	.size	op_jbe_subb, .-op_jbe_subb
	.p2align 4,,15
.globl op_js_subb
	.type	op_js_subb, @function
op_js_subb:
	cmpb	$0, 44(%ebp)
	jns	.L571
#APP
	jmp __op_gen_label1
#NO_APP
.L571:
	ret
	.size	op_js_subb, .-op_js_subb
	.p2align 4,,15
.globl op_jl_subb
	.type	op_jl_subb, @function
op_jl_subb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpb	%al, 40(%ebp)
	jle	.L573
#APP
	jmp __op_gen_label1
#NO_APP
.L573:
	ret
	.size	op_jl_subb, .-op_jl_subb
	.p2align 4,,15
.globl op_jle_subb
	.type	op_jle_subb, @function
op_jle_subb:
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpb	%al, 40(%ebp)
	jl	.L575
#APP
	jmp __op_gen_label1
#NO_APP
.L575:
	ret
	.size	op_jle_subb, .-op_jle_subb
	.p2align 4,,15
.globl op_setb_T0_subb
	.type	op_setb_T0_subb, @function
op_setb_T0_subb:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpb	%al, 40(%ebp)
	seta	%bl
	ret
	.size	op_setb_T0_subb, .-op_setb_T0_subb
	.p2align 4,,15
.globl op_setz_T0_subb
	.type	op_setz_T0_subb, @function
op_setz_T0_subb:
	xorl	%ebx, %ebx
	cmpb	$0, 44(%ebp)
	sete	%bl
	ret
	.size	op_setz_T0_subb, .-op_setz_T0_subb
	.p2align 4,,15
.globl op_setbe_T0_subb
	.type	op_setbe_T0_subb, @function
op_setbe_T0_subb:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpb	%al, 40(%ebp)
	setae	%bl
	ret
	.size	op_setbe_T0_subb, .-op_setbe_T0_subb
	.p2align 4,,15
.globl op_sets_T0_subb
	.type	op_sets_T0_subb, @function
op_sets_T0_subb:
	movl	44(%ebp), %eax
	sarl	$7, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_sets_T0_subb, .-op_sets_T0_subb
	.p2align 4,,15
.globl op_setl_T0_subb
	.type	op_setl_T0_subb, @function
op_setl_T0_subb:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpb	%al, 40(%ebp)
	setg	%bl
	ret
	.size	op_setl_T0_subb, .-op_setl_T0_subb
	.p2align 4,,15
.globl op_setle_T0_subb
	.type	op_setle_T0_subb, @function
op_setle_T0_subb:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpb	%al, 40(%ebp)
	setge	%bl
	ret
	.size	op_setle_T0_subb, .-op_setle_T0_subb
	.p2align 4,,15
.globl op_shlb_T0_T1
	.type	op_shlb_T0_T1, @function
op_shlb_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
	.size	op_shlb_T0_T1, .-op_shlb_T0_T1
	.p2align 4,,15
.globl op_shrb_T0_T1
	.type	op_shrb_T0_T1, @function
op_shrb_T0_T1:
	movl	%esi, %ecx
	andl	$255, %ebx
	andl	$31, %ecx
	shrl	%cl, %ebx
	ret
	.size	op_shrb_T0_T1, .-op_shrb_T0_T1
	.p2align 4,,15
.globl op_sarb_T0_T1
	.type	op_sarb_T0_T1, @function
op_sarb_T0_T1:
	movl	%esi, %ecx
	movsbl	%bl,%eax
	andl	$31, %ecx
	movl	%eax, %ebx
	sarl	%cl, %ebx
	ret
	.size	op_sarb_T0_T1, .-op_sarb_T0_T1
	.p2align 4,,15
.globl op_rolb_T0_T1_cc
	.type	op_rolb_T0_T1_cc, @function
op_rolb_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L589
	movl	%ebx, (%esp)
	movl	%esi, %eax
	andl	$7, %eax
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sall	$4, (%esp)
	andl	$2048, (%esp)
	orl	%eax, (%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L589:
	popl	%eax
	ret
	.size	op_rolb_T0_T1_cc, .-op_rolb_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_T0_T1_cc
	.type	op_rorb_T0_T1_cc, @function
op_rorb_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L594
	movl	%ebx, (%esp)
	movl	%esi, %eax
	andl	$7, %eax
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sall	$4, (%esp)
	andl	$2048, (%esp)
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%ebx, %edx
	shrl	$7, %edx
	andl	$1, %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L594:
	popl	%ecx
	ret
	.size	op_rorb_T0_T1_cc, .-op_rorb_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_T0_T1
	.type	op_rolb_T0_T1, @function
op_rolb_T0_T1:
	movl	%esi, %ecx
	subl	$4, %esp
	andl	$7, %ecx
	je	.L599
	movl	$8, (%esp)
	andl	$255, %ebx
	movl	%ebx, %eax
	subl	%ecx, (%esp)
	sall	%cl, %eax
	movl	%ebx, %edx
	movl	%eax, %ebx
	movzbl	(%esp), %ecx
	shrl	%cl, %edx
	orl	%edx, %ebx
.L599:
	popl	%ecx
	ret
	.size	op_rolb_T0_T1, .-op_rolb_T0_T1
	.p2align 4,,15
.globl op_rorb_T0_T1
	.type	op_rorb_T0_T1, @function
op_rorb_T0_T1:
	movl	%esi, %ecx
	subl	$4, %esp
	andl	$7, %ecx
	je	.L601
	movl	$8, (%esp)
	andl	$255, %ebx
	movl	%ebx, %eax
	subl	%ecx, (%esp)
	shrl	%cl, %eax
	movl	%ebx, %edx
	movl	%eax, %ebx
	movzbl	(%esp), %ecx
	sall	%cl, %edx
	orl	%edx, %ebx
.L601:
	popl	%eax
	ret
	.size	op_rorb_T0_T1, .-op_rorb_T0_T1
	.p2align 4,,15
.globl op_rclb_T0_T1_cc
	.type	op_rclb_T0_T1_cc, @function
op_rclb_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 8(%esp)
	je	.L603
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%eax, (%esp)
	movl	8(%esp), %ecx
	movl	4(%esp), %eax
	decl	%ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L604
	movl	8(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, (%esp)
.L604:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	$8, %ecx
	xorl	%ebx, %eax
	sall	$4, %eax
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	movl	8(%esp), %eax
	subl	%eax, %ecx
	movl	4(%esp), %eax
	shrl	%cl, %edx
	andl	$1, %edx
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L603:
	addl	$12, %esp
	ret
	.size	op_rclb_T0_T1_cc, .-op_rclb_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_T0_T1_cc
	.type	op_rcrb_T0_T1_cc, @function
op_rcrb_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 8(%esp)
	je	.L609
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%eax, (%esp)
	movl	$8, %ecx
	movl	4(%esp), %eax
	subl	8(%esp), %ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L610
	movl	8(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
.L610:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	8(%esp), %ecx
	xorl	%ebx, %eax
	sall	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	shrl	%cl, %edx
	andl	$1, %edx
	movl	4(%esp), %eax
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L609:
	addl	$12, %esp
	ret
	.size	op_rcrb_T0_T1_cc, .-op_rcrb_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_T0_T1_cc
	.type	op_shlb_T0_T1_cc, @function
op_shlb_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L615
	movl	$34, 48(%ebp)
	movzbl	%bl, %eax
	leal	-1(%edx), %ecx
	sall	%cl, %eax
	movb	%dl, %cl
	sall	%cl, %ebx
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L615:
	ret
	.size	op_shlb_T0_T1_cc, .-op_shlb_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_T0_T1_cc
	.type	op_shrb_T0_T1_cc, @function
op_shrb_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L617
	movl	$38, 48(%ebp)
	andl	$255, %ebx
	leal	-1(%edx), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movb	%dl, %cl
	movl	%eax, 40(%ebp)
	shrl	%cl, %ebx
	movl	%ebx, 44(%ebp)
.L617:
	ret
	.size	op_shrb_T0_T1_cc, .-op_shrb_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_T0_T1_cc
	.type	op_sarb_T0_T1_cc, @function
op_sarb_T0_T1_cc:
	movl	%esi, %ecx
	andl	$31, %ecx
	je	.L619
	movl	$38, 48(%ebp)
	movsbl	%bl,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L619:
	ret
	.size	op_sarb_T0_T1_cc, .-op_sarb_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_T0_T1_cc
	.type	op_adcb_T0_T1_cc, @function
op_adcb_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	leal	(%ebx,%esi), %edx
	leal	(%edx,%eax), %ebx
	movl	%ebx, 44(%ebp)
	leal	6(,%eax,4), %eax
	movl	%eax, 48(%ebp)
	ret
	.size	op_adcb_T0_T1_cc, .-op_adcb_T0_T1_cc
	.p2align 4,,15
.globl op_sbbb_T0_T1_cc
	.type	op_sbbb_T0_T1_cc, @function
op_sbbb_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	movl	%ebx, %edx
	subl	%esi, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	14(,%eax,4), %eax
	movl	%ebx, 44(%ebp)
	movl	%eax, 48(%ebp)
	ret
	.size	op_sbbb_T0_T1_cc, .-op_sbbb_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgb_T0_T1_EAX_cc
	.type	op_cmpxchgb_T0_T1_EAX_cc, @function
op_cmpxchgb_T0_T1_EAX_cc:
	movl	(%ebp), %edx
	movl	%ebx, %ecx
	subl	%ebx, %edx
	testb	%dl, %dl
	jne	.L623
	movl	%esi, %ebx
	jmp	.L624
	.p2align 4,,7
.L623:
	movb	%bl, (%ebp)
.L624:
	movl	%ecx, 40(%ebp)
	movl	%edx, 44(%ebp)
	ret
	.size	op_cmpxchgb_T0_T1_EAX_cc, .-op_cmpxchgb_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolb_raw_T0_T1_cc
	.type	op_rolb_raw_T0_T1_cc, @function
op_rolb_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L626
	movl	%ebx, 8(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, (%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sall	$4, 8(%esp)
	andl	$2048, 8(%esp)
	orl	%eax, 8(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	8(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L626:
	addl	$12, %esp
	ret
	.size	op_rolb_raw_T0_T1_cc, .-op_rolb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_raw_T0_T1_cc
	.type	op_rorb_raw_T0_T1_cc, @function
op_rorb_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L632
	movl	%ebx, 8(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, (%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$7, %edx
	andl	$1, %edx
	sall	$4, 8(%esp)
	andl	$2048, 8(%esp)
	movl	8(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L632:
	addl	$12, %esp
	ret
	.size	op_rorb_raw_T0_T1_cc, .-op_rorb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_raw_T0_T1
	.type	op_rolb_raw_T0_T1, @function
op_rolb_raw_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$7, %ecx
	je	.L638
	movl	%edi, (%esp)
	movl	$8, %eax
	andl	$255, %ebx
	movl	%eax, 8(%esp)
	movl	%ebx, %edx
	sall	%cl, %edx
	subl	%ecx, 8(%esp)
	movl	%ebx, %eax
	movl	%edx, %ebx
	movzbl	8(%esp), %ecx
	shrl	%cl, %eax
	orl	%eax, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	.p2align 4,,15
.L638:
	addl	$12, %esp
	ret
	.size	op_rolb_raw_T0_T1, .-op_rolb_raw_T0_T1
	.p2align 4,,15
.globl op_rorb_raw_T0_T1
	.type	op_rorb_raw_T0_T1, @function
op_rorb_raw_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$7, %ecx
	je	.L641
	movl	%edi, (%esp)
	movl	$8, %eax
	andl	$255, %ebx
	movl	%eax, 8(%esp)
	movl	%ebx, %edx
	shrl	%cl, %edx
	subl	%ecx, 8(%esp)
	movl	%ebx, %eax
	movl	%edx, %ebx
	movzbl	8(%esp), %ecx
	sall	%cl, %eax
	orl	%eax, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	.p2align 4,,15
.L641:
	addl	$12, %esp
	ret
	.size	op_rorb_raw_T0_T1, .-op_rorb_raw_T0_T1
	.p2align 4,,15
.globl op_rclb_raw_T0_T1_cc
	.type	op_rclb_raw_T0_T1_cc, @function
op_rclb_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 16(%esp)
	je	.L644
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 12(%esp)
	sall	%cl, %edx
	movl	16(%esp), %ecx
	movl	%ebx, 8(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L645
	movl	16(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L645:
	movl	%edi, (%esp)
	movzbl	%dl, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	andl	$-2050, 12(%esp)
	movl	$8, %ecx
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sall	$4, %eax
	andl	$2048, %eax
	orl	%eax, 12(%esp)
	movl	16(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 8(%esp)
	movl	12(%esp), %eax
	andl	$1, 8(%esp)
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L644:
	addl	$20, %esp
	ret
	.size	op_rclb_raw_T0_T1_cc, .-op_rclb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_raw_T0_T1_cc
	.type	op_rcrb_raw_T0_T1_cc, @function
op_rcrb_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 16(%esp)
	je	.L651
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	16(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 8(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L652
	movl	16(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L652:
	movl	%edi, (%esp)
	movzbl	%dl, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	andl	$-2050, 12(%esp)
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	movl	16(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 8(%esp)
	orl	%eax, 12(%esp)
	andl	$1, 8(%esp)
	movl	12(%esp), %eax
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L651:
	addl	$20, %esp
	ret
	.size	op_rcrb_raw_T0_T1_cc, .-op_rcrb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_raw_T0_T1_cc
	.type	op_shlb_raw_T0_T1_cc, @function
op_shlb_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	je	.L658
	movl	%edi, (%esp)
	leal	-1(%eax), %ecx
	movzbl	%bl, %edx
	sall	%cl, %edx
	movb	%al, %cl
	sall	%cl, %ebx
	movl	%edx, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$34, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L658:
	addl	$12, %esp
	ret
	.size	op_shlb_raw_T0_T1_cc, .-op_shlb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_raw_T0_T1_cc
	.type	op_shrb_raw_T0_T1_cc, @function
op_shrb_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	je	.L661
	movl	%edi, (%esp)
	andl	$255, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 8(%esp)
	shrl	%cl, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$38, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L661:
	addl	$12, %esp
	ret
	.size	op_shrb_raw_T0_T1_cc, .-op_shrb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_raw_T0_T1_cc
	.type	op_sarb_raw_T0_T1_cc, @function
op_sarb_raw_T0_T1_cc:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L664
	movl	%edi, (%esp)
	movsbl	%bl,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$38, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L664:
	addl	$12, %esp
	ret
	.size	op_sarb_raw_T0_T1_cc, .-op_sarb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_raw_T0_T1_cc
	.type	op_adcb_raw_T0_T1_cc, @function
op_adcb_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	8(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, (%esp)
	leal	(%eax,%edx), %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	6(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_adcb_raw_T0_T1_cc, .-op_adcb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sbbb_raw_T0_T1_cc
	.type	op_sbbb_raw_T0_T1_cc, @function
op_sbbb_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, (%esp)
	movl	%eax, %ebx
	movl	8(%esp), %eax
	subl	%eax, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	14(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_sbbb_raw_T0_T1_cc, .-op_sbbb_raw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgb_raw_T0_T1_EAX_cc
	.type	op_cmpxchgb_raw_T0_T1_EAX_cc, @function
op_cmpxchgb_raw_T0_T1_EAX_cc:
	subl	$16, %esp
	movl	(%ebp), %eax
	movl	%ebx, 12(%esp)
	subl	%ebx, %eax
	movl	%eax, 8(%esp)
	movzbl	8(%esp), %eax
	testb	%al, %al
	jne	.L671
	movl	%edi, (%esp)
	movl	%esi, %ebx
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	jmp	.L673
	.p2align 4,,7
.L671:
	movb	%bl, (%ebp)
.L673:
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$16, %esp
	ret
	.size	op_cmpxchgb_raw_T0_T1_EAX_cc, .-op_cmpxchgb_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolb_kernel_T0_T1_cc
	.type	op_rolb_kernel_T0_T1_cc, @function
op_rolb_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L675
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L676
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L679
	.p2align 4,,7
.L676:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L679:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sall	$4, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L675:
	addl	$16, %esp
	ret
	.size	op_rolb_kernel_T0_T1_cc, .-op_rolb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_kernel_T0_T1_cc
	.type	op_rorb_kernel_T0_T1_cc, @function
op_rorb_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L684
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L685
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L688
	.p2align 4,,7
.L685:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L688:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$7, %edx
	andl	$1, %edx
	sall	$4, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L684:
	addl	$16, %esp
	ret
	.size	op_rorb_kernel_T0_T1_cc, .-op_rorb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_kernel_T0_T1
	.type	op_rolb_kernel_T0_T1, @function
op_rolb_kernel_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$7, %eax
	je	.L693
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L694
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L693
	.p2align 4,,7
.L694:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L693:
	addl	$12, %esp
	ret
	.size	op_rolb_kernel_T0_T1, .-op_rolb_kernel_T0_T1
	.p2align 4,,15
.globl op_rorb_kernel_T0_T1
	.type	op_rorb_kernel_T0_T1, @function
op_rorb_kernel_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$7, %eax
	je	.L699
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L700
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L699
	.p2align 4,,7
.L700:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L699:
	addl	$12, %esp
	ret
	.size	op_rorb_kernel_T0_T1, .-op_rorb_kernel_T0_T1
	.p2align 4,,15
.globl op_rclb_kernel_T0_T1_cc
	.type	op_rclb_kernel_T0_T1_cc, @function
op_rclb_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L705
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	sall	%cl, %edx
	movl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L706
	movl	20(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L706:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4096, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L707
	movl	$0, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L710
	.p2align 4,,7
.L707:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L710:
	andl	$-2050, 16(%esp)
	movl	$8, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sall	$4, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L705:
	addl	$24, %esp
	ret
	.size	op_rclb_kernel_T0_T1_cc, .-op_rclb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_kernel_T0_T1_cc
	.type	op_rcrb_kernel_T0_T1_cc, @function
op_rcrb_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L715
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L716
	movl	20(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L716:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4096, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L717
	movl	$0, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L720
	.p2align 4,,7
.L717:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L720:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L715:
	addl	$24, %esp
	ret
	.size	op_rcrb_kernel_T0_T1_cc, .-op_rcrb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_kernel_T0_T1_cc
	.type	op_shlb_kernel_T0_T1_cc, @function
op_shlb_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L725
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movzbl	%bl, %edx
	sall	%cl, %edx
	movb	%al, %cl
	sall	%cl, %ebx
	movl	%edx, 12(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4096, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L726
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L729
	.p2align 4,,7
.L726:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L729:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$34, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L725:
	addl	$16, %esp
	ret
	.size	op_shlb_kernel_T0_T1_cc, .-op_shlb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_kernel_T0_T1_cc
	.type	op_shrb_kernel_T0_T1_cc, @function
op_shrb_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L731
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 12(%esp)
	shrl	%cl, %ebx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L732
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L735
	.p2align 4,,7
.L732:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L735:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$38, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L731:
	addl	$16, %esp
	ret
	.size	op_shrb_kernel_T0_T1_cc, .-op_shrb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_kernel_T0_T1_cc
	.type	op_sarb_kernel_T0_T1_cc, @function
op_sarb_kernel_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L737
	movl	%edi, 8(%esp)
	movsbl	%bl,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L738
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L741
	.p2align 4,,7
.L738:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
.L741:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$38, 48(%ebp)
	movl	%eax, 40(%ebp)
.L737:
	addl	$16, %esp
	ret
	.size	op_sarb_kernel_T0_T1_cc, .-op_sarb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_kernel_T0_T1_cc
	.type	op_adcb_kernel_T0_T1_cc, @function
op_adcb_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%edi, %ecx
	movl	12(%esp), %edx
	movl	%edi, 8(%esp)
	leal	(%ebx,%esi), %eax
	shrl	$8, %ecx
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L743
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L746
	.p2align 4,,7
.L743:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L746:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	6(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcb_kernel_T0_T1_cc, .-op_adcb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sbbb_kernel_T0_T1_cc
	.type	op_sbbb_kernel_T0_T1_cc, @function
op_sbbb_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	movl	%edi, %ecx
	shrl	$8, %ecx
	subl	%eax, %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L748
	movl	$0, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L751
	.p2align 4,,7
.L748:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L751:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	14(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbb_kernel_T0_T1_cc, .-op_sbbb_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgb_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgb_kernel_T0_T1_EAX_cc, @function
op_cmpxchgb_kernel_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	movl	%eax, 12(%esp)
	movzbl	12(%esp), %eax
	testb	%al, %al
	jne	.L753
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4096, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%esi, %ebx
	movl	%esi, %edx
	je	.L754
	movl	$0, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L758
	.p2align 4,,7
.L754:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
	jmp	.L758
	.p2align 4,,7
.L753:
	movb	%bl, (%ebp)
.L758:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgb_kernel_T0_T1_EAX_cc, .-op_cmpxchgb_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolb_user_T0_T1_cc
	.type	op_rolb_user_T0_T1_cc, @function
op_rolb_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L760
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L761
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L764
	.p2align 4,,7
.L761:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L764:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sall	$4, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L760:
	addl	$16, %esp
	ret
	.size	op_rolb_user_T0_T1_cc, .-op_rolb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rorb_user_T0_T1_cc
	.type	op_rorb_user_T0_T1_cc, @function
op_rorb_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L769
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$7, %eax
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L770
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L773
	.p2align 4,,7
.L770:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L773:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$7, %edx
	andl	$1, %edx
	sall	$4, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L769:
	addl	$16, %esp
	ret
	.size	op_rorb_user_T0_T1_cc, .-op_rorb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rolb_user_T0_T1
	.type	op_rolb_user_T0_T1, @function
op_rolb_user_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$7, %eax
	je	.L778
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L779
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L778
	.p2align 4,,7
.L779:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L778:
	addl	$12, %esp
	ret
	.size	op_rolb_user_T0_T1, .-op_rolb_user_T0_T1
	.p2align 4,,15
.globl op_rorb_user_T0_T1
	.type	op_rorb_user_T0_T1, @function
op_rorb_user_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$7, %eax
	je	.L784
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L785
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L784
	.p2align 4,,7
.L785:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L784:
	addl	$12, %esp
	ret
	.size	op_rorb_user_T0_T1, .-op_rorb_user_T0_T1
	.p2align 4,,15
.globl op_rclb_user_T0_T1_cc
	.type	op_rclb_user_T0_T1_cc, @function
op_rclb_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L790
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	sall	%cl, %edx
	movl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L791
	movl	20(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L791:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L792
	movl	$1, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L795
	.p2align 4,,7
.L792:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L795:
	andl	$-2050, 16(%esp)
	movl	$8, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sall	$4, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L790:
	addl	$24, %esp
	ret
	.size	op_rclb_user_T0_T1_cc, .-op_rclb_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrb_user_T0_T1_cc
	.type	op_rcrb_user_T0_T1_cc, @function
op_rcrb_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclb_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L800
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$255, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	shrl	%cl, %edx
	movl	$8, %ecx
	subl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L801
	movl	20(%esp), %eax
	movl	$9, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L801:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L802
	movl	$1, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L805
	.p2align 4,,7
.L802:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU8
.L805:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sall	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L800:
	addl	$24, %esp
	ret
	.size	op_rcrb_user_T0_T1_cc, .-op_rcrb_user_T0_T1_cc
	.p2align 4,,15
.globl op_shlb_user_T0_T1_cc
	.type	op_shlb_user_T0_T1_cc, @function
op_shlb_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L810
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movzbl	%bl, %edx
	sall	%cl, %edx
	movb	%al, %cl
	movl	%edi, %eax
	movl	%edx, 12(%esp)
	shrl	$12, %eax
	andl	$255, %eax
	sall	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L811
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L814
	.p2align 4,,7
.L811:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L814:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$34, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L810:
	addl	$16, %esp
	ret
	.size	op_shlb_user_T0_T1_cc, .-op_shlb_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrb_user_T0_T1_cc
	.type	op_shrb_user_T0_T1_cc, @function
op_shrb_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L816
	movl	%edi, 8(%esp)
	andl	$255, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 12(%esp)
	movl	%edi, %eax
	shrl	%cl, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L817
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L820
	.p2align 4,,7
.L817:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L820:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$38, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L816:
	addl	$16, %esp
	ret
	.size	op_shrb_user_T0_T1_cc, .-op_shrb_user_T0_T1_cc
	.p2align 4,,15
.globl op_sarb_user_T0_T1_cc
	.type	op_sarb_user_T0_T1_cc, @function
op_sarb_user_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L822
	movl	%edi, 8(%esp)
	movsbl	%bl,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L823
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L826
	.p2align 4,,7
.L823:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
.L826:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$38, 48(%ebp)
	movl	%eax, 40(%ebp)
.L822:
	addl	$16, %esp
	ret
	.size	op_sarb_user_T0_T1_cc, .-op_sarb_user_T0_T1_cc
	.p2align 4,,15
.globl op_adcb_user_T0_T1_cc
	.type	op_adcb_user_T0_T1_cc, @function
op_adcb_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	12(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, 8(%esp)
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L828
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L831
	.p2align 4,,7
.L828:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L831:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	6(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcb_user_T0_T1_cc, .-op_adcb_user_T0_T1_cc
	.p2align 4,,15
.globl op_sbbb_user_T0_T1_cc
	.type	op_sbbb_user_T0_T1_cc, @function
op_sbbb_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	subl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L833
	movl	$1, (%esp)
	movzbl	%bl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L836
	.p2align 4,,7
.L833:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%bl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU8
.L836:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	14(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbb_user_T0_T1_cc, .-op_sbbb_user_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgb_user_T0_T1_EAX_cc
	.type	op_cmpxchgb_user_T0_T1_EAX_cc, @function
op_cmpxchgb_user_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	movl	%eax, 12(%esp)
	movzbl	12(%esp), %eax
	testb	%al, %al
	jne	.L838
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%esi, %ebx
	shrl	$12, %eax
	movl	%esi, %edx
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4096, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L839
	movl	$1, (%esp)
	movzbl	%dl, %edx
	movl	%edi, %eax
	call	__stb_mmu
	jmp	.L843
	.p2align 4,,7
.L839:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzbl	%dl, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU8
	jmp	.L843
	.p2align 4,,7
.L838:
	movb	%bl, (%ebp)
.L843:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgb_user_T0_T1_EAX_cc, .-op_cmpxchgb_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftb
	.type	op_movl_T0_Dshiftb, @function
op_movl_T0_Dshiftb:
	movl	52(%ebp), %ebx
	ret
	.size	op_movl_T0_Dshiftb, .-op_movl_T0_Dshiftb
	.p2align 4,,15
.globl op_outb_T0_T1
	.type	op_outb_T0_T1, @function
op_outb_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$255, %eax
	movl	%eax, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_outb
	addl	$12, %esp
	ret
	.size	op_outb_T0_T1, .-op_outb_T0_T1
	.p2align 4,,15
.globl op_inb_T0_T1
	.type	op_inb_T0_T1, @function
op_inb_T0_T1:
	subl	$8, %esp
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_inb
	movl	%eax, %esi
	addl	$8, %esp
	ret
	.size	op_inb_T0_T1, .-op_inb_T0_T1
	.p2align 4,,15
.globl op_inb_DX_T0
	.type	op_inb_DX_T0, @function
op_inb_DX_T0:
	subl	$8, %esp
	movzwl	8(%ebp), %eax
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_inb
	movl	%eax, %ebx
	addl	$8, %esp
	ret
	.size	op_inb_DX_T0, .-op_inb_DX_T0
	.p2align 4,,15
.globl op_outb_DX_T0
	.type	op_outb_DX_T0, @function
op_outb_DX_T0:
	subl	$12, %esp
	movzwl	8(%ebp), %eax
	movl	%ebx, 8(%esp)
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_outb
	addl	$12, %esp
	ret
	.size	op_outb_DX_T0, .-op_outb_DX_T0
	.p2align 4,,15
.globl op_check_iob_T0
	.type	op_check_iob_T0, @function
op_check_iob_T0:
	call	check_iob_T0
	ret
	.size	op_check_iob_T0, .-op_check_iob_T0
	.p2align 4,,15
.globl op_check_iob_DX
	.type	op_check_iob_DX, @function
op_check_iob_DX:
	call	check_iob_DX
	ret
	.size	op_check_iob_DX, .-op_check_iob_DX
	.p2align 4,,15
	.type	compute_all_addw, @function
compute_all_addw:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	subl	%eax, %ecx
	cmpw	%ax, 44(%ebp)
	movzbl	parity_table(%edx), %edx
	setb	%al
	movzbl	%al, %eax
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movzwl	44(%ebp), %edx
	cmpw	$1, %dx
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, 12(%esp)
	movl	44(%ebp), %edx
	sarl	$8, %edx
	andl	$128, %edx
	movl	%edx, (%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	notl	%ecx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sarl	$4, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_addw, .-compute_all_addw
	.p2align 4,,15
	.type	compute_c_addw, @function
compute_c_addw:
	movl	44(%ebp), %eax
	cmpw	%ax, 40(%ebp)
	seta	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_addw, .-compute_c_addw
	.p2align 4,,15
	.type	compute_all_adcw, @function
compute_all_adcw:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	movzbl	44(%ebp), %ecx
	movl	%eax, 8(%esp)
	subl	%eax, %edx
	decl	%edx
	movzbl	parity_table(%ecx), %ecx
	cmpw	%ax, 44(%ebp)
	movl	%ecx, 16(%esp)
	movl	8(%esp), %ecx
	setbe	%al
	xorl	44(%ebp), %ecx
	movzbl	%al, %eax
	xorl	%edx, %ecx
	andl	$16, %ecx
	movl	%ecx, 4(%esp)
	movzwl	44(%ebp), %ecx
	cmpw	$1, %cx
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	movl	%ecx, 12(%esp)
	movl	44(%ebp), %ecx
	sarl	$8, %ecx
	andl	$128, %ecx
	movl	%ecx, (%esp)
	movl	8(%esp), %ecx
	xorl	%ecx, %edx
	movl	44(%ebp), %ecx
	notl	%edx
	xorl	%ecx, 8(%esp)
	movl	8(%esp), %ecx
	andl	%ecx, %edx
	movl	16(%esp), %ecx
	sarl	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	orl	%ecx, %eax
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	addl	$20, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_adcw, .-compute_all_adcw
	.p2align 4,,15
	.type	compute_c_adcw, @function
compute_c_adcw:
	movl	44(%ebp), %eax
	cmpw	%ax, 40(%ebp)
	setae	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_adcw, .-compute_c_adcw
	.p2align 4,,15
	.type	compute_all_subw, @function
compute_all_subw:
	subl	$20, %esp
	movl	44(%ebp), %edx
	movl	40(%ebp), %eax
	movl	40(%ebp), %ecx
	addl	%edx, %eax
	cmpw	%cx, %ax
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	setb	%al
	movzbl	%al, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movzwl	44(%ebp), %edx
	cmpw	$1, %dx
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, 12(%esp)
	movl	44(%ebp), %edx
	sarl	$8, %edx
	andl	$128, %edx
	movl	%edx, (%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sarl	$4, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_subw, .-compute_all_subw
	.p2align 4,,15
	.type	compute_c_subw, @function
compute_c_subw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpw	%ax, 40(%ebp)
	seta	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_subw, .-compute_c_subw
	.p2align 4,,15
	.type	compute_all_sbbw, @function
compute_all_sbbw:
	subl	$16, %esp
	movl	44(%ebp), %eax
	movl	40(%ebp), %ecx
	movzbl	44(%ebp), %edx
	addl	%eax, %ecx
	movl	40(%ebp), %eax
	movzbl	parity_table(%edx), %edx
	incl	%ecx
	cmpw	%ax, %cx
	movl	%eax, 4(%esp)
	setbe	%al
	movzbl	%al, %eax
	movl	%edx, 12(%esp)
	movl	44(%ebp), %edx
	xorl	%ecx, %edx
	movl	%edx, (%esp)
	movl	4(%esp), %edx
	xorl	%edx, (%esp)
	movzwl	44(%ebp), %edx
	andl	$16, (%esp)
	cmpw	$1, %dx
	sbbl	%edx, %edx
	xorl	%ecx, 4(%esp)
	andl	$64, %edx
	xorl	44(%ebp), %ecx
	movl	%edx, 8(%esp)
	movl	44(%ebp), %edx
	andl	%ecx, 4(%esp)
	movl	12(%esp), %ecx
	sarl	$8, %edx
	sarl	$4, 4(%esp)
	andl	$128, %edx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	andl	$2048, 4(%esp)
	orl	%ecx, %eax
	movl	8(%esp), %ecx
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	orl	%edx, %eax
	addl	$16, %esp
	orl	%ecx, %eax
	ret
	.size	compute_all_sbbw, .-compute_all_sbbw
	.p2align 4,,15
	.type	compute_c_sbbw, @function
compute_c_sbbw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	incl	%eax
	cmpw	%ax, 40(%ebp)
	setae	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_sbbw, .-compute_c_sbbw
	.p2align 4,,15
	.type	compute_all_logicw, @function
compute_all_logicw:
	movzwl	44(%ebp), %edx
	movzbl	44(%ebp), %eax
	cmpw	$1, %dx
	movl	44(%ebp), %edx
	movzbl	parity_table(%eax), %eax
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	sarl	$8, %edx
	andl	$128, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_logicw, .-compute_all_logicw
	.p2align 4,,15
	.type	compute_c_logicw, @function
compute_c_logicw:
	xorl	%eax, %eax
	ret
	.size	compute_c_logicw, .-compute_c_logicw
	.p2align 4,,15
	.type	compute_all_incw, @function
compute_all_incw:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	movl	44(%ebp), %ecx
	decl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzwl	44(%ebp), %edx
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpw	$1, %dx
	sbbl	%edx, %edx
	movzbl	parity_table(%eax), %eax
	andl	$64, %edx
	sarl	$8, %ecx
	movl	%edx, 4(%esp)
	andl	$128, %ecx
	xorl	%edx, %edx
	cmpw	$-32768, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	4(%esp), %eax
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_incw, .-compute_all_incw
	.p2align 4,,15
	.type	compute_all_decw, @function
compute_all_decw:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	movl	44(%ebp), %ecx
	incl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzwl	44(%ebp), %edx
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpw	$1, %dx
	sbbl	%edx, %edx
	movzbl	parity_table(%eax), %eax
	andl	$64, %edx
	sarl	$8, %ecx
	movl	%edx, 4(%esp)
	andl	$128, %ecx
	xorl	%edx, %edx
	cmpw	$32767, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	4(%esp), %eax
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_decw, .-compute_all_decw
	.p2align 4,,15
	.type	compute_all_shlw, @function
compute_all_shlw:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	shrl	$15, %eax
	andl	$1, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movzwl	44(%ebp), %edx
	cmpw	$1, %dx
	sbbl	%edx, %edx
	orl	4(%esp), %eax
	andl	$64, %edx
	sarl	$8, %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	$128, %ecx
	xorl	40(%ebp), %edx
	orl	(%esp), %eax
	addl	$8, %esp
	sarl	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_shlw, .-compute_all_shlw
	.p2align 4,,15
	.type	compute_c_shlw, @function
compute_c_shlw:
	movl	40(%ebp), %eax
	shrl	$15, %eax
	andl	$1, %eax
	ret
	.size	compute_c_shlw, .-compute_c_shlw
	.p2align 4,,15
	.type	compute_all_sarw, @function
compute_all_sarw:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	andl	$1, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movzwl	44(%ebp), %edx
	cmpw	$1, %dx
	sbbl	%edx, %edx
	orl	4(%esp), %eax
	andl	$64, %edx
	sarl	$8, %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	$128, %ecx
	xorl	40(%ebp), %edx
	orl	(%esp), %eax
	addl	$8, %esp
	sarl	$4, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_sarw, .-compute_all_sarw
	.p2align 4,,15
	.type	compute_all_mulw, @function
compute_all_mulw:
	subl	$12, %esp
	xorl	%eax, %eax
	movzbl	44(%ebp), %edx
	cmpl	$0, 40(%ebp)
	movzbl	parity_table(%edx), %edx
	setne	%al
	movl	%eax, %ecx
	movl	%edx, 8(%esp)
	movzwl	44(%ebp), %edx
	cmpw	$1, %dx
	sbbl	%edx, %edx
	orl	8(%esp), %eax
	andl	$64, %edx
	sall	$11, %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	orl	(%esp), %eax
	addl	$12, %esp
	sarl	$8, %edx
	andl	$128, %edx
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_mulw, .-compute_all_mulw
	.p2align 4,,15
.globl op_jb_subw
	.type	op_jb_subw, @function
op_jb_subw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpw	%ax, 40(%ebp)
	jbe	.L940
#APP
	jmp __op_gen_label1
#NO_APP
.L940:
	ret
	.size	op_jb_subw, .-op_jb_subw
	.p2align 4,,15
.globl op_jz_subw
	.type	op_jz_subw, @function
op_jz_subw:
	cmpw	$0, 44(%ebp)
	jne	.L942
#APP
	jmp __op_gen_label1
#NO_APP
.L942:
	ret
	.size	op_jz_subw, .-op_jz_subw
	.p2align 4,,15
.globl op_jnz_subw
	.type	op_jnz_subw, @function
op_jnz_subw:
	cmpw	$0, 44(%ebp)
	je	.L944
#APP
	jmp __op_gen_label1
#NO_APP
.L944:
	ret
	.size	op_jnz_subw, .-op_jnz_subw
	.p2align 4,,15
.globl op_jbe_subw
	.type	op_jbe_subw, @function
op_jbe_subw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpw	%ax, 40(%ebp)
	jb	.L946
#APP
	jmp __op_gen_label1
#NO_APP
.L946:
	ret
	.size	op_jbe_subw, .-op_jbe_subw
	.p2align 4,,15
.globl op_js_subw
	.type	op_js_subw, @function
op_js_subw:
	movl	44(%ebp), %eax
	andl	$32768, %eax
	testw	%ax, %ax
	je	.L948
#APP
	jmp __op_gen_label1
#NO_APP
.L948:
	ret
	.size	op_js_subw, .-op_js_subw
	.p2align 4,,15
.globl op_jl_subw
	.type	op_jl_subw, @function
op_jl_subw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpw	%ax, 40(%ebp)
	jle	.L950
#APP
	jmp __op_gen_label1
#NO_APP
.L950:
	ret
	.size	op_jl_subw, .-op_jl_subw
	.p2align 4,,15
.globl op_jle_subw
	.type	op_jle_subw, @function
op_jle_subw:
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpw	%ax, 40(%ebp)
	jl	.L952
#APP
	jmp __op_gen_label1
#NO_APP
.L952:
	ret
	.size	op_jle_subw, .-op_jle_subw
	.p2align 4,,15
.globl op_loopnzw
	.type	op_loopnzw, @function
op_loopnzw:
	cmpw	$0, 4(%ebp)
	je	.L954
	testb	$64, %bl
	jne	.L954
#APP
	jmp __op_gen_label1
	.p2align 4,,15
#NO_APP
.L954:
	ret
	.size	op_loopnzw, .-op_loopnzw
	.p2align 4,,15
.globl op_loopzw
	.type	op_loopzw, @function
op_loopzw:
	cmpw	$0, 4(%ebp)
	je	.L956
	testb	$64, %bl
	je	.L956
#APP
	jmp __op_gen_label1
	.p2align 4,,15
#NO_APP
.L956:
	ret
	.size	op_loopzw, .-op_loopzw
	.p2align 4,,15
.globl op_jz_ecxw
	.type	op_jz_ecxw, @function
op_jz_ecxw:
	cmpw	$0, 4(%ebp)
	jne	.L958
#APP
	jmp __op_gen_label1
#NO_APP
.L958:
	ret
	.size	op_jz_ecxw, .-op_jz_ecxw
	.p2align 4,,15
.globl op_jnz_ecxw
	.type	op_jnz_ecxw, @function
op_jnz_ecxw:
	cmpw	$0, 4(%ebp)
	je	.L960
#APP
	jmp __op_gen_label1
#NO_APP
.L960:
	ret
	.size	op_jnz_ecxw, .-op_jnz_ecxw
	.p2align 4,,15
.globl op_setb_T0_subw
	.type	op_setb_T0_subw, @function
op_setb_T0_subw:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpw	%ax, 40(%ebp)
	seta	%bl
	ret
	.size	op_setb_T0_subw, .-op_setb_T0_subw
	.p2align 4,,15
.globl op_setz_T0_subw
	.type	op_setz_T0_subw, @function
op_setz_T0_subw:
	xorl	%ebx, %ebx
	cmpw	$0, 44(%ebp)
	sete	%bl
	ret
	.size	op_setz_T0_subw, .-op_setz_T0_subw
	.p2align 4,,15
.globl op_setbe_T0_subw
	.type	op_setbe_T0_subw, @function
op_setbe_T0_subw:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpw	%ax, 40(%ebp)
	setae	%bl
	ret
	.size	op_setbe_T0_subw, .-op_setbe_T0_subw
	.p2align 4,,15
.globl op_sets_T0_subw
	.type	op_sets_T0_subw, @function
op_sets_T0_subw:
	movl	44(%ebp), %eax
	sarl	$15, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_sets_T0_subw, .-op_sets_T0_subw
	.p2align 4,,15
.globl op_setl_T0_subw
	.type	op_setl_T0_subw, @function
op_setl_T0_subw:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpw	%ax, 40(%ebp)
	setg	%bl
	ret
	.size	op_setl_T0_subw, .-op_setl_T0_subw
	.p2align 4,,15
.globl op_setle_T0_subw
	.type	op_setle_T0_subw, @function
op_setle_T0_subw:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpw	%ax, 40(%ebp)
	setge	%bl
	ret
	.size	op_setle_T0_subw, .-op_setle_T0_subw
	.p2align 4,,15
.globl op_shlw_T0_T1
	.type	op_shlw_T0_T1, @function
op_shlw_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
	.size	op_shlw_T0_T1, .-op_shlw_T0_T1
	.p2align 4,,15
.globl op_shrw_T0_T1
	.type	op_shrw_T0_T1, @function
op_shrw_T0_T1:
	movl	%esi, %ecx
	andl	$65535, %ebx
	andl	$31, %ecx
	shrl	%cl, %ebx
	ret
	.size	op_shrw_T0_T1, .-op_shrw_T0_T1
	.p2align 4,,15
.globl op_sarw_T0_T1
	.type	op_sarw_T0_T1, @function
op_sarw_T0_T1:
	movl	%esi, %ecx
	movswl	%bx,%eax
	andl	$31, %ecx
	movl	%eax, %ebx
	sarl	%cl, %ebx
	ret
	.size	op_sarw_T0_T1, .-op_sarw_T0_T1
	.p2align 4,,15
.globl op_rolw_T0_T1_cc
	.type	op_rolw_T0_T1_cc, @function
op_rolw_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L974
	movl	%ebx, (%esp)
	movl	%esi, %eax
	andl	$15, %eax
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$4, (%esp)
	andl	$2048, (%esp)
	orl	%eax, (%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L974:
	popl	%eax
	ret
	.size	op_rolw_T0_T1_cc, .-op_rolw_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_T0_T1_cc
	.type	op_rorw_T0_T1_cc, @function
op_rorw_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L979
	movl	%ebx, (%esp)
	movl	%esi, %eax
	andl	$15, %eax
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$4, (%esp)
	andl	$2048, (%esp)
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%ebx, %edx
	shrl	$15, %edx
	andl	$1, %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L979:
	popl	%ecx
	ret
	.size	op_rorw_T0_T1_cc, .-op_rorw_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_T0_T1
	.type	op_rolw_T0_T1, @function
op_rolw_T0_T1:
	movl	%esi, %ecx
	subl	$4, %esp
	andl	$15, %ecx
	je	.L984
	movl	$16, (%esp)
	andl	$65535, %ebx
	movl	%ebx, %eax
	subl	%ecx, (%esp)
	sall	%cl, %eax
	movl	%ebx, %edx
	movl	%eax, %ebx
	movzbl	(%esp), %ecx
	shrl	%cl, %edx
	orl	%edx, %ebx
.L984:
	popl	%ecx
	ret
	.size	op_rolw_T0_T1, .-op_rolw_T0_T1
	.p2align 4,,15
.globl op_rorw_T0_T1
	.type	op_rorw_T0_T1, @function
op_rorw_T0_T1:
	movl	%esi, %ecx
	subl	$4, %esp
	andl	$15, %ecx
	je	.L986
	movl	$16, (%esp)
	andl	$65535, %ebx
	movl	%ebx, %eax
	subl	%ecx, (%esp)
	shrl	%cl, %eax
	movl	%ebx, %edx
	movl	%eax, %ebx
	movzbl	(%esp), %ecx
	sall	%cl, %edx
	orl	%edx, %ebx
.L986:
	popl	%eax
	ret
	.size	op_rorw_T0_T1, .-op_rorw_T0_T1
	.p2align 4,,15
.globl op_rclw_T0_T1_cc
	.type	op_rclw_T0_T1_cc, @function
op_rclw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 8(%esp)
	je	.L988
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%eax, (%esp)
	movl	8(%esp), %ecx
	movl	4(%esp), %eax
	decl	%ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L989
	movl	8(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, (%esp)
.L989:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	$16, %ecx
	xorl	%ebx, %eax
	sarl	$4, %eax
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	movl	8(%esp), %eax
	subl	%eax, %ecx
	movl	4(%esp), %eax
	shrl	%cl, %edx
	andl	$1, %edx
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L988:
	addl	$12, %esp
	ret
	.size	op_rclw_T0_T1_cc, .-op_rclw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_T0_T1_cc
	.type	op_rcrw_T0_T1_cc, @function
op_rcrw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 8(%esp)
	je	.L994
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%eax, (%esp)
	movl	$16, %ecx
	movl	4(%esp), %eax
	subl	8(%esp), %ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L995
	movl	8(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
.L995:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	8(%esp), %ecx
	xorl	%ebx, %eax
	sarl	$4, %eax
	decl	%ecx
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	shrl	%cl, %edx
	andl	$1, %edx
	movl	4(%esp), %eax
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L994:
	addl	$12, %esp
	ret
	.size	op_rcrw_T0_T1_cc, .-op_rcrw_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_T0_T1_cc
	.type	op_shlw_T0_T1_cc, @function
op_shlw_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L1000
	movl	$35, 48(%ebp)
	movzwl	%bx, %eax
	leal	-1(%edx), %ecx
	sall	%cl, %eax
	movb	%dl, %cl
	sall	%cl, %ebx
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L1000:
	ret
	.size	op_shlw_T0_T1_cc, .-op_shlw_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_T0_T1_cc
	.type	op_shrw_T0_T1_cc, @function
op_shrw_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L1002
	movl	$39, 48(%ebp)
	andl	$65535, %ebx
	leal	-1(%edx), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movb	%dl, %cl
	movl	%eax, 40(%ebp)
	shrl	%cl, %ebx
	movl	%ebx, 44(%ebp)
.L1002:
	ret
	.size	op_shrw_T0_T1_cc, .-op_shrw_T0_T1_cc
	.p2align 4,,15
.globl op_sarw_T0_T1_cc
	.type	op_sarw_T0_T1_cc, @function
op_sarw_T0_T1_cc:
	movl	%esi, %ecx
	andl	$31, %ecx
	je	.L1004
	movl	$39, 48(%ebp)
	movswl	%bx,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L1004:
	ret
	.size	op_sarw_T0_T1_cc, .-op_sarw_T0_T1_cc
	.p2align 4,,15
.globl op_shldw_T0_T1_im_cc
	.type	op_shldw_T0_T1_im_cc, @function
op_shldw_T0_T1_im_cc:
	movl	%ebx, %edx
	andl	$65535, %esi
	sall	$16, %edx
	orl	%esi, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%edx, %eax
	subl	$8, %esp
	shrl	%cl, %eax
	movl	%eax, (%esp)
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1006
	movl	%esi, %eax
	movl	$__op_param1-16, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1006:
	movl	(%esp), %ecx
	movl	%edx, %ebx
	shrl	$16, %ebx
	movl	%ebx, 44(%ebp)
	movl	%ecx, 40(%ebp)
	addl	$8, %esp
	ret
	.size	op_shldw_T0_T1_im_cc, .-op_shldw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_T0_T1_ECX_cc
	.type	op_shldw_T0_T1_ECX_cc, @function
op_shldw_T0_T1_ECX_cc:
	subl	$12, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, (%esp)
	je	.L1008
	movl	$32, %ecx
	movl	%ebx, %edx
	andl	$65535, %esi
	movl	%ecx, 4(%esp)
	sall	$16, %edx
	orl	%esi, %edx
	subl	%eax, 4(%esp)
	movl	%edx, %eax
	movzbl	4(%esp), %ecx
	shrl	%cl, %eax
	movzbl	(%esp), %ecx
	movl	%eax, 8(%esp)
	sall	%cl, %edx
	cmpl	$16, (%esp)
	jle	.L1009
	movl	(%esp), %ecx
	movl	%esi, %eax
	subl	$16, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1009:
	movl	$39, 48(%ebp)
	movl	8(%esp), %eax
	movl	%edx, %ebx
	shrl	$16, %ebx
	movl	%ebx, 44(%ebp)
	movl	%eax, 40(%ebp)
.L1008:
	addl	$12, %esp
	ret
	.size	op_shldw_T0_T1_ECX_cc, .-op_shldw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_T0_T1_im_cc
	.type	op_shrdw_T0_T1_im_cc, @function
op_shrdw_T0_T1_im_cc:
	movl	%esi, %eax
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	movl	$__op_param1-1, %ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movl	$__op_param1, %ecx
	subl	$4, %esp
	movl	%eax, (%esp)
	shrl	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1011
	movl	$32, %ecx
	movl	%esi, %eax
	subl	$__op_param1, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1011:
	movl	%edx, 44(%ebp)
	movl	(%esp), %eax
	movl	%edx, %ebx
	movl	%eax, 40(%ebp)
	popl	%eax
	ret
	.size	op_shrdw_T0_T1_im_cc, .-op_shrdw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdw_T0_T1_ECX_cc
	.type	op_shrdw_T0_T1_ECX_cc, @function
op_shrdw_T0_T1_ECX_cc:
	subl	$16, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 4(%esp)
	je	.L1013
	movl	%esi, %eax
	movl	4(%esp), %ecx
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	decl	%ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movzbl	4(%esp), %ecx
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	cmpl	$16, 4(%esp)
	jle	.L1014
	movl	$32, (%esp)
	movl	4(%esp), %eax
	subl	%eax, (%esp)
	movl	%esi, %eax
	movzbl	(%esp), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1014:
	movl	%edx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	%edx, %ebx
	movl	$39, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1013:
	addl	$16, %esp
	ret
	.size	op_shrdw_T0_T1_ECX_cc, .-op_shrdw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcw_T0_T1_cc
	.type	op_adcw_T0_T1_cc, @function
op_adcw_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	leal	(%ebx,%esi), %edx
	leal	(%edx,%eax), %ebx
	movl	%ebx, 44(%ebp)
	leal	7(,%eax,4), %eax
	movl	%eax, 48(%ebp)
	ret
	.size	op_adcw_T0_T1_cc, .-op_adcw_T0_T1_cc
	.p2align 4,,15
.globl op_sbbw_T0_T1_cc
	.type	op_sbbw_T0_T1_cc, @function
op_sbbw_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	movl	%ebx, %edx
	subl	%esi, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	15(,%eax,4), %eax
	movl	%ebx, 44(%ebp)
	movl	%eax, 48(%ebp)
	ret
	.size	op_sbbw_T0_T1_cc, .-op_sbbw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_T0_T1_EAX_cc
	.type	op_cmpxchgw_T0_T1_EAX_cc, @function
op_cmpxchgw_T0_T1_EAX_cc:
	movl	(%ebp), %eax
	movl	%ebx, %edx
	subl	%ebx, %eax
	testw	%ax, %ax
	jne	.L1018
	movl	%esi, %ebx
	jmp	.L1019
	.p2align 4,,7
.L1018:
	movw	%bx, (%ebp)
.L1019:
	movl	%edx, 40(%ebp)
	movl	%eax, 44(%ebp)
	ret
	.size	op_cmpxchgw_T0_T1_EAX_cc, .-op_cmpxchgw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolw_raw_T0_T1_cc
	.type	op_rolw_raw_T0_T1_cc, @function
op_rolw_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L1021
	movl	%ebx, 8(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, (%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$4, 8(%esp)
	andl	$2048, 8(%esp)
	orl	%eax, 8(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	8(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1021:
	addl	$12, %esp
	ret
	.size	op_rolw_raw_T0_T1_cc, .-op_rolw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_raw_T0_T1_cc
	.type	op_rorw_raw_T0_T1_cc, @function
op_rorw_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L1027
	movl	%ebx, 8(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, (%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$15, %edx
	andl	$1, %edx
	sarl	$4, 8(%esp)
	andl	$2048, 8(%esp)
	movl	8(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1027:
	addl	$12, %esp
	ret
	.size	op_rorw_raw_T0_T1_cc, .-op_rorw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_raw_T0_T1
	.type	op_rolw_raw_T0_T1, @function
op_rolw_raw_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$15, %ecx
	je	.L1033
	movl	%edi, (%esp)
	movl	$16, %eax
	andl	$65535, %ebx
	movl	%eax, 8(%esp)
	movl	%ebx, %edx
	sall	%cl, %edx
	subl	%ecx, 8(%esp)
	movl	%ebx, %eax
	movl	%edx, %ebx
	movzbl	8(%esp), %ecx
	shrl	%cl, %eax
	orl	%eax, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	.p2align 4,,15
.L1033:
	addl	$12, %esp
	ret
	.size	op_rolw_raw_T0_T1, .-op_rolw_raw_T0_T1
	.p2align 4,,15
.globl op_rorw_raw_T0_T1
	.type	op_rorw_raw_T0_T1, @function
op_rorw_raw_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$15, %ecx
	je	.L1036
	movl	%edi, (%esp)
	movl	$16, %eax
	andl	$65535, %ebx
	movl	%eax, 8(%esp)
	movl	%ebx, %edx
	shrl	%cl, %edx
	subl	%ecx, 8(%esp)
	movl	%ebx, %eax
	movl	%edx, %ebx
	movzbl	8(%esp), %ecx
	sall	%cl, %eax
	orl	%eax, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	.p2align 4,,15
.L1036:
	addl	$12, %esp
	ret
	.size	op_rorw_raw_T0_T1, .-op_rorw_raw_T0_T1
	.p2align 4,,15
.globl op_rclw_raw_T0_T1_cc
	.type	op_rclw_raw_T0_T1_cc, @function
op_rclw_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 16(%esp)
	je	.L1039
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 12(%esp)
	sall	%cl, %edx
	movl	16(%esp), %ecx
	movl	%ebx, 8(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L1040
	movl	16(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1040:
	movl	%edi, (%esp)
	movzwl	%dx, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	andl	$-2050, 12(%esp)
	movl	$16, %ecx
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$4, %eax
	andl	$2048, %eax
	orl	%eax, 12(%esp)
	movl	16(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 8(%esp)
	movl	12(%esp), %eax
	andl	$1, 8(%esp)
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1039:
	addl	$20, %esp
	ret
	.size	op_rclw_raw_T0_T1_cc, .-op_rclw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_raw_T0_T1_cc
	.type	op_rcrw_raw_T0_T1_cc, @function
op_rcrw_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 16(%esp)
	je	.L1046
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	16(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 8(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L1047
	movl	16(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1047:
	movl	%edi, (%esp)
	movzwl	%dx, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	andl	$-2050, 12(%esp)
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	movl	16(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 8(%esp)
	orl	%eax, 12(%esp)
	andl	$1, 8(%esp)
	movl	12(%esp), %eax
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1046:
	addl	$20, %esp
	ret
	.size	op_rcrw_raw_T0_T1_cc, .-op_rcrw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_raw_T0_T1_cc
	.type	op_shlw_raw_T0_T1_cc, @function
op_shlw_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	je	.L1053
	movl	%edi, (%esp)
	leal	-1(%eax), %ecx
	movzwl	%bx, %edx
	sall	%cl, %edx
	movb	%al, %cl
	sall	%cl, %ebx
	movl	%edx, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$35, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1053:
	addl	$12, %esp
	ret
	.size	op_shlw_raw_T0_T1_cc, .-op_shlw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_raw_T0_T1_cc
	.type	op_shrw_raw_T0_T1_cc, @function
op_shrw_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	je	.L1056
	movl	%edi, (%esp)
	andl	$65535, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 8(%esp)
	shrl	%cl, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1056:
	addl	$12, %esp
	ret
	.size	op_shrw_raw_T0_T1_cc, .-op_shrw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sarw_raw_T0_T1_cc
	.type	op_sarw_raw_T0_T1_cc, @function
op_sarw_raw_T0_T1_cc:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1059
	movl	%edi, (%esp)
	movswl	%bx,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1059:
	addl	$12, %esp
	ret
	.size	op_sarw_raw_T0_T1_cc, .-op_sarw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shldw_raw_T0_T1_im_cc
	.type	op_shldw_raw_T0_T1_im_cc, @function
op_shldw_raw_T0_T1_im_cc:
	movl	%ebx, %edx
	andl	$65535, %esi
	sall	$16, %edx
	orl	%esi, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%edx, %eax
	subl	$12, %esp
	shrl	%cl, %eax
	movl	%eax, 8(%esp)
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1062
	movl	$__op_param1-16, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1062:
	movl	%edi, (%esp)
	movl	%edx, %ebx
	shrl	$16, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_shldw_raw_T0_T1_im_cc, .-op_shldw_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_raw_T0_T1_ECX_cc
	.type	op_shldw_raw_T0_T1_ECX_cc, @function
op_shldw_raw_T0_T1_ECX_cc:
	subl	$20, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1065
	movl	$32, %ecx
	movl	%ebx, %edx
	andl	$65535, %esi
	movl	%ecx, 12(%esp)
	sall	$16, %edx
	orl	%esi, %edx
	subl	%eax, 12(%esp)
	movl	%edx, %eax
	movzbl	12(%esp), %ecx
	shrl	%cl, %eax
	movzbl	8(%esp), %ecx
	movl	%eax, 16(%esp)
	sall	%cl, %edx
	cmpl	$16, 8(%esp)
	jle	.L1066
	movl	8(%esp), %ecx
	movl	%esi, %eax
	subl	$16, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1066:
	movl	%edi, (%esp)
	movl	%edx, %ebx
	shrl	$16, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	16(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1065:
	addl	$20, %esp
	ret
	.size	op_shldw_raw_T0_T1_ECX_cc, .-op_shldw_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_raw_T0_T1_im_cc
	.type	op_shrdw_raw_T0_T1_im_cc, @function
op_shrdw_raw_T0_T1_im_cc:
	movl	%esi, %eax
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	movl	$__op_param1-1, %ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movl	$__op_param1, %ecx
	subl	$12, %esp
	movl	%eax, 8(%esp)
	shrl	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1069
	movl	$32, %ecx
	movl	%esi, %eax
	subl	$__op_param1, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1069:
	movl	%edi, (%esp)
	movzwl	%dx, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_shrdw_raw_T0_T1_im_cc, .-op_shrdw_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdw_raw_T0_T1_ECX_cc
	.type	op_shrdw_raw_T0_T1_ECX_cc, @function
op_shrdw_raw_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1072
	movl	%esi, %eax
	movl	12(%esp), %ecx
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	decl	%ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movzbl	12(%esp), %ecx
	movl	%eax, 20(%esp)
	shrl	%cl, %edx
	cmpl	$16, 12(%esp)
	jle	.L1073
	movl	$32, %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	subl	%eax, 8(%esp)
	movl	%esi, %eax
	movzbl	8(%esp), %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1073:
	movl	%edi, (%esp)
	movzwl	%dx, %eax
	movl	%edx, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %ecx
	movl	$39, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1072:
	addl	$24, %esp
	ret
	.size	op_shrdw_raw_T0_T1_ECX_cc, .-op_shrdw_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcw_raw_T0_T1_cc
	.type	op_adcw_raw_T0_T1_cc, @function
op_adcw_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	8(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, (%esp)
	leal	(%eax,%edx), %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	7(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_adcw_raw_T0_T1_cc, .-op_adcw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sbbw_raw_T0_T1_cc
	.type	op_sbbw_raw_T0_T1_cc, @function
op_sbbw_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, (%esp)
	movl	%eax, %ebx
	movl	8(%esp), %eax
	subl	%eax, %ebx
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	15(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_sbbw_raw_T0_T1_cc, .-op_sbbw_raw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_raw_T0_T1_EAX_cc
	.type	op_cmpxchgw_raw_T0_T1_EAX_cc, @function
op_cmpxchgw_raw_T0_T1_EAX_cc:
	subl	$16, %esp
	movl	(%ebp), %eax
	movl	%ebx, 12(%esp)
	subl	%ebx, %eax
	testw	%ax, %ax
	movl	%eax, 8(%esp)
	jne	.L1080
	movl	%edi, (%esp)
	movzwl	%si, %eax
	movl	%esi, %ebx
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
	jmp	.L1082
	.p2align 4,,7
.L1080:
	movw	%bx, (%ebp)
.L1082:
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$16, %esp
	ret
	.size	op_cmpxchgw_raw_T0_T1_EAX_cc, .-op_cmpxchgw_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolw_kernel_T0_T1_cc
	.type	op_rolw_kernel_T0_T1_cc, @function
op_rolw_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1084
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1085
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1088
	.p2align 4,,7
.L1085:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1088:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$4, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1084:
	addl	$16, %esp
	ret
	.size	op_rolw_kernel_T0_T1_cc, .-op_rolw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_kernel_T0_T1_cc
	.type	op_rorw_kernel_T0_T1_cc, @function
op_rorw_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1093
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1094
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1097
	.p2align 4,,7
.L1094:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1097:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$15, %edx
	andl	$1, %edx
	sarl	$4, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1093:
	addl	$16, %esp
	ret
	.size	op_rorw_kernel_T0_T1_cc, .-op_rorw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_kernel_T0_T1
	.type	op_rolw_kernel_T0_T1, @function
op_rolw_kernel_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$15, %eax
	je	.L1102
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1103
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1102
	.p2align 4,,7
.L1103:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1102:
	addl	$12, %esp
	ret
	.size	op_rolw_kernel_T0_T1, .-op_rolw_kernel_T0_T1
	.p2align 4,,15
.globl op_rorw_kernel_T0_T1
	.type	op_rorw_kernel_T0_T1, @function
op_rorw_kernel_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$15, %eax
	je	.L1108
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1109
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1108
	.p2align 4,,7
.L1109:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1108:
	addl	$12, %esp
	ret
	.size	op_rorw_kernel_T0_T1, .-op_rorw_kernel_T0_T1
	.p2align 4,,15
.globl op_rclw_kernel_T0_T1_cc
	.type	op_rclw_kernel_T0_T1_cc, @function
op_rclw_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L1114
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	sall	%cl, %edx
	movl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1115
	movl	20(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1115:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1116
	movl	$0, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1119
	.p2align 4,,7
.L1116:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1119:
	andl	$-2050, 16(%esp)
	movl	$16, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$4, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1114:
	addl	$24, %esp
	ret
	.size	op_rclw_kernel_T0_T1_cc, .-op_rclw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_kernel_T0_T1_cc
	.type	op_rcrw_kernel_T0_T1_cc, @function
op_rcrw_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L1124
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1125
	movl	20(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1125:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1126
	movl	$0, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1129
	.p2align 4,,7
.L1126:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1129:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1124:
	addl	$24, %esp
	ret
	.size	op_rcrw_kernel_T0_T1_cc, .-op_rcrw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_kernel_T0_T1_cc
	.type	op_shlw_kernel_T0_T1_cc, @function
op_shlw_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1134
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movzwl	%bx, %edx
	sall	%cl, %edx
	movb	%al, %cl
	sall	%cl, %ebx
	movl	%edx, 12(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1135
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1138
	.p2align 4,,7
.L1135:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1138:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$35, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1134:
	addl	$16, %esp
	ret
	.size	op_shlw_kernel_T0_T1_cc, .-op_shlw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_kernel_T0_T1_cc
	.type	op_shrw_kernel_T0_T1_cc, @function
op_shrw_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1140
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 12(%esp)
	shrl	%cl, %ebx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1141
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1144
	.p2align 4,,7
.L1141:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1144:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$39, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1140:
	addl	$16, %esp
	ret
	.size	op_shrw_kernel_T0_T1_cc, .-op_shrw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sarw_kernel_T0_T1_cc
	.type	op_sarw_kernel_T0_T1_cc, @function
op_sarw_kernel_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L1146
	movl	%edi, 8(%esp)
	movswl	%bx,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1147
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1150
	.p2align 4,,7
.L1147:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L1150:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1146:
	addl	$16, %esp
	ret
	.size	op_sarw_kernel_T0_T1_cc, .-op_sarw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shldw_kernel_T0_T1_im_cc
	.type	op_shldw_kernel_T0_T1_im_cc, @function
op_shldw_kernel_T0_T1_im_cc:
	movl	%ebx, %edx
	andl	$65535, %esi
	sall	$16, %edx
	orl	%esi, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%edx, %eax
	subl	$16, %esp
	shrl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1152
	movl	$__op_param1-16, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1152:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	movl	%edx, %ebx
	andl	$4080, %ecx
	andl	$-4095, %eax
	shrl	$16, %ebx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1153
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1156
	.p2align 4,,7
.L1153:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1156:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shldw_kernel_T0_T1_im_cc, .-op_shldw_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_kernel_T0_T1_ECX_cc
	.type	op_shldw_kernel_T0_T1_ECX_cc, @function
op_shldw_kernel_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1158
	movl	$32, %ecx
	movl	%ebx, %edx
	andl	$65535, %esi
	movl	%ecx, 12(%esp)
	sall	$16, %edx
	orl	%esi, %edx
	subl	%eax, 12(%esp)
	movl	%edx, %eax
	movzbl	12(%esp), %ecx
	shrl	%cl, %eax
	movzbl	8(%esp), %ecx
	movl	%eax, 20(%esp)
	sall	%cl, %edx
	cmpl	$16, 8(%esp)
	jle	.L1159
	movl	8(%esp), %ecx
	movl	%esi, %eax
	subl	$16, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1159:
	movl	%edi, 16(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	movl	%edx, %ebx
	andl	$4080, %ecx
	andl	$-4095, %eax
	shrl	$16, %ebx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1160
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1163
	.p2align 4,,7
.L1160:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1163:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1158:
	addl	$24, %esp
	ret
	.size	op_shldw_kernel_T0_T1_ECX_cc, .-op_shldw_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_kernel_T0_T1_im_cc
	.type	op_shrdw_kernel_T0_T1_im_cc, @function
op_shrdw_kernel_T0_T1_im_cc:
	movl	%esi, %eax
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	movl	$__op_param1-1, %ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movl	$__op_param1, %ecx
	subl	$16, %esp
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1165
	movl	$32, %ecx
	movl	%esi, %eax
	subl	$__op_param1, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1165:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1166
	movl	$0, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1169
	.p2align 4,,7
.L1166:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1169:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shrdw_kernel_T0_T1_im_cc, .-op_shrdw_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdw_kernel_T0_T1_ECX_cc
	.type	op_shrdw_kernel_T0_T1_ECX_cc, @function
op_shrdw_kernel_T0_T1_ECX_cc:
	subl	$20, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 16(%esp)
	je	.L1171
	movl	%esi, %eax
	movl	16(%esp), %ecx
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	decl	%ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movzbl	16(%esp), %ecx
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	cmpl	$16, 16(%esp)
	jle	.L1172
	movl	16(%esp), %eax
	movl	$32, %ecx
	subl	%eax, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1172:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1173
	movl	$0, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1176
	.p2align 4,,7
.L1173:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1176:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1171:
	addl	$20, %esp
	ret
	.size	op_shrdw_kernel_T0_T1_ECX_cc, .-op_shrdw_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcw_kernel_T0_T1_cc
	.type	op_adcw_kernel_T0_T1_cc, @function
op_adcw_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%edi, %ecx
	movl	12(%esp), %edx
	movl	%edi, 8(%esp)
	leal	(%ebx,%esi), %eax
	shrl	$8, %ecx
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1178
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1181
	.p2align 4,,7
.L1178:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1181:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	7(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcw_kernel_T0_T1_cc, .-op_adcw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sbbw_kernel_T0_T1_cc
	.type	op_sbbw_kernel_T0_T1_cc, @function
op_sbbw_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	movl	%edi, %ecx
	shrl	$8, %ecx
	subl	%eax, %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1183
	movl	$0, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1186
	.p2align 4,,7
.L1183:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1186:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	15(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbw_kernel_T0_T1_cc, .-op_sbbw_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgw_kernel_T0_T1_EAX_cc, @function
op_cmpxchgw_kernel_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	testw	%ax, %ax
	movl	%eax, 12(%esp)
	jne	.L1188
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4095, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%esi, %ebx
	je	.L1189
	movl	$0, (%esp)
	movzwl	%si, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1193
	.p2align 4,,7
.L1189:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%si, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
	jmp	.L1193
	.p2align 4,,7
.L1188:
	movw	%bx, (%ebp)
.L1193:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgw_kernel_T0_T1_EAX_cc, .-op_cmpxchgw_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_rolw_user_T0_T1_cc
	.type	op_rolw_user_T0_T1_cc, @function
op_rolw_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1195
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1196
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1199
	.p2align 4,,7
.L1196:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1199:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$4, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1195:
	addl	$16, %esp
	ret
	.size	op_rolw_user_T0_T1_cc, .-op_rolw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rorw_user_T0_T1_cc
	.type	op_rorw_user_T0_T1_cc, @function
op_rorw_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1204
	movl	%ebx, 12(%esp)
	movl	%esi, %eax
	andl	$15, %eax
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1205
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1208
	.p2align 4,,7
.L1205:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1208:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	movl	%ebx, %edx
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	shrl	$15, %edx
	andl	$1, %edx
	sarl	$4, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1204:
	addl	$16, %esp
	ret
	.size	op_rorw_user_T0_T1_cc, .-op_rorw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rolw_user_T0_T1
	.type	op_rolw_user_T0_T1, @function
op_rolw_user_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$15, %eax
	je	.L1213
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1214
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1213
	.p2align 4,,7
.L1214:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1213:
	addl	$12, %esp
	ret
	.size	op_rolw_user_T0_T1, .-op_rolw_user_T0_T1
	.p2align 4,,15
.globl op_rorw_user_T0_T1
	.type	op_rorw_user_T0_T1, @function
op_rorw_user_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$15, %eax
	je	.L1219
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	movb	%al, %cl
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1220
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1219
	.p2align 4,,7
.L1220:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1219:
	addl	$12, %esp
	ret
	.size	op_rorw_user_T0_T1, .-op_rorw_user_T0_T1
	.p2align 4,,15
.globl op_rclw_user_T0_T1_cc
	.type	op_rclw_user_T0_T1_cc, @function
op_rclw_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L1225
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	sall	%cl, %edx
	movl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	andl	$1, %eax
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1226
	movl	20(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1226:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1227
	movl	$1, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1230
	.p2align 4,,7
.L1227:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1230:
	andl	$-2050, 16(%esp)
	movl	$16, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$4, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1225:
	addl	$24, %esp
	ret
	.size	op_rclw_user_T0_T1_cc, .-op_rclw_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrw_user_T0_T1_cc
	.type	op_rcrw_user_T0_T1_cc, @function
op_rcrw_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movzbl	rclw_table(%eax), %ecx
	testl	%ecx, %ecx
	movl	%ecx, 20(%esp)
	je	.L1235
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	andl	$65535, %ebx
	movl	%ebx, %edx
	movl	%eax, 16(%esp)
	shrl	%cl, %edx
	movl	$16, %ecx
	subl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1236
	movl	20(%esp), %eax
	movl	$17, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1236:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1237
	movl	$1, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1240
	.p2align 4,,7
.L1237:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1240:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$4, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1235:
	addl	$24, %esp
	ret
	.size	op_rcrw_user_T0_T1_cc, .-op_rcrw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shlw_user_T0_T1_cc
	.type	op_shlw_user_T0_T1_cc, @function
op_shlw_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1245
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movzwl	%bx, %edx
	sall	%cl, %edx
	movb	%al, %cl
	movl	%edi, %eax
	movl	%edx, 12(%esp)
	shrl	$12, %eax
	andl	$255, %eax
	sall	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1246
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1249
	.p2align 4,,7
.L1246:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1249:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$35, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1245:
	addl	$16, %esp
	ret
	.size	op_shlw_user_T0_T1_cc, .-op_shlw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrw_user_T0_T1_cc
	.type	op_shrw_user_T0_T1_cc, @function
op_shrw_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1251
	movl	%edi, 8(%esp)
	andl	$65535, %ebx
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edx, 12(%esp)
	movl	%edi, %eax
	shrl	%cl, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1252
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1255
	.p2align 4,,7
.L1252:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1255:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$39, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1251:
	addl	$16, %esp
	ret
	.size	op_shrw_user_T0_T1_cc, .-op_shrw_user_T0_T1_cc
	.p2align 4,,15
.globl op_sarw_user_T0_T1_cc
	.type	op_sarw_user_T0_T1_cc, @function
op_sarw_user_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L1257
	movl	%edi, 8(%esp)
	movswl	%bx,%eax
	movl	%eax, %ebx
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1258
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1261
	.p2align 4,,7
.L1258:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
.L1261:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1257:
	addl	$16, %esp
	ret
	.size	op_sarw_user_T0_T1_cc, .-op_sarw_user_T0_T1_cc
	.p2align 4,,15
.globl op_shldw_user_T0_T1_im_cc
	.type	op_shldw_user_T0_T1_im_cc, @function
op_shldw_user_T0_T1_im_cc:
	movl	%ebx, %edx
	andl	$65535, %esi
	sall	$16, %edx
	orl	%esi, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%edx, %eax
	subl	$16, %esp
	shrl	%cl, %eax
	movl	%eax, 12(%esp)
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1263
	movl	$__op_param1-16, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1263:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	shrl	$16, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1264
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1267
	.p2align 4,,7
.L1264:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1267:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shldw_user_T0_T1_im_cc, .-op_shldw_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldw_user_T0_T1_ECX_cc
	.type	op_shldw_user_T0_T1_ECX_cc, @function
op_shldw_user_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1269
	movl	$32, %ecx
	movl	%ebx, %edx
	andl	$65535, %esi
	movl	%ecx, 12(%esp)
	sall	$16, %edx
	orl	%esi, %edx
	subl	%eax, 12(%esp)
	movl	%edx, %eax
	movzbl	12(%esp), %ecx
	shrl	%cl, %eax
	movzbl	8(%esp), %ecx
	movl	%eax, 20(%esp)
	sall	%cl, %edx
	cmpl	$16, 8(%esp)
	jle	.L1270
	movl	8(%esp), %ecx
	movl	%esi, %eax
	subl	$16, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1270:
	movl	%edi, 16(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	shrl	$16, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1271
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1274
	.p2align 4,,7
.L1271:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1274:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1269:
	addl	$24, %esp
	ret
	.size	op_shldw_user_T0_T1_ECX_cc, .-op_shldw_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdw_user_T0_T1_im_cc
	.type	op_shrdw_user_T0_T1_im_cc, @function
op_shrdw_user_T0_T1_im_cc:
	movl	%esi, %eax
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	movl	$__op_param1-1, %ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movl	$__op_param1, %ecx
	subl	$16, %esp
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	cmpl	$16, %ecx
	jle	.L1276
	movl	$32, %ecx
	movl	%esi, %eax
	subl	$__op_param1, %ecx
	sall	%cl, %eax
	orl	%eax, %edx
.L1276:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1277
	movl	$1, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1280
	.p2align 4,,7
.L1277:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1280:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shrdw_user_T0_T1_im_cc, .-op_shrdw_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdw_user_T0_T1_ECX_cc
	.type	op_shrdw_user_T0_T1_ECX_cc, @function
op_shrdw_user_T0_T1_ECX_cc:
	subl	$20, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 16(%esp)
	je	.L1282
	movl	%esi, %eax
	movl	16(%esp), %ecx
	movzwl	%bx,%edx
	sall	$16, %eax
	orl	%eax, %edx
	decl	%ecx
	movl	%edx, %eax
	shrl	%cl, %eax
	movzbl	16(%esp), %ecx
	movl	%eax, 12(%esp)
	shrl	%cl, %edx
	cmpl	$16, 16(%esp)
	jle	.L1283
	movl	16(%esp), %eax
	movl	$32, %ecx
	subl	%eax, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1283:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1284
	movl	$1, (%esp)
	movzwl	%dx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1287
	.p2align 4,,7
.L1284:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%dx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU16
.L1287:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$39, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1282:
	addl	$20, %esp
	ret
	.size	op_shrdw_user_T0_T1_ECX_cc, .-op_shrdw_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcw_user_T0_T1_cc
	.type	op_adcw_user_T0_T1_cc, @function
op_adcw_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	12(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, 8(%esp)
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1289
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1292
	.p2align 4,,7
.L1289:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1292:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	7(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcw_user_T0_T1_cc, .-op_adcw_user_T0_T1_cc
	.p2align 4,,15
.globl op_sbbw_user_T0_T1_cc
	.type	op_sbbw_user_T0_T1_cc, @function
op_sbbw_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	subl	%eax, %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1294
	movl	$1, (%esp)
	movzwl	%bx, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1297
	.p2align 4,,7
.L1294:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%bx, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU16
.L1297:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	15(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbw_user_T0_T1_cc, .-op_sbbw_user_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgw_user_T0_T1_EAX_cc
	.type	op_cmpxchgw_user_T0_T1_EAX_cc, @function
op_cmpxchgw_user_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	testw	%ax, %ax
	movl	%eax, 12(%esp)
	jne	.L1299
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%esi, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4095, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1300
	movl	$1, (%esp)
	movzwl	%si, %edx
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L1304
	.p2align 4,,7
.L1300:
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movzwl	%si, %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU16
	jmp	.L1304
	.p2align 4,,7
.L1299:
	movw	%bx, (%ebp)
.L1304:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgw_user_T0_T1_EAX_cc, .-op_cmpxchgw_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_btw_T0_T1_cc
	.type	op_btw_T0_T1_cc, @function
op_btw_T0_T1_cc:
	movl	%esi, %ecx
	movl	%ebx, %eax
	andl	$15, %ecx
	shrl	%cl, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_btw_T0_T1_cc, .-op_btw_T0_T1_cc
	.p2align 4,,15
.globl op_btsw_T0_T1_cc
	.type	op_btsw_T0_T1_cc, @function
op_btsw_T0_T1_cc:
	movl	%esi, %ecx
	movl	$1, %eax
	andl	$15, %ecx
	movl	%ebx, %esi
	sall	%cl, %eax
	shrl	%cl, %esi
	orl	%eax, %ebx
	ret
	.size	op_btsw_T0_T1_cc, .-op_btsw_T0_T1_cc
	.p2align 4,,15
.globl op_btrw_T0_T1_cc
	.type	op_btrw_T0_T1_cc, @function
op_btrw_T0_T1_cc:
	movl	%esi, %ecx
	movl	$-2, %eax
	andl	$15, %ecx
	movl	%ebx, %esi
	roll	%cl, %eax
	shrl	%cl, %esi
	andl	%eax, %ebx
	ret
	.size	op_btrw_T0_T1_cc, .-op_btrw_T0_T1_cc
	.p2align 4,,15
.globl op_btcw_T0_T1_cc
	.type	op_btcw_T0_T1_cc, @function
op_btcw_T0_T1_cc:
	movl	%esi, %ecx
	movl	$1, %eax
	andl	$15, %ecx
	movl	%ebx, %esi
	sall	%cl, %eax
	shrl	%cl, %esi
	xorl	%eax, %ebx
	ret
	.size	op_btcw_T0_T1_cc, .-op_btcw_T0_T1_cc
	.p2align 4,,15
.globl op_add_bitw_A0_T1
	.type	op_add_bitw_A0_T1, @function
op_add_bitw_A0_T1:
	movswl	%si,%eax
	sarl	$4, %eax
	leal	(%edi,%eax,2), %edi
	ret
	.size	op_add_bitw_A0_T1, .-op_add_bitw_A0_T1
	.p2align 4,,15
.globl op_bsfw_T0_cc
	.type	op_bsfw_T0_cc, @function
op_bsfw_T0_cc:
	movl	%ebx, %eax
	andl	$65535, %eax
	je	.L1311
	xorl	%edx, %edx
	.p2align 4,,15
.L1318:
	testb	$1, %al
	jne	.L1317
	incl	%edx
	sarl	%eax
	jmp	.L1318
	.p2align 4,,7
.L1317:
	movl	$1, 44(%ebp)
	movl	%edx, %esi
	jmp	.L1315
	.p2align 4,,7
.L1311:
	movl	$0, 44(%ebp)
.L1315:
	ret
	.size	op_bsfw_T0_cc, .-op_bsfw_T0_cc
	.p2align 4,,15
.globl op_bsrw_T0_cc
	.type	op_bsrw_T0_cc, @function
op_bsrw_T0_cc:
	movl	%ebx, %edx
	andl	$65535, %edx
	je	.L1320
	movl	$15, %ecx
	.p2align 4,,15
.L1327:
	movl	%edx, %eax
	andl	$32768, %eax
	testw	%ax, %ax
	jne	.L1326
	decl	%ecx
	addl	%edx, %edx
	jmp	.L1327
	.p2align 4,,7
.L1326:
	movl	$1, 44(%ebp)
	movl	%ecx, %esi
	jmp	.L1324
	.p2align 4,,7
.L1320:
	movl	$0, 44(%ebp)
.L1324:
	ret
	.size	op_bsrw_T0_cc, .-op_bsrw_T0_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftw
	.type	op_movl_T0_Dshiftw, @function
op_movl_T0_Dshiftw:
	movl	52(%ebp), %eax
	leal	(%eax,%eax), %ebx
	ret
	.size	op_movl_T0_Dshiftw, .-op_movl_T0_Dshiftw
	.p2align 4,,15
.globl op_outw_T0_T1
	.type	op_outw_T0_T1, @function
op_outw_T0_T1:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$65535, %eax
	movl	%eax, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_outw
	addl	$12, %esp
	ret
	.size	op_outw_T0_T1, .-op_outw_T0_T1
	.p2align 4,,15
.globl op_inw_T0_T1
	.type	op_inw_T0_T1, @function
op_inw_T0_T1:
	subl	$8, %esp
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_inw
	movl	%eax, %esi
	addl	$8, %esp
	ret
	.size	op_inw_T0_T1, .-op_inw_T0_T1
	.p2align 4,,15
.globl op_inw_DX_T0
	.type	op_inw_DX_T0, @function
op_inw_DX_T0:
	subl	$8, %esp
	movzwl	8(%ebp), %eax
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_inw
	movl	%eax, %ebx
	addl	$8, %esp
	ret
	.size	op_inw_DX_T0, .-op_inw_DX_T0
	.p2align 4,,15
.globl op_outw_DX_T0
	.type	op_outw_DX_T0, @function
op_outw_DX_T0:
	subl	$12, %esp
	movzwl	8(%ebp), %eax
	movl	%ebx, 8(%esp)
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_outw
	addl	$12, %esp
	ret
	.size	op_outw_DX_T0, .-op_outw_DX_T0
	.p2align 4,,15
.globl op_check_iow_T0
	.type	op_check_iow_T0, @function
op_check_iow_T0:
	call	check_iow_T0
	ret
	.size	op_check_iow_T0, .-op_check_iow_T0
	.p2align 4,,15
.globl op_check_iow_DX
	.type	op_check_iow_DX, @function
op_check_iow_DX:
	call	check_iow_DX
	ret
	.size	op_check_iow_DX, .-op_check_iow_DX
	.p2align 4,,15
	.type	compute_all_addl, @function
compute_all_addl:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	subl	%eax, %ecx
	cmpl	%eax, 44(%ebp)
	movzbl	parity_table(%edx), %edx
	setb	%al
	movzbl	%al, %eax
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, 12(%esp)
	movsbl	47(%ebp),%edx
	andl	$128, %edx
	movl	%edx, (%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	notl	%ecx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sarl	$20, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_addl, .-compute_all_addl
	.p2align 4,,15
	.type	compute_c_addl, @function
compute_c_addl:
	movl	44(%ebp), %eax
	cmpl	40(%ebp), %eax
	setb	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_addl, .-compute_c_addl
	.p2align 4,,15
	.type	compute_all_adcl, @function
compute_all_adcl:
	subl	$20, %esp
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	movzbl	44(%ebp), %ecx
	movl	%eax, 8(%esp)
	subl	%eax, %edx
	decl	%edx
	movzbl	parity_table(%ecx), %ecx
	cmpl	%eax, 44(%ebp)
	movl	%ecx, 16(%esp)
	movl	8(%esp), %ecx
	setbe	%al
	xorl	44(%ebp), %ecx
	movzbl	%al, %eax
	xorl	%edx, %ecx
	andl	$16, %ecx
	movl	%ecx, 4(%esp)
	movl	44(%ebp), %ecx
	cmpl	$1, %ecx
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	movl	%ecx, 12(%esp)
	movsbl	47(%ebp),%ecx
	andl	$128, %ecx
	movl	%ecx, (%esp)
	movl	8(%esp), %ecx
	xorl	%ecx, %edx
	movl	44(%ebp), %ecx
	notl	%edx
	xorl	%ecx, 8(%esp)
	movl	8(%esp), %ecx
	andl	%ecx, %edx
	movl	16(%esp), %ecx
	sarl	$20, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	orl	%ecx, %eax
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	movl	(%esp), %ecx
	addl	$20, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_adcl, .-compute_all_adcl
	.p2align 4,,15
	.type	compute_c_adcl, @function
compute_c_adcl:
	movl	44(%ebp), %eax
	cmpl	40(%ebp), %eax
	setbe	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_adcl, .-compute_c_adcl
	.p2align 4,,15
	.type	compute_all_subl, @function
compute_all_subl:
	subl	$20, %esp
	movl	44(%ebp), %edx
	movl	40(%ebp), %eax
	movl	40(%ebp), %ecx
	addl	%edx, %eax
	cmpl	%ecx, %eax
	movzbl	44(%ebp), %edx
	movl	%eax, 8(%esp)
	setb	%al
	movzbl	%al, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 16(%esp)
	movl	8(%esp), %edx
	xorl	44(%ebp), %edx
	xorl	%ecx, %edx
	andl	$16, %edx
	movl	%edx, 4(%esp)
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	sbbl	%edx, %edx
	andl	$64, %edx
	movl	%edx, 12(%esp)
	movsbl	47(%ebp),%edx
	andl	$128, %edx
	movl	%edx, (%esp)
	movl	8(%esp), %edx
	xorl	%edx, %ecx
	movl	44(%ebp), %edx
	xorl	%edx, 8(%esp)
	movl	8(%esp), %edx
	andl	%edx, %ecx
	movl	16(%esp), %edx
	sarl	$20, %ecx
	andl	$2048, %ecx
	orl	%edx, %eax
	movl	4(%esp), %edx
	orl	%edx, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	(%esp), %edx
	addl	$20, %esp
	orl	%edx, %eax
	orl	%ecx, %eax
	ret
	.size	compute_all_subl, .-compute_all_subl
	.p2align 4,,15
	.type	compute_c_subl, @function
compute_c_subl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	setb	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_subl, .-compute_c_subl
	.p2align 4,,15
	.type	compute_all_sbbl, @function
compute_all_sbbl:
	subl	$16, %esp
	movl	44(%ebp), %eax
	movl	40(%ebp), %ecx
	movzbl	44(%ebp), %edx
	addl	%eax, %ecx
	movl	40(%ebp), %eax
	movzbl	parity_table(%edx), %edx
	incl	%ecx
	cmpl	%eax, %ecx
	movl	%eax, 4(%esp)
	setbe	%al
	movzbl	%al, %eax
	movl	%edx, 12(%esp)
	movl	44(%ebp), %edx
	xorl	%ecx, %edx
	movl	%edx, (%esp)
	movl	4(%esp), %edx
	xorl	%edx, (%esp)
	movl	44(%ebp), %edx
	andl	$16, (%esp)
	cmpl	$1, %edx
	sbbl	%edx, %edx
	xorl	%ecx, 4(%esp)
	andl	$64, %edx
	xorl	44(%ebp), %ecx
	movl	%edx, 8(%esp)
	movsbl	47(%ebp),%edx
	andl	%ecx, 4(%esp)
	movl	12(%esp), %ecx
	andl	$128, %edx
	sarl	$20, 4(%esp)
	orl	%ecx, %eax
	movl	(%esp), %ecx
	andl	$2048, 4(%esp)
	orl	%ecx, %eax
	movl	8(%esp), %ecx
	orl	%ecx, %eax
	movl	4(%esp), %ecx
	orl	%edx, %eax
	addl	$16, %esp
	orl	%ecx, %eax
	ret
	.size	compute_all_sbbl, .-compute_all_sbbl
	.p2align 4,,15
	.type	compute_c_sbbl, @function
compute_c_sbbl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	incl	%eax
	cmpl	40(%ebp), %eax
	setbe	%al
	movzbl	%al, %eax
	ret
	.size	compute_c_sbbl, .-compute_c_sbbl
	.p2align 4,,15
	.type	compute_all_logicl, @function
compute_all_logicl:
	movzbl	44(%ebp), %eax
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	movzbl	parity_table(%eax), %eax
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	movsbl	47(%ebp),%edx
	orl	%ecx, %eax
	andl	$128, %edx
	orl	%edx, %eax
	ret
	.size	compute_all_logicl, .-compute_all_logicl
	.p2align 4,,15
	.type	compute_c_logicl, @function
compute_c_logicl:
	xorl	%eax, %eax
	ret
	.size	compute_c_logicl, .-compute_c_logicl
	.p2align 4,,15
	.type	compute_all_incl, @function
compute_all_incl:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	movsbl	47(%ebp),%ecx
	decl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpl	$1, %edx
	sbbl	%edx, %edx
	movzbl	parity_table(%eax), %eax
	andl	$64, %edx
	andl	$128, %ecx
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	cmpl	$-2147483648, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	4(%esp), %eax
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_incl, .-compute_all_incl
	.p2align 4,,15
	.type	compute_c_incl, @function
compute_c_incl:
	movl	40(%ebp), %eax
	ret
	.size	compute_c_incl, .-compute_c_incl
	.p2align 4,,15
	.type	compute_all_decl, @function
compute_all_decl:
	subl	$8, %esp
	movl	44(%ebp), %eax
	movl	44(%ebp), %edx
	movsbl	47(%ebp),%ecx
	incl	%eax
	movl	%eax, (%esp)
	xorl	%edx, (%esp)
	movzbl	44(%ebp), %eax
	andl	$16, (%esp)
	cmpl	$1, %edx
	sbbl	%edx, %edx
	movzbl	parity_table(%eax), %eax
	andl	$64, %edx
	andl	$128, %ecx
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	cmpl	$2147483647, 44(%ebp)
	setne	%dl
	decl	%edx
	orl	40(%ebp), %eax
	andl	$2048, %edx
	orl	(%esp), %eax
	orl	4(%esp), %eax
	addl	$8, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_decl, .-compute_all_decl
	.p2align 4,,15
	.type	compute_all_shll, @function
compute_all_shll:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movzbl	44(%ebp), %edx
	movsbl	47(%ebp),%ecx
	shrl	$31, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	sbbl	%edx, %edx
	andl	$64, %edx
	orl	4(%esp), %eax
	andl	$128, %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	xorl	40(%ebp), %edx
	orl	(%esp), %eax
	addl	$8, %esp
	sarl	$20, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_shll, .-compute_all_shll
	.p2align 4,,15
	.type	compute_c_shll, @function
compute_c_shll:
	movl	40(%ebp), %eax
	shrl	$31, %eax
	ret
	.size	compute_c_shll, .-compute_c_shll
	.p2align 4,,15
	.type	compute_c_sarl, @function
compute_c_sarl:
	movl	40(%ebp), %eax
	andl	$1, %eax
	ret
	.size	compute_c_sarl, .-compute_c_sarl
	.p2align 4,,15
	.type	compute_all_sarl, @function
compute_all_sarl:
	subl	$8, %esp
	movl	40(%ebp), %eax
	movzbl	44(%ebp), %edx
	movsbl	47(%ebp),%ecx
	andl	$1, %eax
	movzbl	parity_table(%edx), %edx
	movl	%edx, 4(%esp)
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	sbbl	%edx, %edx
	andl	$64, %edx
	orl	4(%esp), %eax
	andl	$128, %ecx
	movl	%edx, (%esp)
	movl	44(%ebp), %edx
	xorl	40(%ebp), %edx
	orl	(%esp), %eax
	addl	$8, %esp
	sarl	$20, %edx
	andl	$2048, %edx
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_sarl, .-compute_all_sarl
	.p2align 4,,15
	.type	compute_c_mull, @function
compute_c_mull:
	xorl	%eax, %eax
	cmpl	$0, 40(%ebp)
	setne	%al
	ret
	.size	compute_c_mull, .-compute_c_mull
	.p2align 4,,15
	.type	compute_all_mull, @function
compute_all_mull:
	subl	$12, %esp
	xorl	%eax, %eax
	movzbl	44(%ebp), %edx
	cmpl	$0, 40(%ebp)
	movzbl	parity_table(%edx), %edx
	setne	%al
	movl	%edx, 8(%esp)
	movl	44(%ebp), %edx
	cmpl	$1, %edx
	movsbl	47(%ebp),%edx
	sbbl	%ecx, %ecx
	andl	$64, %ecx
	andl	$128, %edx
	movl	%edx, (%esp)
	movl	%eax, %edx
	sall	$11, %edx
	orl	8(%esp), %eax
	orl	%ecx, %eax
	movl	(%esp), %ecx
	addl	$12, %esp
	orl	%ecx, %eax
	orl	%edx, %eax
	ret
	.size	compute_all_mull, .-compute_all_mull
	.p2align 4,,15
.globl op_jb_subl
	.type	op_jb_subl, @function
op_jb_subl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	jae	.L1427
#APP
	jmp __op_gen_label1
#NO_APP
.L1427:
	ret
	.size	op_jb_subl, .-op_jb_subl
	.p2align 4,,15
.globl op_jz_subl
	.type	op_jz_subl, @function
op_jz_subl:
	movl	44(%ebp), %ecx
	testl	%ecx, %ecx
	jne	.L1429
#APP
	jmp __op_gen_label1
#NO_APP
.L1429:
	ret
	.size	op_jz_subl, .-op_jz_subl
	.p2align 4,,15
.globl op_jnz_subl
	.type	op_jnz_subl, @function
op_jnz_subl:
	movl	44(%ebp), %eax
	testl	%eax, %eax
	je	.L1431
#APP
	jmp __op_gen_label1
#NO_APP
.L1431:
	ret
	.size	op_jnz_subl, .-op_jnz_subl
	.p2align 4,,15
.globl op_jbe_subl
	.type	op_jbe_subl, @function
op_jbe_subl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	ja	.L1433
#APP
	jmp __op_gen_label1
#NO_APP
.L1433:
	ret
	.size	op_jbe_subl, .-op_jbe_subl
	.p2align 4,,15
.globl op_js_subl
	.type	op_js_subl, @function
op_js_subl:
	movl	44(%ebp), %ecx
	testl	%ecx, %ecx
	jns	.L1435
#APP
	jmp __op_gen_label1
	.p2align 4,,15
#NO_APP
.L1435:
	ret
	.size	op_js_subl, .-op_js_subl
	.p2align 4,,15
.globl op_jl_subl
	.type	op_jl_subl, @function
op_jl_subl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	jge	.L1437
#APP
	jmp __op_gen_label1
#NO_APP
.L1437:
	ret
	.size	op_jl_subl, .-op_jl_subl
	.p2align 4,,15
.globl op_jle_subl
	.type	op_jle_subl, @function
op_jle_subl:
	movl	40(%ebp), %eax
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpl	40(%ebp), %eax
	jg	.L1439
#APP
	jmp __op_gen_label1
#NO_APP
.L1439:
	ret
	.size	op_jle_subl, .-op_jle_subl
	.p2align 4,,15
.globl op_loopnzl
	.type	op_loopnzl, @function
op_loopnzl:
	movl	4(%ebp), %eax
	testl	%eax, %eax
	je	.L1441
	testb	$64, %bl
	jne	.L1441
#APP
	jmp __op_gen_label1
	.p2align 4,,15
#NO_APP
.L1441:
	ret
	.size	op_loopnzl, .-op_loopnzl
	.p2align 4,,15
.globl op_loopzl
	.type	op_loopzl, @function
op_loopzl:
	movl	4(%ebp), %eax
	testl	%eax, %eax
	je	.L1443
	testb	$64, %bl
	je	.L1443
#APP
	jmp __op_gen_label1
	.p2align 4,,15
#NO_APP
.L1443:
	ret
	.size	op_loopzl, .-op_loopzl
	.p2align 4,,15
.globl op_jz_ecxl
	.type	op_jz_ecxl, @function
op_jz_ecxl:
	movl	4(%ebp), %eax
	testl	%eax, %eax
	jne	.L1445
#APP
	jmp __op_gen_label1
#NO_APP
.L1445:
	ret
	.size	op_jz_ecxl, .-op_jz_ecxl
	.p2align 4,,15
.globl op_jnz_ecxl
	.type	op_jnz_ecxl, @function
op_jnz_ecxl:
	movl	4(%ebp), %eax
	testl	%eax, %eax
	je	.L1447
#APP
	jmp __op_gen_label1
#NO_APP
.L1447:
	ret
	.size	op_jnz_ecxl, .-op_jnz_ecxl
	.p2align 4,,15
.globl op_setb_T0_subl
	.type	op_setb_T0_subl, @function
op_setb_T0_subl:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	setb	%bl
	ret
	.size	op_setb_T0_subl, .-op_setb_T0_subl
	.p2align 4,,15
.globl op_setz_T0_subl
	.type	op_setz_T0_subl, @function
op_setz_T0_subl:
	xorl	%ebx, %ebx
	cmpl	$0, 44(%ebp)
	sete	%bl
	ret
	.size	op_setz_T0_subl, .-op_setz_T0_subl
	.p2align 4,,15
.globl op_setbe_T0_subl
	.type	op_setbe_T0_subl, @function
op_setbe_T0_subl:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpl	40(%ebp), %eax
	setbe	%bl
	ret
	.size	op_setbe_T0_subl, .-op_setbe_T0_subl
	.p2align 4,,15
.globl op_sets_T0_subl
	.type	op_sets_T0_subl, @function
op_sets_T0_subl:
	movl	44(%ebp), %eax
	movl	%eax, %ebx
	shrl	$31, %ebx
	ret
	.size	op_sets_T0_subl, .-op_sets_T0_subl
	.p2align 4,,15
.globl op_setl_T0_subl
	.type	op_setl_T0_subl, @function
op_setl_T0_subl:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %edx
	addl	%edx, %eax
	cmpl	40(%ebp), %eax
	setl	%bl
	ret
	.size	op_setl_T0_subl, .-op_setl_T0_subl
	.p2align 4,,15
.globl op_setle_T0_subl
	.type	op_setle_T0_subl, @function
op_setle_T0_subl:
	movl	40(%ebp), %eax
	xorl	%ebx, %ebx
	movl	44(%ebp), %ecx
	addl	%ecx, %eax
	cmpl	40(%ebp), %eax
	setle	%bl
	ret
	.size	op_setle_T0_subl, .-op_setle_T0_subl
	.p2align 4,,15
.globl op_shll_T0_T1
	.type	op_shll_T0_T1, @function
op_shll_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	sall	%cl, %ebx
	ret
	.size	op_shll_T0_T1, .-op_shll_T0_T1
	.p2align 4,,15
.globl op_shrl_T0_T1
	.type	op_shrl_T0_T1, @function
op_shrl_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	shrl	%cl, %ebx
	ret
	.size	op_shrl_T0_T1, .-op_shrl_T0_T1
	.p2align 4,,15
.globl op_sarl_T0_T1
	.type	op_sarl_T0_T1, @function
op_sarl_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	sarl	%cl, %ebx
	ret
	.size	op_sarl_T0_T1, .-op_sarl_T0_T1
	.p2align 4,,15
.globl op_roll_T0_T1_cc
	.type	op_roll_T0_T1_cc, @function
op_roll_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L1461
	movl	%ebx, (%esp)
	movl	%esi, %ecx
	andl	$31, %ecx
	roll	%cl, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$20, (%esp)
	andl	$2048, (%esp)
	orl	%eax, (%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1461:
	popl	%eax
	ret
	.size	op_roll_T0_T1_cc, .-op_roll_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_T0_T1_cc
	.type	op_rorl_T0_T1_cc, @function
op_rorl_T0_T1_cc:
	subl	$4, %esp
	testl	$31, %esi
	je	.L1466
	movl	%ebx, (%esp)
	movl	%esi, %ecx
	andl	$31, %ecx
	rorl	%cl, %ebx
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, (%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$20, (%esp)
	andl	$2048, (%esp)
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%ebx, %edx
	shrl	$31, %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1466:
	popl	%ecx
	ret
	.size	op_rorl_T0_T1_cc, .-op_rorl_T0_T1_cc
	.p2align 4,,15
.globl op_roll_T0_T1
	.type	op_roll_T0_T1, @function
op_roll_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	je	.L1471
	roll	%cl, %ebx
.L1471:
	ret
	.size	op_roll_T0_T1, .-op_roll_T0_T1
	.p2align 4,,15
.globl op_rorl_T0_T1
	.type	op_rorl_T0_T1, @function
op_rorl_T0_T1:
	movl	%esi, %ecx
	andl	$31, %ecx
	je	.L1473
	rorl	%cl, %ebx
.L1473:
	ret
	.size	op_rorl_T0_T1, .-op_rorl_T0_T1
	.p2align 4,,15
.globl op_rcll_T0_T1_cc
	.type	op_rcll_T0_T1_cc, @function
op_rcll_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1475
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%eax, (%esp)
	movl	8(%esp), %ecx
	movl	4(%esp), %eax
	decl	%ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L1476
	movl	8(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, (%esp)
.L1476:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	$32, %ecx
	xorl	%ebx, %eax
	sarl	$20, %eax
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	movl	8(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, %edx
	movl	4(%esp), %ecx
	andl	$1, %edx
	orl	%ecx, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L1475:
	addl	$12, %esp
	ret
	.size	op_rcll_T0_T1_cc, .-op_rcll_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_T0_T1_cc
	.type	op_rcrl_T0_T1_cc, @function
op_rcrl_T0_T1_cc:
	movl	%esi, %eax
	subl	$12, %esp
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1481
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	8(%esp), %ecx
	movl	%ebx, %edx
	movl	%eax, 4(%esp)
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%eax, (%esp)
	movl	$32, %ecx
	movl	4(%esp), %eax
	subl	8(%esp), %ecx
	andl	$1, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
	cmpl	$1, 8(%esp)
	jle	.L1482
	movl	8(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, (%esp)
.L1482:
	andl	$-2050, 4(%esp)
	movl	%edx, %eax
	movl	(%esp), %ebx
	movl	$1, 48(%ebp)
	movl	8(%esp), %ecx
	xorl	%ebx, %eax
	sarl	$20, %eax
	decl	%ecx
	andl	$2048, %eax
	orl	%eax, 4(%esp)
	shrl	%cl, %edx
	andl	$1, %edx
	movl	4(%esp), %eax
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	.p2align 4,,15
.L1481:
	addl	$12, %esp
	ret
	.size	op_rcrl_T0_T1_cc, .-op_rcrl_T0_T1_cc
	.p2align 4,,15
.globl op_shll_T0_T1_cc
	.type	op_shll_T0_T1_cc, @function
op_shll_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L1487
	movl	$36, 48(%ebp)
	leal	-1(%edx), %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movb	%dl, %cl
	sall	%cl, %ebx
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L1487:
	ret
	.size	op_shll_T0_T1_cc, .-op_shll_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_T0_T1_cc
	.type	op_shrl_T0_T1_cc, @function
op_shrl_T0_T1_cc:
	movl	%esi, %edx
	andl	$31, %edx
	je	.L1489
	movl	$40, 48(%ebp)
	leal	-1(%edx), %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movb	%dl, %cl
	shrl	%cl, %ebx
	movl	%eax, 40(%ebp)
	movl	%ebx, 44(%ebp)
.L1489:
	ret
	.size	op_shrl_T0_T1_cc, .-op_shrl_T0_T1_cc
	.p2align 4,,15
.globl op_sarl_T0_T1_cc
	.type	op_sarl_T0_T1_cc, @function
op_sarl_T0_T1_cc:
	movl	%esi, %ecx
	andl	$31, %ecx
	je	.L1491
	movl	$40, 48(%ebp)
	movl	%ebx, %eax
	sarl	%cl, %ebx
	movl	%ebx, 44(%ebp)
	decl	%ecx
	sarl	%cl, %eax
	movl	%eax, 40(%ebp)
.L1491:
	ret
	.size	op_sarl_T0_T1_cc, .-op_sarl_T0_T1_cc
	.p2align 4,,15
.globl op_shldl_T0_T1_im_cc
	.type	op_shldl_T0_T1_im_cc, @function
op_shldl_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%eax, 40(%ebp)
	movl	$__op_param1, %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%esi, %eax
	movl	%edx, %ebx
	shrl	%cl, %eax
	orl	%eax, %ebx
	movl	%ebx, 44(%ebp)
	ret
	.size	op_shldl_T0_T1_im_cc, .-op_shldl_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_T0_T1_ECX_cc
	.type	op_shldl_T0_T1_ECX_cc, @function
op_shldl_T0_T1_ECX_cc:
	subl	$8, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 4(%esp)
	je	.L1494
	movl	$36, 48(%ebp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	sall	%cl, %edx
	movzbl	4(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 40(%ebp)
	movl	4(%esp), %edx
	sall	%cl, %eax
	movl	%eax, (%esp)
	movl	$32, %eax
	movl	(%esp), %ebx
	subl	%edx, %eax
	movb	%al, %cl
	movl	%esi, %edx
	shrl	%cl, %edx
	orl	%edx, %ebx
	movl	%ebx, 44(%ebp)
.L1494:
	addl	$8, %esp
	ret
	.size	op_shldl_T0_T1_ECX_cc, .-op_shldl_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_T0_T1_im_cc
	.type	op_shrdl_T0_T1_im_cc, @function
op_shrdl_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%eax, 40(%ebp)
	movl	$__op_param1, %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%esi, %eax
	movl	%edx, %ebx
	sall	%cl, %eax
	orl	%eax, %ebx
	movl	%ebx, 44(%ebp)
	ret
	.size	op_shrdl_T0_T1_im_cc, .-op_shrdl_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdl_T0_T1_ECX_cc
	.type	op_shrdl_T0_T1_ECX_cc, @function
op_shrdl_T0_T1_ECX_cc:
	subl	$8, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 4(%esp)
	je	.L1497
	movl	$40, 48(%ebp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	shrl	%cl, %edx
	movzbl	4(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 40(%ebp)
	movl	%esi, %edx
	shrl	%cl, %eax
	movl	%eax, (%esp)
	movl	4(%esp), %ecx
	movl	$32, %eax
	movl	(%esp), %ebx
	subl	%ecx, %eax
	movb	%al, %cl
	sall	%cl, %edx
	orl	%edx, %ebx
	movl	%ebx, 44(%ebp)
.L1497:
	addl	$8, %esp
	ret
	.size	op_shrdl_T0_T1_ECX_cc, .-op_shrdl_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcl_T0_T1_cc
	.type	op_adcl_T0_T1_cc, @function
op_adcl_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	leal	(%ebx,%esi), %edx
	leal	(%edx,%eax), %ebx
	movl	%ebx, 44(%ebp)
	leal	8(,%eax,4), %eax
	movl	%eax, 48(%ebp)
	ret
	.size	op_adcl_T0_T1_cc, .-op_adcl_T0_T1_cc
	.p2align 4,,15
.globl op_sbbl_T0_T1_cc
	.type	op_sbbl_T0_T1_cc, @function
op_sbbl_T0_T1_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%esi, 40(%ebp)
	movl	%ebx, %edx
	subl	%esi, %edx
	movl	%edx, %ebx
	subl	%eax, %ebx
	leal	16(,%eax,4), %eax
	movl	%ebx, 44(%ebp)
	movl	%eax, 48(%ebp)
	ret
	.size	op_sbbl_T0_T1_cc, .-op_sbbl_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgl_T0_T1_EAX_cc
	.type	op_cmpxchgl_T0_T1_EAX_cc, @function
op_cmpxchgl_T0_T1_EAX_cc:
	movl	(%ebp), %eax
	movl	%ebx, %edx
	subl	%ebx, %eax
	jne	.L1501
	movl	%esi, %ebx
	jmp	.L1502
	.p2align 4,,7
.L1501:
	movl	%ebx, (%ebp)
.L1502:
	movl	%edx, 40(%ebp)
	movl	%eax, 44(%ebp)
	ret
	.size	op_cmpxchgl_T0_T1_EAX_cc, .-op_cmpxchgl_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_roll_raw_T0_T1_cc
	.type	op_roll_raw_T0_T1_cc, @function
op_roll_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L1504
	movl	%ebx, 8(%esp)
	movl	%esi, %ecx
	andl	$31, %ecx
	movl	%edi, (%esp)
	roll	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$20, 8(%esp)
	andl	$2048, 8(%esp)
	orl	%eax, 8(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	8(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1504:
	addl	$12, %esp
	ret
	.size	op_roll_raw_T0_T1_cc, .-op_roll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_raw_T0_T1_cc
	.type	op_rorl_raw_T0_T1_cc, @function
op_rorl_raw_T0_T1_cc:
	subl	$12, %esp
	testl	$31, %esi
	je	.L1510
	movl	%ebx, 8(%esp)
	movl	%esi, %ecx
	andl	$31, %ecx
	movl	%edi, (%esp)
	rorl	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 8(%esp)
	andl	$-2050, %eax
	movl	%ebx, %edx
	movl	$1, 48(%ebp)
	shrl	$31, %edx
	sarl	$20, 8(%esp)
	andl	$2048, 8(%esp)
	movl	8(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1510:
	addl	$12, %esp
	ret
	.size	op_rorl_raw_T0_T1_cc, .-op_rorl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_roll_raw_T0_T1
	.type	op_roll_raw_T0_T1, @function
op_roll_raw_T0_T1:
	movl	%esi, %ecx
	subl	$8, %esp
	andl	$31, %ecx
	je	.L1516
	movl	%edi, (%esp)
	roll	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	.p2align 4,,15
.L1516:
	addl	$8, %esp
	ret
	.size	op_roll_raw_T0_T1, .-op_roll_raw_T0_T1
	.p2align 4,,15
.globl op_rorl_raw_T0_T1
	.type	op_rorl_raw_T0_T1, @function
op_rorl_raw_T0_T1:
	movl	%esi, %ecx
	subl	$8, %esp
	andl	$31, %ecx
	je	.L1519
	movl	%edi, (%esp)
	rorl	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	.p2align 4,,15
.L1519:
	addl	$8, %esp
	ret
	.size	op_rorl_raw_T0_T1, .-op_rorl_raw_T0_T1
	.p2align 4,,15
.globl op_rcll_raw_T0_T1_cc
	.type	op_rcll_raw_T0_T1_cc, @function
op_rcll_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movl	%eax, 16(%esp)
	je	.L1522
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	%eax, 12(%esp)
	movl	16(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 8(%esp)
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L1523
	movl	16(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1523:
	movl	%edx, 4(%esp)
	movl	%edx, %ebx
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	andl	$-2050, 12(%esp)
	movl	$32, %ecx
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$20, %eax
	andl	$2048, %eax
	orl	%eax, 12(%esp)
	movl	16(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 8(%esp)
	movl	12(%esp), %eax
	andl	$1, 8(%esp)
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1522:
	addl	$20, %esp
	ret
	.size	op_rcll_raw_T0_T1_cc, .-op_rcll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_raw_T0_T1_cc
	.type	op_rcrl_raw_T0_T1_cc, @function
op_rcrl_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$20, %esp
	andl	$31, %eax
	movl	%eax, 16(%esp)
	je	.L1529
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	16(%esp), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	%eax, 12(%esp)
	movl	$32, %ecx
	andl	$1, %eax
	subl	16(%esp), %ecx
	movl	%ebx, 8(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 16(%esp)
	jle	.L1530
	movl	16(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1530:
	movl	%edx, 4(%esp)
	movl	%edx, %ebx
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	andl	$-2050, 12(%esp)
	movl	8(%esp), %eax
	movl	$1, 48(%ebp)
	movl	16(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, 8(%esp)
	orl	%eax, 12(%esp)
	andl	$1, 8(%esp)
	movl	12(%esp), %eax
	orl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1529:
	addl	$20, %esp
	ret
	.size	op_rcrl_raw_T0_T1_cc, .-op_rcrl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shll_raw_T0_T1_cc
	.type	op_shll_raw_T0_T1_cc, @function
op_shll_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1536
	movl	%edi, (%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	sall	%cl, %edx
	movzbl	8(%esp), %ecx
	movl	%edx, 12(%esp)
	sall	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$36, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1536:
	addl	$16, %esp
	ret
	.size	op_shll_raw_T0_T1_cc, .-op_shll_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_raw_T0_T1_cc
	.type	op_shrl_raw_T0_T1_cc, @function
op_shrl_raw_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	movl	%eax, 8(%esp)
	je	.L1539
	movl	%edi, (%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	shrl	%cl, %edx
	movzbl	8(%esp), %ecx
	movl	%edx, 12(%esp)
	shrl	%cl, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$40, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1539:
	addl	$16, %esp
	ret
	.size	op_shrl_raw_T0_T1_cc, .-op_shrl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sarl_raw_T0_T1_cc
	.type	op_sarl_raw_T0_T1_cc, @function
op_sarl_raw_T0_T1_cc:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1542
	movl	%ebx, 8(%esp)
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	$40, 48(%ebp)
	movl	%eax, 40(%ebp)
	.p2align 4,,15
.L1542:
	addl	$12, %esp
	ret
	.size	op_sarl_raw_T0_T1_cc, .-op_sarl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_shldl_raw_T0_T1_im_cc
	.type	op_shldl_raw_T0_T1_im_cc, @function
op_shldl_raw_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	subl	$12, %esp
	movl	%eax, 8(%esp)
	sall	%cl, %edx
	movl	$32, %ecx
	movl	%edi, (%esp)
	subl	$__op_param1, %ecx
	movl	%esi, %eax
	shrl	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_shldl_raw_T0_T1_im_cc, .-op_shldl_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_raw_T0_T1_ECX_cc
	.type	op_shldl_raw_T0_T1_ECX_cc, @function
op_shldl_raw_T0_T1_ECX_cc:
	subl	$20, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1547
	movl	%edi, (%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	sall	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 16(%esp)
	movl	12(%esp), %edx
	sall	%cl, %eax
	movl	%eax, 8(%esp)
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%edx, %eax
	movb	%al, %cl
	movl	%esi, %edx
	shrl	%cl, %edx
	orl	%edx, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	16(%esp), %ecx
	movl	$36, 48(%ebp)
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1547:
	addl	$20, %esp
	ret
	.size	op_shldl_raw_T0_T1_ECX_cc, .-op_shldl_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_raw_T0_T1_im_cc
	.type	op_shrdl_raw_T0_T1_im_cc, @function
op_shrdl_raw_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	subl	$12, %esp
	movl	%eax, 8(%esp)
	shrl	%cl, %edx
	movl	$32, %ecx
	movl	%edi, (%esp)
	subl	$__op_param1, %ecx
	movl	%esi, %eax
	sall	%cl, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_shrdl_raw_T0_T1_im_cc, .-op_shrdl_raw_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdl_raw_T0_T1_ECX_cc
	.type	op_shrdl_raw_T0_T1_ECX_cc, @function
op_shrdl_raw_T0_T1_ECX_cc:
	subl	$20, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1552
	movl	%edi, (%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	shrl	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 16(%esp)
	movl	%esi, %edx
	shrl	%cl, %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %ecx
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%ecx, %eax
	movb	%al, %cl
	sall	%cl, %edx
	orl	%edx, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%ebx, 44(%ebp)
	movl	16(%esp), %ecx
	movl	$40, 48(%ebp)
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1552:
	addl	$20, %esp
	ret
	.size	op_shrdl_raw_T0_T1_ECX_cc, .-op_shrdl_raw_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcl_raw_T0_T1_cc
	.type	op_adcl_raw_T0_T1_cc, @function
op_adcl_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	8(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, (%esp)
	leal	(%eax,%edx), %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	8(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_adcl_raw_T0_T1_cc, .-op_adcl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_sbbl_raw_T0_T1_cc
	.type	op_sbbl_raw_T0_T1_cc, @function
op_sbbl_raw_T0_T1_cc:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 8(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, (%esp)
	movl	%eax, %ebx
	movl	8(%esp), %eax
	subl	%eax, %ebx
	movl	%ebx, 4(%esp)
	call	remR3PhysWriteU32
	movl	%esi, 40(%ebp)
	movl	8(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	16(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$12, %esp
	ret
	.size	op_sbbl_raw_T0_T1_cc, .-op_sbbl_raw_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgl_raw_T0_T1_EAX_cc
	.type	op_cmpxchgl_raw_T0_T1_EAX_cc, @function
op_cmpxchgl_raw_T0_T1_EAX_cc:
	subl	$16, %esp
	movl	(%ebp), %eax
	movl	%ebx, 12(%esp)
	subl	%ebx, %eax
	movl	%eax, 8(%esp)
	jne	.L1559
	movl	%esi, 4(%esp)
	movl	%esi, %ebx
	movl	%edi, (%esp)
	call	remR3PhysWriteU32
	jmp	.L1561
	.p2align 4,,7
.L1559:
	movl	%ebx, (%ebp)
.L1561:
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	8(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$16, %esp
	ret
	.size	op_cmpxchgl_raw_T0_T1_EAX_cc, .-op_cmpxchgl_raw_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_roll_kernel_T0_T1_cc
	.type	op_roll_kernel_T0_T1_cc, @function
op_roll_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1563
	movl	%ebx, 12(%esp)
	movl	%esi, %ecx
	movl	%edi, %eax
	movl	%edi, 8(%esp)
	andl	$31, %ecx
	andl	$-4093, %eax
	roll	%cl, %ebx
	movl	%edi, %ecx
	movl	%ebx, %edx
	shrl	$8, %ecx
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1564
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1567
	.p2align 4,,7
.L1564:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1567:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$20, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1563:
	addl	$16, %esp
	ret
	.size	op_roll_kernel_T0_T1_cc, .-op_roll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_kernel_T0_T1_cc
	.type	op_rorl_kernel_T0_T1_cc, @function
op_rorl_kernel_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1572
	movl	%ebx, 12(%esp)
	movl	%esi, %ecx
	movl	%edi, %eax
	movl	%edi, 8(%esp)
	andl	$31, %ecx
	andl	$-4093, %eax
	rorl	%cl, %ebx
	movl	%edi, %ecx
	movl	%ebx, %edx
	shrl	$8, %ecx
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1573
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1576
	.p2align 4,,7
.L1573:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1576:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	%ebx, %edx
	movl	$1, 48(%ebp)
	shrl	$31, %edx
	sarl	$20, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1572:
	addl	$16, %esp
	ret
	.size	op_rorl_kernel_T0_T1_cc, .-op_rorl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_roll_kernel_T0_T1
	.type	op_roll_kernel_T0_T1, @function
op_roll_kernel_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1581
	movl	%edi, 8(%esp)
	roll	%cl, %ebx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1582
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1581
	.p2align 4,,7
.L1582:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1581:
	addl	$12, %esp
	ret
	.size	op_roll_kernel_T0_T1, .-op_roll_kernel_T0_T1
	.p2align 4,,15
.globl op_rorl_kernel_T0_T1
	.type	op_rorl_kernel_T0_T1, @function
op_rorl_kernel_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1587
	movl	%edi, 8(%esp)
	rorl	%cl, %ebx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1588
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1587
	.p2align 4,,7
.L1588:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1587:
	addl	$12, %esp
	ret
	.size	op_rorl_kernel_T0_T1, .-op_rorl_kernel_T0_T1
	.p2align 4,,15
.globl op_rcll_kernel_T0_T1_cc
	.type	op_rcll_kernel_T0_T1_cc, @function
op_rcll_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movl	%eax, 20(%esp)
	je	.L1593
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	%eax, 16(%esp)
	movl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1594
	movl	20(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1594:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4093, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1595
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1598
	.p2align 4,,7
.L1595:
	movl	%edx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU32
.L1598:
	andl	$-2050, 16(%esp)
	movl	$32, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$20, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1593:
	addl	$24, %esp
	ret
	.size	op_rcll_kernel_T0_T1_cc, .-op_rcll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_kernel_T0_T1_cc
	.type	op_rcrl_kernel_T0_T1_cc, @function
op_rcrl_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movl	%eax, 20(%esp)
	je	.L1603
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	%eax, 16(%esp)
	movl	$32, %ecx
	andl	$1, %eax
	subl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1604
	movl	20(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1604:
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4093, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%edx, %ebx
	je	.L1605
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1608
	.p2align 4,,7
.L1605:
	movl	%edx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU32
.L1608:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1603:
	addl	$24, %esp
	ret
	.size	op_rcrl_kernel_T0_T1_cc, .-op_rcrl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shll_kernel_T0_T1_cc
	.type	op_shll_kernel_T0_T1_cc, @function
op_shll_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1613
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movb	%al, %cl
	sall	%cl, %ebx
	movl	%edx, 12(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4093, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1614
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1617
	.p2align 4,,7
.L1614:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1617:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$36, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1613:
	addl	$16, %esp
	ret
	.size	op_shll_kernel_T0_T1_cc, .-op_shll_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_kernel_T0_T1_cc
	.type	op_shrl_kernel_T0_T1_cc, @function
op_shrl_kernel_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1619
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	shrl	%cl, %ebx
	movl	%edx, 12(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4093, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1620
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1623
	.p2align 4,,7
.L1620:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1623:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$40, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1619:
	addl	$16, %esp
	ret
	.size	op_shrl_kernel_T0_T1_cc, .-op_shrl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sarl_kernel_T0_T1_cc
	.type	op_sarl_kernel_T0_T1_cc, @function
op_sarl_kernel_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L1625
	movl	%ebx, 12(%esp)
	sarl	%cl, %ebx
	decl	%ecx
	sarl	%cl, 12(%esp)
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1626
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1629
	.p2align 4,,7
.L1626:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1629:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$40, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1625:
	addl	$16, %esp
	ret
	.size	op_sarl_kernel_T0_T1_cc, .-op_sarl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_shldl_kernel_T0_T1_im_cc
	.type	op_shldl_kernel_T0_T1_im_cc, @function
op_shldl_kernel_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	subl	$16, %esp
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%eax, 12(%esp)
	movl	%esi, %eax
	shrl	%cl, %eax
	movl	%edi, 8(%esp)
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1631
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1634
	.p2align 4,,7
.L1631:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1634:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shldl_kernel_T0_T1_im_cc, .-op_shldl_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_kernel_T0_T1_ECX_cc
	.type	op_shldl_kernel_T0_T1_ECX_cc, @function
op_shldl_kernel_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1636
	movl	%edi, 16(%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	sall	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 20(%esp)
	movl	12(%esp), %edx
	sall	%cl, %eax
	movl	%eax, 8(%esp)
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%edx, %eax
	movb	%al, %cl
	movl	%esi, %edx
	shrl	%cl, %edx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	orl	%edx, %ebx
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1637
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1640
	.p2align 4,,7
.L1637:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1640:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %ecx
	movl	$36, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1636:
	addl	$24, %esp
	ret
	.size	op_shldl_kernel_T0_T1_ECX_cc, .-op_shldl_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_kernel_T0_T1_im_cc
	.type	op_shrdl_kernel_T0_T1_im_cc, @function
op_shrdl_kernel_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	shrl	%cl, %edx
	subl	$16, %esp
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%eax, 12(%esp)
	movl	%esi, %eax
	sall	%cl, %eax
	movl	%edi, 8(%esp)
	movl	%edx, %ebx
	movl	%edi, %ecx
	orl	%eax, %ebx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1642
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1645
	.p2align 4,,7
.L1642:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1645:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shrdl_kernel_T0_T1_im_cc, .-op_shrdl_kernel_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdl_kernel_T0_T1_ECX_cc
	.type	op_shrdl_kernel_T0_T1_ECX_cc, @function
op_shrdl_kernel_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1647
	movl	%edi, 16(%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	shrl	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 20(%esp)
	movl	%esi, %edx
	shrl	%cl, %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %ecx
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%ecx, %eax
	movb	%al, %cl
	sall	%cl, %edx
	movl	%edi, %ecx
	shrl	$8, %ecx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	orl	%edx, %ebx
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1648
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1651
	.p2align 4,,7
.L1648:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1651:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %ecx
	movl	$40, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1647:
	addl	$24, %esp
	ret
	.size	op_shrdl_kernel_T0_T1_ECX_cc, .-op_shrdl_kernel_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcl_kernel_T0_T1_cc
	.type	op_adcl_kernel_T0_T1_cc, @function
op_adcl_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%edi, %ecx
	movl	12(%esp), %edx
	movl	%edi, 8(%esp)
	leal	(%ebx,%esi), %eax
	shrl	$8, %ecx
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1653
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1656
	.p2align 4,,7
.L1653:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1656:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	8(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcl_kernel_T0_T1_cc, .-op_adcl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_sbbl_kernel_T0_T1_cc
	.type	op_sbbl_kernel_T0_T1_cc, @function
op_sbbl_kernel_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	movl	%edi, %ecx
	shrl	$8, %ecx
	subl	%eax, %ebx
	movl	%edi, %eax
	andl	$4080, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1658
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1661
	.p2align 4,,7
.L1658:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1661:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	16(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbl_kernel_T0_T1_cc, .-op_sbbl_kernel_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgl_kernel_T0_T1_EAX_cc
	.type	op_cmpxchgl_kernel_T0_T1_EAX_cc, @function
op_cmpxchgl_kernel_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	movl	%eax, 12(%esp)
	jne	.L1663
	movl	%edi, 8(%esp)
	movl	%edi, %ecx
	movl	%edi, %eax
	shrl	$8, %ecx
	andl	$-4093, %eax
	andl	$4080, %ecx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%esi, %ebx
	movl	%esi, %edx
	je	.L1664
	movl	$0, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1668
	.p2align 4,,7
.L1664:
	movl	%esi, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
	jmp	.L1668
	.p2align 4,,7
.L1663:
	movl	%ebx, (%ebp)
.L1668:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgl_kernel_T0_T1_EAX_cc, .-op_cmpxchgl_kernel_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_roll_user_T0_T1_cc
	.type	op_roll_user_T0_T1_cc, @function
op_roll_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1670
	movl	%ebx, 12(%esp)
	movl	%edi, %eax
	movl	%esi, %ecx
	movl	%edi, 8(%esp)
	shrl	$12, %eax
	andl	$31, %ecx
	andl	$255, %eax
	roll	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1671
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1674
	.p2align 4,,7
.L1671:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1674:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	$1, 48(%ebp)
	sarl	$20, 12(%esp)
	andl	$2048, 12(%esp)
	orl	%eax, 12(%esp)
	movl	%ebx, %eax
	andl	$1, %eax
	movl	12(%esp), %edx
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1670:
	addl	$16, %esp
	ret
	.size	op_roll_user_T0_T1_cc, .-op_roll_user_T0_T1_cc
	.p2align 4,,15
.globl op_rorl_user_T0_T1_cc
	.type	op_rorl_user_T0_T1_cc, @function
op_rorl_user_T0_T1_cc:
	subl	$16, %esp
	testl	$31, %esi
	je	.L1679
	movl	%ebx, 12(%esp)
	movl	%edi, %eax
	movl	%esi, %ecx
	movl	%edi, 8(%esp)
	shrl	$12, %eax
	andl	$31, %ecx
	andl	$255, %eax
	rorl	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1680
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1683
	.p2align 4,,7
.L1680:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1683:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, 12(%esp)
	andl	$-2050, %eax
	movl	%ebx, %edx
	movl	$1, 48(%ebp)
	shrl	$31, %edx
	sarl	$20, 12(%esp)
	andl	$2048, 12(%esp)
	movl	12(%esp), %ecx
	orl	%ecx, %eax
	orl	%edx, %eax
	movl	%eax, 40(%ebp)
.L1679:
	addl	$16, %esp
	ret
	.size	op_rorl_user_T0_T1_cc, .-op_rorl_user_T0_T1_cc
	.p2align 4,,15
.globl op_roll_user_T0_T1
	.type	op_roll_user_T0_T1, @function
op_roll_user_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1688
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	roll	%cl, %ebx
	shrl	$12, %eax
	movl	%ebx, %edx
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1689
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1688
	.p2align 4,,7
.L1689:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1688:
	addl	$12, %esp
	ret
	.size	op_roll_user_T0_T1, .-op_roll_user_T0_T1
	.p2align 4,,15
.globl op_rorl_user_T0_T1
	.type	op_rorl_user_T0_T1, @function
op_rorl_user_T0_T1:
	movl	%esi, %ecx
	subl	$12, %esp
	andl	$31, %ecx
	je	.L1694
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	rorl	%cl, %ebx
	shrl	$12, %eax
	movl	%ebx, %edx
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1695
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1694
	.p2align 4,,7
.L1695:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1694:
	addl	$12, %esp
	ret
	.size	op_rorl_user_T0_T1, .-op_rorl_user_T0_T1
	.p2align 4,,15
.globl op_rcll_user_T0_T1_cc
	.type	op_rcll_user_T0_T1_cc, @function
op_rcll_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movl	%eax, 20(%esp)
	je	.L1700
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movl	%eax, 16(%esp)
	movl	20(%esp), %ecx
	andl	$1, %eax
	movl	%ebx, 12(%esp)
	decl	%ecx
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1701
	movl	20(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	orl	%eax, %edx
.L1701:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1702
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1705
	.p2align 4,,7
.L1702:
	movl	%edx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU32
.L1705:
	andl	$-2050, 16(%esp)
	movl	$32, %ecx
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	xorl	%ebx, %eax
	sarl	$20, %eax
	andl	$2048, %eax
	orl	%eax, 16(%esp)
	movl	20(%esp), %eax
	subl	%eax, %ecx
	shrl	%cl, 12(%esp)
	movl	16(%esp), %eax
	andl	$1, 12(%esp)
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1700:
	addl	$24, %esp
	ret
	.size	op_rcll_user_T0_T1_cc, .-op_rcll_user_T0_T1_cc
	.p2align 4,,15
.globl op_rcrl_user_T0_T1_cc
	.type	op_rcrl_user_T0_T1_cc, @function
op_rcrl_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$24, %esp
	andl	$31, %eax
	movl	%eax, 20(%esp)
	je	.L1710
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movzbl	20(%esp), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movl	%eax, 16(%esp)
	movl	$32, %ecx
	andl	$1, %eax
	subl	20(%esp), %ecx
	movl	%ebx, 12(%esp)
	sall	%cl, %eax
	orl	%eax, %edx
	cmpl	$1, 20(%esp)
	jle	.L1711
	movl	20(%esp), %eax
	movl	$33, %ecx
	subl	%eax, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	orl	%eax, %edx
.L1711:
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%edx, %ebx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1712
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1715
	.p2align 4,,7
.L1712:
	movl	%edx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %ecx
	movl	%ecx, (%esp)
	call	remR3PhysWriteU32
.L1715:
	andl	$-2050, 16(%esp)
	movl	12(%esp), %eax
	movl	$1, 48(%ebp)
	movl	20(%esp), %ecx
	xorl	%ebx, %eax
	decl	%ecx
	sarl	$20, %eax
	andl	$2048, %eax
	shrl	%cl, 12(%esp)
	orl	%eax, 16(%esp)
	andl	$1, 12(%esp)
	movl	16(%esp), %eax
	orl	%eax, 12(%esp)
	movl	12(%esp), %ecx
	movl	%ecx, 40(%ebp)
	.p2align 4,,15
.L1710:
	addl	$24, %esp
	ret
	.size	op_rcrl_user_T0_T1_cc, .-op_rcrl_user_T0_T1_cc
	.p2align 4,,15
.globl op_shll_user_T0_T1_cc
	.type	op_shll_user_T0_T1_cc, @function
op_shll_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1720
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	sall	%cl, %edx
	movb	%al, %cl
	movl	%edi, %eax
	movl	%edx, 12(%esp)
	shrl	$12, %eax
	andl	$255, %eax
	sall	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1721
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1724
	.p2align 4,,7
.L1721:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1724:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$36, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1720:
	addl	$16, %esp
	ret
	.size	op_shll_user_T0_T1_cc, .-op_shll_user_T0_T1_cc
	.p2align 4,,15
.globl op_shrl_user_T0_T1_cc
	.type	op_shrl_user_T0_T1_cc, @function
op_shrl_user_T0_T1_cc:
	movl	%esi, %eax
	subl	$16, %esp
	andl	$31, %eax
	je	.L1726
	movl	%edi, 8(%esp)
	leal	-1(%eax), %ecx
	movl	%ebx, %edx
	shrl	%cl, %edx
	movb	%al, %cl
	movl	%edi, %eax
	movl	%edx, 12(%esp)
	shrl	$12, %eax
	andl	$255, %eax
	shrl	%cl, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	movl	%ebx, %edx
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1727
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1730
	.p2align 4,,7
.L1727:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1730:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %ecx
	movl	$40, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1726:
	addl	$16, %esp
	ret
	.size	op_shrl_user_T0_T1_cc, .-op_shrl_user_T0_T1_cc
	.p2align 4,,15
.globl op_sarl_user_T0_T1_cc
	.type	op_sarl_user_T0_T1_cc, @function
op_sarl_user_T0_T1_cc:
	movl	%esi, %ecx
	subl	$16, %esp
	andl	$31, %ecx
	je	.L1732
	movl	%ebx, 12(%esp)
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%edi, 8(%esp)
	sarl	%cl, %ebx
	andl	$255, %eax
	decl	%ecx
	movl	%ebx, %edx
	sarl	%cl, 12(%esp)
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1733
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1736
	.p2align 4,,7
.L1733:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1736:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	$40, 48(%ebp)
	movl	%eax, 40(%ebp)
.L1732:
	addl	$16, %esp
	ret
	.size	op_sarl_user_T0_T1_cc, .-op_sarl_user_T0_T1_cc
	.p2align 4,,15
.globl op_shldl_user_T0_T1_im_cc
	.type	op_shldl_user_T0_T1_im_cc, @function
op_shldl_user_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	sall	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	sall	%cl, %edx
	subl	$16, %esp
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%eax, 12(%esp)
	movl	%esi, %eax
	shrl	%cl, %eax
	movl	%edi, 8(%esp)
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	movl	%ebx, %edx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1738
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1741
	.p2align 4,,7
.L1738:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1741:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shldl_user_T0_T1_im_cc, .-op_shldl_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_shldl_user_T0_T1_ECX_cc
	.type	op_shldl_user_T0_T1_ECX_cc, @function
op_shldl_user_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1743
	movl	%edi, 16(%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	sall	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 20(%esp)
	movl	12(%esp), %edx
	sall	%cl, %eax
	movl	%eax, 8(%esp)
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%edx, %eax
	movb	%al, %cl
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%esi, %edx
	andl	$255, %eax
	shrl	%cl, %edx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	orl	%edx, %ebx
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1744
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1747
	.p2align 4,,7
.L1744:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1747:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %ecx
	movl	$36, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1743:
	addl	$24, %esp
	ret
	.size	op_shldl_user_T0_T1_ECX_cc, .-op_shldl_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_shrdl_user_T0_T1_im_cc
	.type	op_shrdl_user_T0_T1_im_cc, @function
op_shrdl_user_T0_T1_im_cc:
	movl	$__op_param1-1, %ecx
	movl	%ebx, %eax
	shrl	%cl, %eax
	movl	%ebx, %edx
	movl	$__op_param1, %ecx
	shrl	%cl, %edx
	subl	$16, %esp
	movl	$32, %ecx
	subl	$__op_param1, %ecx
	movl	%eax, 12(%esp)
	movl	%esi, %eax
	sall	%cl, %eax
	movl	%edi, 8(%esp)
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	%edi, %eax
	movl	%ebx, %edx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1749
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1752
	.p2align 4,,7
.L1749:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
.L1752:
	movl	%ebx, 44(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_shrdl_user_T0_T1_im_cc, .-op_shrdl_user_T0_T1_im_cc
	.p2align 4,,15
.globl op_shrdl_user_T0_T1_ECX_cc
	.type	op_shrdl_user_T0_T1_ECX_cc, @function
op_shrdl_user_T0_T1_ECX_cc:
	subl	$24, %esp
	movl	4(%ebp), %eax
	andl	$31, %eax
	movl	%eax, 12(%esp)
	je	.L1754
	movl	%edi, 16(%esp)
	decl	%eax
	movl	%ebx, %edx
	movb	%al, %cl
	shrl	%cl, %edx
	movzbl	12(%esp), %ecx
	movl	%ebx, %eax
	movl	%edx, 20(%esp)
	movl	%esi, %edx
	shrl	%cl, %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %ecx
	movl	$32, %eax
	movl	8(%esp), %ebx
	subl	%ecx, %eax
	movb	%al, %cl
	movl	%edi, %eax
	sall	%cl, %edx
	shrl	$12, %eax
	andl	$255, %eax
	orl	%edx, %ebx
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	movl	%ebx, %edx
	je	.L1755
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1758
	.p2align 4,,7
.L1755:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 16(%esp)
	movl	16(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1758:
	movl	%ebx, 44(%ebp)
	movl	20(%esp), %ecx
	movl	$40, 48(%ebp)
	movl	%ecx, 40(%ebp)
.L1754:
	addl	$24, %esp
	ret
	.size	op_shrdl_user_T0_T1_ECX_cc, .-op_shrdl_user_T0_T1_ECX_cc
	.p2align 4,,15
.globl op_adcl_user_T0_T1_cc
	.type	op_adcl_user_T0_T1_cc, @function
op_adcl_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	12(%esp), %edx
	leal	(%ebx,%esi), %eax
	movl	%edi, 8(%esp)
	leal	(%eax,%edx), %ebx
	movl	%edi, %eax
	shrl	$12, %eax
	movl	%ebx, %edx
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1760
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1763
	.p2align 4,,7
.L1760:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1763:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	8(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_adcl_user_T0_T1_cc, .-op_adcl_user_T0_T1_cc
	.p2align 4,,15
.globl op_sbbl_user_T0_T1_cc
	.type	op_sbbl_user_T0_T1_cc, @function
op_sbbl_user_T0_T1_cc:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, 12(%esp)
	movl	%ebx, %eax
	subl	%esi, %eax
	movl	%edi, 8(%esp)
	movl	%eax, %ebx
	movl	12(%esp), %eax
	subl	%eax, %ebx
	movl	%edi, %eax
	movl	%ebx, %edx
	shrl	$12, %eax
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1765
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1768
	.p2align 4,,7
.L1765:
	movl	%ebx, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	%edx, (%esp)
	call	remR3PhysWriteU32
.L1768:
	movl	%esi, 40(%ebp)
	movl	12(%esp), %edx
	movl	%ebx, 44(%ebp)
	leal	16(,%edx,4), %eax
	movl	%eax, 48(%ebp)
	addl	$16, %esp
	ret
	.size	op_sbbl_user_T0_T1_cc, .-op_sbbl_user_T0_T1_cc
	.p2align 4,,15
.globl op_cmpxchgl_user_T0_T1_EAX_cc
	.type	op_cmpxchgl_user_T0_T1_EAX_cc, @function
op_cmpxchgl_user_T0_T1_EAX_cc:
	subl	$20, %esp
	movl	(%ebp), %eax
	movl	%ebx, 16(%esp)
	subl	%ebx, %eax
	movl	%eax, 12(%esp)
	jne	.L1770
	movl	%edi, 8(%esp)
	movl	%edi, %eax
	movl	%esi, %ebx
	shrl	$12, %eax
	movl	%esi, %edx
	andl	$255, %eax
	leal	256(%eax), %ecx
	movl	%edi, %eax
	sall	$4, %ecx
	andl	$-4093, %eax
	cmpl	%eax, 888(%ecx,%ebp)
	je	.L1771
	movl	$1, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1775
	.p2align 4,,7
.L1771:
	movl	%esi, 4(%esp)
	movl	896(%ecx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysWriteU32
	jmp	.L1775
	.p2align 4,,7
.L1770:
	movl	%ebx, (%ebp)
.L1775:
	movl	16(%esp), %eax
	movl	%eax, 40(%ebp)
	movl	12(%esp), %eax
	movl	%eax, 44(%ebp)
	addl	$20, %esp
	ret
	.size	op_cmpxchgl_user_T0_T1_EAX_cc, .-op_cmpxchgl_user_T0_T1_EAX_cc
	.p2align 4,,15
.globl op_btl_T0_T1_cc
	.type	op_btl_T0_T1_cc, @function
op_btl_T0_T1_cc:
	movl	%esi, %ecx
	movl	%ebx, %eax
	andl	$31, %ecx
	shrl	%cl, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_btl_T0_T1_cc, .-op_btl_T0_T1_cc
	.p2align 4,,15
.globl op_btsl_T0_T1_cc
	.type	op_btsl_T0_T1_cc, @function
op_btsl_T0_T1_cc:
	movl	%esi, %ecx
	movl	$1, %eax
	andl	$31, %ecx
	movl	%ebx, %esi
	sall	%cl, %eax
	shrl	%cl, %esi
	orl	%eax, %ebx
	ret
	.size	op_btsl_T0_T1_cc, .-op_btsl_T0_T1_cc
	.p2align 4,,15
.globl op_btrl_T0_T1_cc
	.type	op_btrl_T0_T1_cc, @function
op_btrl_T0_T1_cc:
	movl	%esi, %ecx
	movl	$-2, %eax
	andl	$31, %ecx
	movl	%ebx, %esi
	roll	%cl, %eax
	shrl	%cl, %esi
	andl	%eax, %ebx
	ret
	.size	op_btrl_T0_T1_cc, .-op_btrl_T0_T1_cc
	.p2align 4,,15
.globl op_btcl_T0_T1_cc
	.type	op_btcl_T0_T1_cc, @function
op_btcl_T0_T1_cc:
	movl	%esi, %ecx
	movl	$1, %eax
	andl	$31, %ecx
	movl	%ebx, %esi
	sall	%cl, %eax
	shrl	%cl, %esi
	xorl	%eax, %ebx
	ret
	.size	op_btcl_T0_T1_cc, .-op_btcl_T0_T1_cc
	.p2align 4,,15
.globl op_add_bitl_A0_T1
	.type	op_add_bitl_A0_T1, @function
op_add_bitl_A0_T1:
	movl	%esi, %eax
	sarl	$5, %eax
	leal	(%edi,%eax,4), %edi
	ret
	.size	op_add_bitl_A0_T1, .-op_add_bitl_A0_T1
	.p2align 4,,15
.globl op_bsfl_T0_cc
	.type	op_bsfl_T0_cc, @function
op_bsfl_T0_cc:
	testl	%ebx, %ebx
	movl	%ebx, %eax
	je	.L1782
	xorl	%edx, %edx
	.p2align 4,,15
.L1789:
	testb	$1, %al
	jne	.L1788
	incl	%edx
	sarl	%eax
	jmp	.L1789
	.p2align 4,,7
.L1788:
	movl	$1, 44(%ebp)
	movl	%edx, %esi
	jmp	.L1786
	.p2align 4,,7
.L1782:
	movl	$0, 44(%ebp)
.L1786:
	ret
	.size	op_bsfl_T0_cc, .-op_bsfl_T0_cc
	.p2align 4,,15
.globl op_bsrl_T0_cc
	.type	op_bsrl_T0_cc, @function
op_bsrl_T0_cc:
	testl	%ebx, %ebx
	movl	%ebx, %eax
	je	.L1791
	movl	$31, %edx
	jl	.L1797
	.p2align 4,,15
.L1794:
	decl	%edx
	addl	%eax, %eax
	jns	.L1794
.L1797:
	movl	$1, 44(%ebp)
	movl	%edx, %esi
	jmp	.L1795
	.p2align 4,,7
.L1791:
	movl	$0, 44(%ebp)
.L1795:
	ret
	.size	op_bsrl_T0_cc, .-op_bsrl_T0_cc
	.p2align 4,,15
.globl op_update_bt_cc
	.type	op_update_bt_cc, @function
op_update_bt_cc:
	movl	%esi, 40(%ebp)
	ret
	.size	op_update_bt_cc, .-op_update_bt_cc
	.p2align 4,,15
.globl op_movl_T0_Dshiftl
	.type	op_movl_T0_Dshiftl, @function
op_movl_T0_Dshiftl:
	movl	52(%ebp), %eax
	leal	0(,%eax,4), %ebx
	ret
	.size	op_movl_T0_Dshiftl, .-op_movl_T0_Dshiftl
	.p2align 4,,15
.globl op_outl_T0_T1
	.type	op_outl_T0_T1, @function
op_outl_T0_T1:
	subl	$12, %esp
	movl	%esi, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_outl
	addl	$12, %esp
	ret
	.size	op_outl_T0_T1, .-op_outl_T0_T1
	.p2align 4,,15
.globl op_inl_T0_T1
	.type	op_inl_T0_T1, @function
op_inl_T0_T1:
	subl	$8, %esp
	movl	%ebx, 4(%esp)
	movl	%ebp, (%esp)
	call	cpu_inl
	movl	%eax, %esi
	addl	$8, %esp
	ret
	.size	op_inl_T0_T1, .-op_inl_T0_T1
	.p2align 4,,15
.globl op_inl_DX_T0
	.type	op_inl_DX_T0, @function
op_inl_DX_T0:
	subl	$8, %esp
	movzwl	8(%ebp), %eax
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_inl
	movl	%eax, %ebx
	addl	$8, %esp
	ret
	.size	op_inl_DX_T0, .-op_inl_DX_T0
	.p2align 4,,15
.globl op_outl_DX_T0
	.type	op_outl_DX_T0, @function
op_outl_DX_T0:
	subl	$12, %esp
	movzwl	8(%ebp), %eax
	movl	%ebx, 8(%esp)
	movl	%ebp, (%esp)
	movl	%eax, 4(%esp)
	call	cpu_outl
	addl	$12, %esp
	ret
	.size	op_outl_DX_T0, .-op_outl_DX_T0
	.p2align 4,,15
.globl op_check_iol_T0
	.type	op_check_iol_T0, @function
op_check_iol_T0:
	call	check_iol_T0
	ret
	.size	op_check_iol_T0, .-op_check_iol_T0
	.p2align 4,,15
.globl op_check_iol_DX
	.type	op_check_iol_DX, @function
op_check_iol_DX:
	call	check_iol_DX
	ret
	.size	op_check_iol_DX, .-op_check_iol_DX
	.p2align 4,,15
.globl op_movsbl_T0_T0
	.type	op_movsbl_T0_T0, @function
op_movsbl_T0_T0:
	movsbl	%bl,%ebx
	ret
	.size	op_movsbl_T0_T0, .-op_movsbl_T0_T0
	.p2align 4,,15
.globl op_movzbl_T0_T0
	.type	op_movzbl_T0_T0, @function
op_movzbl_T0_T0:
	movzbl	%bl, %ebx
	ret
	.size	op_movzbl_T0_T0, .-op_movzbl_T0_T0
	.p2align 4,,15
.globl op_movswl_T0_T0
	.type	op_movswl_T0_T0, @function
op_movswl_T0_T0:
	movswl	%bx,%ebx
	ret
	.size	op_movswl_T0_T0, .-op_movswl_T0_T0
	.p2align 4,,15
.globl op_movzwl_T0_T0
	.type	op_movzwl_T0_T0, @function
op_movzwl_T0_T0:
	movzwl	%bx, %ebx
	ret
	.size	op_movzwl_T0_T0, .-op_movzwl_T0_T0
	.p2align 4,,15
.globl op_movswl_EAX_AX
	.type	op_movswl_EAX_AX, @function
op_movswl_EAX_AX:
	movswl	(%ebp),%eax
	movl	%eax, (%ebp)
	ret
	.size	op_movswl_EAX_AX, .-op_movswl_EAX_AX
	.p2align 4,,15
.globl op_movsbw_AX_AL
	.type	op_movsbw_AX_AL, @function
op_movsbw_AX_AL:
	movsbl	(%ebp),%eax
	movw	%ax, (%ebp)
	ret
	.size	op_movsbw_AX_AL, .-op_movsbw_AX_AL
	.p2align 4,,15
.globl op_movslq_EDX_EAX
	.type	op_movslq_EDX_EAX, @function
op_movslq_EDX_EAX:
	movl	(%ebp), %eax
	sarl	$31, %eax
	movl	%eax, 8(%ebp)
	ret
	.size	op_movslq_EDX_EAX, .-op_movslq_EDX_EAX
	.p2align 4,,15
.globl op_movswl_DX_AX
	.type	op_movswl_DX_AX, @function
op_movswl_DX_AX:
	movswl	(%ebp),%eax
	sarl	$15, %eax
	movw	%ax, 8(%ebp)
	ret
	.size	op_movswl_DX_AX, .-op_movswl_DX_AX
	.p2align 4,,15
.globl op_addl_ESI_T0
	.type	op_addl_ESI_T0, @function
op_addl_ESI_T0:
	addl	%ebx, 24(%ebp)
	ret
	.size	op_addl_ESI_T0, .-op_addl_ESI_T0
	.p2align 4,,15
.globl op_addw_ESI_T0
	.type	op_addw_ESI_T0, @function
op_addw_ESI_T0:
	movl	24(%ebp), %eax
	addl	%ebx, %eax
	movw	%ax, 24(%ebp)
	ret
	.size	op_addw_ESI_T0, .-op_addw_ESI_T0
	.p2align 4,,15
.globl op_addl_EDI_T0
	.type	op_addl_EDI_T0, @function
op_addl_EDI_T0:
	addl	%ebx, 28(%ebp)
	ret
	.size	op_addl_EDI_T0, .-op_addl_EDI_T0
	.p2align 4,,15
.globl op_addw_EDI_T0
	.type	op_addw_EDI_T0, @function
op_addw_EDI_T0:
	movl	28(%ebp), %eax
	addl	%ebx, %eax
	movw	%ax, 28(%ebp)
	ret
	.size	op_addw_EDI_T0, .-op_addw_EDI_T0
	.p2align 4,,15
.globl op_decl_ECX
	.type	op_decl_ECX, @function
op_decl_ECX:
	decl	4(%ebp)
	ret
	.size	op_decl_ECX, .-op_decl_ECX
	.p2align 4,,15
.globl op_decw_ECX
	.type	op_decw_ECX, @function
op_decw_ECX:
	movl	4(%ebp), %eax
	decl	%eax
	movw	%ax, 4(%ebp)
	ret
	.size	op_decw_ECX, .-op_decw_ECX
	.p2align 4,,15
.globl op_addl_A0_SS
	.type	op_addl_A0_SS, @function
op_addl_A0_SS:
	movl	104(%ebp), %eax
	addl	%eax, %edi
	ret
	.size	op_addl_A0_SS, .-op_addl_A0_SS
	.p2align 4,,15
.globl op_subl_A0_2
	.type	op_subl_A0_2, @function
op_subl_A0_2:
	subl	$2, %edi
	ret
	.size	op_subl_A0_2, .-op_subl_A0_2
	.p2align 4,,15
.globl op_subl_A0_4
	.type	op_subl_A0_4, @function
op_subl_A0_4:
	subl	$4, %edi
	ret
	.size	op_subl_A0_4, .-op_subl_A0_4
	.p2align 4,,15
.globl op_addl_ESP_4
	.type	op_addl_ESP_4, @function
op_addl_ESP_4:
	addl	$4, 16(%ebp)
	ret
	.size	op_addl_ESP_4, .-op_addl_ESP_4
	.p2align 4,,15
.globl op_addl_ESP_2
	.type	op_addl_ESP_2, @function
op_addl_ESP_2:
	addl	$2, 16(%ebp)
	ret
	.size	op_addl_ESP_2, .-op_addl_ESP_2
	.p2align 4,,15
.globl op_addw_ESP_4
	.type	op_addw_ESP_4, @function
op_addw_ESP_4:
	movl	16(%ebp), %eax
	addl	$4, %eax
	movw	%ax, 16(%ebp)
	ret
	.size	op_addw_ESP_4, .-op_addw_ESP_4
	.p2align 4,,15
.globl op_addw_ESP_2
	.type	op_addw_ESP_2, @function
op_addw_ESP_2:
	movl	16(%ebp), %eax
	addl	$2, %eax
	movw	%ax, 16(%ebp)
	ret
	.size	op_addw_ESP_2, .-op_addw_ESP_2
	.p2align 4,,15
.globl op_addl_ESP_im
	.type	op_addl_ESP_im, @function
op_addl_ESP_im:
	addl	$__op_param1, 16(%ebp)
	ret
	.size	op_addl_ESP_im, .-op_addl_ESP_im
	.p2align 4,,15
.globl op_addw_ESP_im
	.type	op_addw_ESP_im, @function
op_addw_ESP_im:
	movl	16(%ebp), %eax
	addl	$__op_param1, %eax
	movw	%ax, 16(%ebp)
	ret
	.size	op_addw_ESP_im, .-op_addw_ESP_im
	.p2align 4,,15
.globl op_rdtsc
	.type	op_rdtsc, @function
op_rdtsc:
	call	helper_rdtsc
	ret
	.size	op_rdtsc, .-op_rdtsc
	.p2align 4,,15
.globl op_cpuid
	.type	op_cpuid, @function
op_cpuid:
	call	helper_cpuid
	ret
	.size	op_cpuid, .-op_cpuid
	.p2align 4,,15
.globl op_enter_level
	.type	op_enter_level, @function
op_enter_level:
	subl	$8, %esp
	movl	$__op_param2, %eax
	movl	%eax, 4(%esp)
	movl	$__op_param1, (%esp)
	call	helper_enter_level
	addl	$8, %esp
	ret
	.size	op_enter_level, .-op_enter_level
	.p2align 4,,15
.globl op_sysenter
	.type	op_sysenter, @function
op_sysenter:
	call	helper_sysenter
	ret
	.size	op_sysenter, .-op_sysenter
	.p2align 4,,15
.globl op_sysexit
	.type	op_sysexit, @function
op_sysexit:
	call	helper_sysexit
	ret
	.size	op_sysexit, .-op_sysexit
	.p2align 4,,15
.globl op_rdmsr
	.type	op_rdmsr, @function
op_rdmsr:
	call	helper_rdmsr
	ret
	.size	op_rdmsr, .-op_rdmsr
	.p2align 4,,15
.globl op_wrmsr
	.type	op_wrmsr, @function
op_wrmsr:
	call	helper_wrmsr
	ret
	.size	op_wrmsr, .-op_wrmsr
	.p2align 4,,15
.globl op_aam
	.type	op_aam, @function
op_aam:
	subl	$4, %esp
	movl	$__op_param1, %ecx
	movzbl	(%ebp), %eax
	cltd
	idivl	%ecx
	movl	%edx, %ecx
	movl	%eax, (%esp)
	movl	(%ebp), %eax
	sall	$8, (%esp)
	movl	%ecx, 44(%ebp)
	andl	$-65536, %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	%eax, (%ebp)
	popl	%eax
	ret
	.size	op_aam, .-op_aam
	.p2align 4,,15
.globl op_aad
	.type	op_aad, @function
op_aad:
	movzbl	1(%ebp), %eax
	imull	$__op_param1, %eax, %eax
	addb	(%ebp), %al
	movzbl	%al, %eax
	movw	%ax, (%ebp)
	movl	%eax, 44(%ebp)
	ret
	.size	op_aad, .-op_aad
	.p2align 4,,15
.globl op_aaa
	.type	op_aaa, @function
op_aaa:
	subl	$20, %esp
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, 8(%esp)
	movl	(%ebp), %edx
	movzbl	1(%ebp), %eax
	movzbl	%dl,%ecx
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	cmpl	$249, %ecx
	setg	%al
	andl	$15, %edx
	cmpl	$9, %edx
	movl	%edx, 4(%esp)
	setg	%dl
	testb	$16, 8(%esp)
	movl	%eax, 16(%esp)
	setne	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L1839
	orl	$17, 8(%esp)
	addl	$6, %ecx
	movl	12(%esp), %edx
	movl	16(%esp), %eax
	andl	$15, %ecx
	leal	1(%edx,%eax), %edx
	movzbl	%dl,%eax
	movl	%eax, 12(%esp)
	jmp	.L1840
	.p2align 4,,7
.L1839:
	andl	$-18, 8(%esp)
	movl	4(%esp), %ecx
.L1840:
	sall	$8, 12(%esp)
	movl	(%ebp), %eax
	movl	8(%esp), %edx
	andl	$-65536, %eax
	orl	%ecx, %eax
	movl	12(%esp), %ecx
	movl	%edx, 40(%ebp)
	orl	%ecx, %eax
	movl	%eax, (%ebp)
	addl	$20, %esp
	ret
	.size	op_aaa, .-op_aaa
	.p2align 4,,15
.globl op_aas
	.type	op_aas, @function
op_aas:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, 4(%esp)
	movl	(%ebp), %edx
	movzbl	1(%ebp), %ecx
	movzbl	%dl,%eax
	cmpl	$5, %eax
	movl	%eax, 8(%esp)
	setle	%al
	andl	$15, %edx
	movl	%edx, (%esp)
	cmpl	$9, %edx
	movzbl	%al, %eax
	movl	%eax, 12(%esp)
	setg	%dl
	testb	$16, 4(%esp)
	setne	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L1842
	subl	$6, 8(%esp)
	movl	12(%esp), %eax
	orl	$17, 4(%esp)
	andl	$15, 8(%esp)
	subl	%eax, %ecx
	leal	-1(%ecx), %eax
	movzbl	%al,%ecx
	jmp	.L1843
	.p2align 4,,7
.L1842:
	andl	$-18, 4(%esp)
	movl	(%esp), %eax
	movl	%eax, 8(%esp)
.L1843:
	movl	(%ebp), %eax
	sall	$8, %ecx
	movl	8(%esp), %edx
	andl	$-65536, %eax
	orl	%edx, %eax
	orl	%ecx, %eax
	movl	%eax, (%ebp)
	movl	4(%esp), %eax
	movl	%eax, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_aas, .-op_aas
	.p2align 4,,15
.globl op_daa
	.type	op_daa, @function
op_daa:
	subl	$12, %esp
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, (%esp)
	andl	$1, %eax
	movl	%eax, 4(%esp)
	movl	(%esp), %eax
	movl	$0, (%esp)
	andl	$16, %eax
	movl	%eax, 8(%esp)
	movl	(%ebp), %eax
	movzbl	%al,%ecx
	andl	$15, %eax
	cmpl	$9, %eax
	movl	8(%esp), %eax
	setg	%dl
	testl	%eax, %eax
	setne	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L1845
	movl	$16, (%esp)
	leal	6(%ecx), %eax
	movzbl	%al,%ecx
.L1845:
	movl	4(%esp), %edx
	cmpl	$159, %ecx
	setg	%al
	testl	%edx, %edx
	setne	%dl
	orl	%edx, %eax
	testb	$1, %al
	je	.L1846
	orl	$1, (%esp)
	leal	96(%ecx), %eax
	movzbl	%al,%ecx
.L1846:
	movb	%cl, (%ebp)
	testl	%ecx, %ecx
	jne	.L1847
	orl	$64, (%esp)
.L1847:
	movzbl	parity_table(%ecx), %eax
	andl	$128, %ecx
	movl	(%esp), %edx
	orl	%edx, %eax
	orl	%eax, %ecx
	movl	%ecx, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_daa, .-op_daa
	.p2align 4,,15
.globl op_das
	.type	op_das, @function
op_das:
	subl	$16, %esp
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, (%esp)
	andl	$1, %eax
	movl	%eax, 4(%esp)
	movl	(%esp), %eax
	movl	$0, (%esp)
	andl	$16, %eax
	movl	%eax, 8(%esp)
	movl	(%ebp), %eax
	movzbl	%al,%ecx
	movl	%ecx, 12(%esp)
	andl	$15, %eax
	cmpl	$9, %eax
	movl	8(%esp), %eax
	setg	%dl
	testl	%eax, %eax
	setne	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L1849
	movl	$16, (%esp)
	movl	4(%esp), %edx
	cmpl	$5, %ecx
	setle	%al
	testl	%edx, %edx
	setne	%dl
	orl	%edx, %eax
	testb	$1, %al
	je	.L1850
	movl	$17, (%esp)
.L1850:
	leal	-6(%ecx), %eax
	movzbl	%al,%ecx
.L1849:
	cmpl	$153, 12(%esp)
	movl	4(%esp), %edx
	setg	%al
	testl	%edx, %edx
	setne	%dl
	orl	%edx, %eax
	testb	$1, %al
	je	.L1851
	orl	$1, (%esp)
	leal	-96(%ecx), %eax
	movzbl	%al,%ecx
.L1851:
	movb	%cl, (%ebp)
	testl	%ecx, %ecx
	jne	.L1852
	orl	$64, (%esp)
.L1852:
	movzbl	parity_table(%ecx), %eax
	andl	$128, %ecx
	movl	(%esp), %edx
	orl	%edx, %eax
	orl	%eax, %ecx
	movl	%ecx, 40(%ebp)
	addl	$16, %esp
	ret
	.size	op_das, .-op_das
	.p2align 4,,15
.globl op_movl_seg_T0
	.type	op_movl_seg_T0, @function
op_movl_seg_T0:
	subl	$8, %esp
	movl	%ebx, 4(%esp)
	movl	$__op_param1, (%esp)
	call	load_seg
	addl	$8, %esp
	ret
	.size	op_movl_seg_T0, .-op_movl_seg_T0
	.p2align 4,,15
.globl op_movl_seg_T0_vm
	.type	op_movl_seg_T0_vm, @function
op_movl_seg_T0_vm:
	leal	__op_param1(%ebp), %edx
	movzwl	%bx,%eax
	movl	%eax, (%edx)
	sall	$4, %eax
	movl	%eax, 4(%edx)
	movl	$0, 12(%edx)
	ret
	.size	op_movl_seg_T0_vm, .-op_movl_seg_T0_vm
	.p2align 4,,15
.globl op_movl_T0_seg
	.type	op_movl_T0_seg, @function
op_movl_T0_seg:
	movl	$__op_param1, %eax
	leal	__op_param1(,%eax,4), %eax
	movl	60(%ebp,%eax,4), %ebx
	ret
	.size	op_movl_T0_seg, .-op_movl_T0_seg
	.p2align 4,,15
.globl op_lsl
	.type	op_lsl, @function
op_lsl:
	call	helper_lsl
	ret
	.size	op_lsl, .-op_lsl
	.p2align 4,,15
.globl op_lar
	.type	op_lar, @function
op_lar:
	call	helper_lar
	ret
	.size	op_lar, .-op_lar
	.p2align 4,,15
.globl op_verr
	.type	op_verr, @function
op_verr:
	call	helper_verr
	ret
	.size	op_verr, .-op_verr
	.p2align 4,,15
.globl op_verw
	.type	op_verw, @function
op_verw:
	call	helper_verw
	ret
	.size	op_verw, .-op_verw
	.p2align 4,,15
.globl op_arpl
	.type	op_arpl, @function
op_arpl:
	movl	%ebx, %edx
	movl	%esi, %eax
	andl	$3, %edx
	andl	$3, %eax
	cmpl	%eax, %edx
	jae	.L1861
	movl	%ebx, %edx
	movl	%esi, %eax
	andl	$-4, %edx
	andl	$3, %eax
	movl	%edx, %ebx
	orl	%eax, %ebx
	movl	$64, %esi
	jmp	.L1862
	.p2align 4,,7
.L1861:
	xorl	%esi, %esi
.L1862:
	ret
	.size	op_arpl, .-op_arpl
	.p2align 4,,15
.globl op_arpl_update
	.type	op_arpl_update, @function
op_arpl_update:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	andl	$-65, %eax
	orl	%esi, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_arpl_update, .-op_arpl_update
	.p2align 4,,15
.globl op_ljmp_protected_T0_T1
	.type	op_ljmp_protected_T0_T1, @function
op_ljmp_protected_T0_T1:
	subl	$4, %esp
	movl	$__op_param1, (%esp)
	call	helper_ljmp_protected_T0_T1
	popl	%eax
	ret
	.size	op_ljmp_protected_T0_T1, .-op_ljmp_protected_T0_T1
	.p2align 4,,15
.globl op_lcall_real_T0_T1
	.type	op_lcall_real_T0_T1, @function
op_lcall_real_T0_T1:
	subl	$8, %esp
	movl	$__op_param2, %eax
	movl	%eax, 4(%esp)
	movl	$__op_param1, (%esp)
	call	helper_lcall_real_T0_T1
	addl	$8, %esp
	ret
	.size	op_lcall_real_T0_T1, .-op_lcall_real_T0_T1
	.p2align 4,,15
.globl op_lcall_protected_T0_T1
	.type	op_lcall_protected_T0_T1, @function
op_lcall_protected_T0_T1:
	subl	$8, %esp
	movl	$__op_param2, %eax
	movl	%eax, 4(%esp)
	movl	$__op_param1, (%esp)
	call	helper_lcall_protected_T0_T1
	addl	$8, %esp
	ret
	.size	op_lcall_protected_T0_T1, .-op_lcall_protected_T0_T1
	.p2align 4,,15
.globl op_iret_real
	.type	op_iret_real, @function
op_iret_real:
	subl	$4, %esp
	movl	$__op_param1, (%esp)
	call	helper_iret_real
	popl	%eax
	ret
	.size	op_iret_real, .-op_iret_real
	.p2align 4,,15
.globl op_iret_protected
	.type	op_iret_protected, @function
op_iret_protected:
	subl	$8, %esp
	movl	$__op_param2, %eax
	movl	%eax, 4(%esp)
	movl	$__op_param1, (%esp)
	call	helper_iret_protected
	addl	$8, %esp
	ret
	.size	op_iret_protected, .-op_iret_protected
	.p2align 4,,15
.globl op_lret_protected
	.type	op_lret_protected, @function
op_lret_protected:
	subl	$8, %esp
	movl	$__op_param2, %edx
	movl	%edx, 4(%esp)
	movl	$__op_param1, (%esp)
	call	helper_lret_protected
	addl	$8, %esp
	ret
	.size	op_lret_protected, .-op_lret_protected
	.p2align 4,,15
.globl op_lldt_T0
	.type	op_lldt_T0, @function
op_lldt_T0:
	call	helper_lldt_T0
	ret
	.size	op_lldt_T0, .-op_lldt_T0
	.p2align 4,,15
.globl op_ltr_T0
	.type	op_ltr_T0, @function
op_ltr_T0:
	call	helper_ltr_T0
	ret
	.size	op_ltr_T0, .-op_ltr_T0
	.p2align 4,,15
.globl op_movl_crN_T0
	.type	op_movl_crN_T0, @function
op_movl_crN_T0:
	subl	$4, %esp
	movl	$__op_param1, (%esp)
	call	helper_movl_crN_T0
	popl	%ecx
	ret
	.size	op_movl_crN_T0, .-op_movl_crN_T0
	.p2align 4,,15
.globl op_movtl_T0_cr8
	.type	op_movtl_T0_cr8, @function
op_movtl_T0_cr8:
	subl	$4, %esp
	movl	%ebp, (%esp)
	call	cpu_get_apic_tpr
	movzbl	%al, %ebx
	popl	%eax
	ret
	.size	op_movtl_T0_cr8, .-op_movtl_T0_cr8
	.p2align 4,,15
.globl op_movl_drN_T0
	.type	op_movl_drN_T0, @function
op_movl_drN_T0:
	subl	$4, %esp
	movl	$__op_param1, (%esp)
	call	helper_movl_drN_T0
	popl	%eax
	ret
	.size	op_movl_drN_T0, .-op_movl_drN_T0
	.p2align 4,,15
.globl op_lmsw_T0
	.type	op_lmsw_T0, @function
op_lmsw_T0:
	subl	$4, %esp
	movl	%ebx, %eax
	movl	260(%ebp), %edx
	movl	$0, (%esp)
	andl	$15, %eax
	andl	$-15, %edx
	movl	%edx, %ebx
	orl	%eax, %ebx
	call	helper_movl_crN_T0
	popl	%eax
	ret
	.size	op_lmsw_T0, .-op_lmsw_T0
	.p2align 4,,15
.globl op_invlpg_A0
	.type	op_invlpg_A0, @function
op_invlpg_A0:
	subl	$4, %esp
	movl	%edi, (%esp)
	call	helper_invlpg
	popl	%eax
	ret
	.size	op_invlpg_A0, .-op_invlpg_A0
	.p2align 4,,15
.globl op_movl_T0_env
	.type	op_movl_T0_env, @function
op_movl_T0_env:
	movl	__op_param1(%ebp), %ebx
	ret
	.size	op_movl_T0_env, .-op_movl_T0_env
	.p2align 4,,15
.globl op_movl_env_T0
	.type	op_movl_env_T0, @function
op_movl_env_T0:
	movl	%ebx, __op_param1(%ebp)
	ret
	.size	op_movl_env_T0, .-op_movl_env_T0
	.p2align 4,,15
.globl op_movl_env_T1
	.type	op_movl_env_T1, @function
op_movl_env_T1:
	movl	%esi, __op_param1(%ebp)
	ret
	.size	op_movl_env_T1, .-op_movl_env_T1
	.p2align 4,,15
.globl op_movtl_T0_env
	.type	op_movtl_T0_env, @function
op_movtl_T0_env:
	movl	__op_param1(%ebp), %ebx
	ret
	.size	op_movtl_T0_env, .-op_movtl_T0_env
	.p2align 4,,15
.globl op_movtl_env_T0
	.type	op_movtl_env_T0, @function
op_movtl_env_T0:
	movl	%ebx, __op_param1(%ebp)
	ret
	.size	op_movtl_env_T0, .-op_movtl_env_T0
	.p2align 4,,15
.globl op_movtl_T1_env
	.type	op_movtl_T1_env, @function
op_movtl_T1_env:
	movl	__op_param1(%ebp), %esi
	ret
	.size	op_movtl_T1_env, .-op_movtl_T1_env
	.p2align 4,,15
.globl op_movtl_env_T1
	.type	op_movtl_env_T1, @function
op_movtl_env_T1:
	movl	%esi, __op_param1(%ebp)
	ret
	.size	op_movtl_env_T1, .-op_movtl_env_T1
	.p2align 4,,15
.globl op_clts
	.type	op_clts, @function
op_clts:
	andl	$-9, 260(%ebp)
	andl	$-2049, 56(%ebp)
	ret
	.size	op_clts, .-op_clts
	.data
	.align 4
	.type	dummy0.1, @object
	.size	dummy0.1, 4
dummy0.1:
	.long	.L1887
	.align 4
	.type	__op_label0.op_goto_tb0, @object
	.size	__op_label0.op_goto_tb0, 4
__op_label0.op_goto_tb0:
	.long	.L1888
	.text
	.p2align 4,,15
.globl op_goto_tb0
	.type	op_goto_tb0, @function
op_goto_tb0:
	jmp	*__op_param1+44
	.p2align 4,,7
.L1888:
.L1887:
	ret
	.size	op_goto_tb0, .-op_goto_tb0
	.data
	.align 4
	.type	dummy1.0, @object
	.size	dummy1.0, 4
dummy1.0:
	.long	.L1892
	.align 4
	.type	__op_label1.op_goto_tb1, @object
	.size	__op_label1.op_goto_tb1, 4
__op_label1.op_goto_tb1:
	.long	.L1893
	.text
	.p2align 4,,15
.globl op_goto_tb1
	.type	op_goto_tb1, @function
op_goto_tb1:
	jmp	*__op_param1+48
	.p2align 4,,7
.L1893:
.L1892:
	ret
	.size	op_goto_tb1, .-op_goto_tb1
	.p2align 4,,15
.globl op_jmp_label
	.type	op_jmp_label, @function
op_jmp_label:
#APP
	jmp __op_gen_label1
#NO_APP
	ret
	.size	op_jmp_label, .-op_jmp_label
	.p2align 4,,15
.globl op_jnz_T0_label
	.type	op_jnz_T0_label, @function
op_jnz_T0_label:
	testl	%ebx, %ebx
	je	.L1897
#APP
	jmp __op_gen_label1
#NO_APP
.L1897:
	ret
	.size	op_jnz_T0_label, .-op_jnz_T0_label
	.p2align 4,,15
.globl op_jz_T0_label
	.type	op_jz_T0_label, @function
op_jz_T0_label:
	testl	%ebx, %ebx
	jne	.L1899
#APP
	jmp __op_gen_label1
#NO_APP
.L1899:
	ret
	.size	op_jz_T0_label, .-op_jz_T0_label
	.p2align 4,,15
.globl op_seto_T0_cc
	.type	op_seto_T0_cc, @function
op_seto_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	sarl	$11, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_seto_T0_cc, .-op_seto_T0_cc
	.p2align 4,,15
.globl op_setb_T0_cc
	.type	op_setb_T0_cc, @function
op_setb_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	movl	%eax, %ebx
	ret
	.size	op_setb_T0_cc, .-op_setb_T0_cc
	.p2align 4,,15
.globl op_setz_T0_cc
	.type	op_setz_T0_cc, @function
op_setz_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	sarl	$6, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_setz_T0_cc, .-op_setz_T0_cc
	.p2align 4,,15
.globl op_setbe_T0_cc
	.type	op_setbe_T0_cc, @function
op_setbe_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ebx, %ebx
	testb	$65, %al
	setne	%bl
	ret
	.size	op_setbe_T0_cc, .-op_setbe_T0_cc
	.p2align 4,,15
.globl op_sets_T0_cc
	.type	op_sets_T0_cc, @function
op_sets_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	sarl	$7, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_sets_T0_cc, .-op_sets_T0_cc
	.p2align 4,,15
.globl op_setp_T0_cc
	.type	op_setp_T0_cc, @function
op_setp_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	sarl	$2, %eax
	movl	%eax, %ebx
	andl	$1, %ebx
	ret
	.size	op_setp_T0_cc, .-op_setp_T0_cc
	.p2align 4,,15
.globl op_setl_T0_cc
	.type	op_setl_T0_cc, @function
op_setl_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, %edx
	sarl	$4, %edx
	xorl	%eax, %edx
	sarl	$7, %edx
	movl	%edx, %ebx
	andl	$1, %ebx
	ret
	.size	op_setl_T0_cc, .-op_setl_T0_cc
	.p2align 4,,15
.globl op_setle_T0_cc
	.type	op_setle_T0_cc, @function
op_setle_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	%ecx, %ecx
	movl	%eax, %edx
	shrl	$4, %eax
	xorb	%dl, %al
	js	.L1909
	testb	$64, %dl
	je	.L1908
.L1909:
	movl	$1, %ecx
.L1908:
	xorl	%ebx, %ebx
	testl	%ecx, %ecx
	setne	%bl
	ret
	.size	op_setle_T0_cc, .-op_setle_T0_cc
	.p2align 4,,15
.globl op_xor_T0_1
	.type	op_xor_T0_1, @function
op_xor_T0_1:
	xorl	$1, %ebx
	ret
	.size	op_xor_T0_1, .-op_xor_T0_1
	.p2align 4,,15
.globl op_set_cc_op
	.type	op_set_cc_op, @function
op_set_cc_op:
	movl	$__op_param1, 48(%ebp)
	ret
	.size	op_set_cc_op, .-op_set_cc_op
	.p2align 4,,15
.globl op_mov_T0_cc
	.type	op_mov_T0_cc, @function
op_mov_T0_cc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%eax, %ebx
	ret
	.size	op_mov_T0_cc, .-op_mov_T0_cc
	.p2align 4,,15
.globl op_movl_eflags_T0
	.type	op_movl_eflags_T0, @function
op_movl_eflags_T0:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$2375936, %ecx
	andl	$-2375937, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movl_eflags_T0, .-op_movl_eflags_T0
	.p2align 4,,15
.globl op_movw_eflags_T0
	.type	op_movw_eflags_T0, @function
op_movw_eflags_T0:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$16640, %ecx
	andl	$-16641, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movw_eflags_T0, .-op_movw_eflags_T0
	.p2align 4,,15
.globl op_movl_eflags_T0_io
	.type	op_movl_eflags_T0_io, @function
op_movl_eflags_T0_io:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$2376448, %ecx
	andl	$-2376449, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movl_eflags_T0_io, .-op_movl_eflags_T0_io
	.p2align 4,,15
.globl op_movw_eflags_T0_io
	.type	op_movw_eflags_T0_io, @function
op_movw_eflags_T0_io:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$17152, %ecx
	andl	$-17153, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movw_eflags_T0_io, .-op_movw_eflags_T0_io
	.p2align 4,,15
.globl op_movl_eflags_T0_cpl0
	.type	op_movl_eflags_T0_cpl0, @function
op_movl_eflags_T0_cpl0:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$2388736, %ecx
	andl	$-2388737, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movl_eflags_T0_cpl0, .-op_movl_eflags_T0_cpl0
	.p2align 4,,15
.globl op_movw_eflags_T0_cpl0
	.type	op_movw_eflags_T0_cpl0, @function
op_movw_eflags_T0_cpl0:
	movl	%ebx, %eax
	movl	%ebx, %edx
	andl	$2261, %eax
	movl	%eax, 40(%ebp)
	shrl	$9, %edx
	movl	$1, %eax
	andl	$2, %edx
	movl	%ebx, %ecx
	subl	%edx, %eax
	movl	%eax, 52(%ebp)
	movl	36(%ebp), %eax
	andl	$29440, %ecx
	andl	$-29441, %eax
	orl	%ecx, %eax
	movl	%eax, 36(%ebp)
	ret
	.size	op_movw_eflags_T0_cpl0, .-op_movw_eflags_T0_cpl0
	.p2align 4,,15
.globl op_movb_eflags_T0
	.type	op_movb_eflags_T0, @function
op_movb_eflags_T0:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	%ebx, %edx
	andl	$2048, %eax
	andl	$213, %edx
	orl	%eax, %edx
	movl	%edx, 40(%ebp)
	ret
	.size	op_movb_eflags_T0, .-op_movb_eflags_T0
	.p2align 4,,15
.globl op_movl_T0_eflags
	.type	op_movl_T0_eflags, @function
op_movl_T0_eflags:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	52(%ebp), %edx
	andl	$1024, %edx
	orl	%eax, %edx
	movl	36(%ebp), %eax
	movl	%edx, %ebx
	andl	$-196609, %eax
	orl	%eax, %ebx
	ret
	.size	op_movl_T0_eflags, .-op_movl_T0_eflags
	.p2align 4,,15
.globl op_cld
	.type	op_cld, @function
op_cld:
	movl	$1, 52(%ebp)
	ret
	.size	op_cld, .-op_cld
	.p2align 4,,15
.globl op_std
	.type	op_std, @function
op_std:
	movl	$-1, 52(%ebp)
	ret
	.size	op_std, .-op_std
	.p2align 4,,15
.globl op_clc
	.type	op_clc, @function
op_clc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	andl	$-2, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_clc, .-op_clc
	.p2align 4,,15
.globl op_stc
	.type	op_stc, @function
op_stc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	orl	$1, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_stc, .-op_stc
	.p2align 4,,15
.globl op_cmc
	.type	op_cmc, @function
op_cmc:
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	xorl	$1, %eax
	movl	%eax, 40(%ebp)
	ret
	.size	op_cmc, .-op_cmc
	.p2align 4,,15
.globl op_salc
	.type	op_salc, @function
op_salc:
	movl	48(%ebp), %eax
	call	*cc_table+4(,%eax,8)
	negl	%eax
	movb	%al, (%ebp)
	ret
	.size	op_salc, .-op_salc
	.p2align 4,,15
	.type	compute_all_eflags, @function
compute_all_eflags:
	movl	40(%ebp), %eax
	ret
	.size	compute_all_eflags, .-compute_all_eflags
	.p2align 4,,15
	.type	compute_c_eflags, @function
compute_c_eflags:
	movl	40(%ebp), %eax
	andl	$1, %eax
	ret
	.size	compute_c_eflags, .-compute_c_eflags
	.p2align 4,,15
.globl op_flds_FT0_A0
	.type	op_flds_FT0_A0, @function
op_flds_FT0_A0:
	subl	$12, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1936
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldl_mmu
	jmp	.L1937
	.p2align 4,,7
.L1936:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU32
.L1937:
	movl	%eax, 4(%esp)
	flds	4(%esp)
	fstpt	436(%ebp)
	addl	$12, %esp
	ret
	.size	op_flds_FT0_A0, .-op_flds_FT0_A0
	.p2align 4,,15
.globl op_fldl_FT0_A0
	.type	op_fldl_FT0_A0, @function
op_fldl_FT0_A0:
	subl	$16, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 12(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1942
	movl	12(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L1943
	.p2align 4,,7
.L1942:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU64
.L1943:
	movl	%eax, 4(%esp)
	movl	%edx, 8(%esp)
	fldl	4(%esp)
	fstpt	436(%ebp)
	addl	$16, %esp
	ret
	.size	op_fldl_FT0_A0, .-op_fldl_FT0_A0
	.p2align 4,,15
.globl op_fild_FT0_A0
	.type	op_fild_FT0_A0, @function
op_fild_FT0_A0:
	subl	$12, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1948
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldw_mmu
	jmp	.L1952
	.p2align 4,,7
.L1948:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadS16
.L1952:
	cwtl
	pushl	%eax
	fildl	(%esp)
	fstpt	436(%ebp)
	addl	$16, %esp
	ret
	.size	op_fild_FT0_A0, .-op_fild_FT0_A0
	.p2align 4,,15
.globl op_fildl_FT0_A0
	.type	op_fildl_FT0_A0, @function
op_fildl_FT0_A0:
	subl	$12, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1954
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldl_mmu
	jmp	.L1955
	.p2align 4,,7
.L1954:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU32
.L1955:
	pushl	%eax
	fildl	(%esp)
	fstpt	436(%ebp)
	addl	$16, %esp
	ret
	.size	op_fildl_FT0_A0, .-op_fildl_FT0_A0
	.p2align 4,,15
.globl op_fildll_FT0_A0
	.type	op_fildll_FT0_A0, @function
op_fildll_FT0_A0:
	subl	$12, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1959
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L1960
	.p2align 4,,7
.L1959:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L1960:
	pushl	%edx
	pushl	%eax
	fildll	(%esp)
	fstpt	436(%ebp)
	addl	$20, %esp
	ret
	.size	op_fildll_FT0_A0, .-op_fildll_FT0_A0
	.p2align 4,,15
.globl op_flds_ST0_A0
	.type	op_flds_ST0_A0, @function
op_flds_ST0_A0:
	subl	$20, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 16(%esp)
	sall	$4, %eax
	leal	304(%eax,%ebp), %eax
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1964
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldl_mmu
	jmp	.L1965
	.p2align 4,,7
.L1964:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU32
.L1965:
	movl	%eax, 4(%esp)
	movl	12(%esp), %eax
	xorl	%edx, %edx
	flds	4(%esp)
	fstpt	(%eax)
	movl	16(%esp), %eax
	movl	%eax, 284(%ebp)
	movb	%dl, 296(%eax,%ebp)
	addl	$20, %esp
	ret
	.size	op_flds_ST0_A0, .-op_flds_ST0_A0
	.p2align 4,,15
.globl op_fldl_ST0_A0
	.type	op_fldl_ST0_A0, @function
op_fldl_ST0_A0:
	subl	$24, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 20(%esp)
	sall	$4, %eax
	leal	304(%eax,%ebp), %eax
	movl	%eax, 16(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 12(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1970
	movl	12(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L1971
	.p2align 4,,7
.L1970:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	%ecx, (%esp)
	call	remR3PhysReadU64
.L1971:
	movl	%eax, 4(%esp)
	movl	16(%esp), %eax
	movl	%edx, 8(%esp)
	xorl	%edx, %edx
	fldl	4(%esp)
	fstpt	(%eax)
	movl	20(%esp), %eax
	movl	%eax, 284(%ebp)
	movb	%dl, 296(%eax,%ebp)
	addl	$24, %esp
	ret
	.size	op_fldl_ST0_A0, .-op_fldl_ST0_A0
	.p2align 4,,15
.globl op_fldt_ST0_A0
	.type	op_fldt_ST0_A0, @function
op_fldt_ST0_A0:
	call	helper_fldt_ST0_A0
	ret
	.size	op_fldt_ST0_A0, .-op_fldt_ST0_A0
	.p2align 4,,15
.globl op_fild_ST0_A0
	.type	op_fild_ST0_A0, @function
op_fild_ST0_A0:
	subl	$20, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 16(%esp)
	sall	$4, %eax
	leal	304(%eax,%ebp), %eax
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1977
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldw_mmu
	jmp	.L1981
	.p2align 4,,7
.L1977:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadS16
.L1981:
	cwtl
	xorl	%edx, %edx
	pushl	%eax
	movl	16(%esp), %eax
	fildl	(%esp)
	fstpt	(%eax)
	movl	20(%esp), %eax
	movl	%eax, 284(%ebp)
	movb	%dl, 296(%eax,%ebp)
	addl	$24, %esp
	ret
	.size	op_fild_ST0_A0, .-op_fild_ST0_A0
	.p2align 4,,15
.globl op_fildl_ST0_A0
	.type	op_fildl_ST0_A0, @function
op_fildl_ST0_A0:
	subl	$20, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 16(%esp)
	sall	$4, %eax
	leal	304(%eax,%ebp), %eax
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1983
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldl_mmu
	jmp	.L1984
	.p2align 4,,7
.L1983:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU32
.L1984:
	pushl	%eax
	xorl	%ecx, %ecx
	movl	16(%esp), %eax
	fildl	(%esp)
	fstpt	(%eax)
	movl	20(%esp), %eax
	movl	%eax, 284(%ebp)
	movb	%cl, 296(%eax,%ebp)
	addl	$24, %esp
	ret
	.size	op_fildl_ST0_A0, .-op_fildl_ST0_A0
	.p2align 4,,15
.globl op_fildll_ST0_A0
	.type	op_fildll_ST0_A0, @function
op_fildll_ST0_A0:
	subl	$20, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 16(%esp)
	sall	$4, %eax
	leal	304(%eax,%ebp), %eax
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L1988
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldq_mmu
	jmp	.L1989
	.p2align 4,,7
.L1988:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU64
.L1989:
	pushl	%edx
	xorl	%edx, %edx
	pushl	%eax
	movl	20(%esp), %eax
	fildll	(%esp)
	fstpt	(%eax)
	movl	24(%esp), %eax
	movl	%eax, 284(%ebp)
	movb	%dl, 296(%eax,%ebp)
	addl	$28, %esp
	ret
	.size	op_fildll_ST0_A0, .-op_fildll_ST0_A0
	.p2align 4,,15
.globl op_fsts_ST0_A0
	.type	op_fsts_ST0_A0, @function
op_fsts_ST0_A0:
	subl	$20, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	56(%ebp), %eax
	fstps	12(%esp)
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L1993
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L1997
	.p2align 4,,7
.L1993:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU32
.L1997:
	addl	$20, %esp
	ret
	.size	op_fsts_ST0_A0, .-op_fsts_ST0_A0
	.p2align 4,,15
.globl op_fstl_ST0_A0
	.type	op_fstl_ST0_A0, @function
op_fstl_ST0_A0:
	subl	$32, %esp
	movl	%edi, %edx
	movl	284(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	56(%ebp), %eax
	fstpl	16(%esp)
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 12(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L1999
	movl	12(%esp), %eax
	movl	20(%esp), %edx
	movl	%eax, 8(%esp)
	movl	16(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L2003
	.p2align 4,,7
.L1999:
	movl	896(%edx,%ebp), %eax
	movl	20(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	16(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L2003:
	addl	$32, %esp
	ret
	.size	op_fstl_ST0_A0, .-op_fstl_ST0_A0
	.p2align 4,,15
.globl op_fstt_ST0_A0
	.type	op_fstt_ST0_A0, @function
op_fstt_ST0_A0:
	call	helper_fstt_ST0_A0
	ret
	.size	op_fstt_ST0_A0, .-op_fstt_ST0_A0
	.p2align 4,,15
.globl op_fist_ST0_A0
	.type	op_fist_ST0_A0, @function
op_fist_ST0_A0:
	subl	$24, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int32
	movl	%eax, 20(%esp)
	movswl	20(%esp),%eax
	cmpl	20(%esp), %eax
	je	.L2006
	movl	$-32768, %eax
	movl	%eax, 20(%esp)
.L2006:
	movl	56(%ebp), %eax
	movl	%edi, %edx
	movl	%edi, %ecx
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2007
	movzwl	20(%esp), %edx
	movl	16(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L2010
	.p2align 4,,7
.L2007:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzwl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
.L2010:
	addl	$24, %esp
	ret
	.size	op_fist_ST0_A0, .-op_fist_ST0_A0
	.p2align 4,,15
.globl op_fistl_ST0_A0
	.type	op_fistl_ST0_A0, @function
op_fistl_ST0_A0:
	subl	$24, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int32
	movl	%eax, 20(%esp)
	movl	56(%ebp), %eax
	movl	%edi, %edx
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2012
	movl	16(%esp), %eax
	movl	20(%esp), %edx
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L2015
	.p2align 4,,7
.L2012:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU32
.L2015:
	addl	$24, %esp
	ret
	.size	op_fistl_ST0_A0, .-op_fistl_ST0_A0
	.p2align 4,,15
.globl op_fistll_ST0_A0
	.type	op_fistll_ST0_A0, @function
op_fistll_ST0_A0:
	subl	$28, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int64
	movl	%eax, 20(%esp)
	movl	56(%ebp), %eax
	movl	%edi, %ecx
	movl	%edx, 24(%esp)
	movl	%edi, %edx
	andl	$3, %eax
	shrl	$12, %edx
	andl	$255, %edx
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2017
	movl	16(%esp), %eax
	movl	24(%esp), %edx
	movl	%eax, 8(%esp)
	movl	20(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L2020
	.p2align 4,,7
.L2017:
	movl	896(%edx,%ebp), %eax
	movl	24(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L2020:
	addl	$28, %esp
	ret
	.size	op_fistll_ST0_A0, .-op_fistll_ST0_A0
	.p2align 4,,15
.globl op_fistt_ST0_A0
	.type	op_fistt_ST0_A0, @function
op_fistt_ST0_A0:
	subl	$24, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int32_round_to_zero
	movl	%eax, 20(%esp)
	movswl	20(%esp),%eax
	cmpl	20(%esp), %eax
	je	.L2022
	movl	$-32768, %eax
	movl	%eax, 20(%esp)
.L2022:
	movl	56(%ebp), %eax
	movl	%edi, %edx
	movl	%edi, %ecx
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2023
	movzwl	20(%esp), %edx
	movl	16(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L2026
	.p2align 4,,7
.L2023:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzwl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
.L2026:
	addl	$24, %esp
	ret
	.size	op_fistt_ST0_A0, .-op_fistt_ST0_A0
	.p2align 4,,15
.globl op_fisttl_ST0_A0
	.type	op_fisttl_ST0_A0, @function
op_fisttl_ST0_A0:
	subl	$24, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int32_round_to_zero
	movl	%eax, 20(%esp)
	movl	56(%ebp), %eax
	movl	%edi, %edx
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4093, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2028
	movl	16(%esp), %eax
	movl	20(%esp), %edx
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stl_mmu
	jmp	.L2031
	.p2align 4,,7
.L2028:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU32
.L2031:
	addl	$24, %esp
	ret
	.size	op_fisttl_ST0_A0, .-op_fisttl_ST0_A0
	.p2align 4,,15
.globl op_fisttll_ST0_A0
	.type	op_fisttll_ST0_A0, @function
op_fisttll_ST0_A0:
	subl	$28, %esp
	leal	432(%ebp), %ecx
	movl	284(%ebp), %eax
	movl	%ecx, 12(%esp)
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fstpt	(%esp)
	call	floatx80_to_int64_round_to_zero
	movl	%eax, 20(%esp)
	movl	56(%ebp), %eax
	movl	%edi, %ecx
	movl	%edx, 24(%esp)
	movl	%edi, %edx
	andl	$3, %eax
	shrl	$12, %edx
	andl	$255, %edx
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 16(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4089, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2033
	movl	16(%esp), %eax
	movl	24(%esp), %edx
	movl	%eax, 8(%esp)
	movl	20(%esp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stq_mmu
	jmp	.L2036
	.p2align 4,,7
.L2033:
	movl	896(%edx,%ebp), %eax
	movl	24(%esp), %edx
	addl	%eax, %ecx
	movl	%edx, 8(%esp)
	movl	20(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU64
.L2036:
	addl	$28, %esp
	ret
	.size	op_fisttll_ST0_A0, .-op_fisttll_ST0_A0
	.p2align 4,,15
.globl op_fbld_ST0_A0
	.type	op_fbld_ST0_A0, @function
op_fbld_ST0_A0:
	call	helper_fbld_ST0_A0
	ret
	.size	op_fbld_ST0_A0, .-op_fbld_ST0_A0
	.p2align 4,,15
.globl op_fbst_ST0_A0
	.type	op_fbst_ST0_A0, @function
op_fbst_ST0_A0:
	call	helper_fbst_ST0_A0
	ret
	.size	op_fbst_ST0_A0, .-op_fbst_ST0_A0
	.p2align 4,,15
.globl op_fpush
	.type	op_fpush, @function
op_fpush:
	movl	284(%ebp), %eax
	xorl	%edx, %edx
	decl	%eax
	andl	$7, %eax
	movl	%eax, 284(%ebp)
	movb	%dl, 296(%eax,%ebp)
	ret
	.size	op_fpush, .-op_fpush
	.p2align 4,,15
.globl op_fpop
	.type	op_fpop, @function
op_fpop:
	movl	284(%ebp), %eax
	movb	$1, %cl
	movb	%cl, 296(%eax,%ebp)
	incl	%eax
	andl	$7, %eax
	movl	%eax, 284(%ebp)
	ret
	.size	op_fpop, .-op_fpop
	.p2align 4,,15
.globl op_fdecstp
	.type	op_fdecstp, @function
op_fdecstp:
	andl	$-18177, 288(%ebp)
	movl	284(%ebp), %eax
	decl	%eax
	andl	$7, %eax
	movl	%eax, 284(%ebp)
	ret
	.size	op_fdecstp, .-op_fdecstp
	.p2align 4,,15
.globl op_fincstp
	.type	op_fincstp, @function
op_fincstp:
	andl	$-18177, 288(%ebp)
	movl	284(%ebp), %eax
	incl	%eax
	andl	$7, %eax
	movl	%eax, 284(%ebp)
	ret
	.size	op_fincstp, .-op_fincstp
	.p2align 4,,15
.globl op_ffree_STN
	.type	op_ffree_STN, @function
op_ffree_STN:
	movl	284(%ebp), %eax
	movb	$1, %dl
	addl	$__op_param1, %eax
	andl	$7, %eax
	movb	%dl, 296(%eax,%ebp)
	ret
	.size	op_ffree_STN, .-op_ffree_STN
	.p2align 4,,15
.globl op_fmov_ST0_FT0
	.type	op_fmov_ST0_FT0, @function
op_fmov_ST0_FT0:
	fldt	436(%ebp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fmov_ST0_FT0, .-op_fmov_ST0_FT0
	.p2align 4,,15
.globl op_fmov_FT0_STN
	.type	op_fmov_FT0_STN, @function
op_fmov_FT0_STN:
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 444(%ebp)
	movl	%edx, 440(%ebp)
	movl	%eax, 436(%ebp)
	ret
	.size	op_fmov_FT0_STN, .-op_fmov_FT0_STN
	.p2align 4,,15
.globl op_fmov_ST0_STN
	.type	op_fmov_ST0_STN, @function
op_fmov_ST0_STN:
	subl	$4, %esp
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	(%esp), %eax
	fstpt	304(%eax,%ebp)
	popl	%ecx
	ret
	.size	op_fmov_ST0_STN, .-op_fmov_ST0_STN
	.p2align 4,,15
.globl op_fmov_STN_ST0
	.type	op_fmov_STN_ST0, @function
op_fmov_STN_ST0:
	subl	$4, %esp
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	(%esp), %eax
	fstpt	304(%eax,%ebp)
	popl	%eax
	ret
	.size	op_fmov_STN_ST0, .-op_fmov_STN_ST0
	.p2align 4,,15
.globl op_fxchg_ST0_STN
	.type	op_fxchg_ST0_STN, @function
op_fxchg_ST0_STN:
	subl	$4, %esp
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	(%esp), %eax
	fstpt	304(%eax,%ebp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	popl	%eax
	ret
	.size	op_fxchg_ST0_STN, .-op_fxchg_ST0_STN
	.p2align 4,,15
.globl op_fcom_ST0_FT0
	.type	op_fcom_ST0_FT0, @function
op_fcom_ST0_FT0:
	subl	$28, %esp
	leal	432(%ebp), %eax
	movl	440(%ebp), %edx
	movl	%eax, 24(%esp)
	movl	436(%ebp), %eax
	movl	444(%ebp), %ecx
	movl	%edx, 16(%esp)
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	movl	%ecx, 20(%esp)
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	floatx80_compare
	movl	288(%ebp), %edx
	movl	fcom_ccval+4(,%eax,4), %ecx
	andl	$-17665, %edx
	orl	%ecx, %edx
	movl	%edx, 288(%ebp)
	addl	$28, %esp
	ret
	.size	op_fcom_ST0_FT0, .-op_fcom_ST0_FT0
	.p2align 4,,15
.globl op_fucom_ST0_FT0
	.type	op_fucom_ST0_FT0, @function
op_fucom_ST0_FT0:
	subl	$28, %esp
	leal	432(%ebp), %eax
	movl	440(%ebp), %edx
	movl	%eax, 24(%esp)
	movl	436(%ebp), %eax
	movl	444(%ebp), %ecx
	movl	%edx, 16(%esp)
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	movl	%ecx, 20(%esp)
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	floatx80_compare_quiet
	movl	288(%ebp), %edx
	movl	fcom_ccval+4(,%eax,4), %ecx
	andl	$-17665, %edx
	orl	%ecx, %edx
	movl	%edx, 288(%ebp)
	addl	$28, %esp
	ret
	.size	op_fucom_ST0_FT0, .-op_fucom_ST0_FT0
	.p2align 4,,15
.globl op_fcomi_ST0_FT0
	.type	op_fcomi_ST0_FT0, @function
op_fcomi_ST0_FT0:
	subl	$32, %esp
	leal	432(%ebp), %eax
	movl	440(%ebp), %edx
	movl	%eax, 24(%esp)
	movl	436(%ebp), %eax
	movl	444(%ebp), %ecx
	movl	%edx, 16(%esp)
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	movl	%ecx, 20(%esp)
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	floatx80_compare
	movl	%eax, 28(%esp)
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	28(%esp), %edx
	movl	fcomi_ccval+4(,%edx,4), %ecx
	andl	$-70, %eax
	orl	%ecx, %eax
	movl	%eax, 40(%ebp)
	addl	$32, %esp
	ret
	.size	op_fcomi_ST0_FT0, .-op_fcomi_ST0_FT0
	.p2align 4,,15
.globl op_fucomi_ST0_FT0
	.type	op_fucomi_ST0_FT0, @function
op_fucomi_ST0_FT0:
	subl	$32, %esp
	leal	432(%ebp), %eax
	movl	440(%ebp), %edx
	movl	%eax, 24(%esp)
	movl	436(%ebp), %eax
	movl	444(%ebp), %ecx
	movl	%edx, 16(%esp)
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	movl	%ecx, 20(%esp)
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	floatx80_compare_quiet
	movl	%eax, 28(%esp)
	movl	48(%ebp), %eax
	call	*cc_table(,%eax,8)
	movl	28(%esp), %edx
	movl	fcomi_ccval+4(,%edx,4), %ecx
	andl	$-70, %eax
	orl	%ecx, %eax
	movl	%eax, 40(%ebp)
	addl	$32, %esp
	ret
	.size	op_fucomi_ST0_FT0, .-op_fucomi_ST0_FT0
	.p2align 4,,15
.globl op_fcmov_ST0_STN_T0
	.type	op_fcmov_ST0_STN_T0, @function
op_fcmov_ST0_STN_T0:
	subl	$4, %esp
	testl	%ebx, %ebx
	je	.L2056
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	movl	(%esp), %eax
	fstpt	304(%eax,%ebp)
.L2056:
	popl	%eax
	ret
	.size	op_fcmov_ST0_STN_T0, .-op_fcmov_ST0_STN_T0
	.p2align 4,,15
.globl op_fadd_ST0_FT0
	.type	op_fadd_ST0_FT0, @function
op_fadd_ST0_FT0:
	fldt	436(%ebp)
	movl	284(%ebp), %edx
	sall	$4, %edx
	fldt	304(%edx,%ebp)
	faddp	%st, %st(1)
	fstpt	304(%edx,%ebp)
	ret
	.size	op_fadd_ST0_FT0, .-op_fadd_ST0_FT0
	.p2align 4,,15
.globl op_fmul_ST0_FT0
	.type	op_fmul_ST0_FT0, @function
op_fmul_ST0_FT0:
	fldt	436(%ebp)
	movl	284(%ebp), %edx
	sall	$4, %edx
	fldt	304(%edx,%ebp)
	fmulp	%st, %st(1)
	fstpt	304(%edx,%ebp)
	ret
	.size	op_fmul_ST0_FT0, .-op_fmul_ST0_FT0
	.p2align 4,,15
.globl op_fsub_ST0_FT0
	.type	op_fsub_ST0_FT0, @function
op_fsub_ST0_FT0:
	fldt	436(%ebp)
	movl	284(%ebp), %edx
	sall	$4, %edx
	fldt	304(%edx,%ebp)
	fsubp	%st, %st(1)
	fstpt	304(%edx,%ebp)
	ret
	.size	op_fsub_ST0_FT0, .-op_fsub_ST0_FT0
	.p2align 4,,15
.globl op_fsubr_ST0_FT0
	.type	op_fsubr_ST0_FT0, @function
op_fsubr_ST0_FT0:
	fldt	436(%ebp)
	movl	284(%ebp), %edx
	sall	$4, %edx
	fldt	304(%edx,%ebp)
	fsubrp	%st, %st(1)
	fstpt	304(%edx,%ebp)
	ret
	.size	op_fsubr_ST0_FT0, .-op_fsubr_ST0_FT0
	.p2align 4,,15
.globl op_fdiv_ST0_FT0
	.type	op_fdiv_ST0_FT0, @function
op_fdiv_ST0_FT0:
	subl	$28, %esp
	movl	284(%ebp), %eax
	movl	440(%ebp), %edx
	movl	444(%ebp), %ecx
	sall	$4, %eax
	addl	%ebp, %eax
	movl	%eax, 24(%esp)
	movl	436(%ebp), %eax
	movl	%edx, 16(%esp)
	movl	%ecx, 20(%esp)
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	helper_fdiv
	movl	24(%esp), %eax
	fstpt	304(%eax)
	addl	$28, %esp
	ret
	.size	op_fdiv_ST0_FT0, .-op_fdiv_ST0_FT0
	.p2align 4,,15
.globl op_fdivr_ST0_FT0
	.type	op_fdivr_ST0_FT0, @function
op_fdivr_ST0_FT0:
	subl	$28, %esp
	movl	284(%ebp), %eax
	sall	$4, %eax
	addl	%ebp, %eax
	movl	%eax, 24(%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 20(%esp)
	movl	444(%ebp), %ecx
	movl	%eax, 12(%esp)
	movl	436(%ebp), %eax
	movl	%edx, 16(%esp)
	movl	440(%ebp), %edx
	movl	%eax, (%esp)
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	call	helper_fdiv
	movl	24(%esp), %eax
	fstpt	304(%eax)
	addl	$28, %esp
	ret
	.size	op_fdivr_ST0_FT0, .-op_fdivr_ST0_FT0
	.p2align 4,,15
.globl op_fadd_STN_ST0
	.type	op_fadd_STN_ST0, @function
op_fadd_STN_ST0:
	movl	284(%ebp), %ecx
	movl	284(%ebp), %eax
	addl	$__op_param1, %ecx
	andl	$7, %ecx
	sall	$4, %eax
	sall	$4, %ecx
	fldt	304(%ecx,%ebp)
	fldt	304(%eax,%ebp)
	faddp	%st, %st(1)
	fstpt	304(%ecx,%ebp)
	ret
	.size	op_fadd_STN_ST0, .-op_fadd_STN_ST0
	.p2align 4,,15
.globl op_fmul_STN_ST0
	.type	op_fmul_STN_ST0, @function
op_fmul_STN_ST0:
	movl	284(%ebp), %ecx
	movl	284(%ebp), %eax
	addl	$__op_param1, %ecx
	andl	$7, %ecx
	sall	$4, %eax
	sall	$4, %ecx
	fldt	304(%ecx,%ebp)
	fldt	304(%eax,%ebp)
	fmulp	%st, %st(1)
	fstpt	304(%ecx,%ebp)
	ret
	.size	op_fmul_STN_ST0, .-op_fmul_STN_ST0
	.p2align 4,,15
.globl op_fsub_STN_ST0
	.type	op_fsub_STN_ST0, @function
op_fsub_STN_ST0:
	movl	284(%ebp), %ecx
	movl	284(%ebp), %eax
	addl	$__op_param1, %ecx
	andl	$7, %ecx
	sall	$4, %eax
	sall	$4, %ecx
	fldt	304(%ecx,%ebp)
	fldt	304(%eax,%ebp)
	fsubrp	%st, %st(1)
	fstpt	304(%ecx,%ebp)
	ret
	.size	op_fsub_STN_ST0, .-op_fsub_STN_ST0
	.p2align 4,,15
.globl op_fsubr_STN_ST0
	.type	op_fsubr_STN_ST0, @function
op_fsubr_STN_ST0:
	movl	284(%ebp), %edx
	movl	284(%ebp), %eax
	addl	$__op_param1, %edx
	andl	$7, %edx
	sall	$4, %eax
	sall	$4, %edx
	fldt	304(%eax,%ebp)
	addl	%ebp, %edx
	fldt	304(%edx)
	fsubrp	%st, %st(1)
	fstpt	304(%edx)
	ret
	.size	op_fsubr_STN_ST0, .-op_fsubr_STN_ST0
	.p2align 4,,15
.globl op_fdiv_STN_ST0
	.type	op_fdiv_STN_ST0, @function
op_fdiv_STN_ST0:
	subl	$28, %esp
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	addl	%ebp, %eax
	movl	%eax, 24(%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 20(%esp)
	movl	%edx, 16(%esp)
	movl	%eax, 12(%esp)
	movl	24(%esp), %eax
	fldt	304(%eax)
	fstpt	(%esp)
	call	helper_fdiv
	movl	24(%esp), %eax
	fstpt	304(%eax)
	addl	$28, %esp
	ret
	.size	op_fdiv_STN_ST0, .-op_fdiv_STN_ST0
	.p2align 4,,15
.globl op_fdivr_STN_ST0
	.type	op_fdivr_STN_ST0, @function
op_fdivr_STN_ST0:
	subl	$28, %esp
	movl	284(%ebp), %eax
	addl	$__op_param1, %eax
	andl	$7, %eax
	sall	$4, %eax
	addl	%ebp, %eax
	movl	%eax, 24(%esp)
	fldt	304(%eax)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	12(%esp)
	movl	312(%eax,%ebp), %ecx
	movl	308(%eax,%ebp), %edx
	movl	304(%eax,%ebp), %eax
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	helper_fdiv
	movl	24(%esp), %eax
	fstpt	304(%eax)
	addl	$28, %esp
	ret
	.size	op_fdivr_STN_ST0, .-op_fdivr_STN_ST0
	.p2align 4,,15
.globl op_fchs_ST0
	.type	op_fchs_ST0, @function
op_fchs_ST0:
	subl	$4, %esp
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fchs
	fstpt	304(%eax,%ebp)
	popl	%eax
	ret
	.size	op_fchs_ST0, .-op_fchs_ST0
	.p2align 4,,15
.globl op_fabs_ST0
	.type	op_fabs_ST0, @function
op_fabs_ST0:
	subl	$4, %esp
	movl	284(%ebp), %eax
	sall	$4, %eax
	movl	%eax, (%esp)
	movl	284(%ebp), %eax
	sall	$4, %eax
	fldt	304(%eax,%ebp)
	fabs
	fstpt	304(%eax,%ebp)
	popl	%eax
	ret
	.size	op_fabs_ST0, .-op_fabs_ST0
	.p2align 4,,15
.globl op_fxam_ST0
	.type	op_fxam_ST0, @function
op_fxam_ST0:
	call	helper_fxam_ST0
	ret
	.size	op_fxam_ST0, .-op_fxam_ST0
	.p2align 4,,15
.globl op_fld1_ST0
	.type	op_fld1_ST0, @function
op_fld1_ST0:
	fldt	f15rk+12
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fld1_ST0, .-op_fld1_ST0
	.p2align 4,,15
.globl op_fldl2t_ST0
	.type	op_fldl2t_ST0, @function
op_fldl2t_ST0:
	fldt	f15rk+72
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldl2t_ST0, .-op_fldl2t_ST0
	.p2align 4,,15
.globl op_fldl2e_ST0
	.type	op_fldl2e_ST0, @function
op_fldl2e_ST0:
	fldt	f15rk+60
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldl2e_ST0, .-op_fldl2e_ST0
	.p2align 4,,15
.globl op_fldpi_ST0
	.type	op_fldpi_ST0, @function
op_fldpi_ST0:
	fldt	f15rk+24
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldpi_ST0, .-op_fldpi_ST0
	.p2align 4,,15
.globl op_fldlg2_ST0
	.type	op_fldlg2_ST0, @function
op_fldlg2_ST0:
	fldt	f15rk+36
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldlg2_ST0, .-op_fldlg2_ST0
	.p2align 4,,15
.globl op_fldln2_ST0
	.type	op_fldln2_ST0, @function
op_fldln2_ST0:
	fldt	f15rk+48
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldln2_ST0, .-op_fldln2_ST0
	.p2align 4,,15
.globl op_fldz_ST0
	.type	op_fldz_ST0, @function
op_fldz_ST0:
	fldt	f15rk
	movl	284(%ebp), %eax
	sall	$4, %eax
	fstpt	304(%eax,%ebp)
	ret
	.size	op_fldz_ST0, .-op_fldz_ST0
	.p2align 4,,15
.globl op_fldz_FT0
	.type	op_fldz_FT0, @function
op_fldz_FT0:
	movl	f15rk, %eax
	movl	f15rk+4, %edx
	movl	f15rk+8, %ecx
	movl	%eax, 436(%ebp)
	movl	%edx, 440(%ebp)
	movl	%ecx, 444(%ebp)
	ret
	.size	op_fldz_FT0, .-op_fldz_FT0
	.p2align 4,,15
.globl op_f2xm1
	.type	op_f2xm1, @function
op_f2xm1:
	call	helper_f2xm1
	ret
	.size	op_f2xm1, .-op_f2xm1
	.p2align 4,,15
.globl op_fyl2x
	.type	op_fyl2x, @function
op_fyl2x:
	call	helper_fyl2x
	ret
	.size	op_fyl2x, .-op_fyl2x
	.p2align 4,,15
.globl op_fptan
	.type	op_fptan, @function
op_fptan:
	call	helper_fptan
	ret
	.size	op_fptan, .-op_fptan
	.p2align 4,,15
.globl op_fpatan
	.type	op_fpatan, @function
op_fpatan:
	call	helper_fpatan
	ret
	.size	op_fpatan, .-op_fpatan
	.p2align 4,,15
.globl op_fxtract
	.type	op_fxtract, @function
op_fxtract:
	call	helper_fxtract
	ret
	.size	op_fxtract, .-op_fxtract
	.p2align 4,,15
.globl op_fprem1
	.type	op_fprem1, @function
op_fprem1:
	call	helper_fprem1
	ret
	.size	op_fprem1, .-op_fprem1
	.p2align 4,,15
.globl op_fprem
	.type	op_fprem, @function
op_fprem:
	call	helper_fprem
	ret
	.size	op_fprem, .-op_fprem
	.p2align 4,,15
.globl op_fyl2xp1
	.type	op_fyl2xp1, @function
op_fyl2xp1:
	call	helper_fyl2xp1
	ret
	.size	op_fyl2xp1, .-op_fyl2xp1
	.p2align 4,,15
.globl op_fsqrt
	.type	op_fsqrt, @function
op_fsqrt:
	call	helper_fsqrt
	ret
	.size	op_fsqrt, .-op_fsqrt
	.p2align 4,,15
.globl op_fsincos
	.type	op_fsincos, @function
op_fsincos:
	call	helper_fsincos
	ret
	.size	op_fsincos, .-op_fsincos
	.p2align 4,,15
.globl op_frndint
	.type	op_frndint, @function
op_frndint:
	call	helper_frndint
	ret
	.size	op_frndint, .-op_frndint
	.p2align 4,,15
.globl op_fscale
	.type	op_fscale, @function
op_fscale:
	call	helper_fscale
	ret
	.size	op_fscale, .-op_fscale
	.p2align 4,,15
.globl op_fsin
	.type	op_fsin, @function
op_fsin:
	call	helper_fsin
	ret
	.size	op_fsin, .-op_fsin
	.p2align 4,,15
.globl op_fcos
	.type	op_fcos, @function
op_fcos:
	call	helper_fcos
	ret
	.size	op_fcos, .-op_fcos
	.p2align 4,,15
.globl op_fnstsw_A0
	.type	op_fnstsw_A0, @function
op_fnstsw_A0:
	subl	$16, %esp
	movl	%edi, %edx
	movl	288(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	andl	$-14337, %eax
	movl	%eax, 12(%esp)
	movl	284(%ebp), %eax
	andl	$7, %eax
	sall	$11, %eax
	orl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2097
	movzwl	12(%esp), %edx
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L2100
	.p2align 4,,7
.L2097:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzwl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
.L2100:
	addl	$16, %esp
	ret
	.size	op_fnstsw_A0, .-op_fnstsw_A0
	.p2align 4,,15
.globl op_fnstsw_EAX
	.type	op_fnstsw_EAX, @function
op_fnstsw_EAX:
	movl	284(%ebp), %edx
	movl	288(%ebp), %eax
	andl	$7, %edx
	sall	$11, %edx
	andl	$-14337, %eax
	orl	%edx, %eax
	movl	(%ebp), %edx
	andl	$-65536, %edx
	orl	%eax, %edx
	movl	%edx, (%ebp)
	ret
	.size	op_fnstsw_EAX, .-op_fnstsw_EAX
	.p2align 4,,15
.globl op_fnstcw_A0
	.type	op_fnstcw_A0, @function
op_fnstcw_A0:
	subl	$16, %esp
	movl	%edi, %edx
	movl	292(%ebp), %eax
	shrl	$12, %edx
	movl	%edi, %ecx
	andl	$255, %edx
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2103
	movzwl	12(%esp), %edx
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__stw_mmu
	jmp	.L2106
	.p2align 4,,7
.L2103:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzwl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU16
.L2106:
	addl	$16, %esp
	ret
	.size	op_fnstcw_A0, .-op_fnstcw_A0
	.p2align 4,,15
.globl op_fldcw_A0
	.type	op_fldcw_A0, @function
op_fldcw_A0:
	subl	$12, %esp
	movl	%edi, %edx
	movl	56(%ebp), %eax
	movl	%edi, 8(%esp)
	shrl	$12, %edx
	andl	$255, %edx
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 4(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%edi, %eax
	sall	$4, %edx
	andl	$-4095, %eax
	cmpl	%eax, 884(%edx,%ebp)
	je	.L2108
	movl	4(%esp), %eax
	movl	%eax, (%esp)
	movl	%edi, %eax
	call	__ldw_mmu
	jmp	.L2112
	.p2align 4,,7
.L2108:
	movl	896(%edx,%ebp), %eax
	addl	%eax, 8(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	remR3PhysReadU16
.L2112:
	movzwl	%ax, %eax
	movl	%eax, 292(%ebp)
	call	update_fp_status
	addl	$12, %esp
	ret
	.size	op_fldcw_A0, .-op_fldcw_A0
	.p2align 4,,15
.globl op_fclex
	.type	op_fclex, @function
op_fclex:
	andl	$32512, 288(%ebp)
	ret
	.size	op_fclex, .-op_fclex
	.p2align 4,,15
.globl op_fwait
	.type	op_fwait, @function
op_fwait:
	cmpb	$0, 288(%ebp)
	jns	.L2115
	call	fpu_raise_exception
	.p2align 4,,15
.L2115:
	ret
	.size	op_fwait, .-op_fwait
	.p2align 4,,15
.globl op_fninit
	.type	op_fninit, @function
op_fninit:
	movb	$1, 296(%ebp)
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	movl	%ecx, 288(%ebp)
	movl	$895, %eax
	movl	%edx, 284(%ebp)
	movl	%eax, 292(%ebp)
	movb	$1, 297(%ebp)
	movb	$1, 298(%ebp)
	movb	$1, 299(%ebp)
	movb	$1, 300(%ebp)
	movb	$1, 301(%ebp)
	movb	$1, 302(%ebp)
	movb	$1, 303(%ebp)
	ret
	.size	op_fninit, .-op_fninit
	.p2align 4,,15
.globl op_fnstenv_A0
	.type	op_fnstenv_A0, @function
op_fnstenv_A0:
	subl	$8, %esp
	movl	$__op_param1, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	helper_fstenv
	addl	$8, %esp
	ret
	.size	op_fnstenv_A0, .-op_fnstenv_A0
	.p2align 4,,15
.globl op_fldenv_A0
	.type	op_fldenv_A0, @function
op_fldenv_A0:
	subl	$8, %esp
	movl	$__op_param1, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	helper_fldenv
	addl	$8, %esp
	ret
	.size	op_fldenv_A0, .-op_fldenv_A0
	.p2align 4,,15
.globl op_fnsave_A0
	.type	op_fnsave_A0, @function
op_fnsave_A0:
	subl	$8, %esp
	movl	$__op_param1, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	helper_fsave
	addl	$8, %esp
	ret
	.size	op_fnsave_A0, .-op_fnsave_A0
	.p2align 4,,15
.globl op_frstor_A0
	.type	op_frstor_A0, @function
op_frstor_A0:
	subl	$8, %esp
	movl	$__op_param1, %eax
	movl	%eax, 4(%esp)
	movl	%edi, (%esp)
	call	helper_frstor
	addl	$8, %esp
	ret
	.size	op_frstor_A0, .-op_frstor_A0
	.p2align 4,,15
.globl op_lock
	.type	op_lock, @function
op_lock:
	call	cpu_lock
	ret
	.size	op_lock, .-op_lock
	.p2align 4,,15
.globl op_unlock
	.type	op_unlock, @function
op_unlock:
	call	cpu_unlock
	ret
	.size	op_unlock, .-op_unlock
	.p2align 4,,15
.globl op_movo
	.type	op_movo, @function
op_movo:
	leal	__op_param2(%ebp), %edx
	movl	(%edx), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%ecx)
	movl	4(%edx), %eax
	movl	%eax, 4(%ecx)
	movl	8(%edx), %eax
	movl	%eax, 8(%ecx)
	movl	12(%edx), %eax
	movl	%eax, 12(%ecx)
	ret
	.size	op_movo, .-op_movo
	.p2align 4,,15
.globl op_movq
	.type	op_movq, @function
op_movq:
	movl	__op_param2(%ebp), %eax
	movl	__op_param2+4(%ebp), %edx
	movl	%eax, __op_param1(%ebp)
	movl	%edx, __op_param1+4(%ebp)
	ret
	.size	op_movq, .-op_movq
	.p2align 4,,15
.globl op_movl
	.type	op_movl, @function
op_movl:
	movl	__op_param2(%ebp), %eax
	movl	%eax, __op_param1(%ebp)
	ret
	.size	op_movl, .-op_movl
	.p2align 4,,15
.globl op_movq_env_0
	.type	op_movq_env_0, @function
op_movq_env_0:
	xorl	%eax, %eax
	movl	%eax, __op_param1(%ebp)
	xorl	%eax, %eax
	movl	%eax, __op_param1+4(%ebp)
	ret
	.size	op_movq_env_0, .-op_movq_env_0
	.p2align 4,,15
.globl op_fxsave_A0
	.type	op_fxsave_A0, @function
op_fxsave_A0:
	subl	$8, %esp
	movl	$__op_param1, %edx
	movl	%edx, 4(%esp)
	movl	%edi, (%esp)
	call	helper_fxsave
	addl	$8, %esp
	ret
	.size	op_fxsave_A0, .-op_fxsave_A0
	.p2align 4,,15
.globl op_fxrstor_A0
	.type	op_fxrstor_A0, @function
op_fxrstor_A0:
	subl	$8, %esp
	movl	$__op_param1, %ecx
	movl	%ecx, 4(%esp)
	movl	%edi, (%esp)
	call	helper_fxrstor
	addl	$8, %esp
	ret
	.size	op_fxrstor_A0, .-op_fxrstor_A0
	.p2align 4,,15
.globl op_enter_mmx
	.type	op_enter_mmx, @function
op_enter_mmx:
	xorl	%eax, %eax
	movl	%eax, 284(%ebp)
	xorl	%eax, %eax
	movl	%eax, 296(%ebp)
	xorl	%eax, %eax
	movl	%eax, 300(%ebp)
	ret
	.size	op_enter_mmx, .-op_enter_mmx
	.p2align 4,,15
.globl op_emms
	.type	op_emms, @function
op_emms:
	movl	$16843009, %eax
	movl	%eax, 296(%ebp)
	movl	$16843009, %eax
	movl	%eax, 300(%ebp)
	ret
	.size	op_emms, .-op_emms
	.p2align 4,,15
.globl op_psrlw_mmx
	.type	op_psrlw_mmx, @function
op_psrlw_mmx:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2134
	cmpl	$15, %eax
	jbe	.L2133
.L2134:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	jmp	.L2135
	.p2align 4,,7
.L2133:
	movl	12(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %edx
	movzwl	(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, (%ecx)
	movzwl	2(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 2(%ecx)
	movzwl	4(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 4(%ecx)
	movzwl	6(%ecx), %eax
	movb	%dl, %cl
	movl	8(%esp), %edx
	sarl	%cl, %eax
	movw	%ax, 6(%edx)
	.p2align 4,,15
.L2135:
	addl	$16, %esp
	ret
	.size	op_psrlw_mmx, .-op_psrlw_mmx
	.p2align 4,,15
.globl op_psraw_mmx
	.type	op_psraw_mmx, @function
op_psraw_mmx:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2138
	cmpl	$15, %eax
	jbe	.L2137
.L2138:
	movl	$15, %edx
	jmp	.L2139
	.p2align 4,,7
.L2137:
	movl	12(%esp), %ecx
	movzbl	(%ecx), %edx
	.p2align 4,,15
.L2139:
	movl	8(%esp), %ecx
	movswl	(%ecx),%eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, (%ecx)
	movswl	2(%ecx),%eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 2(%ecx)
	movswl	4(%ecx),%eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 4(%ecx)
	movswl	6(%ecx),%eax
	movb	%dl, %cl
	movl	8(%esp), %edx
	sarl	%cl, %eax
	movw	%ax, 6(%edx)
	addl	$16, %esp
	ret
	.size	op_psraw_mmx, .-op_psraw_mmx
	.p2align 4,,15
.globl op_psllw_mmx
	.type	op_psllw_mmx, @function
op_psllw_mmx:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2142
	cmpl	$15, %eax
	jbe	.L2141
.L2142:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	jmp	.L2143
	.p2align 4,,7
.L2141:
	movl	12(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %edx
	movzwl	(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, (%ecx)
	movzwl	2(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 2(%ecx)
	movzwl	4(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 4(%ecx)
	movzwl	6(%ecx), %eax
	movb	%dl, %cl
	movl	8(%esp), %edx
	sall	%cl, %eax
	movw	%ax, 6(%edx)
	.p2align 4,,15
.L2143:
	addl	$16, %esp
	ret
	.size	op_psllw_mmx, .-op_psllw_mmx
	.p2align 4,,15
.globl op_psrld_mmx
	.type	op_psrld_mmx, @function
op_psrld_mmx:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, (%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	cmpl	$0, %edx
	ja	.L2146
	cmpl	$31, %eax
	jbe	.L2145
.L2146:
	movl	(%esp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	jmp	.L2147
	.p2align 4,,7
.L2145:
	movzbl	(%ecx), %ecx
	movl	(%esp), %eax
	shrl	%cl, (%eax)
	shrl	%cl, 4(%eax)
	.p2align 4,,15
.L2147:
	popl	%eax
	ret
	.size	op_psrld_mmx, .-op_psrld_mmx
	.p2align 4,,15
.globl op_psrad_mmx
	.type	op_psrad_mmx, @function
op_psrad_mmx:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2150
	cmpl	$31, %eax
	jbe	.L2149
.L2150:
	movl	$31, %eax
	jmp	.L2151
	.p2align 4,,7
.L2149:
	movl	12(%esp), %ecx
	movzbl	(%ecx), %eax
	.p2align 4,,15
.L2151:
	movl	8(%esp), %edx
	movb	%al, %cl
	sarl	%cl, (%edx)
	sarl	%cl, 4(%edx)
	addl	$16, %esp
	ret
	.size	op_psrad_mmx, .-op_psrad_mmx
	.p2align 4,,15
.globl op_pslld_mmx
	.type	op_pslld_mmx, @function
op_pslld_mmx:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, (%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	cmpl	$0, %edx
	ja	.L2154
	cmpl	$31, %eax
	jbe	.L2153
.L2154:
	movl	(%esp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	jmp	.L2155
	.p2align 4,,7
.L2153:
	movzbl	(%ecx), %ecx
	movl	(%esp), %eax
	sall	%cl, (%eax)
	sall	%cl, 4(%eax)
	.p2align 4,,15
.L2155:
	popl	%edx
	ret
	.size	op_pslld_mmx, .-op_pslld_mmx
	.p2align 4,,15
.globl op_psrlq_mmx
	.type	op_psrlq_mmx, @function
op_psrlq_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	movl	%ecx, 8(%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	cmpl	$0, %edx
	ja	.L2158
	cmpl	$63, %eax
	jbe	.L2157
.L2158:
	movl	(%esp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	jmp	.L2159
	.p2align 4,,7
.L2157:
	movl	8(%esp), %ecx
	movzbl	(%ecx), %ecx
	movl	%ecx, 4(%esp)
	movl	(%esp), %ecx
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	movzbl	4(%esp), %ecx
	shrdl	%edx, %eax
	shrl	%cl, %edx
	testb	$32, %cl
	je	.L2160
	movl	%edx, %eax
	xorl	%edx, %edx
.L2160:
	movl	(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	.p2align 4,,15
.L2159:
	addl	$12, %esp
	ret
	.size	op_psrlq_mmx, .-op_psrlq_mmx
	.p2align 4,,15
.globl op_psllq_mmx
	.type	op_psllq_mmx, @function
op_psllq_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	movl	%ecx, 8(%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	cmpl	$0, %edx
	ja	.L2163
	cmpl	$63, %eax
	jbe	.L2162
.L2163:
	movl	(%esp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	jmp	.L2164
	.p2align 4,,7
.L2162:
	movl	8(%esp), %ecx
	movzbl	(%ecx), %ecx
	movl	%ecx, 4(%esp)
	movl	(%esp), %ecx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	movzbl	4(%esp), %ecx
	shldl	%eax, %edx
	sall	%cl, %eax
	testb	$32, %cl
	je	.L2165
	movl	%eax, %edx
	xorl	%eax, %eax
.L2165:
	movl	(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	.p2align 4,,15
.L2164:
	addl	$12, %esp
	ret
	.size	op_psllq_mmx, .-op_psllq_mmx
	.p2align 4,,15
.globl op_paddb_mmx
	.type	op_paddb_mmx, @function
op_paddb_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %eax
	movzbl	(%ecx), %edx
	addb	%dl, (%eax)
	movzbl	1(%ecx), %edx
	addb	%dl, 1(%eax)
	movzbl	2(%ecx), %edx
	addb	%dl, 2(%eax)
	movzbl	3(%ecx), %edx
	addb	%dl, 3(%eax)
	movzbl	4(%ecx), %edx
	addb	%dl, 4(%eax)
	movzbl	5(%ecx), %edx
	addb	%dl, 5(%eax)
	movzbl	6(%ecx), %edx
	addb	%dl, 6(%eax)
	movzbl	7(%ecx), %edx
	addb	%dl, 7(%eax)
	ret
	.size	op_paddb_mmx, .-op_paddb_mmx
	.p2align 4,,15
.globl op_paddw_mmx
	.type	op_paddw_mmx, @function
op_paddw_mmx:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movzwl	(%eax), %edx
	movzwl	(%ecx), %eax
	addl	%edx, %eax
	movw	%ax, (%ecx)
	movl	(%esp), %eax
	movzwl	2(%eax), %edx
	movzwl	2(%ecx), %eax
	addl	%edx, %eax
	movw	%ax, 2(%ecx)
	movl	(%esp), %eax
	movzwl	4(%eax), %edx
	movzwl	4(%ecx), %eax
	addl	%edx, %eax
	movw	%ax, 4(%ecx)
	movl	(%esp), %eax
	movzwl	6(%eax), %edx
	movzwl	6(%ecx), %eax
	addl	%edx, %eax
	movw	%ax, 6(%ecx)
	popl	%ecx
	ret
	.size	op_paddw_mmx, .-op_paddw_mmx
	.p2align 4,,15
.globl op_paddl_mmx
	.type	op_paddl_mmx, @function
op_paddl_mmx:
	leal	__op_param2(%ebp), %ecx
	movl	(%ecx), %edx
	leal	__op_param1(%ebp), %eax
	addl	%edx, (%eax)
	movl	4(%ecx), %edx
	addl	%edx, 4(%eax)
	ret
	.size	op_paddl_mmx, .-op_paddl_mmx
	.p2align 4,,15
.globl op_paddq_mmx
	.type	op_paddq_mmx, @function
op_paddq_mmx:
	movl	__op_param2(%ebp), %eax
	movl	__op_param2+4(%ebp), %edx
	addl	%eax, __op_param1(%ebp)
	adcl	%edx, __op_param1+4(%ebp)
	ret
	.size	op_paddq_mmx, .-op_paddq_mmx
	.p2align 4,,15
.globl op_psubb_mmx
	.type	op_psubb_mmx, @function
op_psubb_mmx:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	(%edx), %eax
	subb	(%ecx), %al
	movb	%al, (%edx)
	movzbl	1(%edx), %eax
	subb	1(%ecx), %al
	movb	%al, 1(%edx)
	movzbl	2(%edx), %eax
	subb	2(%ecx), %al
	movb	%al, 2(%edx)
	movzbl	3(%edx), %eax
	subb	3(%ecx), %al
	movb	%al, 3(%edx)
	movzbl	4(%edx), %eax
	subb	4(%ecx), %al
	movb	%al, 4(%edx)
	movzbl	5(%edx), %eax
	subb	5(%ecx), %al
	movb	%al, 5(%edx)
	movzbl	6(%edx), %eax
	subb	6(%ecx), %al
	movb	%al, 6(%edx)
	movzbl	7(%edx), %eax
	subb	7(%ecx), %al
	movb	%al, 7(%edx)
	ret
	.size	op_psubb_mmx, .-op_psubb_mmx
	.p2align 4,,15
.globl op_psubw_mmx
	.type	op_psubw_mmx, @function
op_psubw_mmx:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movzwl	(%eax), %edx
	movzwl	(%ecx), %eax
	subl	%edx, %eax
	movw	%ax, (%ecx)
	movl	(%esp), %eax
	movzwl	2(%eax), %edx
	movzwl	2(%ecx), %eax
	subl	%edx, %eax
	movw	%ax, 2(%ecx)
	movl	(%esp), %eax
	movzwl	4(%eax), %edx
	movzwl	4(%ecx), %eax
	subl	%edx, %eax
	movw	%ax, 4(%ecx)
	movl	(%esp), %eax
	movzwl	6(%eax), %edx
	movzwl	6(%ecx), %eax
	subl	%edx, %eax
	movw	%ax, 6(%ecx)
	popl	%eax
	ret
	.size	op_psubw_mmx, .-op_psubw_mmx
	.p2align 4,,15
.globl op_psubl_mmx
	.type	op_psubl_mmx, @function
op_psubl_mmx:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movl	(%edx), %eax
	subl	(%ecx), %eax
	movl	%eax, (%edx)
	movl	4(%edx), %eax
	subl	4(%ecx), %eax
	movl	%eax, 4(%edx)
	ret
	.size	op_psubl_mmx, .-op_psubl_mmx
	.p2align 4,,15
.globl op_psubq_mmx
	.type	op_psubq_mmx, @function
op_psubq_mmx:
	leal	__op_param1(%ebp), %ecx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	subl	__op_param2(%ebp), %eax
	sbbl	__op_param2+4(%ebp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	ret
	.size	op_psubq_mmx, .-op_psubq_mmx
	.p2align 4,,15
.globl op_paddusb_mmx
	.type	op_paddusb_mmx, @function
op_paddusb_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzbl	(%ecx), %edx
	movzbl	(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2176
	movl	4(%esp), %edx
.L2176:
	movb	%dl, (%ecx)
	movl	8(%esp), %edx
	movzbl	1(%ecx), %eax
	movzbl	1(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2181
	movl	%eax, %edx
.L2181:
	movb	%dl, 1(%ecx)
	movl	8(%esp), %eax
	movzbl	2(%ecx), %edx
	movzbl	2(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2186
	movl	4(%esp), %edx
.L2186:
	movb	%dl, 2(%ecx)
	movl	8(%esp), %edx
	movzbl	3(%ecx), %eax
	movzbl	3(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2191
	movl	%eax, %edx
.L2191:
	movb	%dl, 3(%ecx)
	movl	8(%esp), %eax
	movzbl	4(%ecx), %edx
	movzbl	4(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2196
	movl	4(%esp), %edx
.L2196:
	movb	%dl, 4(%ecx)
	movl	8(%esp), %edx
	movzbl	5(%ecx), %eax
	movzbl	5(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2201
	movl	%eax, %edx
.L2201:
	movb	%dl, 5(%ecx)
	movl	8(%esp), %eax
	movzbl	6(%ecx), %edx
	movzbl	6(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2206
	movl	4(%esp), %edx
.L2206:
	movb	%dl, 6(%ecx)
	movl	8(%esp), %edx
	movzbl	7(%ecx), %eax
	movzbl	7(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2211
	movl	%eax, %edx
.L2211:
	movb	%dl, 7(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddusb_mmx, .-op_paddusb_mmx
	.p2align 4,,15
.globl op_paddsb_mmx
	.type	op_paddsb_mmx, @function
op_paddsb_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movsbl	(%eax),%eax
	movsbl	(%ecx),%edx
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2217
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2217
	movl	%eax, %edx
	.p2align 4,,15
.L2217:
	movb	%dl, (%ecx)
	movl	8(%esp), %eax
	movsbl	1(%ecx),%edx
	movsbl	1(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2222
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2222
	movl	%eax, %edx
	.p2align 4,,15
.L2222:
	movb	%dl, 1(%ecx)
	movl	8(%esp), %eax
	movsbl	2(%ecx),%edx
	movsbl	2(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2227
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2227
	movl	%eax, %edx
	.p2align 4,,15
.L2227:
	movb	%dl, 2(%ecx)
	movl	8(%esp), %eax
	movsbl	3(%ecx),%edx
	movsbl	3(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2232
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2232
	movl	%eax, %edx
	.p2align 4,,15
.L2232:
	movb	%dl, 3(%ecx)
	movl	8(%esp), %eax
	movsbl	4(%ecx),%edx
	movsbl	4(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2237
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2237
	movl	%eax, %edx
	.p2align 4,,15
.L2237:
	movb	%dl, 4(%ecx)
	movl	8(%esp), %eax
	movsbl	5(%ecx),%edx
	movsbl	5(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2242
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2242
	movl	%eax, %edx
	.p2align 4,,15
.L2242:
	movb	%dl, 5(%ecx)
	movl	8(%esp), %eax
	movsbl	6(%ecx),%edx
	movsbl	6(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2247
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2247
	movl	%eax, %edx
	.p2align 4,,15
.L2247:
	movb	%dl, 6(%ecx)
	movl	8(%esp), %eax
	movsbl	7(%ecx),%edx
	movsbl	7(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2252
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2252
	movl	%eax, %edx
	.p2align 4,,15
.L2252:
	movb	%dl, 7(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddsb_mmx, .-op_paddsb_mmx
	.p2align 4,,15
.globl op_psubusb_mmx
	.type	op_psubusb_mmx, @function
op_psubusb_mmx:
	subl	$40, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 36(%esp)
	movzbl	(%eax), %eax
	movzbl	(%ecx), %edx
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 32(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2258
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 32(%esp)
	jg	.L2258
	movl	%eax, 32(%esp)
.L2258:
	movzbl	32(%esp), %eax
	movzbl	1(%ecx), %edx
	movb	%al, (%ecx)
	movl	36(%esp), %eax
	movzbl	1(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 28(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2263
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 28(%esp)
	jg	.L2263
	movl	%eax, 28(%esp)
.L2263:
	movzbl	28(%esp), %eax
	movzbl	2(%ecx), %edx
	movb	%al, 1(%ecx)
	movl	36(%esp), %eax
	movzbl	2(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 24(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2268
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 24(%esp)
	jg	.L2268
	movl	%eax, 24(%esp)
.L2268:
	movzbl	24(%esp), %eax
	movzbl	3(%ecx), %edx
	movb	%al, 2(%ecx)
	movl	36(%esp), %eax
	movzbl	3(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 20(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2273
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 20(%esp)
	jg	.L2273
	movl	%eax, 20(%esp)
.L2273:
	movzbl	20(%esp), %eax
	movzbl	4(%ecx), %edx
	movb	%al, 3(%ecx)
	movl	36(%esp), %eax
	movzbl	4(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 16(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2278
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 16(%esp)
	jg	.L2278
	movl	%eax, 16(%esp)
.L2278:
	movzbl	16(%esp), %eax
	movzbl	5(%ecx), %edx
	movb	%al, 4(%ecx)
	movl	36(%esp), %eax
	movzbl	5(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2283
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 12(%esp)
	jg	.L2283
	movl	%eax, 12(%esp)
.L2283:
	movzbl	12(%esp), %eax
	movzbl	6(%ecx), %edx
	movb	%al, 5(%ecx)
	movl	36(%esp), %eax
	movzbl	6(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2288
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 8(%esp)
	jg	.L2288
	movl	%eax, 8(%esp)
.L2288:
	movzbl	8(%esp), %eax
	movzbl	7(%ecx), %edx
	movb	%al, 6(%ecx)
	movl	36(%esp), %eax
	movzbl	7(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2293
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 4(%esp)
	jg	.L2293
	movl	%eax, 4(%esp)
.L2293:
	movzbl	4(%esp), %eax
	movb	%al, 7(%ecx)
	addl	$40, %esp
	ret
	.size	op_psubusb_mmx, .-op_psubusb_mmx
	.p2align 4,,15
.globl op_psubsb_mmx
	.type	op_psubsb_mmx, @function
op_psubsb_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movsbl	(%eax),%eax
	movsbl	(%ecx),%edx
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2299
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2299
	movl	%edx, %eax
	.p2align 4,,15
.L2299:
	movb	%al, (%ecx)
	movl	4(%esp), %eax
	movsbl	1(%ecx),%edx
	movsbl	1(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2304
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2304
	movl	%edx, %eax
	.p2align 4,,15
.L2304:
	movb	%al, 1(%ecx)
	movl	4(%esp), %eax
	movsbl	2(%ecx),%edx
	movsbl	2(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2309
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2309
	movl	%edx, %eax
	.p2align 4,,15
.L2309:
	movb	%al, 2(%ecx)
	movl	4(%esp), %eax
	movsbl	3(%ecx),%edx
	movsbl	3(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2314
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2314
	movl	%edx, %eax
	.p2align 4,,15
.L2314:
	movb	%al, 3(%ecx)
	movl	4(%esp), %eax
	movsbl	4(%ecx),%edx
	movsbl	4(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2319
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2319
	movl	%edx, %eax
	.p2align 4,,15
.L2319:
	movb	%al, 4(%ecx)
	movl	4(%esp), %eax
	movsbl	5(%ecx),%edx
	movsbl	5(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2324
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2324
	movl	%edx, %eax
	.p2align 4,,15
.L2324:
	movb	%al, 5(%ecx)
	movl	4(%esp), %eax
	movsbl	6(%ecx),%edx
	movsbl	6(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2329
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2329
	movl	%edx, %eax
	.p2align 4,,15
.L2329:
	movb	%al, 6(%ecx)
	movl	4(%esp), %eax
	movsbl	7(%ecx),%edx
	movsbl	7(%eax),%eax
	subl	%eax, %edx
	cmpl	$-128, %edx
	movl	$-128, %eax
	jl	.L2334
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2334
	movl	%edx, %eax
	.p2align 4,,15
.L2334:
	movb	%al, 7(%ecx)
	addl	$8, %esp
	ret
	.size	op_psubsb_mmx, .-op_psubsb_mmx
	.p2align 4,,15
.globl op_paddusw_mmx
	.type	op_paddusw_mmx, @function
op_paddusw_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzwl	(%ecx), %edx
	movzwl	(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L2340
	movl	4(%esp), %edx
.L2340:
	movw	%dx, (%ecx)
	movl	8(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L2345
	movl	%eax, %edx
.L2345:
	movw	%dx, 2(%ecx)
	movl	8(%esp), %eax
	movzwl	4(%ecx), %edx
	movzwl	4(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L2350
	movl	4(%esp), %edx
.L2350:
	movw	%dx, 4(%ecx)
	movl	8(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L2355
	movl	%eax, %edx
.L2355:
	movw	%dx, 6(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddusw_mmx, .-op_paddusw_mmx
	.p2align 4,,15
.globl op_paddsw_mmx
	.type	op_paddsw_mmx, @function
op_paddsw_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movswl	(%eax),%eax
	movswl	(%ecx),%edx
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L2361
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2361
	movl	%eax, %edx
	.p2align 4,,15
.L2361:
	movw	%dx, (%ecx)
	movl	8(%esp), %eax
	movswl	2(%ecx),%edx
	movswl	2(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L2366
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2366
	movl	%eax, %edx
	.p2align 4,,15
.L2366:
	movw	%dx, 2(%ecx)
	movl	8(%esp), %eax
	movswl	4(%ecx),%edx
	movswl	4(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L2371
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2371
	movl	%eax, %edx
	.p2align 4,,15
.L2371:
	movw	%dx, 4(%ecx)
	movl	8(%esp), %eax
	movswl	6(%ecx),%edx
	movswl	6(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L2376
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2376
	movl	%eax, %edx
	.p2align 4,,15
.L2376:
	movw	%dx, 6(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddsw_mmx, .-op_paddsw_mmx
	.p2align 4,,15
.globl op_psubusw_mmx
	.type	op_psubusw_mmx, @function
op_psubusw_mmx:
	subl	$24, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 20(%esp)
	movzwl	(%eax), %eax
	movzwl	(%ecx), %edx
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 16(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2382
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 16(%esp)
	jg	.L2382
	movl	%eax, 16(%esp)
.L2382:
	movzwl	2(%ecx), %edx
	movl	16(%esp), %eax
	movw	%ax, (%ecx)
	movl	20(%esp), %eax
	movzwl	2(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2387
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 12(%esp)
	jg	.L2387
	movl	%eax, 12(%esp)
.L2387:
	movzwl	4(%ecx), %edx
	movl	12(%esp), %eax
	movw	%ax, 2(%ecx)
	movl	20(%esp), %eax
	movzwl	4(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2392
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 8(%esp)
	jg	.L2392
	movl	%eax, 8(%esp)
.L2392:
	movzwl	6(%ecx), %edx
	movl	8(%esp), %eax
	movw	%ax, 4(%ecx)
	movl	20(%esp), %eax
	movzwl	6(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2397
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 4(%esp)
	jg	.L2397
	movl	%eax, 4(%esp)
.L2397:
	movl	4(%esp), %eax
	movw	%ax, 6(%ecx)
	addl	$24, %esp
	ret
	.size	op_psubusw_mmx, .-op_psubusw_mmx
	.p2align 4,,15
.globl op_psubsw_mmx
	.type	op_psubsw_mmx, @function
op_psubsw_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movswl	(%eax),%eax
	movswl	(%ecx),%edx
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L2403
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L2403
	movl	%edx, %eax
	.p2align 4,,15
.L2403:
	movw	%ax, (%ecx)
	movl	4(%esp), %eax
	movswl	2(%ecx),%edx
	movswl	2(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L2408
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L2408
	movl	%edx, %eax
	.p2align 4,,15
.L2408:
	movw	%ax, 2(%ecx)
	movl	4(%esp), %eax
	movswl	4(%ecx),%edx
	movswl	4(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L2413
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L2413
	movl	%edx, %eax
	.p2align 4,,15
.L2413:
	movw	%ax, 4(%ecx)
	movl	4(%esp), %eax
	movswl	6(%ecx),%edx
	movswl	6(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L2418
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L2418
	movl	%edx, %eax
	.p2align 4,,15
.L2418:
	movw	%ax, 6(%ecx)
	addl	$8, %esp
	ret
	.size	op_psubsw_mmx, .-op_psubsw_mmx
	.p2align 4,,15
.globl op_pminub_mmx
	.type	op_pminub_mmx, @function
op_pminub_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	(%edx), %eax
	movb	%al, 7(%esp)
	movzbl	(%ecx), %eax
	cmpb	7(%esp), %al
	jbe	.L2423
	movzbl	7(%esp), %eax
.L2423:
	movb	%al, (%edx)
	movzbl	1(%edx), %eax
	movb	%al, 6(%esp)
	movzbl	1(%ecx), %eax
	cmpb	6(%esp), %al
	jbe	.L2424
	movzbl	6(%esp), %eax
.L2424:
	movb	%al, 1(%edx)
	movzbl	2(%edx), %eax
	movb	%al, 5(%esp)
	movzbl	2(%ecx), %eax
	cmpb	5(%esp), %al
	jbe	.L2425
	movzbl	5(%esp), %eax
.L2425:
	movb	%al, 2(%edx)
	movzbl	3(%edx), %eax
	movb	%al, 4(%esp)
	movzbl	3(%ecx), %eax
	cmpb	4(%esp), %al
	jbe	.L2426
	movzbl	4(%esp), %eax
.L2426:
	movb	%al, 3(%edx)
	movzbl	4(%edx), %eax
	movb	%al, 3(%esp)
	movzbl	4(%ecx), %eax
	cmpb	3(%esp), %al
	jbe	.L2427
	movzbl	3(%esp), %eax
.L2427:
	movb	%al, 4(%edx)
	movzbl	5(%edx), %eax
	movb	%al, 2(%esp)
	movzbl	5(%ecx), %eax
	cmpb	2(%esp), %al
	jbe	.L2428
	movzbl	2(%esp), %eax
.L2428:
	movb	%al, 5(%edx)
	movzbl	6(%edx), %eax
	movb	%al, 1(%esp)
	movzbl	6(%ecx), %eax
	cmpb	1(%esp), %al
	jbe	.L2429
	movzbl	1(%esp), %eax
.L2429:
	movb	%al, 6(%edx)
	movzbl	7(%edx), %eax
	movb	%al, (%esp)
	movzbl	7(%ecx), %eax
	cmpb	(%esp), %al
	jbe	.L2430
	movzbl	(%esp), %eax
.L2430:
	movb	%al, 7(%edx)
	addl	$8, %esp
	ret
	.size	op_pminub_mmx, .-op_pminub_mmx
	.p2align 4,,15
.globl op_pmaxub_mmx
	.type	op_pmaxub_mmx, @function
op_pmaxub_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	(%edx), %eax
	movb	%al, 7(%esp)
	movzbl	(%ecx), %eax
	cmpb	7(%esp), %al
	jae	.L2432
	movzbl	7(%esp), %eax
.L2432:
	movb	%al, (%edx)
	movzbl	1(%edx), %eax
	movb	%al, 6(%esp)
	movzbl	1(%ecx), %eax
	cmpb	6(%esp), %al
	jae	.L2433
	movzbl	6(%esp), %eax
.L2433:
	movb	%al, 1(%edx)
	movzbl	2(%edx), %eax
	movb	%al, 5(%esp)
	movzbl	2(%ecx), %eax
	cmpb	5(%esp), %al
	jae	.L2434
	movzbl	5(%esp), %eax
.L2434:
	movb	%al, 2(%edx)
	movzbl	3(%edx), %eax
	movb	%al, 4(%esp)
	movzbl	3(%ecx), %eax
	cmpb	4(%esp), %al
	jae	.L2435
	movzbl	4(%esp), %eax
.L2435:
	movb	%al, 3(%edx)
	movzbl	4(%edx), %eax
	movb	%al, 3(%esp)
	movzbl	4(%ecx), %eax
	cmpb	3(%esp), %al
	jae	.L2436
	movzbl	3(%esp), %eax
.L2436:
	movb	%al, 4(%edx)
	movzbl	5(%edx), %eax
	movb	%al, 2(%esp)
	movzbl	5(%ecx), %eax
	cmpb	2(%esp), %al
	jae	.L2437
	movzbl	2(%esp), %eax
.L2437:
	movb	%al, 5(%edx)
	movzbl	6(%edx), %eax
	movb	%al, 1(%esp)
	movzbl	6(%ecx), %eax
	cmpb	1(%esp), %al
	jae	.L2438
	movzbl	1(%esp), %eax
.L2438:
	movb	%al, 6(%edx)
	movzbl	7(%edx), %eax
	movb	%al, (%esp)
	movzbl	7(%ecx), %eax
	cmpb	(%esp), %al
	jae	.L2439
	movzbl	(%esp), %eax
.L2439:
	movb	%al, 7(%edx)
	addl	$8, %esp
	ret
	.size	op_pmaxub_mmx, .-op_pmaxub_mmx
	.p2align 4,,15
.globl op_pminsw_mmx
	.type	op_pminsw_mmx, @function
op_pminsw_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, 4(%esp)
	movzwl	(%edx), %ecx
	movw	%cx, 2(%esp)
	movzwl	(%eax), %eax
	cmpw	2(%esp), %ax
	jle	.L2441
	movzwl	2(%esp), %eax
.L2441:
	movw	%ax, (%edx)
	movl	4(%esp), %ecx
	movzwl	2(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	2(%ecx), %eax
	cmpw	2(%esp), %ax
	jle	.L2442
	movzwl	2(%esp), %eax
.L2442:
	movw	%ax, 2(%edx)
	movl	4(%esp), %ecx
	movzwl	4(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	4(%ecx), %eax
	cmpw	2(%esp), %ax
	jle	.L2443
	movzwl	2(%esp), %eax
.L2443:
	movw	%ax, 4(%edx)
	movl	4(%esp), %ecx
	movzwl	6(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	6(%ecx), %eax
	cmpw	2(%esp), %ax
	jle	.L2444
	movzwl	2(%esp), %eax
.L2444:
	movw	%ax, 6(%edx)
	addl	$8, %esp
	ret
	.size	op_pminsw_mmx, .-op_pminsw_mmx
	.p2align 4,,15
.globl op_pmaxsw_mmx
	.type	op_pmaxsw_mmx, @function
op_pmaxsw_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, 4(%esp)
	movzwl	(%edx), %ecx
	movw	%cx, 2(%esp)
	movzwl	(%eax), %eax
	cmpw	2(%esp), %ax
	jge	.L2446
	movzwl	2(%esp), %eax
.L2446:
	movw	%ax, (%edx)
	movl	4(%esp), %ecx
	movzwl	2(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	2(%ecx), %eax
	cmpw	2(%esp), %ax
	jge	.L2447
	movzwl	2(%esp), %eax
.L2447:
	movw	%ax, 2(%edx)
	movl	4(%esp), %ecx
	movzwl	4(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	4(%ecx), %eax
	cmpw	2(%esp), %ax
	jge	.L2448
	movzwl	2(%esp), %eax
.L2448:
	movw	%ax, 4(%edx)
	movl	4(%esp), %ecx
	movzwl	6(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	6(%ecx), %eax
	cmpw	2(%esp), %ax
	jge	.L2449
	movzwl	2(%esp), %eax
.L2449:
	movw	%ax, 6(%edx)
	addl	$8, %esp
	ret
	.size	op_pmaxsw_mmx, .-op_pmaxsw_mmx
	.p2align 4,,15
.globl op_pand_mmx
	.type	op_pand_mmx, @function
op_pand_mmx:
	leal	__op_param1(%ebp), %eax
	movl	__op_param2(%ebp), %ecx
	movl	(%eax), %edx
	andl	%ecx, %edx
	movl	4(%eax), %ecx
	andl	__op_param2+4(%ebp), %ecx
	movl	%edx, (%eax)
	movl	%ecx, 4(%eax)
	ret
	.size	op_pand_mmx, .-op_pand_mmx
	.p2align 4,,15
.globl op_pandn_mmx
	.type	op_pandn_mmx, @function
op_pandn_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	movl	(%eax), %edx
	notl	%edx
	movl	%edx, (%esp)
	movl	4(%eax), %ecx
	andl	__op_param2(%ebp), %edx
	notl	%ecx
	movl	%ecx, 4(%esp)
	andl	__op_param2+4(%ebp), %ecx
	movl	%edx, (%eax)
	movl	%ecx, 4(%eax)
	addl	$8, %esp
	ret
	.size	op_pandn_mmx, .-op_pandn_mmx
	.p2align 4,,15
.globl op_por_mmx
	.type	op_por_mmx, @function
op_por_mmx:
	leal	__op_param1(%ebp), %eax
	movl	__op_param2(%ebp), %ecx
	movl	(%eax), %edx
	orl	%ecx, %edx
	movl	4(%eax), %ecx
	orl	__op_param2+4(%ebp), %ecx
	movl	%edx, (%eax)
	movl	%ecx, 4(%eax)
	ret
	.size	op_por_mmx, .-op_por_mmx
	.p2align 4,,15
.globl op_pxor_mmx
	.type	op_pxor_mmx, @function
op_pxor_mmx:
	leal	__op_param1(%ebp), %eax
	movl	__op_param2(%ebp), %ecx
	movl	(%eax), %edx
	xorl	%ecx, %edx
	movl	4(%eax), %ecx
	xorl	__op_param2+4(%ebp), %ecx
	movl	%edx, (%eax)
	movl	%ecx, 4(%eax)
	ret
	.size	op_pxor_mmx, .-op_pxor_mmx
	.p2align 4,,15
.globl op_pcmpgtb_mmx
	.type	op_pcmpgtb_mmx, @function
op_pcmpgtb_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzbl	(%ecx), %eax
	cmpb	%al, (%edx)
	setle	%al
	decb	%al
	movb	%al, (%edx)
	movzbl	1(%ecx), %eax
	cmpb	%al, 1(%edx)
	setle	%al
	decb	%al
	movb	%al, 1(%edx)
	movzbl	2(%ecx), %eax
	cmpb	%al, 2(%edx)
	setle	%al
	decb	%al
	movb	%al, 2(%edx)
	movzbl	3(%ecx), %eax
	cmpb	%al, 3(%edx)
	setle	%al
	decb	%al
	movb	%al, 3(%edx)
	movzbl	4(%ecx), %eax
	cmpb	%al, 4(%edx)
	setle	%al
	decb	%al
	movb	%al, 4(%edx)
	movzbl	5(%ecx), %eax
	cmpb	%al, 5(%edx)
	setle	%al
	decb	%al
	movb	%al, 5(%edx)
	movzbl	6(%ecx), %eax
	cmpb	%al, 6(%edx)
	setle	%al
	decb	%al
	movb	%al, 6(%edx)
	movzbl	7(%ecx), %eax
	cmpb	%al, 7(%edx)
	setle	%al
	decb	%al
	movb	%al, 7(%edx)
	ret
	.size	op_pcmpgtb_mmx, .-op_pcmpgtb_mmx
	.p2align 4,,15
.globl op_pcmpgtw_mmx
	.type	op_pcmpgtw_mmx, @function
op_pcmpgtw_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzwl	(%ecx), %eax
	cmpw	%ax, (%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, (%edx)
	movzwl	2(%ecx), %eax
	cmpw	%ax, 2(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%edx)
	movzwl	4(%ecx), %eax
	cmpw	%ax, 4(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%edx)
	movzwl	6(%ecx), %eax
	cmpw	%ax, 6(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%edx)
	ret
	.size	op_pcmpgtw_mmx, .-op_pcmpgtw_mmx
	.p2align 4,,15
.globl op_pcmpgtl_mmx
	.type	op_pcmpgtl_mmx, @function
op_pcmpgtl_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movl	(%ecx), %eax
	cmpl	%eax, (%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, (%edx)
	movl	4(%ecx), %eax
	cmpl	%eax, 4(%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%edx)
	ret
	.size	op_pcmpgtl_mmx, .-op_pcmpgtl_mmx
	.p2align 4,,15
.globl op_pcmpeqb_mmx
	.type	op_pcmpeqb_mmx, @function
op_pcmpeqb_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzbl	(%ecx), %eax
	cmpb	%al, (%edx)
	setne	%al
	decb	%al
	movb	%al, (%edx)
	movzbl	1(%ecx), %eax
	cmpb	%al, 1(%edx)
	setne	%al
	decb	%al
	movb	%al, 1(%edx)
	movzbl	2(%ecx), %eax
	cmpb	%al, 2(%edx)
	setne	%al
	decb	%al
	movb	%al, 2(%edx)
	movzbl	3(%ecx), %eax
	cmpb	%al, 3(%edx)
	setne	%al
	decb	%al
	movb	%al, 3(%edx)
	movzbl	4(%ecx), %eax
	cmpb	%al, 4(%edx)
	setne	%al
	decb	%al
	movb	%al, 4(%edx)
	movzbl	5(%ecx), %eax
	cmpb	%al, 5(%edx)
	setne	%al
	decb	%al
	movb	%al, 5(%edx)
	movzbl	6(%ecx), %eax
	cmpb	%al, 6(%edx)
	setne	%al
	decb	%al
	movb	%al, 6(%edx)
	movzbl	7(%ecx), %eax
	cmpb	%al, 7(%edx)
	setne	%al
	decb	%al
	movb	%al, 7(%edx)
	ret
	.size	op_pcmpeqb_mmx, .-op_pcmpeqb_mmx
	.p2align 4,,15
.globl op_pcmpeqw_mmx
	.type	op_pcmpeqw_mmx, @function
op_pcmpeqw_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzwl	(%ecx), %eax
	cmpw	%ax, (%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, (%edx)
	movzwl	2(%ecx), %eax
	cmpw	%ax, 2(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%edx)
	movzwl	4(%ecx), %eax
	cmpw	%ax, 4(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%edx)
	movzwl	6(%ecx), %eax
	cmpw	%ax, 6(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%edx)
	ret
	.size	op_pcmpeqw_mmx, .-op_pcmpeqw_mmx
	.p2align 4,,15
.globl op_pcmpeql_mmx
	.type	op_pcmpeql_mmx, @function
op_pcmpeql_mmx:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movl	(%ecx), %eax
	cmpl	%eax, (%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, (%edx)
	movl	4(%ecx), %eax
	cmpl	%eax, 4(%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%edx)
	ret
	.size	op_pcmpeql_mmx, .-op_pcmpeql_mmx
	.p2align 4,,15
.globl op_pmullw_mmx
	.type	op_pmullw_mmx, @function
op_pmullw_mmx:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movzwl	(%eax), %edx
	movzwl	(%ecx), %eax
	imull	%edx, %eax
	movw	%ax, (%ecx)
	movl	(%esp), %eax
	movzwl	2(%eax), %edx
	movzwl	2(%ecx), %eax
	imull	%edx, %eax
	movw	%ax, 2(%ecx)
	movl	(%esp), %eax
	movzwl	4(%eax), %edx
	movzwl	4(%ecx), %eax
	imull	%edx, %eax
	movw	%ax, 4(%ecx)
	movl	(%esp), %eax
	movzwl	6(%eax), %edx
	movzwl	6(%ecx), %eax
	imull	%edx, %eax
	movw	%ax, 6(%ecx)
	popl	%eax
	ret
	.size	op_pmullw_mmx, .-op_pmullw_mmx
	.p2align 4,,15
.globl op_pmulhuw_mmx
	.type	op_pmulhuw_mmx, @function
op_pmulhuw_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movzwl	(%ecx), %eax
	movzwl	(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, (%ecx)
	movl	4(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%ecx)
	movl	4(%esp), %edx
	movzwl	4(%ecx), %eax
	movzwl	4(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%ecx)
	movl	4(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%ecx)
	addl	$8, %esp
	ret
	.size	op_pmulhuw_mmx, .-op_pmulhuw_mmx
	.p2align 4,,15
.globl op_pmulhw_mmx
	.type	op_pmulhw_mmx, @function
op_pmulhw_mmx:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movswl	(%ecx),%eax
	movswl	(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, (%ecx)
	movl	4(%esp), %edx
	movswl	2(%ecx),%eax
	movswl	2(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%ecx)
	movl	4(%esp), %edx
	movswl	4(%ecx),%eax
	movswl	4(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%ecx)
	movl	4(%esp), %edx
	movswl	6(%ecx),%eax
	movswl	6(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%ecx)
	addl	$8, %esp
	ret
	.size	op_pmulhw_mmx, .-op_pmulhw_mmx
	.p2align 4,,15
.globl op_pavgb_mmx
	.type	op_pavgb_mmx, @function
op_pavgb_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzbl	(%eax), %eax
	movzbl	(%ecx), %edx
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movb	%dl, (%ecx)
	movl	8(%esp), %edx
	movzbl	1(%ecx), %eax
	movzbl	1(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 1(%ecx)
	movl	8(%esp), %eax
	movzbl	2(%ecx), %edx
	movzbl	2(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movb	%dl, 2(%ecx)
	movl	8(%esp), %edx
	movzbl	3(%ecx), %eax
	movzbl	3(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 3(%ecx)
	movl	8(%esp), %eax
	movzbl	4(%ecx), %edx
	movzbl	4(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movb	%dl, 4(%ecx)
	movl	8(%esp), %edx
	movzbl	5(%ecx), %eax
	movzbl	5(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 5(%ecx)
	movl	8(%esp), %eax
	movzbl	6(%ecx), %edx
	movzbl	6(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movb	%dl, 6(%ecx)
	movl	8(%esp), %edx
	movzbl	7(%ecx), %eax
	movzbl	7(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 7(%ecx)
	addl	$12, %esp
	ret
	.size	op_pavgb_mmx, .-op_pavgb_mmx
	.p2align 4,,15
.globl op_pavgw_mmx
	.type	op_pavgw_mmx, @function
op_pavgw_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzwl	(%eax), %eax
	movzwl	(%ecx), %edx
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, (%ecx)
	movl	8(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 2(%ecx)
	movl	8(%esp), %eax
	movzwl	4(%ecx), %edx
	movzwl	4(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, 4(%ecx)
	movl	8(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 6(%ecx)
	addl	$12, %esp
	ret
	.size	op_pavgw_mmx, .-op_pavgw_mmx
	.p2align 4,,15
.globl op_pmuludq_mmx
	.type	op_pmuludq_mmx, @function
op_pmuludq_mmx:
	leal	__op_param1(%ebp), %ecx
	movl	(%ecx), %eax
	mull	__op_param2(%ebp)
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	ret
	.size	op_pmuludq_mmx, .-op_pmuludq_mmx
	.p2align 4,,15
.globl op_pmaddwd_mmx
	.type	op_pmaddwd_mmx, @function
op_pmaddwd_mmx:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	movl	%edx, 8(%esp)
	movl	%eax, 16(%esp)
.L2526:
	movl	16(%esp), %eax
	movl	8(%esp), %edx
	movswl	(%edx,%eax,4),%ecx
	movl	12(%esp), %edx
	movswl	(%edx,%eax,4),%edx
	imull	%edx, %ecx
	movl	8(%esp), %edx
	movswl	2(%edx,%eax,4),%edx
	movl	%edx, 4(%esp)
	movl	12(%esp), %edx
	movswl	2(%edx,%eax,4),%edx
	movl	4(%esp), %eax
	imull	%edx, %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	12(%esp), %eax
	movl	%ecx, (%eax,%edx,4)
	incl	%edx
	movl	%edx, 16(%esp)
	decl	%edx
	jle	.L2526
	addl	$20, %esp
	ret
	.size	op_pmaddwd_mmx, .-op_pmaddwd_mmx
	.p2align 4,,15
.globl op_psadbw_mmx
	.type	op_psadbw_mmx, @function
op_psadbw_mmx:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 8(%esp)
	movl	%edx, 4(%esp)
	movzbl	(%edx), %edx
	movzbl	(%eax), %eax
	subl	%edx, %eax
	cltd
	movl	%edx, %ecx
	xorl	%eax, %ecx
	subl	%edx, %ecx
	movl	8(%esp), %edx
	movzbl	1(%edx), %eax
	movl	4(%esp), %edx
	movzbl	1(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	2(%edx), %eax
	movl	4(%esp), %edx
	movzbl	2(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	3(%edx), %eax
	movl	4(%esp), %edx
	movzbl	3(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	4(%edx), %eax
	movl	4(%esp), %edx
	movzbl	4(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	5(%edx), %eax
	movl	4(%esp), %edx
	movzbl	5(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	6(%edx), %eax
	movl	4(%esp), %edx
	movzbl	6(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	7(%edx), %eax
	movl	4(%esp), %edx
	movzbl	7(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %ecx
	movl	8(%esp), %eax
	movl	%ecx, (%eax)
	movl	$0, 4(%eax)
	addl	$12, %esp
	ret
	.size	op_psadbw_mmx, .-op_psadbw_mmx
	.p2align 4,,15
.globl op_maskmov_mmx
	.type	op_maskmov_mmx, @function
op_maskmov_mmx:
	subl	$28, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	xorl	%eax, %eax
	movl	%edx, 16(%esp)
	movl	%eax, 24(%esp)
	.p2align 4,,15
.L2563:
	movl	16(%esp), %eax
	movl	24(%esp), %edx
	cmpb	$0, (%eax,%edx)
	jns	.L2557
	movl	20(%esp), %eax
	leal	(%edi,%edx), %ecx
	movzbl	(%eax,%edx), %eax
	movl	%ecx, %edx
	shrl	$12, %edx
	andl	$255, %edx
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4096, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L2559
	movzbl	12(%esp), %edx
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%ecx, %eax
	call	__stb_mmu
	jmp	.L2557
	.p2align 4,,7
.L2559:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzbl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	.p2align 4,,15
.L2557:
	incl	24(%esp)
	cmpl	$7, 24(%esp)
	jle	.L2563
	addl	$28, %esp
	ret
	.size	op_maskmov_mmx, .-op_maskmov_mmx
	.p2align 4,,15
.globl op_movl_mm_T0_mmx
	.type	op_movl_mm_T0_mmx, @function
op_movl_mm_T0_mmx:
	leal	__op_param1(%ebp), %eax
	movl	%ebx, (%eax)
	movl	$0, 4(%eax)
	ret
	.size	op_movl_mm_T0_mmx, .-op_movl_mm_T0_mmx
	.p2align 4,,15
.globl op_movl_T0_mm_mmx
	.type	op_movl_T0_mm_mmx, @function
op_movl_T0_mm_mmx:
	movl	__op_param1(%ebp), %ebx
	ret
	.size	op_movl_T0_mm_mmx, .-op_movl_T0_mm_mmx
	.p2align 4,,15
.globl op_pshufw_mmx
	.type	op_pshufw_mmx, @function
op_pshufw_mmx:
	movl	$__op_param3, %edx
	subl	$8, %esp
	movl	%edx, %eax
	leal	__op_param2(%ebp), %ecx
	andl	$3, %eax
	movzwl	(%ecx,%eax,2), %eax
	movw	%ax, (%esp)
	movl	%edx, %eax
	sarl	$2, %eax
	andl	$3, %eax
	sarl	$4, %edx
	movzwl	(%ecx,%eax,2), %eax
	movw	%ax, 2(%esp)
	movl	%edx, %eax
	andl	$3, %eax
	movzwl	(%ecx,%eax,2), %eax
	sarl	$2, %edx
	andl	$3, %edx
	movw	%ax, 4(%esp)
	movzwl	(%ecx,%edx,2), %eax
	movw	%ax, 6(%esp)
	movl	4(%esp), %edx
	movl	(%esp), %eax
	movl	%edx, __op_param1+4(%ebp)
	movl	%eax, __op_param1(%ebp)
	addl	$8, %esp
	ret
	.size	op_pshufw_mmx, .-op_pshufw_mmx
	.p2align 4,,15
.globl op_pmovmskb_mmx
	.type	op_pmovmskb_mmx, @function
op_pmovmskb_mmx:
	leal	__op_param1(%ebp), %edx
	movzbl	(%edx), %eax
	shrb	$7, %al
	movzbl	%al, %ebx
	movzbl	1(%edx), %eax
	shrb	$6, %al
	andl	$2, %eax
	orl	%eax, %ebx
	movzbl	2(%edx), %eax
	shrb	$5, %al
	andl	$4, %eax
	orl	%eax, %ebx
	movzbl	3(%edx), %eax
	shrb	$4, %al
	andl	$8, %eax
	orl	%eax, %ebx
	movzbl	4(%edx), %eax
	shrb	$3, %al
	andl	$16, %eax
	orl	%eax, %ebx
	movzbl	5(%edx), %eax
	shrb	$2, %al
	andl	$32, %eax
	orl	%eax, %ebx
	movzbl	6(%edx), %eax
	shrb	%al
	andl	$64, %eax
	orl	%eax, %ebx
	movzbl	7(%edx), %eax
	andl	$128, %eax
	orl	%eax, %ebx
	ret
	.size	op_pmovmskb_mmx, .-op_pmovmskb_mmx
	.p2align 4,,15
.globl op_pinsrw_mmx
	.type	op_pinsrw_mmx, @function
op_pinsrw_mmx:
	movl	$__op_param2, %eax
	movw	%bx, __op_param1(%ebp,%eax,2)
	ret
	.size	op_pinsrw_mmx, .-op_pinsrw_mmx
	.p2align 4,,15
.globl op_pextrw_mmx
	.type	op_pextrw_mmx, @function
op_pextrw_mmx:
	movl	$__op_param2, %eax
	movzwl	__op_param1(%ebp,%eax,2), %ebx
	ret
	.size	op_pextrw_mmx, .-op_pextrw_mmx
	.p2align 4,,15
.globl op_packsswb_mmx
	.type	op_packsswb_mmx, @function
op_packsswb_mmx:
	subl	$12, %esp
	xorl	%eax, %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	movl	$-128, %edx
	movl	%eax, 8(%esp)
	leal	__op_param2(%ebp), %eax
	movl	%eax, (%esp)
	movswl	(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L2574
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2574
	movl	%eax, %edx
	.p2align 4,,15
.L2574:
	movl	4(%esp), %eax
	movzbl	%dl, %edx
	andl	$-256, %eax
	orl	%edx, %eax
	movl	$-128, %edx
	movl	%eax, 4(%esp)
	movswl	2(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L2579
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2579
	movl	%eax, %edx
	.p2align 4,,15
.L2579:
	movl	4(%esp), %eax
	movb	%dl, %ah
	movl	%eax, 4(%esp)
	movl	$-128, %eax
	movswl	4(%ecx),%edx
	cmpl	$-128, %edx
	jl	.L2584
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2584
	movl	%edx, %eax
	.p2align 4,,15
.L2584:
	movl	4(%esp), %edx
	movzbl	%al, %eax
	sall	$16, %eax
	andl	$-16711681, %edx
	orl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$-128, %edx
	movswl	6(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L2589
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2589
	movl	%eax, %edx
	.p2align 4,,15
.L2589:
	movl	4(%esp), %eax
	sall	$24, %edx
	andl	$16777215, %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	movl	%eax, 4(%esp)
	movswl	(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L2594
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2594
	movl	%eax, %edx
	.p2align 4,,15
.L2594:
	movl	8(%esp), %eax
	movzbl	%dl, %edx
	andl	$-256, %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	movl	%eax, 8(%esp)
	movswl	2(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L2599
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2599
	movl	%eax, %edx
	.p2align 4,,15
.L2599:
	movl	8(%esp), %eax
	movb	%dl, %ah
	movl	%eax, 8(%esp)
	movl	(%esp), %eax
	movswl	4(%eax),%edx
	movl	$-128, %eax
	cmpl	$-128, %edx
	jl	.L2604
	cmpl	$127, %edx
	movl	$127, %eax
	jg	.L2604
	movl	%edx, %eax
	.p2align 4,,15
.L2604:
	movl	8(%esp), %edx
	movzbl	%al, %eax
	sall	$16, %eax
	andl	$-16711681, %edx
	orl	%eax, %edx
	movl	%edx, 8(%esp)
	movl	(%esp), %edx
	movswl	6(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L2609
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2609
	movl	%eax, %edx
	.p2align 4,,15
.L2609:
	movl	8(%esp), %eax
	sall	$24, %edx
	andl	$16777215, %eax
	orl	%edx, %eax
	movl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$12, %esp
	ret
	.size	op_packsswb_mmx, .-op_packsswb_mmx
	.p2align 4,,15
.globl op_packuswb_mmx
	.type	op_packuswb_mmx, @function
op_packuswb_mmx:
	subl	$12, %esp
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	movl	%ecx, 4(%esp)
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%edx, 8(%esp)
	xorl	%edx, %edx
	movl	%eax, (%esp)
	movswl	(%ecx),%eax
	testl	%eax, %eax
	js	.L2615
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2615
	movl	%eax, %edx
.L2615:
	movl	4(%esp), %eax
	movzbl	%dl, %edx
	andl	$-256, %eax
	orl	%edx, %eax
	xorl	%edx, %edx
	movl	%eax, 4(%esp)
	movswl	2(%ecx),%eax
	testl	%eax, %eax
	js	.L2620
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2620
	movl	%eax, %edx
.L2620:
	movl	4(%esp), %eax
	movb	%dl, %ah
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	movswl	4(%ecx),%edx
	testl	%edx, %edx
	js	.L2625
	cmpl	$255, %edx
	movl	$255, %eax
	jg	.L2625
	movl	%edx, %eax
.L2625:
	movl	4(%esp), %edx
	movzbl	%al, %eax
	sall	$16, %eax
	andl	$-16711681, %edx
	orl	%eax, %edx
	movl	%edx, 4(%esp)
	xorl	%edx, %edx
	movswl	6(%ecx),%eax
	testl	%eax, %eax
	js	.L2630
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2630
	movl	%eax, %edx
.L2630:
	movl	4(%esp), %eax
	sall	$24, %edx
	andl	$16777215, %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	movl	%eax, 4(%esp)
	movswl	(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L2635
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2635
	movl	%eax, %edx
.L2635:
	movl	8(%esp), %eax
	movzbl	%dl, %edx
	andl	$-256, %eax
	orl	%edx, %eax
	movl	(%esp), %edx
	movl	%eax, 8(%esp)
	movswl	2(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L2640
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2640
	movl	%eax, %edx
.L2640:
	movl	8(%esp), %eax
	movb	%dl, %ah
	movl	%eax, 8(%esp)
	movl	(%esp), %eax
	movswl	4(%eax),%edx
	xorl	%eax, %eax
	testl	%edx, %edx
	js	.L2645
	cmpl	$255, %edx
	movl	$255, %eax
	jg	.L2645
	movl	%edx, %eax
.L2645:
	movl	8(%esp), %edx
	movzbl	%al, %eax
	sall	$16, %eax
	andl	$-16711681, %edx
	orl	%eax, %edx
	movl	%edx, 8(%esp)
	movl	(%esp), %edx
	movswl	6(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L2650
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L2650
	movl	%eax, %edx
.L2650:
	movl	8(%esp), %eax
	sall	$24, %edx
	andl	$16777215, %eax
	orl	%edx, %eax
	movl	%eax, 8(%esp)
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$12, %esp
	ret
	.size	op_packuswb_mmx, .-op_packuswb_mmx
	.p2align 4,,15
.globl op_packssdw_mmx
	.type	op_packssdw_mmx, @function
op_packssdw_mmx:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movl	$-32768, %edx
	movl	(%ecx), %eax
	cmpl	$-32768, %eax
	jl	.L2656
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2656
	movl	%eax, %edx
	.p2align 4,,15
.L2656:
	movw	%dx, 4(%esp)
	movl	4(%ecx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L2661
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2661
	movl	%eax, %edx
	.p2align 4,,15
.L2661:
	leal	4(%esp), %eax
	movw	%dx, 2(%eax)
	movl	(%esp), %edx
	movl	(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L2666
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2666
	movl	%eax, %edx
	.p2align 4,,15
.L2666:
	leal	4(%esp), %eax
	movw	%dx, 4(%eax)
	movl	(%esp), %edx
	movl	4(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L2671
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L2671
	movl	%eax, %edx
	.p2align 4,,15
.L2671:
	leal	4(%esp), %eax
	movw	%dx, 6(%eax)
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$12, %esp
	ret
	.size	op_packssdw_mmx, .-op_packssdw_mmx
	.p2align 4,,15
.globl op_punpcklbw_mmx
	.type	op_punpcklbw_mmx, @function
op_punpcklbw_mmx:
	subl	$76, %esp
	xorl	%ecx, %ecx
	leal	__op_param1(%ebp), %eax
	movl	%ecx, 68(%esp)
	leal	__op_param2(%ebp), %edx
	xorl	%ecx, %ecx
	movl	%edx, 60(%esp)
	movl	%ecx, 72(%esp)
	movl	%eax, 64(%esp)
	movzbl	(%eax), %ecx
	movl	%ecx, 68(%esp)
	movzbl	(%edx), %eax
	movl	%ecx, %edx
	movl	64(%esp), %ecx
	movb	%al, %dh
	movl	%edx, %eax
	andl	$-16711681, %eax
	movl	%edx, 68(%esp)
	movl	64(%esp), %edx
	movzbl	1(%ecx), %ecx
	movl	%ecx, 56(%esp)
	sall	$16, %ecx
	orl	%ecx, %eax
	movl	%eax, 68(%esp)
	movl	60(%esp), %eax
	movl	68(%esp), %ecx
	movzbl	1(%eax), %eax
	andl	$16777215, %ecx
	movl	%eax, 52(%esp)
	sall	$24, %eax
	orl	%eax, %ecx
	movl	%ecx, 68(%esp)
	movl	60(%esp), %ecx
	movzbl	2(%edx), %edx
	movl	%edx, 48(%esp)
	movl	72(%esp), %edx
	movl	48(%esp), %eax
	andl	$-256, %edx
	orl	%eax, %edx
	movl	%edx, %eax
	movl	%edx, 72(%esp)
	movl	64(%esp), %edx
	movzbl	2(%ecx), %ecx
	movb	%cl, %ah
	movl	%eax, %ecx
	movl	%eax, 72(%esp)
	andl	$-16711681, %ecx
	movl	60(%esp), %eax
	movzbl	3(%edx), %edx
	movl	%ecx, 40(%esp)
	movl	%edx, 44(%esp)
	sall	$16, %edx
	orl	%edx, %ecx
	movl	%ecx, 72(%esp)
	movl	%ecx, %edx
	andl	$16777215, %edx
	movzbl	3(%eax), %eax
	movl	%eax, 36(%esp)
	sall	$24, %eax
	movl	%edx, 32(%esp)
	orl	%eax, %edx
	movl	64(%esp), %ecx
	movl	%edx, 72(%esp)
	movl	68(%esp), %eax
	movl	72(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	addl	$76, %esp
	ret
	.size	op_punpcklbw_mmx, .-op_punpcklbw_mmx
	.p2align 4,,15
.globl op_punpcklwd_mmx
	.type	op_punpcklwd_mmx, @function
op_punpcklwd_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %ecx
	leal	__op_param2(%ebp), %edx
	movzwl	(%ecx), %eax
	movw	%ax, (%esp)
	movzwl	(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	2(%ecx), %eax
	movw	%ax, 4(%esp)
	movzwl	2(%edx), %eax
	movw	%ax, 6(%esp)
	movl	4(%esp), %edx
	movl	(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$8, %esp
	ret
	.size	op_punpcklwd_mmx, .-op_punpcklwd_mmx
	.p2align 4,,15
.globl op_punpckldq_mmx
	.type	op_punpckldq_mmx, @function
op_punpckldq_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %ecx
	movl	(%ecx), %eax
	movl	%eax, (%esp)
	movl	__op_param2(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movl	(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$8, %esp
	ret
	.size	op_punpckldq_mmx, .-op_punpckldq_mmx
	.p2align 4,,15
.globl op_punpckhbw_mmx
	.type	op_punpckhbw_mmx, @function
op_punpckhbw_mmx:
	subl	$76, %esp
	xorl	%ecx, %ecx
	leal	__op_param1(%ebp), %eax
	movl	%ecx, 68(%esp)
	leal	__op_param2(%ebp), %edx
	xorl	%ecx, %ecx
	movl	%edx, 60(%esp)
	movl	%ecx, 72(%esp)
	movl	%eax, 64(%esp)
	movzbl	4(%eax), %ecx
	movl	%ecx, 68(%esp)
	movzbl	4(%edx), %eax
	movl	%ecx, %edx
	movl	64(%esp), %ecx
	movb	%al, %dh
	movl	%edx, %eax
	andl	$-16711681, %eax
	movl	%edx, 68(%esp)
	movl	64(%esp), %edx
	movzbl	5(%ecx), %ecx
	movl	%ecx, 56(%esp)
	sall	$16, %ecx
	orl	%ecx, %eax
	movl	%eax, 68(%esp)
	movl	60(%esp), %eax
	movl	68(%esp), %ecx
	movzbl	5(%eax), %eax
	andl	$16777215, %ecx
	movl	%eax, 52(%esp)
	sall	$24, %eax
	orl	%eax, %ecx
	movl	%ecx, 68(%esp)
	movl	60(%esp), %ecx
	movzbl	6(%edx), %edx
	movl	%edx, 48(%esp)
	movl	72(%esp), %edx
	movl	48(%esp), %eax
	andl	$-256, %edx
	orl	%eax, %edx
	movl	%edx, %eax
	movl	%edx, 72(%esp)
	movl	64(%esp), %edx
	movzbl	6(%ecx), %ecx
	movb	%cl, %ah
	movl	%eax, %ecx
	movl	%eax, 72(%esp)
	andl	$-16711681, %ecx
	movl	60(%esp), %eax
	movzbl	7(%edx), %edx
	movl	%ecx, 40(%esp)
	movl	%edx, 44(%esp)
	sall	$16, %edx
	orl	%edx, %ecx
	movl	%ecx, 72(%esp)
	movl	%ecx, %edx
	andl	$16777215, %edx
	movzbl	7(%eax), %eax
	movl	%eax, 36(%esp)
	sall	$24, %eax
	movl	%edx, 32(%esp)
	orl	%eax, %edx
	movl	64(%esp), %ecx
	movl	%edx, 72(%esp)
	movl	68(%esp), %eax
	movl	72(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	addl	$76, %esp
	ret
	.size	op_punpckhbw_mmx, .-op_punpckhbw_mmx
	.p2align 4,,15
.globl op_punpckhwd_mmx
	.type	op_punpckhwd_mmx, @function
op_punpckhwd_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %ecx
	leal	__op_param2(%ebp), %edx
	movzwl	4(%ecx), %eax
	movw	%ax, (%esp)
	movzwl	4(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	6(%ecx), %eax
	movw	%ax, 4(%esp)
	movzwl	6(%edx), %eax
	movw	%ax, 6(%esp)
	movl	4(%esp), %edx
	movl	(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$8, %esp
	ret
	.size	op_punpckhwd_mmx, .-op_punpckhwd_mmx
	.p2align 4,,15
.globl op_punpckhdq_mmx
	.type	op_punpckhdq_mmx, @function
op_punpckhdq_mmx:
	subl	$8, %esp
	leal	__op_param1(%ebp), %ecx
	movl	4(%ecx), %eax
	movl	%eax, (%esp)
	movl	__op_param2+4(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movl	(%esp), %eax
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	addl	$8, %esp
	ret
	.size	op_punpckhdq_mmx, .-op_punpckhdq_mmx
	.p2align 4,,15
.globl op_psrlw_xmm
	.type	op_psrlw_xmm, @function
op_psrlw_xmm:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2683
	cmpl	$15, %eax
	jbe	.L2682
.L2683:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2684
	.p2align 4,,7
.L2682:
	movl	12(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %edx
	movzwl	(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, (%ecx)
	movzwl	2(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 2(%ecx)
	movzwl	4(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 4(%ecx)
	movzwl	6(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 6(%ecx)
	movzwl	8(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 8(%ecx)
	movzwl	10(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 10(%ecx)
	movzwl	12(%ecx), %eax
	movb	%dl, %cl
	sarl	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 12(%ecx)
	movzwl	14(%ecx), %eax
	movb	%dl, %cl
	movl	8(%esp), %edx
	sarl	%cl, %eax
	movw	%ax, 14(%edx)
	.p2align 4,,15
.L2684:
	addl	$16, %esp
	ret
	.size	op_psrlw_xmm, .-op_psrlw_xmm
	.p2align 4,,15
.globl op_psraw_xmm
	.type	op_psraw_xmm, @function
op_psraw_xmm:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	leal	__op_param2(%ebp), %eax
	movl	4(%eax), %ecx
	movl	(%eax), %edx
	cmpl	$0, %ecx
	movl	%edx, 4(%esp)
	movl	%ecx, 8(%esp)
	ja	.L2687
	cmpl	$15, %edx
	jbe	.L2686
.L2687:
	movl	$15, %ecx
	jmp	.L2688
	.p2align 4,,7
.L2686:
	movzbl	(%eax), %ecx
	.p2align 4,,15
.L2688:
	movl	(%esp), %edx
	movswl	(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, (%edx)
	movswl	2(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 2(%edx)
	movswl	4(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 4(%edx)
	movswl	6(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 6(%edx)
	movswl	8(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 8(%edx)
	movswl	10(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 10(%edx)
	movswl	12(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 12(%edx)
	movswl	14(%edx),%eax
	sarl	%cl, %eax
	movw	%ax, 14(%edx)
	addl	$12, %esp
	ret
	.size	op_psraw_xmm, .-op_psraw_xmm
	.p2align 4,,15
.globl op_psllw_xmm
	.type	op_psllw_xmm, @function
op_psllw_xmm:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2691
	cmpl	$15, %eax
	jbe	.L2690
.L2691:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2692
	.p2align 4,,7
.L2690:
	movl	12(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %edx
	movzwl	(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, (%ecx)
	movzwl	2(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 2(%ecx)
	movzwl	4(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 4(%ecx)
	movzwl	6(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 6(%ecx)
	movzwl	8(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 8(%ecx)
	movzwl	10(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 10(%ecx)
	movzwl	12(%ecx), %eax
	movb	%dl, %cl
	sall	%cl, %eax
	movl	8(%esp), %ecx
	movw	%ax, 12(%ecx)
	movzwl	14(%ecx), %eax
	movb	%dl, %cl
	movl	8(%esp), %edx
	sall	%cl, %eax
	movw	%ax, 14(%edx)
	.p2align 4,,15
.L2692:
	addl	$16, %esp
	ret
	.size	op_psllw_xmm, .-op_psllw_xmm
	.p2align 4,,15
.globl op_psrld_xmm
	.type	op_psrld_xmm, @function
op_psrld_xmm:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2695
	cmpl	$31, %eax
	jbe	.L2694
.L2695:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2696
	.p2align 4,,7
.L2694:
	movl	12(%esp), %edx
	movzbl	(%edx), %eax
	movl	8(%esp), %edx
	movb	%al, %cl
	shrl	%cl, (%edx)
	shrl	%cl, 4(%edx)
	shrl	%cl, 8(%edx)
	shrl	%cl, 12(%edx)
	.p2align 4,,15
.L2696:
	addl	$16, %esp
	ret
	.size	op_psrld_xmm, .-op_psrld_xmm
	.p2align 4,,15
.globl op_psrad_xmm
	.type	op_psrad_xmm, @function
op_psrad_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %edx
	movl	%edx, %ecx
	movl	%edx, 8(%esp)
	leal	__op_param1(%ebp), %eax
	movl	4(%ecx), %ecx
	movl	(%edx), %edx
	cmpl	$0, %ecx
	movl	%edx, (%esp)
	movl	%ecx, 4(%esp)
	ja	.L2699
	cmpl	$31, %edx
	jbe	.L2698
.L2699:
	movl	$31, %ecx
	jmp	.L2700
	.p2align 4,,7
.L2698:
	movl	8(%esp), %edx
	movzbl	(%edx), %ecx
	.p2align 4,,15
.L2700:
	sarl	%cl, (%eax)
	sarl	%cl, 4(%eax)
	sarl	%cl, 8(%eax)
	sarl	%cl, 12(%eax)
	addl	$12, %esp
	ret
	.size	op_psrad_xmm, .-op_psrad_xmm
	.p2align 4,,15
.globl op_pslld_xmm
	.type	op_pslld_xmm, @function
op_pslld_xmm:
	subl	$16, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2703
	cmpl	$31, %eax
	jbe	.L2702
.L2703:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2704
	.p2align 4,,7
.L2702:
	movl	12(%esp), %edx
	movzbl	(%edx), %eax
	movl	8(%esp), %edx
	movb	%al, %cl
	sall	%cl, (%edx)
	sall	%cl, 4(%edx)
	sall	%cl, 8(%edx)
	sall	%cl, 12(%edx)
	.p2align 4,,15
.L2704:
	addl	$16, %esp
	ret
	.size	op_pslld_xmm, .-op_pslld_xmm
	.p2align 4,,15
.globl op_psrlq_xmm
	.type	op_psrlq_xmm, @function
op_psrlq_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 16(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2707
	cmpl	$63, %eax
	jbe	.L2706
.L2707:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2708
	.p2align 4,,7
.L2706:
	movl	16(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %eax
	movl	%eax, 12(%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	movzbl	12(%esp), %ecx
	shrdl	%edx, %eax
	shrl	%cl, %edx
	testb	$32, %cl
	je	.L2709
	movl	%edx, %eax
	xorl	%edx, %edx
.L2709:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	8(%esp), %ecx
	movl	12(%ecx), %edx
	movl	8(%ecx), %eax
	movzbl	12(%esp), %ecx
	shrdl	%edx, %eax
	shrl	%cl, %edx
	testb	$32, %cl
	je	.L2710
	movl	%edx, %eax
	xorl	%edx, %edx
.L2710:
	movl	8(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	.p2align 4,,15
.L2708:
	addl	$20, %esp
	ret
	.size	op_psrlq_xmm, .-op_psrlq_xmm
	.p2align 4,,15
.globl op_psllq_xmm
	.type	op_psllq_xmm, @function
op_psllq_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	%edx, 16(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	cmpl	$0, %edx
	movl	%edx, 4(%esp)
	ja	.L2713
	cmpl	$63, %eax
	jbe	.L2712
.L2713:
	movl	8(%esp), %ecx
	movl	$0, (%ecx)
	movl	$0, 4(%ecx)
	movl	$0, 8(%ecx)
	movl	$0, 12(%ecx)
	jmp	.L2714
	.p2align 4,,7
.L2712:
	movl	16(%esp), %eax
	movl	8(%esp), %ecx
	movzbl	(%eax), %eax
	movl	%eax, 12(%esp)
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	movzbl	12(%esp), %ecx
	shldl	%eax, %edx
	sall	%cl, %eax
	testb	$32, %cl
	je	.L2715
	movl	%eax, %edx
	xorl	%eax, %eax
.L2715:
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	8(%esp), %ecx
	movl	8(%ecx), %eax
	movl	12(%ecx), %edx
	movzbl	12(%esp), %ecx
	shldl	%eax, %edx
	sall	%cl, %eax
	testb	$32, %cl
	je	.L2716
	movl	%eax, %edx
	xorl	%eax, %eax
.L2716:
	movl	8(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	.p2align 4,,15
.L2714:
	addl	$20, %esp
	ret
	.size	op_psllq_xmm, .-op_psllq_xmm
	.p2align 4,,15
.globl op_psrldq_xmm
	.type	op_psrldq_xmm, @function
op_psrldq_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	__op_param2(%ebp), %eax
	cmpl	$16, %eax
	movl	%eax, 8(%esp)
	jle	.L2718
	movl	$16, %ecx
	movl	%ecx, 8(%esp)
.L2718:
	movl	8(%esp), %edx
	movl	$16, %eax
	xorl	%ecx, %ecx
	subl	%edx, %eax
	cmpl	$0, %eax
	jle	.L2728
	movl	%eax, 4(%esp)
	.p2align 4,,15
.L2722:
	movl	12(%esp), %eax
	leal	(%ecx,%eax), %edx
	movl	8(%esp), %eax
	incl	%ecx
	movzbl	(%edx,%eax), %eax
	movb	%al, (%edx)
	cmpl	%ecx, 4(%esp)
	jg	.L2722
.L2728:
	movl	8(%esp), %eax
	movl	$16, %ecx
	subl	%eax, %ecx
	.p2align 4,,15
.L2731:
	cmpl	$15, %ecx
	jg	.L2730
	movl	12(%esp), %eax
	movb	$0, (%eax,%ecx)
	incl	%ecx
	jmp	.L2731
	.p2align 4,,7
.L2730:
	addl	$16, %esp
	ret
	.size	op_psrldq_xmm, .-op_psrldq_xmm
	.p2align 4,,15
.globl op_pslldq_xmm
	.type	op_pslldq_xmm, @function
op_pslldq_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	movl	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	cmpl	$16, %edx
	movl	%edx, 8(%esp)
	jle	.L2733
	movl	$16, %ecx
	movl	%ecx, 8(%esp)
.L2733:
	movl	$15, %ecx
	cmpl	8(%esp), %ecx
	jl	.L2743
	movl	12(%esp), %eax
	movl	8(%esp), %edx
	subl	%edx, %eax
	addl	$15, %eax
	movl	%eax, 4(%esp)
	.p2align 4,,15
.L2737:
	movl	4(%esp), %eax
	movl	12(%esp), %edx
	movzbl	(%eax), %eax
	movb	%al, (%edx,%ecx)
	decl	%ecx
	decl	4(%esp)
	cmpl	8(%esp), %ecx
	jge	.L2737
.L2743:
	xorl	%ecx, %ecx
	.p2align 4,,15
.L2746:
	cmpl	8(%esp), %ecx
	jge	.L2745
	movl	12(%esp), %eax
	movb	$0, (%eax,%ecx)
	incl	%ecx
	jmp	.L2746
	.p2align 4,,7
.L2745:
	addl	$16, %esp
	ret
	.size	op_pslldq_xmm, .-op_pslldq_xmm
	.p2align 4,,15
.globl op_paddb_xmm
	.type	op_paddb_xmm, @function
op_paddb_xmm:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movzbl	(%edx), %ecx
	addb	%cl, (%eax)
	movzbl	1(%edx), %ecx
	addb	%cl, 1(%eax)
	movzbl	2(%edx), %ecx
	addb	%cl, 2(%eax)
	movzbl	3(%edx), %ecx
	addb	%cl, 3(%eax)
	movzbl	4(%edx), %ecx
	addb	%cl, 4(%eax)
	movzbl	5(%edx), %ecx
	addb	%cl, 5(%eax)
	movzbl	6(%edx), %ecx
	addb	%cl, 6(%eax)
	movzbl	7(%edx), %ecx
	addb	%cl, 7(%eax)
	movzbl	8(%edx), %ecx
	addb	%cl, 8(%eax)
	movzbl	9(%edx), %ecx
	addb	%cl, 9(%eax)
	movzbl	10(%edx), %ecx
	addb	%cl, 10(%eax)
	movzbl	11(%edx), %ecx
	addb	%cl, 11(%eax)
	movzbl	12(%edx), %ecx
	addb	%cl, 12(%eax)
	movzbl	13(%edx), %ecx
	addb	%cl, 13(%eax)
	movzbl	14(%edx), %ecx
	addb	%cl, 14(%eax)
	movzbl	15(%edx), %edx
	addb	%dl, 15(%eax)
	ret
	.size	op_paddb_xmm, .-op_paddb_xmm
	.p2align 4,,15
.globl op_paddw_xmm
	.type	op_paddw_xmm, @function
op_paddw_xmm:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, (%esp)
	movzwl	(%eax), %ecx
	movzwl	(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, (%edx)
	movl	(%esp), %eax
	movzwl	2(%eax), %ecx
	movzwl	2(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 2(%edx)
	movl	(%esp), %eax
	movzwl	4(%eax), %ecx
	movzwl	4(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 4(%edx)
	movl	(%esp), %eax
	movzwl	6(%eax), %ecx
	movzwl	6(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 6(%edx)
	movl	(%esp), %eax
	movzwl	8(%eax), %ecx
	movzwl	8(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 8(%edx)
	movl	(%esp), %eax
	movzwl	10(%eax), %ecx
	movzwl	10(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 10(%edx)
	movl	(%esp), %eax
	movzwl	12(%eax), %ecx
	movzwl	12(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 12(%edx)
	movl	(%esp), %eax
	movzwl	14(%eax), %ecx
	movzwl	14(%edx), %eax
	addl	%ecx, %eax
	movw	%ax, 14(%edx)
	popl	%eax
	ret
	.size	op_paddw_xmm, .-op_paddw_xmm
	.p2align 4,,15
.globl op_paddl_xmm
	.type	op_paddl_xmm, @function
op_paddl_xmm:
	leal	__op_param2(%ebp), %ecx
	movl	(%ecx), %edx
	leal	__op_param1(%ebp), %eax
	addl	%edx, (%eax)
	movl	4(%ecx), %edx
	addl	%edx, 4(%eax)
	movl	8(%ecx), %edx
	addl	%edx, 8(%eax)
	movl	12(%ecx), %edx
	addl	%edx, 12(%eax)
	ret
	.size	op_paddl_xmm, .-op_paddl_xmm
	.p2align 4,,15
.globl op_paddq_xmm
	.type	op_paddq_xmm, @function
op_paddq_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movl	%eax, %edx
	movl	(%eax), %eax
	movl	4(%edx), %edx
	addl	%eax, (%ecx)
	adcl	%edx, 4(%ecx)
	movl	8(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	addl	%eax, 8(%ecx)
	adcl	%edx, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_paddq_xmm, .-op_paddq_xmm
	.p2align 4,,15
.globl op_psubb_xmm
	.type	op_psubb_xmm, @function
op_psubb_xmm:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	(%edx), %eax
	subb	(%ecx), %al
	movb	%al, (%edx)
	movzbl	1(%edx), %eax
	subb	1(%ecx), %al
	movb	%al, 1(%edx)
	movzbl	2(%edx), %eax
	subb	2(%ecx), %al
	movb	%al, 2(%edx)
	movzbl	3(%edx), %eax
	subb	3(%ecx), %al
	movb	%al, 3(%edx)
	movzbl	4(%edx), %eax
	subb	4(%ecx), %al
	movb	%al, 4(%edx)
	movzbl	5(%edx), %eax
	subb	5(%ecx), %al
	movb	%al, 5(%edx)
	movzbl	6(%edx), %eax
	subb	6(%ecx), %al
	movb	%al, 6(%edx)
	movzbl	7(%edx), %eax
	subb	7(%ecx), %al
	movb	%al, 7(%edx)
	movzbl	8(%edx), %eax
	subb	8(%ecx), %al
	movb	%al, 8(%edx)
	movzbl	9(%edx), %eax
	subb	9(%ecx), %al
	movb	%al, 9(%edx)
	movzbl	10(%edx), %eax
	subb	10(%ecx), %al
	movb	%al, 10(%edx)
	movzbl	11(%edx), %eax
	subb	11(%ecx), %al
	movb	%al, 11(%edx)
	movzbl	12(%edx), %eax
	subb	12(%ecx), %al
	movb	%al, 12(%edx)
	movzbl	13(%edx), %eax
	subb	13(%ecx), %al
	movb	%al, 13(%edx)
	movzbl	14(%edx), %eax
	subb	14(%ecx), %al
	movb	%al, 14(%edx)
	movzbl	15(%edx), %eax
	subb	15(%ecx), %al
	movb	%al, 15(%edx)
	ret
	.size	op_psubb_xmm, .-op_psubb_xmm
	.p2align 4,,15
.globl op_psubw_xmm
	.type	op_psubw_xmm, @function
op_psubw_xmm:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, (%esp)
	movzwl	(%eax), %ecx
	movzwl	(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, (%edx)
	movl	(%esp), %eax
	movzwl	2(%eax), %ecx
	movzwl	2(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 2(%edx)
	movl	(%esp), %eax
	movzwl	4(%eax), %ecx
	movzwl	4(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 4(%edx)
	movl	(%esp), %eax
	movzwl	6(%eax), %ecx
	movzwl	6(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 6(%edx)
	movl	(%esp), %eax
	movzwl	8(%eax), %ecx
	movzwl	8(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 8(%edx)
	movl	(%esp), %eax
	movzwl	10(%eax), %ecx
	movzwl	10(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 10(%edx)
	movl	(%esp), %eax
	movzwl	12(%eax), %ecx
	movzwl	12(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 12(%edx)
	movl	(%esp), %eax
	movzwl	14(%eax), %ecx
	movzwl	14(%edx), %eax
	subl	%ecx, %eax
	movw	%ax, 14(%edx)
	popl	%eax
	ret
	.size	op_psubw_xmm, .-op_psubw_xmm
	.p2align 4,,15
.globl op_psubl_xmm
	.type	op_psubl_xmm, @function
op_psubl_xmm:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movl	(%edx), %eax
	subl	(%ecx), %eax
	movl	%eax, (%edx)
	movl	4(%edx), %eax
	subl	4(%ecx), %eax
	movl	%eax, 4(%edx)
	movl	8(%edx), %eax
	subl	8(%ecx), %eax
	movl	%eax, 8(%edx)
	movl	12(%edx), %eax
	subl	12(%ecx), %eax
	movl	%eax, 12(%edx)
	ret
	.size	op_psubl_xmm, .-op_psubl_xmm
	.p2align 4,,15
.globl op_psubq_xmm
	.type	op_psubq_xmm, @function
op_psubq_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movl	4(%ecx), %edx
	movl	(%ecx), %eax
	movl	%edx, 4(%esp)
	movl	8(%esp), %edx
	movl	%eax, (%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	subl	%eax, (%esp)
	movl	(%esp), %eax
	sbbl	%edx, 4(%esp)
	movl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	8(%ecx), %eax
	movl	%edx, 4(%ecx)
	movl	12(%ecx), %edx
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	movl	8(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	subl	%eax, (%esp)
	movl	(%esp), %eax
	sbbl	%edx, 4(%esp)
	movl	4(%esp), %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$12, %esp
	ret
	.size	op_psubq_xmm, .-op_psubq_xmm
	.p2align 4,,15
.globl op_paddusb_xmm
	.type	op_paddusb_xmm, @function
op_paddusb_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzbl	(%ecx), %edx
	movzbl	(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2757
	movl	4(%esp), %edx
.L2757:
	movb	%dl, (%ecx)
	movl	8(%esp), %edx
	movzbl	1(%ecx), %eax
	movzbl	1(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2762
	movl	%eax, %edx
.L2762:
	movb	%dl, 1(%ecx)
	movl	8(%esp), %eax
	movzbl	2(%ecx), %edx
	movzbl	2(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2767
	movl	4(%esp), %edx
.L2767:
	movb	%dl, 2(%ecx)
	movl	8(%esp), %edx
	movzbl	3(%ecx), %eax
	movzbl	3(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2772
	movl	%eax, %edx
.L2772:
	movb	%dl, 3(%ecx)
	movl	8(%esp), %eax
	movzbl	4(%ecx), %edx
	movzbl	4(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2777
	movl	4(%esp), %edx
.L2777:
	movb	%dl, 4(%ecx)
	movl	8(%esp), %edx
	movzbl	5(%ecx), %eax
	movzbl	5(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2782
	movl	%eax, %edx
.L2782:
	movb	%dl, 5(%ecx)
	movl	8(%esp), %eax
	movzbl	6(%ecx), %edx
	movzbl	6(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2787
	movl	4(%esp), %edx
.L2787:
	movb	%dl, 6(%ecx)
	movl	8(%esp), %edx
	movzbl	7(%ecx), %eax
	movzbl	7(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2792
	movl	%eax, %edx
.L2792:
	movb	%dl, 7(%ecx)
	movl	8(%esp), %eax
	movzbl	8(%ecx), %edx
	movzbl	8(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2797
	movl	4(%esp), %edx
.L2797:
	movb	%dl, 8(%ecx)
	movl	8(%esp), %edx
	movzbl	9(%ecx), %eax
	movzbl	9(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2802
	movl	%eax, %edx
.L2802:
	movb	%dl, 9(%ecx)
	movl	8(%esp), %eax
	movzbl	10(%ecx), %edx
	movzbl	10(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2807
	movl	4(%esp), %edx
.L2807:
	movb	%dl, 10(%ecx)
	movl	8(%esp), %edx
	movzbl	11(%ecx), %eax
	movzbl	11(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2812
	movl	%eax, %edx
.L2812:
	movb	%dl, 11(%ecx)
	movl	8(%esp), %eax
	movzbl	12(%ecx), %edx
	movzbl	12(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2817
	movl	4(%esp), %edx
.L2817:
	movb	%dl, 12(%ecx)
	movl	8(%esp), %edx
	movzbl	13(%ecx), %eax
	movzbl	13(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2822
	movl	%eax, %edx
.L2822:
	movb	%dl, 13(%ecx)
	movl	8(%esp), %eax
	movzbl	14(%ecx), %edx
	movzbl	14(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$255, %edx
	cmpl	$255, 4(%esp)
	jg	.L2827
	movl	4(%esp), %edx
.L2827:
	movb	%dl, 14(%ecx)
	movl	8(%esp), %edx
	movzbl	15(%ecx), %eax
	movzbl	15(%edx), %edx
	addl	%edx, %eax
	cmpl	$255, %eax
	movl	%eax, 4(%esp)
	movl	$255, %edx
	jg	.L2832
	movl	%eax, %edx
.L2832:
	movb	%dl, 15(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddusb_xmm, .-op_paddusb_xmm
	.p2align 4,,15
.globl op_paddsb_xmm
	.type	op_paddsb_xmm, @function
op_paddsb_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movsbl	(%eax),%eax
	movsbl	(%ecx),%edx
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2838
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2838
	movl	%eax, %edx
	.p2align 4,,15
.L2838:
	movb	%dl, (%ecx)
	movl	8(%esp), %eax
	movsbl	1(%ecx),%edx
	movsbl	1(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2843
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2843
	movl	%eax, %edx
	.p2align 4,,15
.L2843:
	movb	%dl, 1(%ecx)
	movl	8(%esp), %eax
	movsbl	2(%ecx),%edx
	movsbl	2(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2848
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2848
	movl	%eax, %edx
	.p2align 4,,15
.L2848:
	movb	%dl, 2(%ecx)
	movl	8(%esp), %eax
	movsbl	3(%ecx),%edx
	movsbl	3(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2853
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2853
	movl	%eax, %edx
	.p2align 4,,15
.L2853:
	movb	%dl, 3(%ecx)
	movl	8(%esp), %eax
	movsbl	4(%ecx),%edx
	movsbl	4(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2858
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2858
	movl	%eax, %edx
	.p2align 4,,15
.L2858:
	movb	%dl, 4(%ecx)
	movl	8(%esp), %eax
	movsbl	5(%ecx),%edx
	movsbl	5(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2863
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2863
	movl	%eax, %edx
	.p2align 4,,15
.L2863:
	movb	%dl, 5(%ecx)
	movl	8(%esp), %eax
	movsbl	6(%ecx),%edx
	movsbl	6(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2868
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2868
	movl	%eax, %edx
	.p2align 4,,15
.L2868:
	movb	%dl, 6(%ecx)
	movl	8(%esp), %eax
	movsbl	7(%ecx),%edx
	movsbl	7(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2873
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2873
	movl	%eax, %edx
	.p2align 4,,15
.L2873:
	movb	%dl, 7(%ecx)
	movl	8(%esp), %eax
	movsbl	8(%ecx),%edx
	movsbl	8(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2878
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2878
	movl	%eax, %edx
	.p2align 4,,15
.L2878:
	movb	%dl, 8(%ecx)
	movl	8(%esp), %eax
	movsbl	9(%ecx),%edx
	movsbl	9(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2883
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2883
	movl	%eax, %edx
	.p2align 4,,15
.L2883:
	movb	%dl, 9(%ecx)
	movl	8(%esp), %eax
	movsbl	10(%ecx),%edx
	movsbl	10(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2888
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2888
	movl	%eax, %edx
	.p2align 4,,15
.L2888:
	movb	%dl, 10(%ecx)
	movl	8(%esp), %eax
	movsbl	11(%ecx),%edx
	movsbl	11(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2893
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2893
	movl	%eax, %edx
	.p2align 4,,15
.L2893:
	movb	%dl, 11(%ecx)
	movl	8(%esp), %eax
	movsbl	12(%ecx),%edx
	movsbl	12(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2898
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2898
	movl	%eax, %edx
	.p2align 4,,15
.L2898:
	movb	%dl, 12(%ecx)
	movl	8(%esp), %eax
	movsbl	13(%ecx),%edx
	movsbl	13(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2903
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2903
	movl	%eax, %edx
	.p2align 4,,15
.L2903:
	movb	%dl, 13(%ecx)
	movl	8(%esp), %eax
	movsbl	14(%ecx),%edx
	movsbl	14(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2908
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2908
	movl	%eax, %edx
	.p2align 4,,15
.L2908:
	movb	%dl, 14(%ecx)
	movl	8(%esp), %eax
	movsbl	15(%ecx),%edx
	movsbl	15(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-128, %eax
	movl	$-128, %edx
	jl	.L2913
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L2913
	movl	%eax, %edx
	.p2align 4,,15
.L2913:
	movb	%dl, 15(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddsb_xmm, .-op_paddsb_xmm
	.p2align 4,,15
.globl op_psubusb_xmm
	.type	op_psubusb_xmm, @function
op_psubusb_xmm:
	subl	$72, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 68(%esp)
	movzbl	(%eax), %eax
	movzbl	(%ecx), %edx
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 64(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2919
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 64(%esp)
	jg	.L2919
	movl	%eax, 64(%esp)
.L2919:
	movzbl	64(%esp), %eax
	movzbl	1(%ecx), %edx
	movb	%al, (%ecx)
	movl	68(%esp), %eax
	movzbl	1(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 60(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2924
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 60(%esp)
	jg	.L2924
	movl	%eax, 60(%esp)
.L2924:
	movzbl	60(%esp), %eax
	movzbl	2(%ecx), %edx
	movb	%al, 1(%ecx)
	movl	68(%esp), %eax
	movzbl	2(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 56(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2929
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 56(%esp)
	jg	.L2929
	movl	%eax, 56(%esp)
.L2929:
	movzbl	56(%esp), %eax
	movzbl	3(%ecx), %edx
	movb	%al, 2(%ecx)
	movl	68(%esp), %eax
	movzbl	3(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 52(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2934
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 52(%esp)
	jg	.L2934
	movl	%eax, 52(%esp)
.L2934:
	movzbl	52(%esp), %eax
	movzbl	4(%ecx), %edx
	movb	%al, 3(%ecx)
	movl	68(%esp), %eax
	movzbl	4(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 48(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2939
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 48(%esp)
	jg	.L2939
	movl	%eax, 48(%esp)
.L2939:
	movzbl	48(%esp), %eax
	movzbl	5(%ecx), %edx
	movb	%al, 4(%ecx)
	movl	68(%esp), %eax
	movzbl	5(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 44(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2944
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 44(%esp)
	jg	.L2944
	movl	%eax, 44(%esp)
.L2944:
	movzbl	44(%esp), %eax
	movzbl	6(%ecx), %edx
	movb	%al, 5(%ecx)
	movl	68(%esp), %eax
	movzbl	6(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 40(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2949
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 40(%esp)
	jg	.L2949
	movl	%eax, 40(%esp)
.L2949:
	movzbl	40(%esp), %eax
	movzbl	7(%ecx), %edx
	movb	%al, 6(%ecx)
	movl	68(%esp), %eax
	movzbl	7(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 36(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2954
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 36(%esp)
	jg	.L2954
	movl	%eax, 36(%esp)
.L2954:
	movzbl	36(%esp), %eax
	movzbl	8(%ecx), %edx
	movb	%al, 7(%ecx)
	movl	68(%esp), %eax
	movzbl	8(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 32(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2959
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 32(%esp)
	jg	.L2959
	movl	%eax, 32(%esp)
.L2959:
	movzbl	32(%esp), %eax
	movzbl	9(%ecx), %edx
	movb	%al, 8(%ecx)
	movl	68(%esp), %eax
	movzbl	9(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 28(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2964
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 28(%esp)
	jg	.L2964
	movl	%eax, 28(%esp)
.L2964:
	movzbl	28(%esp), %eax
	movzbl	10(%ecx), %edx
	movb	%al, 9(%ecx)
	movl	68(%esp), %eax
	movzbl	10(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 24(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2969
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 24(%esp)
	jg	.L2969
	movl	%eax, 24(%esp)
.L2969:
	movzbl	24(%esp), %eax
	movzbl	11(%ecx), %edx
	movb	%al, 10(%ecx)
	movl	68(%esp), %eax
	movzbl	11(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 20(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2974
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 20(%esp)
	jg	.L2974
	movl	%eax, 20(%esp)
.L2974:
	movzbl	20(%esp), %eax
	movzbl	12(%ecx), %edx
	movb	%al, 11(%ecx)
	movl	68(%esp), %eax
	movzbl	12(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 16(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2979
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 16(%esp)
	jg	.L2979
	movl	%eax, 16(%esp)
.L2979:
	movzbl	16(%esp), %eax
	movzbl	13(%ecx), %edx
	movb	%al, 12(%ecx)
	movl	68(%esp), %eax
	movzbl	13(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2984
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 12(%esp)
	jg	.L2984
	movl	%eax, 12(%esp)
.L2984:
	movzbl	12(%esp), %eax
	movzbl	14(%ecx), %edx
	movb	%al, 13(%ecx)
	movl	68(%esp), %eax
	movzbl	14(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2989
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 8(%esp)
	jg	.L2989
	movl	%eax, 8(%esp)
.L2989:
	movzbl	8(%esp), %eax
	movzbl	15(%ecx), %edx
	movb	%al, 14(%ecx)
	movl	68(%esp), %eax
	movzbl	15(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L2994
	movl	$255, %edx
	cmpl	$255, %eax
	movl	%edx, 4(%esp)
	jg	.L2994
	movl	%eax, 4(%esp)
.L2994:
	movzbl	4(%esp), %eax
	movb	%al, 15(%ecx)
	addl	$72, %esp
	ret
	.size	op_psubusb_xmm, .-op_psubusb_xmm
	.p2align 4,,15
.globl op_psubsb_xmm
	.type	op_psubsb_xmm, @function
op_psubsb_xmm:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, 4(%esp)
	movsbl	(%eax),%eax
	movsbl	(%edx),%ecx
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3000
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3000
	movl	%ecx, %eax
	.p2align 4,,15
.L3000:
	movb	%al, (%edx)
	movl	4(%esp), %eax
	movsbl	1(%edx),%ecx
	movsbl	1(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3005
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3005
	movl	%ecx, %eax
	.p2align 4,,15
.L3005:
	movb	%al, 1(%edx)
	movl	4(%esp), %eax
	movsbl	2(%edx),%ecx
	movsbl	2(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3010
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3010
	movl	%ecx, %eax
	.p2align 4,,15
.L3010:
	movb	%al, 2(%edx)
	movl	4(%esp), %eax
	movsbl	3(%edx),%ecx
	movsbl	3(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3015
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3015
	movl	%ecx, %eax
	.p2align 4,,15
.L3015:
	movb	%al, 3(%edx)
	movl	4(%esp), %eax
	movsbl	4(%edx),%ecx
	movsbl	4(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3020
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3020
	movl	%ecx, %eax
	.p2align 4,,15
.L3020:
	movb	%al, 4(%edx)
	movl	4(%esp), %eax
	movsbl	5(%edx),%ecx
	movsbl	5(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3025
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3025
	movl	%ecx, %eax
	.p2align 4,,15
.L3025:
	movb	%al, 5(%edx)
	movl	4(%esp), %eax
	movsbl	6(%edx),%ecx
	movsbl	6(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3030
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3030
	movl	%ecx, %eax
	.p2align 4,,15
.L3030:
	movb	%al, 6(%edx)
	movl	4(%esp), %eax
	movsbl	7(%edx),%ecx
	movsbl	7(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3035
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3035
	movl	%ecx, %eax
	.p2align 4,,15
.L3035:
	movb	%al, 7(%edx)
	movl	4(%esp), %eax
	movsbl	8(%edx),%ecx
	movsbl	8(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3040
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3040
	movl	%ecx, %eax
	.p2align 4,,15
.L3040:
	movb	%al, 8(%edx)
	movl	4(%esp), %eax
	movsbl	9(%edx),%ecx
	movsbl	9(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3045
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3045
	movl	%ecx, %eax
	.p2align 4,,15
.L3045:
	movb	%al, 9(%edx)
	movl	4(%esp), %eax
	movsbl	10(%edx),%ecx
	movsbl	10(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3050
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3050
	movl	%ecx, %eax
	.p2align 4,,15
.L3050:
	movb	%al, 10(%edx)
	movl	4(%esp), %eax
	movsbl	11(%edx),%ecx
	movsbl	11(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3055
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3055
	movl	%ecx, %eax
	.p2align 4,,15
.L3055:
	movb	%al, 11(%edx)
	movl	4(%esp), %eax
	movsbl	12(%edx),%ecx
	movsbl	12(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3060
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3060
	movl	%ecx, %eax
	.p2align 4,,15
.L3060:
	movb	%al, 12(%edx)
	movl	4(%esp), %eax
	movsbl	13(%edx),%ecx
	movsbl	13(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3065
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3065
	movl	%ecx, %eax
	.p2align 4,,15
.L3065:
	movb	%al, 13(%edx)
	movl	4(%esp), %eax
	movsbl	14(%edx),%ecx
	movsbl	14(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3070
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3070
	movl	%ecx, %eax
	.p2align 4,,15
.L3070:
	movb	%al, 14(%edx)
	movl	4(%esp), %eax
	movsbl	15(%edx),%ecx
	movsbl	15(%eax),%eax
	subl	%eax, %ecx
	cmpl	$-128, %ecx
	movl	$-128, %eax
	jl	.L3075
	cmpl	$127, %ecx
	movl	$127, %eax
	jg	.L3075
	movl	%ecx, %eax
	.p2align 4,,15
.L3075:
	movb	%al, 15(%edx)
	addl	$8, %esp
	ret
	.size	op_psubsb_xmm, .-op_psubsb_xmm
	.p2align 4,,15
.globl op_paddusw_xmm
	.type	op_paddusw_xmm, @function
op_paddusw_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzwl	(%ecx), %edx
	movzwl	(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L3081
	movl	4(%esp), %edx
.L3081:
	movw	%dx, (%ecx)
	movl	8(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L3086
	movl	%eax, %edx
.L3086:
	movw	%dx, 2(%ecx)
	movl	8(%esp), %eax
	movzwl	4(%ecx), %edx
	movzwl	4(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L3091
	movl	4(%esp), %edx
.L3091:
	movw	%dx, 4(%ecx)
	movl	8(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L3096
	movl	%eax, %edx
.L3096:
	movw	%dx, 6(%ecx)
	movl	8(%esp), %eax
	movzwl	8(%ecx), %edx
	movzwl	8(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L3101
	movl	4(%esp), %edx
.L3101:
	movw	%dx, 8(%ecx)
	movl	8(%esp), %edx
	movzwl	10(%ecx), %eax
	movzwl	10(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L3106
	movl	%eax, %edx
.L3106:
	movw	%dx, 10(%ecx)
	movl	8(%esp), %eax
	movzwl	12(%ecx), %edx
	movzwl	12(%eax), %eax
	addl	%eax, %edx
	movl	%edx, 4(%esp)
	movl	$65535, %edx
	cmpl	$65535, 4(%esp)
	jg	.L3111
	movl	4(%esp), %edx
.L3111:
	movw	%dx, 12(%ecx)
	movl	8(%esp), %edx
	movzwl	14(%ecx), %eax
	movzwl	14(%edx), %edx
	addl	%edx, %eax
	cmpl	$65535, %eax
	movl	%eax, 4(%esp)
	movl	$65535, %edx
	jg	.L3116
	movl	%eax, %edx
.L3116:
	movw	%dx, 14(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddusw_xmm, .-op_paddusw_xmm
	.p2align 4,,15
.globl op_paddsw_xmm
	.type	op_paddsw_xmm, @function
op_paddsw_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movswl	(%eax),%eax
	movswl	(%ecx),%edx
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3122
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3122
	movl	%eax, %edx
	.p2align 4,,15
.L3122:
	movw	%dx, (%ecx)
	movl	8(%esp), %eax
	movswl	2(%ecx),%edx
	movswl	2(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3127
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3127
	movl	%eax, %edx
	.p2align 4,,15
.L3127:
	movw	%dx, 2(%ecx)
	movl	8(%esp), %eax
	movswl	4(%ecx),%edx
	movswl	4(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3132
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3132
	movl	%eax, %edx
	.p2align 4,,15
.L3132:
	movw	%dx, 4(%ecx)
	movl	8(%esp), %eax
	movswl	6(%ecx),%edx
	movswl	6(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3137
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3137
	movl	%eax, %edx
	.p2align 4,,15
.L3137:
	movw	%dx, 6(%ecx)
	movl	8(%esp), %eax
	movswl	8(%ecx),%edx
	movswl	8(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3142
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3142
	movl	%eax, %edx
	.p2align 4,,15
.L3142:
	movw	%dx, 8(%ecx)
	movl	8(%esp), %eax
	movswl	10(%ecx),%edx
	movswl	10(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3147
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3147
	movl	%eax, %edx
	.p2align 4,,15
.L3147:
	movw	%dx, 10(%ecx)
	movl	8(%esp), %eax
	movswl	12(%ecx),%edx
	movswl	12(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3152
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3152
	movl	%eax, %edx
	.p2align 4,,15
.L3152:
	movw	%dx, 12(%ecx)
	movl	8(%esp), %eax
	movswl	14(%ecx),%edx
	movswl	14(%eax),%eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	movl	4(%esp), %edx
	addl	%edx, %eax
	cmpl	$-32768, %eax
	movl	$-32768, %edx
	jl	.L3157
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L3157
	movl	%eax, %edx
	.p2align 4,,15
.L3157:
	movw	%dx, 14(%ecx)
	addl	$12, %esp
	ret
	.size	op_paddsw_xmm, .-op_paddsw_xmm
	.p2align 4,,15
.globl op_psubusw_xmm
	.type	op_psubusw_xmm, @function
op_psubusw_xmm:
	subl	$40, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 36(%esp)
	movzwl	(%eax), %eax
	movzwl	(%ecx), %edx
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 32(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3163
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 32(%esp)
	jg	.L3163
	movl	%eax, 32(%esp)
.L3163:
	movzwl	2(%ecx), %edx
	movl	32(%esp), %eax
	movw	%ax, (%ecx)
	movl	36(%esp), %eax
	movzwl	2(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 28(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3168
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 28(%esp)
	jg	.L3168
	movl	%eax, 28(%esp)
.L3168:
	movzwl	4(%ecx), %edx
	movl	28(%esp), %eax
	movw	%ax, 2(%ecx)
	movl	36(%esp), %eax
	movzwl	4(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 24(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3173
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 24(%esp)
	jg	.L3173
	movl	%eax, 24(%esp)
.L3173:
	movzwl	6(%ecx), %edx
	movl	24(%esp), %eax
	movw	%ax, 4(%ecx)
	movl	36(%esp), %eax
	movzwl	6(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 20(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3178
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 20(%esp)
	jg	.L3178
	movl	%eax, 20(%esp)
.L3178:
	movzwl	8(%ecx), %edx
	movl	20(%esp), %eax
	movw	%ax, 6(%ecx)
	movl	36(%esp), %eax
	movzwl	8(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 16(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3183
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 16(%esp)
	jg	.L3183
	movl	%eax, 16(%esp)
.L3183:
	movzwl	10(%ecx), %edx
	movl	16(%esp), %eax
	movw	%ax, 8(%ecx)
	movl	36(%esp), %eax
	movzwl	10(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3188
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 12(%esp)
	jg	.L3188
	movl	%eax, 12(%esp)
.L3188:
	movzwl	12(%ecx), %edx
	movl	12(%esp), %eax
	movw	%ax, 10(%ecx)
	movl	36(%esp), %eax
	movzwl	12(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 8(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3193
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 8(%esp)
	jg	.L3193
	movl	%eax, 8(%esp)
.L3193:
	movzwl	14(%ecx), %edx
	movl	8(%esp), %eax
	movw	%ax, 12(%ecx)
	movl	36(%esp), %eax
	movzwl	14(%eax), %eax
	movl	%eax, (%esp)
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	subl	(%esp), %eax
	js	.L3198
	movl	$65535, %edx
	cmpl	$65535, %eax
	movl	%edx, 4(%esp)
	jg	.L3198
	movl	%eax, 4(%esp)
.L3198:
	movl	4(%esp), %eax
	movw	%ax, 14(%ecx)
	addl	$40, %esp
	ret
	.size	op_psubusw_xmm, .-op_psubusw_xmm
	.p2align 4,,15
.globl op_psubsw_xmm
	.type	op_psubsw_xmm, @function
op_psubsw_xmm:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movswl	(%eax),%eax
	movswl	(%ecx),%edx
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3204
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3204
	movl	%edx, %eax
	.p2align 4,,15
.L3204:
	movw	%ax, (%ecx)
	movl	4(%esp), %eax
	movswl	2(%ecx),%edx
	movswl	2(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3209
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3209
	movl	%edx, %eax
	.p2align 4,,15
.L3209:
	movw	%ax, 2(%ecx)
	movl	4(%esp), %eax
	movswl	4(%ecx),%edx
	movswl	4(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3214
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3214
	movl	%edx, %eax
	.p2align 4,,15
.L3214:
	movw	%ax, 4(%ecx)
	movl	4(%esp), %eax
	movswl	6(%ecx),%edx
	movswl	6(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3219
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3219
	movl	%edx, %eax
	.p2align 4,,15
.L3219:
	movw	%ax, 6(%ecx)
	movl	4(%esp), %eax
	movswl	8(%ecx),%edx
	movswl	8(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3224
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3224
	movl	%edx, %eax
	.p2align 4,,15
.L3224:
	movw	%ax, 8(%ecx)
	movl	4(%esp), %eax
	movswl	10(%ecx),%edx
	movswl	10(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3229
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3229
	movl	%edx, %eax
	.p2align 4,,15
.L3229:
	movw	%ax, 10(%ecx)
	movl	4(%esp), %eax
	movswl	12(%ecx),%edx
	movswl	12(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3234
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3234
	movl	%edx, %eax
	.p2align 4,,15
.L3234:
	movw	%ax, 12(%ecx)
	movl	4(%esp), %eax
	movswl	14(%ecx),%edx
	movswl	14(%eax),%eax
	subl	%eax, %edx
	cmpl	$-32768, %edx
	movl	$-32768, %eax
	jl	.L3239
	cmpl	$32767, %edx
	movl	$32767, %eax
	jg	.L3239
	movl	%edx, %eax
	.p2align 4,,15
.L3239:
	movw	%ax, 14(%ecx)
	addl	$8, %esp
	ret
	.size	op_psubsw_xmm, .-op_psubsw_xmm
	.p2align 4,,15
.globl op_pminub_xmm
	.type	op_pminub_xmm, @function
op_pminub_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movzbl	(%eax), %edx
	movb	%dl, 15(%esp)
	movzbl	(%ecx), %edx
	cmpb	15(%esp), %dl
	jbe	.L3244
	movzbl	15(%esp), %edx
.L3244:
	movb	%dl, (%eax)
	movzbl	1(%eax), %edx
	movb	%dl, 14(%esp)
	movzbl	1(%ecx), %edx
	cmpb	14(%esp), %dl
	jbe	.L3245
	movzbl	14(%esp), %edx
.L3245:
	movb	%dl, 1(%eax)
	movzbl	2(%eax), %edx
	movb	%dl, 13(%esp)
	movzbl	2(%ecx), %edx
	cmpb	13(%esp), %dl
	jbe	.L3246
	movzbl	13(%esp), %edx
.L3246:
	movb	%dl, 2(%eax)
	movzbl	3(%eax), %edx
	movb	%dl, 12(%esp)
	movzbl	3(%ecx), %edx
	cmpb	12(%esp), %dl
	jbe	.L3247
	movzbl	12(%esp), %edx
.L3247:
	movb	%dl, 3(%eax)
	movzbl	4(%eax), %edx
	movb	%dl, 11(%esp)
	movzbl	4(%ecx), %edx
	cmpb	11(%esp), %dl
	jbe	.L3248
	movzbl	11(%esp), %edx
.L3248:
	movb	%dl, 4(%eax)
	movzbl	5(%eax), %edx
	movb	%dl, 10(%esp)
	movzbl	5(%ecx), %edx
	cmpb	10(%esp), %dl
	jbe	.L3249
	movzbl	10(%esp), %edx
.L3249:
	movb	%dl, 5(%eax)
	movzbl	6(%eax), %edx
	movb	%dl, 9(%esp)
	movzbl	6(%ecx), %edx
	cmpb	9(%esp), %dl
	jbe	.L3250
	movzbl	9(%esp), %edx
.L3250:
	movb	%dl, 6(%eax)
	movzbl	7(%eax), %edx
	movb	%dl, 8(%esp)
	movzbl	7(%ecx), %edx
	cmpb	8(%esp), %dl
	jbe	.L3251
	movzbl	8(%esp), %edx
.L3251:
	movb	%dl, 7(%eax)
	movzbl	8(%eax), %edx
	movb	%dl, 7(%esp)
	movzbl	8(%ecx), %edx
	cmpb	7(%esp), %dl
	jbe	.L3252
	movzbl	7(%esp), %edx
.L3252:
	movb	%dl, 8(%eax)
	movzbl	9(%eax), %edx
	movb	%dl, 6(%esp)
	movzbl	9(%ecx), %edx
	cmpb	6(%esp), %dl
	jbe	.L3253
	movzbl	6(%esp), %edx
.L3253:
	movb	%dl, 9(%eax)
	movzbl	10(%eax), %edx
	movb	%dl, 5(%esp)
	movzbl	10(%ecx), %edx
	cmpb	5(%esp), %dl
	jbe	.L3254
	movzbl	5(%esp), %edx
.L3254:
	movb	%dl, 10(%eax)
	movzbl	11(%eax), %edx
	movb	%dl, 4(%esp)
	movzbl	11(%ecx), %edx
	cmpb	4(%esp), %dl
	jbe	.L3255
	movzbl	4(%esp), %edx
.L3255:
	movb	%dl, 11(%eax)
	movzbl	12(%eax), %edx
	movb	%dl, 3(%esp)
	movzbl	12(%ecx), %edx
	cmpb	3(%esp), %dl
	jbe	.L3256
	movzbl	3(%esp), %edx
.L3256:
	movb	%dl, 12(%eax)
	movzbl	13(%eax), %edx
	movb	%dl, 2(%esp)
	movzbl	13(%ecx), %edx
	cmpb	2(%esp), %dl
	jbe	.L3257
	movzbl	2(%esp), %edx
.L3257:
	movb	%dl, 13(%eax)
	movzbl	14(%eax), %edx
	movb	%dl, 1(%esp)
	movzbl	14(%ecx), %edx
	cmpb	1(%esp), %dl
	jbe	.L3258
	movzbl	1(%esp), %edx
.L3258:
	movb	%dl, 14(%eax)
	movzbl	15(%eax), %edx
	movb	%dl, (%esp)
	movzbl	15(%ecx), %edx
	cmpb	(%esp), %dl
	jbe	.L3259
	movzbl	(%esp), %edx
.L3259:
	movb	%dl, 15(%eax)
	addl	$16, %esp
	ret
	.size	op_pminub_xmm, .-op_pminub_xmm
	.p2align 4,,15
.globl op_pmaxub_xmm
	.type	op_pmaxub_xmm, @function
op_pmaxub_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movzbl	(%eax), %edx
	movb	%dl, 15(%esp)
	movzbl	(%ecx), %edx
	cmpb	15(%esp), %dl
	jae	.L3261
	movzbl	15(%esp), %edx
.L3261:
	movb	%dl, (%eax)
	movzbl	1(%eax), %edx
	movb	%dl, 14(%esp)
	movzbl	1(%ecx), %edx
	cmpb	14(%esp), %dl
	jae	.L3262
	movzbl	14(%esp), %edx
.L3262:
	movb	%dl, 1(%eax)
	movzbl	2(%eax), %edx
	movb	%dl, 13(%esp)
	movzbl	2(%ecx), %edx
	cmpb	13(%esp), %dl
	jae	.L3263
	movzbl	13(%esp), %edx
.L3263:
	movb	%dl, 2(%eax)
	movzbl	3(%eax), %edx
	movb	%dl, 12(%esp)
	movzbl	3(%ecx), %edx
	cmpb	12(%esp), %dl
	jae	.L3264
	movzbl	12(%esp), %edx
.L3264:
	movb	%dl, 3(%eax)
	movzbl	4(%eax), %edx
	movb	%dl, 11(%esp)
	movzbl	4(%ecx), %edx
	cmpb	11(%esp), %dl
	jae	.L3265
	movzbl	11(%esp), %edx
.L3265:
	movb	%dl, 4(%eax)
	movzbl	5(%eax), %edx
	movb	%dl, 10(%esp)
	movzbl	5(%ecx), %edx
	cmpb	10(%esp), %dl
	jae	.L3266
	movzbl	10(%esp), %edx
.L3266:
	movb	%dl, 5(%eax)
	movzbl	6(%eax), %edx
	movb	%dl, 9(%esp)
	movzbl	6(%ecx), %edx
	cmpb	9(%esp), %dl
	jae	.L3267
	movzbl	9(%esp), %edx
.L3267:
	movb	%dl, 6(%eax)
	movzbl	7(%eax), %edx
	movb	%dl, 8(%esp)
	movzbl	7(%ecx), %edx
	cmpb	8(%esp), %dl
	jae	.L3268
	movzbl	8(%esp), %edx
.L3268:
	movb	%dl, 7(%eax)
	movzbl	8(%eax), %edx
	movb	%dl, 7(%esp)
	movzbl	8(%ecx), %edx
	cmpb	7(%esp), %dl
	jae	.L3269
	movzbl	7(%esp), %edx
.L3269:
	movb	%dl, 8(%eax)
	movzbl	9(%eax), %edx
	movb	%dl, 6(%esp)
	movzbl	9(%ecx), %edx
	cmpb	6(%esp), %dl
	jae	.L3270
	movzbl	6(%esp), %edx
.L3270:
	movb	%dl, 9(%eax)
	movzbl	10(%eax), %edx
	movb	%dl, 5(%esp)
	movzbl	10(%ecx), %edx
	cmpb	5(%esp), %dl
	jae	.L3271
	movzbl	5(%esp), %edx
.L3271:
	movb	%dl, 10(%eax)
	movzbl	11(%eax), %edx
	movb	%dl, 4(%esp)
	movzbl	11(%ecx), %edx
	cmpb	4(%esp), %dl
	jae	.L3272
	movzbl	4(%esp), %edx
.L3272:
	movb	%dl, 11(%eax)
	movzbl	12(%eax), %edx
	movb	%dl, 3(%esp)
	movzbl	12(%ecx), %edx
	cmpb	3(%esp), %dl
	jae	.L3273
	movzbl	3(%esp), %edx
.L3273:
	movb	%dl, 12(%eax)
	movzbl	13(%eax), %edx
	movb	%dl, 2(%esp)
	movzbl	13(%ecx), %edx
	cmpb	2(%esp), %dl
	jae	.L3274
	movzbl	2(%esp), %edx
.L3274:
	movb	%dl, 13(%eax)
	movzbl	14(%eax), %edx
	movb	%dl, 1(%esp)
	movzbl	14(%ecx), %edx
	cmpb	1(%esp), %dl
	jae	.L3275
	movzbl	1(%esp), %edx
.L3275:
	movb	%dl, 14(%eax)
	movzbl	15(%eax), %edx
	movb	%dl, (%esp)
	movzbl	15(%ecx), %edx
	cmpb	(%esp), %dl
	jae	.L3276
	movzbl	(%esp), %edx
.L3276:
	movb	%dl, 15(%eax)
	addl	$16, %esp
	ret
	.size	op_pmaxub_xmm, .-op_pmaxub_xmm
	.p2align 4,,15
.globl op_pminsw_xmm
	.type	op_pminsw_xmm, @function
op_pminsw_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzwl	(%edx), %eax
	movw	%ax, 14(%esp)
	movzwl	(%ecx), %eax
	cmpw	14(%esp), %ax
	jle	.L3278
	movzwl	14(%esp), %eax
.L3278:
	movw	%ax, (%edx)
	movzwl	2(%edx), %eax
	movw	%ax, 12(%esp)
	movzwl	2(%ecx), %eax
	cmpw	12(%esp), %ax
	jle	.L3279
	movl	12(%esp), %eax
.L3279:
	movw	%ax, 2(%edx)
	movzwl	4(%edx), %eax
	movw	%ax, 10(%esp)
	movzwl	4(%ecx), %eax
	cmpw	10(%esp), %ax
	jle	.L3280
	movzwl	10(%esp), %eax
.L3280:
	movw	%ax, 4(%edx)
	movzwl	6(%edx), %eax
	movw	%ax, 8(%esp)
	movzwl	6(%ecx), %eax
	cmpw	8(%esp), %ax
	jle	.L3281
	movl	8(%esp), %eax
.L3281:
	movw	%ax, 6(%edx)
	movzwl	8(%edx), %eax
	movw	%ax, 6(%esp)
	movzwl	8(%ecx), %eax
	cmpw	6(%esp), %ax
	jle	.L3282
	movzwl	6(%esp), %eax
.L3282:
	movw	%ax, 8(%edx)
	movzwl	10(%edx), %eax
	movw	%ax, 4(%esp)
	movzwl	10(%ecx), %eax
	cmpw	4(%esp), %ax
	jle	.L3283
	movl	4(%esp), %eax
.L3283:
	movw	%ax, 10(%edx)
	movzwl	12(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	12(%ecx), %eax
	cmpw	2(%esp), %ax
	jle	.L3284
	movzwl	2(%esp), %eax
.L3284:
	movw	%ax, 12(%edx)
	movzwl	14(%edx), %eax
	movw	%ax, (%esp)
	movzwl	14(%ecx), %eax
	cmpw	(%esp), %ax
	jle	.L3285
	movl	(%esp), %eax
.L3285:
	movw	%ax, 14(%edx)
	addl	$16, %esp
	ret
	.size	op_pminsw_xmm, .-op_pminsw_xmm
	.p2align 4,,15
.globl op_pmaxsw_xmm
	.type	op_pmaxsw_xmm, @function
op_pmaxsw_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzwl	(%edx), %eax
	movw	%ax, 14(%esp)
	movzwl	(%ecx), %eax
	cmpw	14(%esp), %ax
	jge	.L3287
	movzwl	14(%esp), %eax
.L3287:
	movw	%ax, (%edx)
	movzwl	2(%edx), %eax
	movw	%ax, 12(%esp)
	movzwl	2(%ecx), %eax
	cmpw	12(%esp), %ax
	jge	.L3288
	movl	12(%esp), %eax
.L3288:
	movw	%ax, 2(%edx)
	movzwl	4(%edx), %eax
	movw	%ax, 10(%esp)
	movzwl	4(%ecx), %eax
	cmpw	10(%esp), %ax
	jge	.L3289
	movzwl	10(%esp), %eax
.L3289:
	movw	%ax, 4(%edx)
	movzwl	6(%edx), %eax
	movw	%ax, 8(%esp)
	movzwl	6(%ecx), %eax
	cmpw	8(%esp), %ax
	jge	.L3290
	movl	8(%esp), %eax
.L3290:
	movw	%ax, 6(%edx)
	movzwl	8(%edx), %eax
	movw	%ax, 6(%esp)
	movzwl	8(%ecx), %eax
	cmpw	6(%esp), %ax
	jge	.L3291
	movzwl	6(%esp), %eax
.L3291:
	movw	%ax, 8(%edx)
	movzwl	10(%edx), %eax
	movw	%ax, 4(%esp)
	movzwl	10(%ecx), %eax
	cmpw	4(%esp), %ax
	jge	.L3292
	movl	4(%esp), %eax
.L3292:
	movw	%ax, 10(%edx)
	movzwl	12(%edx), %eax
	movw	%ax, 2(%esp)
	movzwl	12(%ecx), %eax
	cmpw	2(%esp), %ax
	jge	.L3293
	movzwl	2(%esp), %eax
.L3293:
	movw	%ax, 12(%edx)
	movzwl	14(%edx), %eax
	movw	%ax, (%esp)
	movzwl	14(%ecx), %eax
	cmpw	(%esp), %ax
	jge	.L3294
	movl	(%esp), %eax
.L3294:
	movw	%ax, 14(%edx)
	addl	$16, %esp
	ret
	.size	op_pmaxsw_xmm, .-op_pmaxsw_xmm
	.p2align 4,,15
.globl op_pand_xmm
	.type	op_pand_xmm, @function
op_pand_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	movl	%eax, %edx
	movl	%eax, 16(%esp)
	leal	__op_param1(%ebp), %ecx
	movl	(%eax), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	movl	(%ecx), %eax
	movl	(%esp), %edx
	andl	%edx, %eax
	movl	4(%ecx), %edx
	andl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	8(%ecx), %eax
	movl	8(%esp), %edx
	andl	%edx, %eax
	movl	12(%ecx), %edx
	andl	12(%esp), %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_pand_xmm, .-op_pand_xmm
	.p2align 4,,15
.globl op_pandn_xmm
	.type	op_pandn_xmm, @function
op_pandn_xmm:
	subl	$28, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 24(%esp)
	movl	(%ecx), %edx
	notl	%edx
	movl	%edx, (%esp)
	movl	4(%ecx), %eax
	movl	24(%esp), %edx
	notl	%eax
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	4(%edx), %edx
	movl	%eax, 8(%esp)
	movl	(%esp), %eax
	movl	%edx, 12(%esp)
	movl	8(%esp), %edx
	andl	%edx, %eax
	movl	4(%esp), %edx
	andl	12(%esp), %edx
	movl	%eax, (%ecx)
	movl	8(%ecx), %eax
	movl	%edx, 4(%ecx)
	notl	%eax
	movl	%eax, (%esp)
	movl	12(%ecx), %edx
	notl	%edx
	movl	%edx, 4(%esp)
	movl	24(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 16(%esp)
	movl	(%esp), %eax
	movl	%edx, 20(%esp)
	movl	16(%esp), %edx
	andl	%edx, %eax
	movl	4(%esp), %edx
	andl	20(%esp), %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$28, %esp
	ret
	.size	op_pandn_xmm, .-op_pandn_xmm
	.p2align 4,,15
.globl op_por_xmm
	.type	op_por_xmm, @function
op_por_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	movl	%eax, %edx
	movl	%eax, 16(%esp)
	leal	__op_param1(%ebp), %ecx
	movl	(%eax), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	movl	(%ecx), %eax
	movl	(%esp), %edx
	orl	%edx, %eax
	movl	4(%ecx), %edx
	orl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	8(%ecx), %eax
	movl	8(%esp), %edx
	orl	%edx, %eax
	movl	12(%ecx), %edx
	orl	12(%esp), %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_por_xmm, .-op_por_xmm
	.p2align 4,,15
.globl op_pxor_xmm
	.type	op_pxor_xmm, @function
op_pxor_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	movl	%eax, %edx
	movl	%eax, 16(%esp)
	leal	__op_param1(%ebp), %ecx
	movl	(%eax), %eax
	movl	4(%edx), %edx
	movl	%eax, (%esp)
	movl	%edx, 4(%esp)
	movl	(%ecx), %eax
	movl	(%esp), %edx
	xorl	%edx, %eax
	movl	4(%ecx), %edx
	xorl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %edx
	movl	8(%edx), %eax
	movl	12(%edx), %edx
	movl	%eax, 8(%esp)
	movl	%edx, 12(%esp)
	movl	8(%ecx), %eax
	movl	8(%esp), %edx
	xorl	%edx, %eax
	movl	12(%ecx), %edx
	xorl	12(%esp), %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_pxor_xmm, .-op_pxor_xmm
	.p2align 4,,15
.globl op_pcmpgtb_xmm
	.type	op_pcmpgtb_xmm, @function
op_pcmpgtb_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzbl	(%ecx), %eax
	cmpb	%al, (%edx)
	setle	%al
	decb	%al
	movb	%al, (%edx)
	movzbl	1(%ecx), %eax
	cmpb	%al, 1(%edx)
	setle	%al
	decb	%al
	movb	%al, 1(%edx)
	movzbl	2(%ecx), %eax
	cmpb	%al, 2(%edx)
	setle	%al
	decb	%al
	movb	%al, 2(%edx)
	movzbl	3(%ecx), %eax
	cmpb	%al, 3(%edx)
	setle	%al
	decb	%al
	movb	%al, 3(%edx)
	movzbl	4(%ecx), %eax
	cmpb	%al, 4(%edx)
	setle	%al
	decb	%al
	movb	%al, 4(%edx)
	movzbl	5(%ecx), %eax
	cmpb	%al, 5(%edx)
	setle	%al
	decb	%al
	movb	%al, 5(%edx)
	movzbl	6(%ecx), %eax
	cmpb	%al, 6(%edx)
	setle	%al
	decb	%al
	movb	%al, 6(%edx)
	movzbl	7(%ecx), %eax
	cmpb	%al, 7(%edx)
	setle	%al
	decb	%al
	movb	%al, 7(%edx)
	movzbl	8(%ecx), %eax
	cmpb	%al, 8(%edx)
	setle	%al
	decb	%al
	movb	%al, 8(%edx)
	movzbl	9(%ecx), %eax
	cmpb	%al, 9(%edx)
	setle	%al
	decb	%al
	movb	%al, 9(%edx)
	movzbl	10(%ecx), %eax
	cmpb	%al, 10(%edx)
	setle	%al
	decb	%al
	movb	%al, 10(%edx)
	movzbl	11(%ecx), %eax
	cmpb	%al, 11(%edx)
	setle	%al
	decb	%al
	movb	%al, 11(%edx)
	movzbl	12(%ecx), %eax
	cmpb	%al, 12(%edx)
	setle	%al
	decb	%al
	movb	%al, 12(%edx)
	movzbl	13(%ecx), %eax
	cmpb	%al, 13(%edx)
	setle	%al
	decb	%al
	movb	%al, 13(%edx)
	movzbl	14(%ecx), %eax
	cmpb	%al, 14(%edx)
	setle	%al
	decb	%al
	movb	%al, 14(%edx)
	movzbl	15(%ecx), %eax
	cmpb	%al, 15(%edx)
	setle	%al
	decb	%al
	movb	%al, 15(%edx)
	ret
	.size	op_pcmpgtb_xmm, .-op_pcmpgtb_xmm
	.p2align 4,,15
.globl op_pcmpgtw_xmm
	.type	op_pcmpgtw_xmm, @function
op_pcmpgtw_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzwl	(%ecx), %eax
	cmpw	%ax, (%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, (%edx)
	movzwl	2(%ecx), %eax
	cmpw	%ax, 2(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%edx)
	movzwl	4(%ecx), %eax
	cmpw	%ax, 4(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%edx)
	movzwl	6(%ecx), %eax
	cmpw	%ax, 6(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%edx)
	movzwl	8(%ecx), %eax
	cmpw	%ax, 8(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 8(%edx)
	movzwl	10(%ecx), %eax
	cmpw	%ax, 10(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 10(%edx)
	movzwl	12(%ecx), %eax
	cmpw	%ax, 12(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 12(%edx)
	movzwl	14(%ecx), %eax
	cmpw	%ax, 14(%edx)
	setle	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 14(%edx)
	ret
	.size	op_pcmpgtw_xmm, .-op_pcmpgtw_xmm
	.p2align 4,,15
.globl op_pcmpgtl_xmm
	.type	op_pcmpgtl_xmm, @function
op_pcmpgtl_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movl	(%ecx), %eax
	cmpl	%eax, (%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, (%edx)
	movl	4(%ecx), %eax
	cmpl	%eax, 4(%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%edx)
	movl	8(%ecx), %eax
	cmpl	%eax, 8(%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%edx)
	movl	12(%ecx), %eax
	cmpl	%eax, 12(%edx)
	setle	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%edx)
	ret
	.size	op_pcmpgtl_xmm, .-op_pcmpgtl_xmm
	.p2align 4,,15
.globl op_pcmpeqb_xmm
	.type	op_pcmpeqb_xmm, @function
op_pcmpeqb_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzbl	(%ecx), %eax
	cmpb	%al, (%edx)
	setne	%al
	decb	%al
	movb	%al, (%edx)
	movzbl	1(%ecx), %eax
	cmpb	%al, 1(%edx)
	setne	%al
	decb	%al
	movb	%al, 1(%edx)
	movzbl	2(%ecx), %eax
	cmpb	%al, 2(%edx)
	setne	%al
	decb	%al
	movb	%al, 2(%edx)
	movzbl	3(%ecx), %eax
	cmpb	%al, 3(%edx)
	setne	%al
	decb	%al
	movb	%al, 3(%edx)
	movzbl	4(%ecx), %eax
	cmpb	%al, 4(%edx)
	setne	%al
	decb	%al
	movb	%al, 4(%edx)
	movzbl	5(%ecx), %eax
	cmpb	%al, 5(%edx)
	setne	%al
	decb	%al
	movb	%al, 5(%edx)
	movzbl	6(%ecx), %eax
	cmpb	%al, 6(%edx)
	setne	%al
	decb	%al
	movb	%al, 6(%edx)
	movzbl	7(%ecx), %eax
	cmpb	%al, 7(%edx)
	setne	%al
	decb	%al
	movb	%al, 7(%edx)
	movzbl	8(%ecx), %eax
	cmpb	%al, 8(%edx)
	setne	%al
	decb	%al
	movb	%al, 8(%edx)
	movzbl	9(%ecx), %eax
	cmpb	%al, 9(%edx)
	setne	%al
	decb	%al
	movb	%al, 9(%edx)
	movzbl	10(%ecx), %eax
	cmpb	%al, 10(%edx)
	setne	%al
	decb	%al
	movb	%al, 10(%edx)
	movzbl	11(%ecx), %eax
	cmpb	%al, 11(%edx)
	setne	%al
	decb	%al
	movb	%al, 11(%edx)
	movzbl	12(%ecx), %eax
	cmpb	%al, 12(%edx)
	setne	%al
	decb	%al
	movb	%al, 12(%edx)
	movzbl	13(%ecx), %eax
	cmpb	%al, 13(%edx)
	setne	%al
	decb	%al
	movb	%al, 13(%edx)
	movzbl	14(%ecx), %eax
	cmpb	%al, 14(%edx)
	setne	%al
	decb	%al
	movb	%al, 14(%edx)
	movzbl	15(%ecx), %eax
	cmpb	%al, 15(%edx)
	setne	%al
	decb	%al
	movb	%al, 15(%edx)
	ret
	.size	op_pcmpeqb_xmm, .-op_pcmpeqb_xmm
	.p2align 4,,15
.globl op_pcmpeqw_xmm
	.type	op_pcmpeqw_xmm, @function
op_pcmpeqw_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movzwl	(%ecx), %eax
	cmpw	%ax, (%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, (%edx)
	movzwl	2(%ecx), %eax
	cmpw	%ax, 2(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 2(%edx)
	movzwl	4(%ecx), %eax
	cmpw	%ax, 4(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 4(%edx)
	movzwl	6(%ecx), %eax
	cmpw	%ax, 6(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 6(%edx)
	movzwl	8(%ecx), %eax
	cmpw	%ax, 8(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 8(%edx)
	movzwl	10(%ecx), %eax
	cmpw	%ax, 10(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 10(%edx)
	movzwl	12(%ecx), %eax
	cmpw	%ax, 12(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 12(%edx)
	movzwl	14(%ecx), %eax
	cmpw	%ax, 14(%edx)
	setne	%al
	movzbw	%al, %ax
	decl	%eax
	movw	%ax, 14(%edx)
	ret
	.size	op_pcmpeqw_xmm, .-op_pcmpeqw_xmm
	.p2align 4,,15
.globl op_pcmpeql_xmm
	.type	op_pcmpeql_xmm, @function
op_pcmpeql_xmm:
	leal	__op_param2(%ebp), %ecx
	leal	__op_param1(%ebp), %edx
	movl	(%ecx), %eax
	cmpl	%eax, (%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, (%edx)
	movl	4(%ecx), %eax
	cmpl	%eax, 4(%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 4(%edx)
	movl	8(%ecx), %eax
	cmpl	%eax, 8(%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 8(%edx)
	movl	12(%ecx), %eax
	cmpl	%eax, 12(%edx)
	setne	%al
	movzbl	%al, %eax
	decl	%eax
	movl	%eax, 12(%edx)
	ret
	.size	op_pcmpeql_xmm, .-op_pcmpeql_xmm
	.p2align 4,,15
.globl op_pmullw_xmm
	.type	op_pmullw_xmm, @function
op_pmullw_xmm:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, (%esp)
	movzwl	(%eax), %ecx
	movzwl	(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, (%edx)
	movl	(%esp), %eax
	movzwl	2(%eax), %ecx
	movzwl	2(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 2(%edx)
	movl	(%esp), %eax
	movzwl	4(%eax), %ecx
	movzwl	4(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 4(%edx)
	movl	(%esp), %eax
	movzwl	6(%eax), %ecx
	movzwl	6(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 6(%edx)
	movl	(%esp), %eax
	movzwl	8(%eax), %ecx
	movzwl	8(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 8(%edx)
	movl	(%esp), %eax
	movzwl	10(%eax), %ecx
	movzwl	10(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 10(%edx)
	movl	(%esp), %eax
	movzwl	12(%eax), %ecx
	movzwl	12(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 12(%edx)
	movl	(%esp), %eax
	movzwl	14(%eax), %ecx
	movzwl	14(%edx), %eax
	imull	%ecx, %eax
	movw	%ax, 14(%edx)
	popl	%ecx
	ret
	.size	op_pmullw_xmm, .-op_pmullw_xmm
	.p2align 4,,15
.globl op_pmulhuw_xmm
	.type	op_pmulhuw_xmm, @function
op_pmulhuw_xmm:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movzwl	(%ecx), %eax
	movzwl	(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, (%ecx)
	movl	4(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%ecx)
	movl	4(%esp), %edx
	movzwl	4(%ecx), %eax
	movzwl	4(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%ecx)
	movl	4(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%ecx)
	movl	4(%esp), %edx
	movzwl	8(%ecx), %eax
	movzwl	8(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 8(%ecx)
	movl	4(%esp), %edx
	movzwl	10(%ecx), %eax
	movzwl	10(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 10(%ecx)
	movl	4(%esp), %edx
	movzwl	12(%ecx), %eax
	movzwl	12(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 12(%ecx)
	movl	4(%esp), %edx
	movzwl	14(%ecx), %eax
	movzwl	14(%edx), %edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 14(%ecx)
	addl	$8, %esp
	ret
	.size	op_pmulhuw_xmm, .-op_pmulhuw_xmm
	.p2align 4,,15
.globl op_pmulhw_xmm
	.type	op_pmulhw_xmm, @function
op_pmulhw_xmm:
	subl	$8, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	4(%esp), %edx
	movswl	(%ecx),%eax
	movswl	(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, (%ecx)
	movl	4(%esp), %edx
	movswl	2(%ecx),%eax
	movswl	2(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 2(%ecx)
	movl	4(%esp), %edx
	movswl	4(%ecx),%eax
	movswl	4(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 4(%ecx)
	movl	4(%esp), %edx
	movswl	6(%ecx),%eax
	movswl	6(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 6(%ecx)
	movl	4(%esp), %edx
	movswl	8(%ecx),%eax
	movswl	8(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 8(%ecx)
	movl	4(%esp), %edx
	movswl	10(%ecx),%eax
	movswl	10(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 10(%ecx)
	movl	4(%esp), %edx
	movswl	12(%ecx),%eax
	movswl	12(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 12(%ecx)
	movl	4(%esp), %edx
	movswl	14(%ecx),%eax
	movswl	14(%edx),%edx
	imull	%edx, %eax
	sarl	$16, %eax
	movw	%ax, 14(%ecx)
	addl	$8, %esp
	ret
	.size	op_pmulhw_xmm, .-op_pmulhw_xmm
	.p2align 4,,15
.globl op_pavgb_xmm
	.type	op_pavgb_xmm, @function
op_pavgb_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %edx
	movl	%eax, 8(%esp)
	movzbl	(%eax), %eax
	movzbl	(%edx), %ecx
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, (%edx)
	movl	8(%esp), %ecx
	movzbl	1(%edx), %eax
	movzbl	1(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 1(%edx)
	movl	8(%esp), %eax
	movzbl	2(%edx), %ecx
	movzbl	2(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 2(%edx)
	movl	8(%esp), %ecx
	movzbl	3(%edx), %eax
	movzbl	3(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 3(%edx)
	movl	8(%esp), %eax
	movzbl	4(%edx), %ecx
	movzbl	4(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 4(%edx)
	movl	8(%esp), %ecx
	movzbl	5(%edx), %eax
	movzbl	5(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 5(%edx)
	movl	8(%esp), %eax
	movzbl	6(%edx), %ecx
	movzbl	6(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 6(%edx)
	movl	8(%esp), %ecx
	movzbl	7(%edx), %eax
	movzbl	7(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 7(%edx)
	movl	8(%esp), %eax
	movzbl	8(%edx), %ecx
	movzbl	8(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 8(%edx)
	movl	8(%esp), %ecx
	movzbl	9(%edx), %eax
	movzbl	9(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 9(%edx)
	movl	8(%esp), %eax
	movzbl	10(%edx), %ecx
	movzbl	10(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 10(%edx)
	movl	8(%esp), %ecx
	movzbl	11(%edx), %eax
	movzbl	11(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 11(%edx)
	movl	8(%esp), %eax
	movzbl	12(%edx), %ecx
	movzbl	12(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 12(%edx)
	movl	8(%esp), %ecx
	movzbl	13(%edx), %eax
	movzbl	13(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 13(%edx)
	movl	8(%esp), %eax
	movzbl	14(%edx), %ecx
	movzbl	14(%eax), %eax
	leal	1(%ecx,%eax), %ecx
	movl	%ecx, 4(%esp)
	sarl	%ecx
	movb	%cl, 14(%edx)
	movl	8(%esp), %ecx
	movzbl	15(%edx), %eax
	movzbl	15(%ecx), %ecx
	leal	1(%eax,%ecx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movb	%al, 15(%edx)
	addl	$12, %esp
	ret
	.size	op_pavgb_xmm, .-op_pavgb_xmm
	.p2align 4,,15
.globl op_pavgw_xmm
	.type	op_pavgw_xmm, @function
op_pavgw_xmm:
	subl	$12, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movzwl	(%eax), %eax
	movzwl	(%ecx), %edx
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, (%ecx)
	movl	8(%esp), %edx
	movzwl	2(%ecx), %eax
	movzwl	2(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 2(%ecx)
	movl	8(%esp), %eax
	movzwl	4(%ecx), %edx
	movzwl	4(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, 4(%ecx)
	movl	8(%esp), %edx
	movzwl	6(%ecx), %eax
	movzwl	6(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 6(%ecx)
	movl	8(%esp), %eax
	movzwl	8(%ecx), %edx
	movzwl	8(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, 8(%ecx)
	movl	8(%esp), %edx
	movzwl	10(%ecx), %eax
	movzwl	10(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 10(%ecx)
	movl	8(%esp), %eax
	movzwl	12(%ecx), %edx
	movzwl	12(%eax), %eax
	leal	1(%edx,%eax), %edx
	movl	%edx, 4(%esp)
	sarl	%edx
	movw	%dx, 12(%ecx)
	movl	8(%esp), %edx
	movzwl	14(%ecx), %eax
	movzwl	14(%edx), %edx
	leal	1(%eax,%edx), %eax
	movl	%eax, 4(%esp)
	sarl	%eax
	movw	%ax, 14(%ecx)
	addl	$12, %esp
	ret
	.size	op_pavgw_xmm, .-op_pavgw_xmm
	.p2align 4,,15
.globl op_pmuludq_xmm
	.type	op_pmuludq_xmm, @function
op_pmuludq_xmm:
	subl	$28, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	movl	20(%esp), %ecx
	movl	%edx, 24(%esp)
	movl	(%eax), %eax
	mull	(%edx)
	movl	%eax, (%ecx)
	movl	8(%ecx), %eax
	movl	%edx, 4(%ecx)
	movl	24(%esp), %ecx
	mull	8(%ecx)
	movl	20(%esp), %ecx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$28, %esp
	ret
	.size	op_pmuludq_xmm, .-op_pmuludq_xmm
	.p2align 4,,15
.globl op_pmaddwd_xmm
	.type	op_pmaddwd_xmm, @function
op_pmaddwd_xmm:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	movl	%edx, 8(%esp)
	movl	%eax, 16(%esp)
	.p2align 4,,15
.L3427:
	movl	16(%esp), %eax
	movl	8(%esp), %edx
	movswl	(%edx,%eax,4),%ecx
	movl	12(%esp), %edx
	movswl	(%edx,%eax,4),%edx
	imull	%edx, %ecx
	movl	8(%esp), %edx
	movswl	2(%edx,%eax,4),%edx
	movl	%edx, 4(%esp)
	movl	12(%esp), %edx
	movswl	2(%edx,%eax,4),%edx
	movl	4(%esp), %eax
	imull	%edx, %eax
	movl	16(%esp), %edx
	addl	%eax, %ecx
	movl	12(%esp), %eax
	movl	%ecx, (%eax,%edx,4)
	incl	%edx
	cmpl	$3, %edx
	movl	%edx, 16(%esp)
	jle	.L3427
	addl	$20, %esp
	ret
	.size	op_pmaddwd_xmm, .-op_pmaddwd_xmm
	.p2align 4,,15
.globl op_psadbw_xmm
	.type	op_psadbw_xmm, @function
op_psadbw_xmm:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 8(%esp)
	movl	%edx, 4(%esp)
	movzbl	(%edx), %edx
	movzbl	(%eax), %eax
	subl	%edx, %eax
	cltd
	movl	%edx, %ecx
	xorl	%eax, %ecx
	subl	%edx, %ecx
	movl	8(%esp), %edx
	movzbl	1(%edx), %eax
	movl	4(%esp), %edx
	movzbl	1(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	2(%edx), %eax
	movl	4(%esp), %edx
	movzbl	2(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	3(%edx), %eax
	movl	4(%esp), %edx
	movzbl	3(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	4(%edx), %eax
	movl	4(%esp), %edx
	movzbl	4(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	5(%edx), %eax
	movl	4(%esp), %edx
	movzbl	5(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	6(%edx), %eax
	movl	4(%esp), %edx
	movzbl	6(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	7(%edx), %eax
	movl	4(%esp), %edx
	movzbl	7(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %ecx
	movl	8(%esp), %eax
	movl	%ecx, (%eax)
	movl	$0, 4(%eax)
	movl	8(%esp), %edx
	movl	4(%esp), %ecx
	movzbl	8(%edx), %eax
	movzbl	8(%ecx), %edx
	subl	%edx, %eax
	cltd
	movl	%edx, %ecx
	xorl	%eax, %ecx
	subl	%edx, %ecx
	movl	8(%esp), %edx
	movzbl	9(%edx), %eax
	movl	4(%esp), %edx
	movzbl	9(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	10(%edx), %eax
	movl	4(%esp), %edx
	movzbl	10(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	11(%edx), %eax
	movl	4(%esp), %edx
	movzbl	11(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	12(%edx), %eax
	movl	4(%esp), %edx
	movzbl	12(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	13(%edx), %eax
	movl	4(%esp), %edx
	movzbl	13(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	14(%edx), %eax
	movl	4(%esp), %edx
	movzbl	14(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	movl	8(%esp), %edx
	addl	%eax, %ecx
	movzbl	15(%edx), %eax
	movl	4(%esp), %edx
	movzbl	15(%edx), %edx
	subl	%edx, %eax
	cltd
	xorl	%edx, %eax
	subl	%edx, %eax
	addl	%eax, %ecx
	movl	8(%esp), %eax
	movl	%ecx, 8(%eax)
	movl	$0, 12(%eax)
	addl	$12, %esp
	ret
	.size	op_psadbw_xmm, .-op_psadbw_xmm
	.p2align 4,,15
.globl op_maskmov_xmm
	.type	op_maskmov_xmm, @function
op_maskmov_xmm:
	subl	$28, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	xorl	%eax, %eax
	movl	%edx, 16(%esp)
	movl	%eax, 24(%esp)
	.p2align 4,,15
.L3488:
	movl	16(%esp), %eax
	movl	24(%esp), %edx
	cmpb	$0, (%eax,%edx)
	jns	.L3482
	movl	20(%esp), %eax
	leal	(%edi,%edx), %ecx
	movzbl	(%eax,%edx), %eax
	movl	%ecx, %edx
	shrl	$12, %edx
	andl	$255, %edx
	movl	%eax, 12(%esp)
	movl	56(%ebp), %eax
	andl	$3, %eax
	cmpl	$3, %eax
	sete	%al
	movzbl	%al, %eax
	movl	%eax, 8(%esp)
	sall	$8, %eax
	leal	(%eax,%edx), %edx
	movl	%ecx, %eax
	sall	$4, %edx
	andl	$-4096, %eax
	cmpl	%eax, 888(%edx,%ebp)
	je	.L3484
	movzbl	12(%esp), %edx
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	movl	%ecx, %eax
	call	__stb_mmu
	jmp	.L3482
	.p2align 4,,7
.L3484:
	movl	896(%edx,%ebp), %eax
	addl	%eax, %ecx
	movzbl	12(%esp), %eax
	movl	%ecx, (%esp)
	movl	%eax, 4(%esp)
	call	remR3PhysWriteU8
	.p2align 4,,15
.L3482:
	incl	24(%esp)
	cmpl	$15, 24(%esp)
	jle	.L3488
	addl	$28, %esp
	ret
	.size	op_maskmov_xmm, .-op_maskmov_xmm
	.p2align 4,,15
.globl op_movl_mm_T0_xmm
	.type	op_movl_mm_T0_xmm, @function
op_movl_mm_T0_xmm:
	leal	__op_param1(%ebp), %eax
	movl	%ebx, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	ret
	.size	op_movl_mm_T0_xmm, .-op_movl_mm_T0_xmm
	.p2align 4,,15
.globl op_movl_T0_mm_xmm
	.type	op_movl_T0_mm_xmm, @function
op_movl_T0_mm_xmm:
	movl	__op_param1(%ebp), %ebx
	ret
	.size	op_movl_T0_mm_xmm, .-op_movl_T0_mm_xmm
	.p2align 4,,15
.globl op_shufps
	.type	op_shufps, @function
op_shufps:
	subl	$40, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, (%esp)
	movl	$__op_param3, %edx
	movl	%edx, %eax
	movl	%ecx, 20(%esp)
	movl	(%esp), %ecx
	andl	$3, %eax
	movl	(%ecx,%eax,4), %eax
	movl	%eax, 16(%esp)
	movl	%edx, %eax
	sarl	$2, %eax
	andl	$3, %eax
	sarl	$4, %edx
	movl	(%ecx,%eax,4), %eax
	movl	20(%esp), %ecx
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	andl	$3, %eax
	movl	%eax, 4(%esp)
	sarl	$2, %edx
	movl	(%ecx,%eax,4), %ecx
	movl	20(%esp), %eax
	andl	$3, %edx
	movl	%ecx, 8(%esp)
	movl	(%eax,%edx,4), %edx
	movl	(%esp), %ecx
	movl	16(%esp), %eax
	movl	%eax, (%ecx)
	movl	12(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	8(%esp), %eax
	movl	%edx, 12(%ecx)
	movl	%eax, 8(%ecx)
	addl	$40, %esp
	ret
	.size	op_shufps, .-op_shufps
	.p2align 4,,15
.globl op_shufpd
	.type	op_shufpd, @function
op_shufpd:
	movl	$__op_param3, %ecx
	subl	$24, %esp
	movl	%ecx, %edx
	leal	__op_param1(%ebp), %eax
	andl	$1, %edx
	leal	(%eax,%edx,8), %edx
	movl	%eax, 4(%esp)
	sarl	%ecx
	movl	(%edx), %eax
	movl	4(%edx), %edx
	andl	$1, %ecx
	movl	%eax, 8(%esp)
	movl	__op_param2(%ebp,%ecx,8), %eax
	movl	%edx, 12(%esp)
	movl	__op_param2+4(%ebp,%ecx,8), %edx
	movl	%eax, 16(%esp)
	movl	8(%esp), %eax
	movl	%edx, 20(%esp)
	movl	4(%esp), %edx
	movl	%eax, (%edx)
	movl	12(%esp), %eax
	movl	%eax, 4(%edx)
	movl	16(%esp), %eax
	movl	%eax, 8(%edx)
	movl	20(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$24, %esp
	ret
	.size	op_shufpd, .-op_shufpd
	.p2align 4,,15
.globl op_pshufd_xmm
	.type	op_pshufd_xmm, @function
op_pshufd_xmm:
	subl	$32, %esp
	movl	$__op_param3, %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	%edx, %eax
	leal	__op_param2(%ebp), %ecx
	andl	$3, %eax
	movl	(%ecx,%eax,4), %eax
	movl	%eax, 8(%esp)
	movl	%edx, %eax
	sarl	$2, %eax
	andl	$3, %eax
	sarl	$4, %edx
	movl	(%ecx,%eax,4), %eax
	movl	%eax, 4(%esp)
	movl	%edx, %eax
	andl	$3, %eax
	movl	(%ecx,%eax,4), %eax
	sarl	$2, %edx
	andl	$3, %edx
	movl	%eax, (%esp)
	movl	(%ecx,%edx,4), %eax
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	movl	%ecx, (%edx)
	movl	4(%esp), %ecx
	movl	%ecx, 4(%edx)
	movl	(%esp), %ecx
	movl	%eax, 12(%edx)
	movl	%ecx, 8(%edx)
	addl	$32, %esp
	ret
	.size	op_pshufd_xmm, .-op_pshufd_xmm
	.p2align 4,,15
.globl op_pshuflw_xmm
	.type	op_pshuflw_xmm, @function
op_pshuflw_xmm:
	subl	$20, %esp
	movl	$__op_param3, %edx
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	movl	%edx, %eax
	leal	__op_param2(%ebp), %ecx
	andl	$3, %eax
	movzwl	(%ecx,%eax,2), %eax
	movw	%ax, 4(%esp)
	movl	%edx, %eax
	sarl	$2, %eax
	andl	$3, %eax
	sarl	$4, %edx
	movzwl	(%ecx,%eax,2), %eax
	movw	%ax, 6(%esp)
	movl	%edx, %eax
	andl	$3, %eax
	movzwl	(%ecx,%eax,2), %eax
	sarl	$2, %edx
	andl	$3, %edx
	movw	%ax, 8(%esp)
	movzwl	(%ecx,%edx,2), %eax
	movw	%ax, 10(%esp)
	movl	8(%ecx), %eax
	movl	12(%ecx), %edx
	movl	%eax, 12(%esp)
	movl	4(%esp), %eax
	movl	%edx, 16(%esp)
	movl	(%esp), %edx
	movl	%eax, (%edx)
	movl	8(%esp), %eax
	movl	%eax, 4(%edx)
	movl	12(%esp), %eax
	movl	%eax, 8(%edx)
	movl	16(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$20, %esp
	ret
	.size	op_pshuflw_xmm, .-op_pshuflw_xmm
	.p2align 4,,15
.globl op_pshufhw_xmm
	.type	op_pshufhw_xmm, @function
op_pshufhw_xmm:
	subl	$24, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	$__op_param3, (%esp)
	movl	%eax, 4(%esp)
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	movl	%eax, 8(%esp)
	movl	$__op_param3, %eax
	andl	$3, %eax
	movl	%edx, 12(%esp)
	movzwl	8(%ecx,%eax,2), %eax
	movw	%ax, 16(%esp)
	movl	$__op_param3, %eax
	sarl	$2, %eax
	andl	$3, %eax
	movzwl	8(%ecx,%eax,2), %eax
	sarl	$4, (%esp)
	movw	%ax, 18(%esp)
	movl	(%esp), %eax
	andl	$3, %eax
	movzwl	8(%ecx,%eax,2), %eax
	sarl	$2, (%esp)
	andl	$3, (%esp)
	movw	%ax, 20(%esp)
	movl	(%esp), %edx
	movzwl	8(%ecx,%edx,2), %eax
	movl	4(%esp), %edx
	movw	%ax, 22(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%edx)
	movl	12(%esp), %eax
	movl	%eax, 4(%edx)
	movl	16(%esp), %eax
	movl	%eax, 8(%edx)
	movl	20(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$24, %esp
	ret
	.size	op_pshufhw_xmm, .-op_pshufhw_xmm
	.p2align 4,,15
.globl op_addps
	.type	op_addps, @function
op_addps:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	flds	(%edx)
	fadds	(%eax)
	fstps	(%eax)
	flds	4(%edx)
	fadds	4(%eax)
	fstps	4(%eax)
	flds	8(%edx)
	fadds	8(%eax)
	fstps	8(%eax)
	flds	12(%edx)
	fadds	12(%eax)
	fstps	12(%eax)
	ret
	.size	op_addps, .-op_addps
	.p2align 4,,15
.globl op_addss
	.type	op_addss, @function
op_addss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fadds	(%eax)
	fstps	(%eax)
	ret
	.size	op_addss, .-op_addss
	.p2align 4,,15
.globl op_addpd
	.type	op_addpd, @function
op_addpd:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	fldl	(%edx)
	faddl	(%eax)
	fstpl	(%eax)
	fldl	8(%edx)
	faddl	8(%eax)
	fstpl	8(%eax)
	ret
	.size	op_addpd, .-op_addpd
	.p2align 4,,15
.globl op_addsd
	.type	op_addsd, @function
op_addsd:
	fldl	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	faddl	(%eax)
	fstpl	(%eax)
	ret
	.size	op_addsd, .-op_addsd
	.p2align 4,,15
.globl op_subps
	.type	op_subps, @function
op_subps:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	flds	(%edx)
	fsubrs	(%eax)
	fstps	(%eax)
	flds	4(%edx)
	fsubrs	4(%eax)
	fstps	4(%eax)
	flds	8(%edx)
	fsubrs	8(%eax)
	fstps	8(%eax)
	flds	12(%edx)
	fsubrs	12(%eax)
	fstps	12(%eax)
	ret
	.size	op_subps, .-op_subps
	.p2align 4,,15
.globl op_subss
	.type	op_subss, @function
op_subss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fsubrs	(%eax)
	fstps	(%eax)
	ret
	.size	op_subss, .-op_subss
	.p2align 4,,15
.globl op_subpd
	.type	op_subpd, @function
op_subpd:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	fldl	(%edx)
	fsubrl	(%eax)
	fstpl	(%eax)
	fldl	8(%edx)
	fsubrl	8(%eax)
	fstpl	8(%eax)
	ret
	.size	op_subpd, .-op_subpd
	.p2align 4,,15
.globl op_subsd
	.type	op_subsd, @function
op_subsd:
	fldl	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fsubrl	(%eax)
	fstpl	(%eax)
	ret
	.size	op_subsd, .-op_subsd
	.p2align 4,,15
.globl op_mulps
	.type	op_mulps, @function
op_mulps:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	flds	(%edx)
	fmuls	(%eax)
	fstps	(%eax)
	flds	4(%edx)
	fmuls	4(%eax)
	fstps	4(%eax)
	flds	8(%edx)
	fmuls	8(%eax)
	fstps	8(%eax)
	flds	12(%edx)
	fmuls	12(%eax)
	fstps	12(%eax)
	ret
	.size	op_mulps, .-op_mulps
	.p2align 4,,15
.globl op_mulss
	.type	op_mulss, @function
op_mulss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fmuls	(%eax)
	fstps	(%eax)
	ret
	.size	op_mulss, .-op_mulss
	.p2align 4,,15
.globl op_mulpd
	.type	op_mulpd, @function
op_mulpd:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	fldl	(%edx)
	fmull	(%eax)
	fstpl	(%eax)
	fldl	8(%edx)
	fmull	8(%eax)
	fstpl	8(%eax)
	ret
	.size	op_mulpd, .-op_mulpd
	.p2align 4,,15
.globl op_mulsd
	.type	op_mulsd, @function
op_mulsd:
	fldl	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fmull	(%eax)
	fstpl	(%eax)
	ret
	.size	op_mulsd, .-op_mulsd
	.p2align 4,,15
.globl op_divps
	.type	op_divps, @function
op_divps:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	flds	(%edx)
	fdivrs	(%eax)
	fstps	(%eax)
	flds	4(%edx)
	fdivrs	4(%eax)
	fstps	4(%eax)
	flds	8(%edx)
	fdivrs	8(%eax)
	fstps	8(%eax)
	flds	12(%edx)
	fdivrs	12(%eax)
	fstps	12(%eax)
	ret
	.size	op_divps, .-op_divps
	.p2align 4,,15
.globl op_divss
	.type	op_divss, @function
op_divss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fdivrs	(%eax)
	fstps	(%eax)
	ret
	.size	op_divss, .-op_divss
	.p2align 4,,15
.globl op_divpd
	.type	op_divpd, @function
op_divpd:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	fldl	(%edx)
	fdivrl	(%eax)
	fstpl	(%eax)
	fldl	8(%edx)
	fdivrl	8(%eax)
	fstpl	8(%eax)
	ret
	.size	op_divpd, .-op_divpd
	.p2align 4,,15
.globl op_divsd
	.type	op_divsd, @function
op_divsd:
	fldl	__op_param2(%ebp)
	leal	__op_param1(%ebp), %eax
	fdivrl	(%eax)
	fstpl	(%eax)
	ret
	.size	op_divsd, .-op_divsd
	.p2align 4,,15
.globl op_minps
	.type	op_minps, @function
op_minps:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, (%esp)
	flds	(%eax)
	flds	(%edx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	jbe	.L3547
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3548
	.p2align 4,,7
.L3547:
	fstp	%st(0)
	movl	(%edx), %ecx
.L3548:
	movl	(%esp), %eax
	movl	%ecx, (%eax)
	flds	4(%eax)
	flds	4(%edx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	jbe	.L3550
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3551
	.p2align 4,,7
.L3550:
	fstp	%st(0)
	movl	4(%edx), %ecx
.L3551:
	movl	(%esp), %eax
	movl	%ecx, 4(%eax)
	flds	8(%eax)
	flds	8(%edx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	jbe	.L3553
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3554
	.p2align 4,,7
.L3553:
	fstp	%st(0)
	movl	8(%edx), %ecx
.L3554:
	movl	(%esp), %eax
	movl	%ecx, 8(%eax)
	flds	12(%eax)
	flds	12(%edx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	jbe	.L3556
	fstps	4(%esp)
	movl	4(%esp), %edx
	jmp	.L3557
	.p2align 4,,7
.L3556:
	fstp	%st(0)
	movl	12(%edx), %edx
.L3557:
	movl	(%esp), %eax
	movl	%edx, 12(%eax)
	addl	$8, %esp
	ret
	.size	op_minps, .-op_minps
	.p2align 4,,15
.globl op_minss
	.type	op_minss, @function
op_minss:
	subl	$4, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%edx)
	flds	(%ecx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	jbe	.L3560
	fstps	(%esp)
	movl	(%esp), %eax
	jmp	.L3561
	.p2align 4,,7
.L3560:
	fstp	%st(0)
	movl	(%ecx), %eax
.L3561:
	movl	%eax, (%edx)
	popl	%eax
	ret
	.size	op_minss, .-op_minss
	.p2align 4,,15
.globl op_minpd
	.type	op_minpd, @function
op_minpd:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	fldl	(%edx)
	fldl	(%ecx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	ja	.L3565
	fstp	%st(0)
	fldl	(%ecx)
.L3565:
	fstpl	(%edx)
	fldl	8(%edx)
	fldl	8(%ecx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	ja	.L3568
	fstp	%st(0)
	fldl	8(%ecx)
.L3568:
	fstpl	8(%edx)
	ret
	.size	op_minpd, .-op_minpd
	.p2align 4,,15
.globl op_minsd
	.type	op_minsd, @function
op_minsd:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	fldl	(%edx)
	fldl	(%ecx)
	fucomp	%st(1)
	fnstsw	%ax
	sahf
	ja	.L3572
	fstp	%st(0)
	fldl	(%ecx)
.L3572:
	fstpl	(%edx)
	ret
	.size	op_minsd, .-op_minsd
	.p2align 4,,15
.globl op_maxps
	.type	op_maxps, @function
op_maxps:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, (%esp)
	flds	(%eax)
	flds	(%edx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	jbe	.L3575
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3576
	.p2align 4,,7
.L3575:
	fstp	%st(0)
	movl	(%edx), %ecx
.L3576:
	movl	(%esp), %eax
	movl	%ecx, (%eax)
	flds	4(%eax)
	flds	4(%edx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	jbe	.L3578
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3579
	.p2align 4,,7
.L3578:
	fstp	%st(0)
	movl	4(%edx), %ecx
.L3579:
	movl	(%esp), %eax
	movl	%ecx, 4(%eax)
	flds	8(%eax)
	flds	8(%edx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	jbe	.L3581
	fstps	4(%esp)
	movl	4(%esp), %ecx
	jmp	.L3582
	.p2align 4,,7
.L3581:
	fstp	%st(0)
	movl	8(%edx), %ecx
.L3582:
	movl	(%esp), %eax
	movl	%ecx, 8(%eax)
	flds	12(%eax)
	flds	12(%edx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	jbe	.L3584
	fstps	4(%esp)
	movl	4(%esp), %edx
	jmp	.L3585
	.p2align 4,,7
.L3584:
	fstp	%st(0)
	movl	12(%edx), %edx
.L3585:
	movl	(%esp), %eax
	movl	%edx, 12(%eax)
	addl	$8, %esp
	ret
	.size	op_maxps, .-op_maxps
	.p2align 4,,15
.globl op_maxss
	.type	op_maxss, @function
op_maxss:
	subl	$4, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%edx)
	flds	(%ecx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	jbe	.L3588
	fstps	(%esp)
	movl	(%esp), %eax
	jmp	.L3589
	.p2align 4,,7
.L3588:
	fstp	%st(0)
	movl	(%ecx), %eax
.L3589:
	movl	%eax, (%edx)
	popl	%eax
	ret
	.size	op_maxss, .-op_maxss
	.p2align 4,,15
.globl op_maxpd
	.type	op_maxpd, @function
op_maxpd:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	fldl	(%edx)
	fldl	(%ecx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	ja	.L3593
	fstp	%st(0)
	fldl	(%ecx)
.L3593:
	fstpl	(%edx)
	fldl	8(%edx)
	fldl	8(%ecx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	ja	.L3596
	fstp	%st(0)
	fldl	8(%ecx)
.L3596:
	fstpl	8(%edx)
	ret
	.size	op_maxpd, .-op_maxpd
	.p2align 4,,15
.globl op_maxsd
	.type	op_maxsd, @function
op_maxsd:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	fldl	(%edx)
	fldl	(%ecx)
	fxch	%st(1)
	fucom	%st(1)
	fnstsw	%ax
	fstp	%st(1)
	sahf
	ja	.L3600
	fstp	%st(0)
	fldl	(%ecx)
.L3600:
	fstpl	(%edx)
	ret
	.size	op_maxsd, .-op_maxsd
	.p2align 4,,15
.globl op_sqrtps
	.type	op_sqrtps, @function
op_sqrtps:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	float32_sqrt
	movl	12(%esp), %eax
	fstps	(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	float32_sqrt
	movl	12(%esp), %eax
	fstps	4(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	float32_sqrt
	movl	12(%esp), %eax
	fstps	8(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	float32_sqrt
	movl	12(%esp), %eax
	fstps	12(%eax)
	addl	$16, %esp
	ret
	.size	op_sqrtps, .-op_sqrtps
	.p2align 4,,15
.globl op_sqrtss
	.type	op_sqrtss, @function
op_sqrtss:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param2(%ebp), %eax
	movl	%eax, (%esp)
	call	float32_sqrt
	movl	8(%esp), %eax
	fstps	(%eax)
	addl	$12, %esp
	ret
	.size	op_sqrtss, .-op_sqrtss
	.p2align 4,,15
.globl op_sqrtpd
	.type	op_sqrtpd, @function
op_sqrtpd:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 16(%esp)
	leal	__op_param2(%ebp), %eax
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	(%eax)
	fstpl	(%esp)
	call	float64_sqrt
	movl	16(%esp), %eax
	fstpl	(%eax)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_sqrt
	movl	16(%esp), %eax
	fstpl	8(%eax)
	addl	$20, %esp
	ret
	.size	op_sqrtpd, .-op_sqrtpd
	.p2align 4,,15
.globl op_sqrtsd
	.type	op_sqrtsd, @function
op_sqrtsd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	fldl	__op_param2(%ebp)
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	fstpl	(%esp)
	call	float64_sqrt
	movl	12(%esp), %eax
	fstpl	(%eax)
	addl	$16, %esp
	ret
	.size	op_sqrtsd, .-op_sqrtsd
	.p2align 4,,15
.globl op_cvtps2pd
	.type	op_cvtps2pd, @function
op_cvtps2pd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	leal	__op_param2(%ebp), %eax
	movl	(%eax), %edx
	flds	4(%eax)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	%edx, (%esp)
	fstps	12(%esp)
	call	float32_to_float64
	movl	8(%esp), %eax
	fstpl	(%eax)
	leal	456(%ebp), %eax
	flds	12(%esp)
	movl	%eax, 4(%esp)
	fstps	(%esp)
	call	float32_to_float64
	movl	8(%esp), %eax
	fstpl	8(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtps2pd, .-op_cvtps2pd
	.p2align 4,,15
.globl op_cvtpd2ps
	.type	op_cvtpd2ps, @function
op_cvtpd2ps:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 16(%esp)
	leal	__op_param2(%ebp), %eax
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	(%eax)
	fstpl	(%esp)
	call	float64_to_float32
	movl	16(%esp), %eax
	fstps	(%eax)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_to_float32
	movl	16(%esp), %eax
	fstps	4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	addl	$20, %esp
	ret
	.size	op_cvtpd2ps, .-op_cvtpd2ps
	.p2align 4,,15
.globl op_cvtss2sd
	.type	op_cvtss2sd, @function
op_cvtss2sd:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param2(%ebp), %eax
	movl	%eax, (%esp)
	call	float32_to_float64
	movl	8(%esp), %eax
	fstpl	(%eax)
	addl	$12, %esp
	ret
	.size	op_cvtss2sd, .-op_cvtss2sd
	.p2align 4,,15
.globl op_cvtsd2ss
	.type	op_cvtsd2ss, @function
op_cvtsd2ss:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	fldl	__op_param2(%ebp)
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	fstpl	(%esp)
	call	float64_to_float32
	movl	12(%esp), %eax
	fstps	(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtsd2ss, .-op_cvtsd2ss
	.p2align 4,,15
.globl op_cvtdq2ps
	.type	op_cvtdq2ps, @function
op_cvtdq2ps:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	4(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	8(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	12(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtdq2ps, .-op_cvtdq2ps
	.p2align 4,,15
.globl op_cvtdq2pd
	.type	op_cvtdq2pd, @function
op_cvtdq2pd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 12(%esp)
	leal	__op_param2(%ebp), %eax
	movl	(%eax), %edx
	movl	4(%eax), %eax
	movl	%edx, (%esp)
	movl	%eax, 8(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	call	int32_to_float64
	movl	12(%esp), %eax
	fstpl	(%eax)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%esp), %eax
	movl	%eax, (%esp)
	call	int32_to_float64
	movl	12(%esp), %eax
	fstpl	8(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtdq2pd, .-op_cvtdq2pd
	.p2align 4,,15
.globl op_cvtpi2ps
	.type	op_cvtpi2ps, @function
op_cvtpi2ps:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float32
	movl	12(%esp), %eax
	fstps	4(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtpi2ps, .-op_cvtpi2ps
	.p2align 4,,15
.globl op_cvtpi2pd
	.type	op_cvtpi2pd, @function
op_cvtpi2pd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float64
	movl	12(%esp), %eax
	fstpl	(%eax)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	int32_to_float64
	movl	12(%esp), %eax
	fstpl	8(%eax)
	addl	$16, %esp
	ret
	.size	op_cvtpi2pd, .-op_cvtpi2pd
	.p2align 4,,15
.globl op_cvtsi2ss
	.type	op_cvtsi2ss, @function
op_cvtsi2ss:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	%ebx, (%esp)
	call	int32_to_float32
	movl	8(%esp), %eax
	fstps	(%eax)
	addl	$12, %esp
	ret
	.size	op_cvtsi2ss, .-op_cvtsi2ss
	.p2align 4,,15
.globl op_cvtsi2sd
	.type	op_cvtsi2sd, @function
op_cvtsi2sd:
	subl	$12, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 8(%esp)
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	%ebx, (%esp)
	call	int32_to_float64
	movl	8(%esp), %eax
	fstpl	(%eax)
	addl	$12, %esp
	ret
	.size	op_cvtsi2sd, .-op_cvtsi2sd
	.p2align 4,,15
.globl op_cvtps2dq
	.type	op_cvtps2dq, @function
op_cvtps2dq:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, (%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, 4(%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, 8(%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_cvtps2dq, .-op_cvtps2dq
	.p2align 4,,15
.globl op_cvtpd2dq
	.type	op_cvtpd2dq, @function
op_cvtpd2dq:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 16(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 8(%esp)
	fldl	(%edx)
	fstpl	(%esp)
	call	float64_to_int32
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_to_int32
	movl	16(%esp), %edx
	movl	%eax, 4(%edx)
	movl	$0, 8(%edx)
	movl	$0, 12(%edx)
	addl	$20, %esp
	ret
	.size	op_cvtpd2dq, .-op_cvtpd2dq
	.p2align 4,,15
.globl op_cvtps2pi
	.type	op_cvtps2pi, @function
op_cvtps2pi:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, (%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	12(%esp), %edx
	movl	%eax, 4(%edx)
	addl	$16, %esp
	ret
	.size	op_cvtps2pi, .-op_cvtps2pi
	.p2align 4,,15
.globl op_cvtpd2pi
	.type	op_cvtpd2pi, @function
op_cvtpd2pi:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 16(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 8(%esp)
	fldl	(%edx)
	fstpl	(%esp)
	call	float64_to_int32
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_to_int32
	movl	16(%esp), %edx
	movl	%eax, 4(%edx)
	addl	$20, %esp
	ret
	.size	op_cvtpd2pi, .-op_cvtpd2pi
	.p2align 4,,15
.globl op_cvtss2si
	.type	op_cvtss2si, @function
op_cvtss2si:
	subl	$8, %esp
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	call	float32_to_int32
	movl	%eax, %ebx
	addl	$8, %esp
	ret
	.size	op_cvtss2si, .-op_cvtss2si
	.p2align 4,,15
.globl op_cvtsd2si
	.type	op_cvtsd2si, @function
op_cvtsd2si:
	subl	$12, %esp
	leal	456(%ebp), %eax
	fldl	__op_param1(%ebp)
	movl	%eax, 8(%esp)
	fstpl	(%esp)
	call	float64_to_int32
	movl	%eax, %ebx
	addl	$12, %esp
	ret
	.size	op_cvtsd2si, .-op_cvtsd2si
	.p2align 4,,15
.globl op_cvttps2dq
	.type	op_cvttps2dq, @function
op_cvttps2dq:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, (%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, 4(%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, 8(%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_cvttps2dq, .-op_cvttps2dq
	.p2align 4,,15
.globl op_cvttpd2dq
	.type	op_cvttpd2dq, @function
op_cvttpd2dq:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 16(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 8(%esp)
	fldl	(%edx)
	fstpl	(%esp)
	call	float64_to_int32_round_to_zero
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_to_int32_round_to_zero
	movl	16(%esp), %edx
	movl	%eax, 4(%edx)
	movl	$0, 8(%edx)
	movl	$0, 12(%edx)
	addl	$20, %esp
	ret
	.size	op_cvttpd2dq, .-op_cvttpd2dq
	.p2align 4,,15
.globl op_cvttps2pi
	.type	op_cvttps2pi, @function
op_cvttps2pi:
	subl	$16, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 12(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, (%edx)
	movl	8(%esp), %edx
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	12(%esp), %edx
	movl	%eax, 4(%edx)
	addl	$16, %esp
	ret
	.size	op_cvttps2pi, .-op_cvttps2pi
	.p2align 4,,15
.globl op_cvttpd2pi
	.type	op_cvttpd2pi, @function
op_cvttpd2pi:
	subl	$20, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 16(%esp)
	leal	456(%ebp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, 8(%esp)
	fldl	(%edx)
	fstpl	(%esp)
	call	float64_to_int32_round_to_zero
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	leal	456(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	12(%esp), %eax
	fldl	8(%eax)
	fstpl	(%esp)
	call	float64_to_int32_round_to_zero
	movl	16(%esp), %edx
	movl	%eax, 4(%edx)
	addl	$20, %esp
	ret
	.size	op_cvttpd2pi, .-op_cvttpd2pi
	.p2align 4,,15
.globl op_cvttss2si
	.type	op_cvttss2si, @function
op_cvttss2si:
	subl	$8, %esp
	leal	456(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	call	float32_to_int32_round_to_zero
	movl	%eax, %ebx
	addl	$8, %esp
	ret
	.size	op_cvttss2si, .-op_cvttss2si
	.p2align 4,,15
.globl op_cvttsd2si
	.type	op_cvttsd2si, @function
op_cvttsd2si:
	subl	$12, %esp
	leal	456(%ebp), %eax
	fldl	__op_param1(%ebp)
	movl	%eax, 8(%esp)
	fstpl	(%esp)
	call	float64_to_int32_round_to_zero
	movl	%eax, %ebx
	addl	$12, %esp
	ret
	.size	op_cvttsd2si, .-op_cvttsd2si
	.p2align 4,,15
.globl op_rsqrtps
	.type	op_rsqrtps, @function
op_rsqrtps:
	subl	$12, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, 8(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rsqrt
	movl	8(%esp), %eax
	fstps	(%eax)
	movl	4(%esp), %edx
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rsqrt
	movl	8(%esp), %eax
	fstps	4(%eax)
	movl	4(%esp), %edx
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rsqrt
	movl	8(%esp), %eax
	fstps	8(%eax)
	movl	4(%esp), %edx
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rsqrt
	movl	8(%esp), %eax
	fstps	12(%eax)
	addl	$12, %esp
	ret
	.size	op_rsqrtps, .-op_rsqrtps
	.p2align 4,,15
.globl op_rsqrtss
	.type	op_rsqrtss, @function
op_rsqrtss:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param2(%ebp), %eax
	movl	%eax, (%esp)
	call	approx_rsqrt
	movl	4(%esp), %eax
	fstps	(%eax)
	addl	$8, %esp
	ret
	.size	op_rsqrtss, .-op_rsqrtss
	.p2align 4,,15
.globl op_rcpps
	.type	op_rcpps, @function
op_rcpps:
	subl	$12, %esp
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, 8(%esp)
	movl	(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rcp
	movl	8(%esp), %eax
	fstps	(%eax)
	movl	4(%esp), %edx
	movl	4(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rcp
	movl	8(%esp), %eax
	fstps	4(%eax)
	movl	4(%esp), %edx
	movl	8(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rcp
	movl	8(%esp), %eax
	fstps	8(%eax)
	movl	4(%esp), %edx
	movl	12(%edx), %eax
	movl	%eax, (%esp)
	call	approx_rcp
	movl	8(%esp), %eax
	fstps	12(%eax)
	addl	$12, %esp
	ret
	.size	op_rcpps, .-op_rcpps
	.p2align 4,,15
.globl op_rcpss
	.type	op_rcpss, @function
op_rcpss:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	__op_param2(%ebp), %eax
	movl	%eax, (%esp)
	call	approx_rcp
	movl	4(%esp), %eax
	fstps	(%eax)
	addl	$8, %esp
	ret
	.size	op_rcpss, .-op_rcpss
	.p2align 4,,15
.globl op_haddps
	.type	op_haddps, @function
op_haddps:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %eax
	flds	4(%edx)
	fadds	(%edx)
	fstps	(%esp)
	flds	12(%edx)
	fadds	8(%edx)
	fstps	4(%esp)
	flds	4(%eax)
	fadds	(%eax)
	fstps	8(%esp)
	flds	12(%eax)
	fadds	8(%eax)
	movl	(%esp), %eax
	fstps	12(%esp)
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_haddps, .-op_haddps
	.p2align 4,,15
.globl op_haddpd
	.type	op_haddpd, @function
op_haddpd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	fldl	8(%edx)
	faddl	(%edx)
	fstpl	(%esp)
	movl	(%esp), %eax
	fldl	__op_param2(%ebp)
	faddl	__op_param2+8(%ebp)
	fstpl	8(%esp)
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_haddpd, .-op_haddpd
	.p2align 4,,15
.globl op_hsubps
	.type	op_hsubps, @function
op_hsubps:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %eax
	flds	4(%edx)
	fsubrs	(%edx)
	fstps	(%esp)
	flds	12(%edx)
	fsubrs	8(%edx)
	fstps	4(%esp)
	flds	4(%eax)
	fsubrs	(%eax)
	fstps	8(%esp)
	flds	12(%eax)
	fsubrs	8(%eax)
	movl	(%esp), %eax
	fstps	12(%esp)
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_hsubps, .-op_hsubps
	.p2align 4,,15
.globl op_hsubpd
	.type	op_hsubpd, @function
op_hsubpd:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	fldl	8(%edx)
	fsubrl	(%edx)
	fstpl	(%esp)
	movl	(%esp), %eax
	fldl	__op_param2(%ebp)
	fsubl	__op_param2+8(%ebp)
	fstpl	8(%esp)
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_hsubpd, .-op_hsubpd
	.p2align 4,,15
.globl op_addsubps
	.type	op_addsubps, @function
op_addsubps:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	flds	(%edx)
	fsubrs	(%eax)
	fstps	(%eax)
	flds	4(%edx)
	fadds	4(%eax)
	fstps	4(%eax)
	flds	8(%edx)
	fsubrs	8(%eax)
	fstps	8(%eax)
	flds	12(%edx)
	fadds	12(%eax)
	fstps	12(%eax)
	ret
	.size	op_addsubps, .-op_addsubps
	.p2align 4,,15
.globl op_addsubpd
	.type	op_addsubpd, @function
op_addsubpd:
	leal	__op_param2(%ebp), %edx
	leal	__op_param1(%ebp), %eax
	fldl	(%edx)
	fsubrl	(%eax)
	fstpl	(%eax)
	fldl	8(%edx)
	faddl	8(%eax)
	fstpl	8(%eax)
	ret
	.size	op_addsubpd, .-op_addsubpd
	.p2align 4,,15
.globl op_cmpeqps
	.type	op_cmpeqps, @function
op_cmpeqps:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	flds	(%eax)
	flds	(%ecx)
	fucompp
	fnstsw	%ax
	flds	4(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	flds	4(%eax)
	fucompp
	fnstsw	%ax
	flds	8(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 4(%ecx)
	movl	(%esp), %eax
	flds	8(%eax)
	fucompp
	fnstsw	%ax
	flds	12(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 8(%ecx)
	movl	(%esp), %eax
	flds	12(%eax)
	fucompp
	fnstsw	%ax
	sahf
	sete	%al
	setnp	%dl
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 12(%ecx)
	popl	%eax
	ret
	.size	op_cmpeqps, .-op_cmpeqps
	.p2align 4,,15
.globl op_cmpeqss
	.type	op_cmpeqss, @function
op_cmpeqss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %ecx
	flds	(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	sete	%al
	setnp	%dl
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%ecx)
	ret
	.size	op_cmpeqss, .-op_cmpeqss
	.p2align 4,,15
.globl op_cmpeqpd
	.type	op_cmpeqpd, @function
op_cmpeqpd:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	%ecx, (%esp)
	fldl	(%eax)
	fldl	(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	testb	$1, %al
	sete	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	fldl	8(%ecx)
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	testb	$1, %al
	sete	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$8, %esp
	ret
	.size	op_cmpeqpd, .-op_cmpeqpd
	.p2align 4,,15
.globl op_cmpeqsd
	.type	op_cmpeqsd, @function
op_cmpeqsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	testb	$1, %al
	sete	%cl
	movzbl	%cl, %eax
	movl	(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%edx
	ret
	.size	op_cmpeqsd, .-op_cmpeqsd
	.p2align 4,,15
.globl op_cmpltps
	.type	op_cmpltps, @function
op_cmpltps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpltps, .-op_cmpltps
	.p2align 4,,15
.globl op_cmpltss
	.type	op_cmpltss, @function
op_cmpltss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpltss, .-op_cmpltss
	.p2align 4,,15
.globl op_cmpltpd
	.type	op_cmpltpd, @function
op_cmpltpd:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	%ecx, (%esp)
	fldl	(%eax)
	fldl	(%ecx)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	fldl	8(%ecx)
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$8, %esp
	ret
	.size	op_cmpltpd, .-op_cmpltpd
	.p2align 4,,15
.globl op_cmpltsd
	.type	op_cmpltsd, @function
op_cmpltsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%ecx
	ret
	.size	op_cmpltsd, .-op_cmpltsd
	.p2align 4,,15
.globl op_cmpleps
	.type	op_cmpleps, @function
op_cmpleps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpleps, .-op_cmpleps
	.p2align 4,,15
.globl op_cmpless
	.type	op_cmpless, @function
op_cmpless:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpless, .-op_cmpless
	.p2align 4,,15
.globl op_cmplepd
	.type	op_cmplepd, @function
op_cmplepd:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	%ecx, (%esp)
	fldl	(%eax)
	fldl	(%ecx)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	fldl	8(%ecx)
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$8, %esp
	ret
	.size	op_cmplepd, .-op_cmplepd
	.p2align 4,,15
.globl op_cmplesd
	.type	op_cmplesd, @function
op_cmplesd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	movzbl	%cl, %eax
	movl	(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%eax
	ret
	.size	op_cmplesd, .-op_cmplesd
	.p2align 4,,15
.globl op_cmpunordps
	.type	op_cmpunordps, @function
op_cmpunordps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpunordps, .-op_cmpunordps
	.p2align 4,,15
.globl op_cmpunordss
	.type	op_cmpunordss, @function
op_cmpunordss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fucompp
	fnstsw	%ax
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	notl	%eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpunordss, .-op_cmpunordss
	.p2align 4,,15
.globl op_cmpunordpd
	.type	op_cmpunordpd, @function
op_cmpunordpd:
	subl	$8, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %ecx
	movl	%eax, 4(%esp)
	movl	%ecx, (%esp)
	fldl	(%eax)
	fldl	(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%edx, 4(%ecx)
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	fldl	8(%ecx)
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	movzbl	%cl, %eax
	movl	4(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, 8(%ecx)
	movl	%edx, 12(%ecx)
	addl	$8, %esp
	ret
	.size	op_cmpunordpd, .-op_cmpunordpd
	.p2align 4,,15
.globl op_cmpunordsd
	.type	op_cmpunordsd, @function
op_cmpunordsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	movzbl	%cl, %eax
	movl	(%esp), %ecx
	xorl	%edx, %edx
	addl	$-1, %eax
	adcl	$-1, %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%eax
	ret
	.size	op_cmpunordsd, .-op_cmpunordsd
	.p2align 4,,15
.globl op_cmpneqps
	.type	op_cmpneqps, @function
op_cmpneqps:
	subl	$4, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	flds	(%eax)
	flds	(%ecx)
	fucompp
	fnstsw	%ax
	flds	4(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%ecx)
	movl	(%esp), %eax
	flds	4(%eax)
	fucompp
	fnstsw	%ax
	flds	8(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 4(%ecx)
	movl	(%esp), %eax
	flds	8(%eax)
	fucompp
	fnstsw	%ax
	flds	12(%ecx)
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 8(%ecx)
	movl	(%esp), %eax
	flds	12(%eax)
	fucompp
	fnstsw	%ax
	sahf
	sete	%al
	setnp	%dl
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 12(%ecx)
	popl	%eax
	ret
	.size	op_cmpneqps, .-op_cmpneqps
	.p2align 4,,15
.globl op_cmpneqss
	.type	op_cmpneqss, @function
op_cmpneqss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %ecx
	flds	(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	sete	%al
	setnp	%dl
	andb	%dl, %al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%ecx)
	ret
	.size	op_cmpneqss, .-op_cmpneqss
	.p2align 4,,15
.globl op_cmpneqpd
	.type	op_cmpneqpd, @function
op_cmpneqpd:
	subl	$24, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	movl	%edx, 16(%esp)
	fldl	(%edx)
	fldl	(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	xorl	%ecx, %ecx
	testb	$1, %al
	sete	%cl
	xorl	%eax, %eax
	movl	%ecx, (%esp)
	xorl	%edx, %edx
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	movl	20(%esp), %ecx
	movl	%eax, 8(%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	subl	(%esp), %eax
	fldl	8(%ecx)
	sbbl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %eax
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	testb	$1, %al
	sete	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	subl	%eax, 8(%esp)
	movl	20(%esp), %eax
	sbbl	%edx, 12(%esp)
	movl	8(%esp), %edx
	movl	12(%esp), %ecx
	movl	%edx, 8(%eax)
	movl	%ecx, 12(%eax)
	addl	$24, %esp
	ret
	.size	op_cmpneqpd, .-op_cmpneqpd
	.p2align 4,,15
.globl op_cmpneqsd
	.type	op_cmpneqsd, @function
op_cmpneqsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%dl
	sete	%al
	andb	%dl, %al
	testb	$1, %al
	sete	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	movl	(%esp), %ecx
	negl	%eax
	adcl	$0, %edx
	negl	%edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%edx
	ret
	.size	op_cmpneqsd, .-op_cmpneqsd
	.p2align 4,,15
.globl op_cmpnltps
	.type	op_cmpnltps, @function
op_cmpnltps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpnltps, .-op_cmpnltps
	.p2align 4,,15
.globl op_cmpnltss
	.type	op_cmpnltss, @function
op_cmpnltss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpnltss, .-op_cmpnltss
	.p2align 4,,15
.globl op_cmpnltpd
	.type	op_cmpnltpd, @function
op_cmpnltpd:
	subl	$24, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	xorl	%ecx, %ecx
	movl	%edx, 16(%esp)
	fldl	(%edx)
	fldl	(%eax)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	%eax, 8(%esp)
	xorl	%eax, %eax
	movl	%ecx, (%esp)
	xorl	%ecx, %ecx
	subl	(%esp), %eax
	movl	%ecx, 12(%esp)
	movl	20(%esp), %ecx
	sbbl	4(%esp), %edx
	movl	%eax, (%ecx)
	fldl	8(%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %eax
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	subl	%eax, 8(%esp)
	movl	20(%esp), %eax
	sbbl	%edx, 12(%esp)
	movl	8(%esp), %edx
	movl	12(%esp), %ecx
	movl	%edx, 8(%eax)
	movl	%ecx, 12(%eax)
	addl	$24, %esp
	ret
	.size	op_cmpnltpd, .-op_cmpnltpd
	.p2align 4,,15
.globl op_cmpnltsd
	.type	op_cmpnltsd, @function
op_cmpnltsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	testb	$69, %ah
	setne	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	movl	(%esp), %ecx
	negl	%eax
	adcl	$0, %edx
	negl	%edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%eax
	ret
	.size	op_cmpnltsd, .-op_cmpnltsd
	.p2align 4,,15
.globl op_cmpnleps
	.type	op_cmpnleps, @function
op_cmpnleps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpnleps, .-op_cmpnleps
	.p2align 4,,15
.globl op_cmpnless
	.type	op_cmpnless, @function
op_cmpnless:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	sete	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpnless, .-op_cmpnless
	.p2align 4,,15
.globl op_cmpnlepd
	.type	op_cmpnlepd, @function
op_cmpnlepd:
	subl	$24, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	xorl	%ecx, %ecx
	movl	%edx, 16(%esp)
	fldl	(%edx)
	fldl	(%eax)
	fxch	%st(1)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	%eax, 8(%esp)
	xorl	%eax, %eax
	movl	%eax, 12(%esp)
	xorl	%eax, %eax
	movl	%ecx, (%esp)
	movl	20(%esp), %ecx
	subl	(%esp), %eax
	fldl	8(%ecx)
	sbbl	4(%esp), %edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %eax
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	subl	%eax, 8(%esp)
	movl	20(%esp), %eax
	sbbl	%edx, 12(%esp)
	movl	8(%esp), %edx
	movl	12(%esp), %ecx
	movl	%edx, 8(%eax)
	movl	%ecx, 12(%eax)
	addl	$24, %esp
	ret
	.size	op_cmpnlepd, .-op_cmpnlepd
	.p2align 4,,15
.globl op_cmpnlesd
	.type	op_cmpnlesd, @function
op_cmpnlesd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	testb	$5, %ah
	setne	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	movl	(%esp), %ecx
	negl	%eax
	adcl	$0, %edx
	negl	%edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%edx
	ret
	.size	op_cmpnlesd, .-op_cmpnlesd
	.p2align 4,,15
.globl op_cmpordps
	.type	op_cmpordps, @function
op_cmpordps:
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	flds	(%ecx)
	flds	(%edx)
	fucompp
	fnstsw	%ax
	flds	4(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	flds	4(%ecx)
	fucompp
	fnstsw	%ax
	flds	8(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 4(%edx)
	flds	8(%ecx)
	fucompp
	fnstsw	%ax
	flds	12(%edx)
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 8(%edx)
	flds	12(%ecx)
	fucompp
	fnstsw	%ax
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, 12(%edx)
	ret
	.size	op_cmpordps, .-op_cmpordps
	.p2align 4,,15
.globl op_cmpordss
	.type	op_cmpordss, @function
op_cmpordss:
	flds	__op_param2(%ebp)
	leal	__op_param1(%ebp), %edx
	flds	(%edx)
	fucompp
	fnstsw	%ax
	sahf
	setp	%al
	movzbl	%al, %eax
	cmpl	$1, %eax
	sbbl	%eax, %eax
	movl	%eax, (%edx)
	ret
	.size	op_cmpordss, .-op_cmpordss
	.p2align 4,,15
.globl op_cmpordpd
	.type	op_cmpordpd, @function
op_cmpordpd:
	subl	$24, %esp
	leal	__op_param1(%ebp), %eax
	leal	__op_param2(%ebp), %edx
	movl	%eax, 20(%esp)
	xorl	%ecx, %ecx
	movl	%edx, 16(%esp)
	fldl	(%edx)
	fldl	(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	xorl	%eax, %eax
	movl	%eax, 4(%esp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	%eax, 8(%esp)
	xorl	%eax, %eax
	movl	%ecx, (%esp)
	xorl	%ecx, %ecx
	subl	(%esp), %eax
	movl	%ecx, 12(%esp)
	movl	20(%esp), %ecx
	sbbl	4(%esp), %edx
	movl	%eax, (%ecx)
	fldl	8(%ecx)
	movl	%edx, 4(%ecx)
	movl	16(%esp), %eax
	fldl	8(%eax)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	subl	%eax, 8(%esp)
	movl	20(%esp), %eax
	sbbl	%edx, 12(%esp)
	movl	8(%esp), %edx
	movl	12(%esp), %ecx
	movl	%edx, 8(%eax)
	movl	%ecx, 12(%eax)
	addl	$24, %esp
	ret
	.size	op_cmpordpd, .-op_cmpordpd
	.p2align 4,,15
.globl op_cmpordsd
	.type	op_cmpordsd, @function
op_cmpordsd:
	subl	$4, %esp
	leal	__op_param1(%ebp), %eax
	movl	%eax, (%esp)
	fldl	(%eax)
	fldl	__op_param2(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	setnp	%cl
	movzbl	%cl, %eax
	xorl	%edx, %edx
	movl	(%esp), %ecx
	negl	%eax
	adcl	$0, %edx
	negl	%edx
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	popl	%eax
	ret
	.size	op_cmpordsd, .-op_cmpordsd
	.p2align 4,,15
.globl op_ucomiss
	.type	op_ucomiss, @function
op_ucomiss:
	subl	$12, %esp
	leal	456(%ebp), %eax
	movl	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movl	__op_param2(%ebp), %edx
	movl	%ecx, (%esp)
	movl	%edx, 4(%esp)
	call	float32_compare_quiet
	movl	comis_eflags+4(,%eax,4), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_ucomiss, .-op_ucomiss
	.p2align 4,,15
.globl op_comiss
	.type	op_comiss, @function
op_comiss:
	subl	$12, %esp
	leal	456(%ebp), %eax
	movl	__op_param1(%ebp), %ecx
	movl	%eax, 8(%esp)
	movl	__op_param2(%ebp), %edx
	movl	%ecx, (%esp)
	movl	%edx, 4(%esp)
	call	float32_compare
	movl	comis_eflags+4(,%eax,4), %eax
	movl	%eax, 40(%ebp)
	addl	$12, %esp
	ret
	.size	op_comiss, .-op_comiss
	.p2align 4,,15
.globl op_ucomisd
	.type	op_ucomisd, @function
op_ucomisd:
	subl	$20, %esp
	leal	456(%ebp), %eax
	fldl	__op_param1(%ebp)
	fldl	__op_param2(%ebp)
	fxch	%st(1)
	movl	%eax, 16(%esp)
	fstpl	(%esp)
	fstpl	8(%esp)
	call	float64_compare_quiet
	movl	comis_eflags+4(,%eax,4), %eax
	movl	%eax, 40(%ebp)
	addl	$20, %esp
	ret
	.size	op_ucomisd, .-op_ucomisd
	.p2align 4,,15
.globl op_comisd
	.type	op_comisd, @function
op_comisd:
	subl	$20, %esp
	leal	456(%ebp), %eax
	fldl	__op_param1(%ebp)
	fldl	__op_param2(%ebp)
	fxch	%st(1)
	movl	%eax, 16(%esp)
	fstpl	(%esp)
	fstpl	8(%esp)
	call	float64_compare
	movl	comis_eflags+4(,%eax,4), %eax
	movl	%eax, 40(%ebp)
	addl	$20, %esp
	ret
	.size	op_comisd, .-op_comisd
	.p2align 4,,15
.globl op_movmskps
	.type	op_movmskps, @function
op_movmskps:
	subl	$4, %esp
	leal	__op_param1(%ebp), %ecx
	movl	(%ecx), %eax
	shrl	$31, %eax
	movl	%eax, (%esp)
	movl	4(%ecx), %eax
	movl	8(%ecx), %edx
	movl	12(%ecx), %ecx
	shrl	$31, %eax
	addl	%eax, %eax
	orl	(%esp), %eax
	shrl	$31, %edx
	sall	$2, %edx
	shrl	$31, %ecx
	sall	$3, %ecx
	orl	%edx, %eax
	movl	%eax, %ebx
	orl	%ecx, %ebx
	popl	%eax
	ret
	.size	op_movmskps, .-op_movmskps
	.p2align 4,,15
.globl op_movmskpd
	.type	op_movmskpd, @function
op_movmskpd:
	leal	__op_param1(%ebp), %eax
	movl	4(%eax), %edx
	movl	12(%eax), %eax
	shrl	$31, %edx
	shrl	$31, %eax
	addl	%eax, %eax
	movl	%eax, %ebx
	orl	%edx, %ebx
	ret
	.size	op_movmskpd, .-op_movmskpd
	.p2align 4,,15
.globl op_pmovmskb_xmm
	.type	op_pmovmskb_xmm, @function
op_pmovmskb_xmm:
	leal	__op_param1(%ebp), %edx
	movzbl	(%edx), %eax
	shrb	$7, %al
	movzbl	%al, %ebx
	movzbl	1(%edx), %eax
	shrb	$6, %al
	andl	$2, %eax
	orl	%eax, %ebx
	movzbl	2(%edx), %eax
	shrb	$5, %al
	andl	$4, %eax
	orl	%eax, %ebx
	movzbl	3(%edx), %eax
	shrb	$4, %al
	andl	$8, %eax
	orl	%eax, %ebx
	movzbl	4(%edx), %eax
	shrb	$3, %al
	andl	$16, %eax
	orl	%eax, %ebx
	movzbl	5(%edx), %eax
	shrb	$2, %al
	andl	$32, %eax
	orl	%eax, %ebx
	movzbl	6(%edx), %eax
	shrb	%al
	andl	$64, %eax
	orl	%eax, %ebx
	movzbl	7(%edx), %eax
	andl	$128, %eax
	orl	%eax, %ebx
	movzbl	8(%edx), %eax
	addl	%eax, %eax
	andl	$256, %eax
	orl	%eax, %ebx
	movzbl	9(%edx), %eax
	sall	$2, %eax
	andl	$512, %eax
	orl	%eax, %ebx
	movzbl	10(%edx), %eax
	sall	$3, %eax
	andl	$1024, %eax
	orl	%eax, %ebx
	movzbl	11(%edx), %eax
	sall	$4, %eax
	andl	$2048, %eax
	orl	%eax, %ebx
	movzbl	12(%edx), %eax
	sall	$5, %eax
	andl	$4096, %eax
	orl	%eax, %ebx
	movzbl	13(%edx), %eax
	sall	$6, %eax
	andl	$8192, %eax
	orl	%eax, %ebx
	movzbl	14(%edx), %eax
	sall	$7, %eax
	andl	$16384, %eax
	orl	%eax, %ebx
	movzbl	15(%edx), %eax
	sall	$8, %eax
	andl	$32768, %eax
	orl	%eax, %ebx
	ret
	.size	op_pmovmskb_xmm, .-op_pmovmskb_xmm
	.p2align 4,,15
.globl op_pinsrw_xmm
	.type	op_pinsrw_xmm, @function
op_pinsrw_xmm:
	movl	$__op_param2, %eax
	movw	%bx, __op_param1(%ebp,%eax,2)
	ret
	.size	op_pinsrw_xmm, .-op_pinsrw_xmm
	.p2align 4,,15
.globl op_pextrw_xmm
	.type	op_pextrw_xmm, @function
op_pextrw_xmm:
	movl	$__op_param2, %eax
	movzwl	__op_param1(%ebp,%eax,2), %ebx
	ret
	.size	op_pextrw_xmm, .-op_pextrw_xmm
	.p2align 4,,15
.globl op_packsswb_xmm
	.type	op_packsswb_xmm, @function
op_packsswb_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movl	$-128, %edx
	movswl	(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3873
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3873
	movl	%eax, %edx
	.p2align 4,,15
.L3873:
	movb	%dl, 4(%esp)
	movl	$-128, %edx
	movswl	2(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3878
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3878
	movl	%eax, %edx
	.p2align 4,,15
.L3878:
	movb	%dl, 5(%esp)
	movl	$-128, %edx
	movswl	4(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3883
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3883
	movl	%eax, %edx
	.p2align 4,,15
.L3883:
	movb	%dl, 6(%esp)
	movl	$-128, %edx
	movswl	6(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3888
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3888
	movl	%eax, %edx
	.p2align 4,,15
.L3888:
	movb	%dl, 7(%esp)
	movl	$-128, %edx
	movswl	8(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3893
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3893
	movl	%eax, %edx
	.p2align 4,,15
.L3893:
	movb	%dl, 8(%esp)
	movl	$-128, %edx
	movswl	10(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3898
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3898
	movl	%eax, %edx
	.p2align 4,,15
.L3898:
	movb	%dl, 9(%esp)
	movl	$-128, %edx
	movswl	12(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3903
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3903
	movl	%eax, %edx
	.p2align 4,,15
.L3903:
	movb	%dl, 10(%esp)
	movl	$-128, %edx
	movswl	14(%ecx),%eax
	cmpl	$-128, %eax
	jl	.L3908
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3908
	movl	%eax, %edx
	.p2align 4,,15
.L3908:
	movb	%dl, 11(%esp)
	movl	(%esp), %edx
	movswl	(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3913
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3913
	movl	%eax, %edx
	.p2align 4,,15
.L3913:
	movb	%dl, 12(%esp)
	movl	(%esp), %edx
	movswl	2(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3918
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3918
	movl	%eax, %edx
	.p2align 4,,15
.L3918:
	movb	%dl, 13(%esp)
	movl	(%esp), %edx
	movswl	4(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3923
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3923
	movl	%eax, %edx
	.p2align 4,,15
.L3923:
	movb	%dl, 14(%esp)
	movl	(%esp), %edx
	movswl	6(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3928
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3928
	movl	%eax, %edx
	.p2align 4,,15
.L3928:
	movb	%dl, 15(%esp)
	movl	(%esp), %edx
	movswl	8(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3933
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3933
	movl	%eax, %edx
	.p2align 4,,15
.L3933:
	movb	%dl, 16(%esp)
	movl	(%esp), %edx
	movswl	10(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3938
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3938
	movl	%eax, %edx
	.p2align 4,,15
.L3938:
	movb	%dl, 17(%esp)
	movl	(%esp), %edx
	movswl	12(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3943
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3943
	movl	%eax, %edx
	.p2align 4,,15
.L3943:
	movb	%dl, 18(%esp)
	movl	(%esp), %edx
	movswl	14(%edx),%eax
	movl	$-128, %edx
	cmpl	$-128, %eax
	jl	.L3948
	cmpl	$127, %eax
	movl	$127, %edx
	jg	.L3948
	movl	%eax, %edx
	.p2align 4,,15
.L3948:
	movb	%dl, 19(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%ecx)
	movl	8(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	12(%esp), %eax
	movl	%eax, 8(%ecx)
	movl	16(%esp), %eax
	movl	%eax, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_packsswb_xmm, .-op_packsswb_xmm
	.p2align 4,,15
.globl op_packuswb_xmm
	.type	op_packuswb_xmm, @function
op_packuswb_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	xorl	%edx, %edx
	movswl	(%ecx),%eax
	testl	%eax, %eax
	js	.L3954
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3954
	movl	%eax, %edx
.L3954:
	movb	%dl, 4(%esp)
	xorl	%edx, %edx
	movswl	2(%ecx),%eax
	testl	%eax, %eax
	js	.L3959
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3959
	movl	%eax, %edx
.L3959:
	movb	%dl, 5(%esp)
	xorl	%edx, %edx
	movswl	4(%ecx),%eax
	testl	%eax, %eax
	js	.L3964
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3964
	movl	%eax, %edx
.L3964:
	movb	%dl, 6(%esp)
	xorl	%edx, %edx
	movswl	6(%ecx),%eax
	testl	%eax, %eax
	js	.L3969
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3969
	movl	%eax, %edx
.L3969:
	movb	%dl, 7(%esp)
	xorl	%edx, %edx
	movswl	8(%ecx),%eax
	testl	%eax, %eax
	js	.L3974
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3974
	movl	%eax, %edx
.L3974:
	movb	%dl, 8(%esp)
	xorl	%edx, %edx
	movswl	10(%ecx),%eax
	testl	%eax, %eax
	js	.L3979
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3979
	movl	%eax, %edx
.L3979:
	movb	%dl, 9(%esp)
	xorl	%edx, %edx
	movswl	12(%ecx),%eax
	testl	%eax, %eax
	js	.L3984
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3984
	movl	%eax, %edx
.L3984:
	movb	%dl, 10(%esp)
	xorl	%edx, %edx
	movswl	14(%ecx),%eax
	testl	%eax, %eax
	js	.L3989
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3989
	movl	%eax, %edx
.L3989:
	movb	%dl, 11(%esp)
	movl	(%esp), %edx
	movswl	(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L3994
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3994
	movl	%eax, %edx
.L3994:
	movb	%dl, 12(%esp)
	movl	(%esp), %edx
	movswl	2(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L3999
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L3999
	movl	%eax, %edx
.L3999:
	movb	%dl, 13(%esp)
	movl	(%esp), %edx
	movswl	4(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4004
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4004
	movl	%eax, %edx
.L4004:
	movb	%dl, 14(%esp)
	movl	(%esp), %edx
	movswl	6(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4009
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4009
	movl	%eax, %edx
.L4009:
	movb	%dl, 15(%esp)
	movl	(%esp), %edx
	movswl	8(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4014
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4014
	movl	%eax, %edx
.L4014:
	movb	%dl, 16(%esp)
	movl	(%esp), %edx
	movswl	10(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4019
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4019
	movl	%eax, %edx
.L4019:
	movb	%dl, 17(%esp)
	movl	(%esp), %edx
	movswl	12(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4024
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4024
	movl	%eax, %edx
.L4024:
	movb	%dl, 18(%esp)
	movl	(%esp), %edx
	movswl	14(%edx),%eax
	xorl	%edx, %edx
	testl	%eax, %eax
	js	.L4029
	cmpl	$255, %eax
	movl	$255, %edx
	jg	.L4029
	movl	%eax, %edx
.L4029:
	movb	%dl, 19(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%ecx)
	movl	8(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	12(%esp), %eax
	movl	%eax, 8(%ecx)
	movl	16(%esp), %eax
	movl	%eax, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_packuswb_xmm, .-op_packuswb_xmm
	.p2align 4,,15
.globl op_packssdw_xmm
	.type	op_packssdw_xmm, @function
op_packssdw_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %eax
	leal	__op_param1(%ebp), %ecx
	movl	%eax, (%esp)
	movl	$-32768, %edx
	movl	(%ecx), %eax
	cmpl	$-32768, %eax
	jl	.L4035
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4035
	movl	%eax, %edx
	.p2align 4,,15
.L4035:
	movw	%dx, 4(%esp)
	movl	4(%ecx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4040
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4040
	movl	%eax, %edx
	.p2align 4,,15
.L4040:
	movw	%dx, 6(%esp)
	movl	8(%ecx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4045
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4045
	movl	%eax, %edx
	.p2align 4,,15
.L4045:
	movw	%dx, 8(%esp)
	movl	12(%ecx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4050
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4050
	movl	%eax, %edx
	.p2align 4,,15
.L4050:
	movw	%dx, 10(%esp)
	movl	(%esp), %edx
	movl	(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4055
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4055
	movl	%eax, %edx
	.p2align 4,,15
.L4055:
	movw	%dx, 12(%esp)
	movl	(%esp), %edx
	movl	4(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4060
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4060
	movl	%eax, %edx
	.p2align 4,,15
.L4060:
	movw	%dx, 14(%esp)
	movl	(%esp), %edx
	movl	8(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4065
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4065
	movl	%eax, %edx
	.p2align 4,,15
.L4065:
	movw	%dx, 16(%esp)
	movl	(%esp), %edx
	movl	12(%edx), %eax
	movl	$-32768, %edx
	cmpl	$-32768, %eax
	jl	.L4070
	cmpl	$32767, %eax
	movl	$32767, %edx
	jg	.L4070
	movl	%eax, %edx
	.p2align 4,,15
.L4070:
	movw	%dx, 18(%esp)
	movl	4(%esp), %eax
	movl	%eax, (%ecx)
	movl	8(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	12(%esp), %eax
	movl	%eax, 8(%ecx)
	movl	16(%esp), %eax
	movl	%eax, 12(%ecx)
	addl	$20, %esp
	ret
	.size	op_packssdw_xmm, .-op_packssdw_xmm
	.p2align 4,,15
.globl op_punpcklbw_xmm
	.type	op_punpcklbw_xmm, @function
op_punpcklbw_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	(%edx), %eax
	movb	%al, (%esp)
	movzbl	(%ecx), %eax
	movb	%al, 1(%esp)
	movzbl	1(%edx), %eax
	movb	%al, 2(%esp)
	movzbl	1(%ecx), %eax
	movb	%al, 3(%esp)
	movzbl	2(%edx), %eax
	movb	%al, 4(%esp)
	movzbl	2(%ecx), %eax
	movb	%al, 5(%esp)
	movzbl	3(%edx), %eax
	movb	%al, 6(%esp)
	movzbl	3(%ecx), %eax
	movb	%al, 7(%esp)
	movzbl	4(%edx), %eax
	movb	%al, 8(%esp)
	movzbl	4(%ecx), %eax
	movb	%al, 9(%esp)
	movzbl	5(%edx), %eax
	movb	%al, 10(%esp)
	movzbl	5(%ecx), %eax
	movb	%al, 11(%esp)
	movzbl	6(%edx), %eax
	movb	%al, 12(%esp)
	movzbl	6(%ecx), %eax
	movb	%al, 13(%esp)
	movzbl	7(%edx), %eax
	movb	%al, 14(%esp)
	movzbl	7(%ecx), %eax
	movb	%al, 15(%esp)
	movl	(%esp), %eax
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_punpcklbw_xmm, .-op_punpcklbw_xmm
	.p2align 4,,15
.globl op_punpcklwd_xmm
	.type	op_punpcklwd_xmm, @function
op_punpcklwd_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzwl	(%edx), %eax
	movw	%ax, (%esp)
	movzwl	(%ecx), %eax
	movw	%ax, 2(%esp)
	movzwl	2(%edx), %eax
	movw	%ax, 4(%esp)
	movzwl	2(%ecx), %eax
	movw	%ax, 6(%esp)
	movzwl	4(%edx), %eax
	movw	%ax, 8(%esp)
	movzwl	4(%ecx), %eax
	movw	%ax, 10(%esp)
	movzwl	6(%edx), %eax
	movw	%ax, 12(%esp)
	movzwl	6(%ecx), %eax
	movw	%ax, 14(%esp)
	movl	(%esp), %eax
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_punpcklwd_xmm, .-op_punpcklwd_xmm
	.p2align 4,,15
.globl op_punpckldq_xmm
	.type	op_punpckldq_xmm, @function
op_punpckldq_xmm:
	subl	$24, %esp
	leal	__op_param2(%ebp), %edx
	movl	(%edx), %ecx
	leal	__op_param1(%ebp), %eax
	movl	%ecx, (%esp)
	movl	4(%eax), %ecx
	movl	%ecx, 4(%esp)
	movl	(%esp), %ecx
	movl	4(%edx), %edx
	movl	%ecx, 4(%eax)
	movl	4(%esp), %ecx
	movl	%edx, 12(%eax)
	movl	%ecx, 8(%eax)
	addl	$24, %esp
	ret
	.size	op_punpckldq_xmm, .-op_punpckldq_xmm
	.p2align 4,,15
.globl op_punpcklqdq_xmm
	.type	op_punpcklqdq_xmm, @function
op_punpcklqdq_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %ecx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	movl	%eax, (%esp)
	movl	__op_param2(%ebp), %eax
	movl	%edx, 4(%esp)
	movl	__op_param2+4(%ebp), %edx
	movl	%eax, 8(%esp)
	movl	(%esp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, (%ecx)
	movl	4(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	8(%esp), %eax
	movl	%eax, 8(%ecx)
	movl	12(%esp), %eax
	movl	%eax, 12(%ecx)
	addl	$16, %esp
	ret
	.size	op_punpcklqdq_xmm, .-op_punpcklqdq_xmm
	.p2align 4,,15
.globl op_punpckhbw_xmm
	.type	op_punpckhbw_xmm, @function
op_punpckhbw_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzbl	8(%edx), %eax
	movb	%al, (%esp)
	movzbl	8(%ecx), %eax
	movb	%al, 1(%esp)
	movzbl	9(%edx), %eax
	movb	%al, 2(%esp)
	movzbl	9(%ecx), %eax
	movb	%al, 3(%esp)
	movzbl	10(%edx), %eax
	movb	%al, 4(%esp)
	movzbl	10(%ecx), %eax
	movb	%al, 5(%esp)
	movzbl	11(%edx), %eax
	movb	%al, 6(%esp)
	movzbl	11(%ecx), %eax
	movb	%al, 7(%esp)
	movzbl	12(%edx), %eax
	movb	%al, 8(%esp)
	movzbl	12(%ecx), %eax
	movb	%al, 9(%esp)
	movzbl	13(%edx), %eax
	movb	%al, 10(%esp)
	movzbl	13(%ecx), %eax
	movb	%al, 11(%esp)
	movzbl	14(%edx), %eax
	movb	%al, 12(%esp)
	movzbl	14(%ecx), %eax
	movb	%al, 13(%esp)
	movzbl	15(%edx), %eax
	movb	%al, 14(%esp)
	movzbl	15(%ecx), %eax
	movb	%al, 15(%esp)
	movl	(%esp), %eax
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_punpckhbw_xmm, .-op_punpckhbw_xmm
	.p2align 4,,15
.globl op_punpckhwd_xmm
	.type	op_punpckhwd_xmm, @function
op_punpckhwd_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %edx
	leal	__op_param2(%ebp), %ecx
	movzwl	8(%edx), %eax
	movw	%ax, (%esp)
	movzwl	8(%ecx), %eax
	movw	%ax, 2(%esp)
	movzwl	10(%edx), %eax
	movw	%ax, 4(%esp)
	movzwl	10(%ecx), %eax
	movw	%ax, 6(%esp)
	movzwl	12(%edx), %eax
	movw	%ax, 8(%esp)
	movzwl	12(%ecx), %eax
	movw	%ax, 10(%esp)
	movzwl	14(%edx), %eax
	movw	%ax, 12(%esp)
	movzwl	14(%ecx), %eax
	movw	%ax, 14(%esp)
	movl	(%esp), %eax
	movl	%eax, (%edx)
	movl	4(%esp), %eax
	movl	%eax, 4(%edx)
	movl	8(%esp), %eax
	movl	%eax, 8(%edx)
	movl	12(%esp), %eax
	movl	%eax, 12(%edx)
	addl	$16, %esp
	ret
	.size	op_punpckhwd_xmm, .-op_punpckhwd_xmm
	.p2align 4,,15
.globl op_punpckhdq_xmm
	.type	op_punpckhdq_xmm, @function
op_punpckhdq_xmm:
	subl	$20, %esp
	leal	__op_param2(%ebp), %edx
	movl	8(%edx), %ecx
	leal	__op_param1(%ebp), %eax
	movl	%ecx, (%esp)
	movl	8(%eax), %ecx
	movl	12(%edx), %edx
	movl	%ecx, (%eax)
	movl	(%esp), %ecx
	movl	%ecx, 4(%eax)
	movl	12(%eax), %ecx
	movl	%edx, 12(%eax)
	movl	%ecx, 8(%eax)
	addl	$20, %esp
	ret
	.size	op_punpckhdq_xmm, .-op_punpckhdq_xmm
	.p2align 4,,15
.globl op_punpckhqdq_xmm
	.type	op_punpckhqdq_xmm, @function
op_punpckhqdq_xmm:
	subl	$16, %esp
	leal	__op_param1(%ebp), %ecx
	movl	8(%ecx), %eax
	movl	12(%ecx), %edx
	movl	%eax, (%esp)
	movl	__op_param2+8(%ebp), %eax
	movl	%edx, 4(%esp)
	movl	__op_param2+12(%ebp), %edx
	movl	%eax, 8(%esp)
	movl	(%esp), %eax
	movl	%edx, 12(%esp)
	movl	%eax, (%ecx)
	movl	4(%esp), %eax
	movl	%eax, 4(%ecx)
	movl	8(%esp), %eax
	movl	%eax, 8(%ecx)
	movl	12(%esp), %eax
	movl	%eax, 12(%ecx)
	addl	$16, %esp
	ret
	.size	op_punpckhqdq_xmm, .-op_punpckhqdq_xmm
	.section	.note.GNU-stack,"",@progbits
	.ident	"GCC: (GNU) 3.4.6 (Gentoo 3.4.6-r1, ssp-3.4.5-1.0, pie-8.7.9)"
