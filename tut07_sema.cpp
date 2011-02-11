#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>
using namespace std;

#include "clang/Sema/ParseAST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/Driver/InitHeaderSearch.h"
#include "llvm/Support/CommandLine.h"
#include "PPContext.h"

#include "llvm/System/Signals.h"
using namespace clang;

// This function is already in Preprocessor.cpp and clang.cpp. It should be
// in a library.

// Append a #define line to Buf for Macro.  Macro should be of the form XXX,
// in which case we emit "#define XXX 1" or "XXX=Y z W" in which case we emit
// "#define XXX Y z W".  To get a #define with no value, use "XXX=".
static void DefineBuiltinMacro(std::vector<char> &Buf, const char *Macro,
                               const char *Command = "#define ") {
  Buf.insert(Buf.end(), Command, Command+strlen(Command));
  if (const char *Equal = strchr(Macro, '=')) {
    // Turn the = into ' '.
    Buf.insert(Buf.end(), Macro, Equal);
    Buf.push_back(' ');
    Buf.insert(Buf.end(), Equal+1, Equal+strlen(Equal));
  } else {
    // Push "macroname 1".
    Buf.insert(Buf.end(), Macro, Macro+strlen(Macro));
    Buf.push_back(' ');
    Buf.push_back('1');
  }
  Buf.push_back('\n');
}

class MyASTConsumer : public ASTConsumer {
  SourceManager *sm;
public:
  virtual void Initialize(ASTContext &Context) {
    sm = &Context.getSourceManager();
  }

  virtual void HandleTopLevelDecl(Decl *D) {
    // XXX: prints both declaration and defintion of `a` in input05.c
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      for (; VD; VD = cast_or_null<VarDecl>(VD->getNextDeclarator())) {
        if (VD->isFileVarDecl() && VD->getStorageClass() != VarDecl::Extern) {
          FullSourceLoc loc(D->getLocation(), *sm);
          bool isStatic = VD->getStorageClass() == VarDecl::Static;

          loc = loc.getLogicalLoc();
          cout << loc.getSourceName() << ": "
            << (isStatic?"static ":"") << VD->getName() << "\n";
        }
      }
    }
  }
};


static llvm::cl::list<std::string>
D_macros("D", llvm::cl::value_desc("macro"), llvm::cl::Prefix,
    llvm::cl::desc("Predefine the specified macro"));

static llvm::cl::list<std::string>
I_dirs("I", llvm::cl::value_desc("directory"), llvm::cl::Prefix,
    llvm::cl::desc("Add directory to include search path"));

static llvm::cl::opt<string>
InputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"),
    llvm::cl::Required);

static llvm::cl::list<std::string> IgnoredParams(llvm::cl::Sink);
static llvm::cl::opt<string> dummy("o", llvm::cl::desc("dummy for gcc compat"));


int main(int argc, char* argv[])
{
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::cl::ParseCommandLineOptions(argc, argv, " globalcollect\n"
      "  This collects and prints global variables found in C programs.");

  if (!IgnoredParams.empty()) {
    cerr << "Ignoring the following parameters:";
    copy(IgnoredParams.begin(), IgnoredParams.end(),
        ostream_iterator<string>(cerr, " "));
  }

  // Create Preprocessor object
  PPContext context;

  // Add header search directories
  InitHeaderSearch init(context.headers);
  // user headers
  for (int i = 0; i < I_dirs.size(); ++i) {
    cerr << "adding " << I_dirs[i] << endl;
    init.AddPath(I_dirs[i], InitHeaderSearch::Angled, false, true, false);
  }
  init.AddDefaultSystemIncludePaths(context.opts);
  init.Realize();

  // Add defines passed in through parameters
  vector<char> predefineBuffer;
  for (int i = 0; i < D_macros.size(); ++i) {
    cerr << "defining " << D_macros[i] << endl;
    ::DefineBuiltinMacro(predefineBuffer, D_macros[i].c_str());
  }
  predefineBuffer.push_back('\0');
  context.pp.setPredefines(&predefineBuffer[0]);


  // Add input file
  const FileEntry* File = context.fm.getFile(InputFilename);
  if (!File) {
    cerr << "Failed to open \'" << InputFilename << "\'" << endl;
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File, SourceLocation());


  // Parse it
  cout << "<h2><code>" << InputFilename << "</code></h2>" << endl << endl;
  cout << "<pre><code>";

  MyASTConsumer c;
  ParseAST(context.pp, &c);

  cout << "</pre></code>" << endl << endl;

  cout << endl;

  unsigned NumDiagnostics = context.diags.getNumDiagnostics();
  
  if (NumDiagnostics)
    fprintf(stderr, "%d diagnostic%s generated.\n", NumDiagnostics,
            (NumDiagnostics == 1 ? "" : "s"));
}
