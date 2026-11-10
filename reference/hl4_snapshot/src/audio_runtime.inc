AudioCache audioCache;u64 audioDecodes=0,audioHits=0,audioTicks=0;
Pcm decodeAudio(const Bytes& b){
 require(b.size()>=4&&memcmp(b.data(),"OggS",4)==0,"Unsupported audio format");
 int channels=0,rate=0;short* decoded=nullptr;
 int count=stb_vorbis_decode_memory(b.data(),(int)b.size(),&channels,&rate,&decoded);
 std::unique_ptr<short,decltype(&::free)> samples(decoded,&::free);
 require(count>0&&decoded,"Ogg decoding failed");
 SDL_AudioCVT cvt{};require(SDL_BuildAudioCVT(&cvt,AUDIO_S16SYS,(Uint8)channels,rate,AUDIO_F32SYS,2,48000)>=0,"Audio conversion");
 int size=count*channels*2;Bytes buffer(size*cvt.len_mult);memcpy(buffer.data(),decoded,size);cvt.buf=buffer.data();cvt.len=size;
 require(SDL_ConvertAudio(&cvt)==0,"Audio conversion failed");
 auto pcm=std::make_shared<std::vector<float>>(cvt.len_cvt/4);memcpy(pcm->data(),buffer.data(),cvt.len_cvt);return pcm;
}
u32 loadAudio(u32 id,const std::string& command){
 const u64 begin=SDL_GetPerformanceCounter();
 require(command.rfind("-file ",0)==0,"Unknown media source command");
 auto name=command.substr(6);auto it=data.find(name);if(it==data.end())return u32(-12);
 auto pcm=audioCache.find(name);
 // A clip can retain an oversized track not admitted to the bounded cache.
 if(!pcm&&clips.at(id).name==name)pcm=clips.at(id).pcm;
 if(pcm)audioHits++;else{pcm=decodeAudio(it->second);audioDecodes++;audioCache.insert(name,pcm);}
 Pcm old; // Release old samples outside the callback lock.
 SDL_LockAudioDevice(audio);auto& clip=clips.at(id);old=std::move(clip.pcm);clip.pcm=std::move(pcm);clip.name=name;SDL_UnlockAudioDevice(audio);
 audioTicks+=SDL_GetPerformanceCounter()-begin;return 0;
}
