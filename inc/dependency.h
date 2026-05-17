#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include <diagnostic.h>
#include <oop.h>
#include <str.h>
#include <graph.h>

#define PUBLIC
#define TYPENAME Dependency

OBJECT (const char *path, const Matrix *adjacency, const Set *labels) INHERIT (String)
  const Matrix *adjacency;
  const Set    *labels;

  int start;
END_OBJECT("", NULL, NULL);

int _(Comparer)(const String *other)  VIRTUAL (Comparer);
int _(KeyComparer)(const char *other) VIRTUAL (KeyComparer);

#undef TYPENAME
#undef PUBLIC

#endif