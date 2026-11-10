// Portable SHA-256, FIPS 180-4. Used to verify every archived resource.
static std::array<uint8_t,32> hash(const Bytes& input){
 static constexpr u32 k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 auto rotr=[](u32 x,int n){return (x>>n)|(x<<(32-n));};
 u32 state[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 u64 bits=u64(input.size())*8;size_t total=(input.size()+9+63)&~size_t(63);
 for(size_t pos=0;pos<total;pos+=64){
  uint8_t block[64]{};for(size_t i=0;i<64;i++){size_t at=pos+i;if(at<input.size())block[i]=input[at];else if(at==input.size())block[i]=0x80;else if(at>=total-8)block[i]=uint8_t(bits>>((total-1-at)*8));}
  u32 w[64];for(int i=0;i<16;i++)w[i]=(u32(block[i*4])<<24)|(u32(block[i*4+1])<<16)|(u32(block[i*4+2])<<8)|block[i*4+3];
  for(int i=16;i<64;i++){u32 a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));}
  u32 a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
  for(int i=0;i<64;i++){u32 t1=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];u32 t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
 }
 std::array<uint8_t,32> out{};for(int i=0;i<32;i++)out[i]=uint8_t(state[i/4]>>(24-8*(i%4)));return out;
}
