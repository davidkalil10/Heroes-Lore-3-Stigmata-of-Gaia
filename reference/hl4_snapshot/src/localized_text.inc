// HL4-specific draw-boundary adapter. Original strings/resources remain immutable.
// Arguments 5..8 are transiently adapted; the original ARM renderer still draws.
struct LocalizedRecord{u32 pointer,length;};
struct TextPatch{u32 sp,lr;std::array<u32,4> original;};
std::map<std::string,LocalizedRecord> localized;
std::vector<TextPatch> textPatches;
std::map<std::string,LocalizedRecord> composedText;
std::array<std::vector<std::string>,256> localizedPrefixes;
u32 composedPool=0,composedUsed=0;
// Compose only fully recognized Korean spans and literal ASCII formatting.
// Bound the cache and arena; repeated HUD labels never allocate guest buffers.
const LocalizedRecord* resolveText(const std::string& canonical){
 auto direct=localized.find(canonical);if(direct!=localized.end())return &direct->second;
 auto cached=composedText.find(canonical);if(cached!=composedText.end())return &cached->second;
 if(canonical.size()>1024)return nullptr;
 std::string translated;bool changed=false;
 for(size_t at=0;at<canonical.size();){
  const std::string* found=nullptr;
  for(const auto& key:localizedPrefixes[(unsigned char)canonical[at]])if(canonical.compare(at,key.size(),key)==0){found=&key;break;}
  if(found){auto record=localized.at(*found);translated+=str(record.pointer);at+=found->size();changed=true;}
  else if((unsigned char)canonical[at]<128)translated+=canonical[at++];
  else return nullptr;
 }
 if(!changed||translated.empty())return nullptr;
 if(composedText.size()>=512||composedUsed+translated.size()+1>131072){
  // Only draw/measure adapters expose these pointers transiently. Never reuse
  // arena bytes while an original ARM draw still owns a patched stack frame.
  if(!textPatches.empty())return nullptr;
  composedText.clear();composedUsed=0;
 }
 u32 p=composedPool+composedUsed;string(p,translated);composedUsed+=u32(translated.size()+1);
 return &composedText.emplace(canonical,LocalizedRecord{p,u32(translated.size())}).first->second;
}
std::set<u32> textReturnSites;
u64 localizedDraws=0,restoredTextFrames=0;
std::map<std::string,u64> missingText;

void restoreText(u32 pc){
 if(textPatches.empty())return;
 auto p=textPatches.back();
 if(p.lr!=pc||p.sp!=reg(UC_ARM_REG_SP))return;
 for(size_t i=0;i<4;i++)write(p.sp+4+u32(i)*4,p.original[i]);
 textPatches.pop_back();restoredTextFrames++;
}
static void onTextReturn(uc_engine*,u64 address,u32,void* opaque){
 auto& r=*(Runtime*)opaque;try{r.restoreText(u32(address));}catch(const std::exception& e){r.failure=e.what();uc_emu_stop(r.uc);}
}
static void onTextEntry(uc_engine*,u64,u32,void* opaque){
 auto& r=*(Runtime*)opaque;
 try{
  u32 source=r.arg(5);if(!source)return;
  auto canonical=r.str(source);if(canonical.empty())return;
  auto record=r.resolveText(canonical);
  if(!record){if(std::any_of(canonical.begin(),canonical.end(),[](unsigned char c){return c>=128;}))r.missingText[canonical]++;return;}
  u32 sp=r.reg(UC_ARM_REG_SP),lr=r.reg(UC_ARM_REG_LR);TextPatch p{sp,lr,{}};
  for(size_t i=0;i<4;i++)p.original[i]=r.read(sp+4+u32(i)*4);
  auto scale=[&](u32 index){if(s32(index)<0)return index;return u32(std::min<u64>(index,canonical.size())*record->length/canonical.size());};
  r.write(sp+4,record->pointer);
  r.write(sp+8,scale(p.original[1]));
  r.write(sp+12,scale(p.original[1]+p.original[2])-scale(p.original[1]));
  r.write(sp+16,scale(p.original[3]));
  r.textPatches.push_back(p);r.localizedDraws++;
  require(lr==RET||r.textReturnSites.count(lr),"Unverified localized draw caller");
 }catch(const std::exception& e){r.failure=std::string("Localization: ")+e.what();uc_emu_stop(r.uc);}
}
// HF_GetWidth2(text,end,start,maxWidth,font[sp]); only caller-saved
// registers change. Execute the original measuring routine on the same bytes
// that the draw boundary will use.
static void onTextMeasure(uc_engine*,u64,u32,void* opaque){
 auto& r=*(Runtime*)opaque;
 try{
  u32 source=r.reg(UC_ARM_REG_R0);if(!source)return;
  auto canonical=r.str(source);if(canonical.empty())return;auto record=r.resolveText(canonical);
  if(!record)return;
  auto scale=[&](u32 index){if(s32(index)<0)return index;return u32(std::min<u64>(index,canonical.size())*record->length/canonical.size());};
  r.set(UC_ARM_REG_R0,record->pointer);
  r.set(UC_ARM_REG_R1,scale(r.reg(UC_ARM_REG_R1)));
  r.set(UC_ARM_REG_R2,scale(r.reg(UC_ARM_REG_R2)));
 }catch(const std::exception& e){r.failure=std::string("Localization measure: ")+e.what();uc_emu_stop(r.uc);}
}
void loadLanguage(const fs::path& path,const std::string& language){
 if(language=="ko")return;
 require(language=="en","Runtime currently supports English or original Korean; PT-BR catalog is UTF-8 for a later font integration");
 auto b=readFile(path);require(b.size()>=12&&memcmp(b.data(),"HL4LANG1",8)==0,"Invalid language pack");
 u32 count=structure<u32>(b,8);require(count<20000,"Language count");size_t at=12;u64 poolSize=0;
 struct Entry{std::string source,text;};std::vector<Entry> entries;
 for(u32 i=0;i<count;i++){
  u32 n=structure<u32>(b,at),en=structure<u32>(b,at+4),pt=structure<u32>(b,at+8);at+=12;
  require(n>0&&n<65536&&en<65536&&pt<65536&&at+u64(n)+en+pt<=b.size(),"Language record bounds");
  std::string source((char*)b.data()+at,n),text((char*)b.data()+at+n,en);at+=u64(n)+en+pt;
  require(source.find('\0')==std::string::npos&&text.find('\0')==std::string::npos,"Embedded language NUL");
  require(std::all_of(text.begin(),text.end(),[](unsigned char c){return c<128;}),"English font requires ASCII");
  if(!text.empty()){poolSize+=text.size()+1;entries.push_back({source,text});}
 }
 require(at==b.size()&&poolSize>0&&poolSize<0x400000,"Language pack size");u32 pool=alloc(u32(poolSize));require(pool!=0,"Language buffer allocation");
 for(auto& e:entries){string(pool,e.text);localized.emplace(e.source,LocalizedRecord{pool,u32(e.text.size())});pool+=u32(e.text.size()+1);}
 composedPool=alloc(131072);require(composedPool!=0,"Dynamic language arena");
 for(const auto& [key,value]:localized){
  // ASCII-only/control strings must not substitute digits or formatting.
  if(std::any_of(key.begin(),key.end(),[](unsigned char c){return c>=128;}))localizedPrefixes[(unsigned char)key[0]].push_back(key);
 }
 for(auto& keys:localizedPrefixes)std::sort(keys.begin(),keys.end(),[](const auto& a,const auto& b){return a.size()>b.size();});
 auto entry=exports.at("HF_DrawTextAnchorH2Color");
 require(read(entry)==0xe92d4ff0&&read(entry+4)==0xe59f8b80,"HL4 draw opcode guard");
 // Verified direct BL sites in this pinned HL4 ELF. Register before any ARM
 // execution: adding hooks inside an active translated block can miss returns.
 for(auto [site,opcode]:std::array<std::pair<u32,u32>,2>{{{0x5afa4,0xeb006a38},{0x764a0,0xebfffcf9}}}){
  require(read(BASE+site)==opcode,"HL4 draw caller opcode guard");u32 ret=BASE+site+4;textReturnSites.insert(ret);
  uc_hook hook;check(uc_hook_add(uc,&hook,UC_HOOK_CODE,(void*)onTextReturn,this,ret,ret));
 }
 uc_hook h;check(uc_hook_add(uc,&h,UC_HOOK_CODE,(void*)onTextEntry,this,entry,entry));
 auto measure=exports.at("HF_GetWidth2");
 require(read(measure)==0xe92d47f0&&read(measure+4)==0xe1520001,"HL4 measure opcode guard");
 uc_hook mh;check(uc_hook_add(uc,&mh,UC_HOOK_CODE,(void*)onTextMeasure,this,measure,measure));
 printf("Language: English; %zu canonical strings, original ARM font renderer\n",localized.size());
}

