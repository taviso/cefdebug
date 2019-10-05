CC=cl.exe
RC=rc.exe
MSBUILD=msbuild.exe
CMAKE=cmake.exe
RFLAGS=/nologo
CFLAGS=/nologo /Zi /Od /MD /FS /I libwebsockets/include
LFLAGS=/nologo /machine:x86
VFLAGS=-no_logo
MFLAGS=/p:Configuration=Release /nologo /m /v:q
CXXFLAGS=/nologo /Zi /Od /EHsc /MD /FS
LDLIBS=iphlpapi ws2_32 edit websockets
LDFLAGS=/MD
LINKFLAGS=/ignore:4099
VSDEVCMD=cmd.exe /c vsdevcmd.bat

# Commands for arch specific compiler.
ifeq ($(OS),Windows_NT)
    CC64=$(VSDEVCMD) $(VFLAGS) -arch=amd64 ^& cl
    CC32=$(VSDEVCMD) $(VFLAGS) -arch=x86 ^& cl
else
    CC64=$(VSDEVCMD) $(VFLAGS) -arch=amd64 "&" cl
    CC32=$(VSDEVCMD) $(VFLAGS) -arch=x86 "&" cl
endif

.PHONY: clean distclean

all: cefdebug.exe

release: cefdebug.zip cefdebug-src.zip

%.res: %.rc
	$(RC) $(RFLAGS) $<

%.obj: %.cc
	$(CC) $(CXXFLAGS) /c /Fo:$@ $<

%.obj: %.c
	$(CC) $(CFLAGS) /c /Fo:$@ $<

%.exe: %.obj
	$(CC) $(CFLAGS) $(LDFLAGS) /Fe:$@ $^ /link $(LINKFLAGS) $(LDLIBS:=.lib)

%.dll: %.obj
	$(CC) $(CFLAGS) $(LDFLAGS) /LD /Fe:$@ $^ /link $(LINKFLAGS)

%64.obj: %.c
	$(CC) $(CFLAGS) /c /Fd:$(@:.obj=.pdb) /Fo:$@ $<

%32.obj: %.c
	$(CC) $(CFLAGS) /c /Fd:$(@:.obj=.pdb) /Fo:$@ $<

%64.dll: CC=$(CC64)
%64.dll: %64.obj version.res
	$(CC) $(CFLAGS) $(LDFLAGS) /LD /Fd:$(@:.dll=.pdb) /Fe:$@ $^ /link $(LINKFLAGS)

%32.dll: CC=$(CC32)
%32.dll: %32.obj version.res
	$(CC) $(CFLAGS) $(LDFLAGS) /LD /Fd:$(@:.dll=.pdb) /Fe:$@ $^ /link $(LINKFLAGS)

cefdebug.obj: | websockets.lib

cefdebug.exe: evaluate.obj wsurls.obj ports.obj cefdebug.obj | websockets.lib edit.lib

websockets.lib:
	-$(CMAKE) -S libwebsockets -B build-$@
	$(CMAKE) -ULWS_WITH_SSL -DLWS_WITH_SSL=OFF -S libwebsockets -B build-$@
	$(MSBUILD) $(MFLAGS) build-$@/libwebsockets.sln
	cmd.exe /c copy build-$@\\lib\\Release\\websockets_static.lib $@
	cmd.exe /c copy build-$@\\include\\lws_config.h lws_config.h

edit.lib:
	$(CMAKE) -S wineditline -B build-$@
	$(MSBUILD) $(MFLAGS) build-$@/WinEditLine.sln
	cmd.exe /c copy build-$@\\src\\Release\\edit_a.lib $@

clean:
	-cmd.exe /c del /q /f *.exp *.exe *.obj *.pdb *.ilk *.xml *.res *.ipdb *.iobj *.dll *.tmp 2\>nul
	-cmd.exe /c rmdir /q /s $(wildcard build-*.*) 2\>nul

# These are slow to rebuild and I dont change them often.
distclean: clean
	-cmd.exe /c del /q /f websockets.lib lws_config.h
	-cmd.exe /c del /q /f edit.lib

cefdebug.zip: README.md cefdebug.exe
	(cd .. && zip -r cefdebug/$@ $(patsubst %,cefdebug/%,$^))

cefdebug-src.zip:
	(cd .. && zip -x@cefdebug/.zipignore -r cefdebug/$@ cefdebug)
