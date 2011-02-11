#include <iostream>
#include <vector>
using namespace std;

#include "clang/Sema/ParseAST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/Driver/InitHeaderSearch.h"
#include "PPContext.h"
using namespace clang;

class MyASTConsumer : public ASTConsumer {
public:
  virtual void HandleTopLevelDecl(Decl *D) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      for (; VD; VD = cast_or_null<VarDecl>(VD->getNextDeclarator())) {
        if (VD->isFileVarDecl() && VD->getStorageClass() != VarDecl::Extern)
          cerr << "Read top-level variable decl: '" << VD->getName() << "'\n";
      }
    }
  }
};

int main(int argc, char* argv[])
{
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  PPContext context;

  // Add header search directories
  InitHeaderSearch init(context.headers);
  init.AddDefaultSystemIncludePaths(context.opts);
  init.Realize();

  // Add input file
  const FileEntry* File = context.fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File, SourceLocation());

  // Parse it
  MyASTConsumer c;
  ParseAST(context.pp, &c);  // calls pp.EnterMainSourceFile() for us
}
