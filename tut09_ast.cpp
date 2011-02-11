#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <vector>
#include <map>
#include <cctype>
#include <unistd.h>  // getcwd()
using namespace std;

#include "clang/Sema/ParseAST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/TranslationUnit.h"

#include "clang/Rewrite/HTMLRewrite.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/System/Path.h"
#include "llvm/System/Signals.h"

#include "clang/Driver/InitHeaderSearch.h"
#include "PPContext.h"

using namespace clang;


// define funcs {{{

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
// }}}

string getContainingLine(FullSourceLoc Pos) {
  assert(Pos.isValid());
  FullSourceLoc LPos = Pos.getLogicalLoc();
  const char* lTokPtr = LPos.getCharacterData();
  const char* start = lTokPtr - LPos.getColumnNumber() + 1;

  const llvm::MemoryBuffer *Buffer = LPos.getBuffer();
  const char *BufEnd = Buffer->getBufferEnd();
  const char* end = lTokPtr;
  while (end != BufEnd && 
      *end != '\n' && *end != '\r')
    ++end;
  return string(start, end);
}

// XXX: use some llvm map?
typedef map<VarDecl*, vector<DeclRefExpr*> > UsageMap;

// XXX: hack
map<DeclRefExpr*, FunctionDecl*> enclosing;

class FindUsages : public StmtVisitor<FindUsages> {
  UsageMap& uses;
  FunctionDecl* fd;
public:
  FindUsages(UsageMap& um) : uses(um) {}

  void VisitDeclRefExpr(DeclRefExpr* expr) {
    if (VarDecl* vd = dyn_cast<VarDecl>(expr->getDecl())) {
      UsageMap::iterator im = uses.find(vd);
      if (im != uses.end()) {
        im->second.push_back(expr);
        enclosing[expr] = fd;
      }
    }
  }

  // XXX: should either be directly in visitor or some RecursiveVisitor
  // subclass thereof.
  void VisitStmt(Stmt* stmt) {
    Stmt::child_iterator CI, CE = stmt->child_end();
    for (CI = stmt->child_begin(); CI != CE; ++CI) {
      if (*CI != 0) {
        Visit(*CI);
      }
    }
  }

  void process(FunctionDecl* fd) {
    this->fd = fd;
    Visit(fd->getBody());
  }
};


class MyASTConsumer : public ASTConsumer {
  SourceManager *sm;
  vector<FunctionDecl*> functions;
  vector<pair<VarDecl*, bool> > globals;
  ostream& out;
public:
  MyASTConsumer(ostream& o) : out(o) {}

  virtual void Initialize(ASTContext &Context) {
    sm = &Context.getSourceManager();
  }

  virtual void HandleTopLevelDecl(Decl *D) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->isFileVarDecl()) {
        // XXX: does not store c in `int b, c;`.
        globals.push_back(make_pair(VD,
              VD->getStorageClass() != VarDecl::Extern));
      }
    } else if (FunctionDecl* FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->getBody() != 0) {
        // XXX: This also collects functions from header files.
        // This makes us slower than necesary, but doesn't change the results.
        functions.push_back(FD);
      }
    }
  }

  virtual void HandleTranslationUnit(TranslationUnit& tu) {
    // called when everything is done

    UsageMap uses;
    for (int i = 0; i < globals.size(); ++i) {
      uses[globals[i].first] = vector<DeclRefExpr*>();
    }

    FindUsages fu(uses);
    for (int i = 0; i < functions.size(); ++i) {
      fu.process(functions[i]);
    }

    vector<DeclRefExpr*> allUses;
    vector<VarDecl*> interestingGlobals;
    for (int i = 0; i < globals.size(); ++i) {

      // skip globals from system headers
      VarDecl* VD = globals[i].first;
      FullSourceLoc loc(VD->getLocation(), *sm);
      if (loc.isInSystemHeader()) continue;


      //allUses.append(uses[VD].begin(), uses[VD].end());
      allUses.insert(allUses.end(), uses[VD].begin(), uses[VD].end());

      // externs should not be printed as defined vars, but they
      // should be included in the use list
      if (!globals[i].second) continue;

      interestingGlobals.push_back(VD);
    }

    out << interestingGlobals.size() << " defines\n";
    for (int i = 0; i < interestingGlobals.size(); ++i) {
      VarDecl* VD = interestingGlobals[i];
      FullSourceLoc loc(VD->getLocation(), *sm);

      bool isStatic = VD->getStorageClass() == VarDecl::Static;
      out << loc.getSourceName() << ":" << loc.getLogicalLineNumber() << " "
          << (isStatic?"static ":"") << VD->getName()
          << "\n";
    }

    vector<DeclRefExpr*> allInterestingUses;
    for (int j = 0; j < allUses.size(); ++j) {
      DeclRefExpr* dre = allUses[j];
      FullSourceLoc loc(dre->getLocStart(), *sm);
      // Skip uses in functions from system headers
      if (loc.isInSystemHeader()) continue;
      allInterestingUses.push_back(dre);
    }

    out << allInterestingUses.size() << " uses\n";
    for (int j = 0; j < allInterestingUses.size(); ++j) {
      DeclRefExpr* dre = allInterestingUses[j];
      FunctionDecl* fd = enclosing[dre];
      FullSourceLoc loc(dre->getLocStart(), *sm);

      out << fd->getName() << ":"
          << loc.getLogicalLineNumber()
          << " " << dre->getDecl()->getName()
          << "\n"
          << getContainingLine(loc)
          << "\n";
    }
  }
};

static llvm::cl::list<std::string>
D_macros("D", llvm::cl::value_desc("macro"), llvm::cl::Prefix,
    llvm::cl::desc("Predefine the specified macro"));

static llvm::cl::list<std::string>
I_dirs("I", llvm::cl::value_desc("directory"), llvm::cl::Prefix,
    llvm::cl::desc("Add directory to include search path"));

static llvm::cl::list<string>
InputFilenames(llvm::cl::Positional, llvm::cl::desc("<input file>"),
    llvm::cl::OneOrMore);

//static llvm::cl::opt<llvm::sys::Path>
static llvm::cl::opt<string>
OutputFilename("o", llvm::cl::value_desc("outfile"),
    llvm::cl::desc("Name of output file"), llvm::cl::Required);

static llvm::cl::list<string>
FrameworkDummy("framework", llvm::cl::desc("compat dummy"));

static llvm::cl::list<std::string> IgnoredParams(llvm::cl::Sink);


void addIncludesAndDefines(PPContext& c) {/*{{{*/
  // Add header search directories (C only, no C++ or ObjC)

  InitHeaderSearch init(c.headers);

  // user headers
  for (int i = 0; i < I_dirs.size(); ++i) {
    cerr << "adding " << I_dirs[i] << endl;
    // add as system header
    init.AddPath(I_dirs[i], InitHeaderSearch::Angled, false, true, false);
    //addIncludePath(dirs, I_dirs[i], DirectoryLookup::SystemHeaderDir, c.fm);
  }
  init.AddDefaultSystemIncludePaths(c.opts);
  init.Realize();

  // Add defines passed in through parameters
  vector<char> predefineBuffer;
  for (int i = 0; i < D_macros.size(); ++i) {
    cerr << "defining " << D_macros[i] << endl;
    ::DefineBuiltinMacro(predefineBuffer, D_macros[i].c_str());
  }
  predefineBuffer.push_back('\0');
  c.pp.setPredefines(&predefineBuffer[0]);
}/*}}}*/

bool compile(ostream& out, const string& inFile)
{
  // Create Preprocessor object
  PPContext context;

  addIncludesAndDefines(context);


  // Add input file

  const FileEntry* File = context.fm.getFile(inFile);
  if (!File) {
    cerr << "Failed to open \'" << inFile << "\'" << endl;
    return false;
  }
  context.sm.createMainFileID(File, SourceLocation());

  if (!out) {
    cerr << "Failed to open \'" << OutputFilename << "\'" << endl;
    return false;
  }

  //// Parse it

  out << InputFilenames[0] << endl;

  ASTConsumer* c = new MyASTConsumer(out);
  ParseAST(context.pp, c);
  delete c;

  out << endl;

  unsigned NumDiagnostics = context.diags.getNumDiagnostics();
  
  if (NumDiagnostics)
    fprintf(stderr, "%d diagnostic%s generated.\n", NumDiagnostics,
            (NumDiagnostics == 1 ? "" : "s"));

  return true;
}

void parseNameColonNumber(istream& in, string& nameOut, int& numOut)
{
  string dummy; in >> dummy;
  string::size_type p = dummy.find(":");
  assert(p != string::npos);
  nameOut = dummy.substr(0, p);
  numOut = atoi(dummy.substr(p + 1).c_str());
}

struct Define {
  string definingTU;
  string definingFile;
  int definingLine;
  bool isStatic;
  string name;
  int numUses;
};

struct Use {
  string usingTU;
  string usingFunction;
  int usingLineNr;
  string name;
  Define* var;
  string line;
};

char mylower(char c) { return tolower(c); }

string to_upper(string s) {  // migth also use an ic_string (new char_traits)
  toupper(s[0]);
  transform(s.begin(), s.end(), s.begin(), mylower);
  return s;
}

bool operator<(const Use& a, const Use& b) {
  // sort by number of uses first
  if (a.var->numUses != b.var->numUses)
    return a.var->numUses > b.var->numUses;  // descending

  // sort by name and staticness
  if (to_upper(a.name) != to_upper(b.name))
    return to_upper(a.name) < to_upper(b.name);
  if (a.var->isStatic != b.var->isStatic)
    return a.var->isStatic < b.var->isStatic;

  // then by usage place
  if (a.usingTU != b.usingTU)
    return a.usingTU < b.usingTU;

  // line number usually implies function
  if (a.usingLineNr != b.usingLineNr)
    return a.usingLineNr < b.usingLineNr;
  return a.usingFunction < b.usingFunction;
}

bool link(ostream& out, const vector<string>& files)
{
  vector<Define> allDefines;
  vector<Use> allUses;
  for (int i = 0; i < files.size(); ++i) {
    ifstream in(files[i].c_str());
    if (!in) {
      cerr << "Failed to open \'" << files[i] << "\"\n";
      return false;
    }

    string inName, dummy;
    in >> inName;

    int numDefines; in >> numDefines >> dummy; in.get();
    for (int j = 0; j < numDefines; ++j) {
      Define d;
      d.definingTU = inName;
      parseNameColonNumber(in, d.definingFile, d.definingLine);
      in >> dummy;
      d.isStatic = dummy == "static";
      if (d.isStatic)
        in >> dummy;
      d.name = dummy;
      allDefines.push_back(d);
    }

    int numUses; in >> numUses >> dummy; in.get();
    for (int j = 0; j < numUses; ++j) {
      Use u;
      u.usingTU = inName;
      parseNameColonNumber(in, u.usingFunction, u.usingLineNr);
      in >> u.name; in.get();
      getline(in, u.line);
      allUses.push_back(u);
    }
  }

  map<pair<string, string>, Define*> defineMap;
  for (int i = 0; i < allDefines.size(); ++i) {
    Define& d = allDefines[i];
    pair<string, string> key("", d.name);
    if (d.isStatic)
      key.first = d.definingTU;
    if (defineMap.find(key) != defineMap.end()) {
      cerr << key.first << " " << key.second << " defined twice" << endl;
      exit(1);
    }
    d.numUses =  0;
    defineMap[key] = &d;
  }

  for (int i = 0; i < allUses.size(); ++i) {
    Use& u = allUses[i];
    Define* d = NULL;

    // look up static global
    if (defineMap.find(make_pair(u.usingTU, u.name)) != defineMap.end())
      d = defineMap[make_pair(u.usingTU, u.name)];

    // look up general global
    if (d == NULL && defineMap.find(make_pair("", u.name)) != defineMap.end()) {
      d = defineMap[make_pair("", u.name)];
    }

    if (d == NULL) {
      cerr << "Unresolved global \"" << u.name << "\"\n";
      exit(1);
    }
    u.var = d;
    d->numUses++;
  }

  int numStatics = 0;
  for (int i = 0; i < allDefines.size(); ++i) {
    if (allDefines[i].isStatic)
      ++numStatics;
  }

  out << "<p>" << allDefines.size() << " globals defined, " << numStatics
      << " of them are <code>static</code>.</p>" << endl;

  sort(allUses.begin(), allUses.end());
  Define* currVar = NULL;
  string currUseTU = "";
  for (int i = 0; i < allUses.size();) {
    int start = i;
    int tuCount = 0;
    ostringstream tmp;
    while (i < allUses.size() && allUses[i].var == allUses[start].var) {
      int tuStart = i;

      ostringstream tmq;
      while (i < allUses.size()
          && allUses[i].var == allUses[start].var
          && allUses[i].usingTU == allUses[tuStart].usingTU) {

        int funcStart = i;
        ostringstream tmr;
        while (i < allUses.size()
            && allUses[i].var == allUses[start].var
            && allUses[i].usingTU == allUses[tuStart].usingTU
            && allUses[i].usingFunction == allUses[funcStart].usingFunction) {
          Use& u = allUses[i];


          bool mvimLinks = true;
          char buff[2048];
          tmr << "  ";
          if (mvimLinks)
            tmr << "<a href=\"mvim://open?line=" << u.usingLineNr
                << "&url=file://" << getcwd(buff, 2048)
                << '/' << allUses[tuStart].usingTU << "\">";
          tmr << u.usingLineNr;
          if (mvimLinks)
            tmr << "</a>";
          tmr << ": " << html::EscapeText(u.line)
              << endl;
          ++i;
        }
        tmq << "  " << allUses[funcStart].usingFunction << "():\n" << tmr.str();
      }
      int uses = i - tuStart;
      tmp << "<div class=\"usefile\"><div class=\"file\">"
          << "<span class=\"filename\">"
          << allUses[tuStart].usingTU << "</span><span class=\"filecount\">"
          << " (" << uses << " use" << (uses!=1?"s":"") << ")"
          << "</span></div>" << endl;
      tmp << "<pre><code>";
      tmp << tmq.str();
      tmp << "</code></pre></div>";
      ++tuCount;
    }

    out << "<div class=\"global"
        << (allUses[start].var->isStatic?" static":"") << "\">";
    // XXX: num total uses

    int uses = i - start;
    Define* var = allUses[start].var;
    out << endl << "<div class=\"head\"><code class=\"name\">"
        << allUses[start].var->name << "</code><span class=\"totalcount\">"
        << " (" << uses << " use" << (uses!=1?"s":"")
        << " in " << tuCount
        << " translation unit" << (tuCount!=1?"s":"") << ")"
        << "</span>" << endl
        << "<span class=\"defineinfo\">"
        << "defined " << (var->isStatic?"static ":"")
        << "in translation unit " << var->definingTU
        << ", declared in "
        << var->definingFile << ":" << var->definingLine
        << "</span></div>"
        << endl
        << "<div class=\"uses\">";
    out << tmp.str();
    out << "</div></div>" << endl << endl;
  }
}

int main(int argc, char* argv[])
{
  llvm::cl::ParseCommandLineOptions(argc, argv, " globalcollect\n"
      "  This collects and prints global variables found in C programs.");
  llvm::sys::PrintStackTraceOnErrorSignal();

  llvm::sys::Path MainExecutablePath = 
     llvm::sys::Path::GetMainExecutable(argv[0], (void*)(intptr_t)main);
  assert (!MainExecutablePath.isEmpty() && "could not locate myself");
  MainExecutablePath.eraseComponent();  // remove executable name


  if (!IgnoredParams.empty()) {
    cerr << "Ignoring the following parameters:";
    copy(IgnoredParams.begin(), IgnoredParams.end(),
        ostream_iterator<string>(cerr, " "));
    cerr << endl;
  }

  enum { COMPILE, LINK } mode = COMPILE;

  llvm::sys::Path OutputPath(OutputFilename);
  if (InputFilenames.size() > 1 || OutputPath.getSuffix() != "o")
    mode = LINK;

  if (OutputPath.getSuffix() !=  "o" && mode == COMPILE) {
    cerr << "Need to compile to a .o file" << endl;
    return 1;
  }

  if (mode == COMPILE) {
    ofstream out(OutputFilename.c_str());
    compile(out, InputFilenames[0]);
    out.close();
  } else {
    ofstream out(OutputFilename.c_str());

    llvm::sys::Path frontName = MainExecutablePath;
    frontName.appendComponent("html_front.txt");
    ifstream frontFile(frontName.toString().c_str());
    out << frontFile.rdbuf();


    link(out, InputFilenames);

    llvm::sys::Path backName = MainExecutablePath;
    backName.appendComponent("html_back.txt");
    ifstream backFile(backName.toString().c_str());
    out << backFile.rdbuf();

    out.close();
  }
}
