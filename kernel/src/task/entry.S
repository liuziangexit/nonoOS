.text
.globl kernel_task_entry
kernel_task_entry:
    popl %eax
    call *%eax
    call task_exit
