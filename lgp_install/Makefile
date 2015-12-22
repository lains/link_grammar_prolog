# The link parser is a generic C program that compiles with no errors or
# warnings on Unixes by DEC, HP, SUN, and Linux.  (It also compiles with
# no errors under windows 3.1, 95, and NT.  Get the windows distribution
# parse.zip in the directory above this one.)
#
# To compile the program type "make" while in this directory.  If you
# get an error, you may need to modify the CFLAGS line by replacing it
# with one of the following three options:
#
# CFLAGS     = -c -O -D__linux__
# CFLAGS     = -c -O -D__hpux__
# CFLAGS     = -c -O -D__sun__
#
# On at least one system (BSDI 3.0) I had to use gmake instead of make
# to get this makefile to work.     -- D. Sleator, June 2000

SRC         = /lgp/system-4.1/link-4.1/src
INC         = /lgp/system-4.1/link-4.1/include
OBJ         = /lgp/system-4.1/link-4.1/obj
SWIHOME     = /cygdrive/c/Program\ Files/SWI-Prolog
BIN         = .
CC          = gcc
CFLAGS      = -c -g -Wall -O
CLDFLAGS    = -O -g 

INCLUDES    =\
${INC}/lgp.h\
${INC}/link-includes.h ${INC}/structures.h ${INC}/api-structures.h\
${INC}/api.h ${INC}/externs.h ${INC}/analyze-linkage.h\
${INC}/and.h ${INC}/build-disjuncts.h ${INC}/count.h\
${INC}/error.h ${INC}/extract-links.h ${INC}/fast-match.h\
${INC}/idiom.h ${INC}/linkset.h ${INC}/massage.h\
${INC}/post-process.h ${INC}/pp_knowledge.h ${INC}/pp_lexer.h\
${INC}/pp_linkset.h ${INC}/preparation.h ${INC}/print.h\
${INC}/prune.h ${INC}/read-dict.h ${INC}/resources.h\
${INC}/string-set.h ${INC}/tokenize.h ${INC}/utilities.h\
${INC}/constituents-swi-prolog.h ${INC}/word-file.h ${INC}/print-util.h

OBJECTS     =\
${OBJ}/lgp.o ${OBJ}/prune.o ${OBJ}/and.o\
${OBJ}/post-process.o ${OBJ}/pp_lexer.o ${OBJ}/resources.o\
${OBJ}/analyze-linkage.o ${OBJ}/string-set.o ${OBJ}/pp_linkset.o\
${OBJ}/pp_knowledge.o ${OBJ}/error.o ${OBJ}/word-file.o\
${OBJ}/utilities.o ${OBJ}/tokenize.o ${OBJ}/command-line.o\
${OBJ}/read-dict.o ${OBJ}/print.o ${OBJ}/preparation.o\
${OBJ}/api.o ${OBJ}/massage.o ${OBJ}/linkset.o\
${OBJ}/idiom.o ${OBJ}/fast-match.o ${OBJ}/extract-links.o\
${OBJ}/count.o ${OBJ}/build-disjuncts.o ${OBJ}/constituents-swi-prolog.o\
${OBJ}/print-util.o

${BIN}/lgp:     ${OBJECTS}
		${CC} -shared -Wall ${CLDFLAGS} ${OBJECTS} ${SWIHOME}/bin/LIBPL.DLL -o ${BIN}/lgp.dll

${OBJ}/lgp.o:   ${SRC}/lgp.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} -I${SWIHOME}/include ${SRC}/lgp.c -o ${OBJ}/lgp.o

${OBJ}/constituents-swi-prolog.o: ${SRC}/constituents-swi-prolog.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/constituents-swi-prolog.c -o ${OBJ}/constituents-swi-prolog.o

${OBJ}/prune.o: ${SRC}/prune.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/prune.c -o ${OBJ}/prune.o

${OBJ}/and.o: ${SRC}/and.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/and.c -o ${OBJ}/and.o

${OBJ}/post-process.o: ${SRC}/post-process.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/post-process.c -o ${OBJ}/post-process.o

${OBJ}/pp_lexer.o: ${SRC}/pp_lexer.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/pp_lexer.c -o ${OBJ}/pp_lexer.o

${OBJ}/resources.o: ${SRC}/resources.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/resources.c -o ${OBJ}/resources.o

${OBJ}/analyze-linkage.o: ${SRC}/analyze-linkage.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/analyze-linkage.c -o ${OBJ}/analyze-linkage.o

${OBJ}/string-set.o: ${SRC}/string-set.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/string-set.c -o ${OBJ}/string-set.o

${OBJ}/pp_linkset.o: ${SRC}/pp_linkset.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/pp_linkset.c -o ${OBJ}/pp_linkset.o

${OBJ}/pp_knowledge.o: ${SRC}/pp_knowledge.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/pp_knowledge.c -o ${OBJ}/pp_knowledge.o

${OBJ}/error.o: ${SRC}/error.c ${INC}/error.h
		${CC} ${CFLAGS} -I${INC} ${SRC}/error.c -o ${OBJ}/error.o

${OBJ}/word-file.o: ${SRC}/word-file.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/word-file.c -o ${OBJ}/word-file.o

${OBJ}/utilities.o: ${SRC}/utilities.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/utilities.c -o ${OBJ}/utilities.o

${OBJ}/tokenize.o: ${SRC}/tokenize.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/tokenize.c -o ${OBJ}/tokenize.o

${OBJ}/command-line.o: ${SRC}/command-line.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/command-line.c -o ${OBJ}/command-line.o

${OBJ}/read-dict.o: ${SRC}/read-dict.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/read-dict.c -o ${OBJ}/read-dict.o

${OBJ}/print.o: ${SRC}/print.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/print.c -o ${OBJ}/print.o

${OBJ}/preparation.o: ${SRC}/preparation.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/preparation.c -o ${OBJ}/preparation.o

${OBJ}/api.o: ${SRC}/api.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/api.c -o ${OBJ}/api.o

${OBJ}/massage.o: ${SRC}/massage.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/massage.c -o ${OBJ}/massage.o

${OBJ}/linkset.o: ${SRC}/linkset.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/linkset.c -o ${OBJ}/linkset.o

${OBJ}/idiom.o: ${SRC}/idiom.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/idiom.c -o ${OBJ}/idiom.o

${OBJ}/fast-match.o: ${SRC}/fast-match.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/fast-match.c -o ${OBJ}/fast-match.o

${OBJ}/extract-links.o: ${SRC}/extract-links.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/extract-links.c -o ${OBJ}/extract-links.o

${OBJ}/count.o: ${SRC}/count.c ${INCLUDES} ${INC}/print.h
		${CC} ${CFLAGS} -I${INC} ${SRC}/count.c -o ${OBJ}/count.o

${OBJ}/build-disjuncts.o: ${SRC}/build-disjuncts.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/build-disjuncts.c -o ${OBJ}/build-disjuncts.o

${OBJ}/print-util.o: ${SRC}/print-util.c ${INCLUDES}
		${CC} ${CFLAGS} -I${INC} ${SRC}/print-util.c -o ${OBJ}/print-util.o

clean:
	/bin/rm ${OBJ}/*.o
