/*
 o-----------------------------------------------------------------------------o
 |
 | Dynamic maps implementation in mixed C/C++
 |
 | Methodical Accelerator Design - Copyright (c) 2016+
 | Support: http://cern.ch/mad  - mad at cern.ch
 | Authors: L. Deniau, laurent.deniau at cern.ch
 | Contrib: -
 |
 o-----------------------------------------------------------------------------o
 | You can redistribute this file and/or modify it under the terms of the GNU
 | General Public License GPLv3 (or later), as published by the Free Software
 | Foundation. This file is distributed in the hope that it will be useful, but
 | WITHOUT ANY WARRANTY OF ANY KIND. See http://gnu.org/licenses for details.
 o-----------------------------------------------------------------------------o
*/

#include "mad_tpsa.hpp"

extern "C" {
#include "mad_cst.h"
#include "mad_dynmap.h"
}

// --- types ------------------------------------------------------------------o

extern "C" {

typedef  num_t  par6_t[6];
typedef tpsa_t *map6_t[6];

struct mflw {
  num_t el, eld, beta, T;
  int    npar;
  par6_t *par;
  map6_t *map;
};

}

struct par_t {
  par_t(num_t *a, num_t b)
    : x(a[0]), px(a[1]), y(a[2]), py(a[3]), t(a[4]), pt(a[5]), beta(b) {}
  num_t &x, &px, &y, &py, &t, &pt;
  num_t beta;
};

struct map_t {
  map_t(tpsa_t **a, num_t b)
    : x(a[0]), px(a[1]), y(a[2]), py(a[3]), t(a[4]), pt(a[5]), beta(b) {}
  mad::tpsa_ref x, px, y, py, t, pt;
  num_t beta;
};

// --- implementation ---------------------------------------------------------o

using namespace mad;

// --- helpers ---

const num_t minlen = mad_cst_MINLEN;
const num_t minang = mad_cst_MINANG;
const num_t minstr = mad_cst_MINSTR;

inline num_t dp2 (par_t &p) {
  return 1 + (2/p.beta)*p.pt + sqr(p.pt);
}

inline tpsa dp2 (map_t &p) {
  return 1 + (2/p.beta)*p.pt + sqr(p.pt);
}

inline num_t pz2 (par_t &p) {
  return 1 + (2/p.beta)*p.pt + sqr(p.pt) - sqr(p.px) - sqr(p.py);
}

inline tpsa pz2 (map_t &p) {
  return 1 + (2/p.beta)*p.pt + sqr(p.pt) - sqr(p.px) - sqr(p.py);
}

inline num_t dp (par_t &p) { return std::sqrt(dp2(p)); }
inline num_t pz (par_t &p) { return std::sqrt(pz2(p)); }
inline tpsa  dp (map_t &p) { return mad::sqrt(dp2(p)); }
inline tpsa  pz (map_t &p) { return mad::sqrt(pz2(p)); }

// --- DKD maps ---

void
mad_trk_strex_drift_r (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)istp;
  if (std::abs(m->el*lw) < minlen) return;

  num_t l = m->el*lw, ld = m->eld*lw;
  int T = m->T;

  FOR(i,m->npar) {
    par_t p { m->par[i], m->beta };
    num_t l_pz = l/pz(p);

    p.x = p.x + p.px*l_pz;
    p.y = p.y + p.py*l_pz;
    p.t = p.t - l_pz*(1/p.beta+p.pt) + (1-T)*(ld/p.beta);
  }
}

void
mad_trk_strex_drift_t (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)istp;
  if (std::abs(m->el*lw) < minlen) return;

  num_t l = m->el*lw, ld = m->eld*lw;
  int T = m->T;

  FOR(i,m->npar) {
    map_t p { m->map[i], m->beta };
    const tpsa l_pz = l/pz(p);

    p.x = p.x + p.px*l_pz;
    p.y = p.y + p.py*l_pz;
    p.t = p.t - l_pz*(1/p.beta+p.pt) + (1-T)*(ld/p.beta);
  }
}

void
mad_trk_strex_kick_t (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)m; (void)lw; (void)istp;
}

void
mad_trk_curex_drift_t (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)m; (void)lw; (void)istp;
}

void
mad_trk_curex_kick_r (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)m; (void)lw; (void)istp;
}

void
mad_trk_curex_kick_t (elem_t *e, mflw_t *m, num_t lw, int istp)
{
  (void)e; (void)m; (void)lw; (void)istp;
}

void mad_trk_slice_r (elem_t *e, mflw_t *m, num_t lw, trkfun *dft, trkfun *kck)
{
  (void)e; (void)m; (void)lw; (void)dft; (void)kck;
}

void mad_trk_slice_t (elem_t *e, mflw_t *m, num_t lw, trkfun *dft, trkfun *kck)
{
  (void)e; (void)m; (void)lw; (void)dft; (void)kck;
}

// --- speed tests ------------------------------------------------------------o

#if TPSA_USE_TRC
#define TRC(...) printf(#__VA_ARGS__ "\n"); __VA_ARGS__
#else
#define TRC(...) __VA_ARGS__
#endif

void mad_trk_spdtest (int n, int k)
{
  mad_desc_newv(6, 1);

  tpsa x ( "X"); x .set( 0   , 1);
  tpsa px("PX"); px.set( 1e-7, 2);
  tpsa y ( "Y"); y .set( 0   , 3);
  tpsa py("PY"); py.set(-1e-7, 4);
  tpsa t ( "T"); t .set( 0   , 5);
  tpsa pt("PT"); pt.set( 0   , 6);

  par6_t par1 = { x[0], px[0], y[0], py[0], t[0], pt[0] };
  map6_t map1 = { x.ptr(), px.ptr(), y.ptr(), py.ptr(), t.ptr(), pt.ptr() };

  struct mflw m = {
    .el=1, .eld=1, .beta=1, .T=0,
    .npar=1, .par=&par1, .map=&map1,
  };

  switch(k) {
  case 0: {
    FOR(i,n) mad_trk_strex_drift_r (nullptr, &m, 1, 1);
    par_t p { m.par[0], m.beta };
    printf("x =% -.16e\npx=% -.16e\ny =% -.16e\npy=% -.16e\nt =% -.16e\npt=% -.16e\n",
            p.x, p.px, p.y, p.py, p.t, p.pt);
  } break;

  case 1: {
    FOR(i,n) mad_trk_strex_drift_t (nullptr, &m, 1, 1);
    map_t p { m.map[0], m.beta };
    stdout << p.x << p.px << p.y << p.py << p.t << p.pt;
  } break;

  default:
    printf("unknown use case %d\n", k);
  }
}

/*
time: 0.014369 sec
local t=os.clock() MAD._C.mad_trk_spdtest(1e6,0) print(os.clock()-t, "sec")

time: 0.444638 sec
MAD._C.mad_mcollect()
local t=os.clock() MAD._C.mad_trk_spdtest(1e6,1) print(os.clock()-t, "sec")
MAD._C.mad_mdump(nil)

time: 0.005795 sec
do
local m = {el=1, eld=1, beam={beta=1}, T=0, atdebug=\->(), npar=1,
{x=0,px=1e-7,y=0,py=-1e-7,t=0,pt=0}}
local f =\n=> for i=1,n do MAD.dynmap.strex_drift(nil,m,1,1) end end
m[1].px, m[1].py = 1e-7, -1e-7
local t=os.clock() f(1e6) print(os.clock()-t, "sec")
MAD.utility.printf("x =% -.16e\npx=% -.16e\ny =% -.16e\npy=% -.16e\nt =% -.16e\npt=% -.16e\n",
m[1].x, m[1].px, m[1].y, m[1].py, m[1].t, m[1].pt)
end

time: 2.766905 sec
do
local m = {el=1, eld=1, beam={beta=1}, T=0, atdebug=\->(), npar=1, MAD.damap()}
m[1].px, m[1].py = 1e-7, -1e-7
local f =\n=> for i=1,n do MAD.dynmap.strex_drift(nil,m,1,1) end end
local t=os.clock() f(1e6) print(os.clock()-t, "sec")
m[1]:print()
end
*/


// --- unit tests -------------------------------------------------------------o

void mad_trk_cpptest (void)
{
  mad_desc_newv(6, 1);

  TRC(tpsa a("A");                             )
  TRC(tpsa_ref ar(a.ptr());                    )

  TRC( a  = a;                                 )
  TRC( a += a;                                 )
  TRC( a  = ar;                                )
  TRC( a += ar;                                )
  TRC( a  = a+a;                               )
  TRC( a += a+a;                               )
  TRC( a  = 2*a;                               )
  TRC( a += 2*a;                               )
  TRC( a  = 1;                                 )
  TRC( a += 1;                                 )
  TRC( a  = tpsa();                            ) // tpsa(a);       // error
  TRC( a += tpsa();                            ) // tpsa(a);       // error
  TRC( a  = tpsa(a+a);                         )
  TRC( a += tpsa(a+a);                         )
  TRC( a  = tpsa_ref(a.ptr());                 ) // tpsa_ref(a);   // error
  TRC( a += tpsa_ref(a.ptr());                 ) // tpsa_ref(a+b); // error

  TRC( ar  = a;                                )
  TRC( ar += a;                                )
  TRC( ar  = ar;                               )
  TRC( ar += ar;                               )
  TRC( ar  = a+a;                              )
  TRC( ar += a+a;                              )
  TRC( ar  = 2*a;                              )
  TRC( ar += 2*a;                              )
  TRC( ar  = 1;                                )
  TRC( ar += 1;                                )
  TRC( ar  = tpsa();                           )  // tpsa(a);       // error
  TRC( ar += tpsa();                           )  // tpsa(a);       // error
  TRC( ar  = tpsa(a+a);                        )
  TRC( ar += tpsa(a+a);                        )
  TRC( ar  = tpsa_ref(a.ptr());                ) // tpsa_ref(a);   // error
  TRC( ar += tpsa_ref(a.ptr());                ) // tpsa_ref(a+b); // error

  TRC( tpsa()  = a;                            )
  TRC( tpsa() += a;                            )
  TRC( tpsa()  = ar;                           )
  TRC( tpsa() += ar;                           )
  TRC( tpsa()  = a+a;                          )
  TRC( tpsa() += a+a;                          )
  TRC( tpsa()  = 2*a;                          )
  TRC( tpsa() += 2*a;                          )
  TRC( tpsa()  = 1;                            )
  TRC( tpsa() += 1;                            )
  TRC( tpsa()  = tpsa();                       )  // tpsa(a); // error
  TRC( tpsa() += tpsa();                       )  // tpsa(a); // error
  TRC( tpsa()  = tpsa(a+a);                    )
  TRC( tpsa() += tpsa(a+a);                    )
  TRC( tpsa()  = tpsa_ref(a.ptr());            ) // tpsa_ref(a);   // error
  TRC( tpsa() += tpsa_ref(a.ptr());            ) // tpsa_ref(a+b); // error

  TRC( tpsa_ref(a.ptr())  = a;                 )
  TRC( tpsa_ref(a.ptr()) += a;                 )
  TRC( tpsa_ref(a.ptr())  = ar;                )
  TRC( tpsa_ref(a.ptr()) += ar;                )
  TRC( tpsa_ref(a.ptr())  = a+a;               )
  TRC( tpsa_ref(a.ptr()) += a+a;               )
  TRC( tpsa_ref(a.ptr())  = 2*a;               )
  TRC( tpsa_ref(a.ptr()) += 2*a;               )
  TRC( tpsa_ref(a.ptr())  = 1;                 )
  TRC( tpsa_ref(a.ptr()) += 1;                 )
  TRC( tpsa_ref(a.ptr())  = tpsa();            ) // tpsa(a);       // error
  TRC( tpsa_ref(a.ptr()) += tpsa();            ) // tpsa(a);       // error
  TRC( tpsa_ref(a.ptr())  = tpsa(a+a);         )
  TRC( tpsa_ref(a.ptr()) += tpsa(a+a);         )
  TRC( tpsa_ref(a.ptr())  = tpsa_ref(a.ptr()); ) // tpsa_ref(a);   // error
  TRC( tpsa_ref(a.ptr()) += tpsa_ref(a.ptr()); ) // tpsa_ref(a+b); // error

  TRC( const tpsa b {  a+1+a+2+a+3 };          )
  TRC( const tpsa c { (a+1)*sqr(a+2)+a*2 };    )

  TRC( const tpsa d =  a+1+a+2+a+2;            )
  TRC( const tpsa e = (a+1)*sqr(a+2)+a*2;      )

  TRC(       tpsa f =  a+1+a+2+a+2;            )
  TRC(       tpsa g = (a+1)*sqr(a+2)+a*2;      )
}