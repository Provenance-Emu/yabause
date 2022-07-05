#include "../snddummy.c"

#ifdef HAVE_THREADS
#include "../thr-rthreads.c"
#else
#include "../thr-dummy.c"
#endif

#include "../titan/titan.c"
#include "../vdp1.c"
#include "../vdp2.c"
#include "../vidshared.c"
#include "../vidsoft.c"
#include "../yabause.c"

#include "libretro.c"
