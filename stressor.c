/**
 * Copyright & Credits note:
 *
 * This program has been developed by me (Vratislav Bendel <vbendel@redhat.com>)
 * during my working hours at 'Red Hat Czech, s.r.o', whathever that means for copyright.
 *
 * Intent of this program is aimed to be openly usable within modern GPL licence standards.
 *
 * Few bits were coppied and/or inspired from other OpenSource projects, namely:
 *   -) https://github.com/resurrecting-open-source-projects/stress
 * 
 * Some bits were inspired by OpenAI's ChatGPT outputs.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sched.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>


// Defines
#define cpumask_t long long
#define CPU_UNBOUND -1

// Wrappers
float long_arithmetic_calculation(int loops_multiplier);
void write_data_to_memory(void *mem_page);
void worker_init(int cpu);
void run_anon_worker(long long mem_size, long long report_mem);
void run_file_worker(long long mem_size, long long report_mem, char *file_path);
void print_help(void);

// Parsers
long long atoll_b (const char *nptr);
cpumask_t parse_cpu_list(const char *cpu_list);

// Timers
void start_clock(clock_t *stopwatch);
void report_time(clock_t *stopwatch);

// Options
struct option long_options[] = {
  {"anon-mem", 1, NULL, 'a'},
  {"cpus", 1, NULL, 'c'},
  {"dry-run", 0, NULL, 'd'},
  {"file-mem", 1, NULL, 'f'},
  {"help", 0, NULL, 'h'},
  {"oom-score", 1, NULL, 'o'},
  {"file-path", 1, NULL, 'p'},
  {NULL, 0, NULL, 0}
};

// Globals
static char *exec_name;
static int calc_loops = 1;  // default value

int main(int argc, char *argv[]) {
	
	// Settings (Defaults)
    long long anon_mem = 0;
    long long file_mem = 0;  // default memory size is 0
	char *file_path = "";  // default file_path is unset
	cpumask_t cpus = 0;  // default run unbound workers
	long long report_mem = atoll_b("64K");  // default report every 64K
	int procs_per_cpu = 1;  // Default 1 worker per mode (anon/file)
    static int oom_score = -400;  // default OOM_SCORE_ADJ for this
	// Calculation length (loops) defined by static 'calc_loops', default = 1

	// Var declarations
    int opt;  // option for getopt
	int pid;  // fork pid
	int status;  // wait() status
	int workers = 0;  // forked children count
    char *arg_value;  // value for argument
	int dry_run = 0;
	long long i, t;  // iterators
	exec_name = argv[0];  //static

	// OOM score adj tmp vars
    char proc_file_path[64];
    char oom_str[16];
    int fd;
    ssize_t num_written;


	if (argc < 2) {
		print_help();
		exit(EXIT_FAILURE); // No arguments? Probably not what the user wants..
	}

	// Option parsing
    while ((opt = getopt_long(argc, argv, "hl:r:t:", long_options, NULL)) != -1) { 
        switch (opt) {
			case 'a':
				arg_value = optarg;
                anon_mem = atoll_b(arg_value);
                break;
			case 'c':
				arg_value = optarg;
                cpus = parse_cpu_list(arg_value);
                break;
			case 'd':
				dry_run = 1;
                break;
			case 'f':
				arg_value = optarg;
                file_mem = atoll_b(arg_value);
                break;
			case 'h':
				print_help();
				exit(EXIT_SUCCESS);
            case 'l':
				arg_value = optarg;
				calc_loops = atoi(arg_value);
				break;
			case 'o':
				arg_value = optarg;
                oom_score = atoi(arg_value);
                break;
			case 'p':
				file_path = optarg;
				break;
			case 'r':
				arg_value = optarg;
				report_mem = atoll_b(arg_value);
				break;
			case 't':
				arg_value = optarg;
                procs_per_cpu = atoi(arg_value);
                break;
            default: /* '?' */
				fprintf(stderr, "Unrecognized option: %s\n", optarg);
				print_help();
                exit(EXIT_FAILURE);
        }
    }

	// Sanity Checks
	if (dry_run) {
        printf("DRY RUN:\n");
		printf("--anon-mem == %lld\n", anon_mem);
		printf("--file-mem == %lld\n", file_mem);
		printf("--file-path == \"%s\"\n", file_path);
		printf("--cpus == 0x%x\n", cpus);
		printf("-t  (processes per CPU)  == %d\n", procs_per_cpu);
		printf("--oom-score == %d\n", oom_score);
		printf("-l  (loops) == %d\n", calc_loops);
		printf("-r  (report_mem) == %lld\n", report_mem); 
		exit(EXIT_SUCCESS);
	}

	if (anon_mem <= 0 && file_mem <= 0) {
		fprintf(stderr, "You need to specify ANON and/or FILE memory size.\n");
        exit(EXIT_FAILURE);
	}
	if ((file_mem > 0 && file_path == "") 
		|| (file_mem <= 0) && (file_path != "")) {
		fprintf(stderr, "You need to specify both FILE memory size and FILE_PATH.\n");
        exit(EXIT_FAILURE);
    }

	// Set oom_score_adj
    sprintf(proc_file_path, "/proc/%d/oom_score_adj", getpid());
    fd = open(proc_file_path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Open /proc/PID/oom_score_adj failed\n");
        exit(EXIT_FAILURE);
    }
    sprintf(oom_str, "%d\n", oom_score);
    num_written = write(fd, oom_str, strlen(oom_str));
    if (num_written < 0) {
        fprintf(stderr, "OOM score adjustment write() failed\n");
        exit(EXIT_FAILURE);
    }
    close(fd);

	// Worker forking
	// TODO
	if (!cpus) {
	// No cpu_list specified - spawn an unbound worker for each active mode (anon/file)
		for (t=0; t < procs_per_cpu; t++) {
			if (anon_mem > 0) {
				switch (pid = fork()) {
					case 0:  // child
						worker_init(CPU_UNBOUND);
						run_anon_worker(anon_mem, report_mem);
					case -1:  // error
    	                fprintf(stderr, "Fork failed with (-1)\n");
				        exit(EXIT_FAILURE);
					default:  // parent
						workers++;
				}
			}
			if (file_mem > 0 && file_path != "") {
				switch (pid = fork()) {
            	    case 0:  // child
						worker_init(CPU_UNBOUND);
    	                run_file_worker(file_mem, report_mem, file_path);
	                case -1:  // error
						fprintf(stderr, "Fork failed with (-1)\n");
			    	    exit(EXIT_FAILURE);
            	    default:  // parent
						workers++;
    	        }
			}
		}
	}
	else {  
	// We got specified cpu_list - spawn workers pinned to specified CPUs for each mode (anon/file)
		for (i=0; i < 64; i++) {
			if (cpus & (long long) 1 << i) {
				for (t=0; t < procs_per_cpu; t++) {
			        if (anon_mem > 0) {
			            switch (pid = fork()) {
        			        case 0:  // child
								worker_init(i);
		        	            run_anon_worker(anon_mem, report_mem);
        		    	    case -1:  // error
                			    fprintf(stderr, "Fork failed with (-1)\n");
		                    	exit(EXIT_FAILURE);
	        		        default:  // parent
    	            		    workers++;
			            }
		    	    }
		        	if (file_mem > 0 && file_path != "") {
        		    	switch (pid = fork()) {
		                	case 0:  // child
								worker_init(i);
	        		            run_file_worker(file_mem, report_mem, file_path);
    	            		case -1:  // error
			                    fprintf(stderr, "Fork failed with (-1)\n");
        			            exit(EXIT_FAILURE);
		        	        default:  // parent
        		    	        workers++;
		            	}
	        		}
				}
			}
		}
	}

	// Wait children
	while (workers) {
		if ((pid = wait(&status) > 0))
			workers--;
		// RFE: Improve waiting for children
	}

	// End of Program
   	exit(EXIT_SUCCESS);
}
/* END of MAIN() */

void run_anon_worker(long long mem_size, long long report_mem) {
	void *mem_ptr, *iter_mem_ptr;
	int PAGE_SIZE = getpagesize(); // get the system page size
	long long num_pages = mem_size / PAGE_SIZE;
	long long report_pages = report_mem / PAGE_SIZE;
	pid_t pid = getpid();
	clock_t stopwatch;
	long long i, k;  // iterators

    if (mem_size <= 0) {
        fprintf(stderr, "BUG: Anon thread sanity: Memory size must be greater than 0.\n");
        exit(EXIT_FAILURE);
    }

	while(1) {
		
	    mem_ptr = malloc(mem_size); // allocate memory
    	if (mem_ptr == NULL) {
        	fprintf(stderr, "Failed to allocate memory.\n");
	        exit(EXIT_FAILURE);
    	}
		
    	printf("PID %d: Allocated %d bytes of memory (%d pages)\n", pid, mem_size, num_pages);
	    
		iter_mem_ptr = mem_ptr;

    	for (k = 0; k < 10; k++) {//while (1) {
        	start_clock(&stopwatch); // start the clock

	        for (i = 0; i < num_pages; i++) {
    	        // Write data to memory page
        	    write_data_to_memory(iter_mem_ptr);
	
    	        // Perform arithmetic calculation
        	    long_arithmetic_calculation(calc_loops);

            	// Move to next memory page
	            iter_mem_ptr += PAGE_SIZE;

    	        if (i>0 && (i % report_pages == 0)) {
        	            report_time(&stopwatch); //report elapsed time and reset
            	}
	        }
        // TODO: 
        // - move mem alloc into function
        //

    	    iter_mem_ptr = mem_ptr;
	    }

    	free(mem_ptr); // free the allocated memory
	}
}

void run_file_worker(long long mem_size, long long report_mem, char *file_path) {
	int fd;
	char *file_ptr, *file_iter;
	int PAGE_SIZE = getpagesize(); // get the system page size
	off_t file_size = mem_size;
    long long num_pages = mem_size / PAGE_SIZE;
    long long report_pages = report_mem / PAGE_SIZE;
    pid_t pid = getpid();
    clock_t stopwatch;
    int i;
    
	if (mem_size <= 0) {
        fprintf(stderr, "BUG: File thread sanity: Memory size must be greater than 0.\n");
        exit(EXIT_FAILURE);
    }

	if (file_path == "") {
		fprintf(stderr, "BUG: File thread sanity: File path not specified.\n");
		exit(EXIT_FAILURE);
    }
	
	// Create file and set size
	fd = open(file_path, O_RDWR | O_CREAT);
	if (fd < 0) {
        fprintf(stderr, "Error opening the file %s.\n", file_path);
        exit(EXIT_FAILURE);
    }
	
	if (ftruncate(fd, file_size) < 0) {
        fprintf(stderr, "Error setting file size for %s.\n", file_path);
        close(fd);
        exit(EXIT_FAILURE);
    }

	// mmap the file
	file_ptr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_ptr == MAP_FAILED) {
        fprintf(stderr, "Error mapping the file %s into memory.\n", file_path);
        close(fd);
        exit(EXIT_FAILURE);
    }
	
	// write+fault the file
	while (1) {
        start_clock(&stopwatch); // start the clock

        file_iter = file_ptr;

        for (i = 0; i < num_pages; i++) {
            // Write data to memory page
            write_data_to_memory(file_iter);

            // Perform arithmetic calculation
            long_arithmetic_calculation(calc_loops);

            // Move to next memory page
            file_iter += PAGE_SIZE;

            if (i>0 && (i % report_pages == 0)) {
                report_time(&stopwatch); //report elapsed time and reset
            }
        }
    }
	// Dead code
	if (munmap(file_ptr, file_size) < 0) {
        fprintf(stderr, "Error unmapping the file %s from memory.\n", file_path);
    }
	close(fd);

	exit(EXIT_SUCCESS);
}


void worker_init(int cpu) {
	cpu_set_t mask;

	// Set parent termination signal
	if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
		fprintf(stderr, "Child PRCTL failed with (-1)\n");
        exit(EXIT_FAILURE);
    }

	// Set CPU affinity
	if (cpu != CPU_UNBOUND) {
		CPU_ZERO(&mask);
		CPU_SET(cpu, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) < 0) { 			
			fprintf(stderr, "Set affinity failed.\n");
			exit(EXIT_FAILURE);
		}
	}
}

cpumask_t parse_cpu_list(const char *cpu_list) {
	int online_cpus = get_nprocs();
	char *tmp_cpu_list, *token, *dash;
    long int from, to;
	cpumask_t cpus;

	tmp_cpu_list = malloc(strlen(cpu_list));
	if (!tmp_cpu_list) {
		fprintf(stderr, "Failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}
	strcpy(tmp_cpu_list, cpu_list);

	token = strtok(tmp_cpu_list, ",");
	while (token != NULL) {
		if (dash = strstr(token, "-")) {
			from = strtol(token, NULL, 10);
			to = strtol(++dash, NULL, 10);
			if (to < from) {
				fprintf(stderr, "CPU list parse sanity error. Please correct the CPU ranges to be incremental.\n");
	        	goto error;
			}
		}
		else {
			from = strtol(token, NULL, 10);
			to = from;
		}	
		if (to > online_cpus-1) {
			fprintf(stderr, "CPU range larger than amount of online CPUs!\n");
	        goto error;
		}
		if (to > 63) {
			/**
			 * RFE: This is a 64-bit cpumask limitation
			 */
    	    fprintf(stderr, "This program currently supports only up to CPU number 64. Please adjust your CPU range.\n");
        	goto error;
		}
		while (from <= to) {
			cpus |= 1 << from;
			from++;
		}
		token = strtok(NULL, ",");
	}	
	return cpus;
error:
	free(tmp_cpu_list);
	exit(EXIT_FAILURE);
}

void print_help(void) {
	printf("Usage: %s NEEDS UPDATE!\n", exec_name);
	printf("    [-h --help]          - print this Help\n");
	printf("    [--dry-run]          - don't start anything, only print given setup\n");
	printf("\n");
    printf("    [--anon_mem size]    - malloc() size for anon_workers\n");
    printf("    [--file_mem size]    - file size for file_workers\n");
	printf("    [--file_path fpath]  - specify storage and filesystem target to allocate on\n");
	printf("\n");
	printf("    [--cpus cpu-list]    - specify CPUs on which workers will be spawned and pinned  (default: unbound)\n");
	printf("    [-t num_procs]       - number of workers of each type (anon/file) to spawn  (default: 1)\n");
	printf("                           NOTE: with --cpus spawn this many workers on each specified CPU\n");
	printf("    [--oom-score score]  - set the oom_score_adj of this whole program  (default: -400)\n");
	printf("\n");
	printf("    [-l calc_loops]      - number of calculation loops  (default: 1)\n");
	printf("    [-r report_mem]      - report time after \"going\" through this much memory  (default: 64K)\n");
}

// This function is designed to simply do CPU operation that takes some time.
float long_arithmetic_calculation(int loops_multiplier) {
    float result = 1.0f;
	long long loops = 1000000 * loops_multiplier;
    for (int i = 1; i <= loops; i++) {
        result *= (i % 2 == 0) ? 1.00001f : 0.99999f;
    }
    return result;
}

void write_data_to_memory(void *mem_page) {
    // Define a few bytes of data to write
    char data[] = "ABC";
    
    // Copy the data to the memory page
    memcpy(mem_page, data, sizeof(data));
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

  if (sscanf (nptr, "%lli", &value) != 1)
    {
      fprintf(stderr, "invalid number: %s\n", nptr);
      exit (1);
    }

  value = value << factor;

  return value;
}

void start_clock(clock_t *stopwatch) {
	*stopwatch = clock();
}

void report_time(clock_t *stopwatch) {
	clock_t now = clock();
	pid_t pid = getpid();	
	double cpu_time_used = ((double) (now - *stopwatch)) / CLOCKS_PER_SEC * 1000;
    printf("PID %d: Elapsed time: %f ms\n", pid, cpu_time_used);	
	*stopwatch = now;
}
