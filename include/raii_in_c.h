/* -*- tab-width: 2; indent-tabs-mode: nil -*- */
/** \file
 * \par License
 * The code in this file is <a href="http://creativecommons.org/publicdomain/zero/1.0/legalcode">Public Domain</a>.
 * \par
 * Documentation and comment text, written by Daniel Trebbien, is licensed under the
 * <a rel="license" href="http://creativecommons.org/licenses/by/3.0/">Creative Commons Attribution 3.0 Unported License</a>.
 */

#ifndef RAII_IN_C_H
#define RAII_IN_C_H 1
#include <assert.h>

#define MAKE_DESTRUCTOR_STACK(n) \
  void *destructor_label_addresses[n] __attribute__((unused)); \
  void *return_label_address __attribute__((unused)); \
  return_label_address = NULL; \
  void **next_label_address  __attribute__((unused)); \
  next_label_address = destructor_label_addresses;
#define PUSH_DESTRUCTOR__(lineno, ...) \
  assert(destructor_label_addresses <= next_label_address); \
  assert(destructor_label_addresses + sizeof destructor_label_addresses/sizeof destructor_label_addresses[0] > next_label_address); \
  *next_label_address++ = &&destructor ## lineno; \
  goto after_destructor ## lineno; \
  destructor ## lineno: \
    ({__VA_ARGS__}); \
    goto *return_label_address; \
  after_destructor ## lineno:;
#define PUSH_DESTRUCTOR_(lineno, ...) PUSH_DESTRUCTOR__(lineno, __VA_ARGS__)
#define PUSH_DESTRUCTOR(...) PUSH_DESTRUCTOR_(__LINE__, __VA_ARGS__)
#define BEGIN_SCOPE \
  void **destructor_label_address_stack_end __attribute__((unused)); \
  destructor_label_address_stack_end = next_label_address - 1;
#define END_SCOPE__(lineno) \
  return_label_address = &&return ## lineno; \
  return ## lineno: assert(destructor_label_address_stack_end < next_label_address); if (next_label_address - 1 != destructor_label_address_stack_end) { \
    void **const destructor_label_address = --next_label_address; \
    assert(destructor_label_addresses <= destructor_label_address); \
    goto *(*destructor_label_address); \
  }
#define END_SCOPE_(lineno) END_SCOPE__(lineno)
#define END_SCOPE END_SCOPE_(__LINE__)
#define END_SCOPE_AND_BREAK \
  if (({END_SCOPE 1;})) break; else break
#define END_SCOPE_AND_RETURN(ret) \
  if (destructor_label_address_stack_end = destructor_label_addresses - 1, ({END_SCOPE 1;})) return (ret); else return (ret)

#endif
