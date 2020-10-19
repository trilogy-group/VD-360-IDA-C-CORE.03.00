RELATIVEROOTDIR=..

include $(RELATIVEROOTDIR)/Makefile.head

MK_REL_BINPATH=$(MODULESPATH)

LIBSRCS=IdaTdfChannel \
	IdaTdfChannelMgr \
	IdaRequestContainer \
	IdaRequestList \
	IdaStringToEnum \
	IdaTdfAccess \
        IdaTdfProcess \
	IdaWebConnection \
        IdaWebInterface \
        IdaXmlHelper

LIBNAME=libidatdf

BINARIES= \
	IdaTdfProcess 

include $(RELATIVEROOTDIR)/Makefile.tail

LIBRARIES=



ifeq ($(UNAME), Windows)
$(MK_REL_BINPATH)/IdaTdfProcess$(EXE): $(PLATFORMDIR)/IdaTdfProcessMain.o	
	$(LINKOPS) libidabase.lib \
	libidatdf.lib tdframe.lib \
	tdsearch.lib apibase.lib $(OBJCOMLIBS) $(NETLIBS)
else
$(MK_REL_BINPATH)/IdaTdfProcess$(EXE): $(PLATFORMDIR)/IdaTdfProcessMain.o	
	$(LINKOPS) -lidatdf -ltdframe -ltdsearch \
	-lapibase -lseslogin $(OBJCOMLIBS) $(NETLIBS)
endif






