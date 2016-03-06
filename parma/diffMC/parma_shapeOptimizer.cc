#include <PCU.h>
#include <stdio.h>
#include <limits.h>
#include "parma.h"
#include "parma_balancer.h"
#include "parma_step.h"
#include "parma_sides.h"
#include "parma_weights.h"
#include "parma_centroids.h"
#include "parma_targets.h"
#include "parma_selector.h"
#include "parma_commons.h"

#include <sstream>
#include <string.h>

namespace {
  using parmaCommons::status;

  int getMinSide(parma::Sides* s) {
    int minSides = INT_MAX;
    s->begin();
    const parma::Sides::Item* side;
    while( (side = s->iterate()) ) 
      if(side->second < minSides &&side->second>0) {
        minSides = side->second;
      }
    s->end();
    return minSides;
  }
  int getSmallestSide(parma::Sides* s,bool isOutput=false) {
    int minSides=getMinSide(s);
    if (isOutput)
      PCU_Debug_Print("MinSide %d\n",minSides);
    int min=minSides;
    minSides = PCU_Min_Int(minSides);
    if (min==minSides&&isOutput)
      status("%d has smallest side\n",PCU_Comm_Self());
    return minSides;
  }

  double getAvgSides(parma::Sides* s) {
    double tot = PCU_Add_Double(s->total());
    int cnt = PCU_Add_Int(static_cast<int>(s->size()));
    return tot/cnt;
  }

  int si;
  class ImbOrLong : public parma::Stop {
    public:
      ImbOrLong(parma::Sides* s, double tol)
        : sides(s), sideTol(tol) {}
      bool stop(double imb, double maxImb) {
        const double small = static_cast<double>(getSmallestSide(sides,true));
        si++;
        if (!PCU_Comm_Self())
          status("Smallest Side: %f, endPoint: %f\n", small, sideTol);
        return imb > maxImb || small > sideTol;
      }
    private:
      parma::Sides* sides;
      double sideTol;
  };
  
  class ShapeOptimizer : public parma::Balancer {

    public:
      ShapeOptimizer(apf::Mesh* m, double f, int v)
        : Balancer(m, f, v, "gap") {
        if (!PCU_Comm_Self())
          status("Factor: %f\n",f);
        parma::Sides* s = parma::makeVtxSides(mesh);
        avgSide=getAvgSides(s);
        delete s;
        avgSideMult=0.2;
        iter=0;
        si=0;
        minSideMult=.05;
        misNumber = 0;
        maxMis = 0;
      }
      bool runStep(apf::MeshTag* wtag, double tolerance) {
        PCU_Debug_Print("Outer Iteration: %d Inner Iteration: %d\n",si,iter);
        if (!PCU_Comm_Self())
          status("Outer Iteration: %d Inner Iteration: %d\n",si,iter);
        Parma_ProcessDisconnectedParts(mesh);

        /*
        char name[128];
        sprintf(name,"pop_%d_%d",si,iter);
        apf::writeVtkFiles(name,mesh);
        */

        parma::Sides* s = parma::makeVtxSides(mesh);
        PCU_Debug_Print("%s\n", s->print("sides").c_str());
        
        parma::Weights* w =
          parma::makeEntWeights(mesh, wtag, s, mesh->getDimension());

        const double t1 = PCU_Time();
        misNumber = Parma_MisNumbering(mesh,0);
        double elapsedTime = PCU_Time() - t1;
        elapsedTime = PCU_Max_Double(elapsedTime);
        if( !PCU_Comm_Self() )
          status("mis completed in %f (seconds)\n", elapsedTime);
        maxMis = misNumber;
        PCU_Max_Ints(&maxMis,1);
        double small = static_cast<double>(getSmallestSide(s));
        if (small>=avgSide*avgSideMult||iter==0)
          avgSideMult+=(1-avgSideMult)/10;
        if (!PCU_Comm_Self()) {
          status("The average mult is now %f\n",avgSideMult);
          status("mis maxNum %d\n", maxMis);
        }

        parma::Targets* t = 
          parma::makeShapeTargets(mesh, s, w, factor, avgSideMult, avgSide,
                                  minSideMult, misNumber==iter);
        PCU_Debug_Print("%s\n", t->print("targets").c_str());
        iter++;
        if (iter>maxMis)
          iter=0;
        parma::Centroids c(mesh, wtag, s);
        parma::Selector* sel = parma::makeShapeSelector(mesh, wtag, &c);
        ImbOrLong* stopper = new ImbOrLong(s, avgSide*0.7);
        parma::Stepper b(mesh, factor, s, w, t, sel, "elm", stopper);
        return b.step(tolerance, verbose);
      }
    private:
      double minSideMult;
      double avgSide;
      double avgSideMult;
      int iter;
      int misNumber;
      int maxMis;
  };
}

apf::Balancer* Parma_MakeShapeOptimizer(apf::Mesh* m,
    double stepFactor, int verbosity) {
  return new ShapeOptimizer(m, stepFactor, verbosity);
}
