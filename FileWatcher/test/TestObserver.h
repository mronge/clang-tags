/*
 * TestObserver.h
 *
 *  Created on: Feb 11, 2011
 */

#ifndef TESTOBSERVER_H_
#define TESTOBSERVER_H_

#include "../FileWatcherObserver.h"

class TestObserver: public FileWatcherObserver
{
public:
	virtual void fileModified(const char *file);
};

#endif /* TESTOBSERVER_H_ */
