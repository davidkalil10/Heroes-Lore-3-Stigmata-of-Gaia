// Fixed-size counters; durable independent SD log, once per five seconds.
struct SwitchPerf {
 u64 frequency=SDL_GetPerformanceFrequency(),sample=SDL_GetPerformanceCounter();
 u64 loops=0,input=0,guest=0,present=0,lastFlush=0,lastInstructions=0;
 SwitchPerf(){writeDeviceLog("[hl4-perf] event loop entered; interval=5s\n");}
 void frame(u64 begin,u64 inputEnd,u64 guestEnd,u64 end,u64 flushes,u64 instructions){
  loops++;input+=inputEnd-begin;guest+=guestEnd-inputEnd;present+=end-guestEnd;
  double seconds=double(end-sample)/frequency;if(seconds<5)return;
  double ms=1000.0/frequency/loops;char line[512];
  snprintf(line,sizeof(line),"[hl4-perf] loop_fps=%.1f game_flush_fps=%.1f input_ms=%.2f guest_ms=%.2f present_wait_ms=%.2f native_arm_instructions=%llu\n",loops/seconds,(flushes-lastFlush)/seconds,input*ms,guest*ms,present*ms,(unsigned long long)(instructions-lastInstructions));
  writeDeviceLog(line);
  fputs(line,stdout);fflush(stdout);sample=end;loops=input=guest=present=0;lastFlush=flushes;lastInstructions=instructions;
 }
};
