include ksamd64.inc

custom_code SEGMENT read execute align(64)

delayLoop proc
    or rcx, rcx
    jz end_label
    loop_label : dec rcx;
    jnz loop_label;

    end_label: ret
delayLoop endp

recurse proc
    dec	rcx
    jz endrec
    call recurse

    endrec: ret
recurse endp

custom_code ends

END