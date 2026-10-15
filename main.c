// Ellipse: Test Chamber 01 - PSP homebrew (PSPSDK, pspgu/pspgum)
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("Ellipse", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BW 512
#define SW 480
#define SH 272
#define DT (1.0f/60.0f)
#define RGB(r,g,b) (0xFF000000u|((b)<<16)|((g)<<8)|(r))
#define VT (GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D)

static unsigned int __attribute__((aligned(16))) list[262144];
typedef struct { unsigned int c; float x, y, z; } Vtx;
typedef struct { float x1,x2,y1,y2,z1,z2; unsigned c; int pan; } Box;

#define DIRT RGB(0x4a,0x4e,0x52)
#define FLR  RGB(0x33,0x37,0x3b)
#define WHT  RGB(0xdd,0xe6,0xea)
static const Box W_[] = {
  {-5,5,-1,0,-5,5,FLR,0}, {-5,5,-1,0,-24,-12,FLR,0},            // floors
  {-6,6,5,6,-25,6,RGB(0x25,0x28,0x2c),0},                        // ceiling
  {-6,-5,-1,5,-5,6,DIRT,0}, {5,6,-1,5,-5,6,DIRT,0},             // start walls
  {-6,-5,-7,5,-12,-5,DIRT,0}, {5,6,-7,5,-12,-5,DIRT,0},         // pit walls
  {-6,-5,-1,5,-24,-12,WHT,1}, {5,6,-1,5,-24,-12,WHT,1},         // white panels
  {-6,6,-7,5,-25,-24,DIRT,0}, {-6,6,-1,5,5,6,DIRT,0},           // end walls
  {-5,5,0,3,-24,-20,RGB(0xc9,0xd3,0xd8),0},                     // ledge
  {-.4f,.4f,0,.9f,-16.4f,-15.6f,RGB(0x77,0x7d,0x83),0}          // pedestal
};
#define NB (int)(sizeof(W_)/sizeof(W_[0]))

static float px=0,py=0,pz=3,pvy=0,yaw=0,pitch=0,tt=0;
static int ground=0,hasgun=0,over=0;
static float cl(float v,float a,float b){return v<a?a:(v>b?b:v);}

// ---------- drawing ----------
static unsigned sh(unsigned c,float k){
  int r=(int)((c&255)*k),g=(int)(((c>>8)&255)*k),b=(int)(((c>>16)&255)*k);
  return RGB(r,g,b);
}
static int quad(Vtx*v,int n,const float*a,const float*b,const float*c,const float*d,unsigned col){
  const float*p[6]={a,b,c,a,c,d};
  for(int i=0;i<6;i++){v[n].c=col;v[n].x=p[i][0];v[n].y=p[i][1];v[n].z=p[i][2];n++;}
  return n;
}
static void drawbox(float x1,float x2,float y1,float y2,float z1,float z2,unsigned c){
  Vtx*v=sceGuGetMemory(36*sizeof(Vtx));int n=0;
  float a[3]={x1,y1,z1},b[3]={x2,y1,z1},cc[3]={x2,y2,z1},d[3]={x1,y2,z1};
  float e[3]={x1,y1,z2},f[3]={x2,y1,z2},g[3]={x2,y2,z2},h[3]={x1,y2,z2};
  n=quad(v,n,a,b,cc,d,sh(c,.8f)); n=quad(v,n,e,f,g,h,sh(c,.8f));
  n=quad(v,n,a,e,h,d,sh(c,.65f)); n=quad(v,n,b,f,g,cc,sh(c,.65f));
  n=quad(v,n,d,cc,g,h,sh(c,1.0f)); n=quad(v,n,a,b,f,e,sh(c,.5f));
  sceGumDrawArray(GU_TRIANGLES,VT,36,0,v);
}

// ---------- portals ----------
typedef struct { int on; float x,y,z,nx,nz; } Portal;
static Portal P[2]; // 0 = blue, 1 = orange
static const unsigned PC[2]={RGB(0x27,0xa8,0xff),RGB(0xff,0x9a,0x1f)};
static void setp(int k,float x,float y,float z,float nx,float nz){P[k].on=1;P[k].x=x;P[k].y=y;P[k].z=z;P[k].nx=nx;P[k].nz=nz;}
static void disc(const Portal*q,float s,float off,unsigned col){
  Vtx*v=sceGuGetMemory(48*sizeof(Vtx));float tx=-q->nz,tz=q->nx;
  float cx=q->x+q->nx*off,cz=q->z+q->nz*off;
  for(int i=0;i<16;i++){
    float a0=i*6.2831853f/16,a1=(i+1)*6.2831853f/16;
    v[i*3].c=col;v[i*3].x=cx;v[i*3].y=q->y;v[i*3].z=cz;
    v[i*3+1].c=col;v[i*3+1].x=cx+tx*cosf(a0)*.55f*s;v[i*3+1].y=q->y+sinf(a0)*s;v[i*3+1].z=cz+tz*cosf(a0)*.55f*s;
    v[i*3+2].c=col;v[i*3+2].x=cx+tx*cosf(a1)*.55f*s;v[i*3+2].y=q->y+sinf(a1)*s;v[i*3+2].z=cz+tz*cosf(a1)*.55f*s;
  }
  sceGumDrawArray(GU_TRIANGLES,VT,48,0,v);
}

// ---------- collision / raycast ----------
static void mv(int a,float d){
  if(a==0)px+=d;else if(a==1)py+=d;else pz+=d;
  for(int i=0;i<NB;i++){const Box*b=&W_[i];
    if(px+.3f>b->x1&&px-.3f<b->x2&&pz+.3f>b->z1&&pz-.3f<b->z2&&py+1.7f>b->y1&&py<b->y2){
      if(a==1){if(d<0){py=b->y2;ground=1;}else py=b->y1-1.7f;pvy=0;}
      else if(a==0)px=d>0?b->x1-.3f:b->x2+.3f;
      else pz=d>0?b->z1-.3f:b->z2+.3f;
    }}
}
static int rayb(const float*o,const float*d,const Box*b,float*tt_,float*n){
  float lo[3]={b->x1,b->y1,b->z1},hi[3]={b->x2,b->y2,b->z2},t0=-1e9f,t1=1e9f;
  n[0]=n[1]=n[2]=0;
  for(int a=0;a<3;a++){
    if(fabsf(d[a])<1e-8f){if(o[a]<lo[a]||o[a]>hi[a])return 0;continue;}
    float u=(lo[a]-o[a])/d[a],v=(hi[a]-o[a])/d[a];if(u>v){float s=u;u=v;v=s;}
    if(u>t0){t0=u;n[0]=n[1]=n[2]=0;n[a]=d[a]>0?-1:1;}
    if(v<t1)t1=v;
  }
  if(t1<t0||t0<0)return 0;*tt_=t0;return 1;
}
static void fire(int k){
  if(!hasgun)return;
  float cp=cosf(pitch),d[3]={-sinf(yaw)*cp,sinf(pitch),-cosf(yaw)*cp},o[3]={px,py+1.6f,pz};
  float bt=1e9f,bn[3]={0,0,0};int bi=-1;
  for(int i=0;i<NB;i++){float t,n[3];if(rayb(o,d,&W_[i],&t,n)&&t<bt){bt=t;bi=i;bn[0]=n[0];bn[1]=n[1];bn[2]=n[2];}}
  if(bi<0||!W_[bi].pan||bn[1]!=0)return;
  float hx=o[0]+d[0]*bt,hy=cl(o[1]+d[1]*bt,1,4),hz=o[2]+d[2]*bt;
  const Box*b=&W_[bi];
  if(bn[0]!=0)hz=cl(hz,b->z1+.7f,b->z2-.7f);else hx=cl(hx,b->x1+.7f,b->x2-.7f);
  setp(k,hx,hy,hz,bn[0],bn[2]);
}

// ---------- robot ----------
static float rx=0,ry=1.7f,rz=3,tx=0,ty=1.7f,tz=-1.6f;static int follow=0;

// ---------- story ----------
typedef struct { const char*t; float d; int ev; } St;
static const St ST[]={
 {"Hey... my sensors say you are alive. Unexpected.",3.8f,0},
 {"I am Companion-7, helper of the Ellipse research center. The flickering lights are normal.",4.2f,0},
 {"Two hundred years since the Great Failure. Staff evacuated. Or did not make it. Avoid the south wing.",4.8f,0},
 {"The bridge collapsed long ago. But Ellipse knew how to bend space.",3.8f,1},
 {"Watch how the portal gun works.",2.5f,0},
 {"Orange is the entrance. Blue is the exit. Simple geometry.",3.8f,2},
 {"",1.8f,3},
 {"Now you. Step into the orange portal.",-1,4},
 {"Excellent! The device is on the pedestal ahead. Take it.",-2,5},
 {"Portal device online. L = blue portal, R = orange portal.",4.5f,6},
 {"The exit is on the ledge above. Orange near you, blue high on the white wall by the ledge.",6,0}
};
#define NST (int)(sizeof(ST)/sizeof(ST[0]))
static int step=0,began=0;static float timer=0;
static void ev(int e){
  switch(e){
    case 1:tx=0;ty=1.8f;tz=-4.2f;break;
    case 2:setp(1,-5,1,-3,1,0);setp(0,-5,1,-14,1,0);break;
    case 3:tx=-4.3f;ty=1.4f;tz=-3;break;
    case 4:rx=-4.3f;ry=1.4f;rz=-14;tx=-3;ty=1.8f;tz=-15;break;
    case 5:tx=0;ty=1.9f;tz=-14.5f;break;
    case 6:follow=1;break;
  }
}
static void story(void){
  if(step>=NST)return;
  const St*s=&ST[step];
  if(!began){began=1;timer=0;ev(s->ev);}
  timer+=DT;
  int done=s->d>=0?timer>=s->d:(s->d==-1?pz<-12.5f:hasgun);
  if(done){step++;began=0;}
}
static void subtitle(const char*s){
  char buf[80];int len=(int)strlen(s),i=0,row=31;
  while(i<len&&row<34){
    int n=len-i;
    if(n>66){n=66;while(n>0&&s[i+n]!=' ')n--;if(!n)n=66;}
    memcpy(buf,s+i,n);buf[n]=0;
    pspDebugScreenSetXY(0,row++);pspDebugScreenPrintf("%-68s",buf);
    i+=n;if(s[i]==' ')i++;
  }
}

// ---------- system ----------
static int exit_cb(int a,int b,void*c){sceKernelExitGame();return 0;}
static int cb_thread(SceSize a,void*b){int id=sceKernelCreateCallback("exit",exit_cb,NULL);sceKernelRegisterExitCallback(id);sceKernelSleepThreadCB();return 0;}

int main(void){
  int t=sceKernelCreateThread("cb",cb_thread,0x11,0xFA0,0,0);if(t>=0)sceKernelStartThread(t,0,0);
  pspDebugScreenInit();pspDebugScreenSetBackColor(0xFF000000);pspDebugScreenSetTextColor(0xFFFFFFFF);
  sceGuInit();sceGuStart(GU_DIRECT,list);
  sceGuDrawBuffer(GU_PSM_8888,(void*)0,BW);sceGuDispBuffer(SW,SH,(void*)0x88000,BW);
  sceGuDepthBuffer((void*)0x110000,BW);
  sceGuOffset(2048-(SW/2),2048-(SH/2));sceGuViewport(2048,2048,SW,SH);
  sceGuDepthRange(65535,0);sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);
  sceGuDepthFunc(GU_GEQUAL);sceGuEnable(GU_DEPTH_TEST);sceGuFrontFace(GU_CW);
  sceGuShadeModel(GU_SMOOTH);sceGuDisable(GU_CULL_FACE);
  sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
  sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  SceCtrlData pad;unsigned old=0;int drawoff=0;

  for(;;){
    sceCtrlPeekBufferPositive(&pad,1);
    unsigned pr=pad.Buttons&~old;old=pad.Buttons;tt+=DT;
    if(!over){
      float lx=(pad.Lx-128)/127.0f,ly=(pad.Ly-128)/127.0f;
      if(fabsf(lx)<.22f)lx=0;if(fabsf(ly)<.22f)ly=0;
      if(pad.Buttons&PSP_CTRL_LEFT)yaw+=2.2f*DT;if(pad.Buttons&PSP_CTRL_RIGHT)yaw-=2.2f*DT;
      if(pad.Buttons&PSP_CTRL_UP)pitch=cl(pitch+1.6f*DT,-1.4f,1.4f);
      if(pad.Buttons&PSP_CTRL_DOWN)pitch=cl(pitch-1.6f*DT,-1.4f,1.4f);
      float fx=-sinf(yaw),fz=-cosf(yaw),rxv=cosf(yaw),rzv=-sinf(yaw);
      float vx=(fx*-ly+rxv*lx)*4.2f,vz=(fz*-ly+rzv*lx)*4.2f;
      pvy-=18*DT;if((pr&PSP_CTRL_CROSS)&&ground)pvy=6.3f;
      mv(0,vx*DT);mv(2,vz*DT);ground=0;mv(1,pvy*DT);
      if(py<-3){px=0;py=0;pz=3;pvy=0;yaw=0;}
      if(pr&PSP_CTRL_LTRIGGER)fire(0);
      if(pr&PSP_CTRL_RTRIGGER)fire(1);
      static float cd=0;
      if(cd>0)cd-=DT;
      else for(int k=0;k<2;k++){
        const Portal*A=&P[k],*B=&P[1-k];if(!A->on||!B->on)continue;
        float dx=px-A->x,dy=py+1-A->y,dz=pz-A->z;
        float al=dx*A->nx+dz*A->nz,lat=fabsf(-dx*A->nz+dz*A->nx);
        if(al<.5f&&al>-.3f&&lat<.6f&&fabsf(dy)<1.1f&&vx*A->nx+vz*A->nz<-.5f){
          px=B->x+B->nx*.6f;pz=B->z+B->nz*.6f;py=B->y-1;yaw=atan2f(-B->nx,-B->nz);cd=.7f;break;}
      }
      if(!hasgun&&hypotf(px,pz+16)<1.4f&&py<1){hasgun=1;P[0].on=P[1].on=0;}
      if(hasgun&&py>2.5f&&pz<-22.5f)over=1;
      story();
      if(follow){tx=px+fx*2+rxv*1.2f;ty=py+1.7f;tz=pz+fz*2+rzv*1.2f;}
    }
    rx+=(tx-rx)*.05f;ry+=(ty-ry)*.05f+sinf(tt*3)*.004f;rz+=(tz-rz)*.05f;

    sceGuStart(GU_DIRECT,list);
    sceGuClearColor(0xFF0F0C0A);sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_FOG);sceGuFog(2.0f,28.0f,0xFF0F0C0A);
    sceGumMatrixMode(GU_PROJECTION);sceGumLoadIdentity();sceGumPerspective(70.0f,(float)SW/SH,0.1f,60.0f);
    sceGumMatrixMode(GU_VIEW);sceGumLoadIdentity();
    ScePspFVector3 ps={-px,-(py+1.6f),-pz};
    sceGumRotateX(-pitch);sceGumRotateY(-yaw);sceGumTranslate(&ps);
    sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
    for(int i=0;i<NB;i++)drawbox(W_[i].x1,W_[i].x2,W_[i].y1,W_[i].y2,W_[i].z1,W_[i].z2,W_[i].c);
    drawbox(-5,5,-5.1f,-5,-12,-5,RGB(0x33,0xaa,0x44));          // toxic goo
    drawbox(-1,1,3,5,-24.05f,-23.95f,RGB(0x44,0xff,0x88));      // exit door
    if(!hasgun){float gy=1.2f+sinf(tt*2)*.08f;drawbox(-.08f,.08f,gy,gy+.15f,-16.25f,-15.75f,RGB(255,255,255));}
    drawbox(rx-.25f,rx+.25f,ry-.25f,ry+.25f,rz-.25f,rz+.25f,RGB(0xcf,0xd5,0xda)); // robot body
    float ex=px-rx,ez=pz-rz,el=sqrtf(ex*ex+ez*ez)+1e-4f;ex/=el;ez/=el;
    drawbox(rx+ex*.28f-.08f,rx+ex*.28f+.08f,ry-.08f,ry+.08f,rz+ez*.28f-.08f,rz+ez*.28f+.08f,RGB(0x44,0xdd,0xff));
    for(int k=0;k<2;k++)if(P[k].on){disc(&P[k],1.0f,.03f,PC[k]);disc(&P[k],.78f,.05f,sh(PC[k],.35f));}
    sceGuFinish();sceGuSync(0,0);

    pspDebugScreenSetOffset(drawoff);
    if(over){pspDebugScreenSetXY(20,15);pspDebugScreenPrintf(" TEST CHAMBER 01 COMPLETE ");}
    else if(step<NST&&ST[step].t[0]){char b[160];snprintf(b,sizeof b,"COMPANION-7: %s",ST[step].t);subtitle(b);}
    sceDisplayWaitVblankStart();sceGuSwapBuffers();drawoff=drawoff?0:0x88000;
  }
  return 0;
}
