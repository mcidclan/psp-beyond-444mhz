#define u32 unsigned int

#define hw(addr)                      \
  (*((volatile unsigned int*)(addr)))

#define sync()          \
  asm volatile(         \
    "sync       \n"     \
  )

#define clearTags()       \
  asm volatile (          \
    "mtc0 $0, $28   \n"   \
    "mtc0 $0, $29   \n"   \
    "sync           \n"   \
  )
  
#define delayPipeline()                    \
  asm volatile(                            \
    "nop; nop; nop; nop; nop; nop; nop \n" \
  )

#define suspendCpuIntr(var)    \
  asm volatile(                \
    ".set push             \n" \
    ".set noreorder        \n" \
    ".set volatile         \n" \
    ".set noat             \n" \
    "mfc0  %0, $12         \n" \
    "sync                  \n" \
    "li    $t0, 0xfffffffe \n" \
    "and   $t0, %0, $t0    \n" \
    "mtc0  $t0, $12        \n" \
    "sync                  \n" \
    "nop                   \n" \
    "nop                   \n" \
    "nop                   \n" \
    ".set pop              \n" \
    : "=r"(var)                \
    :                          \
    : "$t0", "memory"          \
  )

#define resumeCpuIntr(var) \
  asm volatile(            \
    ".set push      \n"    \
    ".set noreorder \n"    \
    ".set volatile  \n"    \
    ".set noat      \n"    \
    "mtc0  %0, $12  \n"    \
    "sync           \n"    \
    "nop            \n"    \
    "nop            \n"    \
    "nop            \n"    \
    ".set pop       \n"    \
    :                      \
    : "r"(var)             \
    : "memory"             \
  )

// Set clock domains to ratio 1:1
#define resetDomainRatios()          \
  sync();                            \
  hw(0xbc200000) = 511 << 16 | 511;  \
  hw(0xBC200004) = 511 << 16 | 511;  \
  hw(0xBC200008) = 511 << 16 | 511;  \
  sync();

// Wait for clock stability, signal propagation and pipeline drain
/*
 #define settle()            \
{                           \
  sync();                   \
  u32 i = 0x1fffff;         \
  while (--i) {             \
    delayPipeline();        \
  }                         \
}
*/
#define settle()                \
  asm volatile(                 \
    ".set push              \n" \
    ".set noreorder         \n" \
    ".set nomacro           \n" \
    ".set volatile          \n" \
    ".set noat              \n" \
                                \
    "sync                   \n" \
    "lui  $t0, 0x02         \n" \
    "ori  $t0, $t0, 0xffff  \n" \
                                \
    "1:                     \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  nop                  \n" \
    "  addiu $t0, $t0, -1   \n" \
    "  bnez  $t0, 1b        \n" \
    "  nop                  \n" \
                                \
    ".set pop               \n" \
    :                           \
    :                           \
    : "$t0", "memory"           \
  )

static inline void unlockMemory() {
  const u32 start = 0xbc000000;
  const u32 end   = 0xbc00002c;
  for (u32 reg = start; reg <= end; reg += 4) {
    hw(reg) = -1;
  }
  sync();
}

