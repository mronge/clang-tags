/*
 * TestObserver.cpp
 *
 *  Created on: Feb 11, 2011
 */

#include "TestObserver.h"

#include <iostream>

void TestObserver::fileModified(const char *file)
{
	std::cout << "File " << file << " was modified" << std::endl;
}
