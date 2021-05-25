.text
.globl user_task_entry
user_task_entry:
    #参数
    popl %eax
    popl %ebx
    #用户栈
    popl %ecx
    #用户代码入口
    popl %edx
    #转到用户栈
    movl %ecx, %esp
    #开中断
    sti
    #调用户entry
    call 0x8000000
    #用户entry会通过系统调用task_exit来退出
