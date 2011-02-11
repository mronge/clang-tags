/*
 * Main.cpp
 *
 *  Created on: Feb 11, 2011
 */

#include "../FileWatcher.h"
#include "TestObserver.h"
#include <iostream>

#include <unistd.h> // for sleep()

int main()
{
	TestObserver* observer = new TestObserver();

	FileWatcher watcher(observer);

	watcher.addFile("junk.data");
	std::cout << "touch junk.data to see the changes" << std::endl;

	for (;;)
	{
		watcher.check();
		sleep(1);
	}

	return 0;
}
