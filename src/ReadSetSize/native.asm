include ksamd64.inc

meow0 SEGMENT read execute align(64)

align 64
readMem proc
	xor r8, r8
	xbegin failed
loop_head:
	cmp r8,rdx
	jge loop_end
	mov rax,[rcx+r8]
	add r8, 64
	jmp loop_head
loop_end:
	xend
	; write XBEGIN_STARTED to rax
	xor rax, rax
	not rax
failed:
	ret 

readMem endp

meow0 ends

END