#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
// Server quantities are thousandths. Floats elsewhere are display projections.
namespace PWResourceMath
{
    using Units=std::int64_t;
    constexpr Units Capacity=100000;
    inline Units Quantize(double Value){return std::max<Units>(0,std::llround(Value*1000.0));}
    struct RateCarry
    {
        double Remainder=0;
        Units Integrate(double Seconds,double Rate)
        {
            if(!std::isfinite(Seconds)||Seconds<0||!std::isfinite(Rate)||Rate<0)return 0;
            const double Total=Seconds*Rate+Remainder;
            const Units Whole=static_cast<Units>(std::floor(Total+1.e-8));
            Remainder=std::max(0.0,Total-static_cast<double>(Whole));return Whole;
        }
        void Reset(){Remainder=0;}
    };
}
