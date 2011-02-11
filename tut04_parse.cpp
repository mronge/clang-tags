#include <iostream>
#include <vector>
using namespace std;

#include "clang/Basic/IdentifierTable.h"
#include "clang/Parse/Parser.h"
#include "clang/Driver/InitHeaderSearch.h"
#include "PPContext.h"
using namespace clang;

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
  context.pp.EnterMainSourceFile();


  // Parse it
  IdentifierTable tab(context.opts);
  MinimalAction action(tab);
  Parser p(context.pp, action);
  p.ParseTranslationUnit();

  tab.PrintStats();
}
