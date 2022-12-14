#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process {
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  u32 start_exec_time;
  u32 end_time;
  u32 remaining_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  //each process is a node in the tailq
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);
/*
process_list = struct created by tailQ_head of type process

*/

u32 next_int(const char **data, const char *data_end) {
  u32 current = 0;
  bool started = false;
  while (*data != data_end) {
    char c = **data;

    //check that current character is a number
    if (c < 0x30 || c > 0x39) {
      if (started) {
	return current;
      }
    }
    else {
      if (!started) {
	current = (c - 0x30);
	started = true;
      }
      else {
	current *= 10; //mutiply by 10 to prepare to add in next number
	current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data) {
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++])) {
    if (c < 0x30 || c > 0x39) {
      exit(EINVAL);
    }
    if (!started) {
      current = (c - 0x30);
      started = true;
    }
    else {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED) {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;
  

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL) {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i) {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }
  
  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  /*
  process_list struct contains a pair of pointers, 
  one to the first element and one to the last element of the quque
  */
  TAILQ_INIT(&list); //list  = head pointer of tailq

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */
  //data points to array of process structs
  
  u32 min_start = data[0].arrival_time;
  struct process * proc = &data[0];
  u32 i;
  for(i = 0;i<size;i++){
    data[i].start_exec_time = -1; //initiate start time to -1, process has not started
    data[i].remaining_time = data[i].burst_time;
    if (data[i].arrival_time < min_start){
      min_start = data[i].arrival_time;
      proc  = &data[i];
    }
    // printf("pid: %d, startexec %d, remainingtime %d \n", data[i].pid, data[i].start_exec_time, data[i].remaining_time);
  }
  u32 curr_time = proc->arrival_time;
  u32 finished = 0;
  u32 quant_count;
  TAILQ_INSERT_TAIL(&list, proc,pointers);
  
  while(finished < size){
    printf("currtime %d", curr_time);
    for(i=0; i< size;i++)
    {
      if (data[i].arrival_time == curr_time
        && data[i].remaining_time >0
        && &data[i] !=TAILQ_FIRST(&list) )
      {
        TAILQ_INSERT_TAIL(&list, &data[i],pointers);
      }
    }
    if (quant_count== quantum_length){
      if (proc){
          // printf("reinsert: %d\n", proc->pid);
          TAILQ_INSERT_TAIL(&list, proc, pointers);
        }
        proc = NULL;
        quant_count=0;
    }
    //no process running, run next waiting process
    if (!TAILQ_EMPTY(&list) &&  quant_count==0){
      proc = TAILQ_FIRST(&list);
      // printf("starting: %d", proc->pid);
      TAILQ_REMOVE(&list, TAILQ_FIRST(&list),pointers);
      if (proc->start_exec_time == -1){
          proc->start_exec_time = curr_time;
        }
    }
    if(proc){
      //executing process
      printf("running: %d\n", proc->pid);   
        proc->remaining_time--;
        quant_count++; 
        if (proc->remaining_time ==0){
          proc->end_time = curr_time + 1;
          finished++;
          proc = NULL;
          quant_count = 0;
          // printf("finished %d", finished);
        } 
    }
    
    //no processes to execute right now
    curr_time++;
  }
  for (i = 0; i<size;i++){
    // printf("end time %d, arrival time %d, start exec: %d",data[i].end_time, data[i].arrival_time, data[i].start_exec_time);
    total_waiting_time += data[i].end_time - data[i].arrival_time - data[i].burst_time;
    total_response_time += data[i].start_exec_time - data[i].arrival_time;
  }
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float) total_waiting_time / (float) size);
  printf("Average response time: %.2f\n", (float) total_response_time / (float) size);

  free(data);
  return 0;
}