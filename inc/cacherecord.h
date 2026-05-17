#ifndef CACHERECORD_H
#define CACHERECORD_H

// CUT
#include <diagnostic.h>
#include <oop.h>
#include <str.h>

#define PUBLIC
#define TYPENAME CacheRecord

OBJECT (String *value, long timestamp) NOBASE
  String *value;
  long    timestamp;
END_OBJECT(NULL, 0);

long statfile(const char *filename);

#undef TYPENAME
#undef PUBLIC

#endif