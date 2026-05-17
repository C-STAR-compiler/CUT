// C
#include <stdio.h>

// CUT
#include <args.h>
#include <objectarray.h>
#include <directory.h>
#include <filestream.h>
#include <diagnostic.h>
#include <string.path.h>

#define MAX_ROOTS 8

typedef struct _global {
  const char *directory;
  int         library;
  const char *roots[MAX_ROOTS];
  const char *args;
  const char *home;
} Global;

void option_dir(Args *args, ArgValue value)
{
  Global *env = args->env;
  env->directory = value.as_charptr ? value.as_charptr : ".";
}

void option_new(Args *args, ArgValue value)
{
  option_dir(args, value);
}

void option_lib(Args *args, ArgValue value)
{
  Global *env  = args->env;
  env->library = value.as_integer;
}

void option_roots(Args *args, ArgValue value)
{
  Global *env = args->env;
  
  int   list_size = 0;
  char *list      = (char*)value.as_charptr;

  if (list) {
    env->roots[list_size++] = list;

    for (int i = 0; list[i]; i++) {
      if (list[i] == ',') {
        list[i] = 0;
        env->roots[list_size++] = &list[i + 1];
      }
    }
  }
}

void option_build(Args *args, ArgValue value)
{
  option_dir(args, value);
}

void option_depends(Args *args, ArgValue value)
{
  option_dir(args, value);
}

void option_remove(Args *args, ArgValue value)
{
  option_dir(args, value);
}

void option_args(Args *args, ArgValue value)
{
  Global *env = args->env;
  env->args = value.as_charptr;
}

void option_todo(Args *args, ArgValue value)
{
  exit(0);
}

void option_test(Args *args, ArgValue value)
{
  //exit(0);
}

void param_command(Args *args, ArgValue value)
{
  // Make all subsequent elements parameters (and not options)
  args->param_mode = 1;
}

OPTIONS(
  { "path",      ' ', "Sets the working directory, default to current directory", ARG_TYPE_CHARPTR, option_dir     },
  { "init",      ' ', "Initializes a new CUT directory (program by default)",     ARG_TYPE_CHARPTR, option_new     },
  { "lib",       ' ', "Sets the CUT directory as a library",                      ARG_TYPE_BOOLEAN, option_lib     },
  { "roots",     ' ', "Comma separated list of project roots",                    ARG_TYPE_CHARPTR, option_roots   },
  { "build",     ' ', "Builds the CUT directory, defaults to current directory",  ARG_TYPE_CHARPTR, option_build   },
  { "depends",   ' ', "Rebuilds the dependency graph",                            ARG_TYPE_CHARPTR, option_depends },
  { "remove",    ' ', "Removes CUT tracking for this directory",                  ARG_TYPE_CHARPTR, option_remove  },
  { "command",   '+', "The cut command to execute",                               ARG_TYPE_CHARPTR, param_command  },
  { "arguments", '*', "The arguments passed to the cut command",                  ARG_TYPE_CHARPTR, NULL           }
);

void print_help(const ObjectArray *knownCommands) {
  printf("Known commands are:\n");

  for (int i = 0; i < knownCommands->base.size; i++) {
    printf("\t%s\n", (char*)Array_AtDeref((const Array*)knownCommands, i));
  }

  printf("\nTo know more about a specific command, try running 'cut COMMAND -h'...\n");
}

int main(int argc, char *argv[])
{
  Global env = {
    .directory = ".",
    .library   = 0,
    .roots     = { NULL }
  };

  CHECK_MEMORY

  env.home = getenv("CUT_HOME");

  if (!env.home || !env.home[0]) {
    THROW(NEW (Exception)("No CUT_HOME environment variable defined... exiting!"));
  }

  ObjectArray *knownCommands = ObjectArray_Fill(NEW (ObjectArray)(TYPEOF(String)),
    NEW (String) ("todo"),
    NEW (String) ("cache"),
    NEW (String) ("depends"),
    NEW (String) ("build"),
    NEW (String) ("roots"),
    NULL
  );

  Args        *args;
  const char  *command;
  
  TRY {
    args    = NEW (Args) (argc, argv, &env);
    command = Args_Name(args, "command").as_charptr;
  } CATCH (Exception) {
    fprintf(stderr, "No command provided to cut!\n");
    print_help(knownCommands);
    exit(0);
  } END_TRY;


  if (ObjectArray_ContainsKey(knownCommands, command))
  {
    String *binPath   = Path_Combine(env.home, "CUT/bin/");
    String *cmdParams = Path_Combine(binPath->base, command);
    Array  *arguments = Args_Params(args);

    for (int i = 0; i < arguments->size; i++) {
      String_Append(cmdParams, ' ');
      String_Concat(cmdParams, NEW (String)(Array_AtDeref(arguments, i)));
    }

    FILE *process = popen(cmdParams->base, "r");

    for (int c = getc(process); c != EOF; c = getc(process))
    {
      putc(c, stdout);
    }

    pclose(process);

    DELETE (arguments);
    DELETE (cmdParams);
    DELETE (binPath);
  } else {
    fprintf(stderr, "The command '%s' is unknown!\n", command);
    print_help(knownCommands);
  }

  // TODO: (high): Refactor: Remove
  //char *nargv[2] = { "test", "." };
  //todo(2, nargv);
  // TODO: (low): Test: Remove this shiiit

  // printf("Current folder: %s\n", env.directory);
  // printf("Library?: %s\n", env.library ? "true" : "false");

  // for (int i = 0; env.roots[i] && i < MAX_ROOTS; i++)
  // {
  //   printf("Item %d: %s\n", i, env.roots[i]);
  // }

  // for (DirectoryIterator *di = dopen(env.directory); di; dnext(&di))
  // {
  //   printf("%s\n", di->current.name);
  // }


  DELETE (knownCommands);
  DELETE (args);
  
  CHECK_MEMORY
  STOP_WATCHING

  return 0;
}
