#define _GNU_SOURCE
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/un.h>
#include <x86intrin.h>
#include <inttypes.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#ifdef SYM_ELEVATE

#include "LINF/sym_all.h"
#include "greeter.kh"

#endif

#define MAX_SIZE 8192
#define PF_MAX_SIZE 100 * 4096
#define LOOP 100
#define STEP 256
#define PF_STEP 4096
#define CENT ((MAX_SIZE / STEP) / 100)
#define PF_CENT ((PF_MAX_SIZE / PF_STEP) / 100)

#define ADDR_HINT 0x300000000000
#define CPU1 1

FILE *fp;

struct Record
{
	size_t size;
	struct timespec start;
	struct timespec end;
};
char* dummy_data;


static inline void GET_GSBASE(unsigned long *gsbase)
{
	asm volatile("rdgsbase %0" : "=r"(*gsbase));
}



//---------------------------------------------------------------------
#ifdef USE_VMALLOC
extern void *vmalloc(unsigned long size);
extern void vfree(const void *addr);
#endif
//---------------------------------------------------------------------
#ifdef BYPASS
extern ssize_t bp_write(int fd, const void *buf, size_t count);
extern ssize_t bp_read(int fd, void *buf, size_t count);
extern void *bp_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int bp_munmap(void *addr, size_t length);
extern int bp_select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict exceptfds, struct timeval *restrict timeout);
extern int bp_poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern int bp_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
extern pid_t bp_getppid(void);
extern ssize_t bp_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
extern ssize_t bp_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);
#endif

#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif
//---------------------------------------------------------------------



#ifdef SYM_ELEVATE
#include "kernel.h"

static inline void my_lower() {	  
	symbi_lower((void*)dummy_data, (void*)dummy_data);
	sym_iret();
}
#endif

void calc_diff(struct timespec *diff, struct timespec *bigger, struct timespec *smaller)
{
	if (smaller->tv_nsec > bigger->tv_nsec)
	{
		diff->tv_nsec = 1000000000 + bigger->tv_nsec - smaller->tv_nsec;
		diff->tv_sec = bigger->tv_sec - 1 - smaller->tv_sec;
	}
	else
	{
		diff->tv_nsec = bigger->tv_nsec - smaller->tv_nsec;
		diff->tv_sec = bigger->tv_sec - smaller->tv_sec;
	}
}

void calc_sum(struct timespec *sum, struct timespec *bigger, struct timespec *smaller)
{
	sum->tv_sec = 0;
	sum->tv_nsec = 0;
	if (smaller->tv_nsec >= (1000000000 - bigger->tv_nsec))
	{
		bigger->tv_sec = bigger->tv_sec + 1;
		sum->tv_nsec = smaller->tv_nsec - (1000000000 - bigger->tv_nsec);
		sum->tv_sec = bigger->tv_sec + smaller->tv_sec;
	}
	else
	{

		sum->tv_nsec = bigger->tv_nsec + smaller->tv_nsec;
		sum->tv_sec = bigger->tv_sec + smaller->tv_sec;
	}
}

void add_diff_to_sum(struct timespec *result, struct timespec *a, struct timespec *b)
{
	struct timespec diff;
	struct timespec tmp;
	calc_diff(&diff, a, b);
	tmp.tv_sec = result->tv_sec;
	tmp.tv_nsec = result->tv_nsec;
	calc_sum(result, &tmp, &diff);
}

void calc_average(struct timespec *average, struct timespec *sum, int size)
{
	average->tv_sec = 0;
	average->tv_nsec = 0;
	if (size == 0)
		return;

	average->tv_nsec = sum->tv_nsec / size + sum->tv_sec % size * 1000000000 / size;
	average->tv_sec = sum->tv_sec / size;
}

//---------------------------------------------------------------------
void getppid_bench(void)
{
	struct timespec diff = {0, 0};
	int l;
	int loop = 100000;
	struct Record *runs;


	runs = mmap(NULL, sizeof(struct Record) * loop, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	memset(runs, 0, sizeof(struct Record) * loop);

#ifdef SYM_ELEVATE
  sym_elevate();
#endif
	for (l = 0; l < loop; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
#ifdef SYM_SHORTCUT
    __x64_sys_getppid();
#else
#ifdef BYPASS
		bp_getppid();
#else
		syscall(SYS_getppid);
#endif
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif

	for (l = 0; l < loop; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%ld.%09ld\n", l, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	munmap(runs, sizeof(struct Record) * loop);
	return;
}

void clock_bench(void)
{
	struct timespec diff = {0, 0};
	int l;
	int loop = 100000;
	struct Record *runs;

	runs = mmap(NULL, sizeof(struct Record) * loop, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	memset(runs, 0, sizeof(struct Record) * loop);
#ifdef SYM_ELEVATE
      sym_elevate();
#endif
	for (l = 0; l < loop; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif

	for (l = 0; l < loop; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%ld.%09ld\n", l, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	munmap(runs, sizeof(struct Record) * loop);

	return;
}

void cpu_bench(void)
{
	struct timespec diff = {0, 0};
	int i, l;
	double start;
	double div;
	int loop = 1000;
	struct Record *runs;

	runs = mmap(NULL, sizeof(struct Record) * loop, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	memset(runs, 0, sizeof(struct Record) * loop);
	for (l = 0; l < loop; l++)
	{
		start = 9903290.789798798;
		div = 3232.32;

#ifdef SYM_ELEVATE
    sym_elevate();
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		for (i = 0; i < 500000; i++)
		{
			start = start / div;
		}
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
#ifdef SYM_ELEVATE
    my_lower();
#endif
	}

	for (l = 0; l < loop; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%ld.%09ld\n", l, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	munmap(runs, sizeof(struct Record) * loop);
	return;
}

void write_bench(int file_size)
{
	struct timespec diff = {0, 0};
	char *buf;
	int fd, i, l;
	struct Record *runs;

#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * file_size);
	runs = (struct Record*)vmalloc(sizeof(struct Record) * LOOP * 10);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * file_size);
	runs = (struct Record*)malloc(sizeof(struct Record) * LOOP * 10);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * file_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	runs = (struct Record*)mmap(NULL, sizeof(struct Record) * LOOP * 10, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	for (i = 0; i < file_size; i++)
	{
		buf[i] = i % 93 + 33;
	}
	fd = open("test_file.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		printf("invalid fd in write: %d\n", fd);
		exit(0);
	}

	memset(runs, 0, sizeof(struct Record) * LOOP * 10);
#ifdef SYM_ELEVATE
  sym_elevate();
#endif
	for (l = 0; l < LOOP * 10; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
#ifdef SYM_SHORTCUT
    ksys_write(fd, buf, file_size);
#else
#ifdef BYPASS
		bp_write(fd, buf, file_size);
#else
		syscall(SYS_write, fd, buf, file_size);
#endif
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
	}

#ifdef SYM_ELEVATE
  my_lower();
#endif
	close(fd);

	for (l = 0; l < LOOP * 10; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(buf);
	vfree(runs);
#elif defined(USE_MALLOC)
	free(buf);
	free(runs);
#else
	syscall(SYS_munmap, buf, file_size);
	munmap(runs, sizeof(struct Record) * LOOP * 10);
#endif

	return;
}

void read_bench(int file_size)
{
	struct timespec diff = {0, 0};
	char *buf;
	int fd, l, i;
	struct Record *runs;

#if DEBUG
	bool printed_cr3 = false;
#endif


#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * file_size);
	runs = (struct Record*)vmalloc(sizeof(struct Record) * LOOP * 10);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * file_size);
	runs = (struct Record*)malloc(sizeof(struct Record) * LOOP * 10);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * file_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	runs = (struct Record*)mmap(NULL, sizeof(struct Record) * LOOP * 10, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	for (i = 0; i < file_size; i++)
	{
		buf[i] = i % 93;
	}
	fd = open("test_file.txt", O_RDONLY);
	if (fd < 0)
	{
		perror("invalid fd in read\n");
		exit(0);
	}

	memset(runs, 0, sizeof(struct Record) * LOOP * 10);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (l = 0; l < LOOP * 10; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
#ifdef SYM_SHORTCUT
		ksys_read(fd, buf, file_size);
#else
#ifdef BYPASS
		bp_read(fd, buf, file_size);
#else
		syscall(SYS_read, fd, buf, file_size);
#endif
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif

	close(fd);

	for (l = 0; l < LOOP * 10; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(buf);
	vfree(runs);
#elif defined(USE_MALLOC)
	free(buf);
	free(runs);
#else
	syscall(SYS_munmap, buf, file_size);
	munmap(runs, sizeof(struct Record) * LOOP * 10);
#endif

	return;
}

#define sock "./my_sock"
void send_bench(int msg_size)
{
	struct timespec diff = {0, 0};

	// need to set affinity of parent and child to different cpus
	int retval, forkId, status, l;
	int fds1[2], fds2[2];
	char w = 'b', r;
	char *buf;
	struct sockaddr_un server_addr;
	struct Record *runs;
	#if DEBUG
	bool printed_cr3 = false;
	#endif

	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;

	strncpy(server_addr.sun_path, sock, sizeof(server_addr.sun_path) - 1);

	// create a buffer (this needs to be mmap)
#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * msg_size);
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * msg_size);
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * msg_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	// write character 'a' in the buffer
	for (int i = 0; i < msg_size; i++)
	{
		buf[i] = 'a';
	}

	memset(runs, 0, sizeof(struct Record) * LOOP);

	for (l = 0; l < LOOP; l++)
	{
		retval = pipe(fds1);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		retval = pipe(fds2);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");

		forkId = fork();

		if (forkId < 0)
		{
			printf("[error] fork failed.\n");
			return;
		}
		if (forkId == 0)
		{
			if (DEBUG) 
				printf("In child process.\n");

			close(fds1[0]); // close the read end of pipe 1
			close(fds2[1]); // close the write end of pipe 2

			int fd_server = socket(AF_UNIX, SOCK_STREAM, 0); // create a socket for server
			if (fd_server < 0)
				printf("[error] failed to open server socket.\n");

			// bind the socket to a filename so communication can occur
			retval = bind(fd_server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to bind.\n");

			// Listen on the socket
			retval = listen(fd_server, 10);
			if (retval == -1)
				printf("[error] failed to listen.\n");
			if (DEBUG)
				printf("Waiting for connection\n");

			// Write to pipe 1 to let parent know we are listening
			write(fds1[1], &w, 1);

			// wait for connection
			int fd_connect = accept(fd_server, (struct sockaddr *)0,
									(socklen_t *)0);
			if (DEBUG)
				printf("Connection accepted.\n");

			// Read update from parent on pipe 2
			read(fds2[0], &r, 1);

			if (DEBUG)
				printf("Received data from client, shutting down.\n");
			// remove sockets and close file descriptors and pipes
			remove(sock);
			close(fd_server);
			close(fd_connect);
			close(fds1[1]);
			close(fds2[0]);

			// job done, time to exit
			exit(0);
		}
		else
		{
			if (DEBUG)
				printf("In parent process, child PID: %d\n", forkId);
				
			close(fds1[1]); // close the write end of pipe 1
			close(fds2[0]); // close the read end of pipe 2

			if (DEBUG)
				printf("Waiting for child to be ready.\n");
			// Wait for update from child
			read(fds1[0], &r, 1);

			// create socket
			int fd_client = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_client < 0)
				printf("[error] failed to open client socket.\n");

			// connect to child address
			retval = connect(fd_client, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to connect.\n");

			// send the buffer over to child (for warm-up)
			retval = send(fd_client, buf, msg_size, MSG_DONTWAIT);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif
			// send buffer over to child and measure latency
			clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
#ifdef SYM_SHORTCUT
        retval = __sys_sendto(fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);
#else
#ifdef BYPASS
			retval = bp_sendto(fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);
#else
			retval = syscall(SYS_sendto, fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);
#endif
#endif
			clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
#ifdef SYM_ELEVATE
      my_lower();
#endif

			if (retval == -1)
			{
				printf("[error %d] failed to send. %s\n", errno, strerror(errno));
			}

			// update child so clean-up can happen
			write(fds2[1], &w, 1);

			// do cleanup your-self
			close(fd_client);
			close(fds1[0]);
			close(fds2[1]);

			wait(&status);
		}
	}

	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, msg_size, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(buf);
	vfree(runs);
#elif defined(USE_MALLOC)
	free(buf);
	free(runs);
#else
	syscall(SYS_munmap, buf, msg_size);
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

void recv_bench(int msg_size)
{
	struct timespec diff = {0, 0};

	// need to set affinity of parent and child to different cpus
	int retval, forkId, status, l;
	int fds1[2], fds2[2];
	char w = 'b', r;
	char *buf;
	struct sockaddr_un server_addr;
	struct Record *runs;

	if (DEBUG)
		printf("recv_bench(%d)\n", msg_size);

	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;

	strncpy(server_addr.sun_path, sock, sizeof(server_addr.sun_path) - 1);

	// create a buffer (this needs to be mmap)
#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * msg_size);
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * msg_size);
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * msg_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	memset(runs, 0, sizeof(struct Record) * LOOP);

	for (l = 0; l < LOOP; l++)
	{
		retval = pipe(fds1);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		retval = pipe(fds2);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		forkId = fork();

		if (forkId < 0)
		{
			printf("[error] fork failed.\n");
			return;
		}
		if (forkId > 0)
		{					// parent
			close(fds1[0]); // close the read end of pipe 1
			close(fds2[1]); // close the write end of pipe 2

			// create a socket for server
			int fd_server = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_server < 0)
				printf("[error] failed to open server socket.\n");

			// bind the socket to a filename so communication can occur
			retval = bind(fd_server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to bind.\n");

			// Listen on the socket
			retval = listen(fd_server, 11);
			if (retval == -1)
				printf("[error] failed to listen.\n");
			if (DEBUG)
				printf("Waiting for connection\n");

			// Write to pipe 1 to let parent know we are listening
			write(fds1[1], &w, 1);

			
			// wait for connection
			int fd_connect = accept(fd_server, (struct sockaddr *)0,
									(socklen_t *)0);
			if (DEBUG)
				printf("Connection accepted.\n");

			// Read update from parent on pipe 2
			read(fds2[0], &r, 1);

			// recv data from child (for warm-up)
			retval = recv(fd_connect, buf, msg_size, MSG_DONTWAIT);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif
			// recv data from child and measure latency
			clock_gettime(CLOCK_MONOTONIC, &runs[l].start);

#ifdef SYM_SHORTCUT
			retval = __sys_recvfrom(fd_connect, buf, msg_size, MSG_DONTWAIT, NULL, NULL);
#else
#ifdef BYPASS
			retval = bp_recvfrom(fd_connect, buf, msg_size, MSG_DONTWAIT, NULL, NULL);
#else
			retval = syscall(SYS_recvfrom, fd_connect, buf, msg_size, MSG_DONTWAIT, NULL, NULL);
			// retval = 0;
#endif
#endif
			clock_gettime(CLOCK_MONOTONIC, &runs[l].end);

#ifdef SYM_ELEVATE
      my_lower();
#endif


			if (retval == -1)
			{
				printf("[error %d] failed to send. %s\n", errno, strerror(errno));
			}

			// update child so clean-up can happen
			write(fds1[1], &w, 1);

			// remove sockets and close file descriptors and pipes
			remove(sock);
			close(fd_server);
			close(fd_connect);
			close(fds1[1]);
			close(fds2[0]);

			wait(&status);
		}
		else
		{
			close(fds1[1]); // close the write end of pipe 1
			close(fds2[0]); // close the read end of pipe 2

			// Wait for update from parent
			read(fds1[0], &r, 1);

			// create socket
			int fd_client = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_client < 0)
				printf("[error] failed to open client socket.\n");

			// connect to parent address
			retval = connect(fd_client, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to connect.\n");

			// write character 'a' in the buffer
			for (int i = 0; i < msg_size; i++)
			{
				buf[i] = i % 93;
			}

			// send data over to parent + one more copy for warm up
			for (l = 0; l < 2; l++)
			{
				retval = syscall(SYS_sendto, fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);

				if (retval == -1)
				{
					printf("[error %d] failed to send. %s\n", errno, strerror(errno));
				}
			}

			// tell parent it should start reading
			write(fds2[1], &w, 1);

			// wait for parent
			read(fds1[0], &r, 1);

			// do cleanup your-self
			close(fd_client);
			close(fds1[0]);
			close(fds2[1]);

			// job done, time to exit
			exit(0);
		}
	}

	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, msg_size, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(buf);
	vfree(runs);
#elif defined(USE_MALLOC)
	free(buf);
	free(runs);
#else
	syscall(SYS_munmap, buf, msg_size);
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

struct timespec *forkTime;

void fork_bench(void)
{
	struct timespec diff = {0, 0}, ave= {0, 0};
	forkTime = mmap(NULL, sizeof(struct timespec) * LOOP, PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int forkId, l, status;
	struct Record *runs;

#if defined(USE_VMALLOC)
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	memset(runs, 0, sizeof(struct Record) * LOOP);
	memset(forkTime, 0, sizeof(struct timespec) * LOOP);
	for (l = 0; l < LOOP; l++)
	{
#ifdef SYM_ELEVATE
    sym_elevate();
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		forkId = fork();
		if (forkId == 0)
		{
			clock_gettime(CLOCK_MONOTONIC, &forkTime[l]);
#ifdef SYM_ELEVATE
      my_lower();
#endif
			exit(0);
		}
		else if (forkId > 0)
		{
			clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
#ifdef SYM_ELEVATE
      my_lower();
#endif
			wait(&status);
		}
		else
		{
			printf("[error] fork failed.\n");
			fflush(stdout);
		}
	}

	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		calc_diff(&ave, forkTime, &runs[l].start);
		fprintf(fp, "%d,%ld.%09ld,%ld.%09ld\n", l, diff.tv_sec, diff.tv_nsec, ave.tv_sec, ave.tv_nsec);
	}
	fflush(fp);

	munmap(forkTime, sizeof(struct timespec));

#if defined(USE_VMALLOC)
	vfree(runs);
#elif defined(USE_MALLOC)
	free(runs);
#else
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

void *thrdfnc(void *args)
{
	clock_gettime(CLOCK_MONOTONIC, (struct timespec*)args);
	pthread_exit(0);
}

void thread_bench(void)
{
	struct timespec diff = {0, 0}, ave= {0, 0};
	int l;
	pthread_t newThrd;
	struct timespec *threads;
	struct Record *runs;

#if defined(USE_VMALLOC)
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
	threads = (struct timespec *)vmalloc(sizeof(struct timespec) * LOOP);
#elif defined(USE_MALLOC)
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
	threads = (struct timespec *)malloc(sizeof(struct timespec) * LOOP);
#else
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	threads = (struct timespec *)mmap(NULL, sizeof(struct timespec) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	memset(runs, 0, sizeof(struct Record) * LOOP);
	memset(threads, 0, sizeof(struct timespec) * LOOP);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (l = 0; l < LOOP; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		pthread_create(&newThrd, NULL, thrdfnc, &threads[l]);
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);

		pthread_join(newThrd, NULL);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif

	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		calc_diff(&ave, &threads[l], &runs[l].start);
		fprintf(fp, "%d,%ld.%09ld,%ld.%09ld\n", l, diff.tv_sec, diff.tv_nsec, ave.tv_sec, ave.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(runs);
	vfree(threads);
#elif defined(USE_MALLOC)
	free(runs);
	free(threads);
#else
	munmap(runs, sizeof(struct Record) * LOOP);
	munmap(threads, sizeof(struct timespec) * LOOP);
#endif

	return;
}

void pagefault_bench(int file_size)
{
	struct timespec diff = {0, 0};
	struct Record *runs;
	int l, i;
	char *addr;

#if defined(USE_VMALLOC)
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	memset(runs, 0, sizeof(struct Record) * LOOP);

 #ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (l = 0; l < LOOP; l++)
	{
		addr = (char *)mmap((void *)ADDR_HINT, file_size, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		i = 0;
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		while (i < file_size)
		{
			addr[i] = i % 93 + 33;
			i = i + 4096;
		}
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);

		munmap(addr, file_size);

	}
#ifdef SYM_ELEVATE
      my_lower();
#endif


	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diff.tv_sec, diff.tv_nsec);

	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(runs);
#elif defined(USE_MALLOC)
	free(runs);
#else
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

void stack_pagefault_bench(int file_size)
{
	int l, i;
	char *addr;
	struct Record *runs;

#if defined(USE_VMALLOC)
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	memset(runs, 0, sizeof(struct Record) * LOOP);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (l = 0; l < LOOP; l++)
	{
		addr = (char *)alloca(file_size * sizeof(long));
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		i = 0;
		while (i < file_size)
		{
			addr[i] = i % 93 + 33;
			i = i + 4096;
		}
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif

	for (l = 0; l < LOOP; l++)
	{
		struct timespec diff;
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(runs);
#elif defined(USE_MALLOC)
	free(runs);
#else
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

static void fault_around_bench(int file_size)
{
	struct timespec diff = {0, 0};
	struct Record *runs;
	int l, i;
	char *addr;

#if defined(USE_VMALLOC)
	runs = (struct Record *)vmalloc(sizeof(struct Record) * LOOP);
#elif defined(USE_MALLOC)
	runs = (struct Record *)malloc(sizeof(struct Record) * LOOP);
#else
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	int fd = open("test_file.txt", O_RDONLY);

	memset(runs, 0, sizeof(struct Record) * LOOP);
	for (l = 0; l < LOOP; l++)
	{
		addr = (char *)mmap((void *)ADDR_HINT, file_size, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		i = 0;
		clock_gettime(CLOCK_MONOTONIC, &runs[l].start);
		char a = *addr;
		clock_gettime(CLOCK_MONOTONIC, &runs[l].end);

		munmap(addr, file_size);

	}

	close(fd);

	for (l = 0; l < LOOP; l++)
	{
		calc_diff(&diff, &runs[l].end, &runs[l].start);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diff.tv_sec, diff.tv_nsec);

	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(runs);
#elif defined(USE_MALLOC)
	free(runs);
#else
	munmap(runs, sizeof(struct Record) * LOOP);
#endif

	return;
}

static void select_bench(size_t fd_count, int iters)
{
	struct Record *runs;
	int *fds;
	fd_set rfds;
	struct timeval timeout;
	int maxFd = 0;

	FD_ZERO(&rfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

#if defined(USE_VMALLOC)
	fds = (int *)vmalloc(sizeof(int) * fd_count);
	runs = (struct Record *)vmalloc(sizeof(struct Record) * iters);
#elif defined(USE_MALLOC)
	fds = (int *)malloc(sizeof(int) * fd_count);
	runs = (struct Record *)malloc(sizeof(struct Record) * iters);
#else
	fds = (int *)mmap(NULL, sizeof(int) * fd_count, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	runs = (struct Record *)mmap(NULL, sizeof(struct Record) * iters, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	for (size_t i = 0; i < fd_count; i++)
	{
		fds[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (fds[i] < 0)
		{
			i--;
			continue;
		}
		if (fds[i] > maxFd)
			maxFd = fds[i];
		FD_SET(fds[i], &rfds);
	}

	memset(runs, 0, sizeof(struct Record) * iters);
#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (int i = 0; i < iters; i++)
	{
		clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
#ifdef SYM_SHORTCUT
		__x64_sys_select(maxFd + 1, &rfds, NULL, NULL, &timeout);
#else
		syscall(SYS_select, maxFd + 1, &rfds, NULL, NULL, &timeout);
#endif
		clock_gettime(CLOCK_MONOTONIC, &runs[i].end);
	}
#ifdef SYM_ELEVATE
  my_lower();
#endif


	for (size_t i = 0; i < fd_count; i++)
	{
		close(fds[i]);
	}

	for (int i = 0; i < iters; i++)
	{
		struct timespec diff;
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld,%ld.%09ld\n", i, fd_count, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

#if defined(USE_VMALLOC)
	vfree(fds);
	vfree(runs);
#elif defined(USE_MALLOC)
	free(fds);
	free(runs);
#else
	munmap(fds, sizeof(int) * fd_count);
	munmap(runs, sizeof(struct Record) * iters);
#endif
}

static void poll_bench(size_t fd_count, int iters)
{
	int retval;

	struct Record *runs;

	int fds[fd_count];
	struct pollfd pfds[fd_count];

	runs = mmap(NULL, sizeof(struct Record) * iters, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(runs, 0, sizeof(struct Record) * iters);

#ifdef SYM_ELEVATE
	sym_elevate();
#endif

	for (int i = 0; i < iters; i++)
	{
		memset(pfds, 0, sizeof(pfds));

		for (int i = 0; i < fd_count; i++)
		{
			char name[10];
			name[0] = 'f';
			name[1] = 'i';
			name[2] = 'l';
			name[3] = 'e';
			int j = i;
			int index = 4;
			while (j > 0)
			{
				name[index] = 48 + j % 10;
				j = j / 10;
				index++;
			}
			name[index] = '\0';
			int fd = socket(AF_INET, SOCK_STREAM, 0);
			if (fd < 0)
				printf("invalid fd in poll: %d\n", fd);

			pfds[i].fd = fd;
			pfds[i].events = POLLIN;

			fds[i] = fd;
		}

		clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
		retval = syscall(SYS_poll, pfds, fd_count, 0);
		clock_gettime(CLOCK_MONOTONIC, &runs[i].end);

		if (retval != fd_count)
		{
			printf("[error] poll return unexpected: %d\n", retval);
		}

		for (int i = 0; i < fd_count; i++)
		{
			retval = close(fds[i]);
			if (retval == -1)
				printf("[error] close failed in poll test %d.\n", fds[i]);
		}
	}

#ifdef SYM_ELEVATE
	my_lower();
#endif

	for (int i = 0; i < iters; i++)
	{
		struct timespec diff;
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld,%ld.%09ld\n", i, fd_count, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	return;
}

static void epoll_bench(size_t fd_count, int iters)
{
	int retval;

	int fds[fd_count];
	struct Record *runs;


	runs = mmap(NULL, sizeof(struct Record) * iters, PROT_READ | PROT_WRITE,
								MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(runs, 0, sizeof(struct Record) * iters);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (int i = 0; i < iters; i++)
	{

		int epfd = epoll_create(fd_count);

		for (int i = 0; i < fd_count; i++)
		{
			char name[10];
			name[0] = 'f';
			name[1] = 'i';
			name[2] = 'l';
			name[3] = 'e';
			int j = i;
			int index = 4;
			while (j > 0)
			{
				name[index] = 48 + j % 10;
				j = j / 10;
				index++;
			}
			name[index] = '\0';
			int fd = socket(AF_INET, SOCK_STREAM, 0);
			if (fd < 0)
				printf("[error] invalid fd in epoll: %d\n", fd);

			struct epoll_event event;
			event.events = EPOLLIN;
			event.data.fd = fd;

			retval = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
			if (retval == -1)
			{
				printf("[error] epoll_ctl failed.\n");
			}

			fds[i] = fd;
		}

		struct epoll_event *events = (struct epoll_event *)malloc(fd_count * sizeof(struct epoll_event));
		clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
		retval = epoll_wait(epfd, events, fd_count, 0);
		clock_gettime(CLOCK_MONOTONIC, &runs[i].end);

		free(events);
		if (retval != fd_count)
		{
			printf("[error] epoll return unexpected: %d\n", retval);
		}

		retval = close(epfd);
		if (retval == -1)
			printf("[error] close epfd failed in epoll test %d.\n", epfd);
		for (int i = 0; i < fd_count; i++)
		{
			retval = close(fds[i]);
			if (retval == -1)
				printf("[error] close failed in epoll test %d.\n", fds[i]);
		}
	}

#ifdef SYM_ELEVATE
      my_lower();
#endif

	for (int i = 0; i < iters; i++)
	{
		struct timespec diff;
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld,%ld.%09ld\n", i, fd_count, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	return;
}

static void context_switch_bench(void)
{
	int iter = 1000;
	int fds1[2], fds2[2], retval;
	retval = pipe(fds1);
	if (retval != 0)
		printf("[error] failed to open pipe1.\n");
	retval = pipe(fds2);
	if (retval != 0)
		printf("[error] failed to open pipe2.\n");

	char w = 'a', r;
	cpu_set_t cpuset;
	int prio;
	struct Record *runs;

	retval = sched_getaffinity(getpid(), sizeof(cpuset), &cpuset);
	if (retval == -1)
		printf("[error] failed to get affinity.\n");
	prio = getpriority(PRIO_PROCESS, 0);
	if (prio == -1)
		printf("[error] failed to get priority.\n");

	int forkId = fork();
	if (forkId > 0) { // is parent
		retval = close(fds1[0]);
		if (retval != 0)
			printf("[error] failed to close fd1.\n");
		retval = close(fds2[1]);
		if (retval != 0)
			printf("[error] failed to close fd2.\n");

		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(0, &set);
		retval = sched_setaffinity(getpid(), sizeof(set), &set);
		if (retval == -1)
		printf("[error] failed to set processor affinity.\n");
		retval = setpriority(PRIO_PROCESS, 0, -20);
		if (retval == -1)
		printf("[error] failed to set process priority.\n");

		runs = mmap(NULL, sizeof(struct Record) * iter, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		memset(runs, 0, sizeof(struct Record) * iter);

		read(fds2[0], &r, 1);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

		for (int i = 0; i < iter; i++) {
			clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
			write(fds1[1], &w, 1);
			read(fds2[0], &r, 1);
			clock_gettime(CLOCK_MONOTONIC, &runs[i].end);
		}
#ifdef SYM_ELEVATE
    my_lower();
#endif

		int status;
		wait(&status);

		close(fds1[1]);
		close(fds2[0]);


	} else if (forkId == 0){

		retval = close(fds1[1]);
		if (retval != 0)
			printf("[error] failed to close fd1.\n");
		retval = close(fds2[0]);
		if (retval != 0)
			printf("[error] failed to close fd2.\n");

		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(0, &set);
		retval = sched_setaffinity(getpid(), sizeof(set), &set);
		if (retval == -1)
			printf("[error] failed to set processor affinity.\n");
		retval = setpriority(PRIO_PROCESS, 0, -20);
		if (retval == -1)
			printf("[error] failed to set process priority.\n");

		write(fds2[1], &w, 1);
		for (int i = 0; i < iter; i++) {
			read(fds1[0], &r, 1);
			write(fds2[1], &w, 1);
		}

        	kill(getpid(), SIGINT);
		printf("[error] unable to kill child process\n");
		return;
	} else {
		printf("[error] failed to fork.\n");
	}

	retval = sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
	if (retval == -1)
		printf("[error] failed to restore affinity.\n");
	retval = setpriority(PRIO_PROCESS, 0, prio);
	if (retval == -1)
		printf("[error] failed to restore priority.\n");

	struct timespec diff;
	for (int i = 0; i < iter; i++)
	{
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld.%09ld\n", i, diff.tv_sec, diff.tv_nsec);
	}
	fflush(fp);

	munmap(runs, sizeof(struct Record) * iter);
}
//---------------------------------------------------------------------

static void mmap_bench(size_t file_size)
{
	struct Record *runs;
	int i;

	runs = mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(runs, 0, sizeof(struct Record) * LOOP);

	int fd = open("test_file.txt", O_RDONLY);
	if (fd < 0)
		printf("invalid fd%d\n", fd);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (i = 0; i < LOOP; i++)
	{
		runs[i].size = file_size;
		clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
#ifdef SYM_SHORTCUT
		void *addr = (void *)ksys_mmap_pgoff(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
#else
		void *addr = (void *)syscall(SYS_mmap, NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
#endif
		clock_gettime(CLOCK_MONOTONIC,&runs[i].end);

		syscall(SYS_munmap, addr, file_size);
	}

#ifdef SYM_ELEVATE
      my_lower();
#endif

	close(fd);

	struct timespec diff;
	for (i = 0; i < LOOP; i++)
	{
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld,%ld.%09ld\n", i, runs[i].size, diff.tv_sec, diff.tv_nsec);
	}

	munmap(runs, sizeof(struct Record) * LOOP);
	return;
}

static void munmap_bench(size_t file_size)
{
	int i;
	struct Record *runs;
	int fd = open("test_file.txt", O_RDWR);

	if (fd < 0)
		printf("invalid fd%d\n", fd);

	runs = mmap(NULL, sizeof(struct Record) * LOOP, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(runs, 0, sizeof(struct Record) * LOOP);

#ifdef SYM_ELEVATE
      sym_elevate();
#endif

	for (i = 0; i < LOOP; i++)
	{
		void *addr = (void *)syscall(SYS_mmap, NULL, file_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
		for (int i = 0; i < file_size; i++) {
			((char *)addr)[i] = 'b';
		}
		clock_gettime(CLOCK_MONOTONIC, &runs[i].start);
#ifdef SYM_SHORTCUT
		__x64_sys_munmap(addr, file_size);
#else
		syscall(SYS_munmap, addr, file_size);
#endif
		clock_gettime(CLOCK_MONOTONIC,&runs[i].end);
	}

#ifdef SYM_ELEVATE
      my_lower();
#endif

	close(fd);

	struct timespec diff;
	for (i = 0; i < LOOP; i++)
	{
		calc_diff(&diff, &runs[i].end, &runs[i].start);
		fprintf(fp, "%d,%ld,%ld.%09ld\n", i, file_size, diff.tv_sec, diff.tv_nsec);
	}

	munmap(runs, sizeof(struct Record) * LOOP);
	return;
}

#ifdef BYPASS
extern void set_bypass_limit(int val);
extern void set_bypass_syscall(int val);
#endif


int main(void)
{
	int file_size, pf_size, retval;
	int i = 0, percentage = 0;

	#ifdef SYM_ELEVATE

	
	
	dummy_data = malloc(0x1000);
	dummy_data[0] = 0x18; //touch this for safety
	
	sym_elevate();

	printf("Running with SYM_ELEVATE enabled, pid %d\n", current_pid());

	//prepare data for my_lower
	symbi_query((void*)dummy_data);
	
	for (i=0; i<0x1000; i++)
	switch (dummy_data[i]) {
		case 0x18:
		dummy_data[i] = 0x2b;
		break;
        case 0x10:
		dummy_data[i] = 0x33;
		break;
		default:
		break;
	}
	
	my_lower();
	i = 0;
	
	#endif
	

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(CPU1, &set);
  /* printf("getpid() %d, sizeof(set) %d, &set %d", getpid(), sizeof(set), &set); */
	retval = sched_setaffinity(getpid(), sizeof(set), &set);
	if (retval == -1)
		printf("[error] failed to set processor affinity.\n");
	retval = setpriority(PRIO_PROCESS, 0, -20);
	if (retval == -1)
		printf("[error] failed to set process priority.\n");

	remove("test_file.txt");
	remove("tmp_file.txt");

// #ifdef SYM_SHORTCUT
//   // initializes kallsym lib
//   sym_lib_init();
//   init_sym_shortcuts();
// #endif

#ifdef BYPASS
	// set_bypass_limit(50);
	// set_bypass_syscall(1);
#endif

	//*************************************

#ifdef REF_TEST
	printf("Starting reference benchmarks\n");

	fp = fopen("./new_lebench_clock.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	clock_bench();
	fclose(fp);

	fp = fopen("./new_lebench_cpu.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	cpu_bench();
	fclose(fp);

	fp = fopen("./new_lebench_getppid.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	getppid_bench();
	fclose(fp);
	printf("Reference benchmarks done\n");
#endif

	//*************************************

#ifdef THREAD_TEST
	printf("Running Thread Create benchmarks\n");

	fp = fopen("./new_lebench_thread.csv", "w");

	fprintf(fp, "Sr,LatencyParent,LatencyChild\n");
	fflush(fp);

	thread_bench();

	printf("Running thread test 100 %% done\n");
	fflush(stdout);
	fclose(fp);
#endif

	//*************************************

#ifdef FORK_TEST
	fp = fopen("./new_lebench_fork.csv", "w");

	fprintf(fp, "Sr,LatencyParent,LatencyChild\n");
	fflush(fp);

	fork_bench();

	printf("Running fork test 100 %% done\n");
	fflush(stdout);
	fclose(fp);
#endif

	//*************************************

#ifdef SEND_TEST
	fp = fopen("./new_lebench_send.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	send_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		send_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running send test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef RECV_TEST
	fp = fopen("./new_lebench_recv.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	recv_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		recv_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running recv test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef WRITE_TEST
	fp = fopen("./new_lebench_write.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	write_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		write_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running write test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef READ_TEST
	fp = fopen("./new_lebench_read.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	read_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		read_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running read test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

#ifdef MMAP_TEST
	fp = fopen("./new_lebench_mmap.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 0;
	while(file_size < MAX_SIZE)
	{
		file_size += STEP;
		mmap_bench(file_size);
	}

	fclose(fp);
#endif

#ifdef MUNMAP_TEST
	fp = fopen("./new_lebench_munmap.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 0;
	while(file_size < MAX_SIZE)
	{
		file_size += STEP;
		munmap_bench(file_size);
	}

	fclose(fp);
#endif

	//*************************************

#ifdef PF_TEST
	fp = fopen("./new_lebench_pagefault.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	pf_size = 0;
	i = 0;
	while (pf_size < PF_MAX_SIZE)
	{
		pf_size = pf_size + PF_STEP;
		pagefault_bench(pf_size);
		i++;
		if (i > PF_CENT)
		{
			i = 0;
			percentage = (pf_size * 100) / (PF_MAX_SIZE);
			printf("Running pagefault test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

#ifdef FAULT_AROUND_TEST
	fp = fopen("./new_lebench_fault_around.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	pf_size = 0;
	i = 0;
	while (pf_size < PF_MAX_SIZE)
	{
		pf_size = pf_size + PF_STEP;
		fault_around_bench(pf_size);
		i++;
		if (i > PF_CENT)
		{
			i = 0;
			percentage = (pf_size * 100) / (PF_MAX_SIZE);
			printf("Running fault around test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef ST_PF_TEST
	fp = fopen("./new_lebench_stackpagefault.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	pf_size = 0;
	i = 0;
	while (pf_size < PF_MAX_SIZE)
	{
		pf_size = pf_size + PF_STEP;
		pagefault_bench(pf_size);
		i++;
		if (i > PF_CENT)
		{
			i = 0;
			percentage = (pf_size * 100) / (PF_MAX_SIZE);
			printf("Running stack pagefault test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

#ifdef SELECT_TEST
	fp = fopen("./new_lebench_select.csv", "w");
	fprintf(fp, "Index,Size,Latency\n");

	printf("Running select test small\n");
	fflush(stdout);
	select_bench(10, LOOP);

	printf("Running select test large\n");
	fflush(stdout);
	select_bench(1000, LOOP);

	fclose(fp);
#endif

#ifdef POLL_TEST
	fp = fopen("./new_lebench_poll.csv", "w");
	fprintf(fp, "Index,Size,Latency\n");

	printf("Running poll test small\n");
	fflush(stdout);
	poll_bench(10, LOOP);

	printf("Running poll test large\n");
	fflush(stdout);
	poll_bench(1000, LOOP);

	fclose(fp);
#endif

#ifdef EPOLL_TEST
	fp = fopen("./new_lebench_epoll.csv", "w");
	fprintf(fp, "Index,Size,Latency\n");

	printf("Running epoll test small\n");
	fflush(stdout);
	epoll_bench(10, LOOP);

	printf("Running epoll test large\n");
	fflush(stdout);
	epoll_bench(1000, LOOP);

	fclose(fp);
#endif

#ifdef CTX_SW_TEST
	fp = fopen("./new_lebench_context_switch.csv", "w");
	fprintf(fp, "Index,Latency\n");

	printf("Running context switch test\n");
	fflush(stdout);
	context_switch_bench();

	fclose(fp);
#endif
}

