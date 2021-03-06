/* 3-D complex-complex FFT interface with threaded FFTW3 */
/*
  Copyright (C) 2010 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <rsf.h>

#ifdef _OPENMP
#include <omp.h>
#endif
#ifdef SF_HAS_FFTW
#include <fftw3.h>
#endif

static int n1, n2, n3, nk;
static float wt;

static sf_complex ***cc=NULL;

#ifdef SF_HAS_FFTW
static fftwf_plan cfg=NULL, icfg=NULL;
#else
static kiss_fft_cfg cfg1, icfg1, cfg2, icfg2, cfg3, icfg3;
static kiss_fft_cpx ***tmp, *ctrace2, *ctrace3;
static sf_complex *trace2, *trace3;
#endif

int cfft3_init(int pad1           /* padding on the first axis */,
	      int nx,   int ny,   int nz   /* input data size */, 
	      int *nx2, int *ny2, int *nz2 /* padded data size */)
/*< initialize >*/
{

#ifdef SF_HAS_FFTW
#ifdef _OPENMP
    fftw_init_threads();
    sf_warning("Using threaded FFTW3! %d\n",omp_get_max_threads());
    fftw_plan_with_nthreads(omp_get_max_threads());
#endif
#else
    int i2, i3;
#endif

    /* axis 1 */

    nk = n1 = kiss_fft_next_fast_size(nx*pad1);

#ifndef SF_HAS_FFTW
    cfg1  = kiss_fft_alloc(n1,0,NULL,NULL);
    icfg1 = kiss_fft_alloc(n1,1,NULL,NULL);
#endif

    /* axis 2 */

    n2 = kiss_fft_next_fast_size(ny);

#ifndef SF_HAS_FFTW
    cfg2  = kiss_fft_alloc(n2,0,NULL,NULL);
    icfg2 = kiss_fft_alloc(n2,1,NULL,NULL);

    trace2 = sf_complexalloc(n2);
    ctrace2 = (kiss_fft_cpx *) trace2;
#endif

    /* axis 3 */

    n3 = kiss_fft_next_fast_size(nz);

#ifndef SF_HAS_FFTW
    cfg3  = kiss_fft_alloc(n3,0,NULL,NULL);
    icfg3 = kiss_fft_alloc(n3,1,NULL,NULL);

    trace3 = sf_complexalloc(n3);
    ctrace3 = (kiss_fft_cpx *) trace3;

    /* --- */

    tmp = (kiss_fft_cpx***) sf_alloc (n3,sizeof(kiss_fft_cpx**));
    tmp[0] = (kiss_fft_cpx**) sf_alloc (n2*n3,sizeof(kiss_fft_cpx*));
    tmp[0][0] = (kiss_fft_cpx*) sf_alloc (nk*n2*n3,sizeof(kiss_fft_cpx));

    for (i2=1; i2 < n2*n3; i2++) {
	tmp[0][i2] = tmp[0][0]+i2*nk;
    }

    for (i3=1; i3 < n3; i3++) {
	tmp[i3] = tmp[0]+i3*n2;
    }
#endif

    cc = sf_complexalloc3(n1,n2,n3);

    *nx2 = n1;
    *ny2 = n2;
    *nz2 = n3;

    wt =  1.0/(n3*n2*n1);

    return (nk*n2*n3);
}

void cfft3(sf_complex *inp /* [n1*n2*n3] */,
	   sf_complex *out /* [nk*n2*n3] */)
/*< 3-D FFT >*/
{
    int i1, i2, i3;
    sf_complex f;

#ifdef SF_HAS_FFTW
    if (NULL==cfg) {
        cfg = fftwf_plan_dft_3d(n3,n2,n1,
			  (fftwf_complex *) cc[0][0], 
			  (fftwf_complex *) out,
			  FFTW_FORWARD, FFTW_MEASURE);
	if (NULL == cfg) sf_error("FFTW failure.");
    }
#endif  
    
    /* FFT centering */    
    for (i3=0; i3<n3; i3++) {
	for (i2=0; i2<n2; i2++) {
	    for (i1=0; i1<n1; i1++) {
		f = inp[(i3*n2+i2)*n1+i1];
#ifdef SF_HAS_COMPLEX_H
		cc[i3][i2][i1] = (((i3%2==0)==(i2%2==0))==(i1%2==0))? f:-f;
#else
		cc[i3][i2][i1] = (((i3%2==0)==(i2%2==0))==(i1%2==0))? f:sf_cneg(f);
#endif
	    }
	}
    }

#ifdef SF_HAS_FFTW
    fftwf_execute(cfg);
#else

    /* FFT over first axis */
    for (i3=0; i3 < n3; i3++) {
	for (i2=0; i2 < n2; i2++) {
	    kiss_fft_stride(cfg1,(kiss_fft_cpx *) cc[i3][i2],tmp[i3][i2],1);
	}
    }

    /* FFT over second axis */
    for (i3=0; i3 < n3; i3++) {
	for (i1=0; i1 < nk; i1++) {
	    kiss_fft_stride(cfg2,tmp[i3][0]+i1,ctrace2,nk);
	    for (i2=0; i2 < n2; i2++) {
		tmp[i3][i2][i1]=ctrace2[i2];
	    }
	}
    }

    /* FFT over third axis */
    for (i2=0; i2 < n2; i2++) {
	for (i1=0; i1 < nk; i1++) {
	    kiss_fft_stride(cfg3,tmp[0][0]+i2*nk+i1,ctrace3,nk*n2);
	    for (i3=0; i3<n3; i3++) {
		out[(i3*n2+i2)*nk+i1] = trace3[i3];
	    }
	}
    } 
   
#endif

}

void icfft3_allocate(sf_complex *inp /* [nk*n2*n3] */)
/*< allocate inverse transform >*/
{
#ifdef SF_HAS_FFTW
    icfg = fftwf_plan_dft_3d(n3,n2,n1,
		     (fftwf_complex *) inp, 
		     (fftwf_complex *) cc[0][0],
		     FFTW_BACKWARD, FFTW_MEASURE);
    if (NULL == icfg) sf_error("FFTW failure.");
#endif
}

void icfft3(sf_complex *out /* [n1*n2*n3] */, 
	    sf_complex *inp /* [nk*n2*n3] */)
/*< 3-D inverse FFT >*/
{
    int i1, i2, i3;

#ifdef SF_HAS_FFTW
    fftwf_execute(icfg);
#else

    /* IFFT over third axis */
    for (i2=0; i2 < n2; i2++) {
	for (i1=0; i1 < nk; i1++) {
	    kiss_fft_stride(icfg3,(kiss_fft_cpx *) (inp+i2*nk+i1),ctrace3,nk*n2);
	    for (i3=0; i3<n3; i3++) {
		tmp[i3][i2][i1] = ctrace3[i3];
	    }
	}
    }
    
    /* IFFT over second axis */
    for (i3=0; i3 < n3; i3++) {
	for (i1=0; i1 < nk; i1++) {
	    kiss_fft_stride(icfg2,tmp[i3][0]+i1,ctrace2,nk);		
	    for (i2=0; i2<n2; i2++) {
		tmp[i3][i2][i1] = ctrace2[i2];
	    }
	}
    }

    /* IFFT over first axis */
    for (i3=0; i3 < n3; i3++) {
	for (i2=0; i2 < n2; i2++) {
	    kiss_fft_stride(icfg1,tmp[i3][i2],(kiss_fft_cpx *) cc[i3][i2],1);
	}
    }

#endif

    /* FFT centering and normalization */
    for (i3=0; i3<n3; i3++) {
	for (i2=0; i2<n2; i2++) {
	    for (i1=0; i1<n1; i1++) {
#ifdef SF_HAS_COMPLEX_H
		out[(i3*n2+i2)*n1+i1] = ((((i3%2==0)==(i2%2==0))==(i1%2==0))? wt:-wt)*cc[i3][i2][i1];
#else
		out[(i3*n2+i2)*n1+i1] = sf_crmul(cc[i3][i2][i1],((((i3%2==0)==(i2%2==0))==(i1%2==0))? wt:-wt));
#endif
	    }
	}
    }
}

void cfft3_finalize()
/*< clean up fftw >*/
{
#ifdef SF_HAS_FFTW
#ifdef _OPENMP
    fftw_cleanup_threads();
#endif
    fftwf_destroy_plan(cfg);
    fftwf_destroy_plan(icfg);
    fftwf_cleanup();
    cfg=NULL;
    icfg=NULL;
#else
    free(cfg1); cfg1=NULL;
    free(icfg1); icfg1=NULL;
    free(cfg2); cfg2=NULL;
    free(icfg2); icfg2=NULL;
    free(cfg3); cfg3=NULL;
    free(icfg3); icfg3=NULL;
#endif

    free(**cc);
    free(*cc);
    free(cc);
}
