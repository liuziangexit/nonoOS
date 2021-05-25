.text
.globl kernel_task_entry
kernel_task_entry:
    popl %eax
    sti #开中断
    call *%eax
    pushl %eax
    call task_exit
