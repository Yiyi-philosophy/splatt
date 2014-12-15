

/******************************************************************************
 * INCLUDES
 *****************************************************************************/
#include "base.h"
#include "sort.h"
#include "io.h"
#include "matrix.h"
#include "ftensor.h"


/******************************************************************************
 * STATIC FUNCTIONS
 *****************************************************************************/
static void __create_fptr(
  ftensor_t * const ft,
  sptensor_t const * const tt,
  idx_t const mode)
{
  if(tt->type == SPLATT_NMODE) {
    fprintf(stderr, "SPLATT: ftensor for n-mode tensors is unfinished.\n");
    exit(1);
  }

  idx_t const nnz = tt->nnz;
  idx_t const nmodes = tt->nmodes;
  idx_t const fmode = ft->dim_perms[mode][nmodes - 1];

  /* permuted tt->ind */
  idx_t * ttinds[MAX_NMODES];
  for(idx_t m=0; m < nmodes; ++m) {
    ttinds[m] = tt->ind[ft->dim_perms[mode][m]];
  }
  /* this avoids some maybe-uninitialized warnings */
  for(idx_t m=nmodes; m < MAX_NMODES; ++m) {
    ttinds[m] = NULL;
  }

  idx_t nfibs = 1;
  ft->inds[mode][0] = ttinds[nmodes-1][0];
  ft->vals[mode][0] = tt->vals[0];

  /* count fibers in tt */
  for(idx_t n=1; n < nnz; ++n) {
    for(idx_t m=0; m < nmodes-1; ++m) {
      /* check for new fiber */
      if(ttinds[m][n] != ttinds[m][n-1]) {
        ++nfibs;
        break;
      }
    }
    ft->inds[mode][n] = ttinds[nmodes-1][n];
    ft->vals[mode][n] = tt->vals[n];
  }

  /* allocate fiber structure */
  ft->sptr[mode] = (idx_t *) malloc((ft->dims[mode]+1) * sizeof(idx_t));
  ft->nfibs[mode] = nfibs;
  ft->fptr[mode] = (idx_t *) malloc((nfibs+1) * sizeof(idx_t));
  ft->fids[mode] = (idx_t *) malloc(nfibs * sizeof(idx_t));

  /* now fill structure */
  idx_t slice = 0;
  ft->sptr[mode][slice] = 0;
  ft->sptr[mode][ft->dims[mode]] = nfibs;
  while(slice != ttinds[0][0]+1) {
    ft->sptr[mode][slice++] = 0;
  }

  ft->fptr[mode][0] = 0;
  ft->fptr[mode][nfibs] = nnz;
  ft->fids[mode][0] = ttinds[1][0];
  idx_t fib = 1;
  for(idx_t n=1; n < nnz; ++n) {
    int newfib = 0;
    /* check for new fiber */
    for(idx_t m=0; m < nmodes-1; ++m) {
      if(ttinds[m][n] != ttinds[m][n-1]) {
        newfib = 1;
        break;
      }
    }
    if(newfib) {
      /* increment slice if necessary and account for empty slices */
      while(slice != ttinds[0][n]+1) {
        ft->sptr[mode][slice++] = fib;
      }
      ft->fptr[mode][fib] = n;
      ft->fids[mode][fib] = ttinds[1][n];
      ++fib;
    }
  }
  /* account for any empty slices at end */
  for(idx_t s=slice; s < ft->dims[mode]; ++s) {
    ft->sptr[mode][s] = ft->nfibs[mode];
  }
}


static void __order_modes(
  ftensor_t * const ft,
  sptensor_t const * const tt)
{
  for(idx_t m=0; m < tt->nmodes; ++m) {
    ft->dim_perms[m][0] = m;
    /* find largest mode */
    idx_t maxm = (m+1) % tt->nmodes;
    for(idx_t mo=1; mo < tt->nmodes; ++mo) {
      if(ft->dims[(m+mo) % tt->nmodes] > ft->dims[maxm]) {
        maxm = (m+mo) % tt->nmodes;
      }
    }

    /* fill in mode permutation */
    ft->dim_perms[m][tt->nmodes-1] = maxm;
    idx_t mark = 1;
    for(idx_t mo=1; mo < tt->nmodes; ++mo) {
      idx_t mround = (m + mo) % tt->nmodes;
      if(mround != maxm) {
        ft->dim_perms[m][mark++] = mround;
      }
    }
  }
}


/******************************************************************************
 * PUBLIC FUNCTIONS
 *****************************************************************************/
ftensor_t * ften_alloc(
  sptensor_t * const tt)
{
  ftensor_t * ft = (ftensor_t *) malloc(sizeof(ftensor_t));
  ft->nnz = tt->nnz;
  ft->nmodes = tt->nmodes;

  for(idx_t m=0; m < tt->nmodes; ++m) {
    ft->dims[m] = tt->dims[m];
  }

  /* compute permutation of modes */
  __order_modes(ft, tt);

  /* allocate modal data */
  for(idx_t m=0; m < tt->nmodes; ++m) {
    ft->inds[m] = (idx_t *) malloc(ft->nnz * sizeof(idx_t));
    ft->vals[m] = (val_t *) malloc(ft->nnz * sizeof(val_t));

    tt_sort(tt, m, ft->dim_perms[m]);
    __create_fptr(ft, tt, m);
  }

  return ft;
}


spmatrix_t * ften_spmat(
  ftensor_t * ft,
  idx_t const mode)
{
  idx_t const nrows = ft->nfibs[mode];
  idx_t const ncols = ft->dims[ft->dim_perms[mode][2]];
  spmatrix_t * mat = spmat_alloc(nrows, ncols, ft->nnz);

  memcpy(mat->rowptr, ft->fptr[mode], (nrows+1) * sizeof(idx_t));
  memcpy(mat->colind, ft->inds[mode], ft->nnz * sizeof(idx_t));
  memcpy(mat->vals,   ft->vals[mode], ft->nnz * sizeof(val_t));

  return mat;
}


void ften_free(
  ftensor_t * ft)
{
  for(idx_t m=0; m < ft->nmodes; ++m) {
    free(ft->sptr[m]);
    free(ft->fptr[m]);
    free(ft->fids[m]);
    free(ft->inds[m]);
    free(ft->vals[m]);
  }
  free(ft);
}


