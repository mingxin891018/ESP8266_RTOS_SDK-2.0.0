
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
UP_EXTRACT_DIR = ..
GEN_LIBS = libopenssl.a
COMPONENTS_libopenssl = library/liblibrary.a platform/libplatform.a
endif

CCFLAGS += -fno-aggressive-loop-optimizations

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
DEFINES += -D_POSIX_SOURCE \
			-DLWIP_OPEN_SRC	\
			-DPBUF_RSV_FOR_WLAN	\
			-DEBUF_LWIP \
			-DMEMLEAK_DEBUG

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(SDK_PATH)/include/openssl/ \
                        -I $(SDK_PATH)/include/openssl/openssl \
                        -I $(SDK_PATH)/include/openssl/internal \
                        -I $(SDK_PATH)/include/openssl/platform
INCLUDES += -I ./
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

