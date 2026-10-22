// One native wire check of the new normal-focus transitions, not a graphics test.
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fd,expected_error,error_seen,event_count;
static uint16_t sequence;
static uint32_t a,b;
static unsigned events[32];
static void fail(const char *s){fprintf(stderr,"focus check: %s\n",s);exit(1);}
static uint16_t r16(const unsigned char*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t r32(const unsigned char*p){return r16(p)|(uint32_t)r16(p+2)<<16;}
static void w16(unsigned char*p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void w32(unsigned char*p,uint32_t v){w16(p,v);w16(p+2,v>>16);}
static void read_all(void*p,size_t n){while(n){ssize_t k=read(fd,p,n);if(k<=0)fail("read/timeout");p=(char*)p+k;n-=k;}}
static uint16_t request(unsigned char*p,size_t n){w16(p+2,n/4);size_t i=0;while(i<n){ssize_t k=write(fd,p+i,n-i);if(k<=0)fail("write");i+=k;}return ++sequence;}
static void reply(uint16_t wanted,unsigned char*out){
    for(;;){unsigned char p[32];read_all(p,32);unsigned type=p[0]&127;
        if(!type){if(p[1]!=expected_error)fail("unexpected X error");error_seen++;}
        else if(type==1){if(r16(p+2)!=wanted||r32(p+4))fail("reply boundary");memcpy(out,p,32);return;}
        else if((type==9||type==10)&&(r32(p+4)==a||r32(p+4)==b)){
            if(p[8]!=0||event_count==32)fail("focus mode/count");
            events[event_count++]=(type<<16)|(p[1]<<8)|(r32(p+4)==a?1:2);
        }
    }
}
static void focus(uint32_t id,unsigned revert){unsigned char p[12]={42};p[1]=revert;w32(p+4,id);request(p,12);}
static void get(unsigned char*p){unsigned char q[4]={43};reply(request(q,4),p);}
static void expect(uint32_t id,unsigned revert){unsigned char p[32];get(p);if(r32(p+8)!=id||p[1]!=revert)fail("focus target/revert");}
static void create(uint32_t id,uint32_t parent){
    unsigned char p[36]={1};w32(p+4,id);w32(p+8,parent);w16(p+16,80);w16(p+18,60);w16(p+22,1);
    w32(p+28,1u<<11);w32(p+32,1u<<21);request(p,36);
    unsigned char m[8]={8};w32(m+4,id);request(m,8);
}
int main(int argc,char**argv){
    if(argc!=2)fail("supply app-private X11 socket");
    fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)fail("socket");
    struct timeval timeout={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if(strlen(argv[1])>=sizeof(address.sun_path))fail("path length");strcpy(address.sun_path,argv[1]);
    if(connect(fd,(struct sockaddr*)&address,sizeof(address)))fail("connect");
    unsigned char setup[12]={'l',0,11},header[8];if(write(fd,setup,12)!=12)fail("setup");read_all(header,8);
    size_t length=r16(header+6)*4;if(header[0]!=1||length<32||length>262144)fail("setup reply");
    unsigned char*body=malloc(length);if(!body)fail("allocation");read_all(body,length);
    size_t screen=32+((r16(body+16)+3)&~(size_t)3)+body[21]*8;if(screen+40>length)fail("screen");
    uint32_t root=r32(body+screen),base=r32(body+4);a=base|1;b=base|2;free(body);
    unsigned char saved[32],p[32];get(saved);
    create(a,root);create(b,a);focus(0,0);get(p);event_count=0;
    focus(a,0);expect(a,0);
    focus(b,2);expect(b,2);
    focus(a,1);expect(a,1);
    const unsigned wanted[]={9u<<16|3u<<8|1,10u<<16|2u<<8|1,9u<<16|2,10u<<16|2,9u<<16|2u<<8|1};
    if(event_count!=5||memcmp(events,wanted,sizeof(wanted)))fail("ancestor event order/detail");
    focus(b,2);expect(b,2);
    unsigned char unmap[8]={10};w32(unmap+4,a);request(unmap,8);expect(root,0);
    focus(1,0);expect(1,0);
    expected_error=2;error_seen=0;focus(1,255);expect(1,0);if(error_seen!=1)fail("BadValue");
    expected_error=8;error_seen=0;focus(a,0);expect(1,0);if(error_seen!=1)fail("BadMatch");
    expected_error=0;focus(r32(saved+8),saved[1]);get(p);
    close(fd);puts("x11_focus_result=0 target_and_revert=1 ancestor_events=1 parent_unmap=1 pointer_root=1 bad_value=1 bad_match=1");return 0;
}
