# Makefile for bounds checking insertion/optimization

# Path to top level of LLVM hierarchy
LEVEL = ../..

# Name of the library to build
LIBRARYNAME = Phase1

# Make the shared library become a loadable module so the tools can 
# dlopen/dlsym on the resulting library.
LOADABLE_MODULE = 1

# Tell the build system which LLVM libraries your pass needs. You'll probably
# need at least LLVMSystem.a, LLVMSupport.a, LLVMCore.a but possibly several
# others too.
LLVMLIBS = LLVMSupport.a LLVMSystem.a

# Include the makefile implementation stuff
include $(LEVEL)/Makefile.common
