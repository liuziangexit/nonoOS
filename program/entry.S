.text
.globl user_entry
user_entry:
    # 切到ring3
    int $121
    # 调用户main
    pushl %eax
    pushl %ebx
    call *%edx
    # 设置exit的参数
    movl %eax, %ecx
    # 设置系统调用号为SYSCALL_TASK
    movl $1, %eax
    # 设置arg[0]为USER_TASK_ACTION_EXIT
    movl $6, %edx
    int $122
    hlt
