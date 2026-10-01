MKDIR?=mkdir -p
INSTALL?=install
RM?=rm -f
PREFIX?=/usr/local
MANDIR?=${PREFIX}/share/man

JTRANSFER_VERSION?=0.1.0

.PHONY: all
all: build install

.PHONY: build
build: jtransfer

jtransfer: jtransfer.c
	${CC} ${CFLAGS} -DJTRANSFER_VERSION="\"${JTRANSFER_VERSION}\"" -o jtransfer jtransfer.c -ljail

.PHONY: install
install: build
	${MKDIR} -m 755 -p "${DESTDIR}${MANDIR}"
	${MKDIR} -m 755 -p "${DESTDIR}${MANDIR}/man1"
	${INSTALL} -m 444 jtransfer.1 "${DESTDIR}${MANDIR}/man1/jtransfer.1"
	${MKDIR} -m 755 -p "${DESTDIR}${PREFIX}/bin"
	${INSTALL} -m 555 jtransfer "${DESTDIR}${PREFIX}/bin/jtransfer"

.PHONY: clean
clean:
	${RM} jtransfer

.PHONY: uninstall
uninstall:
	${RM} "${DESTDIR}${MANDIR}/man1/jtransfer.1"
	${RM} "${DESTDIR}${PREFIX}/bin/jtransfer"

.PHONY: docs
docs: jtransfer.1
	@mandoc -T ascii jtransfer.1 | col -b | tail +3 | sed -e '$$d' | sed -e '$$d' | sed -e 's|%%PREFIX%%|${PREFIX}|'  > README.txt
