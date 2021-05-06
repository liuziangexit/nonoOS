#ifndef __DRIVER_PICIRQ_H__
#define __DRIVER_PICIRQ_H__

void pic_init();
void pic_enable(unsigned int irq);

#define IRQ_OFFSET      32

#endif
