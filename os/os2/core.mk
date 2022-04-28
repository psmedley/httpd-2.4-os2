# Some rules for making a shared core dll on OS/2

os2core: httpd.dll $(CORE_IMPLIB)
	$(LIBTOOL) --mode=link gcc -Zstack 2048 $(LDFLAGS) $(EXTRA_LDFLAGS) -o httpx $(CORE_IMPLIB) -lssl -lcrypto -lcx -lz 
	cp -p httpx.exe httpd.exe

httpd.dll: $(PROGRAM_DEPENDENCIES) $(CORE_IMPLIB)
	$(LINK) -Zdll -Zomf $(EXTRA_LDFLAGS) -o $@ buildmark.o server/exports.lo modules.lo $(PROGRAM_DEPENDENCIES) $(AP_LIBS) server/ApacheCoreOS2.def -lpcre2 -lcx
