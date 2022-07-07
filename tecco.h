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
#ifndef TECCO_H__
# define TECCO_H__

# include <stddef.h>
# include <stdlib.h>

# define tecco_assert_eq(x, y) {			\
    if (x != y) {					\
      puts ("Assertion failed");			\
      printf ("╰ EXPECTED\t%s\n", #x);			\
      printf ("╰ GOT     \t%s\n", #y);			\
      exit (1);						\
    }							\
  }

struct tecco_test {
  const char *name;

  int (*test) (void);

  char *stdout;
  char *stderr;
  int  result;
};

struct tecco_suite {
  const char *name;

  struct tecco_test *tests;
  size_t            test_c;

  int (*setup) (void);
  int (*cleanup) (void);
};

struct tecco_runner {
  struct tecco_suite *suites;
  size_t             suite_c;
};

enum tecco_flag {
  TECCO_NO_OUTPUT_FAILURE  = 1,
  TECCO_NO_OUTPUT_SUCCESS  = 2,
  TECCO_VERBOSE_OUTPUT     = 4,
  TECCO_COLOURS            = 8
};

static size_t __test_count = 0;

struct tecco_runner tecco_init_runner (void);
void                tecco_destroy_runner (struct tecco_runner);

struct tecco_suite * tecco_add_suite (struct tecco_runner *,
				      const char *,
				      int (*) (void),
				      int (*) (void));

struct tecco_test * tecco_add_test (struct tecco_suite *,
				    const char *,
				    int (*) (void));

int  tecco_run_tests (struct tecco_runner *);
void tecco_output_tests (struct tecco_runner, int);

#endif /* TECCO_H__ */
