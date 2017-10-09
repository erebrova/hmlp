/**
 *  HMLP (High-Performance Machine Learning Primitives)
 *  
 *  Copyright (C) 2014-2017, The University of Texas at Austin
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see the LICENSE file.
 *
 **/  



#ifndef GEMM_HPP
#define GEMM_HPP

#include <hmlp.h>
#include <hmlp_blas_lapack.h>
#include <hmlp_runtime.hpp>

/** matrix view */
#include <containers/view.hpp>


namespace hmlp
{
namespace gemm
{

template<typename T>
class xgemmTask : public hmlp::Task
{
  public:

    T alpha = 0.0;

    hmlp::View<T> A;

    hmlp::View<T> B;

    T beta = 0.0;

    hmlp::View<T> C;

    void Set( 
        T alpha, hmlp::View<T> &A,
                 hmlp::View<T> &B,
        T beta,  hmlp::View<T> &C )
    {
      /** main arguments  */
      this->alpha = alpha;
      this->A = A;
      this->B = B;
      this->beta = beta;
      this->C = C;

      /** name and label */
      std::ostringstream ss;
      name = std::string( "gemm" );

      /** flops, mops, cost and event */
      double flops, mops;
      flops = 0.0;
      mops  = 0.0;
      cost  = 2.0 * C.row() * C.col();
      event.Set( name + label, flops, mops );
    };

    void DependencyAnalysis()
    {
      /** read A and B, read/write C */
      A.DependencyAnalysis( hmlp::ReadWriteType::R,  this );
      B.DependencyAnalysis( hmlp::ReadWriteType::R,  this );
      C.DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      this->TryEnqueue();
    };

    void Execute( Worker* user_worker )
    {
      std::string transA, transB;
      if ( A.IsTransposed() ) transA = "Transpose";
      else                    transA = "No transpose";
      if ( B.IsTransposed() ) transB = "Transpose";
      else                    transB = "No transpose";

      size_t m = C.row();
      size_t n = C.col();
      size_t k = A.col();

      assert( A.row() == m );
      assert( B.row() == k );
      assert( B.col() == n );

      //int rand_id = rand();

      //printf( "%d GEMM task %s %s %lu %lu %lu, %E, %E\n", 
      //    rand_id, transA.data(), transB.data(), m, n, k, alpha, beta ); fflush( stdout );
      //printf( "%d lda %lu ldb %lu ldc %lu\n", rand_id, A.ld(), B.ld(), C.ld() ); fflush( stdout );

      xgemm
      ( 
        transA.data(), transB.data(),
        m, n, k,
        alpha, A.data(), A.ld(),
               B.data(), B.ld(),
        beta,  C.data(), C.ld()
      );

      //printf( "%d end GEMM task %s %s %lu %lu %lu, %E, %E\n", 
      //    rand_id, transA.data(), transB.data(), m, n, k, alpha, beta ); fflush( stdout );
    };

}; /** end class xgemmTask */


/**
 *  @brief  This task is generated by the top level routine.
 **/
template<typename T>
class xgemmBarrierTask : public hmlp::Task
{
  public:

    hmlp::View<T> C;

    void Set( 
        T alpha, hmlp::View<T> &A,
                 hmlp::View<T> &B,
        T beta,  hmlp::View<T> &C )
    {
      /** main arguments */
      this->C = C;

      /** name and label */
      std::ostringstream ss;
      name = std::string( "gemmBarrier" );

      /** flops, mops, cost and event */
      double flops, mops;
      flops = 0.0;
      mops  = 0.0;
      cost  = 1.0;
      event.Set( name + label, flops, mops );
    };

    void DependencyAnalysis()
    {
      /** create RAW dependencies on all submatrices C */
      C.DependencyAnalysis( hmlp::ReadWriteType::R, this );
    };

    void Execute( Worker* user_worker )
    {
      /** empty */
    };

}; /** end class xgemmBarrierTask */


template<typename T>
void CreatexgemmTask(
    T alpha, hmlp::View<T> &A, 
             hmlp::View<T> &B, 
    T beta,  hmlp::View<T> &C )
{
  //std::string transA, transB;

  //printf( "\n");
  //printf( "alpha %3.1E beta %3.1E\n", alpha, beta );
  //printf( "A: " ); A.Print();
  //printf( "B: " ); B.Print();
  //printf( "C: " ); C.Print();

  //if ( A.IsTransposed() ) transA = "Transpose";
  //else                    transA = "No transpose";
  //if ( B.IsTransposed() ) transB = "Transpose";
  //else                    transB = "No transpose";

  //size_t m = C.row();
  //size_t n = C.col();
  //size_t k = A.col();

  //xgemm
  //( 
  //  transA.data(), transB.data(),
  //  m, n, k,
  //  alpha, A.data(), A.ld(),
  //         B.data(), B.ld(),
  //  beta,  C.data(), C.ld()
  //);
 
  auto *task = new xgemmTask<T>();
  task->Set( alpha, A, B, beta, C );
  task->Submit();
  task->DependencyAnalysis();

}; /** end xgemmTask() */


/**
 *  @brief
 */ 
template<size_t NB = 512, typename T>
void xgemm_var1( 
    T alpha, hmlp::View<T> &A, 
             hmlp::View<T> &B, 
    T beta,  hmlp::View<T> &C )
{
  //printf( "var1\n" ); fflush( stdout );

  /** all subviews */
  hmlp::View<T> AL, AR, 
                A0, A1, A2;
  hmlp::View<T> BT, BB, 
                B0, B1, B2;
  
  /** A = [ AL, AR ] */
  A.Partition1x2( AL, AR, 0, LEFT );
  /** B = [ BT; BB ] */
  B.Partition2x1( BT,
                  BB,     0, TOP  ); 

  //printf( "AL.col() %lu AR.col() %lu A.col() %lu\n", AL.col(), AR.col(), A.col() );

  while ( AL.col() < A.col() )
  {
    //printf( "AL.col() %lu AR.col() %lu A.col() %lu\n", AL.col(), AR.col(), A.col() );
    size_t b = std::min( AR.col(), NB );

    /** repartition A */
    Repartition1x2To1x3( AL,      AR,
                         /** **** */
                         A0,  A1, A2, b, RIGHT );
    /** repartition B */
    Repartition2x1To3x1( BT, /**/ B0,
                             /**/ B1,
                         BB, /**/ B2, b, BOTTOM );

    /** --------------------------------------------------- */
    CreatexgemmTask( alpha, A1, B1, beta, C );
    beta = 1.0;
    /** --------------------------------------------------- */

    /** merge A */
    ContinueWith1x3To1x2( AL,      AR,
                          /** **** */
                          A0,  A1, A2, LEFT );
    /** merge B */
    ContinueWith3x1To2x1( BT, /**/ B0,
                              /**/ B1,
                          BB, /**/ B2,  TOP );

  } /** end while */

  //printf( "end var1\n" ); fflush( stdout );
}; /** end xgemm_var1() */


/**
 *  @brief [ A * BL + CL, A * BR + CR ] 
 */ 
template<size_t NB = 512, typename T>
void xgemm_var2( 
    T alpha, hmlp::View<T> &A, 
             hmlp::View<T> &B, 
    T beta,  hmlp::View<T> &C )
{
  //printf( "var2\n" ); fflush( stdout );

  /** all subviews */
  hmlp::View<T> CL, CR, 
                C0, C1, C2;
  hmlp::View<T> BL, BR, 
                B0, B1, B2;
  
  C.Partition1x2( CL, CR, 0, LEFT );
  B.Partition1x2( BL, BR, 0, LEFT );

  while ( BL.col() < B.col() )
  {
    size_t b = std::min( BR.col(), NB );

    /** repartition C */
    Repartition1x2To1x3( CL,      CR,
                         /** **** */
                         C0,  C1, C2, b, RIGHT );
    /** repartition B */
    Repartition1x2To1x3( BL,      BR,
                         /** **** */
                         B0,  B1, B2, b, RIGHT );

    /** --------------------------------------------------- */
    xgemm_var1( alpha, A, B1, beta, C1 );
    /** --------------------------------------------------- */

    /** merge C */
    ContinueWith1x3To1x2( CL,      CR,
                          /** **** */
                          C0,  C1, C2, LEFT );
    /** merge B */
    ContinueWith1x3To1x2( BL,      BR,
                          /** **** */
                          B0,  B1, B2, LEFT );

  } /** end while */

  //printf( "end var2\n" ); fflush( stdout );
}; /** end xgemm_var2() */


/**
 *  @brief [ AT * B + CT; AB * B + CB ] 
 */ 
template<size_t NB = 512, typename T>
void xgemm_var3( 
    T alpha, hmlp::View<T> &A, 
             hmlp::View<T> &B, 
    T beta,  hmlp::View<T> &C )
{
  //printf( "var3\n" ); fflush( stdout );

  /** all subviews */
  hmlp::View<T> AT, A0, CT, C0, 
                AB, A1, CB, C1,
                    A2,     C2;

  A.Partition2x1( AT,
                  AB,     0, TOP  ); 
  C.Partition2x1( CT,
                  CB,     0, TOP  ); 

  while ( AT.row() < A.row() )
  {
    size_t b = std::min( AB.row(), NB );

    /** repartition A */
    Repartition2x1To3x1( AT, /**/ A0,
                             /**/ A1,
                         AB, /**/ A2, b, BOTTOM );
    /** repartition B */
    Repartition2x1To3x1( CT, /**/ C0,
                             /**/ C1,
                         CB, /**/ C2, b, BOTTOM );

    /** --------------------------------------------------- */
    xgemm_var2( alpha, A1, B, beta, C1 );
    /** --------------------------------------------------- */

    /** merge A */
    ContinueWith3x1To2x1( AT, /**/ A0,
                              /**/ A1,
                          AB, /**/ A2,  TOP );
    /** merge C */
    ContinueWith3x1To2x1( CT, /**/ C0,
                              /**/ C1,
                          CB, /**/ C2,  TOP );
  }; /** end while */

  //printf( "end var3\n" ); fflush( stdout );
}; /** end xgemm_var3() */


/**
 *  @breif  Interface for automatic task-bsed parallelism.
 **/ 
template<size_t NB = 512, typename T>
void xgemm( 
    T alpha, hmlp::View<T> &A, 
             hmlp::View<T> &B, 
    T beta,  hmlp::View<T> &C )
{
  /** try to  */
  A.CreateLeafMatrixBlocks( NB, NB );
  B.CreateLeafMatrixBlocks( NB, NB );
  C.CreateLeafMatrixBlocks( NB, NB );

  /** call back */
  if ( hmlp_is_in_epoch_session() )
  {
    auto *begXGEMMtask = new xgemmBarrierTask<T>();
    auto *endXGEMMtask = new xgemmBarrierTask<T>();
    /** 
     *  The reason why we need the begin barrier
     *  task is to ensure the whole DAG will be
     *  inserted at once. Otherwise, the dependent
     *  task may not be created while the traversal
     *  has already reached by other workers.
     *
     *  The solution is to create a beginning barrier
     *  such that all the following tasks depend on it.
     *  Only enqueue the beginning barrier while all
     *  dependent tasks have been created.
     */
    begXGEMMtask->Set( alpha, A, B, beta, C );
    begXGEMMtask->Submit();
    begXGEMMtask->DependencyAnalysis();

    /**
     *  Now we create all dependent tasks. Since they
     *  all dependent on begXGEMMtask. They all have
     *  STATUS=NOTREADY. Thus, no one will be enqueued.
     */ 
    xgemm_var3( alpha, A, B, beta, C );

    /**
     *  Create a termination barrier such that it depends
     *  on all tasks.
     */ 
    endXGEMMtask->Set( alpha, A, B, beta, C );
    endXGEMMtask->Submit();
    endXGEMMtask->DependencyAnalysis();

    /**
     *  Now enqueue begXGEMMtask and callback with endXGEMMtask
     */
    begXGEMMtask->TryEnqueue();
    endXGEMMtask->CallBackWhileWaiting();
  }
  else
  {
    xgemm_var3( alpha, A, B, beta, C );
  }

}; /** xgemm() */


template<typename T>
void xgemm( 
    hmlpOperation_t transA, hmlpOperation_t transB, 
    T alpha, hmlp::Data<T> &A, 
             hmlp::Data<T> &B, 
    T beta,  hmlp::Data<T> &C )
{
  const bool TRANS = true;
  const bool NOTRANS = true;

  hmlp::View<T> Aview, Bview, Cview;

  if ( transA == HMLP_OP_T ) Aview.Set( true, A );
  else                       Aview.Set( false, A );
  if ( transB == HMLP_OP_T ) Bview.Set( true, B );
  else                       Bview.Set( false, B );

  /** C is always not transpose */
  Cview.Set( C );

  xgemm( alpha, Aview, Bview, beta, Cview );
  
}; /** xgemm() */


}; /** end namespace gemm */
}; /** end namespace hmlp */


#endif /** define GEMM_HPP */
