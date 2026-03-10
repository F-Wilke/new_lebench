#ifndef KERNEL_H
#define KERNEL_H


#ifdef SYM_STATIC
void symbi_lower(void* regs, void* sreg);
void symbi_query(void* regs);
ssize_t ksys_write(int fd, const void *buf, size_t count);
ssize_t ksys_read(int fd, void *buf, size_t count);
void *ksys_mmap_pgoff(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int __x64_sys_munmap(void *addr, size_t length);
int __x64_sys_select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict exceptfds, struct timeval *restrict timeout);
pid_t __x64_sys_getppid(void);
ssize_t __sys_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t __sys_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);
void symbi_fast_lower(void);

#else //assume dynamic

extern void symbi_lower(void* regs, void* sreg);
extern void symbi_query(void* regs);
extern ssize_t ksys_write(int fd, const void *buf, size_t count);
extern ssize_t ksys_read(int fd, void *buf, size_t count);
extern void *ksys_mmap_pgoff(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int __x64_sys_munmap(void *addr, size_t length);
extern int __x64_sys_select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict exceptfds, struct timeval *restrict timeout);
extern pid_t __x64_sys_getppid(void);
extern ssize_t __sys_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
extern ssize_t __sys_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);
extern void symbi_fast_lower(void);

#endif

#endif