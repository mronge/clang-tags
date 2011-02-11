/*
 * FileWatcher.cpp
 *
 *  Created on: Feb 11, 2011
 */

#include "FileWatcher.h"

#include <cstring>

FileWatcher::FileWatcher(FileWatcherObserver *observer)
{
	m_observer = observer;
}

FileWatcher::~FileWatcher()
{
	delete m_observer;

	for (std::list<MonitoredFile*>::iterator it = m_files.begin(); it != m_files.end(); it++)
	{
		delete *it;
	}
}


void FileWatcher::addFile(const char *filename)
{
	// Make sure it doesn't exist already
	for (std::list<MonitoredFile*>::iterator it = m_files.begin(); it != m_files.end(); it++)
	{
		if (std::strcmp((*it)->getFileName(), filename) == 0)
		{
			return;
		}
	}

	MonitoredFile* f = new MonitoredFile(filename);
	m_files.push_back(f);
}
void FileWatcher::removeFile(const char *filename)
{
	for (std::list<MonitoredFile*>::iterator it = m_files.begin(); it != m_files.end(); it++)
	{
		if (std::strcmp((*it)->getFileName(), filename) == 0)
		{
			m_files.erase(it);
			return;
		}
	}
}

void FileWatcher::check()
{
	for (std::list<MonitoredFile*>::iterator it = m_files.begin(); it != m_files.end(); it++)
	{
		if ((*it)->changed())
		{
			m_observer->fileModified((*it)->getFileName());
		}
	}
}
