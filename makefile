CXXFLAGS := $(shell llvm-config --cxxflags)
LLVMLDFLAGS := $(shell llvm-config --ldflags --libs)
SOURCES = clang-tags.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXES = $(OBJECTS:.o=)
CLANGLIBS = -lclangParse \
    -lclangSema \
    -lclangAnalysis \
    -lclangAST \
	-lclangFrontend \
	-lclangLex \
	-lclangBasic \
	-lLLVMSupport \

all: $(OBJECTS) $(EXES)

%: %.o
	$(CXX) -o $@ $< $(CLANGLIBS) $(LLVMLDFLAGS)

clean:
	-rm -f $(EXES) $(OBJECTS) *~
