#ifndef __IDT_ADAPTOR_KH__
#define __IDT_ADAPTOR_KH__

gate_desc * idt_adaptor_clone(void);
void        idt_adaptor_activate(const gate_desc *idt);
void        idt_adaptor_release(const gate_desc *idt);

#endif
