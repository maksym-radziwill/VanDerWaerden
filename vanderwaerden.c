/* Considerable speedup by only checking starting from the last and newly appended element */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

/* Remark: Using fewer pointers might result in a speed-up */

/* --------------------------------------------------------------
   Memory allocation functions 
   -------------------------------------------------------------- */

#define BITS_CHAR 1

static int block_size = 512; 

struct signs {
  int length;    /* Contains exactly length bits */
  char * signs; 
};

void append_sign_binary(struct signs * s, char sign){
  if(sign == 0){
    /* Nothing to do , by default the newly allocated memory is set to 0 */
    return;
  }

  if(sign == 1){
    int byte = s->length / BITS_CHAR; 
    int rem = s->length % BITS_CHAR;
    /* Set the rem bit to 1 */
    s->signs[byte] = s->signs[byte] | (1 << rem); 
  }
}

void allocate_space(struct signs * s){
  if(s->length == 0){
    s->signs = calloc(block_size,sizeof(char));
    return;
  }

  if((s->length % (block_size*BITS_CHAR)) == 0){
    s->signs = realloc(s->signs, (s->length/BITS_CHAR) + block_size);
    s->signs[s->length/BITS_CHAR] = 0;
    return;
  }
}

void append_sign(struct signs * s, char sign){
  allocate_space(s); 
  append_sign_binary(s, sign);
  s->length = s->length + 1; 
}

char read_sign(struct signs s, int position){
  if(position >= s.length){
    fprintf(stderr, "Attempting to read a sign at an illegal position\n");
    exit(-1); 
  }else{
    int byte = position / BITS_CHAR;
    int rem = position % BITS_CHAR; 
    return ((s.signs[byte] >> rem) & 0b00000001);
  }
}

void init_sign(struct signs * s){
  s->length = 0;
}

void free_sign(struct signs * s){
  s->length = 0;
  free(s->signs); 
}

/* --------------------------------------------- */

void print_sign(struct signs s){
  if(s.length == 0)
    printf("empty");
  else
    for(int i = 0; i < s.length; i++)
      printf("%d", (int) read_sign(s, i));
}

struct signs extract_sign (struct signs s, int i, int j){
  struct signs new;
  init_sign(&new);
  for(int k = 0; i - k*j >= 0; k++)
    append_sign(&new, read_sign(s, i - k*j));
  return new;
}

#define bool int
#define true 1
#define false 0

bool matches(struct signs a, struct signs b){
  if(a.length > b.length)
    return false;

  for(int i = 0; i < a.length; i++)
    if(read_sign(a, i) != read_sign(b, i))
      return false;

  return true; 
}

static struct signs mono0;
static struct signs mono1; 

void init_mono(int n){
  init_sign(&mono0);
  init_sign(&mono1); 
  for(int i = 0; i < n; i++){
    append_sign(&mono0, (char) 0);
    append_sign(&mono1, (char) 1); 
  }
}

void free_mono(){
  free_sign(&mono0);
  free_sign(&mono1); 
}

bool matches_waerden(struct signs a){
  if(mono0.length == 0 || mono1.length == 0){
    fprintf(stderr, "Error: Monochromatic sequence not initialized\n");
    exit(-1); 
  }

  if((matches(mono0, a) == true) || (matches(mono1, a) == true)) return true;

  return false;
}

bool check_matches_waerden(struct signs s){
  int n = mono0.length;  
  
  if(s.length < n) return false;

    for(int j = 1; (n-1)*j < s.length; j++){
      struct signs extracted = extract_sign(s, s.length - 1, j);
      bool matches = matches_waerden(extracted);
      free_sign(&extracted);
      if(matches == true) return true; 
    }
  
  return false;
}

struct signs copy_append_sign(struct signs s, char sign){
  struct signs new0;
  init_sign(&new0);
  for(int i = 0; i < s.length; i++){
    append_sign(&new0, read_sign(s, i)); 
  }
  append_sign(&new0, sign);
  return new0;
}

int max(int a, int b){
  if(a > b) return a;
  return b;
}


/* -------------------------------------------------------
   Main algorithm 
   ------------------------------------------------------- */

sem_t threads_sem;
pthread_mutex_t running_mutex; 
static int max_length = 0; 
static int counter = 0; 

int waerden_length(struct signs * w){
  /* This is a ugly hack because I was lazy to make everything into a pointer version */
  struct signs s;
  s.length = w->length;
  s.signs = w->signs;
  
  struct signs s0 = copy_append_sign(s, 0);
  struct signs s1 = copy_append_sign(s, 1);
  
  bool s0matches = check_matches_waerden(s0);
  bool s1matches = check_matches_waerden(s1);

  
  pthread_mutex_lock(&running_mutex);
  if(max_length < s.length){
    max_length = s.length;
    printf("\n%d ", max_length);
    print_sign(s); 
    fflush(0);
  }
  pthread_mutex_unlock(&running_mutex);

  
  int length; 
  
  if(s0matches == true && s1matches == true){
    length = s0.length;
    free_sign(&s0);
    free_sign(&s1);
  }

  if(s0matches == false && s1matches == true){
    free_sign(&s1);
    pthread_yield(); 
    length = waerden_length(&s0);
    free_sign(&s0);
  }

  if(s0matches == true && s1matches == false){
    free_sign(&s0);
    pthread_yield(); 
    length = waerden_length(&s1);
    free_sign(&s1);
  }

  if(s0matches == false && s1matches == false){
    pthread_t pth_s0;
    pthread_t pth_s1;
    if(sem_trywait(&threads_sem) == 0){
      int ret_s0;
      int ret_s1; 
      pthread_create(&pth_s0, NULL, (void *) waerden_length, &s0);
      pthread_create(&pth_s1, NULL, (void *) waerden_length, &s1); 
      pthread_join(pth_s0, (void *) &ret_s0);
      printf("Thread 1 complete\n"); 
      pthread_join(pth_s1, (void *) &ret_s1);
      printf("Thread 2 complete\n"); 
      sem_post(&threads_sem);
      length = max(ret_s0, ret_s1);
    }else{
      length = max(waerden_length(&s1), waerden_length(&s0)); 
    }
    free_sign(&s1);
    free_sign(&s0); 
  }

  //  printf("Thread vegatating\n"); 
  
  return length; 
  
}

int main (int argc, char ** argv){

  struct signs s; 
  int num_cpu = 1; 
  
  if(argc < 2){
    printf("Usage: %s n num_cpu\nComputes the n-th van der Waerden number\n", argv[0]);
    return 0;
  }

  init_sign(&s); 
  init_mono(atoi(argv[1]));
  if(argc > 2){
    num_cpu = atoi(argv[2]); 
  }
  
  sem_init(&threads_sem,0, num_cpu - 1); 
  printf("\r%d\n", waerden_length(&s));
  free_mono();
  sem_destroy(&threads_sem); 
}
