// CUT
#include <directory.h>
#include <terminal.h>
#include <filestream.h>
#include <str.h>
#include <charstream.h>
#include <collection.str.h>
#include <args.h>

#define WIDTH_FILE 40
#define WIDTH_LINE 10
#define WIDTH_PRIO 15
#define WIDTH_WHAT 20
#define WIDTH_DESC 40

OPTIONS(
  { "path", '-', "The path to examine for 'todos'", ARG_TYPE_CHARPTR, NULL }
);

void printrep(char *file, char *line, char *prio, char *what, char *desc) {
  char filebuf[WIDTH_FILE + 1];
  char linebuf[WIDTH_LINE + 1];
  char priobuf[WIDTH_PRIO + 1];
  char whatbuf[WIDTH_WHAT + 1];
  char descbuf[WIDTH_DESC + 1];

  ljust(file, filebuf, WIDTH_FILE);
  rjust(line, linebuf, WIDTH_LINE);
  cjust(prio, priobuf, WIDTH_PRIO);
  cjust(what, whatbuf, WIDTH_WHAT);
  ljust(desc, descbuf, WIDTH_DESC);

  if (!strcmp(prio, "(low)")) {
    printf(TEXT_GREEN);
  } else if (!strcmp(prio, "(normal)")) {
    printf(TEXT_YELLOW);
  } else if (!strcmp(prio, "(high)")) {
    printf(TEXT_RED);
  } else if (!strcmp(prio, "PRIORITY")) {
    printf(FONT_ULINE);
    printf(FONT_BOLD);
  }

  printf("%s%s%s%s%s\n", filebuf, linebuf, priobuf, whatbuf, descbuf);

  printf(FONT_RESET);
}

int header = 0;
int main(int argc, char *argv[])
{
  Args *args = NEW (Args) (argc, argv, NULL);

  const char *path = Args_Name(args, "path").as_charptr;

  path = path ? path : ".";

  for (DirectoryIterator *di = dopen(path); !ddone(di); dnext(di))
  {
    char fullname[PATH_MAX_LENGTH << 1];
    char extension[32];

    sprintf(fullname, "%s/%s", di->path, di->current.name);
    fext(di->current.name, sizeof(extension), extension);

    if (di->current.type == DIRTYPE_FILE && (!strcmp(extension, ".c") || !strcmp(extension, ".h")))
    {
      CharStream *file = (CharStream*) NEW (FileStream) (fopen(fullname, "r"));

      if (file) {
        int     num = 0;
        String *line;

        while ((line = CharStream_GetLine(file))) {
          int icomment = String_Cnt(line, "//");
          int index    = String_Cnt(line, "TODO:");

          ++num;

          if (icomment >= 0 && index > icomment) {
            String *desc  = NEW (String) (String_Trim(String_SubString(line, index + 5, 0))->base);
            Array  *split = (Array*)String_Split(line, ":");
            char    linenum[WIDTH_LINE + 1];

            sprintf(linenum, "%d", num);

            if (!header)
            {
              printrep("FILE", "LINE", "PRIORITY", "TYPE", "DESCRIPTION");
              header = 1;
            }

            if (split->size == 3) {
              printrep(di->current.name, linenum, 
                String_Trim(Array_At(split, 0))->base, 
                String_Trim(Array_At(split, 1))->base,
                String_Trim(Array_At(split, 2))->base);
            } else {
              printrep(di->current.name, linenum, "(normal)", "Not categorized", desc->base);
            }

            DELETE (desc);
            DELETE (split);
          } else {
            DELETE (line);
          }
        }

        DELETE (file);
      }
    }
    else if (di->current.type == DIRTYPE_DIRECTORY && di->current.name[0] != '.')
    {
      char *nargv[2] = { "", fullname };

      main(2, nargv);
    }
  }

  if (!header && strcmp(argv[0], ""))
  {
    printf("Nothing to do...\n");
  }

  DELETE (args);

  return 0;
}