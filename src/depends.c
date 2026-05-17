#include <diagnostic.h>
#include <directory.h>
#include <oop.h>
#include <exception.h>
#include <filestream.h>
#include <charstream.h>
#include <set.h>
#include <collection.str.h>
#include <string.h>
#include <list.h>
#include <cachefile.h>
#include <args.h>
#include <dependsfile.h>
#include <string.path.h>
#include <rootfile.h>
#include <graphfile.h>

typedef struct {
  const char *home;
  const char *path;
  const char *name;

  int         source;
  int         update;

  Array *updated;
  
} Env;

void option_source(Args *args, ArgValue value)
{
  ((Env*)args->env)->source = value.as_integer;
}

void option_update(Args *args, ArgValue value)
{
  ((Env*)args->env)->update = value.as_integer;
}

void param_workdir(Args *args, ArgValue value)
{
  Env *env = args->env;

  // TODO: add system("pwd") to IO library for windows
  const char *path =  strcmp(value.as_charptr, ".") ? value.as_charptr : "/home/phil/Program/Utilities/CUT"; // TODO: Change!!
  
  int pathlen = strlen(path);
  int namelen = fnamext(path, 0, NULL);

  env->path = path;
  env->name = env->path + (pathlen - namelen);
}

OPTIONS(
  { "source",            's', "Finds dependencies in sources instead of headers",    ARG_TYPE_BOOLEAN, option_source },
  { "update",            'u', "Only rebuild the outdated parts of the depends file", ARG_TYPE_BOOLEAN, option_update },
  { "working-directory", '+', "Specifies the working directory",                     ARG_TYPE_CHARPTR, param_workdir }
);

void update_cache(Env *env, CacheFile *cache, CacheFile *packages, CacheFile *files)
{
  String   *rootpath = Path_Combine(env->path, ".cut/roots");
  RootFile *roots    = NEW (RootFile) (rootpath->base, ACCESS_READ);

  if (!CacheFile_GetKey(packages, env->name))
  {
    CacheFile_Set(packages, NEW (String) (env->name), NEW (String) (env->path), 0);
  }

  for (Iterator *it = NEW (Iterator)(roots); !done(it); next(it)) {
    String *root = it->base;

    for (int i = 0; i <= env->source; i++)
    {
      String *folder = String_Cat(Path_Combine(env->path, root->base), i == 0 ? "/inc" : "/src");

      for (DirectoryIterator *di = dopen(folder->base); !ddone(di); dnext(di)) {
        if (di->current.type == DIRTYPE_FILE) {
          char filepath[PATH_MAX];

          dname(di, sizeof(filepath), filepath);

          long         timestamp = statfile(filepath);
          CacheRecord *record    = CacheFile_GetKey(files, di->current.name);

          if (!record || timestamp > record->timestamp)
          {
            String *file = NEW (String) (di->current.name);

            record = CacheFile_Set(files, file, NEW (String) (filepath),  timestamp);
            
            CacheFile_Set(cache, String_Copy(file), NEW (String) (env->name), timestamp);

            Array_Push(env->updated, &file);
          }
        }
      }

      DELETE (folder);
    }
  }

  DELETE (roots);
  DELETE (rootpath);
}

void update_graph(Env *env, CacheFile *cache, CacheFile *fileCache, Graph *files, Graph *packages)
{
  for (Iterator *it = NEW (Iterator) (env->updated); !done(it); next(it)) {
    String *file    = *(String**)it->base;
    String *srcPkg  = CacheFile_Get(cache, file)->value;
    String *srcPath = Path_Folder(CacheFile_Get(fileCache, file)->value->base);

    CharStream *stream = (CharStream*)FileStream_Open(srcPath->base, ACCESS_READ);

    while (!stream->base.eos) {
      String *line = CharStream_GetLine(stream);

      if (line) {
        if (String_StartsWith(line, "#include")) {
          String_SubString(line, 8, 0);
          String_Trim(line);

          if (String_StartsWith(line, "<")) {
            int end = line->length - String_Cnt(line, ">");

            String_SubString(line, 1, -end);

            CacheRecord *include = CacheFile_Get(cache, line);

            if (include) {
              String *dstPkg  = include->value;
              String *dstPath = Path_Folder(CacheFile_Get(fileCache, line)->value->base);

              if (srcPkg && dstPkg) {
                // Package graph
                Graph_AddKey(packages, srcPkg->base);
                Graph_AddKey(packages, dstPkg->base);

                Graph_SetKey(packages, srcPkg->base, dstPkg->base, 1);
              }

              if (srcPath && dstPath) {
                // Files graph
                Graph_AddKey(files, srcPath->base);
                Graph_AddKey(files, dstPath->base);

                Graph_SetKey(files, srcPath->base, dstPath->base, 1);
              }

              DELETE (dstPath)
            }
          }
        }
      }

      DELETE (line);
    }

    DELETE (stream);
    DELETE (srcPath);
  }
  
  DELETE (env->updated)
}

Matrix *get_matrix(Graph *dependencies)
{
  return Matrix_Pow(Graph_AdjacencyMatrix(dependencies), dependencies->base.rows);
}

void get_dependencies(const Matrix *reach, const Set *labels, const char *target, Set *deps)
{  
  int index = Set_ContainsKey(labels, target);

  if (index >= 0) {
    for (int i = 0; i < ((const Array*)labels)->size; i++) {
      // This package is a dependency
      if (reach->base[index][i] > 0) {
        const char *label = Array_AtDeref(((const Array*)labels), i);

        Set_Add(deps, NEW (Dependency) (label, reach, labels));
      }
    }
  }
}

void get_files(Env *env, Map *depends, CacheFile *cache, CacheFile *files, Graph *deps)
{  
  Matrix    *reach  = get_matrix(deps);
  const Set *labels = deps->labels;

  for (Iterator *it = NEW (Iterator) (cache); !done(it); next(it))
  {
    Pair   *pair    = it->base;
    String *package = ((CacheRecord*)pair->second)->value;

    if (String_Eq(package, env->name)) {
      // This is part of the package
      String *filename = pair->first;
      String *filepath = Path_Folder(CacheFile_Get(files, filename)->value->base);
      
      Set *includes = IFNULL(
        Map_ValueAt((Map*)depends, filename),
        Map_Set((Map*)depends,
                  String_Copy(filename),
                  NEW (Set) (TYPEOF (Dependency))
                )->second);

      get_dependencies(reach, labels, filepath->base, includes);

      DELETE(filepath);
    }
  }

  DELETE (reach);
}

int main(int argc, char *argv[])
{
  Env env = { getenv("CUT_HOME"), NULL, NULL, 0, 0, NEW (Array) (sizeof (CacheRecord*)) };

  CHECK_MEMORY

  Args       *args          = NEW (Args) (argc, argv, &env);
 // const char *workdir      = Args_Name(args, "working-directory").as_charptr;
  String     *dependsfile   = Path_Combine(env.path, ".cut/depends.map");
  String     *cachepath     = NULL;
  String     *filescpath    = NULL;
  String     *packagescpath = NULL;
  String     *filesgpath    = NULL;
  String     *packagesgpath = NULL;

  if (!env.home || !env.home[0]) {
    THROW (NEW (Exception) ("No CUT_HOME environment variable defined... exiting!"));
  } else {
    cachepath     = Path_Combine(env.home, "CUT/.cut/.cache");
    filescpath    = Path_Combine(env.home, "CUT/.cut/files.cache");
    packagescpath = Path_Combine(env.home, "CUT/.cut/packages.cache");
    filesgpath    = Path_Combine(env.home, "CUT/.cut/files.json");
    packagesgpath = Path_Combine(env.home, "CUT/.cut/packages.json");
  }

  // TODO: finish this

  CacheFile *cache         = NEW (CacheFile) (cachepath->base,     ACCESS_WRITE | ACCESS_READ);
  CacheFile *filescache    = NEW (CacheFile) (filescpath->base,    ACCESS_WRITE | ACCESS_READ);
  CacheFile *packagescache = NEW (CacheFile) (packagescpath->base, ACCESS_WRITE | ACCESS_READ);
  GraphFile *filesgraph    = NEW (GraphFile) (filesgpath->base,    ACCESS_WRITE | ACCESS_READ);
  GraphFile *packagesgraph = NEW (GraphFile) (packagesgpath->base, ACCESS_WRITE | ACCESS_READ);
  
  update_cache(&env, cache, packagescache, filescache);
  update_graph(&env, cache, filescache, (Graph*)filesgraph, (Graph*)packagesgraph);
  
  DependsFile *depends  = NEW (DependsFile) (dependsfile->base, ACCESS_WRITE);
  
  Set *packages = IFNULL(
      Map_ValueAtKey((Map*)depends, "packages"), 
      Map_Set((Map*)depends,
                NEW (String) ("packages"),
                NEW (Set) (TYPEOF (Dependency))
              )->second);

  Matrix    *reach  = get_matrix((Graph*)packagesgraph);
  const Set *labels = ((Graph*)packagesgraph)->labels;

  get_dependencies(reach, labels, env.path, packages);
  get_files(&env, (Map*)depends, cache, filescache, (Graph*)filesgraph);

  DELETE (reach);
  DELETE (depends);
  DELETE (packagesgraph);
  DELETE (filesgraph);
  DELETE (packagescache);
  DELETE (filescache);
  DELETE (cache);
  DELETE (packagesgpath);
  DELETE (filesgpath);
  DELETE (packagescpath);
  DELETE (filescpath);
  DELETE (cachepath);
  DELETE (dependsfile);
  DELETE (args);

  DELETE (env.updated);

  CHECK_MEMORY
  STOP_WATCHING
}