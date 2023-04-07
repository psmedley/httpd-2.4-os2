set LIBS = -lpthread -lssl -lcrypto -lexpat -lz -lcx
md srclib\apr
md srclib\apr\build
copy u:\extras\bin\aplibtool.exe srclib\apr\build
dash ./configure --prefix=/apache24 --enable-modules=all --enable-mods-shared=all --disable-cgid --enable-cgi --with-apr=u:/extras/bin/apr-1-config --enable-ssl --enable-proxy --enable-proxy-connect --enable-proxy-ftp --enable-proxy-ajp --enable-proxy-balancer --enable-echo --enable-charset-lite --with-z=u:/extras --with-apr-util=u:/extras/bin/apu-1-config  --enable-cache --enable-disk-cache --disable-file-cache --disable-proxy-fdpass  2>&1 | tee configure.log
rem CFLAGS & LDFLAGS in build\config_vars.mk
rem add .libs\mod_proxy.lib to build\config_vars.mk MOD_PROXY_EXPRESS_LDADD & MOD_PROXY_HCHECK_LDADD
rem remove -export-symbols-regex md_module from build\config_vars.mk MOD_MD_LDADD 
rem add -lpthread -lcx -lz to modules\ssl\modules.mk MOD_LDFLAGS 
rem add -lpthread -lcx -lz to support\makefile PROGRAM_LDADD        