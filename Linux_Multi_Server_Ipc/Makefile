CC=g++
CXX=g++
RANLIB=ranlib
CLIENT_SRC=client.cpp
SERVER_SRC=server.cpp
CLIENT_OBJ=$(CLIENT_SRC:.cpp=.o)
SERVER_OBJ=$(SERVER_SRC:.cpp=.o)
INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)
CLIENT_LIB = libclient.a
SERVER_LIB = libserver.a
TARGETS = $(CLIENT_LIB) $(SERVER_LIB)
TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS=$(CLIENT_SRC) $(SERVER_SRC) Makefile README ANSWERS.txt
all: $(TARGETS)
$(CLIENT_LIB): $(CLIENT_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
$(SERVER_LIB): $(SERVER_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
clean:
	$(RM) $(TARGETS) $(CLIENT_OBJ) $(SERVER_OBJ) *~ *core
depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(CLIENT_SRC) $(SERVER_SRC)
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
