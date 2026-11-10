// Main-thread cache of immutable, converted samples. The callback only reads
// clip-held buffers; eviction cannot invalidate a playing clip.
using Pcm=std::shared_ptr<const std::vector<float>>;
struct AudioCache {
 struct Entry{Pcm pcm;u64 used;};
 std::map<std::string,Entry> entries;
 size_t bytes=0,limit=48u*1024u*1024u;u64 clock=0;
 Pcm find(const std::string& name){auto it=entries.find(name);if(it==entries.end())return {};it->second.used=++clock;return it->second.pcm;}
 void insert(const std::string& name,Pcm pcm){
  size_t size=pcm->size()*sizeof(float);if(size>limit)return;
  auto existing=entries.find(name);if(existing!=entries.end()){bytes-=existing->second.pcm->size()*sizeof(float);entries.erase(existing);}
  while(bytes+size>limit&&!entries.empty()){
   auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
   bytes-=oldest->second.pcm->size()*sizeof(float);entries.erase(oldest);
  }
  entries.emplace(name,Entry{std::move(pcm),++clock});bytes+=size;
 }
};
