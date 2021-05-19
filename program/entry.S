.text
.globl user_entry
user_entry:
    # 切到ring3
    int $121
    # 调用户main
    pushl %eax
    pushl %ebx
    call *%edx
    # 系统调用退出，返回值在eax里
    movl %eax, %edx
    # 设置eax为SYSCALL_EXIT
    movl $0, %eax
    movl $0, %ecx
    movl $0, %ebx
    movl $0, %edi
    movl $0, %esi
    int $122
    hlt
