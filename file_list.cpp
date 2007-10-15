#include "file_list.h"

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

using namespace std;

static bool file_exists(const string file_name)
{
#ifdef _WIN32
  struct _stat buf;
  if (_stat( file_name.c_str(), &buf) == 0) {
#else
  struct stat buf;
  if (stat(file_name.c_str(), &buf) == 0) {
#endif
    return true;
  } else {
    return false;
  }
}

// Make a new file name by filling in the specified number
// sequence that begins at "start" and ends at "start+len" in
// the file name that was passed in.
static string make_filename(const string file_name, string::size_type start, string::size_type len, unsigned long value)
{
  // We're going to write the number into a character array using
  // sprintf(), but first we have to construct an argument to sprintf()
  // that tells how many spaces to fill in and asks it to zero-pad
  // the number on the left.  This format string must be  %0XXu  where
  // XX is replaced by the number of characters to take up.  We have
  // to prefix the % with another % to stick it into the string, then
  // add the  %02u  that is used to fill in the format descriptor, then
  // add the  u  that is written into the format descriptor.
  char	format_descriptor[6];
  char	num_as_chars[100];
  sprintf(format_descriptor, "%%0%02ulu", len);
  sprintf(num_as_chars, format_descriptor, value);

  string retval = file_name;
  retval.replace(start, len, num_as_chars);

  return retval;
}

bool  file_list(const string file_name, vector <string> &file_list)
{
  int stride = 0; // How many steps between file names?
  int i;
  bool found;

  // Empty the list in case we find an error below.
  file_list.clear();

  // If the file exists, add its name to the list.  Otherwise,
  // we're done!
  if (file_exists(file_name)) {
    file_list.push_back(file_name);
  } else {
    return false;
  }

  // Find the start and end location of the last block of
  // changing numbers in the file name.  If there is not a block of
  // numbers, then return with just the one sent in.
  string::size_type first_number = file_name.size();  // Start with it at the end of the string, one past the end.
  string::size_type last_number;
  unsigned num_length;
  unsigned long num_value;
  do {

    // Locate end of the next-closest-to-the-end block of numbers.
    // If there isn't one, then jump out of the do loop, leaving the
    // stride at zero.
    last_number = file_name.find_last_of("0123456789", first_number-1);
    if (last_number == string::npos) {
      break;
    }

    // Locate the first number in this block of digits.
    first_number = file_name.find_last_not_of("0123456789",last_number);
    if (first_number == string::npos) {
      first_number = 0; // All the way back to the beginning of the string
    } else {
      first_number++; // Bump back up one to the first character in the number
    }

    // Compute the length and the numeric value of the block of digits.
    num_length = last_number - first_number + 1;
    num_value = atoi(&file_name.c_str()[first_number]);

    // Start looking for files whose name is the same as the one we
    // have but the number in them is smaller.  We're trying to find
    // the stride to use between file names.  Look for differences of
    // up to 1000 before giving up.  Look both higher and lower than
    // the number in the file name we were given.
    stride = 0;
    for (i = 1; i <= 1000; i++) {
      string newname = make_filename(file_name, first_number, num_length, num_value+i);
      if (file_exists(newname)) {
	stride = i;
	break;
      }
      newname = make_filename(file_name, first_number, num_length, num_value-i);
      if (file_exists(newname)) {
	stride = i;
	break;
      }
    }
  } while (stride == 0);

  // If we didn't find any non-zero stride, we're done!
  if (stride == 0) {
    return true;
  }

  // Insert lower-numbered files as long as we find more.
  i = 0;
  do {
    i -= stride;
    string newname = make_filename(file_name, first_number, num_length, num_value+i);
    if (file_exists(newname)) {
      found = true;
      file_list.push_back(newname);
    } else {
      found = false;
    }
  } while (found);

  // Insert higher-numbered files as long as we find more.
  i = 0;
  do {
    i += stride;
    string newname = make_filename(file_name, first_number, num_length, num_value+i);
    if (file_exists(newname)) {
      found = true;
      file_list.push_back(newname);
    } else {
      found = false;
    }
  } while (found);

  // Sort the list before returning it.

  std::sort(file_list.begin(), file_list.end());
  return true;
}
