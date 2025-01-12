/*
 o-----------------------------------------------------------------------------o
 |
 | TPSA map composition module implementation
 |
 | Methodical Accelerator Design - Copyright (c) 2016+
 | Support: http://cern.ch/mad  - mad at cern.ch
 | Authors: L. Deniau, laurent.deniau at cern.ch
 |          C. Tomoiaga
 | Contrib: -
 |
 o-----------------------------------------------------------------------------o
 | You can redistribute this file and/or modify it under the terms of the GNU
 | General Public License GPLv3 (or later), as published by the Free Software
 | Foundation. This file is distributed in the hope that it will be useful, but
 | WITHOUT ANY WARRANTY OF ANY KIND. See http://gnu.org/licenses for details.
 o-----------------------------------------------------------------------------o
*/

#include <math.h>
#include <string.h>
#include <assert.h>

#include "mad_mem.h"
#include "mad_desc_impl.h"

#ifdef    MAD_CTPSA_IMPL
#include "mad_ctpsa_impl.h"
#else
#include "mad_tpsa_impl.h"
#endif

#define DEBUG_COMPOSE 0

// --- local ------------------------------------------------------------------o

static inline void
check_same_desc (ssz_t sa, const T *ma[sa])
{
  assert(ma);
  FOR(i,1,sa)
    ensure(ma[i]->d == ma[i-1]->d, "incompatibles GTPSA (descriptors differ)");
}

static inline void
check_compose (ssz_t sa, const T *ma[sa], ssz_t sb, const T *mb[sb], T *mc[sa])
{
  assert(ma && mb && mc);
  ensure(sa>0 && sb>0, "invalid map sizes (zero or negative sizes)");
  check_same_desc(sa,ma);
  check_same_desc(sb,mb);
  check_same_desc(sa,(const T**)mc);
  ensure(sb >= ma[0]->d->nv  , "incompatibles damap #B < NV(A)");
  ensure(sb <= ma[0]->d->nn  , "incompatibles damap #B > NV(A)+NP(A)");
  ensure(ma[0]->d == mb[0]->d, "incompatibles damap A vs C (descriptors differ)");
  ensure(ma[0]->d == mc[0]->d, "incompatibles damap A vs B (descriptors differ)");
}

static inline void
print_damap (ssz_t sa, const T *ma[sa], FILE *fp_)
{
  char nam[NAMSZ];

  if (!fp_) fp_ = stdout;

  FOR(i,sa) {
    strcpy(nam, ma[i]->nam);
    if (!nam[0]) snprintf(nam, sizeof(nam), "#%d", i+1);
    FUN(print)(ma[i], nam, 1e-15, 0, fp_);
  }

  (void)print_damap;
}

#ifdef _OPENMP
//#include "mad_tpsa_comp_p.tc" // obsolete
#endif
#include "mad_tpsa_comp_s.tc"

// --- public -----------------------------------------------------------------o

void
FUN(compose) (ssz_t sa, const T *ma[sa], ssz_t sb, const T *mb[sb], T *mc[sa])
{
  DBGFUN(->);
  check_compose(sa, ma, sb, mb, mc);

  const D *d = ma[0]->d;

  // handle aliasing
  mad_alloc_tmp(T*, mc_, sa);
  FOR(ib,sb) DBGTPSA(mb[ib]);
  FOR(ic,sa) {
    DBGTPSA(ma[ic]); DBGTPSA(mc[ic]);
    mc_[ic] = FUN(newd)(d, d->to);
  }

  ord_t hi_ord = 0;
  FOR(i,sa) if (ma[i]->hi > hi_ord) hi_ord = ma[i]->hi;
  hi_ord = MIN(hi_ord, d->to);

  #ifdef _OPENMP
  if (hi_ord >= 5) {
    #pragma omp parallel for schedule(dynamic)
    FOR(ia,sa) {
#if DEBUG_COMPOSE
    printf("compose: thread no %d\n", omp_get_thread_num());
#endif
      compose_serial(1,ma+ia,sb,mb,mc_+ia,ma[ia]->hi);
//    compose_parallel(sa,ma,sb,mb,mc_,hi_ord);
    }
  } else
  #endif // _OPENMP
    compose_serial(sa,ma,sb,mb,mc_,hi_ord);

  // copy back
  FOR(ic,sa) {
    FUN(copy)(mc_[ic], mc[ic]);
    FUN(del )(mc_[ic]);
    DBGTPSA(mc[ic]);
  }
  mad_free_tmp(mc_);
  DBGFUN(<-);
}

void
FUN(translate) (ssz_t sa, const T *ma[sa], ssz_t sb, const NUM tb[sb], T *mc[sa])
{
  assert(ma && tb); DBGFUN(->);
  ensure(sb>0, "invalid vector sizes (zero or negative sizes)");

  // transform translation vector into damap of order 1
  mad_alloc_tmp(const T*, mb, sb);
  FOR(ib,sb) {
    T *t = FUN(newd)(ma[0]->d, 1);
    FUN(setvar)(t, tb[ib], ib+1, 0);
    mb[ib] = t;
  }

  FUN(compose)(sa, ma, sb, mb, mc);

  // cleanup
  FOR(ib,sb) FUN(del)(mb[ib]);
  mad_free_tmp(mb);
  DBGFUN(<-);
}

void
FUN(eval) (ssz_t sa, const T *ma[sa], ssz_t sb, const NUM tb[sb], NUM tc[sa])
{
  assert(ma && tb && tc); DBGFUN(->);
  ensure(sa>0 && sb>0, "invalid map/vector sizes (zero or negative sizes)");
  ensure(sb >= ma[0]->d->nv, "incompatibles GTPSA (number of map variables differ)");

  // transform vectors into damap of order 0
  mad_alloc_tmp(const T*, mb, sb);
  mad_alloc_tmp(      T*, mc, sa);
  FOR(ib,sb) {
    T *t = FUN(newd)(ma[0]->d, 0);
    FUN(setvar)(t, tb[ib], 0,0);
    mb[ib] = t;
  }
  FOR(ic,sa) {
    T *t = FUN(newd)(ma[0]->d, 0);
    FUN(setvar)(t, tc[ic], 0,0);
    mc[ic] = t;
  }

  FUN(compose)(sa, ma, sb, mb, mc);

  // cleanup, save result
  FOR(ib,sb) FUN(del)(mb[ib]);
  FOR(ic,sa) {
    tc[ic] = mc[ic]->coef[0];
    FUN(del)(mc[ic]);
  }
  mad_free_tmp(mb);
  mad_free_tmp(mc);
  DBGFUN(<-);
}

// --- end --------------------------------------------------------------------o
