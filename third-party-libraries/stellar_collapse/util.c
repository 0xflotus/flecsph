/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2018 Triad National Security, LLC
 * All rights reserved.
 * --------------------------------------------------------------------------~*/

/******************************************************************************
 *                                                                            *
 * UTIL.C                                                                     *
 *                                                                            *
 * BROADLY USEFUL UTILITY FUNCTIONS                                           *
 *                                                                            *
 ******************************************************************************/

#include "decs.h"

void *safe_malloc(int size)
{
  // malloc(0) may or may not return NULL, depending on compiler.
  if (size == 0) return NULL;

  void *A = malloc(size);
  if (A == NULL) {
    fprintf(stderr, "Failed to malloc\n");
    exit(-1);
  }
  return A;
}

// Error-handling wrappers for standard C functions
void safe_system(const char *command)
{
  int systemReturn = system(command);
  if (systemReturn == -1) {
    fprintf(stderr, "system() call %s failed! Exiting!\n", command);
    exit(-1);
  }
}

void safe_fscanf(FILE *stream, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int vfscanfReturn = vfscanf(stream, format, args);
  va_end(args);
  if (vfscanfReturn == -1) {
    fprintf(stderr, "fscanf() call failed! Exiting!\n");
    exit(-1);
  }
}

double find_min(const double* array, int size) {
  double min = INFINITY;
  for (int i = 0; i < size; i++) {
    if (array[i] < min) min = array[i];
  }
  return min;
}

double find_max(const double* array, int size) {
  double max = -INFINITY;
  for (int i = 0; i < size; i++) {
    if (array[i] > max) max = array[i];
  }
  return max;
}

int find_index(double value, const double* array, int size)
{
  for (int i = 0; i < size; i++) {
    if (array[i] >= value) return i;
  }
  return -1;
}

// A very general 1D linear interpolator
double interp_1d(double x,
		 const double xmin, const double xmax,
		 const int imin, const int imax,
		 const double* restrict tab_x,
		 const double* restrict tab_y)
{
  if (x < xmin) x = xmin;
  if (x > xmax) x = xmax-SMALL;
  const int nx = imax - imin;
  const double dx = (xmax - xmin)/(nx-1);
  const int ix = imin + (x - xmin)/dx;
  const double delx = (x - tab_x[ix])/dx;
  const double out = (1-delx)*tab_y[ix] + delx*tab_y[ix+1];
  /*
  // DEBUGGING
  if (isnan(out) || x > xmax || x < xmin) {
    fprintf(stderr,"[interp_1d]: out is NaN!\n");
    fprintf(stderr,"\t\tx           = %g\n",x);
    fprintf(stderr,"\t\timin        = %d\n",imin);
    fprintf(stderr,"\t\timax        = %d\n",imax);
    fprintf(stderr,"\t\txmin        = %e\n",xmin);
    fprintf(stderr,"\t\txmax        = %e\n",xmax);
    fprintf(stderr,"\t\tnx          = %d\n",nx);
    fprintf(stderr,"\t\tdx          = %e\n",dx);
    fprintf(stderr,"\t\tix          = %d\n",ix);
    fprintf(stderr,"\t\tdelx        = %e\n",delx);
    fprintf(stderr,"\t\ttab_x[ix]   = %e\n",tab_x[ix]);
    fprintf(stderr,"\t\ttab_x[ix+1] = %e\n",tab_x[ix+1]);
    fprintf(stderr,"\t\ttab_y[ix]   = %e\n",tab_y[ix]);
    fprintf(stderr,"\t\ttab_y[ix+1] = %e\n",tab_y[ix+1]);
    fprintf(stderr,"\n");
    exit(1);
  }
  */
  return out;
}

#if NEED_UNITS
void set_units()
{
  #if METRIC == MKS
  L_unit = GNEWT*Mbh/(CL*CL);
  #endif
  T_unit = L_unit/CL;
  RHO_unit = M_unit*pow(L_unit,-3.);
  U_unit = RHO_unit*CL*CL;
  B_unit = CL*sqrt(4.*M_PI*RHO_unit);
  #if EOS == EOS_TYPE_TABLE
  TEMP_unit = MEV;
  #endif

  #if RADIATION
  Ne_unit = RHO_unit/(MP + ME);
  kphys_to_num = ME/M_unit;
  //(RADIATION == RADTYPE_LIGHT) ? ME/M_unit : MP/M_unit;
  // kphys_to_num = MBary/M_unit;
  #if ELECTRONS
  Thetae_unit = MP/ME;
  #else
  Thetae_unit = EOS_Theta_unit();
  #endif // ELECTRONS
  #endif // RADIATION

  #if EOS == EOS_TYPE_TABLE && POLYTROPE_FALLBACK
  rho_poly_thresh = EOS_SC_get_min_rho();
  #endif
}
#endif // NEED UNITbS
