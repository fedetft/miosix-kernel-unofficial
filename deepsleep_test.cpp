#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/power_manager.h"

using namespace std;
using namespace miosix;

int main(int argc, char** argv) {
    PowerManager& pw=PowerManager::instance();
    
    for(long long i=1000000000;;i+=1500000000){
        printf("To sleep until %lld...",i);
        fflush(stdout);
        pw.deepSleepUntil(i);
        printf("[%lld] Wake up\n\n",getTime());
    }
    
    return 0;
}

