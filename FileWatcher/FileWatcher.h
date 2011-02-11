/*
 * FileWatcher.h
 *
 *  Created on: Feb 11, 2011
 */

#ifndef FILEWATCHER_H_
#define FILEWATCHER_H_

#include "FileWatcherObserver.h"
#include "MonitoredFile.h"

#include <list>

class FileWatcher {
public:
    FileWatcher(FileWatcherObserver *observer);

    void addFile(const char *filename);
    void removeFile(const char *filename);

    void check();
	virtual ~FileWatcher();

private:
	std::list<MonitoredFile*> m_files;
	FileWatcherObserver* m_observer;
};

#endif /* FILEWATCHER_H_ */
