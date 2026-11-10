// HL4 WIPI host. New implementation based on HL4 evidence in docs/RUNTIME_CONTRACT.md.
// Unicorn executes original ARM/Thumb; guarded HL4 localization boundaries.
#define SDL_MAIN_HANDLED
#define NOMINMAX
#include <SDL.h>
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "audio/stb_vorbis.c"
#undef L
#undef C
#undef R
using u32=uint32_t;using u64=uint64_t;using s32=int32_t;using Bytes=std::vector<uint8_t>;
namespace fs=std::filesystem;
constexpr u32 BASE=0x02000000, TEMP=0x04000000, TRAP=0x05000000, RET=0x0500f000, HEAP=0x06000000;
static void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static Bytes readFile(const fs::path& p){auto f=SDL_RWFromFile(p.generic_u8string().c_str(),"rb");require(f!=nullptr,"Cannot open "+p.string()+": "+SDL_GetError());auto n=SDL_RWsize(f);if(n<0||n>128*1024*1024){SDL_RWclose(f);throw std::runtime_error("Resource size");}Bytes out(size_t(n),0);bool ok=SDL_RWread(f,out.data(),1,out.size())==out.size();SDL_RWclose(f);require(ok,"Resource read");return out;}
#include "sha256.inc"
#include "platform_paths.inc"
#pragma pack(push,1)
struct Ehdr{uint8_t id[16];uint16_t type,machine;u32 version,entry,phoff,shoff,flags;uint16_t ehsize,phsize,phnum,shsize,shnum,shstr;};
struct Phdr{u32 type,off,va,pa,filesz,memsz,flags,align;};
struct Shdr{u32 name,type,flags,va,off,size,link,info,align,entsize;};
struct Sym{u32 name,value,size;uint8_t info,other;uint16_t section;};
struct Rel{u32 offset,info;};
#pragma pack(pop)
template<class T>static T structure(const Bytes& b,size_t at){require(at<=b.size()&&sizeof(T)<=b.size()-at,"Truncated file structure");T value;memcpy(&value,b.data()+at,sizeof(T));return value;}
struct Frame{int w,h;u32 pixels;};
struct Timer{u32 callback=0,user=0;u64 due=0;bool armed=false;};
#include "audio_cache.inc"
struct Clip{Pcm pcm;size_t pos=0;bool playing=false,loop=false;float volume=1;std::string name;};
#include "arm_hot_decoder.inc"
#ifdef __SWITCH__
#include "switch_perf.inc"
#endif
#include "emulation_watchdog.inc"
class Runtime{
public:
 bool booting=true;
 fs::path profilePath;std::vector<u64> profileHits,profileBytes;
 static void profileBlock(uc_engine*,u64 address,u32 size,void* opaque){auto& r=*(Runtime*)opaque;if(address>=BASE&&address<BASE+0x200000){size_t i=size_t(address-BASE)/4;r.profileHits[i]++;r.profileBytes[i]+=size;}}
 void enableProfile(const fs::path& path){profilePath=path;profileHits.resize(0x80000);profileBytes.resize(0x80000);uc_hook hook;check(uc_hook_add(uc,&hook,UC_HOOK_BLOCK,(void*)profileBlock,this,BASE,BASE+0x1fffff));}
 void dumpProfile(){if(profilePath.empty())return;std::ofstream out(profilePath);out<<"va,hits,block_bytes\n";for(size_t i=0;i<profileHits.size();i++)if(profileHits[i])out<<i*4<<","<<profileHits[i]<<","<<profileBytes[i]<<"\n";}

 bool legacyWatchdog=false;std::unique_ptr<EmulationWatchdog> watchdog;
 uc_engine* uc=nullptr;Bytes mem=Bytes(0x0a000000);std::map<std::string,Bytes> data;std::map<std::string,u32> exports;
 std::vector<std::string> imports;std::map<u32,u32> freeblocks,allocations;std::map<u32,Frame> frames;
 std::map<u32,std::map<u32,u32>> contexts;std::map<u32,Timer> timers;std::map<u32,Clip> clips;
 std::vector<std::string> resourceNames;std::map<std::string,u32> resourceIds;std::map<std::string,int> seen;
 std::map<int,FILE*> files;fs::path saveRoot;int nextFile=1;u32 nextFrame=1,nextClip=1,screen=0,lastFrame=0,callbacks=0;
 u64 flushes=0,start=SDL_GetTicks64();bool returned=false,quit=false;std::atomic<bool> mute{false};std::string failure;
 std::deque<std::pair<int,bool>> pendingKeys;
 struct PointerEvent{u32 phase,x,y;};std::deque<PointerEvent> pendingTouches;
 bool guestPointerDown=false;u32 pointerX=0,pointerY=0;
 std::map<int,int> translatedKeys;
 struct Deferred{u64 due;u32 address,status,user;};std::vector<Deferred> deferred;
 bool invalidFormat=false,deterministic=false;u64 testClock=0;
 SDL_AudioDeviceID audio=0;std::atomic<u64> nonzero{0};
 explicit Runtime(const fs::path& archive,const fs::path& saves,bool testMode=false,const std::string& language="ko",int canvasWidth=240,int canvasHeight=400):saveRoot(saves),deterministic(testMode){
  auto bytes=readFile(archive);require(bytes.size()>=12&&memcmp(bytes.data(),"HL4DATA1",8)==0,"Invalid data archive");
  size_t at=12;u32 count=structure<u32>(bytes,8);require(count<10000,"Archive count");
  for(u32 i=0;i<count;i++){
   u32 n=structure<u32>(bytes,at),size=structure<u32>(bytes,at+4);at+=8;
   require(n>0&&n<1024&&at+n+32<=bytes.size(),"Archive name");std::string name((char*)bytes.data()+at,n);at+=n;
   std::array<uint8_t,32> expected;memcpy(expected.data(),bytes.data()+at,32);at+=32;
   require(size<=bytes.size()-at,"Archive payload");Bytes blob(bytes.begin()+at,bytes.begin()+at+size);at+=size;
   require(hash(blob)==expected,"Data checksum mismatch: "+name);require(data.emplace(name,std::move(blob)).second,"Duplicate resource");
  }require(at==bytes.size(),"Archive trailing bytes");
  for(auto& [name,blob]:data)if(name!="@elf"&&name!="binary.mod"){resourceNames.push_back(name);resourceIds[name]=(u32)resourceNames.size();}
  bootStage("resources verified; opening ARM engine");
  check(uc_open(UC_ARCH_ARM,UC_MODE_ARM,&uc));watchdog=std::make_unique<EmulationWatchdog>(uc);
#if defined(__SWITCH__) || defined(HL4_TCI)
  // QEMU defaults to a 1 GiB buffer on 64-bit hosts. Set before lazy init/map.
  check(uc_ctl_set_tcg_buffer_size(uc,32u*1024u*1024u));
  bootStage("TCI cache limited to 32 MiB; mapping guest memory");
#endif
  for(auto [a,n]:std::vector<std::pair<u32,u32>>{{BASE,0x200000},{TEMP,0x100000},{TRAP,0x10000},{HEAP,0x4000000}})
   check(uc_mem_map_ptr(uc,a,n,UC_PROT_ALL,mem.data()+a));
  freeblocks[HEAP]=0x4000000;load(data.at("@elf"));screen=newFrame(canvasWidth,canvasHeight);lastFrame=screen;
  uc_hook hook;check(uc_hook_add(uc,&hook,UC_HOOK_CODE,(void*)onTrap,this,TRAP,TRAP+0xffff));
  bootStage("ARM memory and module ready; loading language");
  if(canvasWidth!=240||canvasHeight!=400){
   // HL4 touch_setUiChange subtracts m_TouchDirectKeySize from the physical
   // canvas to obtain the world viewport. Its initializer otherwise assigns
   // all extra horizontal pixels to that panel, keeping the world at 320px.
   u32 entry=exports.at("touch_setUiChange");
   require(read(entry)==0xe92d45f0&&read(entry+4)==0xe59f5468,"HL4 viewport entry guard");
   require(exports.at("m_TouchDirectKeySize")==BASE+0xf13f0,"HL4 panel symbol guard");
   uc_hook vh;check(uc_hook_add(uc,&vh,UC_HOOK_CODE,(void*)+[](uc_engine*,u64,u32,void* opaque){
    auto& r=*(Runtime*)opaque;u32 panel=r.exports.at("m_TouchDirectKeySize");
    // Zero means the game intentionally hides the panel (menus/cutscenes).
    if(r.read(panel)>0)r.write(panel,80);
   },this,entry,entry));
  }
  loadLanguage(archive.parent_path()/"hl4.lang",language);
  fs::create_directories(saveRoot);
  SDL_AudioSpec want{},got{};want.freq=48000;want.channels=2;want.format=AUDIO_F32SYS;want.samples=1024;want.callback=mix;want.userdata=this;
  audio=SDL_OpenAudioDevice(nullptr,0,&want,&got,0);require(audio!=0,std::string("Audio device: ")+SDL_GetError());SDL_PauseAudioDevice(audio,0);
 }
 ~Runtime(){dumpProfile();if(oracle)uc_close(oracle);if(audio)SDL_CloseAudioDevice(audio);for(auto [id,f]:files)fclose(f);watchdog.reset();if(uc)uc_close(uc);}
 void check(uc_err e){require(e==UC_ERR_OK,std::string("ARM: ")+uc_strerror(e));}
 uint8_t* ptr(u32 a,size_t n=1){bool valid=(a>=BASE&&a+u64(n)<=BASE+0x200000)||(a>=TEMP&&a+u64(n)<=TEMP+0x100000)||(a>=TRAP&&a+u64(n)<=TRAP+0x10000)||(a>=HEAP&&a+u64(n)<=HEAP+0x4000000);require(valid,"Guest access "+std::to_string(a)+" bytes "+std::to_string(n));return mem.data()+a;}
 u32 read(u32 a){u32 v;memcpy(&v,ptr(a,4),4);return v;}void write(u32 a,u32 v){memcpy(ptr(a,4),&v,4);}
 u32 reg(int r){u32 v=0;check(uc_reg_read(uc,r,&v));return v;}void set(int r,u32 v){check(uc_reg_write(uc,r,&v));}
 u32 arg(int i){return i<4?reg(UC_ARM_REG_R0+i):read(reg(UC_ARM_REG_SP)+(i-4)*4);}
 std::string str(u32 a){std::string v;for(size_t i=0;i<65536;i++){char c=*ptr(a+(u32)i);if(!c)return v;v+=c;}throw std::runtime_error("Unterminated guest string");}
 void string(u32 a,const std::string& v){memcpy(ptr(a,v.size()+1),v.c_str(),v.size()+1);}
 u32 alloc(u32 size){require(size<=0x4000000,"Huge allocation");size=(std::max(1u,size)+7)&~7u;for(auto [a,n]:freeblocks)if(n>=size){freeblocks.erase(a);if(n>size)freeblocks[a+size]=n-size;allocations[a]=size;memset(ptr(a,size),0,size);return a;}return 0;}
 void free(u32 a){if(!a)return;auto it=allocations.find(a);require(it!=allocations.end(),"Invalid guest free "+std::to_string(a));freeblocks[a]=it->second;allocations.erase(it);for(auto p=freeblocks.begin();p!=freeblocks.end();){auto next=std::next(p);if(next!=freeblocks.end()&&p->first+p->second==next->first){p->second+=next->second;freeblocks.erase(next);}else ++p;}}
 u32 newFrame(int w,int h){require(w>0&&h>0&&w<=4096&&h<=4096,"Framebuffer dimensions");u32 id=nextFrame++,p=alloc(w*h*2);require(p!=0,"Framebuffer allocation");frames[id]={w,h,p};return id;}
 u64 now(){return deterministic?testClock:SDL_GetTicks64()-start;}u32 wide(u64 v){set(UC_ARM_REG_R1,u32(v>>32));return u32(v);}
 void load(const Bytes& b){
  Ehdr e=structure<Ehdr>(b,0);require(memcmp(e.id,"\x7f" "ELF\1\1",6)==0&&e.machine==40&&e.type==3,"Expected ARM ELF32 ET_DYN");
  require(e.phsize==sizeof(Phdr)&&e.shsize==sizeof(Shdr),"ELF table stride");
  std::vector<std::pair<u32,u32>> ranges;
  for(int i=0;i<e.phnum;i++){auto p=structure<Phdr>(b,e.phoff+i*sizeof(Phdr));if(p.type!=1)continue;require(p.filesz<=p.memsz&&p.off+u64(p.filesz)<=b.size()&&p.va+u64(p.memsz)<=0x200000,"Invalid LOAD");memcpy(ptr(BASE+p.va,p.memsz),b.data()+p.off,p.filesz);memset(ptr(BASE+p.va+p.filesz,p.memsz-p.filesz),0,p.memsz-p.filesz);ranges.push_back({p.va,p.va+p.memsz});}
  std::vector<Shdr> sections;for(int i=0;i<e.shnum;i++)sections.push_back(structure<Shdr>(b,e.shoff+i*sizeof(Shdr)));
  auto dyn=std::find_if(sections.begin(),sections.end(),[](auto s){return s.type==11;});require(dyn!=sections.end()&&dyn->link<sections.size(),"Missing dynsym");
  auto strings=sections[dyn->link];require(strings.off+u64(strings.size)<=b.size(),"dynstr bounds");
  std::vector<Sym> syms;std::vector<std::string> names;std::vector<u32> addresses;
  for(u32 at=0;at<dyn->size;at+=sizeof(Sym)){auto s=structure<Sym>(b,dyn->off+at);require(s.name<strings.size,"Symbol name offset");auto begin=b.begin()+strings.off+s.name;auto end=std::find(begin,b.begin()+strings.off+strings.size,0);require(end!=b.begin()+strings.off+strings.size,"Symbol name termination");std::string n(begin,end);syms.push_back(s);names.push_back(n);u32 a=0;if(s.section){a=BASE+s.value;exports[n]=a;}else if(!n.empty()){a=TRAP+(u32)imports.size()*4;imports.push_back(n);}addresses.push_back(a);}
  for(auto s:sections)if(s.type==9){require(s.entsize==8,"REL stride");for(u32 at=0;at<s.size;at+=8){auto r=structure<Rel>(b,s.off+at);u32 type=r.info&255,index=r.info>>8;require(index<syms.size(),"REL symbol index");require(std::any_of(ranges.begin(),ranges.end(),[&](auto p){return r.offset>=p.first&&u64(r.offset)+4<=p.second;}),"REL target");u32 where=BASE+r.offset;if(type==23){require(index==0,"RELATIVE symbol");write(where,read(where)+BASE);}else if(type==21||type==22)write(where,addresses[index]);else throw std::runtime_error("Unsupported relocation "+std::to_string(type));}}
 }
 static void onTrap(uc_engine*,u64 address,u32,void* opaque){auto& r=*(Runtime*)opaque;try{if(address==RET){r.restoreText(RET);r.returned=true;uc_emu_stop(r.uc);return;}require(address>=TRAP&&(address-TRAP)%4==0,"Unaligned trap");size_t i=(size_t(address)-TRAP)/4;require(i<r.imports.size(),"Unknown trap");u32 value=r.service(r.imports[i]);r.set(UC_ARM_REG_R0,value);r.set(UC_ARM_REG_PC,r.reg(UC_ARM_REG_LR));}catch(const std::exception& e){r.failure=std::string(e.what())+" [import="+(address>=TRAP&&(address-TRAP)/4<r.imports.size()?r.imports[(address-TRAP)/4]:"unknown")+" LR_VA="+std::to_string(r.reg(UC_ARM_REG_LR)-BASE)+" r0="+std::to_string(r.reg(UC_ARM_REG_R0))+" r1="+std::to_string(r.reg(UC_ARM_REG_R1))+" r2="+std::to_string(r.reg(UC_ARM_REG_R2))+"]";uc_emu_stop(r.uc);}}
 u32 call(u32 address,std::initializer_list<u32> args={}){
  returned=false;failure.clear();int i=0;for(u32 a:args){if(i<4)set(UC_ARM_REG_R0+i,a);else write(TEMP+0xff000+4*(i-4),a);i++;}set(UC_ARM_REG_SP,TEMP+0xff000);set(UC_ARM_REG_LR,RET);
#if defined(__SWITCH__) || defined(HL4_TCI)
  // The interpreter can spend >5 seconds in original DES initialization on Switch.
  const u64 timeoutUs=booting?120000000:30000000;
#else
  const u64 timeoutUs=5000000;
#endif
#ifdef __SWITCH__
  if(booting)printf("boot ARM entry_va=0x%x starting (budget %llu us)\n",address-BASE,(unsigned long long)timeoutUs);
#endif
  const u64 started=SDL_GetTicks64();
  if(!legacyWatchdog)watchdog->arm(timeoutUs);
  auto result=uc_emu_start(uc,address,RET+4,legacyWatchdog?timeoutUs:0,0);bool hostTimedOut=legacyWatchdog?false:watchdog->disarm();
  if(legacyWatchdog){size_t expired=0;check(uc_query(uc,UC_QUERY_TIMEOUT,&expired));hostTimedOut=expired!=0;}
  if(!failure.empty())throw std::runtime_error(failure);check(result);
  if(!returned){bool timedOut=hostTimedOut;std::ostringstream message;message<<"ARM "<<(timedOut?"execution timeout":"stopped before return")<<" entry_va=0x"<<std::hex<<(address-BASE)<<" pc_va=0x"<<(reg(UC_ARM_REG_PC)-BASE)<<std::dec<<" budget_us="<<timeoutUs<<" elapsed_ms="<<(SDL_GetTicks64()-started);throw std::runtime_error(message.str());}
#ifdef __SWITCH__
  if(booting)printf("boot ARM entry_va=0x%x completed in %llu ms (budget %llu us)\n",address-BASE,(unsigned long long)(SDL_GetTicks64()-started),(unsigned long long)timeoutUs);
#endif
  return reg(UC_ARM_REG_R0);
 }
 u32 call(const std::string& name,std::initializer_list<u32> args={}){return call(exports.at(name),args);}
 void boot(){call("_init_module",{TEMP,TEMP+0x1000,TEMP+0x2000});call("init");require(callbacks!=0,"No Clet callbacks");call(read(callbacks));booting=false;}
 int hostKey(int code){
  // The original title's two consent buttons use numeric/touch area codes,
  // while rotated phone key '2' is remapped to Right. Route logical host
  // Confirm/Back through the original buttons, without changing guest state.
  bool consent=read(exports.at("nBASE_KeyPressed"))==exports.at("TitleFrame_KeyPressed")&&int8_t(*ptr(exports.at("_gameMenu")+0x21))==-1&&*ptr(exports.at("_bSmsShow"))==0;
  if(consent){if(code==53)return 49;if(code==50||code==-7||code==-16)return 0x3b38;}
  if(read(exports.at("nTouchRotate"))!=0){if(code==-1)return -3;if(code==-2)return -4;if(code==-3)return -2;if(code==-4)return -1;}
  return code;
 }
 void key(int code,bool down){int translated=hostKey(code);if(down)translatedKeys[code]=translated;else{auto it=translatedKeys.find(code);if(it!=translatedKeys.end()){translated=it->second;translatedKeys.erase(it);}}pendingKeys.push_back({translated,down});}
 void touch(u32 phase,u32 x,u32 y){if(phase==2&&!pendingTouches.empty()&&pendingTouches.back().phase==2)pendingTouches.back()={phase,x,y};else pendingTouches.push_back({phase,x,y});}
 void cancelTouch(){pendingTouches.clear();if(guestPointerDown)touch(1,pointerX,pointerY);}
 void tick(){for(size_t i=0;i<deferred.size();){if(deferred[i].due>now()){i++;continue;}auto d=deferred[i];deferred.erase(deferred.begin()+i);call(d.address,{d.status,d.user});}std::vector<u32> due;for(auto& [id,t]:timers)if(t.armed&&t.due<=now())due.push_back(id);for(u32 id:due){auto t=timers.at(id);timers[id].armed=false;
  if(t.callback==exports.at("timerCB")){
   // One pointer transition per original tick: quick down/up must not overwrite
   // the guest's single pending touch/key slot before it processes the press.
   if(!pendingTouches.empty()){auto e=pendingTouches.front();pendingTouches.pop_front();pointerX=e.x;pointerY=e.y;guestPointerDown=e.phase!=1;write(TEMP+0x3000,e.x);write(TEMP+0x3004,e.y);call(read(callbacks+20),{0x708,e.phase,TEMP+0x3000});if(deterministic)printf("pointer phase=%u guest=(%u,%u) key=%d\n",e.phase,e.x,e.y,s32(read(exports.at("mainFrame")+0x30)));}
   bool pressed=false,released=false;while(!pendingKeys.empty()){auto [code,down]=pendingKeys.front();if(down?pressed:released)break;pendingKeys.pop_front();call(read(callbacks+20),{down?502u:503u,u32(code),0});auto cb=read(exports.at("nBASE_KeyPressed"));std::string cbname;for(auto& [name,a]:exports)if(a==cb){cbname=name;break;}if(deterministic)printf("input code=%d down=%d stored=%d rotate=%u callback=%s\n",code,down,s32(read(exports.at("mainFrame")+(down?0x30:0x34))),read(exports.at("nTouchRotate")),cbname.c_str());if(down)pressed=true;else released=true;}}
  call(t.callback,{id,t.user});}}
 std::string format(u32 address,int index){invalidFormat=false;std::string fmt=str(address),out;for(size_t i=0;i<fmt.size();i++){if(fmt[i]!='%'){out+=fmt[i];continue;}size_t start=i++;if(i>=fmt.size()||fmt.substr(i)=="0"){invalidFormat=true;break;}if(fmt[i]=='%'){out+='%';continue;}while(i<fmt.size()&&strchr("-+ #0.123456789",fmt[i]))i++;require(i<fmt.size()&&strchr("sdiuxXcp",fmt[i]),"Unsupported guest format: "+fmt);std::string f=fmt.substr(start,i-start+1);u32 value=arg(index++);char tmp[1024];int n=fmt[i]=='s'?snprintf(tmp,sizeof(tmp),f.c_str(),str(value).c_str()):fmt[i]=='p'?snprintf(tmp,sizeof(tmp),"0x%x",value):snprintf(tmp,sizeof(tmp),f.c_str(),value);require(n>=0&&n<(int)sizeof(tmp),"Guest format overflow");out.append(tmp,n);}return out;}
 fs::path filePath(const std::string& name){std::string clean=name;std::replace(clean.begin(),clean.end(),'\\','/');while(!clean.empty()&&clean[0]=='/')clean.erase(clean.begin());fs::path p=clean;require(!p.empty()&&!p.is_absolute()&&!p.has_root_name(),"Invalid save path");for(const auto& part:p)require(part!="..","Save path traversal");return saveRoot/p;}
 static void mix(void* opaque,Uint8* bytes,int n){auto& r=*(Runtime*)opaque;memset(bytes,0,n);auto out=(float*)bytes;int count=n/4;for(auto& [id,c]:r.clips)if(c.playing&&c.pcm&&!c.pcm->empty())for(int i=0;i<count;i++){if(c.pos==c.pcm->size()){if(c.loop)c.pos=0;else{c.playing=false;break;}}out[i]+=(*c.pcm)[c.pos++]*c.volume;}
  bool muted=r.mute.load(std::memory_order_relaxed);u64 nz=0;for(int i=0;i<count;i++){out[i]=muted?0:std::clamp(out[i],-1.f,1.f);nz+=out[i]!=0;}r.nonzero.fetch_add(nz,std::memory_order_relaxed);
 }
 #include "audio_runtime.inc"
 u32 service(const std::string& n);
#include "localized_text.inc"
#include "arm_hot_bridge.inc"
};

u32 Runtime::service(const std::string& n){
 u32 x=arg(0),y=arg(1),z=arg(2),t=arg(3);if(++seen[n]<=2&&(booting||deterministic)){printf("%s(%x,%x,%x,%x) lr=%x\n",n.c_str(),x,y,z,t,reg(UC_ARM_REG_LR)-BASE);fflush(stdout);}
 if(n=="memset"||n=="dlib_memset"){if(z)memset(ptr(x,z),y&255,z);return x;}
 if(n=="memcpy"||n=="dlib_memcpy"||n=="memmove"||n=="dlib_memmove"){if(z)memmove(ptr(x,z),ptr(y,z),z);return x;}
 if(n=="dlib_strlen")return (u32)str(x).size();
 if(n=="dlib_strcpy"){string(x,str(y));return x;}
 if(n=="dlib_strncpy"){if(!z)return x;auto s=str(y);memset(ptr(x,z),0,z);memcpy(ptr(x,z),s.data(),std::min<size_t>(z,s.size()));return x;}
 if(n=="dlib_strcmp")return (s32)str(x).compare(str(y));
 if(n=="dlib_strncmp"){if(!z)return 0;auto a=str(x).substr(0,z),b=str(y).substr(0,z);return (s32)a.compare(b);}
 if(n=="dlib_memcmp")return z?memcmp(ptr(x,z),ptr(y,z),z):0;
 if(n=="dlib_strstr"){auto i=str(x).find(str(y));return i==std::string::npos?0:x+(u32)i;}
 if(n=="MC_knlSprintk"){try{auto s=format(y,2);string(x,s);if(invalidFormat){if(seen[n]==3)printf("Invalid trailing format conversion: returning error with formatted prefix.\n");return u32(-1);}return (u32)s.size();}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" Sprintk fmt VA="+std::to_string(y-BASE)+" LR="+std::to_string(reg(UC_ARM_REG_LR)-BASE));}}
 if(n=="MC_knlPrintk"){auto s=str(x);printf("guest: %s\n",s.c_str());return 0;}
 if(n=="MC_knlAlloc"||n=="MC_knlCalloc")return alloc(x);
 if(n=="MC_knlFree"){free(x);return 0;}
 if(n=="MC_knlGetTotalMemory")return 0x4000000;
 if(n=="MC_knlGetFreeMemory"){u32 size=0;for(auto [a,n]:freeblocks)size+=n;return size;}
 if(n=="dlet_set_property"){require(x==0&&(y==100||y==101)&&t==0,"Unknown window property");if(z)require(z==(y==100?240u:400u),"Unsupported native window size");return 0;}
 if(n=="wipic_event_handler"){require(y==0,"Unknown WIPI handler mode");callbacks=x;return 0;}
 if(n=="MC_knlGetSystemProperty"){auto key=str(x);std::string value;if(key=="PHONEMODEL")value="PC";else if(key=="TIMEZONE")value="GMT-0300";else if(key=="DATA_ROAMING_LOCKED"||key=="DS_LOCK"||key=="AIRPLANE_MODE"||key=="ROAMING_AREA"||key=="PHONENUMBER"){if(z)*ptr(y)=0;return u32(-1);}else throw std::runtime_error("Unknown property "+key);if(value.size()+1>z)return u32(-1);string(y,value);return 0;}
 // Device-only optional effects. Desktop has no handset backlight/vibrator.
 if(n=="MC_miscBackLight"||n=="MC_mdaVibrator")return 0;
 if(n=="MC_mdaSetMuteState"){mute=y==0;return 0;} // WIPI enabled flag: startClet passes (0,1)
 if(n=="MC_knlCurrentTime")return wide(now());
 if(n=="MC_knlDefTimer"){timers[x]={y};return 0;}
 if(n=="MC_knlSetTimer"){auto& timer=timers.at(x);timer.armed=true;timer.due=now()+(u64(z)|(u64(t)<<32));timer.user=arg(4);return 0;} // r1 is AAPCS alignment padding before uint64 timeout
 if(n=="MC_knlUnsetTimer"){timers.at(x).armed=false;return 0;}
 if(n=="MC_knlGetResourceID"){auto name=str(x);auto it=resourceIds.find(name);if(it==resourceIds.end()){printf("missing resource %s\n",name.c_str());return u32(-12);}if(y)write(y,(u32)data.at(name).size());return it->second;}
 if(n=="MC_knlGetResource"){if(x==0||x>resourceNames.size())return u32(-12);auto& b=data.at(resourceNames[x-1]);if(z<b.size())return u32(-18);memcpy(ptr(y,b.size()),b.data(),b.size());return 0;}
 if(n=="MC_fsFileAttribute"){auto p=filePath(str(x));if(!fs::is_regular_file(p))return u32(-1);memset(ptr(y,16),0,16);write(y+8,(u32)fs::file_size(p));return 0;}
 if(n=="MC_fsOpen"){auto p=filePath(str(x));require(y==1||y==4||y==8,"Unknown file mode "+std::to_string(y));if(y==4)fs::create_directories(p.parent_path());FILE* f=openSave(p,int(y));if(!f)return u32(-1);int id=nextFile++;files[id]=f;return id;}
 if(n=="MC_fsClose"){auto f=files.at(x);files.erase(x);return fclose(f)==0?0:u32(-1);}
 if(n=="MC_fsSeek"){require(z<=2,"File seek origin");auto f=files.at(x);if(seekSave(f,s32(y),z)!=0)return u32(-1);return u32(tellSave(f));}
 if(n=="MC_fsRead")return (u32)fread(ptr(y,z),1,z,files.at(x));
 if(n=="MC_fsWrite"){auto f=files.at(x);u32 count=(u32)fwrite(ptr(y,z),1,z,f);fflush(f);return count;}
 if(n=="MC_fsRemove"){std::error_code e;bool removed=fs::remove(filePath(str(x)),e);return removed&&!e?0:u32(-1);}
 if(n=="MC_grpGetScreenFrameBuffer"){require(x==0,"Display index");return screen;}
 if(n=="MC_grpCreateOffScreenFrameBuffer")return newFrame(x,y);
 if(n=="MC_grpDestroyOffScreenFrameBuffer"){free(frames.at(x).pixels);frames.erase(x);return 0;}
 if(n.rfind("wipic_get_frame_",0)==0){auto f=frames.at(x);if(n=="wipic_get_frame_width")return f.w;if(n=="wipic_get_frame_height")return f.h;if(n=="wipic_get_frame_pointer")return f.pixels;}
 if(n=="MC_grpInitContext"){contexts[x]={{1,0xffff},{2,0}};return 0;}
 if(n=="MC_grpSetContext"){require(y==1||y==2,"Unmapped graphic context property "+std::to_string(y));contexts.at(x)[y]=z;return 0;}
 if(n=="MC_grpGetContext")return contexts.at(x).at(y);
 if(n=="MC_grpGetPixelFromRGB")return ((x>>3)<<11)|((y>>2)<<5)|(z>>3);
 if(n=="MC_grpGetRGBFromPixel"){write(y,((x>>11)&31)*255/31);write(z,((x>>5)&63)*255/63);write(t,(x&31)*255/31);return 0;}
 if(n=="MC_grpFillRect"){int height=arg(4);u32 context=arg(5);auto f=frames.at(x);auto pixels=(uint16_t*)ptr(f.pixels,f.w*f.h*2);uint16_t color=(uint16_t)contexts.at(context).at(1);int left=std::clamp(s32(y),0,f.w),right=std::clamp(s32(y)+s32(t),0,f.w);for(int row=std::max(0,s32(z));row<std::min(f.h,s32(z)+height);row++)if(right>left)std::fill(pixels+row*f.w+left,pixels+row*f.w+right,color);return 0;}
 if(n=="MC_grpDrawRect"||n=="MC_grpDrawLine"){
  auto f=frames.at(x);auto pixels=(uint16_t*)ptr(f.pixels,f.w*f.h*2);uint16_t color=(uint16_t)contexts.at(arg(5)).at(1);
  auto line=[&](int ax,int ay,int bx,int by){int dx=abs(bx-ax),sx=ax<bx?1:-1,dy=-abs(by-ay),sy=ay<by?1:-1,err=dx+dy;require(dx<=8192&&-dy<=8192,"Line bounds");for(;;){if(ax>=0&&ax<f.w&&ay>=0&&ay<f.h)pixels[ay*f.w+ax]=color;if(ax==bx&&ay==by)break;int e=2*err;if(e>=dy){err+=dy;ax+=sx;}if(e<=dx){err+=dx;ay+=sy;}}};
  if(n=="MC_grpDrawLine")line(s32(y),s32(z),s32(t),s32(arg(4)));else{int a=s32(y),b=s32(z),w=s32(t),h=s32(arg(4));if(w>=0&&h>=0){line(a,b,a+w,b);line(a,b+h,a+w,b+h);line(a,b,a,b+h);line(a+w,b,a+w,b+h);}}return 0;
 }
 if(n=="MC_grpFlushLcd"){require(frames.count(y)!=0,"Flush framebuffer");lastFrame=y;flushes++;return 0;}
 if(n=="MC_mdaClipCreate"){u32 id=nextClip++;SDL_LockAudioDevice(audio);clips[id]=Clip{};SDL_UnlockAudioDevice(audio);return id;}
 if(n=="MC_mdaClipAllocPlayer")return loadAudio(x,str(y));
 if(n=="MC_mdaPlay"){SDL_LockAudioDevice(audio);auto& c=clips.at(x);c.pos=0;c.playing=c.pcm&&!c.pcm->empty();c.loop=y!=0;bool ready=c.playing;SDL_UnlockAudioDevice(audio);return ready?0:u32(-1);}
 if(n=="MC_mdaStop"||n=="MC_mdaClipClearData"){SDL_LockAudioDevice(audio);auto& c=clips.at(x);c.playing=false;c.pos=0;SDL_UnlockAudioDevice(audio);return 0;}
 if(n=="MC_mdaClipFree"){SDL_LockAudioDevice(audio);clips.erase(x);SDL_UnlockAudioDevice(audio);return 0;}
 if(n=="MC_mdaClipSetVolume"){SDL_LockAudioDevice(audio);clips.at(x).volume=std::clamp(s32(y)/100.f,0.f,1.f);SDL_UnlockAudioDevice(audio);return 0;}
 if(n=="MC_knlExit"){quit=true;return 0;}
 if(n=="MC_netConnect"){printf("Legacy WIPI network unavailable: scheduling connection failure (-1), no external request.\n");require(x!=0,"Network callback");deferred.push_back({now()+1,x,u32(-1),y});return 0;}
 if(n=="__aeabi_idiv"||n=="__aeabi_idivmod"||n=="__aeabi_uidivmod"){require(y!=0,"Guest integer divide by zero");int64_t a=n=="__aeabi_uidivmod"?int64_t(x):int64_t(s32(x)),b=n=="__aeabi_uidivmod"?int64_t(y):int64_t(s32(y));set(UC_ARM_REG_R1,u32(a%b));return u32(a/b);}
 if(n=="__aeabi_ldivmod"){int64_t a=int64_t(u64(x)|(u64(y)<<32)),b=int64_t(u64(z)|(u64(t)<<32));require(b!=0,"Guest long division by zero");require(!(a==INT64_MIN&&b==-1),"Guest long division overflow");int64_t q=a/b,r=a%b;set(UC_ARM_REG_R2,u32(r));set(UC_ARM_REG_R3,u32(u64(r)>>32));return wide(u64(q));}
 if(n.rfind("__aeabi_",0)==0){
  auto asFloat=[](u32 v){float f;memcpy(&f,&v,4);return f;};auto asDouble=[](u32 lo,u32 hi){u64 bits=u64(lo)|(u64(hi)<<32);double d;memcpy(&d,&bits,8);return d;};
  bool isDouble=n.rfind("__aeabi_d",0)==0||n=="__aeabi_i2d"||n=="__aeabi_f2d";double value=0;
  if(n=="__aeabi_i2d"||n=="__aeabi_i2f")value=s32(x);else if(n=="__aeabi_f2d")value=asFloat(x);else if(n=="__aeabi_d2f"){value=asDouble(x,y);isDouble=false;}else if(n=="__aeabi_d2iz")return s32(asDouble(x,y));else{
   double a=isDouble?asDouble(x,y):asFloat(x),b=isDouble?asDouble(z,t):asFloat(y);auto op=n.substr(9);
   if(op=="cmpeq")return a==b;if(op=="cmpge")return a>=b;if(op=="cmpgt")return a>b;if(op=="cmplt")return a<b;
   if(op=="add")value=a+b;else if(op=="sub")value=a-b;else if(op=="mul")value=a*b;else if(op=="div")value=a/b;else throw std::runtime_error("Unknown AEABI "+n);
  }if(isDouble){u64 bits;memcpy(&bits,&value,8);return wide(bits);}float f=(float)value;u32 bits;memcpy(&bits,&f,4);return bits;
 }
 throw std::runtime_error("Unmapped import "+n+" caller "+std::to_string(reg(UC_ARM_REG_LR)-BASE));
}

#ifdef __ANDROID__
extern "C" int SDL_main(int argc,char** argv)
#else
int main(int argc,char** argv)
#endif
{
 startBootLog();bootStage("main entered; initializing SDL");
 bool automated=false;for(int i=1;i<argc;i++)if(std::string(argv[i])=="--smoke"||std::string(argv[i])=="--verify-core")automated=true;
 SDL_SetHint(SDL_HINT_ORIENTATIONS,"LandscapeLeft LandscapeRight");
 SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER)!=0){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
 try{
  bootStage("SDL ready; mounting resources");
  fs::path root=resourceRoot();int seconds=0;bool smoke=false,coreTest=false,deterministic=false,noFast=false,forceFast=false,verifyFast=false,legacyWatchdog=false;int windowWidth=800,windowHeight=480;fs::path saves=defaultSaves(root),script,pointerScript,captures=root,armProfile;std::string inputMode="logical",pointerKind="mouse",language="en",aspect="auto";
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--smoke"&&i+1<argc){smoke=true;seconds=atoi(argv[++i]);}else if(a=="--save-dir"&&i+1<argc)saves=fs::u8path(argv[++i]);else if(a=="--script"&&i+1<argc)script=fs::u8path(argv[++i]);else if(a=="--capture-dir"&&i+1<argc)captures=fs::u8path(argv[++i]);else if(a=="--input-mode"&&i+1<argc)inputMode=argv[++i];else if(a=="--pointer-script"&&i+1<argc)pointerScript=fs::u8path(argv[++i]);else if(a=="--pointer-kind"&&i+1<argc)pointerKind=argv[++i];else if(a=="--window-size"&&i+2<argc){windowWidth=atoi(argv[++i]);windowHeight=atoi(argv[++i]);}else if(a=="--aspect"&&i+1<argc)aspect=argv[++i];else if(a=="--language"&&i+1<argc)language=argv[++i];else if(a=="--legacy-watchdog")legacyWatchdog=true;else if(a=="--fast")forceFast=true;else if(a=="--no-fast")noFast=true;else if(a=="--verify-fast")verifyFast=true;else if(a=="--profile-arm"&&i+1<argc)armProfile=fs::u8path(argv[++i]);else if(a=="--deterministic")deterministic=true;else if(a=="--verify-core")coreTest=true;else throw std::runtime_error("Unknown argument "+a);}
  require(inputMode=="logical"||inputMode=="keyboard"||inputMode=="gamepad","Unknown input mode");if(smoke)fs::create_directories(captures);
  struct InputEvent{u64 ms;int code,down;};std::vector<InputEvent> replay;size_t replayIndex=0;std::set<int> replayHeld;
  if(!script.empty()){std::ifstream f(script);require(bool(f),"Input script");InputEvent event;while(f>>event.ms>>event.code>>event.down)replay.push_back(event);require(f.eof(),"Malformed input script");}
  struct PointerReplay{u64 ms;u32 phase;float x,y;};std::vector<PointerReplay> pointerReplay;size_t pointerIndex=0;
  require(pointerKind=="mouse"||pointerKind=="finger","Unknown pointer kind");require(windowWidth>=240&&windowHeight>=240,"Window size");
  if(!pointerScript.empty()){std::ifstream f(pointerScript);require(bool(f),"Pointer script");PointerReplay e;while(f>>e.ms>>e.phase>>e.x>>e.y){require(e.phase<=2,"Pointer phase");pointerReplay.push_back(e);}require(f.eof(),"Malformed pointer script");}
  require(aspect=="auto"||aspect=="native","Unknown aspect mode");
  SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;int canvasWidth=240,canvasHeight=400;
  if(!coreTest){
  u32 windowFlags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __SWITCH__
  SDL_DisplayMode display{};
  if(SDL_GetCurrentDisplayMode(0,&display)==0&&display.w>0&&display.h>0){windowWidth=display.w;windowHeight=display.h;}
  else {windowWidth=1280;windowHeight=720;}
  windowFlags|=SDL_WINDOW_FULLSCREEN;
  printf("Switch display requested: %dx%d fullscreen\n",windowWidth,windowHeight);
#endif
#ifdef __ANDROID__
  // SDLActivity synchronizes the system bars with this flag, overriding the
  // manifest theme. Without it SDL explicitly re-enables Android's bars.
  windowFlags|=SDL_WINDOW_FULLSCREEN;
#endif
  window=SDL_CreateWindow("Heroes Lore 4",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,windowWidth,windowHeight,windowFlags);require(window!=nullptr,SDL_GetError());
#ifdef __SWITCH__
  if(!deviceLogOpen){char error[256];snprintf(error,sizeof(error),"Cannot write SD diagnostics (FS result 0x%x). Expected path: /switch/heroes_lore_4/performance.log",deviceLogResult);SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,"HL4 diagnostics",error,window);}
#endif
   bootStage("window ready; creating renderer");
  renderer=SDL_CreateRenderer(window,-1,deterministic?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);require(renderer!=nullptr,SDL_GetError());
   int w=0,h=0;require(SDL_GetRendererOutputSize(renderer,&w,&h)==0&&w>0&&h>0,"Renderer surface size");
   if(aspect=="auto"){
    auto even=[](double v){return std::min(1280,int(std::ceil(v/2))*2);};
    if(double(w)/h>=400.0/240)canvasHeight=even(240.0*w/h);
    else canvasWidth=even(400.0*h/w);
   }
   printf("Display startup: output=%dx%d guest=%dx%d landscape=%dx%d mode=%s\n",w,h,canvasWidth,canvasHeight,canvasHeight,canvasWidth,aspect.c_str());
  }
  bootStage("allocating 160 MiB guest backing and reading archive");
  Runtime r(root/"hl4.dat",saves,deterministic,coreTest?"ko":language,canvasWidth,canvasHeight);
#if defined(__SWITCH__) || defined(HL4_TCI) || defined(__ANDROID__)
  const bool defaultFast=true;
#else
  const bool defaultFast=false; // Desktop JIT keeps its existing execution path.
#endif
  r.legacyWatchdog=legacyWatchdog;
  r.installFast(!noFast&&(defaultFast||forceFast||verifyFast),verifyFast);
  if(!armProfile.empty())r.enableProfile(armProfile);
  if(coreTest){
   require(saves!=root/"saves","Core tests require --save-dir for disposable data");
   r.call("_init_module",{TEMP,TEMP+0x1000,TEMP+0x2000});require(r.read(TEMP+0x214)==r.exports.at("_export_table"),"Module ABI");
   r.write(TEMP+0x4000,0xe0900001);r.write(TEMP+0x4004,0xe12fff1e);require(r.call(TEMP+0x4000,{0xffffffff,1})==0,"ARM addition");require((r.reg(UC_ARM_REG_CPSR)&0xe0000000)==0x60000000,"ARM carry/zero/negative flags");
   r.write(TEMP+0x4020,0xeafffffe); // Deliberate infinite ARM loop verifies watchdog semantics.
   r.check(uc_emu_start(r.uc,TEMP+0x4020,RET+4,20000,0));size_t timedOut=0;r.check(uc_query(r.uc,UC_QUERY_TIMEOUT,&timedOut));require(timedOut!=0,"ARM watchdog timeout detection");
   for(int attempt=0;attempt<3;attempt++){
    u64 started=SDL_GetTicks64();r.watchdog->arm(20000);if(attempt==2)SDL_Delay(30);r.check(uc_emu_start(r.uc,TEMP+0x4020,RET+4,0,0));
    require(r.watchdog->disarm()&&SDL_GetTicks64()-started>=20,"Persistent watchdog failed to stop infinite ARM loop");
    r.write(TEMP+0x4030,0xe12fff1e);r.call(TEMP+0x4030); // No stale stop on the next call.
   }
   printf("PASS persistent ARM watchdog: 3 bounded infinite loops and immediate subsequent returns\n");
   r.write(TEMP+0x4010,0x47701840);require(r.call((TEMP+0x4010)|1,{11,17})==28,"Thumb addition/interworking");
   std::array<int,17> registers={UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR};std::array<u32,17> before{};for(size_t i=0;i<registers.size();i++)before[i]=r.reg(registers[i]);uc_context* context=nullptr;r.check(uc_context_alloc(r.uc,&context));r.check(uc_context_save(r.uc,context));r.set(UC_ARM_REG_R0,123);r.set(UC_ARM_REG_CPSR,before.back()^0xf0000000);r.check(uc_context_restore(r.uc,context));uc_context_free(context);for(size_t i=0;i<registers.size();i++)require(r.reg(registers[i])==before[i],"ARM context restore");
   r.call("DM_GET_FRAME_BUFFER_INFO",{TEMP+0x5000,r.screen});require(r.read(TEMP+0x5000)==480&&r.read(TEMP+0x5010)==r.frames.at(r.screen).pixels,"Framebuffer ABI");
   auto hle=[&](const std::string& name,std::initializer_list<u32> args){auto it=std::find(r.imports.begin(),r.imports.end(),name);require(it!=r.imports.end(),"Missing import fixture");return r.call(TRAP+4*(u32)std::distance(r.imports.begin(),it),args);};
   for(const auto& name:{"memcpy","dlib_memcpy","memmove","dlib_memmove","memset","dlib_memset","dlib_strncpy","dlib_strncmp","dlib_memcmp"})if(std::find(r.imports.begin(),r.imports.end(),name)!=r.imports.end())require(hle(name,{0,0,0})==0,"Empty memory contract");
   bool nonemptyRejected=false;try{hle("dlib_memcpy",{TEMP+0x5400,0,1});}catch(const std::exception&){nonemptyRejected=true;}require(nonemptyRejected,"Nonempty null copy accepted");printf("PASS zero-length memory calls; nonempty null copy still rejected\n");
   r.string(TEMP+0x5100,"core-test.dat");r.string(TEMP+0x5200,"HL4 temporary save roundtrip");u32 file=hle("MC_fsOpen",{TEMP+0x5100,4,1});require(s32(file)>0,"Create file");require(hle("MC_fsWrite",{file,TEMP+0x5200,28})==28,"Save write");hle("MC_fsClose",{file});file=hle("MC_fsOpen",{TEMP+0x5100,8,1});require(s32(file)>0,"Reopen save");require(hle("MC_fsSeek",{file,0,0})==0,"Save seek");require(hle("MC_fsRead",{file,TEMP+0x5300,28})==28&&memcmp(r.ptr(TEMP+0x5200,28),r.ptr(TEMP+0x5300,28),28)==0,"Save read");hle("MC_fsClose",{file});require(hle("MC_fsRemove",{TEMP+0x5100,1})==0,"Save cleanup");
   bool rejected=false;try{hle("MC_phnCallPlace",{0,0,0,0});}catch(const std::exception& e){rejected=std::string(e.what()).find("Unmapped import MC_phnCallPlace")!=std::string::npos;}require(rejected,"Unknown import accepted");
   r.loadLanguage(root/"hl4.lang","en");r.boot();
   auto fixture=std::find_if(r.localized.begin(),r.localized.end(),[&](const auto& e){return r.str(e.second.pointer)=="Equip";});require(fixture!=r.localized.end(),"Localized width fixture");
   r.string(TEMP+0x6000,fixture->first);auto canonical=fixture->first;
   u32 expected=r.call("HF_GetWidth2",{fixture->second.pointer,fixture->second.length,0,0,0});
   u32 measured=r.call("HF_GetWidth2",{TEMP+0x6000,u32(canonical.size()),0,0,0});require(expected>0&&expected==measured,"Localized font measurement");
   std::array<u32,8> saved{};for(int i=0;i<8;i++)saved[i]=r.reg(UC_ARM_REG_R4+i);
   r.call("HF_DrawTextAnchorH2Color",{r.screen,0,0,200,0,TEMP+0x6000,0,0,u32(canonical.size()),0,0xffff});
   for(int i=0;i<8;i++)require(saved[i]==r.reg(UC_ARM_REG_R4+i),"Localized callee-saved registers");
   require(r.str(TEMP+0x6000)==canonical&&r.textPatches.empty()&&r.localizedDraws==r.restoredTextFrames,"Localized canonical/stack restoration");
   printf("PASS localized ARM measurement/draw, canonical bytes, stack and callee-saved registers\n");
   for(auto pair:std::array<std::pair<const char*,const char*>,3>{{{"c6d0c0dcbab428312f313029","Straggler(1/10)"},{"baf1bdba20b7d5bcd2b5e5","Bis Longsword"},{"c0dabcf6c1a420","Amethyst "}}}){
    // Pinned CP949 bytes from observed enemy/item labels, not UTF-8 guest data.
    std::string canonical;for(size_t i=0;i<strlen(pair.first);i+=2){char v[3]={pair.first[i],pair.first[i+1],0};canonical+=char(strtoul(v,nullptr,16));}
    auto record=r.resolveText(canonical);require(record&&r.str(record->pointer)==pair.second,"Composed item/enemy localization");
    r.string(TEMP+0x6600,canonical);auto direct=r.call("HF_GetWidth2",{record->pointer,1000,0,0,0});
    require(direct==r.call("HF_GetWidth2",{TEMP+0x6600,1000,0,0,0}),"Composed text width");
    r.call("HF_DrawTextAnchorH2Color",{r.screen,0,0,220,0,TEMP+0x6600,0,0,1000,0,0xffff});
    require(r.str(TEMP+0x6600)==canonical&&r.textPatches.empty(),"Composed canonical restoration");
   }
   for(int i=0;i<600;i++){
    std::string source="\xc6\xd0\xc0\xdc\xba\xb4("+std::to_string(i)+"/1000)";
    auto record=r.resolveText(source);require(record&&r.str(record->pointer)=="Straggler("+std::to_string(i)+"/1000)","Dynamic language cache rollover");
   }
   printf("PASS localized enemy counters, composed equipment names and pickup names\n");

   auto quest=std::find_if(r.localized.begin(),r.localized.end(),[&](const auto& e){return r.str(e.second.pointer)=="&Find Kefnes;{Quest received!";});
   require(quest!=r.localized.end(),"Quest notification fixture");
   r.string(TEMP+0x6400,"&Find Kefnes;$...........{Quest received!");
   u32 paddedLines=r.call("HF_GetLineCount",{TEMP+0x6400,1000,130,0});
   u32 questLines=r.call("HF_GetLineCount",{quest->second.pointer,1000,130,0});
   require(paddedLines==3&&questLines==2,"Quest notification must fit title and status on two lines");
   fs::create_directories(captures);
   for(int padded=0;padded<2;padded++){
    u32 frame=r.newFrame(240,400);r.string(TEMP+0x6000,quest->first);
    r.call("HF_DrawTextAnchorH2Color",{frame,30,30,130,0,padded?TEMP+0x6400:TEMP+0x6000,0,0,1000,0,0xffff});
    auto f=r.frames.at(frame);auto s=SDL_CreateRGBSurfaceFrom(r.ptr(f.pixels,f.w*f.h*2),f.w,f.h,16,f.w*2,0xf800,0x7e0,0x1f,0);
    SDL_SaveBMP(s,(captures/(padded?"quest-padding-before.bmp":"quest-padding-after.bmp")).string().c_str());SDL_FreeSurface(s);
   }
   require(r.str(TEMP+0x6000)==quest->first&&r.textPatches.empty(),"Quest canonical and stack restoration");
   printf("PASS quest notice: original ARM layout 3 lines before, 2 after; original renderer captures\n");
   if(verifyFast){
    // Synthetic 16x16 indexed sprite, original HL4 rotate/blend ABI. This
    // exercises paths not reached by the town-only smoke, without a user save.
    u32 sprite=r.alloc(32),indices=r.alloc(1024),palette=r.alloc(512);
    r.write(sprite,16|(16u<<16));r.write(sprite+4,16u<<16);
    r.write(sprite+12,palette);r.write(sprite+24,indices);*r.ptr(indices)=12;
    for(u32 i=0;i<256;i++){uint16_t pixel=uint16_t(i*173);memcpy(r.ptr(palette+i*2,2),&pixel,2);}
    for(u32 i=8;i<1024;i++)*r.ptr(indices+i)=uint8_t(i%251+1);
    for(int i=0;i<2;i++){
     r.call("DM_FillRectBlendingRGB",{r.screen,4,4,16,16,0,128,0xff0000});
     r.call("DM_DrawRotate565Ex",{r.frames.at(r.screen).pixels,4,4,16,16,240,0,sprite,0,0,0,0,0,90,100,100,16,255,0});
    }
    require(r.hotChecks[8]>=2&&r.hotChecks[9]>=2,"Combat kernel oracle paths not exercised");
    printf("PASS combat rotate/blend kernels: 4 independent register/full-memory checks\n");
   }
   printf("PASS native core: ARM flags, Thumb, ELF/REL/BSS, module ABI, framebuffer ABI, temporary saves, unknown import failure\n");return 0;
  }
  if(auto icon=SDL_LoadBMP((root/"hl4-icon.bmp").generic_u8string().c_str())){SDL_SetWindowIcon(window,icon);SDL_FreeSurface(icon);}


  SDL_RenderSetLogicalSize(renderer,canvasWidth,canvasHeight);SDL_Texture* texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,canvasWidth,canvasHeight);require(texture!=nullptr,SDL_GetError());
  int virtualIndex=-1;SDL_Joystick* virtualJoystick=nullptr;
  if(smoke&&inputMode=="gamepad"){SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_VirtualJoystickDesc v{};v.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;v.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;v.naxes=SDL_CONTROLLER_AXIS_MAX;v.nbuttons=SDL_CONTROLLER_BUTTON_MAX;v.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;v.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;v.name="HL4 validation virtual gamepad";virtualIndex=SDL_JoystickAttachVirtualEx(&v);require(virtualIndex>=0,SDL_GetError());require(SDL_IsGameController(virtualIndex)==SDL_TRUE,"Virtual gamepad automatic mapping");virtualJoystick=SDL_JoystickOpen(virtualIndex);require(virtualJoystick!=nullptr,SDL_GetError());}
  std::map<int,SDL_GameController*> pads;auto openPad=[&](int index){if(SDL_IsGameController(index)){auto pad=SDL_GameControllerOpen(index);if(pad){int id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));if(pads.count(id))SDL_GameControllerClose(pad);else{pads[id]=pad;printf("gamepad: %s\n",SDL_GameControllerName(pad));}}}};for(int i=0;i<SDL_NumJoysticks();i++)openPad(i);
  std::map<SDL_Keycode,int> keys={{SDLK_UP,-1},{SDLK_w,-1},{SDLK_DOWN,-2},{SDLK_s,-2},{SDLK_LEFT,-3},{SDLK_a,-3},{SDLK_RIGHT,-4},{SDLK_d,-4},{SDLK_RETURN,53},{SDLK_SPACE,53},{SDLK_z,53},{SDLK_ESCAPE,-16},{SDLK_x,-16},{SDLK_TAB,-16},{SDLK_q,-6},{SDLK_e,-7},{SDLK_1,49},{SDLK_2,50},{SDLK_3,51},{SDLK_0,48},{SDLK_LSHIFT,42},{SDLK_BACKSPACE,35},{SDLK_F3,35}};
  std::map<int,int> buttons={{SDL_CONTROLLER_BUTTON_DPAD_UP,-1},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,-2},{SDL_CONTROLLER_BUTTON_DPAD_LEFT,-3},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,-4},{SDL_CONTROLLER_BUTTON_A,53},{SDL_CONTROLLER_BUTTON_B,-16},{SDL_CONTROLLER_BUTTON_START,-16},{SDL_CONTROLLER_BUTTON_BACK,35},{SDL_CONTROLLER_BUTTON_X,49},{SDL_CONTROLLER_BUTTON_Y,51},{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,42},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,48}};
#ifdef __SWITCH__
  // SDL's positional south/east and west/north slots differ from Nintendo's
  // printed labels. Physical A confirms/attacks; physical B cancels.
  std::swap(buttons[SDL_CONTROLLER_BUTTON_A],buttons[SDL_CONTROLLER_BUTTON_B]);
  std::swap(buttons[SDL_CONTROLLER_BUTTON_X],buttons[SDL_CONTROLLER_BUTTON_Y]);
#endif
  std::set<int> pressed;auto apply=[&](const std::set<int>& desired){for(int key:pressed)if(!desired.count(key))r.key(key,false);for(int key:desired)if(!pressed.count(key))r.key(key,true);pressed=desired;};
  bootStage("renderer ready; booting original game");
  r.boot();bootStage("original game boot completed; entering event loop");u64 begin=r.now(),lastFlush=0;bool running=true,rotated=false,focused=true,mouseDown=false,fingerActive=false;SDL_FingerID fingerId=0;std::set<SDL_Keycode> keyboard;int smokeStep=0;std::vector<std::string> states;
#ifdef __SWITCH__
  SwitchPerf switchPerf;
#endif
  while(running&&!r.quit){
#ifdef __SWITCH__
   if(!appletMainLoop())break;
   const u64 perfBegin=SDL_GetPerformanceCounter();
#endif
   if(smoke){u64 elapsed=r.now()-begin;while(replayIndex<replay.size()&&replay[replayIndex].ms<=elapsed){auto event=replay[replayIndex++];printf("replay %llu key=%d down=%d mode=%s\n",(unsigned long long)event.ms,event.code,event.down,inputMode.c_str());
    if(inputMode=="logical"){if(event.down)replayHeld.insert(event.code);else replayHeld.erase(event.code);}
    else if(inputMode=="keyboard"){auto key=std::find_if(keys.begin(),keys.end(),[&](auto p){return p.second==event.code;});require(key!=keys.end(),"Replay keyboard mapping");SDL_Event injected{};injected.type=event.down?SDL_KEYDOWN:SDL_KEYUP;injected.key.keysym.sym=key->first;require(SDL_PushEvent(&injected)==1,"Keyboard injection");}
    else {int code=event.code==50?-16:event.code;auto b=std::find_if(buttons.begin(),buttons.end(),[&](auto p){return p.second==code;});require(b!=buttons.end(),"Replay gamepad mapping");require(SDL_JoystickSetVirtualButton(virtualJoystick,b->first,(Uint8)event.down)==0,SDL_GetError());SDL_GameControllerUpdate();}
   }}
   if(smoke)while(pointerIndex<pointerReplay.size()&&pointerReplay[pointerIndex].ms<=r.now()-begin){auto e=pointerReplay[pointerIndex++];int wx,wy;SDL_RenderLogicalToWindow(renderer,e.x,e.y,&wx,&wy);SDL_Event injected{};
    if(pointerKind=="mouse"){if(e.phase==2){injected.type=SDL_MOUSEMOTION;injected.motion.state=SDL_BUTTON_LMASK;injected.motion.x=wx;injected.motion.y=wy;}else{injected.type=e.phase==0?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;injected.button.button=SDL_BUTTON_LEFT;injected.button.x=wx;injected.button.y=wy;}}
    else{int w,h;SDL_GetWindowSize(window,&w,&h);injected.type=e.phase==0?SDL_FINGERDOWN:e.phase==1?SDL_FINGERUP:SDL_FINGERMOTION;injected.tfinger.windowID=SDL_GetWindowID(window);injected.tfinger.touchId=1;injected.tfinger.fingerId=1;injected.tfinger.x=float(wx)/w;injected.tfinger.y=float(wy)/h;injected.tfinger.pressure=e.phase==1?0.f:1.f;}
    if(pointerKind=="mouse")injected.button.windowID=SDL_GetWindowID(window);require(SDL_PushEvent(&injected)==1,"Pointer injection");
   }
   SDL_Event ev;while(SDL_PollEvent(&ev)){
   if(ev.type==SDL_QUIT)running=false;
   if(ev.type==SDL_KEYDOWN&&!ev.key.repeat)keyboard.insert(ev.key.keysym.sym);
   if(ev.type==SDL_KEYUP)keyboard.erase(ev.key.keysym.sym);
   if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_FOCUS_LOST){focused=false;keyboard.clear();mouseDown=false;fingerActive=false;SDL_CaptureMouse(SDL_FALSE);r.cancelTouch();apply({});}
   if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)focused=true;
   if(ev.type==SDL_CONTROLLERDEVICEADDED)openPad(ev.cdevice.which);
   if(ev.type==SDL_CONTROLLERDEVICEREMOVED){auto it=pads.find(ev.cdevice.which);if(it!=pads.end()){SDL_GameControllerClose(it->second);pads.erase(it);}apply({});}
   if(ev.type==SDL_MOUSEBUTTONDOWN||ev.type==SDL_MOUSEBUTTONUP||ev.type==SDL_MOUSEMOTION){
    bool motion=ev.type==SDL_MOUSEMOTION;if((motion?ev.motion.which:ev.button.which)==SDL_TOUCH_MOUSEID)continue;
    if(!motion&&ev.button.button!=SDL_BUTTON_LEFT)continue;if(motion&&!mouseDown)continue;
    // SDL_RenderSetLogicalSize already filters event coordinates, including bars
    // and scaling. Applying RenderWindowToLogical here would transform twice.
    float lx=float(motion?ev.motion.x:ev.button.x),ly=float(motion?ev.motion.y:ev.button.y);
    if(ev.type==SDL_MOUSEBUTTONDOWN){if(lx<0||ly<0||lx>=(rotated?canvasHeight:canvasWidth)||ly>=(rotated?canvasWidth:canvasHeight))continue;mouseDown=true;SDL_CaptureMouse(SDL_TRUE);}
    if(ev.type==SDL_MOUSEBUTTONUP){if(!mouseDown)continue;mouseDown=false;SDL_CaptureMouse(SDL_FALSE);}
    u32 phase=motion?2u:ev.type==SDL_MOUSEBUTTONDOWN?0u:1u;
    int gx=rotated?int(ly):int(lx),gy=rotated?canvasHeight-1-int(lx):int(ly);r.touch(phase,(u32)std::clamp(gx,0,canvasWidth-1),(u32)std::clamp(gy,0,canvasHeight-1));
   }
   if(ev.type==SDL_FINGERDOWN||ev.type==SDL_FINGERUP||ev.type==SDL_FINGERMOTION){
    if(ev.tfinger.touchId==SDL_MOUSE_TOUCHID)continue;
    float lx=ev.tfinger.x*(rotated?canvasHeight:canvasWidth),ly=ev.tfinger.y*(rotated?canvasWidth:canvasHeight);
    if(ev.type==SDL_FINGERDOWN){if(fingerActive||lx<0||ly<0||lx>=(rotated?canvasHeight:canvasWidth)||ly>=(rotated?canvasWidth:canvasHeight))continue;fingerActive=true;fingerId=ev.tfinger.fingerId;}else if(!fingerActive||ev.tfinger.fingerId!=fingerId)continue;
    u32 phase=ev.type==SDL_FINGERDOWN?0u:ev.type==SDL_FINGERUP?1u:2u;
    int gx=rotated?int(ly):int(lx),gy=rotated?canvasHeight-1-int(lx):int(ly);r.touch(phase,(u32)std::clamp(gx,0,canvasWidth-1),(u32)std::clamp(gy,0,canvasHeight-1));if(phase==1)fingerActive=false;
   }
  }
   std::set<int> desired;for(auto key:keyboard)if(keys.count(key))desired.insert(keys.at(key));for(auto [id,pad]:pads){for(auto [button,code]:buttons)if(SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)button))desired.insert(code);int x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX),y=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);if(x<-16000)desired.insert(-3);if(x>16000)desired.insert(-4);if(y<-16000)desired.insert(-1);if(y>16000)desired.insert(-2);}
   if(!focused&&!smoke)desired.clear();
   if(smoke){u64 elapsed=r.now()-begin;int step=int(elapsed/2000);
    desired.insert(replayHeld.begin(),replayHeld.end());
    if(step!=smokeStep){smokeStep=step;if(script.empty())desired.insert(53);auto f=r.frames.at(r.lastFrame);auto s=SDL_CreateRGBSurfaceFrom(r.ptr(f.pixels,f.w*f.h*2),f.w,f.h,16,f.w*2,0xf800,0x7e0,0x1f,0);SDL_SaveBMP(s,(captures/("smoke-"+std::to_string(step)+".bmp")).string().c_str());SDL_FreeSurface(s);
     u32 hero=r.call("MP_getHero");s32 x=(s32)r.call("CHAR_getX",{hero}),y=(s32)r.call("CHAR_getY",{hero});std::string cb;u32 target=r.read(r.exports.at("nBASE_KeyPressed"));for(auto& [name,address]:r.exports)if(address==target){cb=name;break;}printf("viewport capture=%d world=%ux%u camera=(%d,%d) logical=%ux%u panel=%u\n",step,r.read(r.exports.at("_map")+0x84),r.read(r.exports.at("_map")+0x88),s32(r.read(r.exports.at("_map")+0x9c)),s32(r.read(r.exports.at("_map")+0xa0)),r.read(r.exports.at("_lcdG")+0x54),r.read(r.exports.at("_lcdG")+0x58),r.read(r.exports.at("m_TouchDirectKeySize")));states.push_back("{\"ms\":"+std::to_string(elapsed)+",\"callback\":\""+cb+"\",\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}");
    }
    if(elapsed>=u64(seconds)*1000)running=false;}
#ifdef __SWITCH__
   const u64 perfInputEnd=SDL_GetPerformanceCounter();
#endif
   apply(desired);r.tick();
#ifdef __SWITCH__
   const u64 perfGuestEnd=SDL_GetPerformanceCounter();
#endif
   if(lastFlush!=r.flushes){auto f=r.frames.at(r.lastFrame);require(f.w==canvasWidth&&f.h==canvasHeight,"Display dimensions changed");SDL_UpdateTexture(texture,nullptr,r.ptr(f.pixels,f.w*f.h*2),f.w*2);lastFlush=r.flushes;rotated=r.read(r.exports.at("nTouchRotate"))!=0;SDL_RenderSetLogicalSize(renderer,rotated?canvasHeight:canvasWidth,rotated?canvasWidth:canvasHeight);}
   SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);if(rotated){SDL_Rect dest{(canvasHeight-canvasWidth)/2,(canvasWidth-canvasHeight)/2,canvasWidth,canvasHeight};SDL_RenderCopyEx(renderer,texture,nullptr,&dest,90,nullptr,SDL_FLIP_NONE);}else SDL_RenderCopy(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);if(deterministic)r.testClock+=10;else SDL_Delay(1);
#ifdef __SWITCH__
   switchPerf.frame(perfBegin,perfInputEnd,perfGuestEnd,SDL_GetPerformanceCounter(),r.flushes,r.hotInstructions,r.nativeTicks,r.audioTicks,r.audioDecodes,r.audioHits);
#endif
  }
  r.reportFast();
  printf("AUDIO decodes=%llu cache_hits=%llu load_ms=%.2f cached_bytes=%zu\n",(unsigned long long)r.audioDecodes,(unsigned long long)r.audioHits,1000.0*r.audioTicks/SDL_GetPerformanceFrequency(),r.audioCache.bytes);
  if(smoke){require(r.textPatches.empty(),"Unrestored localization stack");printf("LOCALIZATION draws=%llu restores=%llu missing=%zu\n",(unsigned long long)r.localizedDraws,(unsigned long long)r.restoredTextFrames,r.missingText.size());std::ofstream missing(captures/"localization-missing.tsv");for(auto& [text,count]:r.missingText){missing<<count<<"\t";for(unsigned char c:text){char hex[3];snprintf(hex,3,"%02x",c);missing<<hex;}missing<<"\n";}auto f=r.frames.at(r.lastFrame);auto s=SDL_CreateRGBSurfaceFrom(r.ptr(f.pixels,f.w*f.h*2),f.w,f.h,16,f.w*2,0xf800,0x7e0,0x1f,0);SDL_SaveBMP(s,(captures/"smoke.bmp").string().c_str());SDL_FreeSurface(s);printf("SMOKE frames=%llu audio_nonzero=%llu\n",(unsigned long long)r.flushes,(unsigned long long)r.nonzero.load());
   std::ofstream report(captures/"stats.json");report<<"{\"clock_mode\":\""<<(deterministic?"test-10ms":"SDL-real-time")<<"\",\"input_mode\":\""<<inputMode<<"\",\"frames\":"<<r.flushes<<",\"audio_nonzero\":"<<r.nonzero.load()<<",\"states\":[";for(size_t i=0;i<states.size();i++){if(i)report<<',';report<<states[i];}report<<"]}\n";
  }
  if(smoke){int width,height;require(SDL_GetRendererOutputSize(renderer,&width,&height)==0,SDL_GetError());auto s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);require(s!=nullptr,SDL_GetError());require(SDL_RenderReadPixels(renderer,nullptr,s->format->format,s->pixels,s->pitch)==0,SDL_GetError());SDL_SaveBMP(s,(captures/"window.bmp").string().c_str());SDL_FreeSurface(s);}
  apply({});for(auto [id,pad]:pads)SDL_GameControllerClose(pad);if(virtualJoystick)SDL_JoystickClose(virtualJoystick);if(virtualIndex>=0)SDL_JoystickDetachVirtual(virtualIndex);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
 }catch(const std::exception& e){fprintf(stderr,"HL4: %s\n",e.what());if(!automated)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Heroes Lore 4",e.what(),nullptr);SDL_Quit();return 1;}SDL_Quit();return 0;
}
