/* sps_crop: rewrite the H.264 SPS of a recorded mp4 so the decoder crops the
 * bottom rows that the 6.0.1 camera daemon leaves as zero padding.  The CPP
 * pads 1134 -> 1152 and scales without cropping, so the bottom 1080/64 = 16
 * rows of the 1080p output are zero data and decode as a green band.
 *
 * The SPS grows by a couple of bytes, so the avcC box and every enclosing box
 * (moov/trak/mdia/minf/stbl/stsd/avc1) grow too; the 'free' box that the
 * recorder writes between moov and mdat is shrunk by exactly the same amount,
 * which keeps mdat (and therefore every stco/co64 offset) where it was.
 *
 *   sps_crop in.mp4 [out.mp4] [bottom]
 *
 * With one argument the file is patched in place (temp file + rename).  If the
 * SPS already carries a crop box the file is left untouched (exit 3).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void put32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}

typedef struct { uint8_t *b; int len, cap; } Bits;

static void bits_init(Bits *s,int cap){s->b=calloc(cap,1);s->len=0;s->cap=cap;}
static void bits_put(Bits *s,int bit){
    if(s->len/8>=s->cap){s->cap*=2;s->b=realloc(s->b,s->cap);}
    if(bit) s->b[s->len/8] |= 0x80>>(s->len%8);
    s->len++;
}
static void bits_putbits(Bits *s,const uint8_t *src,int n){
    for(int i=0;i<n;i++) bits_put(s,(src[i/8]>>(7-i%8))&1);
}
static void bits_putue(Bits *s,uint32_t v){
    v++; int len=0; for(uint32_t t=v;t>1;t>>=1) len++;
    for(int i=0;i<len;i++) bits_put(s,0);
    for(int i=len;i>=0;i--) bits_put(s,(v>>i)&1);
}
static int getbit(const uint8_t *b,int *p){int v=(b[*p/8]>>(7-*p%8))&1;(*p)++;return v;}
static uint32_t getue(const uint8_t *b,int *p){
    int z=0; while(!getbit(b,p)) z++;
    uint32_t v=0;
    for(int i=0;i<z;i++) v=(v<<1)|getbit(b,p);
    return ((1u<<z)-1)+v;
}
static int32_t getse(const uint8_t *b,int *p){
    uint32_t k=getue(b,p);
    return (k&1)?(int32_t)((k+1)/2):-(int32_t)(k/2);
}
static int de_epb(const uint8_t *in,int n,uint8_t *out){
    int o=0;
    for(int i=0;i<n;i++){
        if(i+2<n && in[i]==0 && in[i+1]==0 && in[i+2]==3){out[o++]=0;out[o++]=0;i+=2;}
        else out[o++]=in[i];
    }
    return o;
}
static int add_epb(const uint8_t *in,int n,uint8_t *out){
    int o=0,z=0;
    for(int i=0;i<n;i++){
        if(z>=2 && in[i]<=3){out[o++]=3;z=0;}
        out[o++]=in[i];
        z = in[i]==0 ? z+1 : 0;
    }
    return o;
}

/* returns new sps length, writes NAL header + cropped rbsp into out.
 * *had_crop is set if the SPS already had a cropping box. */
static int crop_sps(const uint8_t *sps,int n,uint8_t *out,int bottom,int *had_crop){
    uint8_t *rbsp=malloc(n); int rn=de_epb(sps+1,n-1,rbsp);
    int cap=(rn+16)*8; uint8_t *bitsbuf=calloc(cap,1);
    Bits bits; bits.b=bitsbuf; bits.len=rn*8; bits.cap=cap;
    memcpy(bits.b,rbsp,rn); bits.len=rn*8;
    const uint8_t *b=bits.b; int p=0;
    int profile=getbit(b,&p)<<7|getbit(b,&p)<<6|getbit(b,&p)<<5|getbit(b,&p)<<4|getbit(b,&p)<<3|getbit(b,&p)<<2|getbit(b,&p)<<1|getbit(b,&p);
    for(int i=0;i<16;i++) getbit(b,&p);
    getue(b,&p);
    if(profile==100||profile==110||profile==122||profile==244||profile==44||profile==83||profile==86||profile==118||profile==128||profile==138||profile==139||profile==134||profile==135){
        uint32_t cf=getue(b,&p);
        if(cf==3) getbit(b,&p);
        getue(b,&p); getue(b,&p); getbit(b,&p);
        if(getbit(b,&p)){
            int nn=cf!=3?8:12;
            for(int i=0;i<nn;i++){
                if(getbit(b,&p)){
                    int last=8,next=8,sz=i<6?16:64;
                    for(int j=0;j<sz;j++){
                        if(next!=0) next=(last+getse(b,&p)+256)%256;
                        last = next!=0?next:last;
                    }
                }
            }
        }
    }
    getue(b,&p); uint32_t poc=getue(b,&p);
    if(poc==0) getue(b,&p);
    else if(poc==1){ getbit(b,&p); getse(b,&p); getse(b,&p); uint32_t k=getue(b,&p); for(uint32_t i=0;i<k;i++) getse(b,&p); }
    getue(b,&p); getbit(b,&p); getue(b,&p); getue(b,&p);
    int fmo=getbit(b,&p); if(!fmo) getbit(b,&p); getbit(b,&p);
    int crop_pos=p;
    /* the recorder already crops the 1088->1080 padding; keep that and only
     * extend it if the caller asks for more rows. */
    uint32_t l=0,r=0,t=0,b0=0; int has=0;
    {
        const uint8_t *bb=bits.b; int q=crop_pos;
        if(getbit(bb,&q)){
            has=1;
            l=getue(bb,&q); r=getue(bb,&q); t=getue(bb,&q); b0=getue(bb,&q);
        }
    }
    if(has && (int)b0 >= bottom){ *had_crop=1; free(rbsp); free(bitsbuf); return 0; }
    Bits nb; bits_init(&nb, cap);
    bits_putbits(&nb, bits.b, crop_pos);
    bits_put(&nb,1); bits_putue(&nb,l); bits_putue(&nb,r); bits_putue(&nb,t); bits_putue(&nb,bottom);
    for(int i=crop_pos;i<bits.len;i++) bits_put(&nb,(bits.b[i/8]>>(7-i%8))&1);
    int pad=(8-nb.len%8)%8;
    bits_put(&nb,1); for(int i=0;i<(pad?pad-1:7);i++) bits_put(&nb,0);
    int rn2=nb.len/8;
    int enc=add_epb(nb.b,rn2,out+1);
    out[0]=sps[0];
    free(rbsp); free(bitsbuf); free(nb.b);
    return enc+1;
}

int main(int argc,char **argv){
    if(argc<2){fprintf(stderr,"usage: %s in.mp4 [out.mp4] [bottom]\n",argv[0]);return 2;}
    int bottom=argc>3?atoi(argv[3]):12;
    int inplace = (argc<3);
    FILE *f=fopen(argv[1],"rb");
    if(!f){perror("open");return 1;}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<=0){fprintf(stderr,"empty file\n");return 1;}
    uint8_t *d=malloc(n+65536); if(fread(d,1,n,f)!=(size_t)n){fprintf(stderr,"read fail\n");return 1;}
    fclose(f);

    long a=-1;
    for(long i=0;i+8<n;i++) if(!memcmp(d+i+4,"avcC",4)){a=i;break;}
    if(a<0){fprintf(stderr,"no avcC\n");return 1;}
    uint32_t asz=be32(d+a);
    uint8_t *body=d+a+8;
    uint32_t spslen=(body[6]<<8)|body[7];
    uint8_t newsps[128];
    int had=0;
    int newlen=crop_sps(body+8,spslen,newsps,bottom,&had);
    if(had || newlen<=0){fprintf(stderr,"already cropped, skip\n");return 3;}
    int delta=newlen-(int)spslen;
    if(delta<=0){fprintf(stderr,"unexpected delta %d\n",delta);return 1;}

    long moov=-1;
    for(long i=0;i+8<n;i++) if(!memcmp(d+i+4,"moov",4)){moov=i;break;}
    if(moov<0){fprintf(stderr,"no moov\n");return 1;}
    uint32_t msz=be32(d+moov);
    long moov_end=moov+msz;
    long fr=-1; uint32_t fsz=0;
    for(long i=moov_end;i+8<n;){
        uint32_t s=be32(d+i);
        if(!memcmp(d+i+4,"free",4)){fr=i;fsz=s;break;}
        if(s<8) break;
        i+=s;
    }
    if(fr<0){fprintf(stderr,"no free box after moov\n");return 1;}
    if((long)fsz < delta+8){fprintf(stderr,"free box too small\n");return 1;}

    long sps_start=a+16;             /* SPS NAL, after the 2-byte length */
    long sps_end=sps_start+spslen;
    long free_end=fr+fsz;

    uint8_t *out=malloc(n+65536);
    memcpy(out,d,sps_start-2);
    out[sps_start-2]=(newlen>>8)&0xff;
    out[sps_start-1]=newlen&0xff;
    memcpy(out+sps_start,newsps,newlen);
    long w=sps_start+newlen;
    long r1len=(free_end-delta)-sps_end;      /* rest of avcC..free payload minus delta */
    memcpy(out+w,d+sps_end,r1len); w+=r1len;
    memcpy(out+w,d+free_end,n-free_end); w+=n-free_end;

    put32(out+a,asz+delta);
    put32(out+moov,msz+delta);
    const char *chain[]={"trak","mdia","minf","stbl","stsd","avc1"};
    for(unsigned k=0;k<sizeof(chain)/sizeof(chain[0]);k++){
        for(long i=moov;i+8<moov_end+delta;i++){
            if(!memcmp(out+i+4,chain[k],4)){ put32(out+i,be32(out+i)+delta); break; }
        }
    }
    put32(out+fr+delta,fsz-delta);   /* the free box moved by delta, shrank by delta */

    if(w!=n){fprintf(stderr,"internal: size mismatch %ld vs %ld\n",w,n);return 1;}

    char outpath[4096];
    if(inplace) snprintf(outpath,sizeof(outpath),"%s.tmp",argv[1]);
    else        snprintf(outpath,sizeof(outpath),"%s",argv[2]);
    FILE *g=fopen(outpath,"wb");
    if(!g){perror("out");return 1;}
    if(fwrite(out,1,w,g)!=(size_t)w){fprintf(stderr,"write fail\n");fclose(g);unlink(outpath);return 1;}
    fclose(g);
    if(inplace){
        if(rename(outpath,argv[1])){perror("rename");return 1;}
    }
    fprintf(stderr,"cropped sps %u -> %d (bottom=%d), delta %d\n",spslen,newlen,bottom,delta);
    return 0;
}
