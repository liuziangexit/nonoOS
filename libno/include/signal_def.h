#ifndef __LIBNO_SIGNAL_DEF_H__
#define __LIBNO_SIGNAL_DEF_H__

// 不知道是什么原因，kernel里的task.h不能#include libno/signal.h
// 否则就会编译错误。我不想调查这种无聊的问题
// 因此，把一些task.h需要访问的signal模块的东西放在这里

#define SIGMIN 1
#define SIGMAX 32

// 这个函数检查是否有signal被fire了但还没有处理
// 如果有，就处理那些signal
// 在task被换入的时候，调用这个函数
// 这个函数被实现在kernel里的signal.c
// task模块不需要关心它的实现，只需要在约定的地方去调用他就好啦
void signal_handle_on_task_schd_in();

// 预定义的默认信号处理器
void default_signal_handler(int sig);

#endif