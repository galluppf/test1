
;;------------------------------------------------------------------------------
;;
;; sark_init.s	    Initialisation & assembly code for SARK
;;
;; Copyright (C)    The University of Manchester - 2010, 2011
;;
;; Author           Steve Temple, APT Group, School of Computer Science
;; Email            temples@cs.man.ac.uk
;;
;;------------------------------------------------------------------------------

		get	spinnaker.s

		preserve8

;------------------------------------------------------------------------------

; Run time error codes - should move to spinnaker.h....

RTE_RESET	equ	0x10
RTE_UNDEF	equ	0x11
RTE_SVC		equ	0x12
RTE_PABT	equ	0x13
RTE_DABT	equ	0x14
RTE_ADDR	equ	0x15
RTE_IRQ		equ	0x16
RTE_FIQ		equ	0x17


;------------------------------------------------------------------------------

		area	sark_aplx, readonly, code

	if	:def: GNU

		import	RO_TO
		import	RO_FROM
		import	RO_LENGTH
		import	RW_TO
		import	RW_FROM
		import	RW_LENGTH
		import	ZI_TO
		import	ZI_LENGTH

aplx_table	dcd	APLX_RCOPY
		dcd	RO_TO
		dcd	RO_FROM
		dcd	RO_LENGTH

      		dcd	APLX_RCOPY
		dcd	RW_TO
		dcd	RW_FROM
		dcd	RW_LENGTH

		dcd	APLX_FILL
		dcd	ZI_TO
		dcd	ZI_LENGTH
		dcd	0

		dcd	APLX_EXEC
		dcd	RO_TO
		dcd	0
		dcd	0
	else
		import	||Image$$APLX$$RO$$Limit||	; End of APLX block

		import	||Image$$ITCM$$RO$$Base||	; Base/len of ITCM
		import	||Image$$ITCM$$RO$$Length||	; code image
		import	||Image$$ITCM$$RO$$Limit||	; code image

		import	||Image$$DTCM$$RW$$Base||	; Base/len of DTCM
		import	||Image$$DTCM$$RW$$Length||	; RW data

		import	||Image$$DTCM$$ZI$$Base||	; Base/len of DTCM
		import	||Image$$DTCM$$ZI$$Length||	; ZI data

aplx_table	dcd	APLX_RCOPY
		dcd	||Image$$ITCM$$RO$$Base||	; Copy to
		dcd	||Image$$APLX$$RO$$Limit|| - aplx_table	; Copy from
		dcd	||Image$$ITCM$$RO$$Length||	; Copy length

aplx_1		dcd	APLX_RCOPY
		dcd	||Image$$DTCM$$RW$$Base||
		dcd	||Image$$APLX$$RO$$Limit|| + ||Image$$ITCM$$RO$$Length|| - aplx_1
		dcd	||Image$$DTCM$$RW$$Length||

		dcd	APLX_FILL
		dcd	||Image$$DTCM$$ZI$$Base||	; Zero init area
		dcd	||Image$$DTCM$$ZI$$Length||
		dcd	0

		dcd	APLX_EXEC
		dcd	||Image$$ITCM$$RO$$Base||	; Start address
		dcd	0
		dcd	0
	endif
	
;------------------------------------------------------------------------------

		area	sark_init, readonly, code

		import	sark_setup
		import	sark_error
		import	svc_putc

;------------------------------------------------------------------------------

		ldr	pc, reset_vec		; Reset		     
		ldr	pc, undef_vec		; Undefined instr    
		ldr	pc, svc_vec		; SVC		     
		ldr	pc, pabt_vec		; Prefetch abort     
		ldr	pc, dabt_vec		; Data abort	     
		ldr	pc, addr_vec		; (Address exception)
		ldr	pc, [pc, #-0xff0]	; IRQ (via VIC)
		ldr	pc, fiq_vec  		; FIQ

reset_vec	dcd	reset_han			
undef_vec	dcd	undef_err
svc_vec  	dcd	svc_han			
pabt_vec 	dcd	pabt_err
dabt_vec 	dcd	dabt_err
addr_vec	dcd	addr_err		
irq_vec  	dcd	irq_err
fiq_vec  	dcd	fiq_err
	
RTS_STACK	equ	4096
C_STACK		equ	DTCM_TOP-RTS_STACK	;; C code (user mode) stack

reset_han	msr	cpsr_c, #IMASK_ALL+MODE_IRQ	; IRQ mode, ints off
		ldr	sp, =IRQ_STACK			; Set up stack

		msr	cpsr_c, #IMASK_ALL+MODE_FIQ	; FIQ mode, ints off
		ldr	sp, =FIQ_STACK			; Set up stack

		msr	cpsr_c, #IMASK_ALL+MODE_SVC	; SVC mode, ints off
		ldr	sp, =SVC_STACK			; Set up stack

		msr	cpsr_c, #IMASK_ALL+MODE_SYS	; SYS mode, ints off
		ldr	sp, =C_STACK			; Set up stack

		adr	r0, reset_err
		adr	r1, reset_vec
		str	r0, [r1, #0]

		bl	sark_setup		; Interrupts on in here

		if	:def: SARK_API

		import	rts_init
		bl	rts_init

		endif

		import	c_main
		bl 	c_main			; Call C main procedure

		import	rts_cleanup
		bl	rts_cleanup

		b	snooze


;------------------------------------------------------------------------------


reset_err	mov	r0, #RTE_RESET
		b	sark_error

undef_err	mov	r0, #RTE_UNDEF
		b	sark_error

svc_err		mov	r0, #RTE_SVC
		b	sark_error

pabt_err	mov	r0, #RTE_PABT
		b	sark_error

dabt_err	mov	r0, #RTE_DABT
		b	sark_error

addr_err	mov	r0, #RTE_ADDR
		b	sark_error

irq_err		mov	r0, #RTE_IRQ
		b	sark_error

fiq_err		mov	r0, #RTE_FIQ
		b	sark_error


;------------------------------------------------------------------------------


svc_han		push	{r2-r3, r12, lr}	; Save registers

		mrs     r12, spsr    	  	; Get SPSR
		push	{r1, r12}		; Save SPSR, R1

		tst	r12, #THUMB_BIT		; Thumb mode?

		ldrne	r12, [lr, #-2]	  	; Get appropriate data
		ldreq	r12, [lr, #-4]	  	; 
		ands	r12, r12, #255		; and form SVC number

		beq	svc_tube 		; SVC 0 - Tube
		b	svc_err			; otherwise error

svc_tube	bl	svc_putc		; Char in r0 to stream in r1

svc_exit	pop	{r1, r12}		; Restore SPSR, r1
		msr	spsr_cxsf, r12

		ldmfd	sp!, {r2-r3, r12, pc}^	; and return


;------------------------------------------------------------------------------

		export	null_int

null_int	subs	pc, lr, #4

;------------------------------------------------------------------------------


		import	cpu_int_c
		export	cpu_int

cpu_int		sub  	lr, lr, #4		; Adjust LR_irq and save
    		push	{r0, lr}		; with r0

    		mrs    	lr, spsr		; Get SPSR_irq to LR
    		push	{r12, lr}  		; Save SPSR & r12

    		msr    	cpsr_c, #MODE_SYS    	; Go to SYS mode, interrupts enabled

    		push	{r1-r3, lr} 		; Save working regs and LR_svc

    		bl     	cpu_int_c

    		pop	{r1-r3, lr}		; Restore working regs & LR_svc

    		msr    	cpsr_c, #MODE_IRQ+IMASK_IRQ ; Back to IRQ mode, IRQ disabled

		mov    	r12, #VIC_BASE		; Tell VIC we are done
		str    	r12, [r12, #VIC_VADDR]

    		pop	{r12, lr} 	      	; Restore r12 & SPSR_irq
    		msr    	spsr_cxsf, lr

    		ldmfd  	sp!, {r0, pc}^       	; and return restoring r0


;------------------------------------------------------------------------------

 		area 	sark_alib, readonly, code

; void bx (uint addr)

		export	bx
bx     	  	proc

		bx	r0			; Branch to r0

		endp

;------------------------------------------------------------------------------

; void copy_msg (msg_t *to, msg_t *from)

		export	copy_msg
copy_msg	proc

		ldrh     r2, [r1, #4]		; Get length (bytes)
		add	 r0, r0, #4		; Point past link word (from)
		add	 r1, r1, #4		; Point past link word (to)
		add	 r2, r2, #4		; Bump length by 4 (len/sum)

		endp

; Fast copy for word aligned buffers. Byte count "n" need not
; be a multiple of 4 but it will be rounded up to be so.

; void word_cpy (void *dest, const void *src, uint n)

		export	word_cpy
word_cpy	proc

		tst	r2, #3
  		bicne	r2, r2, #3
  		addne	r2, r2, #4

		push    {r4, lr}

	  	subs    r2, r2, #32
  		bcc     wc2

wc1		ldm	r1!, {r3, r4, r12, lr}
  		stm     r0!, {r3, r4, r12, lr}
  		ldm     r1!, {r3, r4, r12, lr}
  		stm     r0!, {r3, r4, r12, lr}
  		subs    r2, r2, #32
  		bcs     wc1

wc2		lsls    r12, r2, #28
  		ldmcs   r1!, {r3, r4, r12, lr}
  		stmcs   r0!, {r3, r4, r12, lr}
  		ldmmi   r1!, {r3, r4}
  		stmmi   r0!, {r3, r4}

  		lsls    r12, r2, #30
  		ldrcs   r3, [r1], #4
  		strcs   r3, [r0], #4

  		pop     {r4, pc}

		endp

;------------------------------------------------------------------------------

; uint cpu_int_off (void);

		export	cpu_int_off
cpu_int_off	proc

		mrs	r0, cpsr
		orr	r1, r0, #IMASK_ALL
		msr	cpsr_c, r1
		bx	lr

		endp

; void cpu_mode_set (uint cpsr);

		export	cpu_mode_set
cpu_mode_set	proc

		msr	cpsr_c, r0
		bx	lr

		endp
;------------------------------------------------------------------------------

		if	:def: THUMB

; uint irq_enable (void);

		export	irq_enable
irq_enable	proc

		mrs	r0, cpsr
		bic	r1, r0, #0x80
		msr	cpsr_c, r1
		bx	lr

		endp

; uint spin1_irq_disable (void);

		export	spin1_irq_disable
spin1_irq_disable	proc

		mrs	r0, cpsr
		orr	r1, r0, #0x80
		msr	cpsr_c, r1
		bx	lr

		endp

; void spin1_irq_restore (uint cpsr);

		export	spin1_irq_restore
spin1_irq_restore	proc

		msr	cpsr_c, r0
		bx	lr

		endp

;  void wait_for_irq ();

		export	wait_for_irq
wait_for_irq	proc

		mcr 	p15, 0, r0, c7, c0, 4
		bx	lr

		endp

		endif

;------------------------------------------------------------------------------

; void snooze (void);

       	        export	snooze
snooze		proc

		mcr 	p15, 0, r0, c7, c0, 4
		b	snooze

		endp

;------------------------------------------------------------------------------

; void str_cpy (char *dest, const char *src);

		export	str_cpy
str_cpy		proc

		ldrb	 r2, [r1], #1
		cmp	 r2, #0
		strb	 r2, [r0], #1
		bne	 str_cpy
		bx	 lr

		endp

;------------------------------------------------------------------------------

; divmod_t div10 (uint n);

		export	div10
div10		proc

		if	:def: GNU

		sub    r2, r1, #10 		
		sub    r1, r1, r1, lsr #2 	
		add    r1, r1, r1, lsr #4  	
		add    r1, r1, r1, lsr #8  	
		add    r1, r1, r1, lsr #16 	
		mov    r1, r1, lsr #3   	
		add    r3, r1, r1, lsl #2 	
		subs   r2, r2, r3, lsl #1 	
		addpl  r1, r1, #1       	
		addmi  r2, r2, #10
		stm    r0, {r1, r2}
		bx     lr

		else

    		sub    r1, r0, #10             ; keep (x-10) for later 
    		sub    r0, r0, r0, lsr #2 
    		add    r0, r0, r0, lsr #4 
    		add    r0, r0, r0, lsr #8 
    		add    r0, r0, r0, lsr #16 
    		mov    r0, r0, lsr #3 
    		add    r2, r0, r0, lsl #2 
    		subs   r1, r1, r2, lsl #1      ; calc (x-10) - (x/10)*10 
    		addpl  r0, r0, #1              ; fix-up quotient 
    		addmi  r1, r1, #10             ; fix-up remainder 
    		bx     lr 

		endif

		endp

;------------------------------------------------------------------------------

; void svc_put_char (uint c, char *stream);

		if	:def: GNU

		export	svc_put_char
svc_put_char	proc

		svc	#0
		bx	lr

		endp

		endif

;------------------------------------------------------------------------------

; void schedule (uchar event_id, uint arg0, uint arg1)

		if	:def: SARK_API

       		import	schedule_sysmode
		export	schedule

schedule	proc
		
		push	{r12, lr}		; save r12 and lr_irq

		mrs	r12, cpsr		; Go to SYS mode
		bic	lr, r12, #0x1f
		orr	lr, lr, #MODE_SYS
		msr	cpsr_c, lr

		push	{r12, lr}		; save lr_sys and cpsr_c

		bl	schedule_sysmode

        	pop	{r12, lr}		; restore lr_sys and cpsr_c
		msr	cpsr_c, r12		; back to IRQ mode

		pop	{r12, lr}		; restore r12 and lr_irq
		bx	lr			; return using lr_irq

		endp

		endif

;------------------------------------------------------------------------------

; APLX Loader

; Format of APLX table - each entry except last is 4 words. First word
; is an opcode - ACOPY, RCOPY, FILL, EXEC, END. Next three words are operands.
; Copy/Fill always rounds up to multiple of 32 bytes and zero length is
; not sensible.

;	APLX_ACOPY, APLX_RCOPY
;	Dest address
;	Srce address (relative to start of this entry if RCOPY)
;	Length (bytes)

;	APLX_FILL
;	Dest address
;	Length (bytes)
;	Fill data

;	APLX_EXEC
;	Program counter
;	(Unused)
;	(Unused)

;	APLX_END (or invalid opcode)
;	(Unused)
;	(Unused)
;	(Unused)

; void proc_aplx (uint *table, uint dummy);

		code16

		export	proc_aplx
proc_aplx	proc

		push	{r4-r7, lr}		; Save link and r4-7
		ldr	r1, =ITCM_TOP_64	; r1 -> destination
		adr	r2, aplx_loader		; r2 -> loader code
		mov	r3, #4			; Move 16 words...

ap0		ldm	r2!, {r4-r7}		; ... four at a time
		stm	r1!, {r4-r7}
		sub	r3, #1
		bne	ap0

		sub	r1, #63			; r1 -> ITCM_TOP - 64 + 1
		bx	r1

; enter with table address in r0

  	     	align	4

aplx_loader	ldm	r0!, {r1-r4}		; Get opcode & operands

		cmp	r1, #APLX_ACOPY		; Copy absolute
		beq	aplx_copy
		cmp	r1, #APLX_RCOPY		; Copy relative
		beq	aplx_rcopy
		cmp	r1, #APLX_FILL		; Fill
		beq	aplx_fill
		cmp	r1, #APLX_EXEC		; Execute
		beq	aplx_exec

		pop	{r4-r7, pc}		; Restore & return

aplx_rcopy	add	r3, r0			; Make pointer relative and
		sub	r3, #APLX_SIZE		; reduce by table entry size

aplx_copy	ldm	r3!, {r1, r5-r7}
		stm	r2!, {r1, r5-r7}
		ldm	r3!, {r1, r5-r7}
		stm	r2!, {r1, r5-r7}
		sub	r4, #32
		bhi	aplx_copy
		b	aplx_loader

aplx_fill	movs	r5, r4
		movs	r6, r4
		movs	r7, r4

ap1		stm     r2!, {r4-r7}
  		stm     r2!, {r4-r7}
  		sub     r3, #32
  		bhi     ap1
		b	aplx_loader

aplx_exec	blx	r2
		b	aplx_loader

		endp

		align	4

;------------------------------------------------------------------------------

; This section is placed last, after all other code to be copied to ITCM.
; It forces the length of the section to be a multiple of 4 bytes as it
; was previously Thumb aligned and this caused problems in copying the RW
; data that followed. This is a bodge!

		area	sark_align, readonly, code
	
		dcd	DEAD_WORD

;------------------------------------------------------------------------------
		end
