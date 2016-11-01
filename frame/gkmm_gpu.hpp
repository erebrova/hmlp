/**
 *  -- GKMX (version 1.1.1) --
 *
 *  NVIDIA Corp, Santa Clara
 *
 *  @date June 2016
 *  @author Chenhan D. Yu
 *
 *  Modification
 *
 *
 *
 */

#ifndef GKMX_CUH
#define GKMX_CUH

#include <hmlp.h>


namespace hmlp
{
namespace gkmm
{

#define version(s,v) s ## _V_ ## v

// GKMM macros (see gkmx_template_kernel_batched.hxx for the definition.)
#define gkmm(ta,tb,s,v) gkmm_template_batched_internal \
  < ta, tb, s ## _V_ ## v, TA, TB, TC, TV, SQ2NRM, OPKERNEL, OP1, OP2> \
  ( \
  stream, \
  m, n, k, \
  Aarray, lda, \
  Barray, ldb, \
  Carray, ldc, \
  batchSize, \
  opkernel, op1, op2, initV ) 

#define gkmm_strided(ta,tb,s,v) gkmm_template_batched_strided_internal \
  < ta, tb, s ## _V_ ## v, TA, TB, TC, TV, SQ2NRM, OPKERNEL, OP1, OP2> \
  ( \
  stream, \
  m, n, k, \
  Aarray, lda, loa, \
  Barray, ldb, lob, \
  Carray, ldc, loc, \
  batchSize, \
  opkernel, op1, op2, initV ) 




template<
bool TRANSA, bool TRANSB,
const int DIM_X, const int DIM_Y,
const int BLK_M, const int BLK_N, const int BLK_K, 
const int DIM_XA, const int DIM_YA, const int DIM_XB, const int DIM_YB, 
const int THR_M, const int THR_N, 
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV>
static __device__ void gkmm_device
(
  int M, int N, int K,
  const TA* __restrict__ A, int LDA,
  const TB* __restrict__ B, int LDB,
        TC* __restrict__ C, int LDC,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV 
)
{
#if (__CUDA_ARCH__ >= 200)

  // Semi-ring rank-k update template
  #include <gkmm_stencil.hpp>

  // SQ2NRM option
  if ( SQ2NRM ) 
  {
    __syncthreads();
    if ( idt < BLK_M && blx * BLK_M + idt < M ) 
    {
      sA[ 0 ][ idt ] = opkernel.A2[ blockIdx.z ][ blx * BLK_M + idt ];
    }
    if ( idt < BLK_N && bly * BLK_N + idt < N ) 
    {
      sB[ idt ][ 0 ] = opkernel.B2[ blockIdx.z ][ bly * BLK_N + idt ];
    }
    __syncthreads();
  }

  // Store C regs->dev
  #pragma unroll
  for ( n = 0; n < THR_N; n ++ ) 
  {
    int coord_dCn = bly * BLK_N + n * DIM_Y + idy;
    #pragma unroll
    for ( m = 0; m < THR_M; m ++ ) 
    {
      int coord_dCm = blx * BLK_M + m * DIM_X + idx;
      if ( coord_dCm < M && coord_dCn < N ) 
      {
        int offsC = coord_dCn * LDC + coord_dCm;
        TV &regC = rC[ n ][ m ];
        TC &memC = C[ offsC ];
        if ( SQ2NRM ) 
        {
          regC *= -2.0;
          regC += sA[ 0 ][ m * DIM_X + idx ] + sB[ n * DIM_Y + idy ][ 0 ];
        }
        memC = opkernel( regC, coord_dCm, coord_dCn, blockIdx.z );
      }
    }
  }

#endif /* (__CUDA_ARCH__ >= 200) */
};



template<
bool TRANSA, bool TRANSB,
const int DIM_X, const int DIM_Y, 
const int BLK_M, const int BLK_N, const int BLK_K,
const int DIM_XA, const int DIM_YA, 
const int DIM_XB, const int DIM_YB,
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV>
static __global__ void gkmm_kernel
(
  int M, int N, int K,
  const TA *Aarray[], int LDA, 
  const TB *Barray[], int LDB, 
        TC *Carray[], int LDC, 
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV
)
{
  gkmm_template_device<
    TRANSA, TRANSB,
    DIM_X, DIM_Y, 
    BLK_M, BLK_N, BLK_K,
    DIM_XA, DIM_YA, DIM_XB, DIM_YB, 
    (BLK_M/DIM_X), (BLK_N/DIM_Y), 
    SQ2NRM, OPKERNEL, OP1, OP2,
    TA, TB, TC, TV>
  (
    M, N, K, 
    Aarray[ blockIdx.z ], LDA,
    Barray[ blockIdx.z ], LDB,
    Carray[ blockIdx.z ], LDC,
    opkernel, op1, op2, initV 
  );
};


template<bool TRANSA, bool TRANSB,
const int DIM_X, const int DIM_Y, 
const int BLK_M, const int BLK_N, const int BLK_K,
const int DIM_XA, const int DIM_YA, 
const int DIM_XB, const int DIM_YB,
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV>
static __global__ void gkmm_kernel
(
  int M, int N, int K,
  const TA *A, int LDA, int LOA,
  const TB *B, int LDB, int LOB,
        TC *C, int LDC, int LOC,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV 
)
{
  gkmm_template_device<
    TRANSA, TRANSB,
    DIM_X, DIM_Y, 
    BLK_M, BLK_N, BLK_K,
    DIM_XA, DIM_YA, DIM_XB, DIM_YB, 
    (BLK_M/DIM_X), (BLK_N/DIM_Y), 
    SQ2NRM, OPKERNEL, OP1, OP2,
    TA, TB, TC, TV>
  (
    M, N, K, 
    A + LOA * blockIdx.z, LDA,
    B + LOB * blockIdx.z, LDB,
    C + LOC * blockIdx.z, LDC,
    opkernel, op1, op2, initV 
  );
};


template<
bool TRANSA, bool TRANSB,
const int DIM_X, const int DIM_Y, 
const int BLK_M, const int BLK_N, const int BLK_K,
const int dim_vec,
const int DIM_XA, const int DIM_YA, const int DIM_XB, const int DIM_YB,
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV>
void gkmm_internal
(
  cudaStream_t stream, 
  int m, int n, int k,
  const TA *Aarray[], int lda,
  const TB *Barray[], int ldb,
        TC *Carray[], int ldc,
  int batchSize,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV
)
{
  dim3 dimBlock( DIM_X, DIM_Y );
  dim3 dimGrid( gkmx_ceildiv( m, BLK_M ), gkmx_ceildiv( n, BLK_N ), batchSize );

  gkmm_template_batched_kernel<
    TRANSA, TRANSB,
    DIM_X, DIM_Y, 
    BLK_M, BLK_N, BLK_K, 
    DIM_XA, DIM_YA, DIM_XB, DIM_YB,
    SQ2NRM, OPKERNEL, OP1, OP2,
    TA, TB, TC, TV>
  <<< dimGrid, dimBlock, 0, stream >>>
  ( 
    m, n, k, 
    Aarray, lda,
    Barray, ldb,
    Carray, ldc,
    opkernel, op1, op2, initV
  );
};


/**
 *  batched version
 */ 
template<
bool TRANSA, bool TRANSB,
const int DIM_X, const int DIM_Y, 
const int BLK_M, const int BLK_N, const int BLK_K,
const int dim_vec,
const int DIM_XA, const int DIM_YA, const int DIM_XB, const int DIM_YB,
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV>
void gkmm_internal
(
  cudaStream_t stream, 
  int m, int n, int k,
  const TA *Aarray, int lda, int loa,
  const TB *Barray, int ldb, int lob,
        TC *Carray, int ldc, int loc,
  int batchSize,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV 
)
{
  dim3 dimBlock( DIM_X, DIM_Y );
  dim3 dimGrid( gkmx_ceildiv( m, BLK_M ), gkmx_ceildiv( n, BLK_N ), batchSize );

  gkmm_template_batched_strided_kernel<
    TRANSA, TRANSB,
    DIM_X, DIM_Y, 
    BLK_M, BLK_N, BLK_K, 
    DIM_XA, DIM_YA, DIM_XB, DIM_YB,
    SQ2NRM, OPKERNEL, OP1, OP2,
    TA, TB, TC, TV>
  <<< dimGrid, dimBlock, 0, stream >>>
  ( 
    m, n, k, 
    Aarray, lda, loa,
    Barray, ldb, lob,
    Carray, ldc, loc,
    opkernel, op1, op2, initV
  );
};


/**
 *  @brief This is the GKMM (General Kernel Matrix Matrix) template wrapper.
 *         This interface accepts double pointers. Here the type rules are
 *         op2: <TA,TB> to <TV>,
 *         op1: <TV,TV> to <TV>, and
 *         opkernel: <TV> to <TC>.
 *
 *  @param TA Type of *Aarray[]
 *  @param TB Type of *Barray[]
 *  @param TC Type of *Carray[]
 *  @param TV Type of of the output of op1 and op2
 *  @param SQ@NRM Whether opkernel uses a^2-2ab+b^2 expansion or not
 *  @param OPKERNEL Type of opkernel
 *  @param OP1 Type of op1
 *  @param OP2 Type of op2
 *
 *  @param stream CUDA stream
 *  @param transA Can either be CUBLAS_OP_N or CUBLAS_OP_T
 *  @param transB Can either be CUBLAS_OP_N or CUBLAS_OP_T
 *  @param m Input matrix dimension
 *  @param n Input matrix dimension
 *  @param k Input matrix dimension
 *  @param Aarray Input matrices in double pointers
 *  @param lda Leading dimension of matrix A
 *  @param Barray Input matrices in double pointers
 *  @param ldb Leading dimension of matrix B
 *  @param Carray Ounput matrices in double pointers
 *  @param ldc Leading dimension of matrix C
 *  @param batchSize number of indepedent gkmm operations
 *  @opkernel Closure of the kernel operators
 *  @op1 Closure of the semi-ring operator
 *  @op2 Closure of the semi-ring operator
 *  @init1 Initial value of the semi-ring operators
 *
 */ 
template<
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV> 
void gkmm
(
  cudaStream_t stream, 
  hmlpOperation_t transA, hmlpOperation_t transB, 
  int m, int n, int k,
  const TA *Aarray[], int lda, 
  const TB *Barray[], int ldb, 
        TC *Carray[], int ldc, 
  int batchSize,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV 
)
{
  // Early return.
  if ( m <= 0 || n <= 0 || k <= 0 ) return;

  // Specify input formats
  int shape = 0;
  if      ( transA == HMLP_OP_N && transB == HMLP_OP_N ) { shape = 0; } // nn
  else if ( transA == HMLP_OP_N && transB == HMLP_OP_T ) { shape = 1; } // nt
  else if ( transA == HMLP_OP_T && transB == HMLP_OP_N ) { shape = 3; } // tn
  else if ( transA == HMLP_OP_T && transB == HMLP_OP_T ) { shape = 4; } // tt

  // Autotuned decision tree
  #include <gkmm_autotune.hpp>
}

/**
 *  @brief This is the GKMM (General Kernel Matrix Matrix) template wrapper.
 *         This interface accepts pointers with strided access. 
 *         Here the type rules are
 *         op2: <TA,TB> to <TV>,
 *         op1: <TV,TV> to <TV>, and
 *         opkernel: <TV> to <TC>.
 *
 *  @param TA Type of *Aarray
 *  @param TB Type of *Barray
 *  @param TC Type of *Carray
 *  @param TV Type of of the output of op1 and op2
 *  @param SQ@NRM Whether opkernel uses a^2-2ab+b^2 expansion or not
 *  @param OPKERNEL Type of opkernel
 *  @param OP1 Type of op1
 *  @param OP2 Type of op2
 *
 *  @param stream CUDA stream
 *  @param transA Can either be CUBLAS_OP_N or CUBLAS_OP_T
 *  @param transB Can either be CUBLAS_OP_N or CUBLAS_OP_T
 *  @param m Input matrix dimension
 *  @param n Input matrix dimension
 *  @param k Input matrix dimension
 *  @param Aarray Input matrices (at least m-by-k-by-batchSize)
 *  @param lda Leading dimension of matrix A
 *  @param loa Stride between each matrix A
 *  @param Barray Input matrices (at least k-by-n-by-batchSize)
 *  @param ldb Leading dimension of matrix B
 *  @param lob Stride between each matrix B
 *  @param Carray Ounput matrices (at least m-by-n-by-batchSize)
 *  @param ldc Leading dimension of matrix C
 *  @param loc Stride between each matrix C
 *  @param batchSize number of indepedent gkmm operations
 *  @opkernel Closure of the kernel operators
 *  @op1 Closure of the semi-ring operator
 *  @op2 Closure of the semi-ring operator
 *  @init1 Initial value of the semi-ring operators
 *
 */ 
template<
bool SQ2NRM, typename OPKERNEL, typename OP1, typename OP2,
typename TA, typename TB, typename TC, typename TV> 
void gkmm
(
  cudaStream_t stream, 
  hmlpOperation_t transA, hmlpOperation_t transB, 
  int m, int n, int k,
  const TA *Aarray, int lda, int loa,
  const TB *Barray, int ldb, int lob,
        TC *Carray, int ldc, int loc,
  int batchSize,
  OPKERNEL opkernel, OP1 op1, OP2 op2, TV initV
)
{
  // Early return.
  if ( m <= 0 || n <= 0 || k <= 0 ) return;

  // Specify input formats
  int shape = 0;
  if      ( transA == HMLP_OP_N && transB == HMLP_OP_N ) { shape = 0; } // nn
  else if ( transA == HMLP_OP_N && transB == HMLP_OP_T ) { shape = 1; } // nt
  else if ( transA == HMLP_OP_T && transB == HMLP_OP_N ) { shape = 3; } // tn
  else if ( transA == HMLP_OP_T && transB == HMLP_OP_T ) { shape = 4; } // tt

  // Autotuned decision tree
  #include <gkmm_strided_autotune.hpp>
};


}; // end namespace gkmm
}; // end namespace hmlp


#endif // define GKMX_CUH