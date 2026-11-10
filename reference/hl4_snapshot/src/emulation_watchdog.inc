// One sleeping watchdog per engine. Unicorn's built-in timeout otherwise
// creates/joins a thread on every call and polls every two microseconds.
// This maintains the same finite budgets without changing guest clocks.
class EmulationWatchdog {
 uc_engine* engine;SDL_mutex* mutex=nullptr;SDL_cond* changed=nullptr;SDL_Thread* thread=nullptr;
 bool closing=false,armed=false,expired=false;u64 deadline=0;
 static int worker(void* opaque){
  auto& w=*(EmulationWatchdog*)opaque;SDL_LockMutex(w.mutex);
  while(!w.closing){
   if(!w.armed){SDL_CondWait(w.changed,w.mutex);continue;}
   u64 now=SDL_GetTicks64();
   if(now<w.deadline){SDL_CondWaitTimeout(w.changed,w.mutex,Uint32(w.deadline-now));continue;}
   // Main cannot arm another call until this stop has completed.
   w.expired=true;uc_emu_stop(w.engine);
   // Keep stopping until disarm: emu_start may reset its stop flag after a
   // host suspension between arm and entry. Do not leave that race unbounded.
   SDL_CondWaitTimeout(w.changed,w.mutex,1);
  }
  SDL_UnlockMutex(w.mutex);return 0;
 }
public:
 explicit EmulationWatchdog(uc_engine* e):engine(e){
  mutex=SDL_CreateMutex();changed=SDL_CreateCond();
  if(mutex&&changed)thread=SDL_CreateThread(worker,"HL4 watchdog",this);
  if(!thread){if(changed)SDL_DestroyCond(changed);if(mutex)SDL_DestroyMutex(mutex);throw std::runtime_error("Cannot create ARM watchdog");}
 }
 ~EmulationWatchdog(){SDL_LockMutex(mutex);closing=true;SDL_CondSignal(changed);SDL_UnlockMutex(mutex);SDL_WaitThread(thread,nullptr);SDL_DestroyCond(changed);SDL_DestroyMutex(mutex);}
 void arm(u64 microseconds){SDL_LockMutex(mutex);deadline=SDL_GetTicks64()+(microseconds+999)/1000;expired=false;armed=true;SDL_CondSignal(changed);SDL_UnlockMutex(mutex);}
 bool disarm(){SDL_LockMutex(mutex);bool result=expired;armed=false;SDL_CondSignal(changed);SDL_UnlockMutex(mutex);return result;}
};
