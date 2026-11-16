// gen N seed out : N random u32 (xorshift64*), little-endian raw
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main(int c,char**v){
  uint64_t n=strtoull(v[1],0,10),s=strtoull(v[2],0,10)*2654435761u+88172645463325252ull;
  FILE*f=fopen(v[3],"wb");uint32_t*b=malloc(4<<20);
  for(uint64_t i=0;i<n;){uint64_t k=0;
    for(;k<(1<<20)&&i<n;k++,i++){s^=s>>12;s^=s<<25;s^=s>>27;b[k]=(uint32_t)((s*2685821657736338717ull)>>32);}
    fwrite(b,4,k,f);}
  fclose(f);return 0;}
