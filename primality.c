#include <gmp.h>
#include "ptypes.h"

#include "primality.h"
#include "gmp_main.h"  /* primality_pretest */
#include "bls75.h"
#include "ecpp.h"
#include "factor.h"

#define FUNC_is_perfect_square 1
#define FUNC_mpz_logn
#include "utility.h"

#define NSMALLPRIMES 54
static const unsigned char sprimes[NSMALLPRIMES] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251};


static INLINE int _GMP_miller_rabin_ui(mpz_t n, UV base)
{
  int rval;
  mpz_t a;
  mpz_init_set_ui(a, base);
  rval = _GMP_miller_rabin(n, a);
  mpz_clear(a);
  return rval;
}

int _GMP_miller_rabin_random(mpz_t n, UV numbases, char* seedstr)
{
  gmp_randstate_t* p_randstate = get_randstate();
  mpz_t t, base;
  UV i;

  if (numbases == 0)  return 1;
  if (mpz_cmp_ui(n, 100) < 0)     /* tiny n */
    return (_GMP_is_prob_prime(n) > 0);

  mpz_init(base);  mpz_init(t);

  if (seedstr != 0) { /* Set the RNG seed if they gave us a seed */
    mpz_set_str(t, seedstr, 0);
    gmp_randseed(*p_randstate, t);
  }

  mpz_sub_ui(t, n, 3);
  for (i = 0; i < numbases; i++) {
    mpz_urandomm(base, *p_randstate, t);  /* base = 0 .. (n-3)-1 */
    mpz_add_ui(base, base, 2);            /* base = 2 .. n-2     */
    if (_GMP_miller_rabin(n, base) == 0)
      break;
  }
  mpz_clear(base);  mpz_clear(t);
  return (i >= numbases);
}

int _GMP_miller_rabin(mpz_t n, mpz_t a)
{
  mpz_t nminus1, d, x;
  UV s, r;
  int rval;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }
  if (mpz_cmp_ui(a, 1) <= 0)
    croak("Base %ld is invalid", mpz_get_si(a));
  mpz_init_set(nminus1, n);
  mpz_sub_ui(nminus1, nminus1, 1);
  mpz_init_set(x, a);

  /* Handle large and small bases.  Use x so we don't modify their input a. */
  if (mpz_cmp(x, n) >= 0)
    mpz_mod(x, x, n);
  if ( (mpz_cmp_ui(x, 1) <= 0) || (mpz_cmp(x, nminus1) >= 0) ) {
    mpz_clear(nminus1);
    mpz_clear(x);
    return 1;
  }

  mpz_init_set(d, nminus1);
  s = mpz_scan1(d, 0);
  mpz_tdiv_q_2exp(d, d, s);

  mpz_powm(x, x, d, n);
  mpz_clear(d); /* done with a and d */
  rval = 0;
  if (!mpz_cmp_ui(x, 1) || !mpz_cmp(x, nminus1)) {
    rval = 1;
  } else {
    for (r = 1; r < s; r++) {
      mpz_powm_ui(x, x, 2, n);
      if (!mpz_cmp_ui(x, 1)) {
        break;
      }
      if (!mpz_cmp(x, nminus1)) {
        rval = 1;
        break;
      }
    }
  }
  mpz_clear(nminus1); mpz_clear(x);
  return rval;
}

int is_miller_prime(mpz_t n, int assume_grh)
{
  mpz_t nminus1, d, x;
  UV s, r, maxa, a;
  int rval;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }

  if (mpz_cmp_ui(n, 1373653) < 0) {
    maxa = 3;
  } else if (assume_grh) {
    double logn = mpz_logn(n);
    double dmaxa = 2 * logn * logn;  /* Bach (1990) */
    /* Wedeniwski (2001) claims the following, but it might be wrong
     * double dmaxa = 1.5L * logn * logn - 44.0L/5.0L * logn + 13; */
    if (dmaxa >= (double)ULONG_MAX)
      croak("is_miller_prime: n is too large for GRH DMR");
    maxa = ceil(dmaxa);
  } else { /* Bober and Goldmakher 2015 (http://arxiv.org/abs/1311.7556) */
    /* n_p < p^(1/(4*sqrt(e))+epsilon).  Do it with logs */
    double dmaxa = exp( (1.0L/6.5948850828L) * mpz_logn(n) );
    if (dmaxa >= (double)ULONG_MAX)
      croak("is_miller_prime: n is too large for unconditional DMR");
    maxa = ceil(dmaxa);
  }
  if (mpz_cmp_ui(n, maxa) <= 0)
    maxa = mpz_get_ui(n) - 1;
  if (get_verbose_level() > 1)
    printf("Deterministic Miller-Rabin testing bases from 2 to %"UVuf"\n", maxa);

  mpz_init_set(nminus1, n);
  mpz_sub_ui(nminus1, nminus1, 1);
  mpz_init_set(d, nminus1);
  s = mpz_scan1(d, 0);
  mpz_tdiv_q_2exp(d, d, s);
  mpz_init(x);

  rval = 1;
  for (a = 2; rval && a <= maxa; a++) {
    rval = 0;
    mpz_set_ui(x, a);
    mpz_powm(x, x, d, n);
    if (!mpz_cmp_ui(x, 1) || !mpz_cmp(x, nminus1)) {
      rval = 1;
    } else {
      for (r = 1; r < s; r++) {
        mpz_powm_ui(x, x, 2, n);
        if (!mpz_cmp_ui(x, 1)) {
          break;
        }
        if (!mpz_cmp(x, nminus1)) {
          rval = 1;
          break;
        }
      }
    }
  }
  mpz_clear(x); mpz_clear(nminus1); mpz_clear(d);
  return rval;
}

/* Returns Lucas sequence  U_k mod n and V_k mod n  defined by P,Q */
void lucas_seq(mpz_t U, mpz_t V, mpz_t n, IV P, IV Q, mpz_t k,
               mpz_t Qk, mpz_t t)
{
  UV b = mpz_sizeinbase(k, 2);
  IV D = P*P - 4*Q;

  if (mpz_cmp_ui(n, 2) < 0) croak("Lucas sequence modulus n must be > 1");
  MPUassert( mpz_cmp_ui(k, 0) >= 0, "lucas_seq: k is negative" );
  MPUassert( mpz_cmp_si(n,(P>=0) ? P : -P) > 0, "lucas_seq: P is out of range");
  MPUassert( mpz_cmp_si(n,(Q>=0) ? Q : -Q) > 0, "lucas_seq: Q is out of range");
  MPUassert( D != 0, "lucas_seq: D is zero" );

  if (mpz_cmp_ui(k, 0) <= 0) {
    mpz_set_ui(U, 0);
    mpz_set_ui(V, 2);
    return;
  }
  if (mpz_even_p(n)) {
    /* If n is even, then we can't divide by 2.  Do it differently. */
    alt_lucas_seq(U, V, n, P, Q, k, Qk, t);
    return;
  }

  mpz_set_ui(U, 1);
  mpz_set_si(V, P);
  mpz_set_si(Qk, Q);

  if (Q == 1) {
    /* Use the fast V method if possible.  Much faster with small n. */
    mpz_set_si(t, P*P-4);
    if (P > 2 && mpz_invert(t, t, n)) {
      /* Compute V_k and V_{k+1}, then computer U_k from them. */
      mpz_set_si(V, P);
      mpz_set_si(U, P*P-2);
      while (b > 1) {
        b--;
        if (mpz_tstbit(k, b-1)) {
          mpz_mul(V, V, U);  mpz_sub_ui(V, V, P);  mpz_mod(V, V, n);
          mpz_mul(U, U, U);  mpz_sub_ui(U, U, 2);  mpz_mod(U, U, n);
        } else {
          mpz_mul(U, V, U);  mpz_sub_ui(U, U, P);  mpz_mod(U, U, n);
          mpz_mul(V, V, V);  mpz_sub_ui(V, V, 2);  mpz_mod(V, V, n);
        }
      }
      mpz_mul_ui(U, U, 2);
      mpz_submul_ui(U, V, P);
      mpz_mul(U, U, t);
    } else {
      /* Fast computation of U_k and V_k, specific to Q = 1 */
      while (b > 1) {
        mpz_mulmod(U, U, V, n, t);     /* U2k = Uk * Vk */
        mpz_mul(V, V, V);
        mpz_sub_ui(V, V, 2);
        mpz_mod(V, V, n);               /* V2k = Vk^2 - 2 Q^k */
        b--;
        if (mpz_tstbit(k, b-1)) {
          mpz_mul_si(t, U, D);
                                      /* U:  U2k+1 = (P*U2k + V2k)/2 */
          mpz_mul_si(U, U, P);
          mpz_add(U, U, V);
          if (mpz_odd_p(U)) mpz_add(U, U, n);
          mpz_fdiv_q_2exp(U, U, 1);
                                      /* V:  V2k+1 = (D*U2k + P*V2k)/2 */
          mpz_mul_si(V, V, P);
          mpz_add(V, V, t);
          if (mpz_odd_p(V)) mpz_add(V, V, n);
          mpz_fdiv_q_2exp(V, V, 1);
        }
      }
    }
  } else {
    while (b > 1) {
      mpz_mulmod(U, U, V, n, t);     /* U2k = Uk * Vk */
      mpz_mul(V, V, V);
      mpz_submul_ui(V, Qk, 2);
      mpz_mod(V, V, n);               /* V2k = Vk^2 - 2 Q^k */
      mpz_mul(Qk, Qk, Qk);            /* Q2k = Qk^2 */
      b--;
      if (mpz_tstbit(k, b-1)) {
        mpz_mul_si(t, U, D);
                                    /* U:  U2k+1 = (P*U2k + V2k)/2 */
        mpz_mul_si(U, U, P);
        mpz_add(U, U, V);
        if (mpz_odd_p(U)) mpz_add(U, U, n);
        mpz_fdiv_q_2exp(U, U, 1);
                                    /* V:  V2k+1 = (D*U2k + P*V2k)/2 */
        mpz_mul_si(V, V, P);
        mpz_add(V, V, t);
        if (mpz_odd_p(V)) mpz_add(V, V, n);
        mpz_fdiv_q_2exp(V, V, 1);

        mpz_mul_si(Qk, Qk, Q);
      }
      mpz_mod(Qk, Qk, n);
    }
  }
  mpz_mod(U, U, n);
  mpz_mod(V, V, n);
}

void alt_lucas_seq(mpz_t Uh, mpz_t Vl, mpz_t n, IV P, IV Q, mpz_t k,
                   mpz_t Ql, mpz_t t)
{
  mpz_t Vh, Qh;
  int j, s = mpz_scan1(k,0), b = mpz_sizeinbase(k,2);

  if (mpz_sgn(k) <= 0) {
    mpz_set_ui(Uh, 0);
    mpz_set_ui(Vl, 2);
    return;
  }

  mpz_set_ui(Uh, 1);
  mpz_set_ui(Vl, 2);
  mpz_set_ui(Ql, 1);
  mpz_init_set_si(Vh,P);
  mpz_init_set_ui(Qh,1);

  for (j = b; j > s; j--) {
    mpz_mul(Ql, Ql, Qh);
    if (mpz_tstbit(k, j)) {
      mpz_mul_si(Qh, Ql, Q);
      mpz_mul(Uh, Uh, Vh);
      mpz_mul_si(t, Ql, P);  mpz_mul(Vl, Vl, Vh); mpz_sub(Vl, Vl, t);
      mpz_mul(Vh, Vh, Vh); mpz_sub(Vh, Vh, Qh); mpz_sub(Vh, Vh, Qh);
    } else {
      mpz_set(Qh, Ql);
      mpz_mul(Uh, Uh, Vl);  mpz_sub(Uh, Uh, Ql);
      mpz_mul_si(t, Ql, P);  mpz_mul(Vh, Vh, Vl); mpz_sub(Vh, Vh, t);
      mpz_mul(Vl, Vl, Vl);  mpz_sub(Vl, Vl, Ql);  mpz_sub(Vl, Vl, Ql);
    }
    mpz_mod(Qh, Qh, n);
    mpz_mod(Uh, Uh, n);
    mpz_mod(Vh, Vh, n);
    mpz_mod(Vl, Vl, n);
  }
  mpz_mul(Ql, Ql, Qh);
  mpz_mul_si(Qh, Ql, Q);
  mpz_mul(Uh, Uh, Vl);  mpz_sub(Uh, Uh, Ql);
  mpz_mul_si(t, Ql, P);  mpz_mul(Vl, Vl, Vh);  mpz_sub(Vl, Vl, t);
  mpz_mul(Ql, Ql, Qh);
  mpz_clear(Qh);  mpz_clear(Vh);
  mpz_mod(Ql, Ql, n);
  mpz_mod(Uh, Uh, n);
  mpz_mod(Vl, Vl, n);
  for (j = 0; j < s; j++) {
    mpz_mul(Uh, Uh, Vl);
    mpz_mul(Vl, Vl, Vl);  mpz_sub(Vl, Vl, Ql);  mpz_sub(Vl, Vl, Ql);
    mpz_mul(Ql, Ql, Ql);
    mpz_mod(Ql, Ql, n);
    mpz_mod(Uh, Uh, n);
    mpz_mod(Vl, Vl, n);
  }
}

void lucasuv(mpz_t Uh, mpz_t Vl, IV P, IV Q, mpz_t k)
{
  mpz_t Vh, Ql, Qh, t;
  int j, s, n;

  if (mpz_sgn(k) <= 0) {
    mpz_set_ui(Uh, 0);
    mpz_set_ui(Vl, 2);
    return;
  }

  mpz_set_ui(Uh, 1);
  mpz_set_ui(Vl, 2);
  mpz_init_set_si(Vh,P);
  mpz_init(t);

  s = mpz_scan1(k, 0);     /* number of zero bits at the end */
  n = mpz_sizeinbase(k,2);

  /* It is tempting to try to pull out the various Q operations when Q=1 or
   * Q=-1.  This doesn't lead to any immediate savings.  Don't bother unless
   * there is a way to reduce the actual operations involving U and V. */
  mpz_init_set_ui(Ql,1);
  mpz_init_set_ui(Qh,1);

  for (j = n; j > s; j--) {
    mpz_mul(Ql, Ql, Qh);
    if (mpz_tstbit(k, j)) {
      mpz_mul_si(Qh, Ql, Q);
      mpz_mul(Uh, Uh, Vh);
      mpz_mul_si(t, Ql, P);  mpz_mul(Vl, Vl, Vh); mpz_sub(Vl, Vl, t);
      mpz_mul(Vh, Vh, Vh); mpz_sub(Vh, Vh, Qh); mpz_sub(Vh, Vh, Qh);
    } else {
      mpz_set(Qh, Ql);
      mpz_mul(Uh, Uh, Vl);  mpz_sub(Uh, Uh, Ql);
      mpz_mul_si(t, Ql, P);  mpz_mul(Vh, Vh, Vl); mpz_sub(Vh, Vh, t);
      mpz_mul(Vl, Vl, Vl);  mpz_sub(Vl, Vl, Ql);  mpz_sub(Vl, Vl, Ql);
    }
  }
  mpz_mul(Ql, Ql, Qh);
  mpz_mul_si(Qh, Ql, Q);
  mpz_mul(Uh, Uh, Vl);  mpz_sub(Uh, Uh, Ql);
  mpz_mul_si(t, Ql, P);  mpz_mul(Vl, Vl, Vh);  mpz_sub(Vl, Vl, t);
  mpz_mul(Ql, Ql, Qh);
  mpz_clear(Qh);  mpz_clear(t);  mpz_clear(Vh);
  for (j = 0; j < s; j++) {
    mpz_mul(Uh, Uh, Vl);
    mpz_mul(Vl, Vl, Vl);  mpz_sub(Vl, Vl, Ql);  mpz_sub(Vl, Vl, Ql);
    mpz_mul(Ql, Ql, Ql);
  }
  mpz_clear(Ql);
}

int lucas_lehmer(UV p)
{
  UV k, tlim;
  int res, pbits;
  mpz_t V, mp, t;

  if (p == 2) return 1;
  if (!(p&1)) return 0;

  mpz_init_set_ui(t, p);
  if (!_GMP_is_prob_prime(t))    /* p must be prime */
    { mpz_clear(t); return 0; }
  if (p < 23)
    { mpz_clear(t); return (p != 11); }

  pbits = mpz_sizeinbase(t,2);
  mpz_init(mp);
  mpz_setbit(mp, p);
  mpz_sub_ui(mp, mp, 1);

  /* If p=3 mod 4 and p,2p+1 both prime, then 2p+1 | 2^p-1.  Cheap test. */
  if (p > 3 && p % 4 == 3) {
    mpz_mul_ui(t, t, 2);
    mpz_add_ui(t, t, 1);
    if (_GMP_is_prob_prime(t) && mpz_divisible_p(mp, t))
      { mpz_clear(mp); mpz_clear(t); return 0; }
  }

  /* Do a little trial division first.  Saves quite a bit of time. */
  tlim = (p < 1500) ? p/2 : (p < 5000) ? p : 2*p;
  if (tlim > UV_MAX/(2*p)) tlim = UV_MAX/(2*p);
  for (k = 1; k < tlim; k++) {
    UV q = 2*p*k+1;
    if ( (q%8==1 || q%8==7) &&                 /* factor must be 1 or 7 mod 8 */
         q % 3 && q % 5 && q % 7 && q % 11 && q % 13) {  /* and must be prime */
      if (1 && q < (UVCONST(1) << (BITS_PER_WORD/2)) ) {
        UV b = 1, j = pbits;
        while (j--) {
          b = (b*b) % q;
          if (p & (UVCONST(1) << j)) { b *= 2; if (b >= q) b -= q; }
        }
        if (b == 1)
          { mpz_clear(mp); mpz_clear(t); return 0; }
      } else {
        if( mpz_divisible_ui_p(mp, q) )
          { mpz_clear(mp); mpz_clear(t); return 0; }
      }
    }
  }
  /* We could do some specialized p+1 factoring here. */

  mpz_init_set_ui(V, 4);

  for (k = 3; k <= p; k++) {
    mpz_mul(V, V, V);
    mpz_sub_ui(V, V, 2);
    /* mpz_mod(V, V, mp) but more efficiently done given mod 2^p-1 */
    if (mpz_sgn(V) < 0) mpz_add(V, V, mp);
    /* while (n > mp) { n = (n >> p) + (n & mp) } if (n==mp) n=0 */
    /* but in this case we can have at most one loop plus a carry */
    mpz_tdiv_r_2exp(t, V, p);
    mpz_tdiv_q_2exp(V, V, p);
    mpz_add(V, V, t);
    while (mpz_cmp(V, mp) >= 0) mpz_sub(V, V, mp);
  }
  res = !mpz_sgn(V);
  mpz_clear(t); mpz_clear(mp); mpz_clear(V);
  return res;
}

/* Returns:  -1 unknown, 0 composite, 2 definitely prime */
int llr(mpz_t N)
{
  mpz_t v, k, V, U, Qk, t;
  UV i, n, P;
  int res = -1;

  if (mpz_cmp_ui(N,100) <= 0) return (_GMP_is_prob_prime(N) ? 2 : 0);
  if (mpz_even_p(N) || mpz_divisible_ui_p(N, 3)) return 0;
  mpz_init(v); mpz_init(k);
  mpz_add_ui(v, N, 1);
  n = mpz_scan1(v, 0);
  mpz_tdiv_q_2exp(k, v, n);
  /* N = k * 2^n - 1 */
  if (mpz_cmp_ui(k,1) == 0) {
    res = lucas_lehmer(n) ? 2 : 0;
    goto DONE_LLR;
  }
  if (mpz_sizeinbase(k,2) > n)
    goto DONE_LLR;

  mpz_init(V);
  mpz_init(U); mpz_init(Qk); mpz_init(t);
  if (!mpz_divisible_ui_p(k, 3)) { /* Select V for 3 not divis k */
    lucas_seq(U, V, N, 4, 1, k, Qk, t);
  } else if ((n % 4 == 0 || n % 4 == 3) && mpz_cmp_ui(k,3)==0) {
    mpz_set_ui(V, 5778);
  } else {
    /* Öystein J. Rödseth: http://www.uib.no/People/nmaoy/papers/luc.pdf */
    for (P=5; P < 1000; P++) {
      mpz_set_ui(t, P-2);
      if (mpz_jacobi(t, N) == 1) {
        mpz_set_ui(t, P+2);
        if (mpz_jacobi(t, N) == -1) {
          break;
        }
      }
    }
    if (P >= 1000) {
      mpz_clear(t);  mpz_clear(Qk); mpz_clear(U);
      mpz_clear(V);
      goto DONE_LLR;
    }
    lucas_seq(U, V, N, P, 1, k, Qk, t);
  }
  mpz_clear(t);  mpz_clear(Qk); mpz_clear(U);

  for (i = 3; i <= n; i++) {
    mpz_mul(V, V, V);
    mpz_sub_ui(V, V, 2);
    mpz_mod(V, V, N);
  }
  res = mpz_sgn(V) ? 0 : 2;
  mpz_clear(V);

DONE_LLR:
  if (res != -1 && get_verbose_level() > 1) printf("N shown %s with LLR\n", res ? "prime" : "composite");
  mpz_clear(k); mpz_clear(v);
  return res;
}
/* Returns:  -1 unknown, 0 composite, 2 definitely prime */
int proth(mpz_t N)
{
  mpz_t v, k, a;
  UV n;
  int i, res = -1;
  if (mpz_cmp_ui(N,100) <= 0) return (_GMP_is_prob_prime(N) ? 2 : 0);
  if (mpz_even_p(N) || mpz_divisible_ui_p(N, 3)) return 0;
  mpz_init(v); mpz_init(k);
  mpz_sub_ui(v, N, 1);
  n = mpz_scan1(v, 0);
  mpz_tdiv_q_2exp(k, v, n);
  /* N = k * 2^n + 1 */
  if (mpz_sizeinbase(k,2) <= n) {
    mpz_init(a);
    for (i = 0; i < 25 && res == -1; i++) {
      mpz_set_ui(a, sprimes[i]);
      if (mpz_jacobi(a, N) == -1)
        res = 0;
    }
    /* (a,N) = -1 if res=0.  Do Proth primality test. */
    if (res == 0) {
      mpz_tdiv_q_2exp(k, v, 1);
      mpz_powm(a, a, k, N);
      if (mpz_cmp(a, v) == 0)
        res = 2;
    }
    mpz_clear(a);
  }
  mpz_clear(k); mpz_clear(v);
  if (res != -1 && get_verbose_level() > 1) printf("N shown %s with Proth\n", res ? "prime" : "composite");
  fflush(stdout);
  return res;
}
int is_proth_form(mpz_t N)
{
  mpz_t v, k;
  UV n;
  int res = 0;
  if (mpz_cmp_ui(N,100) <= 0) return (_GMP_is_prob_prime(N) ? 2 : 0);
  if (mpz_even_p(N) || mpz_divisible_ui_p(N, 3)) return 0;
  mpz_init(v); mpz_init(k);
  mpz_sub_ui(v, N, 1);
  n = mpz_scan1(v, 0);
  mpz_tdiv_q_2exp(k, v, n);
  /* N = k * 2^n + 1 */
  if (mpz_sizeinbase(k,2) <= n)  res = 1;
  mpz_clear(k); mpz_clear(v);
  return res;
}

static int lucas_selfridge_params(IV* P, IV* Q, mpz_t n, mpz_t t)
{
  IV D = 5;
  UV Dui = (UV) D;
  while (1) {
    UV gcd = mpz_gcd_ui(NULL, n, Dui);
    if ((gcd > 1) && mpz_cmp_ui(n, gcd) != 0)
      return 0;
    mpz_set_si(t, D);
    if (mpz_jacobi(t, n) == -1)
      break;
    if (Dui == 21 && mpz_perfect_square_p(n))
      return 0;
    Dui += 2;
    D = (D > 0)  ?  -Dui  :  Dui;
    if (Dui > 1000000)
      croak("lucas_selfridge_params: D exceeded 1e6");
  }
  if (P) *P = 1;
  if (Q) *Q = (1 - D) / 4;
  return 1;
}

static int lucas_extrastrong_params(IV* P, IV* Q, mpz_t n, mpz_t t, UV inc)
{
  UV tP = 3;
  if (inc < 1 || inc > 256)
    croak("Invalid lucas parameter increment: %"UVuf"\n", inc);
  while (1) {
    UV D = tP*tP - 4;
    UV gcd = mpz_gcd_ui(NULL, n, D);
    if (gcd > 1 && mpz_cmp_ui(n, gcd) != 0)
      return 0;
    mpz_set_ui(t, D);
    if (mpz_jacobi(t, n) == -1)
      break;
    if (tP == (3+20*inc) && mpz_perfect_square_p(n))
      return 0;
    tP += inc;
    if (tP > 65535)
      croak("lucas_extrastrong_params: P exceeded 65535");
  }
  if (P) *P = (IV)tP;
  if (Q) *Q = 1;
  return 1;
}



/* This code was verified against Feitsma's psps-below-2-to-64.txt file.
 * is_strong_pseudoprime reduced it from 118,968,378 to 31,894,014.
 * all three variations of the Lucas test reduce it to 0.
 * The test suite should check that they generate the correct pseudoprimes.
 *
 * The standard and strong versions use the method A (Selfridge) parameters,
 * while the extra strong version uses Baillie's parameters from OEIS A217719.
 *
 * Using the strong version, we can implement the strong BPSW test as
 * specified by Baillie and Wagstaff, 1980, page 1401.
 *
 * Testing on my x86_64 machine, the strong Lucas code is over 35% faster than
 * T.R. Nicely's implementation, and over 40% faster than David Cleaver's.
 */
int _GMP_is_lucas_pseudoprime(mpz_t n, int strength)
{
  mpz_t d, U, V, Qk, t;
  IV P, Q;
  UV s = 0;
  int rval;
  int _verbose = get_verbose_level();

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }

  mpz_init(t);
  rval = (strength < 2) ? lucas_selfridge_params(&P, &Q, n, t)
                        : lucas_extrastrong_params(&P, &Q, n, t, 1);
  if (!rval) {
    mpz_clear(t);
    return 0;
  }
  if (_verbose>3) gmp_printf("N: %Zd  D: %"IVdf"  P: %"UVuf"  Q: %"IVdf"\n", n, P*P-4*Q, P, Q);

  mpz_init(U);  mpz_init(V);  mpz_init(Qk);
  mpz_init_set(d, n);
  mpz_add_ui(d, d, 1);

  if (strength > 0) {
    s = mpz_scan1(d, 0);
    mpz_tdiv_q_2exp(d, d, s);
  }

  lucas_seq(U, V, n, P, Q, d, Qk, t);
  mpz_clear(d);

  rval = 0;
  if (strength == 0) {
    /* Standard checks U_{n+1} = 0 mod n. */
    rval = (mpz_sgn(U) == 0);
  } else if (strength == 1) {
    if (mpz_sgn(U) == 0) {
      rval = 1;
    } else {
      while (s--) {
        if (mpz_sgn(V) == 0) {
          rval = 1;
          break;
        }
        if (s) {
          mpz_mul(V, V, V);
          mpz_submul_ui(V, Qk, 2);
          mpz_mod(V, V, n);
          mpz_mulmod(Qk, Qk, Qk, n, t);
        }
      }
    }
  } else {
    mpz_sub_ui(t, n, 2);
    if ( mpz_sgn(U) == 0 && (mpz_cmp_ui(V, 2) == 0 || mpz_cmp(V, t) == 0) ) {
      rval = 1;
    } else {
      s--;  /* The extra strong test tests r < s-1 instead of r < s */
      while (s--) {
        if (mpz_sgn(V) == 0) {
          rval = 1;
          break;
        }
        if (s) {
          mpz_mul(V, V, V);
          mpz_sub_ui(V, V, 2);
          mpz_mod(V, V, n);
        }
      }
    }
  }
  mpz_clear(Qk); mpz_clear(V); mpz_clear(U); mpz_clear(t);
  return rval;
}

/* Pari's clever method.  It's an extra-strong Lucas test, but without
 * computing U_d.  This makes it faster, but yields more pseudoprimes.
 *
 * increment:  1 for Baillie OEIS, 2 for Pari.
 *
 * With increment = 1, these results will be a subset of the extra-strong
 * Lucas pseudoprimes.  With increment = 2, we produce Pari's results (we've
 * added the necessary GCD with D so we produce somewhat fewer).
 */
int _GMP_is_almost_extra_strong_lucas_pseudoprime(mpz_t n, UV increment)
{
  mpz_t d, V, W, t;
  UV P, s;
  int rval;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }

  mpz_init(t);
  {
    IV PP;
    if (! lucas_extrastrong_params(&PP, 0, n, t, increment) ) {
      mpz_clear(t);
      return 0;
    }
    P = (UV) PP;
  }

  mpz_init(d);
  mpz_add_ui(d, n, 1);

  s = mpz_scan1(d, 0);
  mpz_tdiv_q_2exp(d, d, s);

  /* Calculate V_d */
  {
    UV b = mpz_sizeinbase(d, 2);
    mpz_init_set_ui(V, P);
    mpz_init_set_ui(W, P*P-2);   /* V = V_{k}, W = V_{k+1} */

    while (b > 1) {
      b--;
      if (mpz_tstbit(d, b-1)) {
        mpz_mul(V, V, W);
        mpz_sub_ui(V, V, P);

        mpz_mul(W, W, W);
        mpz_sub_ui(W, W, 2);
      } else {
        mpz_mul(W, V, W);
        mpz_sub_ui(W, W, P);

        mpz_mul(V, V, V);
        mpz_sub_ui(V, V, 2);
      }
      mpz_mod(V, V, n);
      mpz_mod(W, W, n);
    }
    mpz_clear(W);
  }
  mpz_clear(d);

  rval = 0;
  mpz_sub_ui(t, n, 2);
  if ( mpz_cmp_ui(V, 2) == 0 || mpz_cmp(V, t) == 0 ) {
    rval = 1;
  } else {
    s--;  /* The extra strong test tests r < s-1 instead of r < s */
    while (s--) {
      if (mpz_sgn(V) == 0) {
        rval = 1;
        break;
      }
      if (s) {
        mpz_mul(V, V, V);
        mpz_sub_ui(V, V, 2);
        mpz_mod(V, V, n);
      }
    }
  }
  mpz_clear(V); mpz_clear(t);
  return rval;
}

static void mat_mulmod_3x3(mpz_t* a, mpz_t* b, mpz_t n, mpz_t* t, mpz_t t2) {
  int i, row, col;
  for (row = 0; row < 3; row++) {
    for (col = 0; col < 3; col++) {
      mpz_mul(t[3*row+col], a[3*row+0], b[0+col]);
      mpz_mul(t2, a[3*row+1], b[3+col]);
      mpz_add(t[3*row+col], t[3*row+col], t2);
      mpz_mul(t2, a[3*row+2], b[6+col]);
      mpz_add(t[3*row+col], t[3*row+col], t2);
    }
  }
  for (i = 0; i < 9; i++) mpz_mod(a[i], t[i], n);
}
static void mat_powmod_3x3(mpz_t* m, mpz_t kin, mpz_t n) {
  mpz_t k, t2, t[9], res[9];
  int i;
  mpz_init_set(k, kin);
  mpz_init(t2);
  for (i = 0; i < 9; i++) { mpz_init(t[i]); mpz_init(res[i]); }
  mpz_set_ui(res[0],1);  mpz_set_ui(res[4],1);  mpz_set_ui(res[8],1);
  while (mpz_sgn(k)) {
    if (mpz_odd_p(k))  mat_mulmod_3x3(res, m, n, t, t2);
    mpz_fdiv_q_2exp(k, k, 1);
    if (mpz_sgn(k))    mat_mulmod_3x3(m, m, n, t, t2);
  }
  for (i = 0; i < 9; i++)
    { mpz_set(m[i],res[i]);  mpz_clear(res[i]);  mpz_clear(t[i]); }
  mpz_clear(t2);
  mpz_clear(k);
}
int is_perrin_pseudoprime(mpz_t n)
{
  int P[9] = {0,1,0, 0,0,1, 1,1,0};
  mpz_t m[9];
  int i, rval;
  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
  }
  for (i = 0; i < 9; i++) mpz_init_set_ui(m[i], P[i]);
  mat_powmod_3x3(m, n, n);
  mpz_add(m[1], m[0], m[4]);
  mpz_add(m[2], m[1], m[8]);
  mpz_mod(m[0], m[2], n);
  rval = mpz_sgn(m[0]) ? 0 : 1;
  for (i = 0; i < 9; i++) mpz_clear(m[i]);
  return rval;
}

int is_frobenius_pseudoprime(mpz_t n, IV P, IV Q)
{
  mpz_t t, Vcomp, d, U, V, Qk;
  IV D;
  int k = 0;
  int rval;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }
  mpz_init(t);
  if (P == 0 && Q == 0) {
    P = 1;  Q = 2;
    do {
      P += 2;
      if (P == 3) P = 5;  /* P=3,Q=2 -> D=9-8=1 => k=1, so skip */
      if (P == 21 && mpz_perfect_square_p(n))
        { mpz_clear(t); return 0; }
      D = P*P-4*Q;
      if (mpz_cmp_ui(n, P >= 0 ? P : -P) <= 0) break;
      if (mpz_cmp_ui(n, D >= 0 ? D : -D) <= 0) break;
      mpz_set_si(t, D);
      k = mpz_jacobi(t, n);
    } while (k == 1);
  } else {
    D = P*P-4*Q;
    if (is_perfect_square( D >= 0 ? D : -D, 0 ))
      croak("Frobenius invalid P,Q: (%"IVdf",%"IVdf")", P, Q);
    mpz_set_si(t, D);
    k = mpz_jacobi(t, n);
  }

  /* Check initial conditions */
  {
    UV Pu = P >= 0 ? P : -P;
    UV Qu = Q >= 0 ? Q : -Q;
    UV Du = D >= 0 ? D : -D;

    /* If abs(P) or abs(Q) or abs(D) >= n, exit early. */
    if (mpz_cmp_ui(n, Pu) <= 0 || mpz_cmp_ui(n, Qu) <= 0 || mpz_cmp_ui(n, Du) <= 0) {
      mpz_clear(t);
      return _GMP_trial_factor(n, 2, Du+Pu+Qu) ? 0 : 1;
    }
    /* If k = 0, then D divides n */
    if (k == 0) {
      mpz_clear(t);
      return 0;
    }
    /* If n is not coprime to P*Q*D then we found a factor */
    if (mpz_gcd_ui(NULL, n, Du*Pu*Qu) > 1) {
      mpz_clear(t);
      return 0;
    }
  }

  mpz_init(Vcomp);
  if (k == 1) {
    mpz_set_si(Vcomp, 2);
  } else {
    mpz_set_si(Vcomp, Q);
    mpz_mul_ui(Vcomp, Vcomp, 2);
    mpz_mod(Vcomp, Vcomp, n);
  }

  mpz_init(U);  mpz_init(V);  mpz_init(Qk);  mpz_init(d);
  if (k == 1) mpz_sub_ui(d, n, 1);
  else        mpz_add_ui(d, n, 1);

  lucas_seq(U, V, n, P, Q, d, Qk, t);
  rval = ( mpz_sgn(U) == 0 && mpz_cmp(V, Vcomp) == 0 );

  mpz_clear(d); mpz_clear(Qk); mpz_clear(V); mpz_clear(U);
  mpz_clear(Vcomp); mpz_clear(t);

  return rval;
}

/* Use Crandall/Pomerance, steps from Loebenberger 2008 */
int is_frobenius_cp_pseudoprime(mpz_t n, UV ntests)
{
  gmp_randstate_t* p_randstate = get_randstate();
  mpz_t t, a, b, d, w1, wm, wm1, m;
  UV tn;
  int j;
  int result = 1;

  if (mpz_cmp_ui(n, 100) < 0)     /* tiny n */
    return (_GMP_is_prob_prime(n) > 0);
  if (mpz_even_p(n))
    return 0;

  mpz_init(t);  mpz_init(a);  mpz_init(b);  mpz_init(d);
  mpz_init(w1);  mpz_init(wm);  mpz_init(wm1);  mpz_init(m);
  for (tn = 0; tn < ntests; tn++) {
    /* Step 1: choose a and b in 1..n-1 and d=a^2-4b not square and coprime */
    do {
      mpz_sub_ui(t, n, 1);
      mpz_urandomm(a, *p_randstate, t);
      mpz_add_ui(a, a, 1);
      mpz_urandomm(b, *p_randstate, t);
      mpz_add_ui(b, b, 1);
      /* Check d and gcd */
      mpz_mul(d, a, a);
      mpz_mul_ui(t, b, 4);
      mpz_sub(d, d, t);
    } while (mpz_perfect_square_p(d));
    mpz_mul(t, a, b);
    mpz_mul(t, t, d);
    mpz_gcd(t, t, n);
    if (mpz_cmp_ui(t, 1) != 0 && mpz_cmp(t, n) != 0)
      { result = 0; break; }
    /* Step 2: W1 = a^2b^-1 - 2 mod n */
    if (!mpz_invert(t, b, n))
      { result = 0; break; }
    mpz_mul(w1, a, a);
    mpz_mul(w1, w1, t);
    mpz_sub_ui(w1, w1, 2);
    mpz_mod(w1, w1, n);
    /* Step 3: m = (n-(d|n))/2 */
    j = mpz_jacobi(d, n);
    if (j == -1)     mpz_add_ui(m, n, 1);
    else if (j == 0) mpz_set(m, n);
    else if (j == 1) mpz_sub_ui(m, n, 1);
    mpz_tdiv_q_2exp(m, m, 1);
    /* Step 8 here:  B = b^(n-1)/2 mod n  (stored in d) */
    mpz_sub_ui(t, n, 1);
    mpz_tdiv_q_2exp(t, t, 1);
    mpz_powm(d, b, t, n);
    /* Quick Euler test */
    mpz_sub_ui(t, n, 1);
    if (mpz_cmp_ui(d, 1) != 0 && mpz_cmp(d, t) != 0)
      { result = 0; break; }
    /* Step 4: calculate Wm,Wm+1 */
    mpz_set_ui(wm, 2);
    mpz_set(wm1, w1);
    {
      UV bit = mpz_sizeinbase(m, 2);
      while (bit-- > 0) {
        if (mpz_tstbit(m, bit)) {
          mpz_mul(t, wm, wm1);
          mpz_sub(wm, t, w1);
          mpz_mul(t, wm1, wm1);
          mpz_sub_ui(wm1, t, 2);
        } else {
          mpz_mul(t, wm, wm1);
          mpz_sub(wm1, t, w1);
          mpz_mul(t, wm, wm);
          mpz_sub_ui(wm, t, 2);
        }
        mpz_mod(wm, wm, n);
        mpz_mod(wm1, wm1, n);
      }
    }
    /* Step 5-7: compare w1/wm */
    mpz_mul(t, w1, wm);
    mpz_mod(t, t, n);
    mpz_mul_ui(wm1, wm1, 2);
    mpz_mod(wm1, wm1, n);
    if (mpz_cmp(t, wm1) != 0)
      { result = 0; break; }
    /* Step 8 was earlier */
    /* Step 9: See if Bwm = 2 mod n */
    mpz_mul(wm, wm, d);
    mpz_mod(wm, wm, n);
    if (mpz_cmp_ui(wm, 2) != 0)
      { result = 0; break; }
  }
  mpz_clear(w1);  mpz_clear(wm);  mpz_clear(wm1);  mpz_clear(m);
  mpz_clear(t);  mpz_clear(a);  mpz_clear(b);  mpz_clear(d);
  return result;
}

/* New code based on draft paper */
int _GMP_is_frobenius_underwood_pseudoprime(mpz_t n)
{
  mpz_t temp1, temp2, n_plus_1, s, t;
  unsigned long a, ap2, len;
  int bit, j, rval = 0;
  int _verbose = get_verbose_level();

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }

  mpz_init(temp1);

  for (a = 0; a < 1000000; a++) {
    if (a==2 || a==4 || a==7 || a==8 || a==10 || a==14 || a==16 || a==18)
      continue;
    mpz_set_si(temp1, (long)(a*a) - 4);
    j = mpz_jacobi(temp1, n);
    if (j == -1) break;
    if (j == 0 || (a == 20 && mpz_perfect_square_p(n)))
      { mpz_clear(temp1); return 0; }
  }
  if (a >= 1000000)
    { mpz_clear(temp1); croak("FU test failure, unable to find suitable a"); }
  if (mpz_gcd_ui(NULL, n, (a+4)*(2*a+5)) != 1)
    { mpz_clear(temp1); return 0; }

  mpz_init(temp2); mpz_init(n_plus_1),

  ap2 = a+2;
  mpz_add_ui(n_plus_1, n, 1);
  len = mpz_sizeinbase(n_plus_1, 2);
  mpz_init_set_ui(s, 1);
  mpz_init_set_ui(t, 2);

  for (bit = len-2; bit >= 0; bit--) {
    mpz_add(temp2, t, t);
    if (a != 0) {
      mpz_mul_ui(temp1, s, a);
      mpz_add(temp2, temp1, temp2);
    }
    mpz_mul(temp1, temp2, s);
    mpz_sub(temp2, t, s);
    mpz_add(s, s, t);
    mpz_mul(t, s, temp2);
    mpz_mod(t, t, n);
    mpz_mod(s, temp1, n);
    if ( mpz_tstbit(n_plus_1, bit) ) {
      if (a == 0)   mpz_add(temp1, s, s);
      else          mpz_mul_ui(temp1, s, ap2);
      mpz_add(temp1, temp1, t);
      mpz_add(temp2, t, t);
      mpz_sub(t, temp2, s);
      mpz_set(s, temp1);
    }
  }
  /* n+1 always has an even last bit, so s and t always modded */
  mpz_set_ui(temp1, 2*a+5);
  mpz_mod(temp1, temp1, n);
  if (mpz_cmp_ui(s, 0) == 0 && mpz_cmp(t, temp1) == 0)
    rval = 1;
  if (_verbose>1) gmp_printf("%Zd is %s with a = %"UVuf"\n", n, (rval) ? "probably prime" : "composite", a);

  mpz_clear(temp1); mpz_clear(temp2); mpz_clear(n_plus_1);
  mpz_clear(s); mpz_clear(t);
  return rval;
}

int _GMP_is_frobenius_khashin_pseudoprime(mpz_t n)
{
  mpz_t t, ta, tb, ra, rb, a, b, n_minus_1;
  unsigned long c = 1;
  int bit, len, k, rval = 0;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }
  if (mpz_perfect_square_p(n)) return 0;

  mpz_init(t);
  do {
    c += 2;
    mpz_set_ui(t, c);
    k = mpz_jacobi(t, n);
  } while (k == 1);
  if (k == 0) {
    mpz_clear(t);
    return 0;
  }

  mpz_init_set_ui(ra, 1);   mpz_init_set_ui(rb, 1);
  mpz_init_set_ui(a, 1);    mpz_init_set_ui(b, 1);
  mpz_init(ta);   mpz_init(tb);
  mpz_init(n_minus_1);
  mpz_sub_ui(n_minus_1, n, 1);

  len = mpz_sizeinbase(n_minus_1, 2);
  for (bit = 0; bit < len; bit++) {
    if ( mpz_tstbit(n_minus_1, bit) ) {
      mpz_mul(ta, ra, a);
      mpz_mul(tb, rb, b);
      mpz_add(t, ra, rb);
      mpz_add(rb, a, b);
      mpz_mul(rb, rb, t);
      mpz_sub(rb, rb, ta);
      mpz_sub(rb, rb, tb);
      mpz_mod(rb, rb, n);
      mpz_mul_ui(tb, tb, c);
      mpz_add(ra, ta, tb);
      mpz_mod(ra, ra, n);
    }
    if (bit < len-1) {
      mpz_mul(t, b, b);
      mpz_mul_ui(t, t, c);
      mpz_mul(b, b, a);
      mpz_add(b, b, b);
      mpz_mod(b, b, n);
      mpz_mul(a, a, a);
      mpz_add(a, a, t);
      mpz_mod(a, a, n);
    }
  }
  if ( (mpz_cmp_ui(ra,1) == 0) && (mpz_cmp(rb, n_minus_1) == 0) )
    rval = 1;

  mpz_clear(n_minus_1);
  mpz_clear(ta); mpz_clear(tb);
  mpz_clear(a);  mpz_clear(b);
  mpz_clear(ra); mpz_clear(rb);
  mpz_clear(t);
  return rval;
}




int _GMP_BPSW(mpz_t n)
{
  if (mpz_cmp_ui(n, 4) < 0)
    return (mpz_cmp_ui(n, 1) <= 0) ? 0 : 1;

  if (_GMP_miller_rabin_ui(n, 2) == 0)   /* Miller Rabin with base 2 */
    return 0;

  if (_GMP_is_lucas_pseudoprime(n, 2 /*extra strong*/) == 0)
    return 0;

  if (mpz_sizeinbase(n, 2) <= 64)        /* BPSW is deterministic below 2^64 */
    return 2;

  return 1;
}

/* Assume n is a BPSW PRP, return 1 (no result), 0 (composite), 2 (prime) */
int is_deterministic_miller_rabin_prime(mpz_t n)
{
  mpz_t t;
  int i, res = 1, maxp = 0;

  if (mpz_sizeinbase(n, 2) <= 82) {
    mpz_init(t);
    /* n < 3825123056546413051  =>  maxp=9, but BPSW should have handled */
    if      (mpz_set_str(t, "318665857834031151167461",10), mpz_cmp(n,t) < 0)
      maxp = 12;
    else if (mpz_set_str(t,"3317044064679887385961981",10), mpz_cmp(n,t) < 0)
      maxp = 13;
    if (maxp > 0) {
      for (i = 1; i < maxp && res; i++) {
        mpz_set_ui(t, sprimes[i]);
        res = _GMP_miller_rabin(n, t);
      }
      if (res == 1) res = 2;
    }
    mpz_clear(t);
  }
  return res;
}


/*
 * is_prob_prime      BPSW -- fast, no known counterexamples
 * is_prime           is_prob_prime + a little extra
 * is_provable_prime  really prove it, which could take a very long time
 *
 * They're all identical for numbers <= 2^64.
 *
 * The extra M-R tests in is_prime start actually costing something after
 * 1000 bits or so.  Primality proving will get quite a bit slower as the
 * number of bits increases.
 *
 * What are these extra M-R tests getting us?  The primary reference is
 * Damg??rd, Landrock, and Pomerance, 1993.  From Rabin-Monier, with random
 * bases we have p <= 4^-t.  So one extra test gives us p = 0.25, and four
 * tests gives us p = 0.00390625.  But for larger k (bits in n) this is very
 * conservative.  Since the value has passed BPSW, we are only interested in
 * k > 64.  See the calculate-mr-probs.pl script in the xt/ directory.
 * For a 256-bit input, we get:
 *   1 test:  p < 0.000244140625
 *   2 tests: p < 0.00000000441533
 *   3 tests: p < 0.0000000000062550875
 *   4 tests: p < 0.000000000000028421709
 *
 * It's even more extreme as k goes higher.  Also recall that this is the
 * probability once we've somehow found a BPSW pseudoprime.
 */

int _GMP_is_prob_prime(mpz_t n)
{
  /*  Step 1: Look for small divisors.  This is done purely for performance.
   *          It is *not* a requirement for the BPSW test. */
  int res = primality_pretest(n);
  if (res != 1)  return res;

  /* We'd like to do the LLR test here, but it screws with certificates. */

  /*  Step 2: The BPSW test.  spsp base 2 and slpsp. */
  return _GMP_BPSW(n);
}

int is_bpsw_dmr_prime(mpz_t n)
{
  int prob_prime = _GMP_BPSW(n);
  if (prob_prime == 1) {
    prob_prime = is_deterministic_miller_rabin_prime(n);
    if (prob_prime == 0) gmp_printf("\n\n**** BPSW counter-example found?  ****\n**** N = %Zd ****\n\n", n);
  }
  return prob_prime;
}

int _GMP_is_prime(mpz_t n)
{
  UV nbits;
  /* Similar to is_prob_prime, but put LLR before BPSW, then do more testing */

  /* First, simple pretesting */
  int prob_prime = primality_pretest(n);
  if (prob_prime != 1)  return prob_prime;

  /* If the number is of form N=k*2^n-1 and we have a fast proof, do it. */
  prob_prime = llr(n);
  if (prob_prime == 0 || prob_prime == 2) return prob_prime;

  /* If the number is of form N=k*2^n+1 and we have a fast proof, do it. */
  prob_prime = proth(n);
  if (prob_prime == 0 || prob_prime == 2) return prob_prime;

  /* Start with BPSW */
  prob_prime = _GMP_BPSW(n);
  nbits = mpz_sizeinbase(n, 2);

  /* Use Sorenson/Webster 2015 deterministic M-R if possible */
  if (prob_prime == 1) {
    prob_prime = is_deterministic_miller_rabin_prime(n);
    if (prob_prime == 0) gmp_printf("\n\n**** BPSW counter-example found?  ****\n**** N = %Zd ****\n\n", n);
  }

  /* n has passed the ES BPSW test, making it quite unlikely it is a
   * composite (and it cannot be if n < 2^64). */

  /* For small numbers, try a quick BLS75 proof. */
  if (prob_prime == 1) {
    if (is_proth_form(n))
      prob_prime = _GMP_primality_bls_nm1(n, 2 /* effort */, 0 /* cert */);
    else if (nbits <= 150)
      prob_prime = _GMP_primality_bls_nm1(n, 0 /* effort */, 0 /* cert */);
    /* We could do far better by calling bls75_hybrid, especially with a
     * larger effort.  But that takes more time.  I've decided it isn't worth
     * the extra time here.  We'll still try a very quick N-1 proof. */
  }

  /* If prob_prime is still 1, let's run some extra tests.
   * Argument against:  Nobody has yet found a BPSW counterexample, so
   *                    this is wasted time.  They can make a call to one of
   *                    the other tests themselves if they care.
   * Argument for:      is_prime() should be as correct as reasonably possible.
   *                    They can call is_prob_prime() to skip extra tests.
   *
   * Choices include:
   *
   *   - A number of fixed-base Miller-Rabin tests.
   *   - A number of random-base Miller-Rabin tests.
   *   - A Frobenius-Underwood test.
   *   - A Frobenius-Khashin test.
   *
   * The Miller-Rabin tests have better performance.
   *
   * We will use random bases to make it more difficult to get a false
   * result even if someone has a way to generate BPSW pseudoprimes.
   * We use enough bases to keep the probability below 1/595,000.
   */

  if (prob_prime == 1) {
    UV ntests;
    if      (nbits <  80) ntests = 5;  /* p < .00000168 */
    else if (nbits < 105) ntests = 4;  /* p < .00000156 */
    else if (nbits < 160) ntests = 3;  /* p < .00000164 */
    else if (nbits < 413) ntests = 2;  /* p < .00000156 */
    else                  ntests = 1;  /* p < .00000159 */
    prob_prime = _GMP_miller_rabin_random(n, ntests, 0);
    /* prob_prime = _GMP_is_frobenius_underwood_pseudoprime(n); */
    /* prob_prime = _GMP_is_frobenius_khashin_pseudoprime(n); */
  }

  /* Using Damg??rd, Landrock, and Pomerance, we get upper bounds:
   * k <=  64      p = 0
   * k <   80      p < 7.53e-08
   * k <  105      p < 3.97e-08
   * k <  160      p < 1.68e-08
   * k >= 160      p < 2.57e-09
   * This (1) pretends the SPSP-2 is included in the random tests, (2) doesn't
   * take into account the trial division, (3) ignores the observed
   * SPSP-2 / Strong Lucas anti-correlation that makes the BPSW test so
   * useful.  Hence these numbers are still extremely conservative.
   *
   * For an idea of how conservative we are being, we have now exceeded the
   * tests used in Mathematica, Maple, Pari, and SAGE.  Note: Pari pre-2.3
   * used just M-R tests.  Pari 2.3+ uses BPSW with no extra M-R checks for
   * is_pseudoprime, but isprime uses APRCL which, being a proof, could only
   * output a pseudoprime through a coding error.
   */

  return prob_prime;
}


int _GMP_is_provable_prime(mpz_t n, char** prooftext)
{
  int prob_prime = primality_pretest(n);
  if (prob_prime != 1)  return prob_prime;

  /* Try LLR and Proth if they don't need a proof certificate. */
  if (prooftext == 0) {
    prob_prime = llr(n);
    if (prob_prime == 0 || prob_prime == 2) return prob_prime;
    prob_prime = proth(n);
    if (prob_prime == 0 || prob_prime == 2) return prob_prime;
  }

  /* Start with BPSW */
  prob_prime = _GMP_BPSW(n);
  if (prob_prime != 1)  return prob_prime;

  /* Use Sorenson/Webster 2015 deterministic M-R if possible */
  if (prooftext == 0) {
    prob_prime = is_deterministic_miller_rabin_prime(n);
    if (prob_prime != 1)  return prob_prime;
  }

  /* Run one more M-R test, just in case. */
  prob_prime = _GMP_miller_rabin_random(n, 1, 0);
  if (prob_prime != 1)  return prob_prime;

  /* We can choose a primality proving algorithm:
   *   AKS    _GMP_is_aks_prime       really slow, don't bother
   *   N-1    _GMP_primality_bls_nm1  small or special numbers
   *   ECPP   _GMP_ecpp               fastest in general
   */

  /* Give n-1 a small go */
  prob_prime = _GMP_primality_bls_nm1(n, is_proth_form(n) ? 3 : 1, prooftext);
  if (prob_prime != 1)  return prob_prime;

  /* ECPP */
  prob_prime = _GMP_ecpp(n, prooftext);

  return prob_prime;
}
