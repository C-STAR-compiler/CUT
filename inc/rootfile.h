#ifndef ROOTS_H
#define ROOTS_H

#include <diagnostic.h>
#include <oop.h>
#include <filestream.h>
#include <set.h>

#define PUBLIC
#define TYPENAME RootFile

OBJECT (const char *filename, AccessModes mode) INHERIT (Set)
  const char  *filename;
  AccessModes  mode;
END_OBJECT("", ACCESS_READ);

#undef TYPENAME
#undef PUBLIC
#endif