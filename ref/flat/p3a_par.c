// 3a only. Every iteration rebuilds the buckets and sorts the original typelinks.
// Unit ranges are found from the input's ordered unit column, never from an oracle.
#include "flat.h"
#include <omp.h>
enum { NB=16, NC=13 };
static const u32 *sym,*sz,*unit,*str; static const u8 *bucket,*cl,*bin;
static const u64 *off;
static const u8 **textptr; static u32 *textlen;
static u32 rounda(u32 x,u32 a) { return (x+a-1)&~(a-1); }
static int cmp(u32 x,u32 y, size_t d) {
 u64 an=textlen[x],bn=textlen[y];
 size_t n=an<bn?an:bn; int c=n>d?memcmp(textptr[x]+d,textptr[y]+d,n-d):0;
 if(c) return c;
 if(an!=bn) return an<bn?-1:1;
 return (sym[x]>sym[y])-(sym[x]<sym[y]);
}
static unsigned digit(u32 i,size_t d) { return d<textlen[i] ? 1+textptr[i][d] : 0; }
// Stable MSD byte radix; insertion sort finishes small leaves and equal strings by id.
static void radix(u32 *v,u32 *tmp,size_t n,size_t d) {
 if(n<24) { for(size_t i=1;i<n;i++) { u32 x=v[i]; size_t j=i; while(j&&cmp(v[j-1],x,d)>0) { v[j]=v[j-1]; j--; } v[j]=x; } return; }
 size_t count[257]={0},pos[258],next[257];
 for(size_t i=0;i<n;i++) count[digit(v[i],d)]++;
 for(unsigned k=1;k<257;k++) if(count[k]==n) { radix(v,tmp,n,d+1); return; }
 pos[0]=0; for(unsigned k=0;k<257;k++) next[k]=pos[k],pos[k+1]=pos[k]+count[k];
 for(size_t i=0;i<n;i++) tmp[next[digit(v[i],d)]++]=v[i];
 memcpy(v,tmp,n*4);
 // Equal strings still require ascending symbol ids.
 if(count[0]>1) { for(size_t i=1;i<count[0];i++) { u32 x=v[i]; size_t j=i; while(j&&sym[v[j-1]]>sym[x]) { v[j]=v[j-1]; j--; } v[j]=x; } }
 for(unsigned k=1;k<257;k++) if(count[k]>1) {
  size_t s=pos[k],m=count[k];
if(m>512) {
#pragma omp task firstprivate(s,m,d)
   radix(v+s,tmp+s,m,d+1);
  } else radix(v+s,tmp+s,m,d+1);
 }
#pragma omp taskwait
}
int main(int argc,char **argv) {
 flatDir=argc>1?argv[1]:"data/minio"; double seconds=argc>2?atof(argv[2]):1.0;
 u32 L=metaGet("p3_placed"),U=metaGet("nunits"),minfunc=metaGet("minFunc");
 sym=FMAP(u32,"p3_sym.u32",NULL); sz=FMAP(u32,"p3_size.u32",NULL); unit=FMAP(u32,"p3_unit.u32",NULL);
 bucket=FMAP(u8,"p3_bucket.u8",NULL); cl=FMAP(u8,"p3_class.u8",NULL); str=FMAP(u32,"p3_tlstr.u32",NULL);
 off=FMAP(u64,"p3_str.off.u64",NULL); bin=FMAP(u8,"p3_str.bin",NULL);
 const u32 *start=FMAP(u32,"p3_bucket_start.u32",NULL),*mina=FMAP(u32,"p3_bucket_minalign.u32",NULL);
 for(u32 i=0;i<L;i++) if(unit[i]>=U || bucket[i]>=NB || cl[i]>=NC || (i&&unit[i]<unit[i-1])) return 2;
 u32 *addr=malloc(4ull*L),*cs=malloc(4ull*U*NB*NC),*base=malloc(4ull*U*NB),*tl=malloc(4ull*L),*tmp=malloc(4ull*L);
 textptr=malloc(sizeof(*textptr)*L); textlen=malloc(4ull*L);
 double total=0,phase[4]={0}; unsigned reps=0; u32 nt=0;
 do {
 double t=now();
#pragma omp parallel for schedule(static)
 for(u32 u=0;u<U;u++) {
  u32 lo=0,hi=L; while(lo<hi) { u32 m=lo+(hi-lo)/2; if(unit[m]<u) lo=m+1; else hi=m; } u32 beg=lo;
  u32 *c=cs+(size_t)u*NB*NC; memset(c,0,NB*NC*4);
  for(u32 i=beg;i<L&&unit[i]==u;i++) { unsigned b=bucket[i],a=cl[i]; if(b==6) continue; u32 s=sz[i]; if(b<=2&&s<minfunc)s=minfunc; c[b*NC+a]+=rounda(s,1u<<a); }
  for(unsigned b=1;b<NB;b++) { u32 v=0; for(int a=NC-1;a>=0;a--) { u32 n=c[b*NC+a]; c[b*NC+a]=v; v+=n; } base[u*NB+b]=v; }
  for(u32 i=beg;i<L&&unit[i]==u;i++) { unsigned b=bucket[i],a=cl[i]; if(b==6)continue; u32 s=sz[i]; if(b<=2&&s<minfunc)s=minfunc; addr[i]=c[b*NC+a]; c[b*NC+a]+=rounda(s,1u<<a); }
 }
 double t1=now();
#pragma omp parallel for
 for(unsigned b=1;b<NB;b++) { if(b==6)continue; u32 v=start[b]; for(u32 u=0;u<U;u++) {
  u32 n=base[u*NB+b],*c=cs+(size_t)u*NB*NC+b*NC;
  // After scatter, c[a] is the end of class a. Highest nonempty class has nonzero end.
  int top=NC-1; while(top>=0&&!c[top])top--;
  if(n&&top>=0)v=rounda(v,mina[b]>(1u<<top)?mina[b]:1u<<top);
  base[u*NB+b]=v; v+=n;
 } }
#pragma omp parallel for schedule(static)
 for(u32 i=0;i<L;i++) if(bucket[i]!=6) addr[i]+=base[unit[i]*NB+bucket[i]];
 double t2=now(); nt=0; for(u32 i=0;i<L;i++)if(bucket[i]==6) { tl[nt++]=i; textptr[i]=bin+off[str[i]]; textlen[i]=off[str[i]+1]-off[str[i]]; }
#pragma omp parallel
 {
#pragma omp single
  radix(tl,tmp,nt,0);
 }
 double t3=now(); u32 v=start[6]; for(u32 k=0;k<nt;k++) { u32 i=tl[k]; v=rounda(v,1u<<cl[i]); addr[i]=v; v+=sz[i]; }
 double t4=now(); total+=t4-t; phase[0]+=t1-t; phase[1]+=t2-t1; phase[2]+=t3-t2; phase[3]+=t4-t3; reps++;
 } while(total<seconds);
 printf("p3a_par threads=%d reps=%u total=%.6f ms=%.6f classify=%.6f prefix_scatter=%.6f sort=%.6f walk=%.6f symbols=%u tl=%u\n",omp_get_max_threads(),reps,total,1e3*total/reps,1e3*phase[0]/reps,1e3*phase[1]/reps,1e3*phase[2]/reps,1e3*phase[3]/reps,L,nt);
 return check32("addr",addr,L,"oracle/p3_addr.u32")!=0;
}
