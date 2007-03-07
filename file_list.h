#ifndef	FILE_LIST_H
#define	FILE_LIST_H
//-------------------------------------------------------------------------
// This function is given a file name and it produces a list of file names
// that are in the same directory and that differ from the input file
// name only in a single block of numeric characters.  It is intended to
// be used to let a program find a sequence of image files:
//    my_image_001.tif, my_image_004.tif, my_image_009.tif, ...
// The numbers do not need to be in any particular sequence, but they
// must come in a continguous block that is the same number of characters
// in each of the file names.  The stride between file names must be
// constant, and it must be less than 100 to be detected by this
// class.  The changing block of numbers selected by this function is
// the one that is nearest to the end of the file name, if there is more
// than one set.
//
// It returns FALSE and clears the list if there is no such file as
// the name passed in.  It returns TRUE and fills in at least the
// original string if that file is found.  The returned list is
// sorted alphabetically (which corresponds to arithmetic sorting when
// the whole block of numbers is filled in).

#pragma warning( disable : 4786 )
#include <vector>
#include <string>

#include <algorithm> // for std::sort()

bool  file_list(const std::string file_name, std::vector <std::string> &file_list);

#endif
