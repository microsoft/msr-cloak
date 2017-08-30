include ksamd64.inc

.DATA
.CODE

;int execTrans(void*)
LEAF_ENTRY execTrans, _TEXT$00
	mov rax, success ; return address
	xbegin failed
	jmp rcx 
success:
	xend
	; write XBEGIN_STARTED to rax
	xor rax, rax
	not rax
	ret 
failed:
    ret
LEAF_END execTrans, _TEXT$00

;int exec2Trans(void*)
LEAF_ENTRY exec2Trans, _TEXT$00
	mov rax, next ; return address
	xbegin failed
	jmp rcx 
next:
	mov rax, success
	jmp rcx
success:
	xend
	; write XBEGIN_STARTED to rax
	xor rax, rax
	not rax
	ret 
failed:
    ret
LEAF_END exec2Trans, _TEXT$00

;int execTransRead1(void*)
LEAF_ENTRY execTransRead1, _TEXT$00
	mov rax, success ; return address
	xbegin failed
	mov rdx, [rsp] ; put one cache line into the read set.
	jmp rcx 
success:
	xend
	; write XBEGIN_STARTED to rax
	xor rax, rax
	not rax
	ret 
failed:
    ret
LEAF_END execTransRead1, _TEXT$00

END