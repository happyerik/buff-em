/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of BUFF-EM.
 *
 * BUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * BUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * FrequencyIntegral.cc -- buff-neq module for numerical quadrature 
 *                      -- over frequencies
 *
 * homer reid           -- 5/2012
 *
 */

#include <stdlib.h>
#include <libSGJC.h>
#include "buff-neq.h"

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteDataToOutputFile(BNEQData *BNEQD, double *I, double *E)
{
  time_t MyTime;
  struct tm *MyTm;
  char TimeString[30];
  
  MyTime=time(0);
  MyTm=localtime(&MyTime);
  strftime(TimeString,30,"%D::%T",MyTm);
  FILE *f=vfopen("%s.out","a",BNEQD->FileBase);
  fprintf(f,"\n");
  fprintf(f,"# buff-neq run on %s (%s)\n",GetHostName(),TimeString);
  fprintf(f,"# data file columns: \n");
  fprintf(f,"# 1 transform tag\n");
  fprintf(f,"# 2 (sourceObject, destObject) \n");
  int nq=3;
  if (BNEQD->QuantityFlags & QFLAG_PABS)
   { fprintf(f,"# (%i,%i) power (value,error)\n",nq,nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_XFORCE) 
   { fprintf(f,"# (%i,%i) x-force (value,error)\n",nq, nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_YFORCE) 
   { fprintf(f,"# (%i,%i) y-force (value,error)\n",nq, nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_ZFORCE) 
   { fprintf(f,"# (%i,%i) z-force (value,error)\n",nq, nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_XTORQUE) 
   { fprintf(f,"# (%i,%i) x-torque (value,error)\n",nq, nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_YTORQUE) 
   { fprintf(f,"# (%i,%i) y-torque (value,error)\n",nq, nq+1); nq+=2; }
  if (BNEQD->QuantityFlags & QFLAG_ZTORQUE) 
   { fprintf(f,"# (%i,%i) z-torque (value,error)\n",nq, nq+1); nq+=2; }

  int NO = BNEQD->G->NumObjects;
  int NT = BNEQD->NumTransformations;
  int NQ = BNEQD->NQ;
  double TotalQuantity[MAXQUANTITIES], TotalError[MAXQUANTITIES];
  for(int nt=0; nt<NT; nt++)
   for(int nsd=0; nsd<NO; nsd++)
    { 
      memset(TotalQuantity,0,NQ*sizeof(double));
      memset(TotalError,   0,NQ*sizeof(double));
      for(int nss=0; nss<NO; nss++)
       { fprintf(f,"%s %i%i ",BNEQD->GTCList[nt]->Tag,nss+1,nsd+1);
         for(nq=0; nq<NQ; nq++)
          { int i = GetNEQIIndex(BNEQD, nt, nss, nsd, nq);
            fprintf(f,"%+16.8e %+16.8e ", I[i], E[i] );
            TotalQuantity[nq] += I[i];
            TotalError[nq] += E[i];
          };
         fprintf(f,"\n");
       };

      fprintf(f,"%s 0%i ",BNEQD->GTCList[nt]->Tag,nsd+1);
      for(nq=0; nq<NQ; nq++)
       fprintf(f,"%e %e ",TotalQuantity[nq],TotalError[nq]);
      fprintf(f,"\n");

    };
  fclose(f);   
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
typedef struct FIData 
 {
   BNEQData *BNEQD;
   double OmegaMin;
   int Infinite;

 } FIData;

int SGJCIntegrand(unsigned ndim, const double *x, void *params,
                   unsigned fdim, double *fval)
{
  (void) ndim; // unused
  (void) fdim;

  FIData *FID         = (FIData *)params;

  BNEQData *BNEQD     = FID->BNEQD; 
  double OmegaMin     = FID->OmegaMin;
  int Infinite        = FID->Infinite;

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  double Omega, Jacobian; 
  if(Infinite)
   { Omega    = OmegaMin + x[0] / (1.0-x[0]);
     Jacobian = 1.0 / ( (1.0-x[0]) * (1.0-x[0]) );
   }
  else
   { Omega    = x[0];
     Jacobian = 1.0;
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  GetNEQIntegrand(BNEQD, Omega, fval);
  for(unsigned int nf=0; nf<fdim; nf++)
   fval[nf]*=Jacobian;

  return 0;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void EvaluateFrequencyIntegral_Adaptive(BNEQData *BNEQD,
                                        double OmegaMin, double OmegaMax,
                                        double AbsTol, double RelTol)
{ 
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  FIData MyFIData, *FID=&MyFIData;
  FID->BNEQD = BNEQD;
  FID->OmegaMin=OmegaMin;

  if (OmegaMax==-1.0)
   { FID->Infinite=1; 
     OmegaMin=0.0;
     OmegaMax=1.0;
   }
  else
   FID->Infinite=0;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  SWGGeometry *G = BNEQD -> G;
  int NO = G->NumObjects;
  int NT = BNEQD->NumTransformations;
  int NQ = BNEQD->NQ;
  int fdim = NT*NO*NO*NQ;
  double *I = new double[ fdim ];
  double *E = new double[ fdim ];

  pcubature_log(fdim, SGJCIntegrand, (void *)FID, 1, &OmegaMin, &OmegaMax,
                1000, AbsTol, RelTol, ERROR_INDIVIDUAL, I, E, "buff-neq.SGJClog");

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  WriteDataToOutputFile(BNEQD, I, E);

  delete[] I;
  delete[] E;

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void EvaluateFrequencyIntegral_TrapSimp(BNEQData *BNEQD,
                                        double OmegaMin, 
                                        double OmegaMax,
                                        int NumIntervals)
{ 
  SWGGeometry *G = BNEQD -> G;
  int NO   = G->NumObjects;
  int NT   = BNEQD->NumTransformations;
  int NQ   = BNEQD->NQ;
  int fdim = NT*NO*NO*NQ;

  double *I      = new double[fdim];
  double *E      = new double[fdim];
  double *fLeft  = new double[fdim];
  double *fMid   = new double[fdim];
  double *fRight = new double[fdim];
  double Omega;

#if 0
  if ( OmegaMax == -1.0 )
   { 
     // if the user didn't specify an upper frequency bound, we choose 
     // OmegaMax to be the frequency at which Theta(OmegaMax, TMax) has 
     // decayed to 10^{-10} of the value of Theta(0,TMax), where TMax 
     // is the largest temperature of any object. 
     // note: the function x/(exp(x)-1) falls below 10^{-10} at x=26.3.

     double TMax=TEnvironment;
     for (int ns=0; ns<NO; ns++)
      if ( TObjects[ns]>=0.0 && TObjects[ns]>TMax ) 
       TMax=TObjects[ns];

     if (TMax==0.0) // all temperatures are zero; no point in calculating
      { memset(I,0,fdim*sizeof(double));
        memset(E,0,fdim*sizeof(double));
        return;
      };  

     OmegaMax = 26.3*BOLTZMANNK*TMax;
     Log("Integrating to a maximum frequency of k=%e um^{-1} (w=%e rad/sec)\n",OmegaMax,OmegaMax*3.0e14);

   };
#endif

  /*--------------------------------------------------------------*/
  /*- evaluate integrand at leftmost point.  ---------------------*/
  /*- If the leftmost point is less than OMEGAMIN (the smallest  -*/
  /*- frequency at which we can do reliable calculations) then   -*/
  /*- we estimate the integral from OmegaMin to OMEGAMIN by      -*/
  /*- a rectangular rule with integrand values computed at       -*/
  /*- OMEGAMIN.                                                  -*/
  /*--------------------------------------------------------------*/
  #define OMEGAMIN 0.01
  if (OmegaMin < OMEGAMIN)
   { 
     Omega=OMEGAMIN;
     GetNEQIntegrand(BNEQD, Omega, fLeft);
     for(int nf=0; nf<fdim; nf++)
      { I[nf] = fLeft[nf] * (OMEGAMIN-OmegaMin);
        E[nf] = 0.0;
      };
     OmegaMin=OMEGAMIN;
   }
  else
   { 
     Omega=OmegaMin;
     GetNEQIntegrand(BNEQD, Omega, fLeft);
     memset(I, 0, fdim*sizeof(double));
     memset(E, 0, fdim*sizeof(double));
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  double Delta = (OmegaMax - OmegaMin) / NumIntervals;
  double *ISimp  = new double[fdim];
  double *ITrap  = new double[fdim];
  for(int nIntervals=0; nIntervals<NumIntervals; nIntervals++)
   { 
     // evaluate integrand at midpoint of interval 
     Omega += 0.5*Delta;
     GetNEQIntegrand(BNEQD, Omega, fMid);

     // evaluate integrand at right end of interval 
     Omega += 0.5*Delta;
     GetNEQIntegrand(BNEQD, Omega, fRight);

     // compute the simpson's rule and trapezoidal rule
     // estimates of the integral over this interval
     // and take their difference as the error
     for(int nf=0; nf<fdim; nf++)
      { ISimp[nf] = (fLeft[nf] + 4.0*fMid[nf] + fRight[nf])*Delta/6.0;
        ITrap[nf] = (fLeft[nf] + 2.0*fMid[nf] + fRight[nf])*Delta/4.0;
        I[nf] += ISimp[nf];
        E[nf] += fabs(ISimp[nf] - ITrap[nf]);
      };

     // prepare for next iteration
     memcpy(fLeft, fRight, fdim*sizeof(double));

   };
  delete[] fLeft;
  delete[] fMid;
  delete[] fRight;
  delete[] ISimp;
  delete[] ITrap;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  WriteDataToOutputFile(BNEQD, I, E);

  delete[] I;
  delete[] E;

}
