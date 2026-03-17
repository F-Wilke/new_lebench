#PATHS
DIR := ${CURDIR}/..

#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------

#MYBENCH_SMALL
TARGETS := sym_lebench sym_lebench_static sym_elevate sym_no_elevate sym_sc

SYM_SHORTCUT=-DSYM_SHORTCUT
SYM_TESTS=-DREF_TEST -DREAD_TEST -DWRITE_TEST -DSEND_TEST -DRECV_TEST 
# SYM_TESTS=-DREF_TEST -DREAD_TEST -DWRITE_TEST 
# SYM_TESTS= -DSEND_TEST 
# SYM_TESTS= -DRECV_TEST
# SYM_TESTS=-DREF_TEST -DTHREAD_TEST -DFORK_TEST -DSEND_TEST -DRECV_TEST -DREAD_TEST \
# 		-DWRITE_TEST -DPF_TEST -DST_PF_TEST -DSELECT_TEST -DCTX_SW_TEST -DPOLL_TEST \
# 		-DMMAP_TEST -DMUNMAP_TEST -DFAULT_AROUND_TEST -DEPOLL_TEST
SYM_CONFIG=-UUSE_VMALLOC -UBYPASS -DUSE_MALLOC -DSYM_ELEVATE
SYM_CONFIG_NO_ELEVATE=-UUSE_VMALLOC -UBYPASS -DUSE_MALLOC
SYM_DEBUG=-DDEBUG
SYM_SYS_LIBS=-pthread
SYMBI=../Symlib/build/libsym.a -I ../Symlib/include
LKS=-lc -L. -lkallsyms  #make link order explicit
SYM_STATIC = -DSYM_STATIC
LIB_K_EXT=-L. -lextension


SYM_CC_DEBUG=-g
CFLAGS=

.kersyms.txt: kernel.h
	python3 ../examples/tools/list_header_symbols.py kernel.h > .kersyms.txt

#will this write a \\0 at the end of the file? Or just EOF?
libkallsyms.a: .kersyms.txt
	KERSYMS_TXT_LEN=$$(wc -c < .kersyms.txt); \
	dd if=.kersyms.txt of=/proc/libkallsyms.a bs=$$KERSYMS_TXT_LEN count=1; \
	cp /proc/libkallsyms.a ./

libkallsyms.so:
	cp /proc/libkallsyms.so ./


libextension.a ext.kbin: $(wildcard *.kc *.kh)
	../examples/tools/kcc

clean:
	rm -rf $(wildcard $(TARGETS) *.o *.so *.a ext.kbin .ext)


# lazy
sym: sym_lebench

sym_lebench: new_lebench.c libkallsyms.so libextension.a
	gcc $< -o new_lebench $(CFLAGS) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(LKS) $(LIB_K_EXT) $(SYM_CC_DEBUG)


sym_no_elevate: new_lebench.c
	gcc $< -o $@ $(CFLAGS) $(SYM_SYS_LIBS) $(SYM_CONFIG_NO_ELEVATE) $(SYM_TESTS) $(SYM_DEBUG)

sym_elevate: sym_elevate.o libkallsyms.so libextension.a
	gcc $< -o $@ $(CFLAGS) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(LKS) $(LIB_K_EXT) $(SYM_CC_DEBUG)

sym_elevate.o: new_lebench.c 
	gcc -c $< -o $@ $(CFLAGS) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(SYM_CC_DEBUG)

sym_sc: new_lebench.c libkallsyms.so libextension.a
	gcc $< -o $@ $(CFLAGS) $(SYM_SHORTCUT) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(LKS) $(LIB_K_EXT) $(SYM_CC_DEBUG)

sym_lebench_static: new_lebench.c libkallsyms.a libextension.a
	gcc $^ -o sym_lebench_static $(CFLAGS) $(SYM_SHORTCUT) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(SYM_CC_DEBUG) $(SYM_STATIC) $(LIB_K_EXT)

sym_lebench_static.o: new_lebench.c 
	gcc -c $^ -o $@ $(CFLAGS) $(SYM_SHORTCUT) $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI) $(SYM_CC_DEBUG) $(SYM_STATIC) 



sym_clean:
	rm -rf sym_lebench new_lebench new_lebench.o sym_no_elevate sym_elevate sym_sc
	rm -rf *.csv
	rm -rf test_file.txt

sym_all:
	make sym_no_elevate
	make sym_elevate
	make sym_sc

sym_clean_all: sym_clean
	rm -rf elevate no_elevate sc
