#### Compiler and tool definitions shared by all build targets #####
CC = cc
CCC = c++
CXX = c++
BASICOPTS = -g -O2 -Wall -fno-strict-aliasing
CFLAGS = $(BASICOPTS)
CCFLAGS = $(BASICOPTS)
CXXFLAGS = $(BASICOPTS)
CCADMIN = 


.PHONY: all
all: ltm_getRAWfdv6 aplic ltmd 

.c.o:
	$(COMPILE.c) $(CFLAGS_ltm_getRAWfd) $(CPPFLAGS_ltm_getRAWfdv6) -o $@ -c $<

.cc.o:
	$(COMPILE.cc) $(CCFLAGS_aplic) $(CPPFLAGS_aplic) -o $@ -c $<

## Target: ltm_getRAWfd
OBJS_ltm_getRAWfdv6 = ltm_getRAWfdv6.o
USERLIBS_ltm_getRAWfdv6 = $(SYSLIBS_ltm_getRAWfdv6)
DEPLIBS_ltm_getRAWfdv6 =
LDLIBS_ltm_getRAWfdv6 = $(USERLIBS_ltm_getRAWfdv6)


# Link or archive
ltm_getRAWfdv6: $(OBJS_ltm_getRAWfdv6) $(DEPLIBS_ltm_getRAWfdv6)
	$(LINK.c) $(CFLAGS_ltm_getRAWfdv6) $(CPPFLAGS_ltm_getRAWfdv6) -o $@ $(OBJS_ltm_getRAWfdv6) $(LDLIBS_ltm_getRAWfdv6)

## Target: aplic
OBJS_aplic =  \
	aplic.o \
	INTERFAZ.o \
	FUNCP.o \
	ltmipcs.o \
	ltmtypes.o \
	comunicacion.o

SYSLIBS_aplic = -lpthread 
USERLIBS_aplic = $(SYSLIBS_aplic) 
DEPLIBS_aplic =  
LDLIBS_aplic = $(USERLIBS_aplic)


# Link or archive
aplic: $(OBJS_aplic) $(DEPLIBS_aplic)
	$(LINK.cc) $(CCFLAGS_aplic) $(CPPFLAGS_aplic) -o $@ $(OBJS_aplic) $(LDLIBS_aplic)

simplex: $(OBJS_simplex) $(DEPLIBS_aplic)
	$(LINK.cc) $(CCFLAGS_aplic) $(CPPFLAGS_aplic) -o $@ $(OBJS_simplex) $(LDLIBS_aplic)

duplex_nonblock: $(OBJS_duplex_nonblock) $(DEPLIBS_aplic)
	$(LINK.cc) $(CCFLAGS_aplic) $(CPPFLAGS_aplic) -o $@ $(OBJS_duplex_nonblock) $(LDLIBS_aplic)

duplex: $(OBJS_duplex) $(DEPLIBS_aplic)
	$(LINK.cc) $(CCFLAGS_aplic) $(CPPFLAGS_aplic) -o $@ $(OBJS_duplex) $(LDLIBS_aplic)

## Target: ltmd
OBJS_ltmd =  \
	BPRINCIPAL.o \
	comunicacion.o \
	FUNCP.o \
	ltmd.o \
	ltmdaemon.o \
	ltmipcs.o \
	ltmtypes.o
SYSLIBS_ltmd = -lpthread 
USERLIBS_ltmd = $(SYSLIBS_ltmd) 
DEPLIBS_ltmd =  
LDLIBS_ltmd = $(USERLIBS_ltmd)


# Link or archive
ltmd: $(TARGETDIR_ltmd) $(OBJS_ltmd) $(DEPLIBS_ltmd)
	$(LINK.cc) $(CCFLAGS_ltmd) $(CPPFLAGS_ltmd) -o $@ $(OBJS_ltmd) $(LDLIBS_ltmd)


#### Clean target deletes all generated files ####
.PHONY: clean
clean:
	rm -f -- \
		ltm_getRAWfdv6 \
		ltm_getRAWfdv6.o \
		aplic \
		aplic.o \
		INTERFAZ.o \
		ltmipcs.o \
		ltmtypes.o \
		ltmd \
		BPRINCIPAL.o \
		comunicacion.o \
		FUNCP.o \
		ltmd.o \
		ltmdaemon.o \
		ltmipcs.o \
		ltmtypes.o
# Enable dependency checking
.KEEP_STATE:
.KEEP_STATE_FILE:.make.state.GNU-amd64-Linux

