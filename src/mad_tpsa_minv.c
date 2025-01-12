/*
 o-----------------------------------------------------------------------------o
 |
 | TPSA map inversion module implementation
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

#include <assert.h>

#include "mad_mem.h"
#include "mad_vec.h"
#include "mad_mat.h"
#include "mad_desc_impl.h"

#ifdef    MAD_CTPSA_IMPL
#include "mad_ctpsa_impl.h"
#else
#include "mad_tpsa_impl.h"
#endif

// --- local ------------------------------------------------------------------o

#define TC (const T**)

static inline void
check_same_desc(ssz_t na, const T *ma[na])
{
  assert(ma);
  FOR(i,1,na)
    ensure(ma[i]->d == ma[i-1]->d, "inconsistent GTPSAs (descriptors differ)");
}

static inline void
check_minv(ssz_t na, const T *ma[na], ssz_t nb, T *mc[na])
{
  ensure(na <= ma[0]->d->nn, "invalid na > #vars+#params");
  ensure(nb <= ma[0]->d->nv, "invalid nb > #vars");
  check_same_desc(na,   ma);
  check_same_desc(na,TC mc);
  ensure(ma[0]->d == mc[0]->d, "incompatible GTPSAs (descriptors differ)");
}

static void
split_and_inv(const D *d, ssz_t na, const T *ma[na], ssz_t nb, T *lininv[na], T *nonlin[na])
{
  ssz_t nn = na, nv = nb, np = na-nb;  // #vars+params, #vars, #params
  mad_alloc_tmp(NUM, mat_var , nv*nv); // canonical vars
  mad_alloc_tmp(NUM, mat_vari, nv*nv); // inverse of vars
  mad_alloc_tmp(NUM, mat_par , nv*np); // params
  mad_alloc_tmp(NUM, mat_pari, nv*np); // 'inverse' of params

  // split linear, (-1 * nonlinear)
  FOR(i,nv) {
    T *t = nonlin[i];

    idx_t v = 0;
    for (; v < nv; ++v) mat_var[i*nv +  v    ] = ma[i]->coef[v+1];
    for (; v < nn; ++v) mat_par[i*np + (v-nv)] = ma[i]->coef[v+1];

    FUN(copy)(ma[i], t);

    // clear constant and linear part of coef
    FOR(c,d->ord2idx[2]) t->coef[c] = 0;

    // clear constant and linear part of nz, adjust lo, hi
    t->nz = mad_bit_mclr(t->nz,3);
    if (t->nz) {
      t->lo = mad_bit_lowest (t->nz);
      t->hi = mad_bit_highest(t->nz);
    } else FUN(reset0)(t);

    FUN(scl)(t,-1,t);
  }

  // invert linear part: mat_vari = mat_var^-1
# ifndef MAD_CTPSA_IMPL
  mad_mat_invn(mat_var, 1, mat_vari, nv, nv, -1);
  if (np != 0) {
    // mat_pari = - mat_vari * mat_par
    mad_mat_mul(mat_vari, mat_par, mat_pari, nv, np, nv);
    mad_vec_muln(mat_pari, -1, mat_pari, nv*np);
  }
# else
  mad_cmat_invn(mat_var, 1, mat_vari, nv, nv, -1);
  if (np != 0) {
    // mat_pari = - mat_vari * mat_par
    mad_cmat_mul(mat_vari, mat_par, mat_pari, nv, np, nv);
    mad_cvec_muln(mat_pari, -1, mat_pari, nv*np);
  }
# endif

  // copy result into TPSA
  FOR(i,nv) {
    T *t = lininv[i];
    FOR(v,nv) FUN(seti)(t, v   +1, 0, mat_vari[i*nv + v]);
    FOR(p,np) FUN(seti)(t, p+nv+1, 0, mat_pari[i*np + p]);
  }

  mad_free_tmp(mat_var );
  mad_free_tmp(mat_vari);
  mad_free_tmp(mat_par );
  mad_free_tmp(mat_pari);
}

// --- public -----------------------------------------------------------------o

void
FUN(minv) (ssz_t na, const T *ma[na], ssz_t nb, T *mc[na])
{
  DBGFUN(->);
  assert(ma && mc);
  ensure(na >= nb, "invalid subtitution ranks, na >= nb expected");
  check_minv(na, ma, nb, mc);
  FOR(i,na) {
    DBGTPSA(ma[i]); DBGTPSA(mc[i]);
    ensure(mad_bit_tst(ma[i]->nz,1),
           "invalid rank-deficient map (1st order has zero row)");
  }

  const D *d = ma[0]->d;
  T *lininv[na], *nonlin[na], *tmp[na];
  FOR(i,nb) { // vars
    lininv[i] = FUN(newd)(d,1);
    nonlin[i] = FUN(new)(ma[i], mad_tpsa_same);
    tmp[i]    = FUN(new)(ma[i], mad_tpsa_same);
  }
  FOR(i,nb,na) { // params
    lininv[i] = (T*)ma[i];
    nonlin[i] = (T*)ma[i];
    tmp[i]    = (T*)ma[i];
  }

  split_and_inv(d, na, ma, nb, lininv, nonlin);

  // iteratively compute higher orders of the inverse:
  // al  = mc[ord=1]
  // anl = mc[ord>1]
  // mc[ord=1] = al^-1
  // mc[ord=i] = al^-1 o ( anl o mc[ord=i-1] + id ) ; i > 1

  log_t isnul = TRUE;
  FOR(i,nb) {
    FUN(copy)(lininv[i], mc[i]);
    isnul &= FUN(isnul)(nonlin[i]);
  }

  if (!isnul) {
    ord_t o_prev = mad_desc_gtrunc(d, 1);
    for (ord_t o = 2; o <= d->mo; ++o) {
      mad_desc_gtrunc(d, o);
      FUN(compose)(nb, TC nonlin, na, TC mc, tmp);

      for (idx_t v = 0; v < nb; ++v) FUN(seti)(tmp[v], v+1, 1,1); // add identity

      FUN(compose)(nb, TC lininv, na, TC tmp, mc);
    }
    mad_desc_gtrunc(d, o_prev);
  }

  // cleanup
  FOR(i,nb) {
    FUN(del)(lininv[i]);
    FUN(del)(nonlin[i]);
    FUN(del)(tmp[i]);
    DBGTPSA (mc [i]);
  }
  DBGFUN(<-);
}

void
FUN(pminv) (ssz_t na, const T *ma[na], ssz_t nb, T *mc[na], idx_t select[na])
{
  DBGFUN(->);
  assert(ma && mc && select);
  ensure(na >= nb, "invalid subtitution rank, na >= nb expected");
  check_minv(na, ma, nb, mc);
  FOR(i,na) {
    if (select[i]) {
      DBGTPSA(ma[i]); DBGTPSA(mc[i]);
      ensure(mad_bit_tst(ma[i]->nz,1),
             "invalid rank-deficient map (1st order has zero row)");
    }
  }

  // split input map into rows that are inverted and rows that are not

  const D *d = ma[0]->d;
  T *mUsed[na], *mUnused[na], *mInv[na];
  FOR(i,nb) { // vars
    if (select[i]) {
      mUsed  [i] = FUN(new) (ma[i], mad_tpsa_same);
      mInv   [i] = FUN(new) (ma[i], mad_tpsa_same);
      mUnused[i] = FUN(newd)(d,1);
      FUN(copy)(ma[i],mUsed[i]);
      FUN(seti)(mUnused[i], i+1,  0,1); // set identity
    }
    else {
      mUsed  [i] = FUN(newd)(d,1);
      mInv   [i] = FUN(newd)(d,1);
      mUnused[i] = FUN(new) (ma[i], mad_tpsa_same);
      FUN(copy)(ma[i],mUnused[i]);
      FUN(seti)(mUsed[i], i+1, 0,1); // set identity
    }
    FUN(set0)(mUsed  [i], 0,0);
    FUN(set0)(mUnused[i], 0,0);
  }
  FOR(i,nb,na) { // params
    mUsed  [i] = (T*)ma[i];
    mInv   [i] = (T*)ma[i];
    mUnused[i] = (T*)ma[i];
  }

  FUN(minv)   (na, TC mUsed  , nb,    mInv);
  FUN(compose)(nb, TC mUnused, na, TC mInv, mc);

  FOR(i,nb) {
    FUN(del)(mUsed[i]);
    FUN(del)(mUnused[i]);
    FUN(del)(mInv[i]);
    DBGTPSA(mc[i]);
  }
  DBGFUN(<-);
}

#undef TC

// --- end --------------------------------------------------------------------o
