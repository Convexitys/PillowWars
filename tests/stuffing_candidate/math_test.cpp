#include "../../Source/PillowWars/PillowWarsResourceMath.h"
#include <cassert>
#include <cstdio>
using namespace PWResourceMath;
int main()
{
    int tests=0;
    for(int hz:{10,20,30,60,144}){
        RateCarry guard,refill;Units a=0,b=0;
        for(int i=0;i<10*hz;++i)a+=guard.Integrate(1.0/hz,8000);
        for(int i=0;i<2*hz;++i)b+=refill.Integrate(1.0/hz,24000);
        assert(a==80000&&b==48000);tests+=2;
    }
    RateCarry carry;Units total=0;
    for(int i=0;i<10000;++i)total+=carry.Integrate(.0000001,8000);
    assert(total==8);++tests;
    assert(carry.Integrate(-1,8000)==0);++tests;
    RateCarry hitch;assert(hitch.Integrate(1.5,8000)==12000);++tests;
    assert(Quantize(8)==8000&&Quantize(32)==32000&&Quantize(8*.1)==800);++tests;
    std::printf("%d production arithmetic assertions passed\n",tests);
}
