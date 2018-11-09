# OPTION
DEBUGBUILD     ?= n
SILENCE        ?= n

# USER APPLICATION
TARGET         := MS500MultiDownload
ROOT           := .
USERLIBS       := pthread \
				  wiringPi

USERDEFINES    := __MP_DEBUG_BUILD__
#USERDEFINES    := __TEST10000__ 

# COMPILER SETTING
PREFIX          = arm-linux-gnueabihf-
CC              = $(PREFIX)gcc
LD              = $(PREFIX)g++
CXX             = $(PREFIX)g++
OBJCOPY         = $(PREFIX)objcopy
OBJDUMP         = $(PREFIX)objdump
RM              = rm -rf

# DIRECTORY DEFINE
ifeq ($(DEBUGBUILD),y)
BUILD          := Debug
else
BUILD          := Release
endif

# BUILD MESSAGE ENABLE/DISABLE
ifeq ($(SILENCE),y)
V               = @
else
V               =
endif

# DEFINES
# INC
INCDIR         := $(ROOT) \
				  $(ROOT)/inc \
				  $(ROOT)/miniini/inc
INCPATHS       := $(foreach dir,$(INCDIR),$(wildcard $(dir)/*.h))

INCS           := $(notdir $(INCPATHS))
INCS           := $(sort $(INCS))

# SRC
SRCDIR         := $(ROOT) \
				  $(ROOT)/src
SRCPATHS       := $(foreach dir,$(SRCDIR),$(wildcard $(dir)/*.c)) \
				  $(foreach dir,$(SRCDIR),$(wildcard $(dir)/*.cpp))
SRCS           := $(notdir $(SRCPATHS))
SRCS           := $(sort $(SRCS))

# LIB
LIBDIR         := $(ROOT)/lib \
				  $(ROOT)/miniini/lib
LIBS           += $(USERLIBS)
LIBS           := $(sort $(LIBS))

# OBJ
OBJDIR         := $(ROOT)/obj
OBJS           := $(basename $(SRCS))
OBJS           := $(OBJS:%=%.o)
OBJS           := $(sort $(OBJS))

DEFINES        += $(USERDEFINES)
ifeq ($(DEBUGBUILD),y)
DEFINES        += 
endif
DEFINES        := $(sort $(DEFINES))

# C OPTION
CFLAGS         := $(INCDIR:%=-I%) \
				  $(DEFINES:%=-D%) \
				  $(LIBDIR:%=-L%) \
				  $(LIBS:%=-l%)
ifeq ($(DEBUGBUILD),y)
CFLAGS         += -g
CFLAGS         += -O0
CFLAGS         += -ggdb
else
CFLAGS         += -O2
endif
CFLAGS         += -pthread
CFLAGS         := $(sort $(CFLAGS))

# CPP OPTION
CXXFLAGS       := $(INCDIR:%=-I%) \
				  $(DEFINES:%=-D%) \
				  $(LIBDIR:%=-L%) \
				  $(LIBS:%=-l%) \
				  -pthread
ifeq ($(DEBUGBUILD),y)
LDFLAGS        += -Wall
CXXFLAGS       += -g
CXXFLAGS       += -ggdb
CXXFLAGS       += -O0
else
CXXFLAGS       += -O2
endif
CXXFLAGS       += 
CXXFLAGS       := $(sort $(CXXFLAGS))

# LD OPTION
LDFLAGS        := $(INCDIR:%=-I%) \
				  $(DEFINES:%=-D%) \
				  $(LIBDIR:%=-L%) \
				  $(LIBS:%=-l%) \
				  -pthread
ifeq ($(DEBUGBUILD),y)
LDFLAGS        += -ggdb
LDFLAGS        += -g
LDFLAGS        += -Wall
LDFLAGS        += -O0
else
LDFLAGS        += -O2
endif
LDFLAGS        := $(sort $(LDFLAGS))

# BUILD
vpath %.c	$(SRCDIR)
.SUFFIXES: .o.c

vpath %.cpp	$(SRCDIR)
.SUFFIXES: .o.cpp

.PHONY: all bin asm clean info
all: $(TARGET)
$(TARGET): $(OBJS)
	@echo "Linking : [$^] => $@"
	@if [ ! -d $(BUILD) ]; then mkdir -p $(BUILD) ; fi
	$(LD) $(LDFLAGS) -o $(BUILD)/$@ $(^:%=$(OBJDIR)/%)
#	scp -i ~/.ssh/id_rsa Debug/MPRaspberryPiClient pi@192.168.0.212:~/Desktop/.

.c.o:
	@echo "C Compile : $< => $@"
	@if [ ! -d $(OBJDIR) ]; then mkdir -p $(OBJDIR) ; fi
	$(V)$(CC) $(CFLAGS) -c $< -o $(OBJDIR)/$@

.cpp.o:
	@echo "CPP Compile : $< => $@"
	@if [ ! -d $(OBJDIR) ]; then mkdir -p $(OBJDIR) ; fi
	$(V)$(CXX) $(CXXFLAGS) -c $< -o $(OBJDIR)/$@

bin:
	$(OBJCOPY) --gap-fill=0xff -O binary  $(BUILD)/$(TARGET) $(BUILD)/$(TARGET).bin

asm:
	$(OBJDUMP) --disassemble-all $(BUILD)/$(TARGET) > $(BUILD)/$(TARGET).S

clean:
	$(RM) $(OBJS)
	$(RM) $(OBJDIR)
	$(RM) $(TARGET)
	$(RM) $(BUILD)

info:
	@echo "INCS: $(INCS)"
	@echo "SRCS: $(SRCS)"
	@echo "LIBS: $(LIBS)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
