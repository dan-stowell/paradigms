// mmap a raw little-endian u32 file, pass to the Futhark library, time it.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "hello.h"
static double now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
int main(int argc,char**argv){
  if(argc<2){fprintf(stderr,"usage: %s file.u32 [threads] [reps]\n",argv[0]);return 2;}
  int threads=argc>2?atoi(argv[2]):0, reps=argc>3?atoi(argv[3]):3;
  int fd=open(argv[1],O_RDONLY); struct stat st; fstat(fd,&st);
  int64_t n=st.st_size/4;
  uint32_t*p=mmap(0,st.st_size,PROT_READ,MAP_PRIVATE|MAP_POPULATE,fd,0);
  struct futhark_context_config*cfg=futhark_context_config_new();
#ifdef MULTICORE
  if(threads>0) futhark_context_config_set_num_threads(cfg,threads);
#endif
  struct futhark_context*ctx=futhark_context_new(cfg);
  double t0=now_ms();
  struct futhark_u32_1d*arr=futhark_new_u32_1d(ctx,p,n); // copies into Futhark memory
  futhark_context_sync(ctx);
  double t1=now_ms();
  printf("upload(copy) %.1f ms\n",t1-t0);
  for(int r=0;r<reps;r++){
    uint64_t o[3]; struct futhark_u64_1d*out;
    double s=now_ms();
    if(futhark_entry_hello(ctx,&out,arr)){fprintf(stderr,"err: %s\n",futhark_context_get_error(ctx));return 1;}
    futhark_context_sync(ctx);
    double e=now_ms()-s;
    futhark_values_u64_1d(ctx,out,o); futhark_context_sync(ctx); futhark_free_u64_1d(ctx,out);
    printf("run %d: %.1f ms  sum=%llu ck_scan=%llu ck_hist=%llu\n",r,e,(unsigned long long)o[0],(unsigned long long)o[1],(unsigned long long)o[2]);
  }
  futhark_free_u32_1d(ctx,arr);
  futhark_context_free(ctx); futhark_context_config_free(cfg);
  return 0;}
