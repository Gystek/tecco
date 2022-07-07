/**
 * Copyright (C) 2022 Gustek <gustek@riseup.net>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __unix__
# error "This library cannot work on a non-UNIX system."
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tecco.h"

struct tecco_runner
tecco_init_runner (void)
{
  struct tecco_runner runner;

  runner.suites = NULL;
  runner.suite_c = 0;

  __test_count = 0;

  return runner;
}

static void
tecco_destroy_test (test)
     struct tecco_test test;
{
  free (test.stdout);
  free (test.stderr);
}

static void
tecco_destroy_suite (suite)
     struct tecco_suite suite;
{
  size_t i;

  for (i = 0; i < suite.test_c; i++)
    tecco_destroy_test (suite.tests[i]);

  free (suite.tests);
}

void
tecco_destroy_runner (runner)
     struct tecco_runner runner;
{
  size_t i;

  for (i = 0; i < runner.suite_c; i++)
    tecco_destroy_suite (runner.suites[i]);

  free (runner.suites);
}

struct tecco_suite *
tecco_add_suite (runner, name, setup, cleanup)
     struct tecco_runner *runner;
     const char          *name;
     int (*setup) (void);
     int (*cleanup) (void);
{
  struct tecco_suite *__s;

  __s = realloc (runner->suites, ++runner->suite_c * sizeof(struct tecco_suite));
  if (__s == NULL)
    {
      free (runner->suites);
      return NULL;
    }

  runner->suites = __s;
  __s = runner->suites + runner->suite_c - 1;

  __s->name    = name;
  __s->tests   = NULL;
  __s->test_c  = 0;
  __s->setup   = setup;
  __s->cleanup = cleanup;

  return __s;
}

struct tecco_test *
tecco_add_test (suite, name, test)
     struct tecco_suite  *suite;
     const char          *name;
     int (*test) (void);
{
  struct tecco_test *__t;

  __t = realloc (suite->tests, ++suite->test_c * sizeof(struct tecco_test));
  if (__t == NULL)
    {
      free (suite->tests);
      return NULL;
    }

  suite->tests = __t;
  __t = suite->tests + suite->test_c - 1;

  __t->name   = name;
  __t->test   = test;
  __t->stdout = NULL;
  __t->stderr = NULL;
  __t->result = 0;

  __test_count++;

  return __t;
}

#define BUF_S (0xFFFF)
#define FREE_REDIRECT(p, orig) {			\
    dup2 (p, orig);					\
    close (p);						\
  }

static char _stdout[BUF_S] = {0};
static char _stderr[BUF_S] = {0};

static int
tecco_run_test (test)
     struct tecco_test *test;
{
  int po[2], pe[2], so, se, flags;
  ssize_t os, es;
  size_t ol, el;
  pid_t pid;

  if (pipe (po) == -1)
    {
      perror (test->name);
      return -1;
    }
  if (pipe (pe) == -1)
    {
      perror (test->name);
      return -1;
    }

  so = dup (STDOUT_FILENO);
  se = dup (STDERR_FILENO);

  dup2 (po[1], STDOUT_FILENO);
  dup2 (pe[1], STDERR_FILENO);

  if (pid = fork (), pid < 0)
    {
      FREE_REDIRECT (so, STDOUT_FILENO);
      FREE_REDIRECT (se, STDERR_FILENO);
      perror (test->name);
      return -1;
    }
  else if (pid == 0)
      exit (test->test ());
  else
    {
      int status;

      do
	{
	  if (waitpid (pid, &status, WUNTRACED) == -1)
	    {
	      FREE_REDIRECT (so, STDOUT_FILENO);
	      FREE_REDIRECT (se, STDERR_FILENO);
	      perror (test->name);

	      return -1;
	    }

	  test->result = WEXITSTATUS (status);

	  if (WIFSIGNALED (status))
	    printf ("Test was killed because of signal %d\n", WTERMSIG (status));
	  if (WIFSTOPPED (status))
	    printf ("Test stopped because of signal %d\n", WTERMSIG (status));
	} while (!WIFEXITED (status) && !WIFSIGNALED (status));
    }

  fflush (stdout);
  fflush (stderr);

  FREE_REDIRECT (so, STDOUT_FILENO);
  FREE_REDIRECT (se, STDERR_FILENO);

  flags = fcntl (*po, F_GETFL);
  fcntl (*po, F_SETFL, flags | O_NONBLOCK);

  flags = fcntl (*pe, F_GETFL);
  fcntl (*pe, F_SETFL, flags | O_NONBLOCK);

  if ((os = read (po[0], _stdout, BUF_S - 1)) == BUF_S - 1
      || (es = read(pe[0], _stderr, BUF_S - 1)) == BUF_S - 1)
    {
      fprintf (stderr, "%s: test output exceeded %d bytes\n", test->name, BUF_S);

      return -1;
    }

  ol = strlen (_stdout);
  el = strlen (_stderr);

  if (os != 0
      && strlen (_stdout) != 0
      && (test->stdout = malloc ((os + 1) * sizeof(char))) == NULL)
    {
      perror (test->name);
      return -1;
    }

  if (es != 0
      && strlen (_stderr) != 0
      && (test->stderr = malloc ((es + 1)  * sizeof(char))) == NULL)
    {
      free (test->stdout);
      test->stdout = NULL;
      perror (test->name);
      return -1;
    }

  strncpy (test->stdout, _stdout, ol);
  strncpy (test->stderr, _stderr, el);

  memset (_stdout, 0, BUF_S);
  memset (_stderr, 0, BUF_S);

  return 0;
}

static int
tecco_run_suite (suite)
     struct tecco_suite *suite;
{
  size_t i;

  if (suite->setup != NULL)
    if (suite->setup () != 0)
      return -1;

  for (i = 0; i < suite->test_c; i++)
    if (tecco_run_test (suite->tests + i) != 0)
      return -1;

  if (suite->cleanup != NULL)
    return (suite->cleanup ());

  return 0;
}

int
tecco_run_tests (runner)
     struct tecco_runner *runner;
{
  size_t i;

  for (i = 0; i < runner->suite_c; i++)
    if (tecco_run_suite (runner->suites + i) != 0)
      return -1;

  return 0;
}

#define GREEN() printf ("\033[0;32m")
#define RED() printf ("\033[0;31m")

static void
print_indent (s, indent)
     const char *s;
     size_t indent;
{
  size_t i, j, l;

  if (s == NULL)
    {
      for (j = 0; j < indent; j++)
	putchar ('\t');
      printf ("(no output)\n");
      return;
    }

  l = strlen (s);

  putchar ('\t');

  for (i = 0; i < l; i++)
    {
      putchar (s[i]);

      if (s[i] == '\n' && i != l - 1)
	for (j = 0; j < indent; j++)
	  putchar ('\t');
    }

}

void
tecco_output_tests (runner, flags)
     struct tecco_runner runner;
     int                 flags;
{
  size_t i;
  size_t succ = 0;

  printf ("running %lu tests\n", __test_count);

  for (i = 0; i < runner.suite_c; i++)
    {
      size_t j;
      struct tecco_suite suite = runner.suites[i];

      if (flags & TECCO_VERBOSE_OUTPUT)
	printf ("\n%s:\n", suite.name);

      for (j = 0; j < suite.test_c; j++)
	{
	  struct tecco_test test = suite.tests[j];
	  int s = test.result == 0;

	  if (flags & TECCO_VERBOSE_OUTPUT)
	    {
	      if (flags & TECCO_COLOURS)
		{
		  if (s) GREEN();
		  else RED();
		}
	      printf ("\t%s %s\033[0m", s ? "✓" : "✗", test.name);
	    }
	  else
	    {
	      printf ("%s:%s ... ", suite.name, test.name);
	      if (flags & TECCO_COLOURS)
		{
		  if (s) GREEN();
		  else RED();
		}

	      printf ("%s\033[0m", s ? "ok" : "FAILED");
	    }

	  putchar ('\n');
	}
    }

  for (i = 0; i < runner.suite_c; i++)
    {
      size_t j;
      struct tecco_suite suite = runner.suites[i];

      for (j = 0; j < suite.test_c; j++)
	{
	  struct tecco_test test = suite.tests[j];
	  int s = test.result == 0;

	  if (s) succ++;

	  if ((s && (flags & TECCO_NO_OUTPUT_SUCCESS))
	      || (!s && (flags & TECCO_NO_OUTPUT_FAILURE)))
	    continue;

	  if (test.stdout == NULL && s)
	    continue;

	  printf ("\n=============== %s:%s ===============\n", suite.name, test.name);
	  if (flags & TECCO_VERBOSE_OUTPUT)
	    printf ("exit code: %d\n", test.result);
	  printf ("stdout:\n");
	  print_indent (test.stdout, 1);
	  printf ("stderr:\n");
	  print_indent (test.stderr, 1);
	}
    }

  printf ("\nSUMMARY:");
  if (flags & TECCO_VERBOSE_OUTPUT)
    {
      if (flags & TECCO_COLOURS)
	GREEN();
      printf ("\n  ✓ %lu passed\033[0m\n", succ);
      if (flags & TECCO_COLOURS)
	RED();
      printf ("  ✗ %lu failed\033[0m\n", __test_count - succ);
    }
  else
      printf ("%lu passed; %lu failed.\n", succ, __test_count - succ);
}
