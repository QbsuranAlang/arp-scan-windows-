@rem compile arp-scan tool for x86 and x64

@echo off
@set COMPILE=cl /nologo /MT /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /I arp-scan arp-scan/arp-scan.c arp-scan/getopt.c

@rem compile x86
rm -Rf Release(x86)
mkdir Release(x86)
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
%COMPILE%
del *.obj *.exp
mv arp-scan.exe Release(x86)/

@rem compile amd64
rm -Rf Release(x64)
mkdir Release(x64)
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
%COMPILE%
del *.obj *.exp
mv arp-scan.exe Release(x64)/

echo Done!
exit /b %errorlevel%