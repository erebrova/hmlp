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



#ifndef HFAMILY_HPP
#define HFAMILY_HPP

#include <assert.h>
#include <typeinfo>
#include <algorithm>
#include <random>


#include <hmlp.h>
#include <hmlp_blas_lapack.h>
#include <hmlp_util.hpp>
#include <hmlp_thread.hpp>
#include <hmlp_runtime.hpp>

#include <containers/tree.hpp>
#include <containers/view.hpp>

#include <gofmm/gofmm.hpp>

//#define DEBUG_IGOFMM 1

namespace hmlp
{
namespace hfamily
{



/**
 *
 * for each level 
 *   for each alpha
 *     
 *     if ( leaf ) 
 *       
 *       LL' = Chol( Kaa )
 *       U   = inv( L ) * proj' or
 *       U'  = proj * inv( L' )
 *       QR  = qr( U ) or 
 *       LQ' = lq( U' )
 *
 *     else
 *       
 *       LL' = Chol( I + k.R * C * k.R' )
 *
 *       if ( not root )
 *
 *         U   = inv( L ) * [ l.R        * proj' or
 *                               r.R ]
 *         U'  = proj' *  [ l.R        * inv( L' )
 *                             r.R ]
 *         QR  = qr( U ) or
 *         LQ' = lq( U' )
 *
 **/ 


typedef enum 
{
  HODLR,
  PHSS,
  HSS,
  ASKIT
} HFamilyType;

typedef enum 
{
  UV,
  COLUMNID,
  ROWID,
  CUR,
  LOWRANKPLUSSPARSE
} OffdiagonalType;

//template<HFamilyType TH, OffdiagonalType TOFFDIAG, typename T>
template<typename T>
class Factor
{
  public:

    Factor() {};
    
    void SetupFactor
    (
      bool issymmetric, bool do_ulv_factorization,
      bool isleft, bool isleaf, bool isroot,
      /** n == nl + nr (left + right) */
      std::size_t n, std::size_t nl, std::size_t nr,
      /** s <= sl + sr */
      std::size_t s, std::size_t sl, std::size_t sr
    )
    {
      this->issymmetric = issymmetric;
      this->do_ulv_factorization = do_ulv_factorization;
      this->isleft = isleft;
      this->isleaf = isleaf;
      this->isroot = isroot;
      this->n = n; this->nl = nl; this->nr = nr;
      this->s = s; this->sl = sl; this->sr = sr;
    };

    void SetupFactor
    (
      bool issymmetric, bool do_ulv_factorization,
      bool isleft, bool isleaf, bool isroot,
      std::size_t n, std::size_t nl, std::size_t nr,
      std::size_t s, std::size_t sl, std::size_t sr,
      /** n-by-?; its rank depends on mu sibling */
      hmlp::Data<T> &U,
      /** ?-by-n; its rank depends on my sibling */
      hmlp::Data<T> &V
    )
    {
      SetupFactor( issymmetric, do_ulv_factorization, 
          isleft, isleaf, isroot, n, nl, nr, s, sl, sr );
    };


    void CheckCondition()
    {
      assert( do_ulv_factorization && issymmetric );
      T max_diag = 0.0;
      T min_diag = 0.0;

      for ( size_t i = 0; i < Z.row(); i ++ )
      {
        T abs_diag =  std::abs( Z( i, i ) );

        if ( !i )
        {
          max_diag = abs_diag;
          min_diag = abs_diag;
        }

        if ( abs_diag > max_diag ) max_diag = abs_diag;
        if ( abs_diag < min_diag ) min_diag = abs_diag;
      }

      printf( "condiditon( Z ): min_diag %3.1E max_diag %3.1E ratio %3.1E\n",
          min_diag, max_diag, min_diag / max_diag );
    };



    /**
     *
     *
     **/ 
    void Factorize( hmlp::Data<T> &Kaa ) 
    {
      assert( isleaf );
      assert( Kaa.row() == n ); assert( Kaa.col() == n );

      /** initialize */
      Z = Kaa;

      /** LU or Cholesky factorization */
      if ( do_ulv_factorization && issymmetric ) 
      {
        /** Cholesky factorization */
        if ( 1 )
        {
          xpotrf( "Lower", n, Z.data(), n );
          //CheckCondition();
        }
        else
        {
          ipiv.resize( n, 0 );
          xgetrf( n, n, Z.data(), n, ipiv.data() );
        }
      }
      else
      {
        ipiv.resize( n, 0 );

        /** compute 1-norm of Z */
        T nrm1 = 0.0;
        for ( size_t i = 0; i < Z.size(); i ++ ) 
          nrm1 += std::abs( Z[ i ] );
        //printf( "1-norm\n" ); fflush( stdout );

        /** pivoted LU factorization */
        xgetrf(   n, n, Z.data(), n, ipiv.data() );
        //printf( "getrf\n" ); fflush( stdout );

        /** compute 1-norm condition number */
        T rcond1 = 0.0;
        hmlp::Data<T> work( Z.row(), 4 );
        std::vector<int> iwork( Z.row() );
        xgecon( "1", Z.row(), Z.data(), Z.row(), nrm1, 
            &rcond1, work.data(), iwork.data() );
        if ( 1.0 / rcond1 > 1E+6 )
          printf( "Warning! large 1-norm condition number %3.1E, nrm1( Z ) %3.1E\n", 
              1.0 / rcond1, nrm1 );
      }

    }; /** end Factorize() */


    /**    
     *   two-sided UCVt   one-sided UBt
     *   
     *      | sl   sr        | sl   sr
     *   -------------    -------------  
     *   nl | Ul          nl | Ul  
     *   nr |      Ur     nr |      Ur
     *
     *      | sl   sr
     *   -------------  
     *   sl |     Clr
     *   sr | Crl      
     *
     *      | nl   nr        | nl   nr
     *   -------------    -------------
     *   sl | Vlt         sl |      Brt
     *   sr |      Vrt    sr | Blt
     *
     *
     **/
    template<bool SYMMETRIC>
    void Factorize
    ( 
      /** Ul,  nl-by-sl */
      hmlp::Data<T> &Ul, 
      /** Ur,  nr-by-sr */
      hmlp::Data<T> &Ur, 
      /** Vl,  nl-by-sr */
      hmlp::Data<T> &Vl,
      /** Vr,  nr-by-sr */
      hmlp::Data<T> &Vr
    )
    {
      assert( !isleaf );
      //assert( Ul.row() == nl ); assert( Ul.col() == sl );
      //assert( Ur.row() == nr ); assert( Ur.col() == sr );
      //assert( Vl.row() == nl ); assert( Vl.col() == sl );
      //assert( Vr.row() == nr ); assert( Vr.col() == sr );

      /** even SYMMETRIC this routine uses LU factorization */
      if ( SYMMETRIC )
      {
        assert( Crl.row() == sr ); assert( Crl.col() == sl );
      }
      else
      {
        assert( Clr.row() == sl ); assert( Clr.col() == sr );
        assert( Crl.row() == sr ); assert( Crl.col() == sl );
      }
     
      /** 
       *  clean up and begin with Z = eye( sl + sr ) =     | sl  sr
       *                                                ------------
       *                                                sl | Zrl Ztr
       *                                                sr | Zbl Zbr 
       **/
      Z.resize( 0, 0 );
      Z.resize( sl + sr, sl + sr, 0.0 );
      for ( size_t i = 0; i < sl + sr; i ++ ) Z[ i * Z.row() + i ] = 1.0;



      if ( do_ulv_factorization )
      {
        /**
         *  Z = I + UR * C * VR' = [                 I  URl * Clr * VRr'
         *                            URr * Crl * VRl'                 I ]
         **/
        if ( SYMMETRIC ) /** Cholesky */
        {
          /** Zbl = URr * Crl * VRl' */
          hmlp::Data<T> Zbl = Crl;

          //printf( "Crl\n" );
          //Crl.Print();


          /** trmm */
          xtrmm
          ( 
            "Right", "Upper", "Transpose", "Non-unit",
            Zbl.row(), Zbl.col(),
            1.0,  Ul.data(),  Ul.row(),
                 Zbl.data(), Zbl.row()
          );
          //printf( "Ul.row() %lu Zbl.row() %lu Zbl.col() %lu\n",
          //    Ul.row(), Zbl.row(), Zbl.col() );

          /** trmm */
          xtrmm
          ( 
            "Left", "Upper", "Non-transpose", "Non-unit",
            Zbl.row(), Zbl.col(),
            1.0,  Ur.data(),  Ur.row(),
                 Zbl.data(), Zbl.row()
          );
          //printf( "Ur.row() %lu Zbl.row() %lu Zbl.col() %lu\n",
          //    Ur.row(), Zbl.row(), Zbl.col() );

          /** Zbl */
          for ( size_t j = 0; j < sl; j ++ )
            for ( size_t i = 0; i < sr; i ++ )
            {
              Z( sl + i, j ) = Zbl( i, j );
              Z( j, sl + i ) = Zbl( i, j );
            }

          /** LL' = potrf( Z ) */
          if ( 1 )
          {
            xpotrf( "Lower", Z.row(), Z.data(), Z.row() );
            //CheckCondition();
          }
          else
          {
            /** pivoting row indices */
            ipiv.resize( Z.row(), 0 );
            xgetrf( Z.row(), Z.col(), Z.data(), Z.row(), ipiv.data() );
          }
        }
        else /** LU */
        {
          /** pivoting row indices */
          ipiv.resize( Z.row(), 0 );
        }
      }
      else /** Sherman-Morrison-Woodbury */
      {
        /** pivoting row indices */
        ipiv.resize( Z.row(), 0 );

        /**    
         *  Z = I + CVtU =  [        I  ClrVrtUr
         *                    CrlVltUl         I ] 
         **/  
        std::vector<T> VltUl( sl * sl, 0.0 );
        std::vector<T> VrtUr( sr * sr, 0.0 );

        /** VltUl */
        xgemm( "T", "N", sl, sl, nl, 
            1.0,    Vl.data(), nl, 
            Ul.data(), nl, 
            0.0, VltUl.data(), sl );

        /** VrtUr */
        xgemm( "T", "N", sr, sr, nr, 
            1.0,    Vr.data(), nr, 
            Ur.data(), nr, 
            0.0, VrtUr.data(), sr );

        /** CrlVltUl */
        xgemm( "N", "N", sr, sl, sl,
            1.0,   Crl.data(), sr, 
            VltUl.data(), sl, 
            0.0,     Z.data() + sl, sl + sr );


        if ( SYMMETRIC )
        {
          /** Crl'VrtUr */
          xgemm( "T", "N", sl, sr, sr,
              1.0,   Crl.data(), sr, 
              VrtUr.data(), sr, 
              0.0,     Z.data() + ( sl + sr ) * sl, sl + sr );
        }
        else
        {
          printf( "bug\n" ); exit( 1 );
          /** ClrVrtUr */
          xgemm( "N", "N", sl, sr, sr,
              1.0,   Clr.data(), sl, 
              VrtUr.data(), sr, 
              0.0,     Z.data() + ( sl + sr ) * sl, sl + sr );
        }

        /** compute 1-norm of Z */
        T nrm1 = 0.0;
        for ( size_t i = 0; i < Z.size(); i ++ ) 
          nrm1 += std::abs( Z[ i ] );

        /** LU factorization */
        xgetrf( Z.row(), Z.col(), Z.data(), Z.row(), ipiv.data() );

        /** record points of children factors */
        this->Ul = &Ul;
        this->Ur = &Ur;
        this->Vl = &Vl;
        this->Vr = &Vr;

        /** compute 1-norm condition number */
        T rcond1 = 0.0;
        hmlp::Data<T> work( Z.row(), 4 );
        std::vector<int> iwork( Z.row() );
        xgecon( "1", Z.row(), Z.data(), Z.row(), nrm1, 
            &rcond1, work.data(), iwork.data() );
        if ( 1.0 / rcond1 > 1E+6 )
          printf( "Warning! large 1-norm condition number %3.1E\n", 
              1.0 / rcond1 ); fflush( stdout );
      }

    }; /** end Factorize() */


    /** 
     *
     */
    template<bool SYMMETRIC>
    void Multiply( hmlp::View<T> &bl, hmlp::View<T> &br )
    {
      assert( !isleaf && bl.col() == br.col() );
    
      size_t nrhs = bl.col();

      std::vector<T> ta( ( sl + sr ) * nrhs );
      std::vector<T> tl(      sl * nrhs );
      std::vector<T> tr(      sr * nrhs );

      /** Vl' * bl */
      xgemm( "T", "N", sl, nrhs, nl,
          1.0, Vl->data(), nl, 
                bl.data(), bl.ld(), 
          0.0,  tl.data(), sl );
      /** Vr' * br */
      xgemm( "T", "N", sr, nrhs, nr,
          1.0, Vr->data(), nr, 
                br.data(), br.ld(), 
          0.0,  tr.data(), sr );

      /** Crl * Vl' * bl */
      xgemm( "N", "N", sr, nrhs, sl,
          1.0, Crl.data(), sr, 
                tl.data(), sl, 
          0.0,  ta.data() + sl, sl + sr );

      if ( SYMMETRIC )
      {
        /** Crl' * Vr' * br */
        xgemm( "T", "N", sl, nrhs, sr,
            1.0, Crl.data(), sr, 
                  tr.data(), sr, 
            0.0,  ta.data(), sl + sr );
      }
      else
      {
        printf( "bug here !!!!!\n" ); fflush( stdout ); exit( 1 );
        /** Clr * Vr' * br */
        xgemm( "N", "N", sl, nrhs, sr,
            1.0, Clr.data(), sl, 
                  tr.data(), sr, 
            0.0,  ta.data(), sl + sr );
      }

      /** bl += Ul * xl */
      xgemm( "N", "N", nl, nrhs, sl,
        -1.0, Ul->data(), nl, 
               ta.data(), sl + sr, 
         1.0,  bl.data(), bl.ld() );

      /** br += Ur * xr */
      xgemm( "N", "N", nr, nrhs, sr,
         -1.0, Ur->data(), nr, 
                ta.data() + sl, sl + sr, 
          1.0,  br.data(), br.ld() );
    };

    /**
     *  @brief Solver for leaf nodes
     */
    template<bool TRANS>
    void Solve( hmlp::View<T> &rhs ) 
    {
      /** assure this is a leaf node */
      assert( isleaf );
      assert( !do_ulv_factorization );
      assert( rhs.data() && Z.data() );
      assert( ipiv.data() );

      //rhs.Print();

      size_t nrhs = rhs.col();

      /** LU solver */
      xgetrs( "Non-transpose", rhs.row(), nrhs, 
          Z.data(), Z.row(), ipiv.data(), 
          rhs.data(), rhs.ld() );

    }; /** end Solve() */



    /**
     *  @brief b - U * inv( Z ) * C * V' * b 
     */
    template<bool TRANS, bool SYMMETRIC = true>
    void Solve( hmlp::View<T> &bl, hmlp::View<T> &br ) 
    {
      size_t nrhs = bl.col();

      //bl.Print();
      //br.Print();

      /** assertion */
      assert( !do_ulv_factorization );
      assert( bl.col() == br.col() );
      assert( bl.row() == nl );
      assert( br.row() == nr );
      assert( Ul && Ur && Vl && Vr );

      /** buffer */
//      hmlp::Data<T> ta( sl + sr, nrhs );
//      hmlp::Data<T> tl(      sl, nrhs );
//      hmlp::Data<T> tr(      sr, nrhs );

      std::vector<T> ta( ( sl + sr ) * nrhs );
      std::vector<T> tl(      sl * nrhs );
      std::vector<T> tr(      sr * nrhs );


      ///** views of buffer */
      //hmlp::View<T> xa( ta ), xl, xr;

      ///** xa = [ xl; xr; ] */
      //xa.Partition2x1
      //( 
      //  xl, 
      //  xr, sl
      //);


        /** Vl' * bl */
        xgemm( "T", "N", sl, nrhs, nl,
            1.0, Vl->data(), nl, 
                  bl.data(), bl.ld(), 
            0.0,  tl.data(), sl );
        /** Vr' * br */
        xgemm( "T", "N", sr, nrhs, nr,
            1.0, Vr->data(), nr, 
                  br.data(), br.ld(), 
            0.0,  tr.data(), sr );


        /** Crl * Vl' * bl */
        xgemm( "N", "N", sr, nrhs, sl,
            1.0, Crl.data(), sr, 
            tl.data(), sl, 
            0.0,  ta.data() + sl, sl + sr );

        if ( SYMMETRIC )
        {
          /** Crl' * Vr' * br */
          xgemm( "T", "N", sl, nrhs, sr,
              1.0, Crl.data(), sr, 
              tr.data(), sr, 
              0.0,  ta.data(), sl + sr );
        }
        else
        {
          printf( "bug here !!!!!\n" ); fflush( stdout ); exit( 1 );
          /** Clr * Vr' * br */
          xgemm( "N", "N", sl, nrhs, sr,
              1.0, Clr.data(), sl, 
              tr.data(), sr, 
              0.0,  ta.data(), sl + sr );
        }

        /** inv( Z ) * x */
        xgetrs( "N", sl + sr, nrhs, 
            Z.data(), Z.row(), ipiv.data(), 
            ta.data(), sl + sr );

      /** bl -= Ul * xl */
      xgemm( "N", "N", nl, nrhs, sl,
          -1.0, Ul->data(), nl, 
          ta.data(), sl + sr, 
          1.0,  bl.data(), bl.ld() );

      /** br -= Ur * xr */
      xgemm( "N", "N", nr, nrhs, sr,
          -1.0, Ur->data(), nr, 
          ta.data() + sl, sl + sr, 
          1.0,  br.data(), br.ld() );

    }; /** end Solve() */




    void Telescope
    (
      bool DO_INVERSE,
      /** n-by-s */
      hmlp::Data<T> &Pa,
      /** s-by-(sl+sr) */
      hmlp::Data<T> &Palr 
    )
    {
      assert( isleaf ); 
      /** Initialize Pa */
      Pa.resize( n, s, 0.0 );

      /** create view and subviews for Pa */
      //hmlp::View<T> Xa;

      //Xa.Set( Pa ); 

      assert( Palr.row() == s ); assert( Palr.col() == n );

      /** Pa = Palr' */
      for ( size_t j = 0; j < Pa.col(); j ++ )
        for ( size_t i = 0; i < Pa.row(); i ++ )
          Pa( i, j ) = Palr( j, i );

      if ( DO_INVERSE )
      {
        if ( do_ulv_factorization )
        {
          if ( 1 )
          {
            xtrsm( "Left", "Lower", "No transpose", "Non-unit", 
                Pa.row(), Pa.col(), 
                1.0,  Z.data(),  Z.row(), Pa.data(), Pa.row() );
          }
          else
          {
            xlaswp( Pa.col(), Pa.data(), Pa.row(), 
                1, Pa.row(), ipiv.data(), 1 );
            xtrsm( "Left", "Lower", "No transpose", "Unit", Pa.row(), Pa.col(), 
                1.0,  Z.data(), Z.row(), Pa.data(), Pa.row() );
          }
        }
        else
        {
          assert( ipiv.size() );
          /** LU solver */
          xgetrs( "Non-transpose", 
              n, s, Z.data(), n, ipiv.data(), Pa.data(), n );
        }
      }


      //printf( "call solve from telescope\n" ); fflush( stdout );
      //if ( DO_INVERSE ) Solve<true>( Xa );
      //printf( "call solve from telescope (exist)\n" ); fflush( stdout );

    }; /** end Telescope() */


    /** RIGHT: V = [ P(:, 0:st-1) * Vl , P(:,st:st+sb-1) * Vr ] 
     *  LEFT:  U = [ Ul * P(:, 0:st-1)'; Ur * P(:,st:st+sb-1) ] */
    void Telescope
    ( 
      bool DO_INVERSE,
      /** n-by-s */
      hmlp::Data<T> &Pa,
      /** s-by-(sl+sr) */
      hmlp::Data<T> &Palr,
      /** nl-by-sl */
      hmlp::Data<T> &Pl,
      /** nr-by-sr */
      hmlp::Data<T> &Pr
    ) 
    {
      assert( !isleaf );
      assert( n == nl + nr );
      assert( Pl.col() == sl );
      assert( Pr.col() == sr );
      assert( Palr.row() == s  ); assert( Palr.col() == ( sl + sr ) );

      /** Initialize Pa */
      Pa.resize( 0, 0 );

      /** create view and subviews for Pa */
      //hmlp::View<T> Xa;

      //Xa.Set( Pa ); 
      //assert( Xa.row() == Pa.row() ); 
      //assert( Xa.col() == Pa.col() ); 

      if ( do_ulv_factorization )
      {
        Pa.resize( sl + sr, s, 0.0 );

        /** Pa = Palr' */
        for ( size_t j = 0; j < Pa.col(); j ++ )
          for ( size_t i = 0; i < Pa.row(); i ++ )
            Pa[ j * Pa.row() + i ] = Palr[ i * Palr.row() + j ];

        /** Pa( 0:sl-1, : ) = Pl * Palr( :, 0:sl-1 )' */
        xtrmm( 
           "Left", "Upper", "Non-transpose", "Non-unit", 
           sl, s,
           1.0,   Pl.data(), Pl.row(), 
                  Pa.data(), Pa.row() );
        //  printf( "Pl.row() %lu Pa.row() %lu Pa.col() %lu\n",
        //      Pl.row(), Pa.row(), Pa.col() );

        /** Pa( sl:sl+sr-1, : ) = Pr * Palr( :, sl:sl+sr-1 )' */
        xtrmm( "Left", "Upper", "No transpose", "Non-unit", 
           sr, s,
           1.0,   Pr.data()     , Pr.row(), 
                  Pa.data() + sl, Pa.row() ); 
        //  printf( "Pr.row() %lu Pa.row() %lu Pa.col() %lu\n",
        //      Pr.row(), Pa.row(), Pa.col() );

        /** inv( L ) * Pa */
        if ( DO_INVERSE )
        {
          if ( 1 )
          {
            xtrsm( "Left", "Lower", "No transpose", "Non-unit", 
                Pa.row(), Pa.col(), 
                1.0,  Z.data(),  Z.row(), 
                     Pa.data(), Pa.row() ); 
          }
          else
          {
            xlaswp( Pa.col(), Pa.data(), Pa.row(), 
                1, Pa.row(), ipiv.data(), 1 );
            xtrsm( "Left", "Lower", "No transpose", "Unit", Pa.row(), Pa.col(), 
                1.0,  Z.data(),  Z.row(), 
                     Pa.data(), Pa.row() ); 
          }
          //printf( "Z.row() %lu Z.col() %lu\n", Z.row(), Z.col() );
        }

      }
      else /** Shernman-Morrison-Woodbury */
      {
        Pa.resize( nl + nr, s, 0.0 );

        ///** */
        //hmlp::View<T> Xl, Xr;

        ///** Xa = [ Xl; Xr; ] */
        //Xa.Partition2x1
        //( 
        //  Xl, 
        //  Xr, nl
        //);

        //assert( Xl.row() == nl );
        //assert( Xr.row() == nr );
        //assert( Xl.col() == s );
        //assert( Xr.col() == s );
        

        /** Pa( 0:nl-1, : ) = Pl * Palr( :, 0:sl-1 )' */
        xgemm( "N", "T", nl, s, sl, 
            1.0,   Pl.data(), nl, 
                 Palr.data(), s, 
            0.0,   Pa.data(), n );
        /** Pa( nl:n-1, : ) = Pr * Palr( :, sl:sl+sr-1 )' */
        xgemm( "N", "T", nr, s, sr, 
            1.0,   Pr.data(), nr, 
                 Palr.data() + s * sl, s, 
            0.0,   Pa.data() + nl, n );

        

        //if ( DO_INVERSE ) Solve<true>( Xl, Xr );
        //printf( "end inner solve from telescope\n" ); fflush( stdout );

        if ( DO_INVERSE )
        {
            hmlp::Data<T> x( sl + sr, s );
            hmlp::Data<T> xl( sl, s );
            hmlp::Data<T> xr( sr, s );

            /** xl = Vlt * Pa( 0:nl-1, : ) */
            xgemm( "T", "N", sl, s, nl, 
                1.0, Vl->data(), nl, 
                Pa.data(), n, 
                0.0,  xl.data(), sl );
            /** xr = Vrt * Pa( nl:n-1, : ) */
            xgemm( "T", "N", sr, s, nr, 
                1.0, Vr->data(), nr, 
                Pa.data() + nl, n, 
                0.0,  xr.data(), sr );

            /** b = [ Crl' * xr;
             *        Crl  * xl; ] */
            xgemm( "T", "N", sl, s, sr, 
                1.0, Crl.data(), sr, 
                xr.data(), sr, 
                0.0,   x.data(), sl + sr );
            xgemm( "N", "N", sr, s, sl, 
                1.0, Crl.data(), sr, 
                xl.data(), sl, 
                0.0,   x.data() + sl, sl + sr );

            /** b = inv( Z ) * b */
            xgetrs( "N", x.row(), x.col(), 
                Z.data(), Z.row(), ipiv.data(), 
                x.data(), x.row() );

            /** Pa( 0:nl-1, : ) -= Ul * b( 0:sl-1, : ) */
            xgemm( "N", "N", nl, s, sl, 
                -1.0, Ul->data(), nl, 
                x.data(), sl + sr, 
                1.0,  Pa.data(), n );
            /** Pa( nl:n-1, : ) -= Ur * b( sl:sl+sr-1, : ) */
            xgemm( "N", "N", nr, s, sr, 
                -1.0, Ur->data(), nr, 
                x.data() + sl, sl + sr, 
                1.0,  Pa.data() + nl, n );
        } /** end if ( DO_INVERSE ) */
      } /** end if ( do_ulv_factorization )*/
    };

    /** */
    void Orthogonalization()
    {
      assert( do_ulv_factorization );

      //hmlp::Data<T> G = U;

      /** initialize tau */
      tau.resize( 0, 0 );
      tau.resize( std::min( U.row(), U.col() ), 1 );

      /** initialize work space */
      hmlp::Data<T> work( U.col() * 512, 1 );
      //printf( "U.row() %lu U.col() %lu tau.size() %lu work.size() %lu\n",
      //    U.row(), U.col(), tau.size(), work.size() );

      xgeqrf(
          U.row(), U.col(), U.data(), U.row(),
          tau.data(), work.data(), work.size() );
      //printf( "finish xgeqrf\n" );

      /** copy U to Q */
      Q = U;

     

      //printf( "U:\n" );
      //U.Print();
      xorgqr(
          Q.row(), Q.col(), Q.col(),
          Q.data(), Q.row(), tau.data(), 
          work.data(), work.size() );
      
      //printf( "Q\n" );
      //Q.Print();
    };


    /**
     *  @brief here x is the return value
     */ 
    void ULVForwardSolve( hmlp::View<T> &x )
    {
      /** get the view of right hand sides */
      auto &b = bview;

      if ( isleaf )
      {
        bskel.resize( b.row(), b.col() );
        for ( size_t j = 0; j < b.col(); j ++ )
          for ( size_t i = 0; i < b.row(); i ++ )
            bskel( i, j ) = b( i, j );
      }
      else
      {
        bskel = qskel;        
      }

      //printf( "bskel:\n" );
      //bskel.Print();


      //printf( "bskel.row() %lu bskel.col() %lu Z.row() %lu\n",
      //    bskel.row(), bskel.col(), Z.row() );


      /** inv( L ) * b */
      if ( 1 )
      {
        xtrsm( "Left", "Lower", "No transpose", "Non-unit", 
            bskel.row(), bskel.col(), 
            1.0, Z.data(), Z.row(), bskel.data(), bskel.row() );
      }
      else
      {
        xlaswp( bskel.col(), bskel.data(), bskel.row(), 
            1, bskel.row(), ipiv.data(), 1 );
        xtrsm( "Left", "Lower", "No transpose", "Unit", 
            bskel.row(), bskel.col(), 
            1.0, Z.data(), Z.row(), bskel.data(), bskel.row() );
      }





      if ( !isroot )
      {
        assert( Q.row() == bskel.row() );
        //x.Print();
        /** x = Q' * bskel */
        xgemm( "Transpose", "Non-transpose",
            x.row(), x.col(), Q.row(), 
            1.0, Q.data(), Q.row(),
            bskel.data(), bskel.row(),
            0.0, x.data(), x.ld() );

        //printf( "x:\n" );
        //for ( size_t j = 0; j < x.col(); j ++ )
        //{
        //  for ( size_t i = 0; i < x.row(); i ++ )
        //  {
        //    printf( "%3.1E ", x( i, j ) );
        //  }
        //  printf( "\n" );
        //}
        //printf( "\n" );
      }
    }; /** end ULVForward() */


    void ULVBackwardSolve( hmlp::View<T> &x )
    {
      /** get the view of right hand sides */
      auto &b = bview;

      if ( !isroot )
      {

        //printf( "x:\n" );
        //for ( size_t j = 0; j < x.col(); j ++ )
        //{
        //  for ( size_t i = 0; i < x.row(); i ++ )
        //  {
        //    printf( "%3.1E ", x( i, j ) );
        //  }
        //  printf( "\n" );
        //}
        //printf( "\n" );

        /** bskel += Q * x */
        xgemm( "Non-transpose", "Non-transpose",
            bskel.row(), bskel.col(), Q.col(), 
            1.0, Q.data(), Q.row(),
            x.data(), x.ld(),
            1.0, bskel.data(), bskel.row() );
        //printf( "bskel += Qx\n" );
      }

      //printf( "bskel\n" );
      //bskel.Print();

      if ( 1 )
      {
        /** inv( L' ) * bskel */
        xtrsm( "Left", "Lower", "Transpose", "Non-unit", 
            bskel.row(), bskel.col(), 
            1.0, Z.data(), Z.row(), bskel.data(), bskel.row() );
      }
      else
      {
        /** inv( U ) * bskel */
        xtrsm( "Left", "Upper", "No transpose", "Non-unit", 
            bskel.row(), bskel.col(), 
            1.0, Z.data(), Z.row(), bskel.data(), bskel.row() );
      }

      if ( isleaf )
      {
        /** return */
        for ( size_t j = 0; j < b.col(); j ++ )
          for ( size_t i = 0; i < b.row(); i ++ )
            b( i, j ) = bskel( i, j );
      }
      else
      {
        for ( size_t j = 0; j < bskel.col(); j ++ )
          for ( size_t i = 0; i < bskel.row(); i ++ )
            bskel( i, j ) -= qskel( i, j );
        //printf( "bskel -= qskel\n" );
        //bskel.Print();
        //printf( "qskel\n" );
        //qskel.Print();
      }
    }; /** end ULVBackward() */

    bool isleft = false;

    bool isleaf = false;

    bool isroot = false;

    size_t n;

    size_t nl;

    size_t nr;

    size_t s;

    size_t sl;

    size_t sr;

    /** Reduced system Z = [ I  VU   if ( HODLR || p-HSS )
     *                       VU  I ] */
    hmlp::Data<T> Z;

    /** pivoting rows (used in SMW) */
    std::vector<int> ipiv;
    
    /** U, n-by-s (SMW) or (sl+sr)-by-s (ULV) */
    hmlp::Data<T> U;

    /** V, n-by-s (SMW) or 0-by-0 (ULV) */
    hmlp::Data<T> V; 

    /** Crl, sr-by-sl */
    hmlp::Data<T> Crl;

    /** Clr, sl-by-sr or 0-by-0 (Symmetric) */
    hmlp::Data<T> Clr;

    /** a correspinding view of the right hand side of this node */
    hmlp::View<T> bview;

    /** pointers to children's factors */
    hmlp::Data<T> *Ul = NULL;
    hmlp::Data<T> *Ur = NULL;
    hmlp::Data<T> *Vl = NULL;
    hmlp::Data<T> *Vr = NULL;


    /** ULV specific */

    /** Q, (sl+sr)-by-s (ULV) */
    hmlp::Data<T> Q;

    /** tau, sl+sr (used in xgeqrf( U ) of ULV) */
    hmlp::Data<T> tau;

    /** (sl+sr)-by-nrhs, qskel = Q' * b */     
    hmlp::Data<T> qskel;
    hmlp::View<T> qview_myself;
    hmlp::View<T> qview_parent; 

    /** (sl+sr)-by-nrhs, bskel = inv( L ) * qskel */
    hmlp::Data<T> bskel;
    hmlp::View<T> bskel_myself;
    hmlp::View<T> bskel_parent;


  private: /** this class will be public inherit by gofmm::Data<T> */

    bool issymmetric = true;

    bool do_ulv_factorization = false;

}; /** end class Factor */


/**
 *  @brief 
 */ 
template<typename NODE, typename T>
void SetupFactor( NODE *node )
{
  size_t n, nl, nr, s, sl, sr;
  bool issymmetric, do_ulv_factorization, isleft;
  

#ifdef DEBUG_IGOFMM
  printf( "begin SetupFactor %lu\n", node->treelist_id ); fflush( stdout );
#endif

  issymmetric = node->setup->issymmetric;
  do_ulv_factorization = node->setup->do_ulv_factorization;
  n  = node->n;
  nl = 0;
  nr = 0;
  s  = node->data.skels.size();
  sl = 0;
  sr = 0;

  if ( !node->isleaf )
  {
    nl = node->lchild->n;
    nr = node->rchild->n;
    sl = node->lchild->data.skels.size();
    sr = node->rchild->data.skels.size();
  }

  isleft = false;
  if ( node->parent )
  {
    if ( node == node->parent->lchild ) isleft = true;
  }

  node->data.SetupFactor
  (
    issymmetric, do_ulv_factorization,
    isleft, node->isleaf, !node->l,
    n, nl, nr,
    s, sl, sr 
  );

#ifdef DEBUG_IGOFMM
  printf( "end SetupFactor %lu\n", node->treelist_id ); fflush( stdout );
#endif

}; /** end void SetupFactor() */


/**
 *  @brief
 */ 
template<typename NODE, typename T>
class SetupFactorTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "sf" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //printf( "Set treelist_id %lu\n", arg->treelist_id ); fflush( stdout );
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    void DependencyAnalysis()
    {
      /** remove all previous read/write records */
      arg->DependencyCleanUp();
      arg->DependencyAnalysis( hmlp::ReadWriteType::W, this );

      //if ( !arg->isleaf )
      //{
      //  arg->lchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      //  arg->rchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      //}
      //else
      //{
        this->Enqueue();
      //}
    };

    void Execute( Worker* user_worker )
    {
      SetupFactor<NODE, T>( arg );
    };

}; /** end class SetupFactorTask */



/** 
 *  @brief This task creates an hierarchical tree view for a matrix.
 */
template<typename NODE>
class TreeViewTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      name = std::string( "TreeView" );
      arg = user_arg;
      cost = 1.0;
      ss << arg->treelist_id;
      label = ss.str();
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    /** preorder dependencies (with a single source node) */
    void DependencyAnalysis()
    {
      if ( !arg->parent ) this->Enqueue();
      /** clean up dependencies */
      arg->DependencyCleanUp();
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      if ( arg->parent )
        arg->parent->DependencyAnalysis( hmlp::ReadWriteType::R, this );
    };

    void Execute( Worker* user_worker )
    {
      //printf( "TreeView %lu\n", node->treelist_id );
      auto *node   = arg;
      auto &data   = node->data;
      auto *setup  = node->setup;
      auto &input  = *(setup->input);
      auto &output = *(setup->output);

      /** create contigious view for output at root level */
      if ( !node->parent ) 
      {
        data.bview.Set( output );
      }

      /** tree view (hierarchical views) */
      if ( !node->isleaf )
      {
        /** A = [ A1; A2; ] */
        data.bview.Partition2x1
        ( 
          node->lchild->data.bview, 
          node->rchild->data.bview, node->lchild->n, TOP 
        );

        /** ULV specific initialization */
        if ( setup->do_ulv_factorization )
        {
          /** initialize qskel */
          data.qskel.resize( data.sl + data.sr, input.col() );
          /** create matrix view for qsekl */
          data.qview_myself.Set( data.qskel );
          /** [ lqskel; rqskel; ] = qskel */
          data.qview_myself.Partition2x1
          ( 
            node->lchild->data.qview_parent,
            node->rchild->data.qview_parent, data.sl, TOP
          );
        }
      }

      //printf( "end TreeView %lu\n", node->treelist_id );
    };

}; /** end class TreeViewTask */



/**
 *  @brief doward traversal to create matrix views, at the leaf
 *         level execute explicit permutation.
 */ 
template<bool FORWARD, typename NODE>
class MatrixPermuteTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      name = std::string( "MatrixPermutation" );
      arg = user_arg;
      cost = 1.0;
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    /** depends on previous task */
    void DependencyAnalysis()
    {
      if ( FORWARD )
      {
        arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      }
      else
      {
        this->Enqueue();
      }
    };

    void Execute( Worker* user_worker )
    {
      //printf( "PermuteMatrix %lu\n", arg->treelist_id );
      auto *node   = arg;
      auto &gids   = node->gids;
      auto &input  = *(node->setup->input);
      auto &output = *(node->setup->output);
      auto &A      = node->data.bview;

      assert( A.row() == gids.size() );
      assert( A.col() == input.col() );

      //for ( size_t i = 0; i < gids.size(); i ++ )
      //  printf( "%lu ", gids[ i ] );
      //printf( "\n" );

      /** perform permutation and output */
      for ( size_t j = 0; j < input.col(); j ++ )
        for ( size_t i = 0; i < gids.size(); i ++ )
          /** foward  permutation */
          if ( FORWARD ) A( i, j ) = input( gids[ i ], j );
          /** inverse permutation */
          else           input( gids[ i ], j ) = A( i, j );

      //for ( size_t j = 0; j < 1; j ++ )
      //  for ( size_t i = 0; i < gids.size(); i ++ )
      //    printf( "%E ", A( i, j ) );
      //printf( "\n" );

      //printf( "end PermuteMatrix %lu\n", arg->treelist_id );
    };

}; /** end class MatrixPermuteTask */



/**
 *  @brief
 */ 
template<typename NODE, typename T>
void Apply( NODE *node )
{
  auto &data = node->data;
  auto &setup = node->setup;
  auto &K = *setup->K;

  if ( node->isleaf )
  {
    auto lambda = setup->lambda;
    auto &amap = node->lids;
    /** evaluate the diagonal block */
    auto Kaa = K( amap, amap );
    /** apply the regularization */
    for ( size_t i = 0; i < Kaa.row(); i ++ ) 
      Kaa[ i * Kaa.row() + i ] += lambda;
    /** LU factorization */
    //data.Apply<true>( Kaa );
  }
  else
  {
    auto &bl = node->lchild->data.bview;
    auto &br = node->rchild->data.bview;
    data.Apply<true>( bl, br );
  }
}; /** end Apply() */



template<typename NODE, typename T>
void ULVForwardSolve( NODE *node )
{
  auto &data = node->data;
  auto *setup = node->setup;
  data.ULVForwardSolve( data.qview_parent );
};



template<typename NODE, typename T>
class ULVForwardSolveTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "ulvforward" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //printf( "Set treelist_id %lu\n", arg->treelist_id ); fflush( stdout );
    };

    void DependencyAnalysis()
    {      
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      /** depend on two children */
      if ( !arg->isleaf )
      {
        arg->lchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
        arg->rchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
      /** dispatch the task if there is no dependency */
      this->TryEnqueue();
    };

    void Execute( Worker* user_worker )
    {
      //printf( "ULVForwardSolveTask %lu\n", arg->treelist_id ); fflush( stdout );
      ULVForwardSolve<NODE, T>( arg );
      //printf( "end ULVForwardSolveTask %lu\n", arg->treelist_id ); fflush( stdout );
    };
    
}; /** end class ULVForwardSolveTask */




template<typename NODE, typename T>
void ULVBackwardSolve( NODE *node )
{
  auto &data = node->data;
  auto *setup = node->setup;

  data.bskel_myself.Set( data.bskel );

  if ( !node->isleaf )
  {
    data.bskel_myself.Partition2x1
    ( 
      node->lchild->data.bskel_parent, 
      node->rchild->data.bskel_parent, data.sl, TOP 
    );
  };

  data.ULVBackwardSolve( data.bskel_parent );

}; /** end ULVBackwardSolve() */




template<typename NODE, typename T>
class ULVBackwardSolveTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "ulvbackward" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //printf( "Set treelist_id %lu\n", arg->treelist_id ); fflush( stdout );
    };

    void DependencyAnalysis()
    {
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      /** depend on parent */
      if ( arg->parent )
        arg->parent->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      /** dispatch the task if there is no dependency */
      this->TryEnqueue();
    };

    void Execute( Worker* user_worker )
    {
      //printf( "ULVBackwardSolveTask %lu\n", arg->treelist_id ); fflush( stdout );
      ULVBackwardSolve<NODE, T>( arg );
      //printf( "end ULVBackwardSolveTask %lu\n", arg->treelist_id ); fflush( stdout );
    };
    
}; /** end class ULVBackwardSolveTask */


















/**
 *  @brief 
 */ 
template<bool TRANS, typename NODE, typename T>
void Solve( NODE *node )
{
  
  auto &data = node->data;
  auto &setup = node->setup;
  auto &K = *setup->K;


  //printf( "%lu beg Solve\n", node->treelist_id ); fflush( stdout );

  /** TODO: need to decide to use LU or not */
  if ( node->isleaf )
  {
    auto &b = data.bview;
    data.Solve<TRANS>( b );
    //printf( "Solve %lu, m %lu n %lu\n", node->treelist_id, b.row(), b.col() );
  }
  else
  {
    auto &bl = node->lchild->data.bview;
    auto &br = node->rchild->data.bview;
    data.Solve<TRANS, true>( bl, br );
    //printf( "Solve %lu, m %lu n %lu\n", node->treelist_id, bl.row(), bl.col() );
  }

  //printf( "%lu end Solve\n", node->treelist_id ); fflush( stdout );

}; /** end Solve() */


/**
 *  @brief
 */ 
template<bool TRANS, typename NODE, typename T>
class SolveTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "sl" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //printf( "Set treelist_id %lu\n", arg->treelist_id ); fflush( stdout );
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    void DependencyAnalysis()
    {
      if ( TRANS )
      {
        arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
        if ( arg->parent )
          arg->parent->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
      else
      {
        arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
        if ( !arg->isleaf )
        {
          arg->lchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
          arg->rchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
        }
        //else
        //{
        //  this->Enqueue();
        //}
      }
    };

    void Execute( Worker* user_worker )
    {
      Solve<TRANS, NODE, T>( arg );
    };

}; /** end class SolveTask */


/**
 *
 */ 
template<typename NODE, typename T, typename TREE>
void Solve( TREE &tree, hmlp::Data<T> &input )
{
  const bool AUTO_DEPENDENCY = true;
  const bool USE_RUNTIME     = true;

  /** copy input to output */
  auto *output = new hmlp::Data<T>( input.row(), input.col() );

  TreeViewTask<NODE>             treeviewtask;
  MatrixPermuteTask<true,  NODE> forwardpermutetask;
  MatrixPermuteTask<false, NODE> inversepermutetask;
  /** Sherman-Morrison-Woodbury */
  SolveTask<false, NODE, T>      solvetask1;
  /** ULV */
  ULVForwardSolveTask<NODE, T>   ulvforwardsolvetask;
  ULVBackwardSolveTask<NODE, T>  ulvbackwardsolvetask;

  /** attach the pointer to the tree structure */
  tree.setup.input  = &input;
  tree.setup.output = output;

  if ( tree.setup.do_ulv_factorization )
  {
    tree.template TraverseDown <AUTO_DEPENDENCY, USE_RUNTIME>( treeviewtask );
    tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( forwardpermutetask );
    tree.template TraverseUp   <AUTO_DEPENDENCY, USE_RUNTIME>( ulvforwardsolvetask );
    tree.template TraverseDown <AUTO_DEPENDENCY, USE_RUNTIME>( ulvbackwardsolvetask );
    if ( USE_RUNTIME ) hmlp_run();
    tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( inversepermutetask );
    if ( USE_RUNTIME ) hmlp_run();
  }
  else
  {
    tree.template TraverseDown <AUTO_DEPENDENCY, USE_RUNTIME>( treeviewtask );
    tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( forwardpermutetask );
    tree.template TraverseUp   <AUTO_DEPENDENCY, USE_RUNTIME>( solvetask1 );
    if ( USE_RUNTIME ) hmlp_run();
    tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( inversepermutetask );
    if ( USE_RUNTIME ) hmlp_run();
  }

  /** delete buffer space */
  delete output;

}; /** end Solve() */





/**
 *  @brief Compute relative Forbenius error for two-sided 
 *  interpolative decomposition.
 */ 
template<typename NODE, typename T>
void LowRankError( NODE *node )
{
  auto &data = node->data;
  auto &setup = node->setup;
  auto &K = *setup->K;

  if ( !node->isleaf )
  {
    auto Krl = K( node->rchild->gids, node->lchild->gids );

    auto nrm2 = hmlp_norm( Krl.row(),  Krl.col(), 
                           Krl.data(), Krl.row() ); 


    hmlp::Data<T> VrCrl( data.nr, data.sl );

    /** VrCrl = Vr * Crl */
    xgemm( "N", "N", data.nr, data.sl, data.sr,
        1.0, data.Vr->data(), data.nr,
             data.Crl.data(), data.sr,
        0.0, VrCrl.data(), data.nr );

    /** Krl - VrCrlVl' */
    xgemm( "N", "T", data.nr, data.nl, data.sl,
       -1.0, VrCrl.data(), data.nr,
             data.Vl->data(), data.nl,
        1.0, Krl.data(), data.nr );

    auto err = hmlp_norm( Krl.row(),  Krl.col(), 
                          Krl.data(), Krl.row() ); 

    printf( "%4lu ||Krl -VrCrlVl|| %3.1E\n", 
        node->treelist_id, std::sqrt( err / nrm2 ) );
  }

}; /** end LowRankError() */



/**
 *  @brief Factorizarion using LU and SMW
 */ 
template<typename NODE, typename T>
void Factorize( NODE *node )
{
  auto &data = node->data;
  auto &setup = node->setup;
  auto &K = *setup->K;
  auto &proj = data.proj;

  auto do_ulv_factorization = setup->do_ulv_factorization;

  if ( node->isleaf )
  {
    auto lambda = setup->lambda;
    auto &amap = node->lids;

    /** evaluate the diagonal block */
    hmlp::Data<T> Kaa = K( amap, amap );

    /** apply the regularization */
    for ( size_t j = 0; j < Kaa.col(); j ++ )
    {
      for ( size_t i = 0; i < Kaa.row(); i ++ )
      {
        //assert( Kaa( i, j ) == Kaa( j, i ) );
        if ( i == j ) Kaa( i, j ) += lambda;
      }
    }

    /** LU factorization */
    data.Factorize( Kaa );


    if ( do_ulv_factorization )
    {
      /** U = inv( L ) * proj' */
      data.Telescope( true, data.U, proj );
      /** QR factorization */
      data.Orthogonalization();
    }
    else
    {
      /** U = inv( Kaa ) * proj' */
      data.Telescope( true, data.U, proj );
      /** V = proj' */
      data.Telescope( false, data.V, proj );
    }

  }
  else
  {
    auto &Ul = node->lchild->data.U;
    auto &Vl = node->lchild->data.V;
    auto &Ur = node->rchild->data.U;
    auto &Vr = node->rchild->data.V;

    /** evluate the skeleton rows and columns */
    auto &amap = node->lchild->data.skels;
    auto &bmap = node->rchild->data.skels;

    /** get the skeleton rows and columns */
    node->data.Crl = K( bmap, amap );
    //printf( "end get Crl\n" ); fflush( stdout );

    /** SMW factorization (LU or Cholesky) */
    data.Factorize<true>( Ul, Ur, Vl, Vr );

    /** telescope U and V */
    if ( !node->data.isroot )
    {
      if ( do_ulv_factorization )
      {
        data.Telescope( true, data.U, proj, Ul, Ur );
        data.Orthogonalization();
      }
      else
      {
        /** U = inv( I + UCV' ) * [ Ul; Ur ] * proj' */
        data.Telescope( true, data.U, proj, Ul, Ur );

        /** V = [ Vl; Vr ] * proj' */
        data.Telescope( false, data.V, proj, Vl, Vr );
      }
    }
    else
    {
      /** output Crl from children */
      
      //size_t L = 3;

      auto *cl = node->lchild;
      auto *cr = node->rchild;
      auto *c1 = cl->lchild;
      auto *c2 = cl->rchild;
      auto *c3 = cr->lchild;
      auto *c4 = cr->rchild;

      //hmlp::Data<T> C21 = K( c2->data.skels, c1->data.skels );
      //hmlp::Data<T> C31 = K( c3->data.skels, c1->data.skels );
      //hmlp::Data<T> C41 = K( c4->data.skels, c1->data.skels );
      //hmlp::Data<T> C32 = K( c3->data.skels, c2->data.skels );
      //hmlp::Data<T> C42 = K( c4->data.skels, c2->data.skels );
      //hmlp::Data<T> C43 = K( c4->data.skels, c3->data.skels );

      //C21.WriteFile( "C21.m" );
      //C31.WriteFile( "C31.m" );
      //C41.WriteFile( "C41.m" );
      //C32.WriteFile( "C32.m" );
      //C42.WriteFile( "C42.m" );
      //C43.WriteFile( "C43.m" );


      //hmlp::Data<T> V11( c1->data.V.col(), c1->data.V.col() );
      //hmlp::Data<T> V22( c2->data.V.col(), c2->data.V.col() );
      //hmlp::Data<T> V33( c3->data.V.col(), c3->data.V.col() );
      //hmlp::Data<T> V44( c4->data.V.col(), c4->data.V.col() );

      //xgemm( "T", "N", c1->data.V.col(), c1->data.V.col(), c1->data.V.row(),
      //    1.0, c1->data.V.data(), c1->data.V.row(),
      //         c1->data.V.data(), c1->data.V.row(), 
      //    0.0,        V11.data(), V11.row() );

      //xgemm( "T", "N", c2->data.V.col(), c2->data.V.col(), c2->data.V.row(),
      //    1.0, c2->data.V.data(), c2->data.V.row(),
      //         c2->data.V.data(), c2->data.V.row(), 
      //    0.0,        V22.data(), V22.row() );

      //xgemm( "T", "N", c3->data.V.col(), c3->data.V.col(), c3->data.V.row(),
      //    1.0, c3->data.V.data(), c3->data.V.row(),
      //         c3->data.V.data(), c3->data.V.row(), 
      //    0.0,        V33.data(), V33.row() );

      //xgemm( "T", "N", c4->data.V.col(), c4->data.V.col(), c4->data.V.row(),
      //    1.0, c4->data.V.data(), c4->data.V.row(),
      //         c4->data.V.data(), c4->data.V.row(), 
      //    0.0,        V44.data(), V44.row() );

      //V11.WriteFile( "V11.m" );
      //V22.WriteFile( "V22.m" );
      //V33.WriteFile( "V33.m" );
      //V44.WriteFile( "V44.m" );
    }
    //printf( "end inner forward telescoping\n" ); fflush( stdout );

    /** check the offdiagonal block VrCrlVl' accuracy */
    if ( !do_ulv_factorization ) 
      LowRankError<NODE, T>( node );
  }

}; /** end void Factorize() */



/**
 *  @brief
 */ 
template<typename NODE, typename T>
class FactorizeTask : public hmlp::Task
{
  public:

    NODE *arg;

    bool do_ulv_factorization = false;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "fa" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //printf( "Set treelist_id %lu\n", arg->treelist_id ); fflush( stdout );
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    void DependencyAnalysis()
    {
      arg->DependencyCleanUp();
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      if ( !arg->isleaf )
      {
        arg->lchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
        arg->rchild->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
      else
      {
        this->Enqueue();
      }
    };

    void Execute( Worker* user_worker )
    { 
      //printf( "%lu Enter Factorize\n", arg->treelist_id );
      Factorize<NODE, T>( arg );
      //printf( "%lu Exit  Factorize\n", arg->treelist_id );
    };

}; /** end class FactorizeTask */





/**
 *  @biref Top level factorization routine.
 */ 
template<typename NODE, typename T, typename TREE>
void Factorize( bool do_ulv_factorization, TREE &tree, T lambda )
{
  const bool AUTO_DEPENDENCY = true;
  const bool USE_RUNTIME = true;

  /** all task instances */
  SetupFactorTask<NODE, T> setupfactortask; 
  FactorizeTask<NODE, T> factorizetask; 

  /** setup the regularization parameter lambda */
  tree.setup.lambda = lambda;

  /** setup the symmetric type */
  tree.setup.issymmetric = true;

  /** setup factorization type */
  tree.setup.do_ulv_factorization = do_ulv_factorization;

  /** setup  */
  tree.template TraverseUp<AUTO_DEPENDENCY, USE_RUNTIME>( setupfactortask );
  if ( USE_RUNTIME ) hmlp_run();
  //printf( "Execute setupfactortask\n" ); fflush( stdout );

  /** factorization */
  tree.template TraverseUp<AUTO_DEPENDENCY, USE_RUNTIME>( factorizetask );
  //printf( "Create factorizetask\n" ); fflush( stdout );
  if ( USE_RUNTIME ) hmlp_run();
  //printf( "Execute factorizetask\n" ); fflush( stdout );

}; /** end Factorize() */



/**
 *  @brief Compute the average 2-norm error. That is given
 *         lambda and weights, 
 */ 
template<typename NODE, typename TREE, typename T>
void ComputeError( TREE &tree, T lambda, 
    hmlp::Data<T> weights, hmlp::Data<T> potentials )
{
  /** assure the dimension matches */
  assert( weights.row() == potentials.row() );
  assert( weights.col() == potentials.col() );

  size_t nrhs = weights.row();
  size_t n = weights.col();

  /** shift lambda and make it a column vector */
  hmlp::Data<T> rhs( n, nrhs );
  for ( size_t j = 0; j < nrhs; j ++ )
    for ( size_t i = 0; i < n; i ++ )
      rhs( i, j ) = potentials( j, i ) + lambda * weights( j, i );

  /** potentials = inv( K + lambda * I ) * potentials */
  hmlp::hfamily::Solve<NODE, T>( tree, rhs );


  /** compute relative error = sqrt( err / nrm2 ) for each rhs */
  printf( "========================================================\n" );
  printf( "Inverse accuracy report\n" );
  printf( "========================================================\n" );
  printf( "#rhs,  max err,        @,  min err,        @,  relative \n" );
  printf( "========================================================\n" );
  size_t ntest = 10;
  T total_err  = 0.0;
  for ( size_t j = 0; j < std::min( nrhs, ntest ); j ++ )
  {
    /** counters */
    T nrm2 = 0.0, err2 = 0.0;
    T max2 = 0.0, min2 = std::numeric_limits<T>::max(); 
    /** indecies */
    size_t maxi = 0, mini = 0;

    for ( size_t i = 0; i < n; i ++ )
    {
      T sse = rhs( i, j ) - weights( j, i );
      assert( rhs( i, j ) == rhs( i, j ) );
      sse = sse * sse;

      nrm2 += weights( j, i ) * weights( j, i );
      err2 += sse;

      //printf( "%lu %3.1E\n", i, sse );


      if ( sse > max2 ) { max2 = sse; maxi = i; }
      if ( sse < min2 ) { min2 = sse; mini = i; }
    }
    total_err += std::sqrt( err2 / nrm2 );

    printf( "%4lu,  %3.1E,  %7lu,  %3.1E,  %7lu,   %3.1E\n", 
        j, std::sqrt( max2 ), maxi, std::sqrt( min2 ), mini, 
        std::sqrt( err2 / nrm2 ) );
  }
  printf( "========================================================\n" );
  printf( "                             avg over %2lu rhs,   %3.1E \n",
      std::min( nrhs, ntest ), total_err / std::min( nrhs, ntest ) );
  printf( "========================================================\n\n" );

}; /** end ComputeError() */








}; // end namespace hfamily
}; // end namespace hmlp

#endif // define HFAMILY_HPP