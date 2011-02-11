/*
 * MonitoredFile.cpp
 *
 *  Created on: Feb 11, 2011
 */

#include "MonitoredFile.h"
#include <sys/stat.h>

MonitoredFile::MonitoredFile(const char *name)
{
	m_fileName = name;
	updateStats();
	m_oldMtime = m_mtime;
}

MonitoredFile::~MonitoredFile()
{
	delete m_fileName;
}

bool MonitoredFile::changed()
{
	updateStats();

	if (m_oldMtime != m_mtime)
	{
		m_oldMtime = m_mtime;
		return true;
	}
	return false;
}

const char* MonitoredFile::getFileName()
{
	return m_fileName;
}

void MonitoredFile::updateStats()
{
	struct stat st;
	if (stat(m_fileName, &st) == 0)
	{
		m_mtime = st.st_mtime;
	}
	else
	{
		m_mtime = 0;
	}
}
