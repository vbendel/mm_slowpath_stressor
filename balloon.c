#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

long long atoll_b(const char *nptr);
void set_oom_score(int oom_adj);
int allocfault_memory(void *mem_ptr, long long size);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <oom_score_adj> <mem_size>\n", argv[0]);
        exit(1);
    }

    int oom_adj = atoi(argv[1]);
    long long mem_size = atoll_b(argv[2]);
	void *mem_ptr;

	set_oom_score(oom_adj);
	if (allocfault_memory(mem_ptr, mem_size) < 0)
		return -1;

    while (1)
        sleep(10);

	free(mem_ptr);

    return 0;
}

void set_oom_score(int oom_adj) {
	char proc_file_path[64];
    sprintf(proc_file_path, "/proc/%d/oom_score_adj", getpid());

    int fd = open(proc_file_path, O_WRONLY);
    if (fd < 0) {
        perror("Err: open oom_score_adj file failed.\n");
        exit(1); 
	}

    char oom_str[16];
    sprintf(oom_str, "%d\n", oom_adj);

    ssize_t num_written = write(fd, oom_str, strlen(oom_str));
    if (num_written < 0) {
        perror("Err: write to oom_score_adj failed.\n");
        exit(1);
    }

    close(fd);

    printf("oom_score_adj set to %d\n", oom_adj);
}

int allocfault_memory(void *mem_ptr, long long size) {
	
	int PAGE_SIZE = getpagesize();
	long long num_pages = size / PAGE_SIZE;
	int i;
	char data[] = "ABC";
	void *mem_iter;

	mem_ptr = malloc(size);
	if (!mem_ptr) {
		perror("Err: ENOMEM.\n");
		return -1;
	}

	printf("Allocated %lld Bytes (%lld pages) of memory.\n", size, num_pages);

	mem_iter = mem_ptr;
	for (i = 0; i < num_pages; ++i) {
		memcpy(mem_iter, data, sizeof(data));
		mem_iter += PAGE_SIZE;
	}

	return 0;
}


/* Convert a string representation of a number with an optional size suffix
 * to a long long.
 *
 * Copyright:
 * This function has been copied from https://github.com/resurrecting-open-source-projects/stress
 */
long long
atoll_b (const char *nptr)
{
  int pos;
  char suffix;
  long long factor = 0;
  long long value;

  if ((pos = strlen (nptr) - 1) < 0)
    {
      fprintf(stderr, "invalid string\n");
      exit (1);
    }

  switch (suffix = nptr[pos])
    {
    case 'b':
    case 'B':
      factor = 0;
      break;
    case 'k':
    case 'K':
      factor = 10;
      break;
    case 'm':
    case 'M':
      factor = 20;
      break;
    case 'g':
    case 'G':
      factor = 30;
      break;
    default:
      if (suffix < '0' || suffix > '9')
        {
          fprintf(stderr, "unrecognized suffix: %c\n", suffix);
          exit (1);
        }
    }

  if (sscanf (nptr, "%llu", &value) != 1)
    {
      fprintf(stderr, "invalid number: %s\n", nptr);
      exit (1);
    }

  value = value << factor;

  return value;
}


