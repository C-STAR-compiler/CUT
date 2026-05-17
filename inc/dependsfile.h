#ifndef DEPENDSFILE_H
#define DEPENDSFILE_H

// CUT
#include <diagnostic.h>
#include <oop.h>
#include <map.h>
#include <dependency.h>
#include <filestream.h>
#include <charstream.h>
#include <set.h>

#define PUBLIC
#define TYPENAME DependsFile

OBJECT (const char *filename, AccessModes mode) INHERIT (Map)
  const char  *filename;
  AccessModes  mode;
END_OBJECT("", ACCESS_READ);

#undef TYPENAME
#undef PUBLIC
#endif