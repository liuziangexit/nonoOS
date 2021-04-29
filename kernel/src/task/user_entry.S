.text
.globl user_task_entry
user_task_entry:
    #参数
    popl %eax
    #用户栈
    popl %ebx
    #用户代码入口
    popl %ecx
    #转到用户栈
    movl 0x0, %ebp
    movl %ebx, %esp
    #TODO 切CS/DS/ES之类的寄存器
    #调用户main
    pushl %eax
    call *%ecx
    #退出（要中断走系统调用吧?）
    call task_exit
