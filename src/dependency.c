#include <dependency.h>

#define TYPENAME Dependency

Dependency *_(Construct)(const char *path, const Matrix *adjacency, const Set *labels)
{
  if (String_Construct(BASE(0), path)) {
    this->adjacency = adjacency;
    this->labels    = labels;
    this->start     = labels && Set_ContainsKey(labels, path);
  }

  return this;
}

void _(Destruct)()
{
  String_Destruct(BASE(0));
}

int _(Comparer)(const String *other)
{
  return Dependency_KeyComparer(this, other->base);
}

int _(KeyComparer)(const char *other)
{
  if (this->labels && this->adjacency) {
    int dest = Set_ContainsKey(this->labels, other);

    if (this->start == dest) {
      return 0;
    } else if (this->adjacency->base[this->start][dest] > 0){
      return 1;
    } else {
      return -1;
    }
  }

  return 0;
}


#undef TYPENAME