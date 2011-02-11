/*
 * MonitoredFile.h
 *
 *  Created on: Feb 11, 2011
 */

#ifndef MONITOREDFILE_H_
#define MONITOREDFILE_H_

#include <time.h> // for time_t

class MonitoredFile {
public:
	MonitoredFile(const char *name);
	virtual ~MonitoredFile();

	bool changed();
	const char* getFileName();
private:
	void updateStats();

	const char *m_fileName;
	time_t m_mtime;
	time_t m_oldMtime;
};

#endif /* MONITOREDFILE_H_ */
