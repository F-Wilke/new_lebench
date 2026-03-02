
#ifndef EXTENSION_H
#define EXTENSION_H



int current_pid(void);
int kernel_add(int a, int b);

gate_desc * idt_adaptor_clone(void);
void        idt_adaptor_activate(const gate_desc *idt);
void        idt_adaptor_release(const gate_desc *idt);

#endif // EXTENSION_H