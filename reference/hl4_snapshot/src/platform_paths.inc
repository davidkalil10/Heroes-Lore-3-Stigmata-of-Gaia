#ifdef __SWITCH__
// Native FS handles bypass stdio/devoptab routing used by the old logs.
static FsFileSystem deviceLogFs{};static FsFile deviceLogFile{};
static bool deviceLogOpen=false;static u64 deviceLogOffset=0;static Result deviceLogResult=0;
static void writeDeviceLog(const char* text){
 if(!deviceLogOpen)return;
 size_t n=strlen(text);Result rc=fsFileWrite(&deviceLogFile,deviceLogOffset,text,n,FsWriteOption_Flush);
 if(R_SUCCEEDED(rc))deviceLogOffset+=n;else {deviceLogResult=rc;deviceLogOpen=false;}
}
static void openDeviceLog(){
 deviceLogResult=fsOpenSdCardFileSystem(&deviceLogFs);if(R_FAILED(deviceLogResult))return;
 fsFsCreateDirectory(&deviceLogFs,"/switch");fsFsCreateDirectory(&deviceLogFs,"/switch/heroes_lore_4");
 const char* path="/switch/heroes_lore_4/performance.log";
 fsFsCreateFile(&deviceLogFs,path,0,0);
 deviceLogResult=fsFsOpenFile(&deviceLogFs,path,FsOpenMode_Write|FsOpenMode_Append,&deviceLogFile);
 if(R_FAILED(deviceLogResult))return;
 deviceLogResult=fsFileSetSize(&deviceLogFile,0);if(R_FAILED(deviceLogResult))return;
 deviceLogOpen=true;writeDeviceLog("HL4 native-FS diagnostics v3; identity=0100F4A400040000; gameplay samples every 5s\n");
}
#endif
// Startup-only diagnostics survive an early Switch exit, before SDL or ARM init.
static void startBootLog(){
#ifdef __SWITCH__
 openDeviceLog();
 mkdir("sdmc:/switch",0777);mkdir("sdmc:/switch/heroes_lore_4",0777);
 FILE* log=fopen("sdmc:/switch/heroes_lore_4/boot.log","w");
 if(!log)log=fopen("sdmc:/hl4-boot.log","w");
 if(log){fflush(stdout);fflush(stderr);dup2(fileno(log),fileno(stdout));dup2(fileno(log),fileno(stderr));fclose(log);setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);}
 printf("HL4 Switch startup; applet type=%d\n",int(appletGetAppletType()));
#endif
}
static void bootStage(const char* stage){
#ifdef __SWITCH__
 printf("boot: %s\n",stage);writeDeviceLog("boot: ");writeDeviceLog(stage);writeDeviceLog("\n");
#else
 (void)stage;
#endif
}
static FILE* openSave(const fs::path& p,int mode){
#ifdef _WIN32
 return _wfopen(p.c_str(),mode==1?L"rb":mode==4?L"w+b":L"r+b");
#else
 return fopen(p.c_str(),mode==1?"rb":mode==4?"w+b":"r+b");
#endif
}
static int seekSave(FILE* file,int64_t offset,int origin){
#ifdef _WIN32
 return _fseeki64(file,offset,origin);
#else
 return fseeko(file,off_t(offset),origin);
#endif
}
static int64_t tellSave(FILE* file){
#ifdef _WIN32
 return _ftelli64(file);
#else
 return ftello(file);
#endif
}
static fs::path resourceRoot(){
#ifdef __SWITCH__
 require(R_SUCCEEDED(romfsInit()),"Cannot mount HL4 RomFS");return "romfs:/";
#elif defined(__ANDROID__)
 return {}; // SDL_RWFromFile resolves packaged APK assets.
#else
 char* p=SDL_GetBasePath();require(p!=nullptr,"Executable path");fs::path result=fs::u8path(p);SDL_free(p);return result;
#endif
}
static fs::path defaultSaves(const fs::path& root){
#ifdef _WIN32
 return root/"saves";
#elif defined(__SWITCH__)
 return "sdmc:/switch/heroes_lore_4/saves";
#else
 char* p=SDL_GetPrefPath("DavidKalilBraga","HeroesLore4");require(p!=nullptr,"Save directory");fs::path result=fs::u8path(p);SDL_free(p);return result/"saves";
#endif
}
