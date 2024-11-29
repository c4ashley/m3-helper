TARGET += M3
OBJECTS += Device
OBJECTS += OutputDevice
OBJECTS += InputDevice
OBJECTS += main
OBJECTS += SysexBuilder
OBJECTS += Event

QUALIFIEDOBJECTS = $(addprefix ${OBJDIR}/,$(addsuffix .o,${OBJECTS}))

MAKEFLAGS += "-j 12"

ifeq (${PLATFORM},)
	PLATFORM = windows
endif

ifeq (${PLATFORM},unix)
    EXTENSION = 
    TOOLCHAIN = 
    OS        = Linux
	DEFINES  += _UNIX
	PLATFORMEXT = unix
	LIBS     += asound
else ifeq (${PLATFORM},windows)
    EXTENSION = .exe
    LIBS     += winmm
    TOOLCHAIN = x86_64-w64-mingw32-
    OS        = Windows
	PLATFORMEXT = win32
else
    $(error Invalid platform. Valid platforms are: unix, windows)
endif

ifeq (${CONFIGURATION},)
    CONFIGURATION = Debug
endif

ifeq (${CONFIGURATION},Debug)
    FLAGS += -g3 -Og
else ifeq (${CONFIGURATION},Release)
        FLAGS += -O3
else
        $(error Invalid configuration. Valid configurations are: Debug, Release)
endif

OBJDIR = obj/${OS}/${CONFIGURATION}
BINDIR = bin/${OS}/${CONFIGURATION}

FLAGS += -std=c++20 -Wno-unknown-pragmas

${BINDIR}/${TARGET}${EXTENSION}: ${QUALIFIEDOBJECTS}
	${TOOLCHAIN}g++ ${FLAGS} -Wall -Wextra -o $@ $^ $(addprefix -l,${LIBS})

${OBJDIR}/%.o: %.cpp %.${PLATFORMEXT}.cpp %.hpp
	${TOOLCHAIN}g++ $(addprefix -D,${DEFINES}) ${FLAGS} -Wall -Wextra -c -o $@ $<
${OBJDIR}/%.o: %.cpp %.hpp
	${TOOLCHAIN}g++ $(addprefix -D,${DEFINES}) ${FLAGS} -Wall -Wextra -c -o $@ $<
${OBJDIR}/%.o: %.cpp %.h
	${TOOLCHAIN}g++ $(addprefix -D,${DEFINES}) ${FLAGS} -Wall -Wextra -c -o $@ $<
${OBJDIR}/%.o: %.cpp
	${TOOLCHAIN}g++ $(addprefix -D,${DEFINES}) ${FLAGS} -Wall -Wextra -c -o $@ $<

clean:
	rm -f ${OBJDIR}/*
	rm -f ${BINDIR}/*

${QUALIFIEDOBJECTS}: | ${OBJDIR} ${BINDIR}

${OBJDIR}:
	mkdir -p "${OBJDIR}"

${BINDIR}:
	mkdir -p "${BINDIR}"
