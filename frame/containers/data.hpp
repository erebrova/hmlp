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





#ifndef DATA_HPP
#define DATA_HPP

/** mmap */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/** stl */
#include <cassert>
#include <typeinfo>

#include <algorithm>
#include <random>

#include <set>
#include <map>
#include <vector>
#include <deque>
#include <map>
#include <string>

/** std::istringstream */
#include <iostream>
#include <fstream>
#include <sstream>

/** hmlp */
#include <hmlp_device.hpp>
#include <hmlp_runtime.hpp>

/** -lmemkind */
#ifdef HMLP_MIC_AVX512
#include <hbwmalloc.h>
#include <hbw_allocator.h>
#endif // ifdef HMLP}_MIC_AVX512

/** gpu related */
#ifdef HMLP_USE_CUDA
#include <hmlp_gpu.hpp>
#include <thrust/tuple.h>
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/system/cuda/experimental/pinned_allocator.h>
#include <thrust/system_error.h>
#include <thrust/system/cuda/error.h>
template<class T>
class managed_allocator
{
  public:

    T* allocate( size_t n )
    {
      T* result = nullptr;

      cudaError_t error = cudaMallocManaged( 
          &result, n * sizeof(T), cudaMemAttachGlobal );

      if ( error != cudaSuccess )
      {
        throw thrust::system_error( error, thrust::cuda_category(), 
            "managed_allocator::allocate(): cudaMallocManaged" );
      }

      return result;
    }; /** end allocate() */

    void deallocate( T* ptr, size_t )
    {
      cudaError_t error = cudaFree( ptr );

      if ( error != cudaSuccess )
      {
        throw thrust::system_error( error, thrust::cuda_category(), 
            "managed_allocator::deallocate(): cudaFree" );
      }
    }; /** end deallocate() */
};
#endif // ifdef HMLP_USE_CUDA

/** debug flag */
#define DEBUG_DATA 1



namespace hmlp
{

#ifdef HMLP_MIC_AVX512
/** use hbw::allocator for Intel Xeon Phi */
template<class T, class Allocator = hbw::allocator<T> >
#elif  HMLP_USE_CUDA
/** use pinned (page-lock) memory for NVIDIA GPUs */
template<class T, class Allocator = thrust::system::cuda::experimental::pinned_allocator<T> >
#else
/** use default stl allocator */
template<class T, class Allocator = std::allocator<T> >
#endif
class Data : public ReadWrite, public std::vector<T, Allocator>
#ifdef HMLP_USE_CUDA
/** inheritate the interface fot the host-device model. */
, public hmlp::DeviceMemory<T>
#endif
{
  public:

    /** the default constructor */
    Data() : m( 0 ), n( 0 ) {};

    Data( std::size_t m, std::size_t n ) 
      : std::vector<T, Allocator>( m * n )
    { 
      this->m = m;
      this->n = n;
    };

    Data( std::size_t m, std::size_t n, T initT ) 
      : std::vector<T, Allocator>( m * n, initT )
    { 
      this->m = m;
      this->n = n;
    };

    Data( std::size_t m, std::size_t n, std::string &filename ) 
      : std::vector<T, Allocator>( m * n )
    {
      this->m = m;
      this->n = n;
      this->read( m, n, filename );

      //std::cout << filename << std::endl;

      //std::ifstream file( filename.data(), std::ios::in|std::ios::binary|std::ios::ate );
      //if ( file.is_open() )
      //{
      //  auto size = file.tellg();
      //  assert( size == m * n * sizeof(T) );
      //  file.seekg( 0, std::ios::beg );
      //  file.read( (char*)this->data(), size );
      //  file.close();
      //}
    };

    void resize( std::size_t m, std::size_t n )
    { 
      this->m = m;
      this->n = n;
      std::vector<T, Allocator>::resize( m * n );
    };

    void resize( std::size_t m, std::size_t n, T initT )
    {
      this->m = m;
      this->n = n;
      std::vector<T, Allocator>::resize( m * n, initT );
    };

    void reserve( std::size_t m, std::size_t n ) 
    {
      std::vector<T, Allocator>::reserve( m * n );
    };

    void read( std::size_t m, std::size_t n, std::string &filename )
    {
      assert( this->m == m );
      assert( this->n == n );
      assert( this->size() == m * n );

      std::cout << filename << std::endl;

      std::ifstream file( filename.data(), std::ios::in|std::ios::binary|std::ios::ate );
      if ( file.is_open() )
      {
        auto size = file.tellg();
        assert( size == m * n * sizeof(T) );
        file.seekg( 0, std::ios::beg );
        file.read( (char*)this->data(), size );
        file.close();
      }
    };

    std::tuple<size_t, size_t> shape()
    {
      return std::make_tuple( m, n );
    };

		template<typename TINDEX>
		T getvalue( TINDEX i )
		{
			return (*this)[ i ];
		};

		template<typename TINDEX>
		T getvalue( TINDEX i, TINDEX j )
		{
			return (*this)[ m * j + i ];
		};

		template<typename TINDEX>
		void setvalue( TINDEX i, T v )
		{
			(*this)[ i ] = v;
		};

		template<typename TINDEX>
		void setvalue( TINDEX i, TINDEX j, T v )
		{
			(*this)[ m * j + i ] = v;
		};

    /** ESSENTIAL: return number of coumns */
    std::size_t row() { return m; };

    /** ESSENTIAL: return number of rows */
    std::size_t col() { return n; };

    /** ESSENTIAL: return an element */
    template<typename TINDEX>
    inline T & operator()( TINDEX i, TINDEX j )
    {
      return (*this)[ m * j + i ];
    };

    /** ESSENTIAL: return a submatrix */
    template<typename TINDEX>
    inline hmlp::Data<T> operator()( std::vector<TINDEX> &imap, std::vector<TINDEX> &jmap )
    {
      hmlp::Data<T> submatrix( imap.size(), jmap.size() );
      #pragma omp parallel for
      for ( int j = 0; j < jmap.size(); j ++ )
      {
        for ( int i = 0; i < imap.size(); i ++ )
        {
          submatrix[ j * imap.size() + i ] = (*this)[ m * jmap[ j ] + imap[ i ] ];
        }
      }

      return submatrix;
    }; 

    /** ESSENTIAL: */
    template<typename TINDEX>
    std::pair<T, TINDEX> ImportantSample( TINDEX j )
    {
      TINDEX i = std::rand() % m;
      std::pair<T, TINDEX> sample( (*this)( i, j ), i );
      return sample; 
    };


    template<typename TINDEX>
    inline hmlp::Data<T> operator()( std::vector<TINDEX> &jmap )
    {
      hmlp::Data<T> submatrix( m, jmap.size() );
      #pragma omp parallel for
      for ( int j = 0; j < jmap.size(); j ++ )
        for ( int i = 0; i < m; i ++ )
          submatrix[ j * m + i ] = (*this)[ m * jmap[ j ] + i ];
      return submatrix;
    };

    template<typename TINDEX>
    void GatherColumns( bool TRANS, std::vector<TINDEX> &jmap, hmlp::Data<T> &submatrix )
    {
      if ( TRANS )
      {
        submatrix.resize( jmap.size(), m );
        for ( int j = 0; j < jmap.size(); j ++ )
          for ( int i = 0; i < m; i ++ )
            submatrix[ i * jmap.size() + j ] = (*this)[ m * jmap[ j ] + i ];
      }
      else
      {
        submatrix.resize( m, jmap.size() );
        for ( int j = 0; j < jmap.size(); j ++ )
          for ( int i = 0; i < m; i ++ )
            submatrix[ j * m + i ] = (*this)[ m * jmap[ j ] + i ];
      }
    }; 


    template<bool SYMMETRIC = false>
    void rand( T a, T b )
    {
      std::default_random_engine generator;
      std::uniform_real_distribution<T> distribution( a, b );

      if ( SYMMETRIC ) assert( m == n );

      for ( std::size_t j = 0; j < n; j ++ )
      {
        for ( std::size_t i = 0; i < m; i ++ )
        {
          if ( SYMMETRIC )
          {
            if ( i > j )
              (*this)[ j * m + i ] = distribution( generator );
            else
              (*this)[ j * m + i ] = (*this)[ i * m + j ];
          }
          else
          {
            (*this)[ j * m + i ] = distribution( generator );
          }
        }
      }
    };

    template<bool SYMMETRIC = false>
    void rand() 
    { 
      rand<SYMMETRIC>( 0.0, 1.0 ); 
    };

    void randn( T mu, T sd )
    {
      std::default_random_engine generator;
      std::normal_distribution<T> distribution( mu, sd );
      for ( std::size_t i = 0; i < m * n; i ++ )
      {
        (*this)[ i ] = distribution( generator );
      }
    };

    void randn() 
    { 
      randn( 0.0, 1.0 ); 
    };

    template<bool USE_LOWRANK>
    void randspd( T a, T b )
    {
      std::default_random_engine generator;
      std::uniform_real_distribution<T> distribution( a, b );

      assert( m == n );

      if ( USE_LOWRANK )
      {
        hmlp::Data<T> X( ( std::rand() % n ) / 10 + 1, n );
        X.rand();
        xgemm
        (
          "T", "N",
          n, n, X.row(),
          1.0, X.data(), X.row(),
               X.data(), X.row(),
          0.0, this->data(), this->row()
        );
      }
      else /** diagonal dominating */
      {
        for ( std::size_t j = 0; j < n; j ++ )
        {
          for ( std::size_t i = 0; i < m; i ++ )
          {
            if ( i > j )
              (*this)[ j * m + i ] = distribution( generator );
            else
              (*this)[ j * m + i ] = (*this)[ i * m + j ];

            /** Make sure diagonal dominated */
            (*this)[ j * m + j ] += std::abs( (*this)[ j * m + i ] );
          }
        }
      }
    };

    void randspd() 
    { 
      randspd<true>( 0.0, 1.0 ); 
    };

    void Print()
    {
      printf( "Data in %lu * %lu\n", m, n );
      //hmlp::hmlp_printmatrix( m, n, this->data(), m );
    };

    void WriteFile( char *name )
    {
      FILE * pFile;
      pFile = fopen ( name, "w" );
      fprintf( pFile, "%s=[\n", name );
      for ( size_t i = 0; i < m; i ++ )
      {
        for ( size_t j = 0; j < n; j ++ )
        {
          fprintf( pFile, "%lf,", (*this)( i, j ) );
        }
        fprintf( pFile, ";\n" );
      }
      fprintf( pFile, "];\n" );
      fclose( pFile );
    };

    template<typename TINDEX>
    double flops( TINDEX na, TINDEX nb ) { return 0.0; };

#ifdef HMLP_USE_CUDA

    void CacheD( hmlp::Device *dev )
    {
      DeviceMemory<T>::CacheD( dev, this->size() * sizeof(T) );
    };

    void AllocateD( hmlp::Device *dev )
    {
      double beg = omp_get_wtime();
      DeviceMemory<T>::AllocateD( dev, m * n * sizeof(T) );
      double alloc_time = omp_get_wtime() - beg;
      printf( "AllocateD %5.3lf\n", alloc_time );
    };

    void FreeD( hmlp::Device *dev )
    {
      DeviceMemory<T>::FreeD( dev, this->size() * sizeof(T) );
    };

    void PrefetchH2D( hmlp::Device *dev, int stream_id )
    {
      double beg = omp_get_wtime();
      DeviceMemory<T>::PrefetchH2D
        ( dev, stream_id, m * n * sizeof(T), this->data() );
      double alloc_time = omp_get_wtime() - beg;
      //printf( "PrefetchH2D %5.3lf\n", alloc_time );
    };

    void PrefetchD2H( hmlp::Device *dev, int stream_id )
    {
      DeviceMemory<T>::PrefetchD2H
        ( dev, stream_id, m * n * sizeof(T), this->data() );
    };

    void WaitPrefetch( hmlp::Device *dev, int stream_id )
    {
      DeviceMemory<T>::Wait( dev, stream_id );
    };

    void FetchH2D( hmlp::Device *dev )
    {
      DeviceMemory<T>::FetchH2D( dev, m * n * sizeof(T), this->data() );
    };

    void FetchD2H( hmlp::Device *dev )
    {
      DeviceMemory<T>::FetchD2H( dev, m * n * sizeof(T), this->data() );
    };
#endif

  private:

    std::size_t m;

    std::size_t n;

}; // end class Data


#ifdef HMLP_MIC_AVX512
template<bool SYMMETRIC, typename T, class Allocator = hbw::allocator<T> >
#else
template<bool SYMMETRIC, typename T, class Allocator = std::allocator<T> >
#endif
class CSC : public ReadWrite
{
  public:

    template<typename TINDEX>
    CSC( TINDEX m, TINDEX n, TINDEX nnz )
    {
      this->m = m;
      this->n = n;
      this->nnz = nnz;
      this->val.resize( nnz, 0.0 );
      this->row_ind.resize( nnz, 0 );
      this->col_ptr.resize( n + 1, 0 );
    };

    // Construct from three arrays.
    // val[ nnz ]
    // row_ind[ nnz ]
    // col_ptr[ n + 1 ]
    // TODO: setup full_row_ind
    template<typename TINDEX>
    CSC( TINDEX m, TINDEX n, TINDEX nnz, T *val, TINDEX *row_ind, TINDEX *col_ptr ) 
    : CSC( m, n, nnz )
    {
      assert( val ); assert( row_ind ); assert( col_ptr );
      for ( size_t i = 0; i < nnz; i ++ )
      {
        this->val[ i ] = val[ i ];
        this->row_ind[ i ] = row_ind[ i ];
      }
      for ( size_t j = 0; j < n + 1; j ++ )
      {
        this->col_ptr[ j ] = col_ptr[ j ];
      }
    };

    ~CSC() {};

    template<typename TINDEX>
    inline T operator()( TINDEX i, TINDEX j )
    {
      if ( SYMMETRIC && ( i < j  ) ) std::swap( i, j );
      size_t row_beg = col_ptr[ j ];
      size_t row_end = col_ptr[ j + 1 ];
      auto lower = std::lower_bound
                   ( 
                     row_ind.begin() + row_beg, 
                     row_ind.begin() + row_end,
                     i
                   );
      if ( *lower == i )
      {
        return val[ std::distance( row_ind.begin(), lower ) ];
      }
      else
      {
        return 0.0;
      }
    };

    template<typename TINDEX>
    inline hmlp::Data<T> operator()( std::vector<TINDEX> &imap, std::vector<TINDEX> &jmap )
    {
      hmlp::Data<T> submatrix( imap.size(), jmap.size() );
      #pragma omp parallel for
      for ( int j = 0; j < jmap.size(); j ++ )
      {
        for ( int i = 0; i < imap.size(); i ++ )
        {
          submatrix[ j * imap.size() + i ] = (*this)( imap[ i ], jmap[ j ] );
        }
      }
      return submatrix;
    }; 

    template<typename TINDEX>
    std::size_t ColPtr( TINDEX j ) { return col_ptr[ j ]; };

    template<typename TINDEX>
    std::size_t RowInd( TINDEX offset ) { return row_ind[ offset ]; };

    template<typename TINDEX>
    T Value( TINDEX offset ) { return val[ offset ]; };

    template<typename TINDEX>
    std::pair<T, TINDEX> ImportantSample( TINDEX j )
    {
      size_t offset = col_ptr[ j ] + rand() % ( col_ptr[ j + 1 ] - col_ptr[ j ] );
      std::pair<T, TINDEX> sample( val[ offset ], row_ind[ offset ] );
      return sample; 
    };

    void Print()
    {
      for ( size_t j = 0; j < n; j ++ ) printf( "%8lu ", j );
      printf( "\n" );
      for ( size_t i = 0; i < m; i ++ )
      {
        for ( size_t j = 0; j < n; j ++ )
        {
          printf( "% 3.1E ", (*this)( i, j ) );
        }
        printf( "\n" );
      }
    }; // end Print()


    /** 
     *  @brief Read matrix market format (ijv) format. Only lower triangular
     *         part is stored
     */ 
    template<bool LOWERTRIANGULAR, bool ISZEROBASE, bool IJONLY = false>
    void readmtx( std::string &filename )
    {
      size_t m_mtx, n_mtx, nnz_mtx;

      std::vector<std::deque<size_t>> full_row_ind( n );
      std::vector<std::deque<T>> full_val( n );

      // Read all tuples.
      printf( "%s ", filename.data() ); fflush( stdout );
      std::ifstream file( filename.data() );
      std::string line;
      if ( file.is_open() )
      {
        size_t nnz_count = 0;

        while ( std::getline( file, line ) )
        {
          if ( line.size() )
          {
            if ( line[ 0 ] != '%' )
            {
              std::istringstream iss( line );
              iss >> m_mtx >> n_mtx >> nnz_mtx;
              assert( this->m == m_mtx );
              assert( this->n == n_mtx );
              assert( this->nnz == nnz_mtx );
              break;
            }
          }
        }

        while ( std::getline( file, line ) )
        {
          if ( nnz_count % ( nnz / 10 ) == 0 )
          {
            printf( "%lu%% ", ( nnz_count * 100 ) / nnz ); fflush( stdout );
          }

          std::istringstream iss( line );

					size_t i, j;
          T v;

          if ( IJONLY )
          {
            if ( !( iss >> i >> j ) )
            {
              printf( "line %lu has illegle format\n", nnz_count );
              break;
            }
            v = 1;
          }
          else
          {
            if ( !( iss >> i >> j >> v ) )
            {
              printf( "line %lu has illegle format\n", nnz_count );
              break;
            }
          }

          if ( !ISZEROBASE )
          {
            i -= 1;
            j -= 1;
          }

          if ( v != 0.0 )
          {
            full_row_ind[ j ].push_back( i );
            full_val[ j ].push_back( v );

            if ( !SYMMETRIC && LOWERTRIANGULAR && i > j  )
            {
              full_row_ind[ i ].push_back( j );
              full_val[ i ].push_back( v );
            }
          }
          nnz_count ++;
        }
        assert( nnz_count == nnz );
      }
      printf( "Done.\n" ); fflush( stdout );
      // Close the file.
      file.close();

      //printf( "Here nnz %lu\n", nnz );

      // Recount nnz for the full storage.
      size_t full_nnz = 0;
      for ( size_t j = 0; j < n; j ++ )
      {
        col_ptr[ j ] = full_nnz;
        full_nnz += full_row_ind[ j ].size();
      }
      nnz = full_nnz;
      col_ptr[ n ] = full_nnz;
      row_ind.resize( full_nnz );
      val.resize( full_nnz );

      //printf( "Here nnz %lu\n", nnz );

      //full_nnz = 0;
      //for ( size_t j = 0; j < n; j ++ )
      //{
      //  for ( size_t i = 0; i < full_row_ind[ j ].size(); i ++ )
      //  {
      //    row_ind[ full_nnz ] = full_row_ind[ j ][ i ];
      //    val[ full_nnz ] = full_val[ j ][ i ];
      //    full_nnz ++;
      //  }
      //}

      //printf( "Close the file. Reformat.\n" );

      #pragma omp parallel for
      for ( size_t j = 0; j < n; j ++ )
      {
        for ( size_t i = 0; i < full_row_ind[ j ].size(); i ++ )
        {
          row_ind[ col_ptr[ j ] + i ] = full_row_ind[ j ][ i ];
          val[ col_ptr[ j ] + i ] = full_val[ j ][ i ];
        }
      }

      printf( "finish readmatrix %s\n", filename.data() ); fflush( stdout );
    };


    std::size_t row() { return m; };

    std::size_t col() { return n; };

    template<typename TINDEX>
    double flops( TINDEX na, TINDEX nb ) { return 0.0; };

  private:

    std::size_t m;

    std::size_t n;

    std::size_t nnz;

    std::vector<T, Allocator> val;

    //std::vector<std::size_t, Allocator> row_ind;
    std::vector<std::size_t> row_ind;
   
    //std::vector<std::size_t, Allocator> col_ptr;
    std::vector<std::size_t> col_ptr;

}; // end class CSC



#ifdef HMLP_MIC_AVX512
template<class T, class Allocator = hbw::allocator<T> >
#else
template<class T, class Allocator = std::allocator<T> >
#endif
class OOC : public ReadWrite
{
  public:

    template<typename TINDEX>
    OOC( TINDEX m, TINDEX n, std::string filename ) :
      file( filename.data(), std::ios::in|std::ios::binary|std::ios::ate )
    {
      this->m = m;
      this->n = n;
      this->filename = filename;

      //if ( file.is_open() )
      //{
      //  auto size = file.tellg();
      //  assert( size == m * n * sizeof(T) );
      //}

      cache.resize( n );

      fd = open( filename.data(), O_RDONLY, 0 ); assert( fd != -1 );
//#ifdef __APPLE__
//      mmappedData = (T*)mmap( NULL, m * n * sizeof(T), PROT_READ, MAP_PRIVATE, fd, 0 );
//#else /** assume linux */
//      mmappedData = (T*)mmap( NULL, m * n * sizeof(T), PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0 );
//#endif
//
//      assert( mmappedData != MAP_FAILED );

      std::cout << filename << std::endl;
    };

    ~OOC()
    {
      //if ( file.is_open() ) file.close();
      /** unmap */
      //int rc = munmap( mmappedData, m * n * sizeof(T) );
      //assert( rc == 0 );
      close( fd );
      printf( "finish readmatrix %s\n", filename.data() );
    };


    template<typename TINDEX>
    inline T operator()( TINDEX i, TINDEX j )
    {
      T Kij;

      if ( !read_from_cache( i, j, &Kij ) )
      {
        if ( !read_from_disk( i, j, &Kij ) )
        {
          printf( "Accessing disk fail\n" );
          exit( 1 );
        }
      }

      return Kij;
    };

    template<typename TINDEX>
    inline hmlp::Data<T> operator()( std::vector<TINDEX> &imap, std::vector<TINDEX> &jmap )
    {
      hmlp::Data<T> submatrix( imap.size(), jmap.size() );
      #pragma omp parallel for
      for ( int j = 0; j < jmap.size(); j ++ )
      {
        for ( int i = 0; i < imap.size(); i ++ )
        {
          submatrix[ j * imap.size() + i ] = (*this)( imap[ i ], jmap[ j ] );
        }
      }
      return submatrix;
    }; 

    template<typename TINDEX>
    std::pair<T, TINDEX> ImportantSample( TINDEX j )
    {
      TINDEX i = std::rand() % m;
      std::pair<T, TINDEX> sample( (*this)( i, j ), i );
      return sample; 
    };

    std::size_t row() { return m; };

    std::size_t col() { return n; };

    template<typename TINDEX>
    double flops( TINDEX na, TINDEX nb ) { return 0.0; };

  private:

    template<typename TINDEX>
    bool read_from_cache( TINDEX i, TINDEX j, T *Kij )
    {
      // Need some method to search in the caahe.
      auto it = cache[ j ].find( i );

      if ( it != cache[ j ].end() ) 
      {
        *Kij = it->second;
        return true;
      }
      else return false;
    };

    /**
     *  @brief We need a lock here.
     */ 
    template<typename TINDEX>
    bool read_from_disk( TINDEX i, TINDEX j, T *Kij )
    {
      if ( file.is_open() )
      {
        //printf( "try %4lu, %4lu ", i, j );
        T K015j[ 16 ];
        size_t batch_size = 1;
        if ( i + batch_size >= m ) batch_size = 1;
        #pragma omp critical
        {
          file.seekg( ( j * m + i ) * sizeof(T) );
          file.read( (char*)K015j, batch_size * sizeof(T) );
        }
        *Kij = K015j[ 0 ];
        for ( size_t p = 0; p < batch_size; p ++ )
        {
          if ( cache[ j ].size() < 16384 )
          {
            #pragma omp critical
            {
              cache[ j ][ i + p ] = K015j[ p ];
            }
          }
        }
        // printf( "read %4lu, %4lu, %E\n", i, j, *Kij );
        // TODO: Need some method to cache the data.
        return true;
      }
      else
      {
        return false;
      }
    };

    std::size_t m;

    std::size_t n;

    std::string filename;

    std::ifstream file;

    // Use mmp
    T *mmappedData;

    /** */
    std::vector<std::map<size_t, T>> cache;

    int fd;

}; // end class OOC







}; /** end namespace hmlp */

#endif //define DATA_HPP
