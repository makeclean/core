#include "parma_stop.h"
#include "parma_commons.h"
#include <PCU.h>
#include <math.h>

namespace parma {
  BalOrStall::BalOrStall(Average* imb, Average* sides, double sidesTol, int v)
    : i(imb), s(sides), sTol(sidesTol), verbose(v) {}
  bool BalOrStall::stop(double imb, double maxImb) {
    const double iTol = (maxImb-1)*.01;
    const double iSlope = i->avg();
    const double sSlope = s->avg();
    if( !PCU_Comm_Self() && verbose )
      parmaCommons::status("imbSlope %f sidesSlope %f\n", iSlope, sSlope);
    return imb < maxImb || 
      ( fabs(iSlope) < iTol && fabs(sSlope) < sTol );
  }
}
