#include <iostream>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Host.h"

#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "clang/Basic/LangOptions.h"
#include "clang/Basic/FileSystemOptions.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/DirectoryLookup.h"
#include "clang/Basic/FileManager.h"

#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Frontend/Utils.h"

#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/Frontend/PreprocessorOptions.h"
#include "clang/Frontend/FrontendOptions.h"

#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/Builtins.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Sema/Sema.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Ownership.h"
#include "clang/AST/DeclGroup.h"

#include "clang/Parse/Parser.h"

#include "clang/Parse/ParseAST.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

//std-c lib
#include <list>

class ETagsWriter {
public:
    ETagsWriter() : m_FD(-1) { }

    virtual ~ETagsWriter() {
        if (m_FD != -1) {
            close(m_FD);
            m_FD = -1;
        }
    }

    virtual int openFile(const char* filename) {
        m_FD = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 644);
        return m_FD;
    }

    virtual void closeFile() {
        close(m_FD);
        m_FD = -1;
    }

    virtual void startSection(const char* sname) {
        strcpy(sourceName, sname);
    }

    virtual void closeSection() {
        if (m_tagDefinitions.size() == 0) {
            return;
        }
        // Write the section start
        char c[2] = { 0x0c, 0x0a };
        int result = doWrite(m_FD, c, 2);
        result = doWrite(m_FD, sourceName, strlen(sourceName));
        result = doWrite(m_FD, ",", 1);

        int totalSize = 0;
        char buf[32];
        for (std::list<char*>::iterator it = m_tagDefinitions.begin(); it != m_tagDefinitions.end(); it++) {
            // std::cout << "Symbol: '" << (*it) << "' is " << std::strlen(*it) << " bytes long" << std::endl;
            totalSize += std::strlen(*it);
        }
        // Now that I have the total size, write it out to the head
        sprintf(buf, "%d\n", totalSize);
        // std::cout << "Total size is " << totalSize << std::endl;
        result = doWrite(m_FD, buf, std::strlen(buf));
        for (std::list<char*>::iterator it = m_tagDefinitions.begin(); it != m_tagDefinitions.end(); it++)   {
            result = doWrite(m_FD, (*it), std::strlen(*it));
            // FIXME: Crasher
            // free(*it);
        }
        m_tagDefinitions.clear();
    }

    virtual void addTag(const char* tagDefinition, const char* tagName, unsigned int lineNumber, unsigned int byteOffset) {
        char* buf = (char* )malloc(2048);
        sprintf(buf, "%s%c%s%c%d,%d\n", tagDefinition, 0x7f, tagName, 0x01, lineNumber, byteOffset);
        // std::cout << "Creating tag: " << buf << std::endl;
        m_tagDefinitions.push_back(buf);
    }

protected:
    int doWrite(int FD, const void* buf, int totalBytes) {
        int result = write(FD, buf, totalBytes);
        if (result <= 0) {
            std::cout << "Error " << result << " writing to file; ETAGS file probably won't be readable" << std::endl;
        }
        return result;
    }

private:
    char sourceName[1024];
    mutable int m_FD;
    std::list<char *> m_tagDefinitions;
};


class TagsASTConsumer : public clang::ASTConsumer {
public:
    TagsASTConsumer(clang::SourceManager *sourceManager, ETagsWriter *writer) 
        : clang::ASTConsumer(), 
          _sourceManager(sourceManager),
          _writer(writer) { 
        _lastBufferName[0] = '\0';
    }

    virtual ~TagsASTConsumer() { }

    virtual void HandleTopLevelDecl( clang::DeclGroupRef d) {
        clang::DeclGroupRef::iterator it;
        for( it = d.begin(); it != d.end(); it++) {
            char *bufferName;
            char name[1024];
            unsigned lineNumber = 0;
            unsigned fileOffset = 0;
            int found = 0;

            if (dyn_cast<clang::ObjCInterfaceDecl>(*it) || dyn_cast<clang::ObjCProtocolDecl>(*it)) {
                found = 1;
            }

            if (found) {
                clang::NamedDecl *decl = dyn_cast<clang::NamedDecl>(*it);
                bufferName = (char *)_sourceManager->getBufferName(decl->getLocation());
                strcpy(name, decl->getNameAsString().c_str());
                lineNumber = _sourceManager->getInstantiationLineNumber(decl->getLocation());
                fileOffset = _sourceManager->getFileOffset(decl->getLocation());
            }

            if (std::strcmp(_lastBufferName, bufferName) != 0 && found) {
                // was there a previous buffer?
                if (std::strlen(_lastBufferName) > 0) {
                    // std::cout << "Close section " << std::endl;
                    _writer->closeSection();
                }
                _writer->startSection(bufferName);
                // std::cout << "New section --------------------------------------------------" <<std::endl;
                std::strcpy(_lastBufferName, bufferName);
            }

            if (found) {
                _writer->addTag("", name, lineNumber, fileOffset);

                // std::cout << "Name: "
                //           << name
                //           << " LineNum: " 
                //           << lineNumber
                //           << " Filename: " 
                //           <<  bufferName
                //           << " File offset " 
                //           << fileOffset
                //           << " Tag Definition "
                //           << std::endl;              
            }
        }
    }
private:
    clang::SourceManager *_sourceManager;
    ETagsWriter *_writer;
    char _lastBufferName[1024];
};


void ParseFile(char *path, ETagsWriter *writer) {

    // I don't know which of these objects can be reused
    // when parsing a new file, so for now just tear
    // it all down and setup again. Maybe someone who knows
    // more about Clang can clean this up...

	clang::DiagnosticOptions diagnosticOptions;
	clang::TextDiagnosticPrinter *pTextDiagnosticPrinter =
		new clang::TextDiagnosticPrinter(
			llvm::outs(),
			diagnosticOptions);
	llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> pDiagIDs;
	clang::Diagnostic diagnostic(pDiagIDs, pTextDiagnosticPrinter);

	clang::LangOptions languageOptions;
    languageOptions.ObjC1 = 1;
    languageOptions.ObjC2 = 1;
    languageOptions.C99 = 1;
    languageOptions.CPlusPlus = 1;
	clang::FileSystemOptions fileSystemOptions;
	clang::FileManager fileManager(fileSystemOptions);

	clang::SourceManager sourceManager(
        diagnostic,
        fileManager);
	clang::HeaderSearch headerSearch(fileManager);

    // We don't want to include any system files
	clang::HeaderSearchOptions headerSearchOptions;
    headerSearchOptions.UseBuiltinIncludes = 0;
    headerSearchOptions.UseStandardIncludes = 0;
    headerSearchOptions.UseStandardCXXIncludes = 0;

	clang::TargetOptions targetOptions;
	targetOptions.Triple = llvm::sys::getHostTriple();

	clang::TargetInfo *pTargetInfo = 
		clang::TargetInfo::CreateTargetInfo(
			diagnostic,
			targetOptions);

	clang::ApplyHeaderSearchOptions(
		headerSearch,
		headerSearchOptions,
		languageOptions,
		pTargetInfo->getTriple());

	clang::Preprocessor preprocessor(
		diagnostic,
		languageOptions,
		*pTargetInfo,
		sourceManager,
		headerSearch);

	clang::PreprocessorOptions preprocessorOptions;
	clang::FrontendOptions frontendOptions;
	clang::InitializePreprocessor(
		preprocessor,
		preprocessorOptions,
		headerSearchOptions,
		frontendOptions);

    // We don't want Clang to load files via #import or #include
    // Otherwise we will visit the same files multiple times as they are imported
    // and we may visit files outside the directories we are crawling
    headerSearch.SetSearchPaths(std::vector<clang::DirectoryLookup>(), 0, 0, true);

    const clang::TargetInfo &targetInfo = *pTargetInfo;

    clang::IdentifierTable identifierTable(languageOptions);
    clang::SelectorTable selectorTable;

    clang::Builtin::Context builtinContext(targetInfo);
    clang::ASTContext astContext(
        languageOptions,
        sourceManager,
        targetInfo,
        identifierTable,
        selectorTable,
        builtinContext,
        0 /* size_reserve*/);

    TagsASTConsumer astConsumer(&sourceManager, writer);

    clang::Sema sema(
        preprocessor,
        astContext,
        astConsumer);
    sema.Initialize();

	const clang::FileEntry *pFile = fileManager.getFile(path);
	sourceManager.createMainFileID(pFile);
    pTextDiagnosticPrinter->BeginSourceFile(languageOptions, &preprocessor);
    clang::ParseAST(preprocessor, &astConsumer, astContext); 
    pTextDiagnosticPrinter->EndSourceFile();

    writer->closeSection();
}

bool CompareExtension(char *filename, char *extension) {

    if (filename == NULL || extension == NULL) {
        return false;
    }

    if (strlen(filename) == 0 || strlen(extension) == 0) {
        return false;
    }

    if (strchr(filename, '.') == NULL || strchr(extension, '.') == NULL) {
        return false;
    }

    /* Iterate backwards through respective strings and compare each char one at a time */
    for (int i = 0; i < strlen(filename); i++) {
        if (tolower(filename[strlen(filename) - i - 1]) == tolower(extension[strlen(extension) - i - 1])) {
            if (i == strlen(extension) - 1) {
                return true;
            }
        } else {
            break;
        }
    }

    return false;
}

void CrawlDirectory(char *dirpath, ETagsWriter *writer) {
    struct dirent **namelist;
    int n = scandir(dirpath, &namelist, 0, alphasort);
    if (n < 0) {
        printf("scandir failed on %s\n", dirpath);
        return;
    }
    while(n--) {
        char *name = namelist[n]->d_name;

        // Skip over entries . and ..
        if (std::strcmp(".", name) != 0 && std::strcmp("..", name) != 0) {
            char path[1024];
            sprintf(path, "%s/%s", dirpath, name);

            struct stat statbuf;
            if (stat(path, &statbuf)) {
                printf("stat failed on %s\n", path);
                free(namelist[n]);
                continue;
            }
            // Is it a directory?
            if (S_ISDIR(statbuf.st_mode)) {
                // If so, recurse!
                CrawlDirectory(path, writer);
            } else {
                // It is a file!
                if (CompareExtension(name, ".h") ||
                    CompareExtension(name, ".m") ||
                    CompareExtension(name, ".mm") ||
                    CompareExtension(name, ".cpp") ||
                    CompareExtension(name, ".c")) {
                 
                    ParseFile(path, writer);
                }
            }
        }
        free(namelist[n]);
    }
    free(namelist);
}

int main() {
    ETagsWriter writer;
    writer.openFile("TAGS");

    //ParseFile("test.m", &writer);
    CrawlDirectory(".", &writer);

    writer.closeFile();
	return 0;
}
