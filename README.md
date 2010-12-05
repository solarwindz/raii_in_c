## Introduction
If you have ever programmed in C++, then you have probably come to love the style of programming called [RAII](https://secure.wikimedia.org/wikipedia/en/wiki/Resource_Acquisition_Is_Initialization), where cleanup routines can be configured to run automatically. It is a technique that was invented by Bjarne Stroustrup, inventor of C++, to help free allocated resources when the "handles" to them go out of scope. Used properly, the technique can eliminate memory leaks, handle leaks, and other common logic errors in programs.

RAII programming is "natively supported" in only a few programming languages. But, for C++ programmers who need to program in C, it is one of the features that are most missed.

This project provides an *ad hoc* solution to RAII-style programming in C.

## Implementation details
The present implementation of raii_in_c is a single header file containing macros. These macros utilize GCC extensions to the C language, so the solution is specific to `gcc` and compilers that support the following extensions:

* [Labels as Values](http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html#Labels-as-Values)
* [Statement Expressions](http://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html#Statement-Exprs)
* [Variable Attributes](http://gcc.gnu.org/onlinedocs/gcc/Variable-Attributes.html#Variable-Attributes)

## Example
Here is an example C program:

    #include <stddef.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <time.h>
    #include <sys/stat.h>
    
    #include <apr.h>
    #include <apr_errno.h>
    #include <apr_general.h>
    #include <apr_pools.h>
    #include <apr_strings.h>
    #include <apr_tables.h>
    #include <svn_types.h>
    #include <svn_error_codes.h>
    #include <svn_error.h>
    #include <svn_string.h>
    #include <svn_io.h>
    #include <svn_subst.h>
    
    #ifndef APR_STATUS_IS_SUCCESS
    #define APR_STATUS_IS_SUCCESS(s) ((s) == APR_SUCCESS)
    #endif
    
    static char s_1KiB_buf[1024];
    
    int main(int argc, const char *const *argv, const char *const *env)
    {
      apr_status_t apr_status = apr_app_initialize(&argc, &argv, &env);
      if (! APR_STATUS_IS_SUCCESS(apr_status)) {
        fprintf(stderr, "`apr_app_initialize` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
        return EXIT_FAILURE;
      }
      atexit(apr_terminate);
      
      apr_pool_t *root_pool = NULL;
      apr_status = apr_pool_create(&root_pool, NULL);
      if (! APR_STATUS_IS_SUCCESS(apr_status)) {
        fprintf(stderr, "`apr_pool_create` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
        return EXIT_FAILURE;
      }
      
      struct stat st;
      if (-1 == stat("2600.txt", &st)) {
        fprintf(stderr, "Failed to stat `2600.txt`. errno = %d\n", (int) errno);
        apr_pool_destroy(root_pool);
        return EXIT_FAILURE;
      }
      
      char *const data = (char*) malloc(((size_t) st.st_size) + 1);
      if (data == NULL) {
        fprintf(stderr, "`malloc` failed to allocate %d bytes.\n", (int) (st.st_size + 1));
        apr_pool_destroy(root_pool);
        return EXIT_FAILURE;
      }
      
      FILE *fp = fopen("2600.txt", "r");
      if (fp == NULL) {
        fprintf(stderr, "Failed to open `2600.txt` for reading\n");
        free(data);
        apr_pool_destroy(root_pool);
        return EXIT_FAILURE;
      }
      
      off_t bytes_read = 0;
      char *p = data + bytes_read;
      size_t size = fread(p, 1, st.st_size - bytes_read, fp);
      bytes_read += size;
      p += size;
      while (bytes_read < st.st_size && ! ferror(fp) && ! feof(fp)) {
        size = fread(p, 1, st.st_size - bytes_read, fp);
        bytes_read += size;
        p += size;
      }
      *p = '\0';
      
      if (ferror(fp)) {
        fprintf(stderr, "An I/O error occurred.\n");
        fclose(fp);
        free(data);
        apr_pool_destroy(root_pool);
        return EXIT_FAILURE;
      }
      
      fclose(fp);
      
      const clock_t before_clocks = clock();
      
      for (size = 0; size < 100; ++size) {
        apr_pool_t *pool = NULL;
        apr_status = apr_pool_create(&pool, root_pool);
        if (! APR_STATUS_IS_SUCCESS(apr_status)) {
          fprintf(stderr, "`apr_pool_create` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
          free(data);
          apr_pool_destroy(root_pool);
          return EXIT_FAILURE;
        }
        
        svn_string_t *translated_string = NULL;
        svn_error_t *svn_error = svn_subst_translate_string(&translated_string, svn_string_create(data, pool), "UTF-8", pool);
        if (svn_error) {
          fprintf(stderr, "`svn_subst_translate_string` failed: %s\n", svn_err_best_message(svn_error, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
          apr_pool_destroy(pool);
          free(data);
          apr_pool_destroy(root_pool);
          return EXIT_FAILURE;
        }
        
        apr_pool_destroy(pool);
      }
      
      const clock_t after_clocks = clock();
      printf("(after_clocks - before_clocks) = %d\n", (int) (after_clocks - before_clocks));
      
      free(data);
      apr_pool_destroy(root_pool);
      return EXIT_SUCCESS;
    }

The idea here was to simulate the effect of RAII cleaning up the APR string pools, file handles, and malloc blocks as if C supported RAII natively. C does not support RAII natively, so several copies of the various cleanup routines (e.g. calls to `apr_pool_destroy`, `free`, etc.) exist within the code. Having to maintain the reverse stack of destructors manually is the main reason why programming C in this way is inflexible and error-prone.

Translated to use raii_in_c, the program looks like:

    #include <stddef.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <time.h>
    #include <sys/stat.h>

    #include <apr.h>
    #include <apr_errno.h>
    #include <apr_general.h>
    #include <apr_pools.h>
    #include <apr_strings.h>
    #include <apr_tables.h>
    #include <svn_types.h>
    #include <svn_error_codes.h>
    #include <svn_error.h>
    #include <svn_string.h>
    #include <svn_io.h>
    #include <svn_subst.h>

    #include <raii_in_c.h>

    #ifndef APR_STATUS_IS_SUCCESS
    #define APR_STATUS_IS_SUCCESS(s) ((s) == APR_SUCCESS)
    #endif

    static char s_1KiB_buf[1024];

    int main(int argc, const char *const *argv, const char *const *env)
    {
      MAKE_DESTRUCTOR_STACK(5)
      BEGIN_SCOPE
      
      apr_status_t apr_status = apr_app_initialize(&argc, &argv, &env);
      if (! APR_STATUS_IS_SUCCESS(apr_status)) {
        fprintf(stderr, "`apr_app_initialize` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
        END_SCOPE_AND_RETURN(EXIT_FAILURE);
      }
      PUSH_DESTRUCTOR( apr_terminate(); )
      
      apr_pool_t *root_pool = NULL;
      apr_status = apr_pool_create(&root_pool, NULL);
      if (! APR_STATUS_IS_SUCCESS(apr_status)) {
        fprintf(stderr, "`apr_pool_create` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
        END_SCOPE_AND_RETURN(EXIT_FAILURE);
      }
      PUSH_DESTRUCTOR( apr_pool_destroy(root_pool); )
      
      struct stat st;
      if (-1 == stat("2600.txt", &st)) {
        fprintf(stderr, "Failed to stat `2600.txt`. errno = %d\n", (int) errno);
        END_SCOPE_AND_RETURN(EXIT_FAILURE);
      }
      
      char *const data = (char*) malloc(((size_t) st.st_size) + 1);
      if (data == NULL) {
        fprintf(stderr, "`malloc` failed to allocate %d bytes.\n", (int) (st.st_size + 1));
        END_SCOPE_AND_RETURN(EXIT_FAILURE);
      }
      PUSH_DESTRUCTOR( free(data); )
      
      do { BEGIN_SCOPE
        FILE *fp = fopen("2600.txt", "r");
        if (fp == NULL) {
          fprintf(stderr, "Failed to open `2600.txt` for reading\n");
          END_SCOPE_AND_RETURN(EXIT_FAILURE);
        }
        PUSH_DESTRUCTOR( fclose(fp); )
        
        off_t bytes_read = 0;
        char *p = data + bytes_read;
        size_t size = fread(p, 1, st.st_size - bytes_read, fp);
        bytes_read += size;
        p += size;
        while (bytes_read < st.st_size && ! ferror(fp) && ! feof(fp)) {
          size = fread(p, 1, st.st_size - bytes_read, fp);
          bytes_read += size;
          p += size;
        }
        *p = '\0';
        
        if (ferror(fp)) {
          fprintf(stderr, "An I/O error occurred.\n");
          END_SCOPE_AND_RETURN(EXIT_FAILURE);
        }
        
        END_SCOPE
      } while (0);
      
      const clock_t before_clocks = clock();
      
      size_t size;
      for (size = 0; size < 100; ++size) { BEGIN_SCOPE
        apr_pool_t *pool = NULL;
        apr_status = apr_pool_create(&pool, root_pool);
        if (! APR_STATUS_IS_SUCCESS(apr_status)) {
          fprintf(stderr, "`apr_pool_create` failed: %s\n", apr_strerror(apr_status, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
          END_SCOPE_AND_RETURN(EXIT_FAILURE);
        }
        PUSH_DESTRUCTOR( apr_pool_destroy(pool); )
        
        svn_string_t *translated_string = NULL;
        svn_error_t *svn_error = svn_subst_translate_string(&translated_string, svn_string_create(data, pool), "UTF-8", pool);
        if (svn_error) {
          fprintf(stderr, "`svn_subst_translate_string` failed: %s\n", svn_err_best_message(svn_error, s_1KiB_buf, sizeof s_1KiB_buf/sizeof s_1KiB_buf[0]));
          END_SCOPE_AND_RETURN(EXIT_FAILURE);
        }
        
        END_SCOPE
      }
      
      const clock_t after_clocks = clock();
      printf("(after_clocks - before_clocks) = %d\n", (int) (after_clocks - before_clocks));
      
      END_SCOPE_AND_RETURN(EXIT_SUCCESS);
    }

The benefit is that blocks of code can be moved and re-arranged without affecting the proper cleanup of resources.
