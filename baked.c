/* baked.c - Ever burned a cake?
 * Copyright 2023 Emil Williams
 *
 * Licensed under the GNU Public License version 3 only, see LICENSE.
 *
 * @EXEC cc $@ -o $* -std=gnu89 -O2 -Wall -Wextra -Wpedantic -pipe $CFLAGS STOP@
 * @COMPILECMD cc $@ -o $* -std=gnu89 -O2 -Wall -Wextra -Wpedantic -pipe $CFLAGS
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Require space after @ABC and before STOP@ (no space required around newline) */
#define REQUIRE_SPACE

# define OTHER_START "@COMPILECMD"
# define START "@EXEC"
# define  STOP "STOP@"
# define  HELP                                                                         \
    "target-file [arguments ...]\n"                                                    \
    "Use the format `@EXEC cmd ...' within the target-file, this will execute the\n"   \
    "rest of line, or if found within the file, until the STOP@ marker. You may use\n" \
    "@COMPILECMD instead of @EXEC.  Whitespace is required after and before both\n"    \
    "operators always.\n"


#define DESC                                                \
  "Options [Must always be first]\n"                        \
  "\t-h, this message, -n dryrun\n"                         \
  "Expansions\n"                                            \
  "\t$@  returns target-file                (abc.x.txt)\n"  \
  "\t$*  returns target-file without suffix (^-> abc.x)\n"  \
  "\t$+  returns arguments\n"

#define local_assert(expr, ret) do { assert(expr); if (!expr) { return ret; }} while (0)

static char * g_filename, * g_short, * g_all;

static const char *
find(const char * x, const char * buf, const size_t max, const size_t min)
{
  const char * start = buf;
  for (; *buf; ++buf)
  {
    if (max - (buf - start) > min && !strncmp(buf, x, min))
    { return buf; }
  }
  return NULL;
}

static char *
find_region(const char * fn)
{
  struct stat s;
  int fd;
  char * buf = NULL;

  fd = open(fn, O_RDONLY);

  if (fd != -1)
  {
    if (!fstat(fd,&s)
        &&   s.st_mode & S_IFREG
        &&   s.st_size)
    {
      const char * start, * stop;
      char * addr;
      addr = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
      if (addr != MAP_FAILED)
      {
        start = find(START, addr, s.st_size, strlen(START));
        if (!start)
        {
          start = find(OTHER_START, addr, s.st_size, strlen(OTHER_START));
          start = start - strlen(START) + strlen(OTHER_START) * (start != 0);
        }
        if (start)
        {
          start += strlen(START);
#ifdef REQUIRE_SPACE
          if (!isspace(*start))
          {
            fprintf(stderr, "ERROR: Found start without suffix spacing.\n");
            return NULL;
          }
#endif
          stop = find(STOP, start, s.st_size - (start - addr), strlen(STOP));
          if (!stop)
          {
            stop = start;
            while (*stop && *stop == '\n'
               /* &&  !(*stop == '\r' */
               /* ||    *stop == '\n') */
              )
            {
              if (stop[0] == '\\')
              {
                if (stop[1] == '\n')
                { stop += 2; }
                /* else if (stop[1] == '\r' && stop[2] == '\n') */
                /* { stop += 3; } */
              }
              ++stop;
            }
          }
#ifdef REQUIRE_SPACE
          else
          {
            if (!isspace(*(stop - 1)))
            {
              fprintf(stderr, "ERROR: Found stop without prefixing spacing.\n");
              return NULL;
            }
          }
#endif
          if (stop)
          { buf = strndup(start, (stop - addr) - (start - addr)); }
        }
        munmap(addr, s.st_size);
      }
    }
    close(fd);
  }
  return buf;
}

static void
swap(char * a, char * b)
{
  *a ^= *b;
  *b ^= *a;
  *a ^= *b;
}

static int
root(char * root)
{
  int ret;
  char x[1] = "\0";
  size_t len = strlen(root);
  while (len && root[len] != '/')
  { --len; }
  if (!len)
  { return 0; }
  swap(root + len, x);
  ret = chdir(root);
  swap(root + len, x);
  return ret;
}

static char *
insert(const char * new, char * str, size_t offset, size_t shift)
{
  size_t len, max;
  local_assert(new, str);
  local_assert(str, NULL);
  len = strlen(new);
  max = (strlen(str) + 1 - offset - shift);
  memmove(str + offset + len, str + offset + shift, max);
  memcpy(str + offset, new, len);
  return str;
}

static char *
shorten(char * fn)
{
  size_t i, last = 0, len;
  char * sh;
  local_assert(fn, NULL);
  len = strlen(fn);
  sh = malloc(len + 1);
  local_assert(sh, NULL);
  for (i = 0; i < len; ++i)
  {
    if (fn[i] == '.')
    { last = i; }
  }
  last = last ? last : i;
  strncpy(sh, fn, last);
  sh[last] = '\0';
  return sh;
}

static char *
all_args(size_t argc, char ** argv)
{
  char * all = NULL;
  if (argc > 2)
  {
    size_t i, len = argc;
    for (i = 2; i < argc; ++i)
    { len += strlen(argv[i]); }
    all = malloc(len + 1);
    local_assert(all, NULL);
    all[len] = '\0';
    len = 0;
    for (i = 2; i < argc; ++i)
    {
      strcpy(all + len, argv[i]);
      len += strlen(argv[i]) + 1;
      if (i + 1 < argc)
      { all[len - 1] = ' '; }
    }
  }
  return all;
}

static size_t
expand_size(char * buf, int argc, char ** argv)
{
  size_t i, len, max;
  len = max = strlen(buf) + 1;
  for (i = 0; i < len; ++i)
  {
    if (buf[i] == '\\')
    { i += 2; continue; }
    else if (buf[i] == '$')
    {
      switch (buf[++i])
      {
      case '@':
        max += strlen(g_filename);
        break;
      case '*':
        if (!g_short)
        { g_short = shorten(g_filename); }
        max += g_short ? strlen(g_short) : 0;
        break;
      case '+':
        if (!g_all)
        { g_all = all_args((size_t) argc, argv); }
        max += g_all ? strlen(g_all) : 0;
        break;
      }
    }
  }
  return max;
}

static char *
expand(char * buf)
{
  size_t i;
  char * ptr = NULL;
  for (i = 0; buf[i]; ++i)
  {
    if (buf[i] == '\\')
    { i += 2; continue; }
    else if (buf[i] == '$')
    {
      switch (buf[++i])
      {
      case '@':
        ptr = g_filename;
        break;
      case '*':
        ptr = g_short;
        break;
      case '+':
        ptr = g_all ? g_all : "";
        break;
      default: continue;
      }
      buf = insert(ptr, buf, i - 1, 2);
    }
  }
  free(g_short); free(g_all);
  return buf;
}

static size_t
strip(char * buf)
{
  size_t i = strlen(buf);
  if (!i)
  { return 0; }
  while (isspace(buf[i - 1]))
  { --i; }
  buf[i] = '\0';
  for (i = 0; isspace(buf[i]); ++i);
  return i;
}

static int
run(const char * buf)
{
  fputs("Output:\n", stderr);
  root(g_filename);
  return system(buf);
}

int
main(int argc, char ** argv)
{
  int ret = 0;
  char * buf;

  assert(setlocale(LC_ALL, "C"));

  if (argc < 2
  ||  !strcmp(argv[1], "-h"))
  { goto help; }

  g_filename = argv[1];

  if (!strcmp(argv[1], "-n"))
  {
    if (argc > 2)
    { ret = 1; g_filename = argv[2]; }
    else
    { goto help; }
  }

  buf = find_region(g_filename);
  if (!buf)
  {
    if (errno)
    { perror(argv[0]); }
    else
    { fprintf(stderr, "%s: File unrecognized.\n", argv[0]); }
    return 1;
  }

  buf = realloc(buf, expand_size(buf, argc, argv));
  local_assert(buf, 1);
  buf = expand(buf);
  fprintf(stderr, "Exec: %s\n", buf + strip(buf) - (buf[0] == '\n'));
  if ((ret = ret ? 0 : run(buf)))
  { fprintf(stderr, "Result: %d\n", ret); }

  free(buf);
  return ret;
help:
  fprintf(stderr, "%s: %s", argv[0], HELP DESC);
  return 1;
}
