HDR=	dbuf.h util.h b64.h xmln.h xml.h mime.h task.h \
	crypt.h net.h log.h queue.h filter.h ebxml.h \
	xcrypt.h payload.h cfg.h basicauth.h find.h fpoller.h \
	qpoller.h 

SRC=	dbuf.c util.c b64.c xmln.c xml.c mime.c task.c \
	crypt.c net.c log.c queue.c fileq.c odbcq.c filter.c ebxml.c \
	xcrypt.c payload.c cpa.c console.c cfg.c config.c server.c \
	basicauth.c find.c fpoller.c qpoller.c ebxml_sender.c \
	ebxml_receiver.c applink.c icon.o

OBJ=	dbuf.o util.o b64.o xmln.o xml.o mime.o task.o \
	crypt.o net.o log.o queue.o fileq.o odbcq.o filter.o ebxml.o \
	xcrypt.o payload.o cpa.o console.o cfg.o config.o server.o \
	basicauth.o find.o fpoller.o qpoller.o ebxml_sender.o \
	ebxml_receiver.o applink.o icon.o	

MAIN=	main.c icon.o

DEFS=	-D__SERVER__ -D__CONSOLE__ -D__FILEQ__ -D__ODBCQ__ 

GCCDIR=	C:\MinGW
WINDIR=	C:\WINDOWS\system32
SSLDIR=	C:\usr\prog\openssl\include
INCDIR=	-I$(GCCDIR)\include -I. -I$(SSLDIR)
LIB=	-LC:\PHP -lgdi32 -lws2_32 -lodbc32 -llibeay32 -lssleay32 
FLAGS=	-Os -s

CC=	gcc $(FLAGS) $(DEFS) $(INCDIR)

all: phineas phineasc phineasd apps

phineass:	$(HDR) $(SRC)
	$(CC) -o ..\bin\$@.exe -D__SENDER_ -mwindows $(MAIN) $(SRC) $(LIB)

phineasr:	$(HDR) $(SRC)
	$(CC) -o ..\bin\$@.exe -D__RECEIVER__ -mwindows  $(MAIN) $(SRC) $(LIB)

phineas:	$(HDR) $(SRC)
	$(CC) -o ..\bin\$@.exe  -D__RECEIVER__ -D__SENDER__ -mwindows  $(MAIN) $(SRC) $(LIB)

phineasc:	$(HDR) $(SRC)
	$(CC) -o ..\bin\$@.exe -DCOMMAND -D__RECEIVER__ -D__SENDER__ $(SRC) $(MAIN) $(LIB)

phineasd:	$(HDR) $(SRC)
	$(CC) -o ..\bin\$@.exe -DSERVICE -D__RECEIVER__ -D__SENDER__ -ladvapi32 $(SRC) $(MAIN) $(LIB)

apps:	chelp cpa xmlb psetup xcrypt

chelp:	$(HDR) chelp.c
	$(CC) -o ..\bin\$@.exe -DCMDLINE chelp.c $(LIB)

cpa:	$(HDR) cpa.c
	$(CC) -o ..\bin\$@.exe -DCMDLINE cpa.c $(LIB)

xmlb:	$(HDR) xmlb.c
	$(CC) -o ..\bin\$@.exe -DCMDLINE xmlb.c $(LIB)

psetup:	$(HDR) psetup.c
	$(CC) -o ..\bin\$@.exe  -DCMDLINE psetup.c $(LIB)

xcrypt:	$(HDR) xcrypt.c
	$(CC) -o ..\bin\$@.exe -DCMDLINE xcrypt.c $(LIB)

clean:
	+del *.o

icon.o:	icon.rc
	windres -O coff -i $< -o $@
