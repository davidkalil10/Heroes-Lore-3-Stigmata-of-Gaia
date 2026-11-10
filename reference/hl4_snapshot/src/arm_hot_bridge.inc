// Sparse Unicorn boundary around generated HL4 ARM regions. No guest patches.
struct HotHook{Runtime* owner=nullptr;unsigned region=0;};
std::array<HotHook,HotArm::regionCount> hotHooks{};
std::array<u64,HotArm::regionCount> hotHits{},hotChecks{};
bool fastEnabled=false,verifyFast=false;u64 hotInstructions=0,nativeTicks=0;
uc_engine* oracle=nullptr;Bytes oracleMemory;
static std::array<int,17>& armRegisters(){static std::array<int,17> ids={UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR};return ids;}
std::array<u32,17> armState(uc_engine* engine){std::array<u32,17> state{};std::array<void*,17> ptrs;for(unsigned i=0;i<17;i++)ptrs[i]=&state[i];check(uc_reg_read_batch(engine,armRegisters().data(),ptrs.data(),17));return state;}
void setArmState(uc_engine* engine,std::array<u32,17>& state){check(uc_reg_write(engine,UC_ARM_REG_CPSR,&state[16]));std::array<void*,15> ptrs;for(unsigned i=0;i<15;i++)ptrs[i]=&state[i];check(uc_reg_write_batch(engine,armRegisters().data(),ptrs.data(),15));check(uc_reg_write(engine,UC_ARM_REG_PC,&state[15]));}
void prepareOracle(){
 if(oracle)return;oracleMemory.resize(mem.size());check(uc_open(UC_ARCH_ARM,UC_MODE_ARM,&oracle));check(uc_ctl_set_tcg_buffer_size(oracle,32u*1024u*1024u));
 for(auto [a,n]:std::vector<std::pair<u32,u32>>{{BASE,0x200000},{TEMP,0x100000},{TRAP,0x10000},{HEAP,0x4000000}})check(uc_mem_map_ptr(oracle,a,n,UC_PROT_ALL,oracleMemory.data()+a));
}
static void hotEntry(uc_engine*,u64 address,u32,void* opaque){
 auto& hook=*(HotHook*)opaque;auto& r=*hook.owner;HotArm cpu(r.mem.data());
 try{
  if(!r.fastEnabled)return;const u64 nativeBegin=SDL_GetPerformanceCounter();auto before=r.armState(r.uc);if(before[16]&0x20)return;
  std::copy_n(before.begin(),16,cpu.r);cpu.N=before[16]>>31;cpu.Z=(before[16]>>30)&1;cpu.C=(before[16]>>29)&1;cpu.V=(before[16]>>28)&1;
  bool verify=r.verifyFast&&r.hotChecks[hook.region]<2;
  if(verify){r.prepareOracle();memcpy(r.oracleMemory.data(),r.mem.data(),r.mem.size());r.setArmState(r.oracle,before);}
  cpu.run(hook.region);auto after=before;std::copy_n(cpu.r,16,after.begin());after[16]=(before[16]&0x0fffffffu)|(u32(cpu.N)<<31)|(u32(cpu.Z)<<30)|(u32(cpu.C)<<29)|(u32(cpu.V)<<28);
  if(verify){
   r.check(uc_emu_start(r.oracle,address,after[15],5000000,0));auto original=r.armState(r.oracle);
   for(unsigned i=0;i<17;i++)if(after[i]!=original[i])throw std::runtime_error("Hot ARM region "+std::to_string(hook.region)+" register "+std::to_string(i)+" mismatch: native="+std::to_string(after[i])+" original="+std::to_string(original[i]));
   require(memcmp(r.mem.data(),r.oracleMemory.data(),r.mem.size())==0,"Hot ARM full-memory mismatch region "+std::to_string(hook.region));
   r.hotChecks[hook.region]++;printf("PASS hot ARM region=%u registers R0-R15/CPSR and complete %zu-byte memory\n",hook.region,r.mem.size());
  }
  r.setArmState(r.uc,after);r.hotHits[hook.region]++;r.hotInstructions+=cpu.icount;r.nativeTicks+=SDL_GetPerformanceCounter()-nativeBegin;
 }catch(const std::exception& e){for(int i=0;i<16;i++)fprintf(stderr,"hot r%d=%x ",i,cpu.r[i]);fprintf(stderr,"\n");r.failure=std::string("Hot ARM region ")+std::to_string(hook.region)+": "+e.what();uc_emu_stop(r.uc);}
}
void installFast(bool enabled,bool verify){
 fastEnabled=enabled;verifyFast=verify;if(!enabled)return;HotArm cpu(mem.data());
 for(unsigned i=0;i<HotArm::regionCount;i++){
  if(!cpu.guard(i)){fprintf(stderr,"Hot ARM region %u opcode mismatch: retaining original execution\n",i);continue;}
  hotHooks[i]={this,i};uc_hook hook;auto entry=HotArm::entry(i);check(uc_hook_add(uc,&hook,UC_HOOK_CODE,(void*)hotEntry,&hotHooks[i],entry,entry));
 }
}
void reportFast(){printf("HOT ARM instructions=%llu",(unsigned long long)hotInstructions);for(unsigned i=0;i<HotArm::regionCount;i++)printf(" r%u=%llu/%llu",i,(unsigned long long)hotHits[i],(unsigned long long)hotChecks[i]);printf("\n");}
