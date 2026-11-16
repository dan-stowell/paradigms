#define main p3holes_program_main
#include "p3holes.c"
#undef main
static unsigned rng=17491;
static unsigned rnd(void) { rng=rng*1664525u+1013904223u; return rng; }
int main(void) {
 Data d={.nobj=1,.ng=17}; State s={.a=&d,.U=1000};
 s.ar=calloc(17,sizeof(Arena));s.tail=calloc(17,4);s.holehead=calloc(17,4);
 s.holes=calloc(1200,sizeof(Hole));s.alloccap=calloc(1000,4);s.address=calloc(1000,4);
 for(u32 g=2;g<=3;g++){s.ar[g]=(Arena){1024+(g-2)*512,256,32};s.tail[g]=s.ar[g].base;u32 b=bucket(&d,g);s.spare[b]=4096+(g-2)*1048576;s.spareend[b]=s.spare[b]+1048576;}
 Sym y={.id=1,.group=2,.occ=32,.align=32};
 CK(reserve(&s,&y)==1024);s.address[1]=1024;
 release(&s,2,1024,32);s.alloccap[1]=0;
 y.occ=8;y.align=8;CK(reserve(&s,&y)==1024);
 y.id=2;y.align=16;y.occ=16;CK(reserve(&s,&y)==1040);
 release(&s,2,1024,8);release(&s,2,1040,16);
 CK(s.holes[s.holehead[2]].lo==1024 && s.holes[s.holehead[2]].len==32);
 s.alloccap[1]=s.alloccap[2]=0;
 Sym live[65]={0};u8 active[65]={0};
 for(unsigned step=0;step<20000;step++) {
  u32 id=1+(rnd()>>8)%64;
  if(active[id]) {release(&s,live[id].group,s.address[id],s.alloccap[id]);s.alloccap[id]=0;active[id]=0;}
  else {Sym *z=&live[id];*z=(Sym){.id=id,.group=2+((rnd()>>7)&1),.align=1u<<((rnd()>>9)%6)};z->occ=up(1+((rnd()>>12)%64),z->align);s.address[id]=reserve(&s,z);active[id]=1;}
  for(u32 i=1;i<=64;i++)if(active[i]){
   CK(!(s.address[i]%live[i].align));
   for(u32 j=i+1;j<=64;j++)if(active[j]) CK(s.address[i]+s.alloccap[i]<=s.address[j] || s.address[j]+s.alloccap[j]<=s.address[i]);
   for(u32 g=2;g<=3;g++)for(u32 h=s.holehead[g];h;h=s.holes[h].next){Hole *x=&s.holes[h];CK(s.address[i]+s.alloccap[i]<=x->lo || x->lo+x->len<=s.address[i]);}
  }
  for(u32 g=2;g<=3;g++)for(u32 h=s.holehead[g];h;h=s.holes[h].next){Hole*x=&s.holes[h];CK(x->len);if(x->next)CK(x->lo+x->len<s.holes[x->next].lo);}
 }
 // Grow beyond capacity moves only that allocation; shrink then regrow retains it.
 Data td={.nobj=2,.ng=33}; State t={.a=&td,.U=1000};
 t.ar=calloc(17,sizeof(Arena));t.tail=calloc(17,4);t.holehead=calloc(17,4);
 t.holes=calloc(1200,sizeof(Hole));t.alloccap=calloc(1000,4);t.address=calloc(1000,4);
 t.ar[3]=(Arena){1024,64,32};t.tail[3]=1024;t.spare[1]=4096;t.spareend[1]=8192;
 Sym before={.id=1,.group=3,.occ=64,.align=32}, after=before;
 Data f={.s=&before,.index=calloc(1000,4)}, to={.s=&after,.index=calloc(1000,4)};
 f.index[1]=to.index[1]=1;u32 ids[]={1};
 t.address[1]=reserve(&t,&before);CK(t.address[1]==1024);
 after.occ=32;allocate_delta(&t,&f,&to,ids,1);CK(after.addr==1024 && t.alloccap[1]==64);
 before=after;after.occ=64;allocate_delta(&t,&f,&to,ids,1);CK(after.addr==1024);
 before=after;after.occ=128;allocate_delta(&t,&f,&to,ids,1);CK(after.addr==4096 && t.alloccap[1]==128);t.address[1]=after.addr;
 // A zero-sized common symbol also retains its address despite capacity zero.
 Sym zero_before={.id=5,.group=3,.occ=0,.align=8}, zero_after=zero_before;
 Data zf={.s=&zero_before,.index=calloc(1000,4)},zt={.s=&zero_after,.index=calloc(1000,4)};
 zf.index[5]=zt.index[5]=1;t.address[5]=1200;u32 zid[]={5};
 allocate_delta(&t,&zf,&zt,zid,1);CK(zero_after.addr==1200 && t.alloccap[5]==0);
 // Freed original extent is used before tail or spare; overflow stays arena-owned.
 Sym z={.id=2,.group=3,.occ=64,.align=32};CK(reserve(&t,&z)==1024);
 release(&t,3,4096,128);t.alloccap[1]=0;
 z.id=3;z.occ=96;CK(reserve(&t,&z)==4096);
 release(&t,3,4096,96);CK(t.holes[t.holehead[3]].lo==4096 && t.holes[t.holehead[3]].len==128);
 t.ar[4]=(Arena){2048,0,32};t.tail[4]=2048;
 // Another arena in this bucket cannot borrow the first arena's freed overflow.
 t.spare[1]=8192;t.spareend[1]=16384;
 z.id=4;z.group=4;z.occ=64;CK(reserve(&t,&z)==8192);
 puts("REVIEW allocator: 20000 random allocate/free steps, alignment, first fit splitting, coalescing, live/live and live/hole disjointness OK");
 return 0;
}
