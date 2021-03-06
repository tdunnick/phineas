Welcome to the PHINEAS project!

PHINEAS is an open source functional replacement for the CDC PHINMS 
application.  It is distributed under the Apache 2.0 license. I 
encourage you to explore the various folders, files, 
and links found with this project for additional information.

The primary external components needed are the TinyC compiler, the
OpenSSL libraries and headers, and a set of PSDK headers.  However,
feel free to substitute the component of your choice.  The source
code is written in plain old ANSI C using standard C libarary calls
where possible and MS API as needed.  To run the binaries you only 
need the OpenSSL libraries installed in your PATH or with the
binarys themselves.

Files of immediate interest are the .BAT files that compile, build,
and optionally run PHINEAS artifacts.  In particular you will need
to modify "build.bat" for the installation paths of the components 
mentioned above.  Release notes are found in the notes directory.

PHINEAS now includes a script for building NSIS installation packages.
See http://nsis.sourceforge.net for more information.

Current build artifacts (in bin/) include:

phineass.exe - a stand alone service tray sender
phineasr.exe - a stand alone service tray receiver
phineas.exe - a stand alone service tray tranceiver
phineasd.exe - a stand MS service (daemon)
xmlb.exe - an XML beautifier
cpa.exe - a CPA generator
chelp.exe - configuration help generator
psetup.exe - configuration setup routine
accessdb.vbs - creates an empty ACCESS dB for Phineas

Before running PHINEAS you will additonally need to edit and/or create
a configuration file using templates/Phineas.xml.  Modify src\unittest.h 
in order to run test.bat.  It includes an embedded configuration.

Be aware this is currently an 'alpha' version of the software.  While
functional, it is not yet "ready for prime time".  Until such a time
that current bugs can be fixed and a complete testing harness can
be created I would not recommend using this code for production systems.
There are still many bugs and issues to resolve.

Links of interest...

https://mywebspace.wisc.edu/tdunnick/web/phineas/index.html
http://code.google.com/p/phinms-shareware/
https://github.com/
http://bellard.org/tcc/
http://www.openssl.org/
http://www.apache.org/licenses/LICENSE-2.0
