CXX = g++
CXXFLAGS = -Wall
SRC = Buzon.cc RondaPapa.cc
EXE = ronda

all: $(EXE)

$(EXE): $(SRC) Buzon.h
	$(CXX) $(CXXFLAGS) -o $(EXE) $(SRC)

clean:
	rm -f $(EXE) *.o

.PHONY: all clean