.text
.globl user_entry
user_entry:
    #切到ring3
    int $121
    #调用户main
    pushl %eax
    pushl %ebx
    call *%edx
    #系统调用退出
    movl $9710, %eax
    movl $1, %edx
    movl $2, %ecx
    movl $3, %ebx
    movl $4, %edi
    movl $5, %esi
    int $122
    hlt
