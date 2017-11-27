@rem compile arp-scan tool for x86 and x64

@echo off
@set COMPILE=cl /nologo /MT /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /I arp-scan arp-scan/arp-scan.c arp-scan/getopt.c

@rem compile x86
rmdir /s /q Release(x86)
mkdir Release(x86)
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86
%COMPILE%
del /q *.obj *.exp
xcopy /Y arp-scan.exe Release(x86)\

@rem compile amd64
rmdir /s /q Release(x64)
mkdir Release(x64)
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
%COMPILE%
del /q *.obj *.exp
xcopy /Y arp-scan.exe Release(x64)\

del /q arp-scan.exe

echo Done!
exit /b %errorlevel%