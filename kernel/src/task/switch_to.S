# FIXME 这里没有保存eax寄存器，考虑这有没有问题？看看别的OS/协程在这里是怎么做的
# UPDATE 是的，至少xv6的实现也是这样的
# call进来以后的栈是这样的: 低地址  eip/save/from/to  高地址
# esp指向eip
# eip是被call指令压进去的
.text
.globl switch_to
switch_to:                      # switch_to(save, from, to)
    # 看看需不需要保存寄存器
    movl 4(%esp), %eax          # load "save"
    cmpl $0x0, %eax             # if(save != 0) goto save;
    jne save

    # 不需要保存寄存器
    popl %eax                   #扔掉eip    
    jmp restore

# save from's registers
save:
    movl 8(%esp), %eax          # eax points to "from"
    popl 0(%eax)                # save eip
    movl %esp, 4(%eax)
    movl %ebx, 8(%eax)
    movl %ecx, 12(%eax)
    movl %edx, 16(%eax)
    movl %esi, 20(%eax)
    movl %edi, 24(%eax)
    movl %ebp, 28(%eax)

restore:
    # 栈: 低地址  save/from/to  高地址
    # 此时esp指向save
    # restore to's registers
    movl 8(%esp), %eax          # eax now points to "to"
    movl 28(%eax), %ebp
    movl 24(%eax), %edi
    movl 20(%eax), %esi
    movl 16(%eax), %edx
    movl 12(%eax), %ecx
    movl 8(%eax), %ebx
    movl 4(%eax), %esp
    pushl 0(%eax)               # push eip
    ret
