/*
 * FileWatcherObserver.h
 *
 *  Created on: Feb 11, 2011
 */

#ifndef FILEWATCHEROBSERVER_H_
#define FILEWATCHEROBSERVER_H_

class FileWatcherObserver {
public:
    virtual void fileModified(const char *file) = 0;
};

#endif /* FILEWATCHEROBSERVER_H_ */
