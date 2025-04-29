# @version @$Id: buildenv.mk 12303 2012-10-02 11:49:49Z phurtley $
# 1. Set NOCYGWIN_USE_GCC3=yes and NOCYGWIN=yes if using gcc 3 in cygwin.
# 2. If using gcc 4 in cygwin, just set NOCYGWIN=yes.
# 3. If using native cygwin or linux, do nothing.  

ifeq "$(strip $(NOCYGWIN))" "yes"
  ifeq "$(NOCYGWIN_USE_GCC3)" "yes"
    SBTOOLS_BUILD_GCC=gcc-3 -mno-cygwin
    SBTOOLS_BUILD_LINKDLL=dllwrap --driver-name gcc-3 -mno-cygwin --dllname $(notdir $1) -o $1 --target=i386-mingw32 --def $2 $3
    SBTOOLS_BUILD_AR=ar
  else
    SBTOOLS_BUILD_GCC=i686-w64-mingw32-gcc -L/lib
    SBTOOLS_BUILD_LINKDLL=i686-w64-mingw32-dllwrap -L/lib --dllname $(notdir $1) -o $1 --target=i386-mingw32 --def $2 $3
    SBTOOLS_BUILD_AR=i686-w64-mingw32-ar
  endif
else
  SBTOOLS_BUILD_GCC?=gcc
  SBTOOLS_BUILD_LINKDLL=$(SBTOOLS_BUILD_GCC) -shared -o $1 $3
  SBTOOLS_BUILD_AR=ar
endif

# vi: set noautoindent nocindent noet:
