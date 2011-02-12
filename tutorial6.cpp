// This code is licensed under the New BSD license.
// See LICENSE.txt for more details.
#include <iostream>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Host.h"

#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "clang/Basic/LangOptions.h"
#include "clang/Basic/FileSystemOptions.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/HeaderSearch.h"
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

//std-c lib
#include <list>

class ETagsWriter
{
public:
    ETagsWriter() : m_FD(-1) { }
    virtual ~ETagsWriter()
        {
            if (m_FD != -1)
            {
                close(m_FD);
                m_FD = -1;
            }
        }

    virtual int openFile(const char* filename)
        {
            m_FD = open(filename, O_CREAT | O_TRUNC | O_RDWR, 644);
            return m_FD;
        }

    virtual void closeFile()
        {
            close(m_FD);
            m_FD = -1;
        }

    virtual void startSection(const char* sourceName)
        {
            char c[2] = { 0x0c, 0x0a };
            int result = doWrite(m_FD, c, 2);
            result = doWrite(m_FD, sourceName, strlen(sourceName));
            result = doWrite(m_FD, ",", 1);
        }

    virtual void closeSection()
        {
            int totalSize = 0;
            char buf[32];
            for (std::list<char*>::iterator it = m_tagDefinitions.begin(); it != m_tagDefinitions.end(); it++)
            {
                // std::cout << "Symbol: '" << (*it) << "' is " << std::strlen(*it) << " bytes long" << std::endl;
                totalSize += std::strlen(*it);
            }
            // Now that I have the total size, write it out to the head
            sprintf(buf, "%d\n", totalSize);
            // std::cout << "Total size is " << totalSize << std::endl;
            int result = doWrite(m_FD, buf, std::strlen(buf));
            for (std::list<char*>::iterator it = m_tagDefinitions.begin(); it != m_tagDefinitions.end(); it++)
            {
                result = doWrite(m_FD, (*it), std::strlen(*it));
		// FIXME: Crasher
                // free(*it);
            }
        }

    virtual void addTag(const char* tagDefinition, const char* tagName, unsigned int lineNumber, unsigned int byteOffset)
        {
            char* buf = (char* )malloc(2048);
            sprintf(buf, "%s%c%s%c%d,%d\n", tagDefinition, 0x7f, tagName, 0x01, lineNumber, byteOffset);
            // std::cout << "Creating tag: " << buf << std::endl;
            m_tagDefinitions.push_back(buf);
        }

protected:
    int doWrite(int FD, const void* buf, int totalBytes)
        {
            int result = write(FD, buf, totalBytes);
            if (result <= 0)
            {
                std::cout << "Error " << result << " writing to file; ETAGS file probably won't be readable" << std::endl;
            }
            return result;
        }

private:
    mutable int m_FD;
    std::list<char *> m_tagDefinitions;
};

class MyASTConsumer : public clang::ASTConsumer
{
public:
    clang::SourceManager *_sourceManager;
    ETagsWriter *_writer;
    char lastBufferName[2048];

    MyASTConsumer(clang::SourceManager *sourceManager, ETagsWriter *writer) 
        : clang::ASTConsumer(), 
          _sourceManager(sourceManager),
          _writer(writer)
    { 
        lastBufferName[0] = '\0';
    }
    virtual ~MyASTConsumer() { }

    virtual void HandleTopLevelDecl( clang::DeclGroupRef d)
    {
        clang::DeclGroupRef::iterator it;
        for( it = d.begin(); it != d.end(); it++)
        {
            char tagDef[1024];
            char *bufferName = "";
            char name[1024];
            unsigned lineNumber = 0;
            unsigned fileOffset = 0;
            int found = 0;

            clang::ObjCInterfaceDecl *vdc = dyn_cast<clang::ObjCInterfaceDecl>(*it);
            if (vdc)
            {
                found = 1;
                bufferName = (char *)_sourceManager->getBufferName(vdc->getClassLoc());
                strcpy(name, vdc->getNameAsString().c_str());
                lineNumber = _sourceManager->getInstantiationLineNumber(vdc->getClassLoc());
                fileOffset = _sourceManager->getFileOffset(vdc->getClassLoc());
                sprintf(tagDef, "@interface %s", name);
            }

            // clang::ObjCProtocolDecl *vdp = dyn_cast<clang::ObjCProtocolDecl>(*it);
            // if (vdp)
            // {
            //     found = 1;
            //     bufferName = (char *)_sourceManager->getBufferName(vdp->getLocStart());
            //     std::cout << "Classname: "
            //               << vdp->getNameAsString() 
            //               << " LineNum: " 
            //               << _sourceManager->getInstantiationLineNumber(vdp->getLocStart()) 
            //               << " Column: "
            //               << _sourceManager->getInstantiationColumnNumber(vdp->getLocStart()) 
            //               << " Filename: " 
            //               <<  _sourceManager->getBufferName(vdp->getLocStart()) 
            //               << " File offset " 
            //               << _sourceManager->getFileOffset(vdp->getLocStart()) 
            //               << std::endl;              
            // }

            if (std::strcmp(lastBufferName, bufferName) != 0 && found) {
                // was there a previous buffer?
                if (std::strlen(lastBufferName) > 0) {
                    // std::cout << "Close section " << std::endl;
                    _writer->closeSection();
                }
                _writer->startSection(bufferName);
                // std::cout << "New section --------------------------------------------------" <<std::endl;
                std::strcpy(lastBufferName, bufferName);
            }

            if (found) {
                _writer->addTag(tagDef, name, lineNumber, fileOffset);

                // std::cout << "Name: "
                //           << name
                //           << " LineNum: " 
                //           << lineNumber
                //           << " Filename: " 
                //           <<  bufferName
                //           << " File offset " 
                //           << fileOffset
                //           << " Tag Definition "
                //           << tagDef
                //           << std::endl;              
            }
        }
    }
};

int main()
{
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
	clang::FileSystemOptions fileSystemOptions;
	clang::FileManager fileManager(fileSystemOptions);

	clang::SourceManager sourceManager(
        diagnostic,
        fileManager);
	clang::HeaderSearch headerSearch(fileManager);

	clang::HeaderSearchOptions headerSearchOptions;
	// <Warning!!> -- Platform Specific Code lives here
	// This depends on A) that you're running linux and
	// B) that you have the same GCC LIBs installed that
	// I do. 
	// Search through Clang itself for something like this,
	// go on, you won't find it. The reason why is Clang
	// has its own versions of std* which are installed under 
	// /usr/local/lib/clang/<version>/include/
	// See somewhere around Driver.cpp:77 to see Clang adding
	// its version of the headers to its include path.
	headerSearchOptions.AddPath("/usr/include/linux",
			clang::frontend::Angled,
			false,
			false,
			false);




	headerSearchOptions.AddPath("/usr/include/c++/4.4/tr1",
			clang::frontend::Angled,
			false,
			false,
			false);
	headerSearchOptions.AddPath("/usr/include/c++/4.4",
			clang::frontend::Angled,
			false,
			false,
			false);
	// </Warning!!> -- End of Platform Specific Code

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
		
	const clang::FileEntry *pFile = fileManager.getFile("test.m");
	sourceManager.createMainFileID(pFile);
	//preprocessor.EnterMainSourceFile();

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
   // clang::ASTConsumer astConsumer;

    ETagsWriter writer;
    writer.openFile("TAGS");
    MyASTConsumer astConsumer(&sourceManager, &writer);

    clang::Sema sema(
        preprocessor,
        astContext,
        astConsumer);
    sema.Initialize();

   //MySemanticAnalisys mySema( preprocessor, astContext, astConsumer);

    //clang::Parser parser( preprocessor, sema);
    //parser.ParseTranslationUnit();
    pTextDiagnosticPrinter->BeginSourceFile(languageOptions, &preprocessor);
    clang::ParseAST(preprocessor, &astConsumer, astContext); 
    pTextDiagnosticPrinter->EndSourceFile();

    writer.closeSection();
    writer.closeFile();
	return 0;
}
