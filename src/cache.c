#include <diagnostic.h>
#include <directory.h>
#include <exception.h>
#include <cachefile.h>
#include <graph.h>
#include <str.h>
#include <string.path.h>
#include <args.h>
#include <rootfile.h>
#include <graphfile.h>
#include <mapset.h>

/*
The goal of this program is to build cache data that is effective enough
while remaining simple enough to be compatible with the bootstrap
*/

typedef struct {
  const char *home;
  int         source;
} Env;

void option_source(Args *args, ArgValue value)
{
  ((Env*)args->env)->source = value.as_integer;
}

OPTIONS(
  { "source", 's', "Include sources in search for dependencies", ARG_TYPE_BOOLEAN, option_source }
);

void build_cache(Env *env, CacheFile *cache, CacheFile *fileCache, CacheFile *pkgCache)
{
  // TODO: (low): Standardize: standardize DirectoryIterator with Iterator (beware of impact on dep tree)
  for (DirectoryIterator *di = dopen(env->home); !ddone(di); dnext(di)) {
    if (di->current.type == DIRTYPE_DIRECTORY && di->current.name[0] != '.') {
      char      pkgpath[PATH_MAX];
      String   *rootpath;

      dname(di, sizeof(pkgpath), pkgpath);

      rootpath = Path_Combine(pkgpath, ".cut/roots");

      if (fexists(rootpath->base, FILE_EXISTS)) {
        // We're in a CUT project
        RootFile *roots = NEW (RootFile) (rootpath->base, ACCESS_READ);

        CacheFile_Set(pkgCache, NEW (String) (di->current.name), NEW (String) (pkgpath), 0);

        for (Iterator *it = NEW (Iterator) (roots); !done(it); next(it)) {
          String *root = it->base;

          for (int i = 0; i <= env->source; i++)
          {
            String *folder = String_Cat(Path_Combine(pkgpath, root->base), i == 0 ? "/inc" : "/src");

            for (DirectoryIterator *dj = dopen(folder->base); !ddone(dj); dnext(dj)) {
              if (dj->current.type == DIRTYPE_FILE) {
                char filepath[PATH_MAX];

                dname(dj, sizeof(filepath), filepath);

                long timestamp = statfile(filepath);

                CacheFile_Set(fileCache, NEW (String) (dj->current.name), NEW (String) (filepath),         timestamp);
                CacheFile_Set(cache,     NEW (String) (dj->current.name), NEW (String) (di->current.name), timestamp);
              }
            }

            DELETE (folder);
          }
        }

        DELETE (roots);
      }

      DELETE (rootpath);
    }
  }
}

// TODO: It's potentially useless to build the file graph if we don't generate the depends file
void build_graph(Env *env, CacheFile *cache, CacheFile *fileCache, CacheFile *pkgCache,  Graph *fileDeps, Graph *pkgDeps)
{
  for (Iterator *it = NEW (Iterator) (fileCache); !done(it); next(it))
  {
    String *file = ((Pair*)it->base)->first;
    String *path = ((CacheRecord*)((Pair*)it->base)->second)->value;

    String *srcPkg  = CacheFile_Get(pkgCache, CacheFile_Get(cache, file)->value)->value;
    String *srcPath = Path_Folder(path->base);

    CharStream  *stream = (CharStream*)FileStream_Open(path->base, ACCESS_READ);

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
              String *dstPkg  = CacheFile_Get(pkgCache, include->value)->value;
              String *dstPath = Path_Folder(CacheFile_Get(fileCache, line)->value->base);

              if (srcPkg && dstPkg) {
                // Package graph
                Graph_AddKey(pkgDeps, srcPkg->base);
                Graph_AddKey(pkgDeps, dstPkg->base);

                Graph_SetKey(pkgDeps, srcPkg->base, dstPkg->base, 1);
              }

              if (srcPath && dstPath) {
                // Files graph
                Graph_AddKey(fileDeps, srcPath->base);
                Graph_AddKey(fileDeps, dstPath->base);

                Graph_SetKey(fileDeps, srcPath->base, dstPath->base, 1);
              }

              DELETE (dstPath)
            }
          }
        }
      }

      DELETE (line);
    }

    DELETE (srcPath);
    DELETE (stream);
  }
}

int main(int argc, char *argv[])
{
  Env   env  = { "", 0 };
  Args *args = NEW (Args) (argc, argv, &env);

  CHECK_MEMORY

  String *cachePath;
  String *fileCachePath;
  String *pkgCachePath;
  String *fileGraphPath;
  String *pkgGraphPath;

  env.home = getenv("CUT_HOME");

  if (!env.home || !env.home[0]) {
    THROW(NEW (Exception)("No CUT_HOME environment variable defined... exiting!"));
  } else {
    // TODO: (low): Refactor: Maybe ".cut" should be at the root of HOME and not CUT
    cachePath     = Path_Combine(env.home, "CUT/.cut/.cache");
    fileCachePath = Path_Combine(env.home, "CUT/.cut/files.cache");
    pkgCachePath  = Path_Combine(env.home, "CUT/.cut/packages.cache");
    fileGraphPath = Path_Combine(env.home, "CUT/.cut/files.json");
    pkgGraphPath  = Path_Combine(env.home, "CUT/.cut/packages.json");
  }

  CacheFile *cache     = NEW (CacheFile)(cachePath->base,     ACCESS_WRITE);
  CacheFile *fileCache = NEW (CacheFile)(fileCachePath->base, ACCESS_WRITE);
  CacheFile *pkgCache  = NEW (CacheFile)(pkgCachePath->base,  ACCESS_WRITE);
  GraphFile *fileDeps  = NEW (GraphFile)(fileGraphPath->base, ACCESS_WRITE);
  GraphFile *pkgDeps   = NEW (GraphFile)(pkgGraphPath->base,  ACCESS_WRITE);

  CHECK_MEMORY

  build_cache(&env, cache, fileCache, pkgCache);
  build_graph(&env, cache, fileCache, pkgCache, (Graph*)fileDeps, (Graph*)pkgDeps);

  CHECK_MEMORY

  DELETE (pkgDeps)
  DELETE (fileDeps);
  DELETE (pkgCache);
  DELETE (fileCache);
  DELETE (cache);
  DELETE (pkgGraphPath);
  DELETE (fileGraphPath);
  DELETE (pkgCachePath);
  DELETE (fileCachePath);
  DELETE (cachePath);
  DELETE (args);

  CHECK_MEMORY
  STOP_WATCHING
}