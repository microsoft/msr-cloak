include ksamd64.inc

aligned SEGMENT read execute align(64)

align 64
nopfunc proc
	nop
	ret
    
nopfunc endp

align 64
udfunc proc
	ud2
	ud2
	ud2
	ud2
	ud2
	ud2
	ud2
udfunc endp

align 64
dummyfunc proc
	nop
	ret
    
dummyfunc endp

recurse proc
inc rax
dec	rcx
jz endrec
call recurse
endrec:
ret
recurse endp

jmpExec32 proc

test rcx, rcx
jnz tag0
ret
tag0:
jnz tag1
ret
tag1:

jnz tag2
ret
tag2:

jnz tag3
ret
tag3:

jnz tag4
ret
tag4:

jnz tag5
ret
tag5:

jnz tag6
ret
tag6:

jnz tag7
ret
tag7:

jnz tag8
ret
tag8:

jnz tag9
ret
tag9:

jnz tag10
ret
tag10:

jnz tag11
ret
tag11:

jnz tag12
ret
tag12:

jnz tag13
ret
tag13:

jnz tag14
ret
tag14:

jnz tag15
ret
tag15:

jnz tag16
ret
tag16:

jnz tag17
ret
tag17:

jnz tag18
ret
tag18:

jnz tag19
ret
tag19:

jnz tag20
ret
tag20:

jnz tag21
ret
tag21:

jnz tag22
ret
tag22:

jnz tag23
ret
tag23:

jnz tag24
ret
tag24:

jnz tag25
ret
tag25:

jnz tag26
ret
tag26:

jnz tag27
ret
tag27:

jnz tag28
ret
tag28:

jnz tag29
ret
tag29:

jnz tag30
ret
tag30:

jnz tag31
ret
tag31:
jmp rcx

jmpExec32 endp

jmpExec16 proc
mov rdx, rcx
dec rdx
jnz tag0
ret
tag0:
dec rdx
jnz tag1
ret
tag1:
dec rdx
jnz tag2
ret
tag2:
dec rdx
jnz tag3
ret
tag3:
dec rdx
jnz tag4
ret
tag4:
dec rdx
jnz tag5
ret
tag5:
dec rdx
jnz tag6
ret
tag6:
dec rdx
jnz tag7
ret
tag7:
dec rdx
jnz tag8
ret
tag8:
dec rdx
jnz tag9
ret
tag9:
dec rdx
jnz tag10
ret
tag10:
dec rdx
jnz tag11
ret
tag11:
dec rdx
jnz tag12
ret
tag12:
dec rdx
jnz tag13
ret
tag13:
dec rdx
jnz tag14
ret
tag14:
dec rdx
jnz tag15
ret
tag15:
jmp rcx
ud2
jmpExec16 endp

align(64)
branchTo proc
jmp rcx
branchTo endp

align(64)
nullfunc proc
ret
nullfunc endp

MNOP16 MACRO
        ;db 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h, 90h
		db 20h, 0c9h, 20h, 0c8h, 20h, 0c6h, 20h, 0c0h, 20h, 0c4h, 20h, 0c9h, 20h, 0c2h, 20h, 0cah 	
ENDM

MNOP64 MACRO
	MNOP16
	MNOP16
	MNOP16
	MNOP16
ENDM

align(64)
nopExec proc

MNOP64
MNOP64
MNOP64
MNOP64

jmp rcx
nopExec endp

aligned ends

END