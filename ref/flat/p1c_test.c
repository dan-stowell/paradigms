// Independent all-pairs oracle for the partitioned content join, including zero
// hash keys, kind separation, mark precedence, size exceptions, and id ties.
// gcc -O2 -march=native -fopenmp -o build/p1c_test ref/flat/p1c_test.c
#include "flat.h"
#include "p1c.h"
int main(void) {
 enum {N=1000};
 u8 kind[N+1], mark[N+1]; u64 key[2*(N+1)]; u32 canon[N+1], out[N+1];
 for (int rep=0; rep<100; rep++) {
  for (u32 i=0;i<=N;i++) {
   kind[i]=1+rand()%5; mark[i]=rand()%3;
   key[2*i]=rand()%31; key[2*i+1]=rand()%9; canon[i]=i;
  }
  contentJoin(N,kind,mark,key,canon,out,16);
  for(u32 i=1;i<=N;i++) {
   u32 w=i, expected=i;
   if ((kind[i]==2 || kind[i]==3) && mark[i]) {
    for(u32 j=1;j<=N;j++) {
     if(!mark[j] || kind[j]!=kind[i] || key[2*j]!=key[2*i] || (kind[i]==3 && key[2*j+1]!=key[2*i+1])) continue;
     if(mark[j]<mark[w] || (mark[j]==mark[w] && ((kind[i]==2 && key[2*j+1]>key[2*w+1]) || ((kind[i]==3 || key[2*j+1]==key[2*w+1]) && j<w)))) w=j;
    }
    if(mark[w]==1 && !(kind[i]==2 && mark[i]==2 && key[2*i+1]>key[2*w+1])) expected=w;
   }
   if(out[i]!=expected) return 1;
  }
 }
 puts("random brute-force 1c: 100 x 1000 slots OK");
}
