#ifndef SPDASKIT_HPP
#define SPDASKIT_HPP

/** stl and omp */
#include <set>
#include <vector>
#include <map>
#include <unordered_set>
#include <deque>
#include <assert.h>
#include <typeinfo>
#include <algorithm>
#include <random>
#include <numeric>
#include <sstream>
#include <iostream>
#include <string>
#include <stdio.h>
#include <omp.h>

/** hmlp */
#include <hmlp.h>
#include <hmlp_blas_lapack.h>
#include <hmlp_util.hpp>
#include <hmlp_thread.hpp>
#include <hmlp_runtime.hpp>
#include <tree.hpp>
#include <skel.hpp>
#include <data.hpp>


/** gpu related */
#ifdef HMLP_USE_CUDA
#include <spdaskit_gpu.hpp>
#endif

//#define DEBUG_SPDASKIT 1

namespace hmlp
{
namespace spdaskit
{



/**
 *  @brief These are data that shared by the whole tree.
 *
 *  @TODO  Make w into a pointer.
 *
 */ 
template<typename SPDMATRIX, typename SPLITTER, typename T>
class Setup : public hmlp::tree::Setup<SPLITTER, T>
{
  public:

    /** humber of neighbors */
    size_t k;

    /** maximum rank */
    size_t s;

    /** relative error for rank-revealed QR */
    T stol;

    /** the SPDMATRIX (dense, CSC or OOC) */
    SPDMATRIX *K;

    /** rhs-by-n all weights */
    hmlp::Data<T> *w;

    /** rhs-by-n all potentials */
    hmlp::Data<T> *u;

}; // end class Setup


/**
 *  @brief This class contains all iaskit related data.
 */ 
template<typename T>
class Data
{
  public:

    Data() : kij_skel( 0.0, 0 ), kij_s2s( 0.0, 0 ), kij_s2n( 0.0, 0 ) {};

    Lock lock;

    /** whether the node can be compressed */
    bool isskel = false;

    /** whether the coefficient mathx has been computed */
    bool hasproj = false;

    /** my skeletons */
    std::vector<size_t> skels;

    /** (buffer) s-by-s upper trianguler for xtrsm( R11, proj ) */
    //hmlp::Data<T> R11; 

    /** 2s, pivoting order of GEQP3 */
    std::vector<int> jpvt;

    /** s-by-2s */
    hmlp::Data<T> proj;

    /** sampling neighbors ids */
    std::map<std::size_t, T> snids; 

    /* pruning neighbors ids */
    std::unordered_set<std::size_t> pnids; 

    /** skeleton weights and potentials */
    hmlp::Data<T> w_skel;
    hmlp::Data<T> u_skel;

    /** permuted weights and potentials (buffer) */
    hmlp::Data<T> w_leaf;
    hmlp::Data<T> u_leaf[ 5 ];

    /** cached Kab */
    hmlp::Data<T> NearKab;
    hmlp::Data<T> FarKab;

    /** Kij evaluation counter counters */
    std::pair<double, std::size_t> kij_skel;
    std::pair<double, std::size_t> kij_s2s;
    std::pair<double, std::size_t> kij_s2n;

    /** many timers */
    double merge_neighbors_time = 0.0;
    double id_time = 0.0;

    /** recorded events (for HMLP Runtime) */
    hmlp::Event skeletonize;
    hmlp::Event updateweight;
    hmlp::Event skeltoskel;
    hmlp::Event skeltonode;

    hmlp::Event s2s;
    hmlp::Event s2n;

	/** knn accuracy */
    double knn_acc = 0.0;
    size_t num_acc = 0;

}; // end class Data


/**
 *  @brief This class does not need to inherit hmlp::Data<T>, but it should
 *         support two interfaces for data fetching.
 */ 
template<typename T>
class SPDMatrix : public hmlp::Data<T>
{
  public:
  private:
}; // end class SPDMatrix


/**
 *
 */ 
template<typename NODE>
class Summary
{

  public:

    Summary() {};

    std::deque<hmlp::Statistic> rank;

    std::deque<hmlp::Statistic> merge_neighbors_time;

    std::deque<hmlp::Statistic> kij_skel;

    std::deque<hmlp::Statistic> kij_skel_time;

    std::deque<hmlp::Statistic> id_time;

    std::deque<hmlp::Statistic> skeletonize;

    /** n2s */
    std::deque<hmlp::Statistic> updateweight;

    /** s2s */
    std::deque<hmlp::Statistic> s2s_kij_t;
    std::deque<hmlp::Statistic> s2s_t;
    std::deque<hmlp::Statistic> s2s_gfp;

    /** s2n */
    std::deque<hmlp::Statistic> s2n_kij_t;
    std::deque<hmlp::Statistic> s2n_t;
    std::deque<hmlp::Statistic> s2n_gfp;


    void operator() ( NODE *node )
    {
      if ( rank.size() <= node->l )
      {
        rank.push_back( hmlp::Statistic() );
        merge_neighbors_time.push_back( hmlp::Statistic() );
        kij_skel.push_back( hmlp::Statistic() );
        kij_skel_time.push_back( hmlp::Statistic() );
        id_time.push_back( hmlp::Statistic() );
        skeletonize.push_back( hmlp::Statistic() );
        updateweight.push_back( hmlp::Statistic() );
        /** s2s */
        s2s_kij_t.push_back( hmlp::Statistic() );
        s2s_t.push_back( hmlp::Statistic() );
        s2s_gfp.push_back( hmlp::Statistic() );
        /** s2n */
        s2n_kij_t.push_back( hmlp::Statistic() );
        s2n_t.push_back( hmlp::Statistic() );
        s2n_gfp.push_back( hmlp::Statistic() );
      }

      rank[ node->l ].Update( (double)node->data.skels.size() );
      merge_neighbors_time[ node->l ].Update( node->data.merge_neighbors_time );
      kij_skel[ node->l ].Update( (double)node->data.kij_skel.second );
      kij_skel_time[ node->l ].Update( node->data.kij_skel.first );
      id_time[ node->l ].Update( node->data.id_time );
      skeletonize[ node->l ].Update( node->data.skeletonize.GetDuration() );
      updateweight[ node->l ].Update( node->data.updateweight.GetDuration() );

      s2s_kij_t[ node->l ].Update( node->data.kij_s2s.first         );
      s2s_t    [ node->l ].Update( node->data.s2s.GetDuration()     );
      s2s_gfp  [ node->l ].Update( node->data.s2s.GflopsPerSecond() );

      s2n_kij_t[ node->l ].Update( node->data.kij_s2n.first         );
      s2n_t    [ node->l ].Update( node->data.s2s.GetDuration()     );
      s2n_gfp  [ node->l ].Update( node->data.s2s.GflopsPerSecond() );

#ifdef DUMP_ANALYSIS_DATA
      if ( node->parent )
      {
        auto *parent = node->parent;
        printf( "@TREE\n" );
        printf( "#%lu (s%lu), #%lu (s%lu), %lu, %lu\n", 
            node->treelist_id, node->data.skels.size(), 
            parent->treelist_id, parent->data.skels.size(),
            node->data.skels.size(), node->l );
      }
      else
      {
        printf( "@TREE\n" );
        printf( "#%lu (s%lu), , %lu, %lu\n", 
            node->treelist_id, node->data.skels.size(), 
            node->data.skels.size(), node->l );
      }
#endif
    };

    void Print()
    {
      for ( size_t l = 1; l < rank.size(); l ++ )
      {
        printf( "===\n" );
        printf( "@SUMMARY\n" );
        printf( "rank:       " ); rank[ l ].Print();
        printf( "merge_neig: " ); merge_neighbors_time[ l ].Print();
        printf( "kij_skel_n: " ); kij_skel[ l ].Print();
        printf( "kij_skel_t: " ); kij_skel_time[ l ].Print();
        printf( "id_t:       " ); id_time[ l ].Print();
        printf( "skel_t:     " ); skeletonize[ l ].Print();
        printf( "... ... ...\n" );
        printf( "n2s_t:      " ); updateweight[ l ].Print();
        //printf( "s2s_kij_t:  " ); s2s_kij_t[ l ].Print();
        printf( "s2s_t:      " ); s2s_t[ l ].Print();
        printf( "s2s_gfp:    " ); s2s_gfp[ l ].Print();
        //printf( "s2n_kij_t:  " ); s2n_kij_t[ l ].Print();
        printf( "s2n_t:      " ); s2n_t[ l ].Print();
        printf( "s2n_gfp:    " ); s2n_gfp[ l ].Print();
        printf( "===\n" );
      }
    };

}; // end class Summary


/**
 *  @brief This the main splitter used to build the Spd-Askit tree.
 *         First compute the approximate center using subsamples.
 *         Then find the two most far away points to do the 
 *         projection.
 *
 *  @TODO  This splitter often fails to produce an even split when
 *         the matrix is sparse.
 *
 */ 
template<typename SPDMATRIX, int N_SPLIT, typename T, bool CONE>
struct centersplit
{
  /** closure */
  SPDMATRIX *Kptr;

  inline std::vector<std::vector<std::size_t> > operator()
  ( 
    std::vector<std::size_t>& gids,
    std::vector<std::size_t>& lids
  ) const 
  {
    assert( N_SPLIT == 2 );

    double beg, d2c_time, d2f_time, projection_time, max_time;

    SPDMATRIX &K = *Kptr;
    size_t n = lids.size();
    std::vector<std::vector<std::size_t> > split( N_SPLIT );

    std::vector<T> temp( n, 0.0 );

    beg = omp_get_wtime();
    // Compute d2c (distance to center)
    #pragma omp parallel for
    for ( size_t i = 0; i < n; i ++ )
    {
      size_t n_samples = std::log( n );
      temp[ i ] = K( lids[ i ], lids[ i ] );
      for ( size_t j = 0; j < n_samples; j ++ )
      {
        std::pair<T, size_t> sample = K.ImportantSample( lids[ i ] );
        //temp[ i ] -= 2.0 * K( lids[ i ], lids[ sample ] );
        temp[ i ] -= 2.0 / n_samples * sample.first;
      }
    }
    d2c_time = omp_get_wtime() - beg;

    // Find the f2c (far most to center)
    auto itf2c = std::max_element( temp.begin(), temp.end() );
    size_t idf2c = std::distance( temp.begin(), itf2c );

    beg = omp_get_wtime();
    // Compute the d2f (distance to far most)
    #pragma omp parallel for
    for ( size_t i = 0; i < n; i ++ )
    {
      temp[ i ] = K( lids[ i ], lids[ i ] ) - 2.0 * K( lids[ i ], lids[ idf2c ] );
    }
    d2f_time = omp_get_wtime() - beg;

    // Find the f2f (far most to far most)
    beg = omp_get_wtime();
    auto itf2f = std::max_element( temp.begin(), temp.end() );
    max_time = omp_get_wtime() - beg;
    size_t idf2f = std::distance( temp.begin(), itf2f );

#ifdef DEBUG_SPDASKIT
    printf( "idf2c %lu idf2f %lu\n", idf2c, idf2f );
#endif

    beg = omp_get_wtime();
    // Compute projection
    #pragma omp parallel for
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( ! CONE) temp[ i ] = K( lids[ i ], lids[ idf2f ] ) - K( lids[ i ], lids[ idf2c ] );
      else temp[ i ] = std::abs((K( lids[ i ], lids[ idf2f ] ) - K( lids[ i ], lids[ idf2c ] )) / K( lids[ i ], lids[ i ] ));
    }
    projection_time = omp_get_wtime() - beg;
    //printf( "log(n) %lu d2c %5.3lfs d2f %5.3lfs proj %5.3lfs max %5.3lfs\n", 
    //	(size_t)std::log( n ), d2c_time, d2f_time, projection_time, max_time );

    /** parallel median search */
    T median = hmlp::tree::Select( n, n / 2, temp );

    split[ 0 ].reserve( n / 2 + 1 );
    split[ 1 ].reserve( n / 2 + 1 );

    /** TODO: Can be parallelized */
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( temp[ i ] > median ) split[ 1 ].push_back( i );
      else                      split[ 0 ].push_back( i );
    }

    return split;
  };
}; // end struct centersplit


/**
 *  @brief This the splitter used in the randomized tree.
 *
 *  @TODO  This splitter often fails to produce an even split when
 *         the matrix is sparse.
 *
 */ 
template<typename SPDMATRIX, int N_SPLIT, typename T, bool CONE>
struct randomsplit
{
  /** closure */
  SPDMATRIX *Kptr;

  inline std::vector<std::vector<std::size_t> > operator()
  ( 
    std::vector<std::size_t>& gids,
    std::vector<std::size_t>& lids
  ) const 
  {
    assert( N_SPLIT == 2 );

    SPDMATRIX &K = *Kptr;
    size_t n = lids.size();
    std::vector<std::vector<std::size_t> > split( N_SPLIT );
    std::vector<T> temp( n, 0.0 );

    size_t idf2c = std::rand() % n;
    size_t idf2f = std::rand() % n;

    while ( idf2c == idf2f ) idf2f = std::rand() % n;

#ifdef DEBUG_SPDASKIT
    printf( "randomsplit idf2c %lu idf2f %lu\n", idf2c, idf2f );
#endif

    // Compute projection
    #pragma omp parallel for
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( ! CONE) temp[ i ] = K( lids[ i ], lids[ idf2f ] ) - K( lids[ i ], lids[ idf2c ] );
      else temp[ i ] = std::abs((K( lids[ i ], lids[ idf2f ] ) - K( lids[ i ], lids[ idf2c ] )) / K( lids[ i ], lids[ i ] ));
   }

    // Parallel median search
    T median = hmlp::tree::Select( n, n / 2, temp );

//    split[ 0 ].reserve( n / 2 + 1 );
//    split[ 1 ].reserve( n / 2 + 1 );
//
//    // TODO: Can be parallelized
//    for ( size_t i = 0; i < n; i ++ )
//    {
//      if ( temp[ i ] > median ) split[ 1 ].push_back( i );
//      else                      split[ 0 ].push_back( i );
//    }


    std::vector<size_t> lflag( n, 0 );
    std::vector<size_t> rflag( n, 0 );
    std::vector<size_t> pscan( n + 1, 0 );

    #pragma omp parallel for
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( temp[ i ] > median ) rflag[ i ] = 1;
      else                      lflag[ i ] = 1;
    }

    hmlp::tree::Scan( lflag, pscan );
    split[ 0 ].resize( pscan[ n ] );
    #pragma omp parallel for 
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( lflag[ i ] ) split[ 0 ][ pscan[ i + 1 ] - 1 ] = i;
    }

    hmlp::tree::Scan( rflag, pscan );
    split[ 1 ].resize( pscan[ n ] );
    #pragma omp parallel for 
    for ( size_t i = 0; i < n; i ++ )
    {
      if ( rflag[ i ] ) split[ 1 ][ pscan[ i + 1 ] - 1 ] = i;
    }


    return split;
  };
}; // end struct randomsplit



/**
 *  @brief This is the task wrapper of the exact KNN search we
 *         perform in the leaf node of the randomized tree.
 *         Currently our heap select cannot deal with duplicate
 *         id; thus, I need to use a std::set to check for the
 *         duplication before inserting the candidate into the
 *         heap.
 *
 *  @TODO  Improve the heap to deal with unique id.
 *
 */ 
template<int NUM_TEST, bool CONE, class NODE, typename T>
class KNNTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "nn" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;

      //--------------------------------------
      double flops, mops;
      auto &lids = arg->lids;
      auto &NN = *arg->setup->NN;
      flops = lids.size();
      flops *= 4.0 * lids.size();
      // Heap select worst case
      mops = (size_t)std::log( NN.row() ) * lids.size();
      mops *= lids.size();
      // Access K
      mops += flops;
      event.Set( name + label, flops, mops );
      //--------------------------------------
    };

    void Execute( Worker* user_worker )
    {
      auto &K = *arg->setup->K;
      auto &NN = *arg->setup->NN;
      auto &lids = arg->lids;
     
      #pragma omp parallel for
      for ( size_t j = 0; j < lids.size(); j ++ )
      {
        std::set<size_t> NNset;

        for ( size_t i = 0; i < NN.row(); i ++ )
        {
          size_t jlid = lids[ j ];
          NNset.insert( NN[ jlid * NN.row() + i ].second );
		}

		for ( size_t i = 0; i < lids.size(); i ++ )
		{
		  size_t ilid = lids[ i ];
		  size_t jlid = lids[ j ];

		  if ( !NNset.count( ilid ) )
		  {
			T dist;
			if ( !CONE ) 
			{
			  dist = K( ilid, ilid ) + K( jlid, jlid ) - 2.0 * K( ilid, jlid );
			}
			else  
			{
			  dist = 1 - std::abs( K( ilid, jlid ) / ( K( jlid, jlid ) * K( ilid, ilid ) ) );
			}
			std::pair<T, size_t> query( dist, ilid );
			hmlp::HeapSelect( 1, NN.row(), &query, NN.data() + jlid * NN.row() );
		  }
		  else
		  {
			/** ignore duplication */
		  }
		}
	  } /** end omp parallel for */


      /** test the accuracy of NN with exhausted search */
	  double knn_acc = 0.0;
	  size_t num_acc = 0;

	  for ( size_t j = 0; j < lids.size(); j ++ )
	  {
		if ( lids[ j ] >= NUM_TEST ) continue;
		
        std::set<size_t> NNset;
	    hmlp::Data<std::pair<T, size_t>> nn_test( NN.row(), 1 );
        for ( size_t i = 0; i < NN.row(); i ++ )
		{
		  nn_test[ i ] = NN( i, lids[ j ] );
          NNset.insert( nn_test[ i ].second );
		}
		/** loop over all references */
        for ( size_t i = 0; i < K.row(); i ++ )
		{
		  size_t ilid = i;
		  size_t jlid = lids[ j ];
		  if ( !NNset.count( ilid ) )
		  {
			T dist;
			if ( !CONE ) 
			{
			  dist = K( ilid, ilid ) + K( jlid, jlid ) - 2.0 * K( ilid, jlid );
			}
			else  
			{
			  dist = 1 - std::abs( K( ilid, jlid ) / ( K( jlid, jlid ) * K( ilid, ilid ) ) );
			}
			std::pair<T, size_t> query( dist, ilid );
			hmlp::HeapSelect( 1, nn_test.row(), &query, nn_test.data() );
		  }
		}

		/** compute the acruracy */
		size_t correct = 0;
		NNset.clear();
        for ( size_t i = 0; i < NN.row(); i ++ ) NNset.insert( nn_test[ i ].second );
        for ( size_t i = 0; i < NN.row(); i ++ ) 
		{
		  if ( NNset.count( NN( i, lids[ j ] ).second ) ) correct ++;
		}
		knn_acc += (double)correct / NN.row();
		num_acc ++;
	  }
	  arg->data.knn_acc = knn_acc;
	  arg->data.num_acc = num_acc;

	};

}; /** end class KNNTask */




template<bool DOAPPROXIMATE, bool SORTED, typename T, typename CSCMATRIX>
hmlp::Data<std::pair<T, std::size_t>> SparsePattern( size_t n, size_t k, CSCMATRIX &K )
{
  std::pair<T, std::size_t> initNN( std::numeric_limits<T>::max(), n );
  hmlp::Data<std::pair<T, std::size_t>> NN( k, n, initNN );

  printf( "SparsePattern k %lu n %lu, NN.row %lu NN.col %lu ...", 
      k, n, NN.row(), NN.col() ); fflush( stdout );

  #pragma omp parallel for schedule( dynamic )
  for ( size_t j = 0; j < n; j ++ )
  {
    std::set<size_t> NNset;
    size_t nnz = K.ColPtr( j + 1 ) - K.ColPtr( j );
    if ( DOAPPROXIMATE && nnz > 2 * k ) nnz = 2 * k;

    //printf( "j %lu nnz %lu\n", j, nnz );

    for ( size_t i = 0; i < nnz; i ++ )
    {
      // TODO: this is lid. Need to be gid.
      auto row_ind = K.RowInd( K.ColPtr( j ) + i );
      auto val     = K.Value( K.ColPtr( j ) + i );

      if ( val ) val = 1.0 / std::abs( val );
      else       val = std::numeric_limits<T>::max() - 1.0;

      NNset.insert( row_ind );
      std::pair<T, std::size_t> query( val, row_ind );
      if ( nnz < k ) // not enough candidates
      {
        NN[ j * k + i  ] = query;
      }
      else
      {
        hmlp::HeapSelect( 1, NN.row(), &query, NN.data() + j * NN.row() );
      }
    }

    while ( nnz < k )
    {
      std::size_t row_ind = rand() % n;
      if ( !NNset.count( row_ind ) )
      {
        T val = std::numeric_limits<T>::max() - 1.0;
        std::pair<T, std::size_t> query( val, row_ind );
        NNset.insert( row_ind );
        NN[ j * k + nnz ] = query;
        nnz ++;
      }
    }
  }
  printf( "Done.\n" ); fflush( stdout );

  if ( SORTED )
  {
    printf( "Sorting ... " ); fflush( stdout );
    struct 
    {
      bool operator () ( std::pair<T, size_t> a, std::pair<T, size_t> b )
      {   
        return a.first < b.first;
      }   
    } ANNLess;

    //printf( "SparsePattern k %lu n %lu, NN.row %lu NN.col %lu\n", k, n, NN.row(), NN.col() );

    #pragma omp parallel for
    for ( size_t j = 0; j < NN.col(); j ++ )
    {
      std::sort( NN.data() + j * NN.row(), NN.data() + ( j + 1 ) * NN.row(), ANNLess );
    }
    printf( "Done.\n" ); fflush( stdout );
  }
  
//  for ( size_t j = 0; j < NN.col(); j ++ )
//  {
//	for ( size_t i = 0; i < NN.row(); i ++ )
//	{
//	  printf( "%4lu ", NN[ j * k + i ].second );
//	}
//	printf( "\n" );
//  }


  return NN;
};


/*
 * @brief Helper functions for sorting sampling neighbors.
 */ 
template<typename TA, typename TB>
std::pair<TB, TA> flip_pair( const std::pair<TA, TB> &p )
{
  return std::pair<TB, TA>( p.second, p.first );
}; // end flip_pair()


template<typename TA, typename TB>
std::multimap<TB, TA> flip_map( const std::map<TA, TB> &src )
{
  std::multimap<TB, TA> dst;
  std::transform( src.begin(), src.end(), std::inserter( dst, dst.begin() ), 
                 flip_pair<TA, TB> );
  return dst;
}; // end flip_map()

/*template<typename TA, typename TB>
bool compare_pair ( std::pair<TA, TB> a , std::pair<TA, TB> b ) 
{
  return (a.first < b.first); 
}
*/

/**
 *  @brief Building neighbors for each tree node.
 */ 
template<typename NODE, typename T>
void BuildNeighbors( NODE *node , size_t nsamples)
{
  auto &NN = node->setup->NN;
  std::vector<size_t> &gids = node->gids;
  std::vector<size_t> &lids = node->lids;
  auto &snids = node->data.snids;
  auto &pnids = node->data.pnids;
  int n = node->n;
  int k = NN->row();
  if ( node->isleaf )
  {
    // Pruning neighbor lists/sets:
    pnids = std::unordered_set<size_t>();
    for ( int ii = 0; ii < k / 2; ii ++ )
    {
      for ( int jj = 0; jj < n; jj ++ )
      {
        pnids.insert( NN->data()[ lids[ jj ] * k + ii ].second );
        //printf("%lu;",NN->data()[ gids[jj] * k + ii].second); 
      }
    }
    // Remove "own" points
    for ( int i = 0; i < n; i ++ )
    {
      pnids.erase( gids[ i ] );
    }
    //printf("Size of pruning neighbor set: %lu \n", pnids.size());
    // Sampling neighbors
    // To think about: Make building sampling neighbor adaptive.  
    // E.g. request 0-100 closest neighbors, 
    // if additional 100 neighbors are requested, return sneighbors 100-200 
    snids = std::map<size_t, T>(); 
    std::vector<std::pair<T, size_t>> tmp ( k / 2 * n ); 
    std::set<size_t> nodeIdx( gids.begin() , gids.end() );    
    // Allocate array for sorting 
    for ( int ii = (k+1) / 2; ii < k; ii ++ )
    {
      for ( int jj = 0; jj < n; jj ++ )
      {
        tmp [ (ii-(k+1)/2) * n + jj ] = NN->data()[ lids[ jj ] * k + ii ];
      }
    }
    std::sort( tmp.begin() , tmp.end() );
    int i = 0;
    while ( snids.size() < nsamples && i <  (k-1) * n / 2 )
    {
      if ( !pnids.count( tmp[i].second ) && !nodeIdx.count( tmp[i].second ) )
      {
        snids.insert( std::pair<size_t,T>( tmp[i].second , tmp[i].first ) );
      }
      i++;
    } 
    //printf("Size of sampling neighbor list: %lu \n", snids.size());
  }
  else
  {
    // At interior node 
    auto &lsnids = node->lchild->data.snids;
    auto &rsnids = node->rchild->data.snids;
    auto &lpnids = node->lchild->data.pnids;
    auto &rpnids = node->rchild->data.pnids;

    // Merge children's sampling neighbors...    
    // Start with left sampling neighbor list 
    snids = lsnids;
    // Add right sampling neighbor list. If duplicate update distace if nec.
    //std::pair<std::map<size_t, T>::iterator, bool> ret;
    for ( auto cur = rsnids.begin(); cur != rsnids.end(); cur ++ )
    {
      auto ret = snids.insert( *cur );
      if ( ret.second == false )
      {
        // Update distance?
        if ( ret.first->second > (*cur).first)
        {
          ret.first->second = (*cur).first;
        }
      }
    }

    // Remove "own" points
    for (int i = 0; i < n; i ++ )
    {
      snids.erase( gids[ i ] );
    }

    // Remove pruning neighbors from left and right
    for (auto cur = lpnids.begin(); cur != lpnids.end(); cur++ )
    {
      snids.erase( *cur );
    }
    for (auto cur = rpnids.begin(); cur != rpnids.end(); cur++ )
    {
      snids.erase( *cur );
    }

    //printf("Interior sampling neighbor size: %lu\n", snids.size());
  }
}; // end BuildNeighbors()



/**
 *  @brief Compute the cofficient matrix by R11^{-1} * proj.
 *
 */ 
template<typename NODE, typename T>
void Interpolate( NODE *node )
{
  /** early return */
  if ( !node ) return;

  auto &data = node->data;
  auto &skels = data.skels;
  auto &proj = data.proj;
  auto &jpvt = data.jpvt;
  auto s = proj.row();
  auto n = proj.col();

  /** proceed if the node can be compressed */
  if ( data.isskel )
  {
    assert( s );
    assert( s <= n );
    assert( jpvt.size() == n );
  }
  else
  {
    return;
  }

  /** if is skeletonized, reserve space for w_skel and u_skel */
  if ( data.isskel )
  {
    data.w_skel.reserve( skels.size(), 1024 );
    data.u_skel.reserve( skels.size(), 1024 );
  }



  /** early return if ( s == n ) */
  //if ( s == n )
  //{
  //  for ( int i = 0; i < s; i ++ ) skels[ i ] = i;
  //  for ( int j = 0; j < s; j ++ )
  //  {
  //    for ( int i = 0; i < s; i ++ )
  //    {
  //      if ( i == j ) proj[ j * s + i ] = 1.0;
  //  	else          proj[ j * s + i ] = 0.0;
  //    }
  //  }
  //  return;
  //}

  /** fill in R11 */
  hmlp::Data<T> R1( s, s, 0.0 );

  for ( int j = 0; j < s; j ++ )
  {
    for ( int i = 0; i < s; i ++ )
    {
      if ( i <= j ) R1[ j * s + i ] = proj[ j * s + i ];
    }
  }

  /** copy proj to tmp */
  hmlp::Data<T> tmp = proj;

  /** proj = inv( R1 ) * proj */
  hmlp::xtrsm( "L", "U", "N", "N", s, n, 1.0, R1.data(), s, tmp.data(), s );

  /** Fill in proj */
  for ( int j = 0; j < n; j ++ )
  {
    for ( int i = 0; i < s; i ++ )
    {
  	  proj[ jpvt[ j ] * s + i ] = tmp[ j * s + i ];
    }
  }
  
}; // end Interpolate()


/**
 *  @brief
 */ 
template<typename NODE, typename T>
class InterpolateTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "it" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();
      // Need an accurate cost model.
      cost = 1.0;
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;
      event.Set( label + name, flops, mops );
    };

    void DependencyAnalysis()
    {
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
    }

    void Execute( Worker* user_worker )
    {
      Interpolate<NODE, T>( arg );
    };

}; // end class InterpolateTask









/**
 *  @brief Skeletonization with interpolative decomposition.
 */ 
template<bool ADAPTIVE, bool LEVELRESTRICTION, typename NODE, typename T>
void Skeletonize( NODE *node )
{
  /** early return if we do not need to skeletonize */
  if ( !node->parent ) return;

  double beg = 0.0, kij_skel_time = 0.0, merge_neighbors_time = 0.0, id_time = 0.0;

  /** gather shared data and create reference */
  auto &K = *node->setup->K;
  auto maxs = node->setup->s;
  auto stol = node->setup->stol;
  auto &NN = *node->setup->NN;

  /** gather per node data and create reference */
  auto &data = node->data;
  auto &skels = data.skels;
  auto &proj = data.proj;
  auto &jpvt = data.jpvt;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  /** early return if fail to skeletonize. */
  if ( LEVELRESTRICTION )
  {
    assert( ADAPTIVE );
    if ( !node->isleaf && ( !lchild->data.isskel || !rchild->data.isskel ) )
    {
      skels.clear();
      proj.resize( 0, 0 );
      data.isskel = false;
      return;
    }
  }
  else
  {
    //skels.resize( maxs );
    //proj.resize( )
  }

  /** random sampling or importance sampling for rows. */
  std::vector<size_t> amap;
  std::vector<size_t> bmap;
  std::vector<size_t> &lids = node->lids;


  /** merge children's skeletons */
  if ( node->isleaf )
  {
    bmap = node->lids;
  }
  else
  {
    auto &lskels = lchild->data.skels;
    auto &rskels = rchild->data.skels;
    bmap = lskels;
    bmap.insert( bmap.end(), rskels.begin(), rskels.end() );
  }

  auto nsamples = 2 * bmap.size();

  /** Build Node Neighbors from all nearest neighbors */
  beg = omp_get_wtime();
  BuildNeighbors<NODE, T>( node, nsamples );
  merge_neighbors_time = omp_get_wtime() - beg;
  
  /** update merge_neighbors timer */
  data.merge_neighbors_time = merge_neighbors_time;


  auto &snids = data.snids;
  // Order snids by distance
  std::multimap<T, size_t > ordered_snids = flip_map( snids );
  if ( nsamples < K.col() - node->n )
  {
    amap.reserve( nsamples );
    for ( auto cur = ordered_snids.begin(); cur != ordered_snids.end(); cur++ )
    {
      amap.push_back( cur->second );
    }
    // Uniform samples.
    if ( amap.size() < nsamples )
    {
      while ( amap.size() < nsamples )
      {
        size_t sample;

        if ( rand() % 5 ) // 80% chances to use important sample
        {
          auto importantsample = K.ImportantSample( bmap[ rand() % bmap.size() ] );
          sample = importantsample.second;
        }
        else
        {
          sample = rand() % K.col();
        }

        if ( std::find( amap.begin(), amap.end(), sample ) == amap.end() &&
            std::find( lids.begin(), lids.end(), sample ) == lids.end() )
        {
          amap.push_back( sample );
        }
      }
    }
  }
  else // Use all off-diagonal blocks without samples.
  {
    for ( int sample = 0; sample < K.col(); sample ++ )
    {
      if ( std::find( amap.begin(), amap.end(), sample ) == amap.end() )
      {
        amap.push_back( sample );
      }
    }
  }

  /** get submatrix Kab from K */
  beg = omp_get_wtime();
  auto Kab = K( amap, bmap );
  kij_skel_time = omp_get_wtime() - beg;


  /** update kij counter */
  data.kij_skel.first  = kij_skel_time;
  data.kij_skel.second = amap.size() * bmap.size();


  /** interpolative decomposition */
  beg = omp_get_wtime();
  hmlp::skel::id<ADAPTIVE, LEVELRESTRICTION>
  ( 
    amap.size(), bmap.size(), maxs, stol, /** ignore if !ADAPTIVE */
    Kab, skels, proj, jpvt
  );
  id_time = omp_get_wtime() - beg;

  /** update id timer */
  data.id_time = id_time;

  /** depending on the flag, decide isskel or not */
  if ( LEVELRESTRICTION )
  {
    data.isskel = (skels.size() != 0);
  }
  else
  {
    assert( skels.size() );
    assert( proj.size() );
    assert( jpvt.size() );
    data.isskel = true;
  }
  
  /** relabel skeletions with the real lids */
  for ( size_t i = 0; i < skels.size(); i ++ )
  {
    skels[ i ] = bmap[ skels[ i ] ];
  }

  /** separate interpolation of proj */
  //Interpolate<NODE, T>( node );

  /** update Pruning neighbor list, TODO: not sure what is this  */
  data.pnids.clear();
  for ( int ii = 0 ; ii < skels.size() ; ii ++ )
  {
    for ( int jj = 0; jj < NN.row() / 2; jj ++ )
    {
      data.pnids.insert( NN.data()[ skels[ ii ] * NN.row() + jj ].second );
    }
  }
}; // end void Skeletonize()


/**
 *
 */ 
template<bool ADAPTIVE, bool LEVELRESTRICTION, typename NODE, typename T>
class SkeletonizeTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      std::ostringstream ss;
      arg = user_arg;
      name = std::string( "sk" );
      //label = std::to_string( arg->treelist_id );
      ss << arg->treelist_id;
      label = ss.str();

      /** we don't know the exact cost here */
      cost = 5.0;

      /** high priority */
      priority = true;
    };

    void GetEventRecord()
    {
      double flops = 0.0, mops = 0.0;

      size_t n = arg->data.proj.col();
      size_t m = 2 * n;
      size_t k = arg->data.proj.row();

      /** GEQP3 */
      flops += ( 2.0 / 3.0 ) * n * n * ( 3 * m - n );
      mops += ( 2.0 / 3.0 ) * n * n * ( 3 * m - n );

      /* TRSM */
      flops += k * ( k - 1 ) * ( n + 1 );
      mops  += 2.0 * ( k * k + k * n );

      //flops += ( 2.0 / 3.0 ) * k * k * ( 3 * m - k );
      //mops += 2.0 * m * k;
      //flops += 2.0 * m * n * k;
      //mops += 2.0 * ( m * k + k * n + m * n );
      //flops += ( 1.0 / 3.0 ) * k * k * n;
      //mops += 2.0 * ( k * k + k * n );

      event.Set( label + name, flops, mops );
      arg->data.skeletonize = event;
    };

    void DependencyAnalysis()
    {
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
      Skeletonize<ADAPTIVE, LEVELRESTRICTION, NODE, T>( arg );
    };

}; // end class SkeletonizeTask







template<typename NODE>
void UpdateWeights( NODE *node )
{
#ifdef DEBUG_SPDASKIT
  printf( "%lu UpdateWeight\n", node->treelist_id );
#endif
  /** early return */
  if ( !node->parent || !node->data.isskel ) return;

  /** eanble nested parallelism */
  //int num_threads = omp_get_num_threads();
  //if ( node->l < 4 ) omp_set_num_threads( 4 );

  /** gather shared data and create reference */
  auto &w = *node->setup->w;

  /** gather per node data and create reference */
  auto &data = node->data;
  auto &proj = data.proj;
  auto &skels = data.skels;
  auto &w_skel = data.w_skel;
  auto &w_leaf = data.w_leaf;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  /** w_skel is s-by-nrhs, initial values are not important */
  w_skel.resize( skels.size(), w.row() );

  //printf( "%lu UpdateWeight w_skel.num() %lu\n", node->treelist_id, w_skel.num() );

  if ( node->isleaf )
  {
    double beg = omp_get_wtime();
    //w_leaf = w( node->lids );
    //double w_leaf_time = omp_get_wtime() - beg;

#ifdef DEBUG_SPDASKIT
    printf( "m %lu n %lu k %lu\n", 
      w_skel.row(), w_skel.col(), w_leaf.col());
    printf( "proj.row() %lu w_leaf.row() %lu w_skel.row() %lu\n", 
        proj.row(), w_leaf.row(), w_skel.row() );
#endif

    xgemm
    (
      "N", "N",
      w_skel.row(), w_skel.col(), w_leaf.row(),
      1.0, proj.data(),   proj.row(),
           w_leaf.data(), w_leaf.row(),
      0.0, w_skel.data(), w_skel.row()
    );

    double update_leaf_time = omp_get_wtime() - beg;
    //printf( "m %lu n %lu k %lu, w_leaf %.3E total %.3E\n", 
    //  w_skel.row(), w_skel.col(), w_leaf.col(),
    //  w_leaf_time, update_leaf_time );
  }
  else
  {
    auto &w_lskel = lchild->data.w_skel;
    auto &w_rskel = rchild->data.w_skel;
    auto &lskel = lchild->data.skels;
    auto &rskel = rchild->data.skels;
    xgemm
    (
      "N", "N",
      w_skel.row(), w_skel.col(), lskel.size(),
      1.0,    proj.data(),    proj.row(),
           w_lskel.data(), w_lskel.row(),
      0.0,  w_skel.data(),  w_skel.row()
    );
    xgemm
    (
      "N", "N",
      w_skel.row(), w_skel.col(), rskel.size(),
      1.0,    proj.data() + proj.row() * lskel.size(), proj.row(),
           w_rskel.data(), w_rskel.row(),
      1.0,  w_skel.data(),  w_skel.row()
    );
  }

  /** reset omp threads */
  //omp_set_num_threads( num_threads );

}; // end void SetWeights()


/**
 *
 */ 
template<typename NODE>
class UpdateWeightsTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      arg = user_arg;
      name = std::string( "n2s" );
      {
        //label = std::to_string( arg->treelist_id );
        std::ostringstream ss;
        ss << arg->treelist_id;
        label = ss.str();
      }

      /** compute flops and mops */
      double flops, mops;
      auto &lids = arg->lids;
      auto &skels = arg->data.skels;
      auto &w = *arg->setup->w;
      if ( arg->isleaf )
      {
        auto m = skels.size();
        auto n = w.row();
        auto k = lids.size();
        flops = 2.0 * m * n * k;
        mops = 2.0 * ( m * n + m * k + k * n );
      }
      else
      {
        auto &lskels = arg->lchild->data.skels;
        auto &rskels = arg->rchild->data.skels;
        auto m = skels.size();
        auto n = w.row();
        auto k = lskels.size() + rskels.size();
        flops = 2.0 * m * n * k;
        mops  = 2.0 * ( m * n + m * k + k * n );
      }

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** assume computation bound */
      cost = flops / 1E+9;

      /** high priority */
      priority = true;
    };

    void Prefetch()
    {
      auto &proj = arg->data.proj;
      __builtin_prefetch( proj.data() );
      auto &w_skel = arg->data.w_skel;
      __builtin_prefetch( w_skel.data() );
      if ( arg->isleaf )
      {
        auto &w_leaf = arg->data.w_leaf;
        __builtin_prefetch( w_leaf.data() );
      }
      else
      {
        auto &w_lskel = arg->lchild->data.w_skel;
        __builtin_prefetch( w_lskel.data() );
        auto &w_rskel = arg->rchild->data.w_skel;
        __builtin_prefetch( w_rskel.data() );
      }
    };

    void GetEventRecord()
    {
      arg->data.updateweight = event;
    };

    void DependencyAnalysis()
    {
      if ( !arg->parent ) 
      {
        this->Enqueue();
        return;
      }

      auto &w_skel = arg->data.w_skel;
      w_skel.DependencyAnalysis( hmlp::ReadWriteType::W, this );

      if ( !arg->isleaf )
      {
        auto &w_lskel = arg->lchild->data.w_skel;
        auto &w_rskel = arg->rchild->data.w_skel;
        w_lskel.DependencyAnalysis( hmlp::ReadWriteType::R, this );
        w_rskel.DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
      else
      {
        this->Enqueue();
      }
    };

    void Execute( Worker* user_worker )
    {
#ifdef HMLP_USE_CUDA 
      hmlp::Device *device = NULL;
      if ( user_worker ) device = user_worker->GetDevice();
      if ( device ) gpu::UpdateWeights( device, arg );
      else               UpdateWeights( arg );
#else
      UpdateWeights( arg );
#endif
    };

}; // end class SetWeights



/**
 *  @brief Compute the interation from column skeletons to row
 *         skeletons. Store the results in the node. Later
 *         there is a SkeletonstoAll function to be called.
 *
 */ 
template<bool NNPRUNE, typename NODE>
void SkeletonsToSkeletons( NODE *node )
{
#ifdef DEBUG_SPDASKIT
  printf( "%lu Skel2Skel \n", node->treelist_id ); fflush( stdout );
#endif

  if ( !node->parent || !node->data.isskel ) return;

  double beg, kij_s2s_time = 0.0, u_skel_time, s2s_time;

  std::set<NODE*> *FarNodes;
  if ( NNPRUNE ) FarNodes = &node->NNFarNodes;
  else           FarNodes = &node->FarNodes;

  auto &K = *node->setup->K;
  auto &data = node->data;
  auto &amap = node->data.skels;
  auto &u_skel = node->data.u_skel;
  auto &FarKab = node->data.FarKab;

  /** initilize u_skel to be zeros( s, nrhs ). */
  beg = omp_get_wtime();
  u_skel.clear();
  u_skel.resize( amap.size(), node->setup->w->row(), 0.0 );
  u_skel_time = omp_get_wtime() - beg;

  size_t offset = 0;

  /** reduce all u_skel */
  for ( auto it = FarNodes->begin(); it != FarNodes->end(); it ++ )
  {
    auto &bmap = (*it)->data.skels;
    auto &w_skel = (*it)->data.w_skel;
    assert( w_skel.col() == u_skel.col() );

    if ( FarKab.size() ) /** Kab is cached */
    {
      xgemm
      (
        "N", "N",
        u_skel.row(), u_skel.col(), w_skel.row(),
        1.0, FarKab.data() + offset, FarKab.row(),
             w_skel.data(),          w_skel.row(),
        1.0, u_skel.data(),          u_skel.row()
      );
      /** move to the next submatrix Kab */
      offset += u_skel.row() * w_skel.row();
    }
    else
    {
      /** get submatrix Kad from K */
      beg = omp_get_wtime();
      auto Kab = K( amap, bmap );
      kij_s2s_time = omp_get_wtime() - beg;

      /** update kij counter */
      data.kij_s2s.first  += kij_s2s_time;
      data.kij_s2s.second += amap.size() * bmap.size();

      //printf( "%lu (%lu, %lu), ", (*it)->treelist_id, w_skel.row(), w_skel.num() );
      //fflush( stdout );
      xgemm
      (
        "N", "N",
        u_skel.row(), u_skel.col(), w_skel.row(),
        1.0, Kab.data(),       Kab.row(),
             w_skel.data(), w_skel.row(),
        1.0, u_skel.data(), u_skel.row()
      );
    }
  }
  s2s_time = omp_get_wtime() - beg;

  //printf( "u_skel %.3E s2s %.3E\n", u_skel_time, s2s_time );

}; // end void SkeletonsToSkeletons()



/**
 *  @brief There is no dependency between each task. However 
 *         there are raw (read after write) dependencies:
 *
 *         NodesToSkeletons (P*w)
 *         SkeletonsToSkeletons ( Sum( Kab * ))
 *
 *  @TODO  The flops and mops of constructing Kab.
 *
 */ 
template<bool NNPRUNE, typename NODE>
class SkeletonsToSkeletonsTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      arg = user_arg;
      name = std::string( "s2s" );
      {
        //label = std::to_string( arg->treelist_id );
        std::ostringstream ss;
        ss << arg->treelist_id;
        label = ss.str();
      }

      /** compute flops and mops */
      double flops = 0.0, mops = 0.0;
      auto &w = *arg->setup->w;
      size_t m = arg->data.skels.size();
      size_t n = w.row();

      std::set<NODE*> *FarNodes;
      if ( NNPRUNE ) FarNodes = &arg->NNFarNodes;
      else           FarNodes = &arg->FarNodes;

      for ( auto it = FarNodes->begin(); it != FarNodes->end(); it ++ )
      {
        size_t k = (*it)->data.skels.size();
        flops += 2.0 * m * n * k;
        mops  += m * k; // cost of Kab
        mops  += 2.0 * ( m * n + n * k + k * n );
      }

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** assume computation bound */
      cost = flops / 1E+9;

      /** high priority */
      priority = true;
    };

    void Prefetch()
    {
      auto &u_skel = arg->data.u_skel;
      __builtin_prefetch( u_skel.data() );
    };

    void GetEventRecord()
    {
      arg->data.s2s = event;
    };

    void DependencyAnalysis()
    {
      auto &u_skel = arg->data.u_skel;
      std::set<NODE*> *FarNodes;
      FarNodes = &arg->NNFarNodes;

      if ( !arg->parent || !FarNodes->size() ) this->Enqueue();

      //printf( "node %lu write u_skel ", arg->treelist_id );
      u_skel.DependencyAnalysis( hmlp::ReadWriteType::W, this );
      for ( auto it = FarNodes->begin(); it != FarNodes->end(); it ++ )
      {
        //printf( "%lu ", (*it)->treelist_id );
        auto &w_skel = (*it)->data.w_skel;
        w_skel.DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
      //printf( "\n" );
    };

    void Execute( Worker* user_worker )
    {
      SkeletonsToSkeletons<NNPRUNE, NODE>( arg );
    };
}; // end class SkeletonsToSkeletonsTask


/**
 *  @brief This is a task in Downward traversal. There is data
 *         dependency on u_skel.
 *         
 */ 
template<bool NNPRUNE, typename NODE, typename T>
void SkeletonsToNodes( NODE *node )
{
#ifdef DEBUG_SPDASKIT
  printf( "%lu Skel2Node u_skel.row() %lu\n", node->treelist_id, node->data.u_skel.row() ); fflush( stdout );
#endif

  double beg, kij_s2n_time = 0.0, u_leaf_time, before_writeback_time, after_writeback_time;

  /** gather shared data and create reference */
  auto &K = *node->setup->K;
  auto &w = *node->setup->w;
  auto &u = *node->setup->u;

  /** Gather per node data and create reference */
  auto &lids = node->lids;
  auto &data = node->data;
  auto &proj = data.proj;
  auto &skels = data.skels;
  auto &u_skel = data.u_skel;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  if ( node->isleaf )
  {
    std::set<NODE*> *NearNodes;
    if ( NNPRUNE ) NearNodes = &node->NNNearNodes;
    else           NearNodes = &node->NearNodes;
    auto &amap = node->lids;
    auto &u_leaf = node->data.u_leaf[ 0 ];
    //beg = omp_get_wtime();
    //u_leaf.resize( w.row(), lids.size(), 0.0 );
    u_leaf.clear();
    u_leaf.resize( lids.size(), w.row(), 0.0 );
    //u_leaf_time = omp_get_wtime() - beg;

    assert( u_leaf.size() == w.row() * lids.size() );

//    /** reduce direct iteractions from 4 copies */
//    for ( size_t p = 1; p < 4; p ++ )
//    {
//      for ( size_t i = 0; i < u_leaf.size(); i ++ )
//      {
//        u_leaf[ i ] += node->data.u_leaf[ p ][ i ];
//      }
//    }

    /** accumulate far interactions */
    if ( data.isskel )
    {
      //xgemm
      //(
      //  "T", "N",
      //  u_leaf.row(), u_leaf.col(), proj.row(),
      //  1.0, u_skel.data(), u_skel.row(),
      //         proj.data(),   proj.row(),
      //  1.0, u_leaf.data(), u_leaf.row()
      //);

      xgemm
      (
        "T", "N",
        u_leaf.row(), u_leaf.col(), u_skel.row(),
        1.0,   proj.data(),   proj.row(),
             u_skel.data(), u_skel.row(),
        1.0, u_leaf.data(), u_leaf.row()
      );
    }

//    /** assemble u_leaf back to u */
//    for ( size_t j = 0; j < amap.size(); j ++ )
//    {
//      for ( size_t i = 0; i < u.row(); i ++ )
//      {
//        //u[ amap[ j ] * u.row() + i ] = u_leaf[ j * u.row() + i ];
//        u[ amap[ j ] * u.row() + i ] = u_leaf( j, i );
//      }
//    }
    after_writeback_time = omp_get_wtime() - beg;

    //printf( "u_leaf %.3E before %.3E after %.3E\n",
    //    u_leaf_time, before_writeback_time, after_writeback_time );
  }
  else
  {
    if ( !node->parent || !node->data.isskel ) return;

    auto &u_lskel = lchild->data.u_skel;
    auto &u_rskel = rchild->data.u_skel;
    auto &lskel = lchild->data.skels;
    auto &rskel = rchild->data.skels;
    xgemm
    (
      "T", "N",
      u_lskel.row(), u_lskel.col(), proj.row(),
      1.0, proj.data(),    proj.row(),
           u_skel.data(),  u_skel.row(),
      1.0, u_lskel.data(), u_lskel.row()
    );
    xgemm
    (
      "T", "N",
      u_rskel.row(), u_rskel.col(), proj.row(),
      1.0, proj.data() + proj.row() * lskel.size(), proj.row(),
           u_skel.data(), u_skel.row(),
      1.0, u_rskel.data(), u_rskel.row()
    );
  }
  //printf( "\n" );

}; // end SkeletonsToNodes()


template<bool NNPRUNE, typename NODE, typename T>
class SkeletonsToNodesTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      arg = user_arg;
      name = std::string( "s2n" );
      {
        //label = std::to_string( arg->treelist_id );
        std::ostringstream ss;
        ss << arg->treelist_id;
        label = ss.str();
      }

      //--------------------------------------
      double flops = 0.0, mops = 0.0;
      auto &lids = arg->lids;
      auto &data = arg->data;
      auto &proj = data.proj;
      auto &skels = data.skels;
      auto &w = *arg->setup->w;

      if ( arg->isleaf )
      {
        size_t m = proj.col();
        size_t n = w.row();
        size_t k = proj.row();
        flops += 2.0 * m * n * k;
        mops  += 2.0 * ( m * n + n * k + m * k );
      }
      else
      {
        if ( !arg->parent || !arg->data.isskel )
        {
          // No computation.
        }
        else
        {
          size_t m = proj.col();
          size_t n = w.row();
          size_t k = proj.row();
          flops += 2.0 * m * n * k;
          mops  += 2.0 * ( m * n + n * k + m * k );
        }
      }

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** asuume computation bound */
      cost = flops / 1E+9;

      /** low priority */
      priority = true;
    };

    void Prefetch()
    {
      auto &proj = arg->data.proj;
      __builtin_prefetch( proj.data() );
      auto &u_skel = arg->data.u_skel;
      __builtin_prefetch( u_skel.data() );
      if ( arg->isleaf )
      {
        //__builtin_prefetch( arg->data.u_leaf[ 0 ].data() );
        //__builtin_prefetch( arg->data.u_leaf[ 1 ].data() );
        //__builtin_prefetch( arg->data.u_leaf[ 2 ].data() );
        //__builtin_prefetch( arg->data.u_leaf[ 3 ].data() );
      }
      else
      {
        auto &u_lskel = arg->lchild->data.u_skel;
        __builtin_prefetch( u_lskel.data() );
        auto &u_rskel = arg->rchild->data.u_skel;
        __builtin_prefetch( u_rskel.data() );
      }
    };

    void GetEventRecord()
    {
      arg->data.s2n = event;
    };

    void DependencyAnalysis()
    {
#ifdef DEBUG_SPDASKIT
      printf( "Skel2Node DepenencyAnalysis %lu\n", arg->treelist_id );
#endif
      if ( !arg->parent ) 
      {
        this->Enqueue();
        return;
      }

      auto &u_skel = arg->data.u_skel;
      u_skel.DependencyAnalysis( hmlp::ReadWriteType::R, this );

      if ( !arg->isleaf )
      {
        auto &u_lskel = arg->lchild->data.u_skel;
        auto &u_rskel = arg->rchild->data.u_skel;
        u_lskel.DependencyAnalysis( hmlp::ReadWriteType::RW, this );
        u_rskel.DependencyAnalysis( hmlp::ReadWriteType::RW, this );
      }
      else
      {
        /** impose rw dependencies on multiple copies */
        //for ( size_t p = 0; p < 4; p ++ )
        //{
        //  auto &u_leaf = arg->data.u_leaf[ p ];
        //  u_leaf.DependencyAnalysis( hmlp::ReadWriteType::RW, this );
        //}
      }
    };

    void Execute( Worker* user_worker )
    {
      SkeletonsToNodes<NNPRUNE, NODE, T>( arg );
    };

}; // end class SkeletonsToNodesTask



template<int SUBTASKID, bool NNPRUNE, typename NODE, typename T>
void LeavesToLeaves( NODE *node, size_t itbeg, size_t itend )
{
  assert( node->isleaf );

  double beg, kij_s2n_time = 0.0, u_leaf_time, before_writeback_time, after_writeback_time;

  /** gather shared data and create reference */
  auto &K = *node->setup->K;
  auto &w = *node->setup->w;
  auto &u = *node->setup->u;

  auto &lids = node->lids;
  auto &data = node->data;
  auto &amap = node->lids;
  auto &NearKab = data.NearKab;

  std::set<NODE*> *NearNodes;
  if ( NNPRUNE ) NearNodes = &node->NNNearNodes;
  else           NearNodes = &node->NearNodes;

  /** TODO: I think there may be a performance bug here. 
   *        Overall there will be 4 task
   **/
  auto &u_leaf = data.u_leaf[ SUBTASKID ];
  u_leaf.clear();

  /** early return if nothing to do */
  if ( itbeg == itend ) 
  {
    u_leaf.resize( 0, 0 );
    return;
  }
  else
  {
    u_leaf.clear();
    u_leaf.resize( lids.size(), w.row(), 0.0 );
  }

  if ( NearKab.size() ) /** Kab is cached */
  {
    size_t itptr = 0;
    size_t offset = 0;

    for ( auto it = NearNodes->begin(); it != NearNodes->end(); it ++ )
    {
      if ( itbeg <= itptr && itptr < itend )
      {
        auto &bmap = (*it)->lids;
        //auto wb = w( bmap );
        auto wb = (*it)->data.w_leaf;

        /** Kab * wb */
        xgemm
        (
          "N", "N",
          u_leaf.row(), u_leaf.col(), wb.row(),
          1.0, NearKab.data() + offset * NearKab.row(), NearKab.row(),
                    wb.data(),                               wb.row(),
          1.0,  u_leaf.data(),                           u_leaf.row()
        );
      }
      offset += (*it)->lids.size();
      itptr ++;
    }
  }
  else /** TODO: make xgemm into NN instead of NT. Kab is not cached */
  {
    size_t itptr = 0;
    for ( auto it = NearNodes->begin(); it != NearNodes->end(); it ++ )
    {
      if ( itbeg <= itptr && itptr < itend )
      {
        auto &bmap = (*it)->lids;
        auto wb = w( bmap );

        /** evaluate the submatrix */
        beg = omp_get_wtime();
        auto Kab = K( amap, bmap );
        kij_s2n_time = omp_get_wtime() - beg;

        /** update kij counter */
        data.kij_s2n.first  += kij_s2n_time;
        data.kij_s2n.second += amap.size() * bmap.size();

        /** ( Kab * wb )' = wb' * Kab' */
        xgemm
        (
          "N", "T",
          u_leaf.row(), u_leaf.col(), wb.col(),
          1.0,     wb.data(),     wb.row(),
                  Kab.data(),    Kab.row(),
          1.0, u_leaf.data(), u_leaf.row()
        );
      }
      itptr ++;
    }
  }
  before_writeback_time = omp_get_wtime() - beg;

}; /** end LeavesToLeaves() */


template<int SUBTASKID, bool NNPRUNE, typename NODE, typename T>
class LeavesToLeavesTask : public hmlp::Task
{
  public:

    NODE *arg;

    size_t itbeg;

	size_t itend;

    void Set( NODE *user_arg )
    {
      arg = user_arg;
      name = std::string( "l2l" );
      {
        //label = std::to_string( arg->treelist_id );
        std::ostringstream ss;
        ss << arg->treelist_id;
        label = ss.str();
      }

      /** TODO: fill in flops and mops */
      //--------------------------------------
      double flops = 0.0, mops = 0.0;
      auto &lids = arg->lids;
      auto &data = arg->data;
      auto &proj = data.proj;
      auto &skels = data.skels;
      auto &w = *arg->setup->w;

      assert( arg->isleaf );

      size_t m = lids.size();
      size_t n = w.row();

      std::set<NODE*> *NearNodes;
      if ( NNPRUNE ) NearNodes = &arg->NNNearNodes;
      else           NearNodes = &arg->NearNodes;

      /** TODO: need to better decide the range [itbeg itend] */ 
      size_t itptr = 0;
      size_t itrange = ( NearNodes->size() + 3 ) / 4;
      if ( itrange < 1 ) itrange = 1;
      itbeg = ( SUBTASKID - 1 ) * itrange;
      itend = ( SUBTASKID + 0 ) * itrange;
      if ( itbeg > NearNodes->size() ) itbeg = NearNodes->size();
      if ( itend > NearNodes->size() ) itend = NearNodes->size();
      if ( SUBTASKID == 4 ) itend = NearNodes->size();

      for ( auto it = NearNodes->begin(); it != NearNodes->end(); it ++ )
      {
        if ( itbeg <= itptr && itptr < itend )
        {
          size_t k = (*it)->lids.size();
          flops += 2.0 * m * n * k;
          mops += m * k;
          mops += 2.0 * ( m * n + n * k + m * k );
        }
        itptr ++;
      }

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** asuume computation bound */
      cost = flops / 1E+9;
    };

    void Prefetch()
    {
      auto &u_leaf = arg->data.u_leaf[ SUBTASKID ];
      __builtin_prefetch( u_leaf.data() );
    };

    void GetEventRecord()
    {
      /** create l2l event */
      //arg->data.s2n = event;
    };

    void DependencyAnalysis()
    {
      assert( arg->isleaf );

      /** depends on nothing */
      this->Enqueue();

      /** impose rw dependencies on multiple copies */
      //auto &u_leaf = arg->data.u_leaf[ SUBTASKID ];
      //u_leaf.DependencyAnalysis( hmlp::ReadWriteType::W, this );
    };

    void Execute( Worker* user_worker )
    {
      LeavesToLeaves<SUBTASKID, NNPRUNE, NODE, T>( arg, itbeg, itend );
    };

}; /** end class LeavesToLeaves */


template<typename NODE, typename T>
void Evaluate( NODE *node, NODE *target, hmlp::Data<T> &potentials )
{
  auto &w = node->setup->w;
  auto &lids = node->lids;
  auto &K = *node->setup->K;
  auto &data = node->data;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  auto &amap = target->lids;

  if ( potentials.size() != amap.size() * w.row() )
  {
    potentials.resize( amap.size(), w.row(), 0.0 );
  }

  assert( target->isleaf );

  if ( ( node == target ) || ( node->isleaf && !node->isskel ) )
  {
    // direct evaluation
#ifdef DEBUG_SPDASKIT
    printf( "level %lu direct evaluation\n", node->l );
#endif
    auto Kab = K( amap, lids ); // amap.size()-by-lids.size()
    auto wb  = w( lids ); // nrhs-by-lids.size()
    xgemm
    (
      "N", "T",
      Kab.row(), wb.row(), wb.col(),
      1.0, Kab.data(),        Kab.row(),
           wb.data(),         wb.row(),
      1.0, potentials.data(), potentials.row()
    );
  }
  else
  {
    if ( !data.isskel || IsMyParent( target->morton, node->morton ) )
    {
#ifdef DEBUG_SPDASKIT
      printf( "level %lu is not prunable\n", node->l );
#endif
      Evaluate( lchild, target, potentials );      
      Evaluate( rchild, target, potentials );
    }
    else
    {
#ifdef DEBUG_SPDASKIT
      printf( "level %lu is prunable\n", node->l );
#endif
      auto Kab = K( amap, node->data.skels );
      auto &w_skel = node->data.w_skel;
      xgemm
      (
        "N", "N",
        Kab.row(), w_skel.col(), w_skel.row(),
        1.0, Kab.data(),        Kab.row(),
             w_skel.data(),     w_skel.row(),
        1.0, potentials.data(), potentials.row()
      );          
    }
  }

}; // end void Evaluate()


/**
 *  @brief This evaluation evaluate the potentils of the whole target node.
 *
 */ 
template<bool SYMMETRIC, bool NNPRUNE, typename NODE>
void Evaluate( NODE *node, NODE *target )
{
  assert( target->isleaf );

  auto &data = node->data;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  std::set<NODE*> *NearNodes;

  if ( NNPRUNE ) NearNodes = &target->NNNearNodes;
  else           NearNodes = &target->NearNodes;

  //std::cout << data.isskel << std::endl;

  if ( !data.isskel || node->ContainAny( *NearNodes ) )
  {
    if ( node->isleaf )
    {
      // Do notthing, because the NearNodes list was constructed.
    }
    else
    {
      //printf( "%lu not prunes %lu\n", target->treelist_id, node->treelist_id );
      Evaluate<SYMMETRIC, NNPRUNE>( lchild, target );
      Evaluate<SYMMETRIC, NNPRUNE>( rchild, target );
    }
  }
  else
  {
    if ( SYMMETRIC && node->morton < target->morton )
    {
      // Since target->morton is larger than the visiting node,
      // the interaction between the target and this node has
      // been computed. 
      //printf( "symmetric prune\n" );
    }
    else
    {
      if ( NNPRUNE ) 
      {
        //printf( "target %lu has FarNode %lu\n", target->treelist_id, node->treelist_id );
        target->NNFarNodes.insert( node );
      }
      else           
      {
        target->FarNodes.insert( node );
      }
    }
  }
};


template<typename NODE>
void PrintSet( std::set<NODE*> &set )
{
  for ( auto it = set.begin(); it != set.end(); it ++ )
  {
    printf( "%lu, ", (*it)->treelist_id );
  }
  printf( "\n" );
};


/**
 *  @brief Compute those near leaf nodes and build a list. This is just like
 *         the neighbor list but the granularity is in nodes but not points.
 *         The algorithm is to compute the node morton ids of neighbor points.
 *         Get the pointers of these nodes and insert them into a std::set.
 *         std::set will automatic remove duplication. Here the insertion 
 *         will be performed twice each time to get a symmetric one. That is
 *         if alpha has beta in its list, then beta will also have alpha in
 *         its list.
 *
 *         Only leaf nodes will have the list `` NearNodes''.
 *
 *         This list will later be used to get the FarNodes using a recursive
 *         node traversal scheme.
 *  
 */ 
template<bool SYMMETRIC, bool NNPRUNE, typename TREE>
void NearNodes( TREE &tree )
{
  auto &setup = tree.setup;
  auto &NN = *setup.NN;
  size_t n_nodes = 1 << tree.depth;
  auto level_beg = tree.treelist.begin() + n_nodes - 1;

  //printf( "NN( %lu, %lu ) depth %lu n_nodes %lu treelist.size() %lu\n", 
  //    NN.row(), NN.num(),      
  //    tree.depth, n_nodes, tree.treelist.size() );


  // Traverse all leaf nodes. 
  #pragma omp parallel for
  for ( size_t node_ind = 0; node_ind < n_nodes; node_ind ++ )
  {
    auto *node = *(level_beg + node_ind);
    auto &data = node->data;

    // If no skeletons, then add every leaf nodes.
    if ( !node->data.isskel )
    {
      //printf( "leaf %lu not skel\n", node->treelist_id );
      for ( size_t i = 0; i < n_nodes; i ++ )
      {
        if ( NNPRUNE ) node->NNNearNodes.insert( *(level_beg + i) );
        else           node->NearNodes.insert(   *(level_beg + i) );
      }
      //continue;
    }

    if ( NNPRUNE )
    {
      /** add myself to the list. */
      node->NNNearNodes.insert( node );

      /** TODO: have some consensus on the interaction list */
      if ( false )
      {
        for ( auto it = node->data.snids.begin(); it != node->data.snids.end(); it ++ )
        {
          size_t neighbor_lid = tree.Getlid( (*it).second );
          size_t neighbor_morton = setup.morton[ neighbor_lid ];
          node->NNNearNodes.insert( tree.Morton2Node( neighbor_morton ) );
        }
      }
      else
      {
        /** Traverse all points and their neighbors. NN is stored as k-by-N */
        for ( size_t j = 0; j < node->lids.size(); j ++ )
        {
          size_t lid = node->lids[ j ];
          for ( size_t i = 0; i < NN.row(); i ++ )
          {
            size_t neighbor_gid = NN( i, lid ).second;
            //printf( "lid %lu i %lu neighbor_gid %lu\n", lid, i, neighbor_gid );
            size_t neighbor_lid = tree.Getlid( neighbor_gid );
            size_t neighbor_morton = setup.morton[ neighbor_lid ];
            //printf( "neighborlid %lu morton %lu\n", neighbor_lid, neighbor_morton );

            node->NNNearNodes.insert( tree.Morton2Node( neighbor_morton ) );
          }
        }
      }
    }
    else
    {
      // Add myself to the list, and it's done.
      node->NearNodes.insert( node );
    }
  }

  if ( SYMMETRIC && NNPRUNE )
  {
    // Make it symmetric
    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);
      auto &NNNearNodes = node->NNNearNodes;
      for ( auto it = NNNearNodes.begin(); it != NNNearNodes.end(); it ++ )
      {
        (*it)->NNNearNodes.insert( node );
      }
    }
#ifdef DEBUG_SPDASKIT
    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);
      auto &NNNearNodes = node->NNNearNodes;
      printf( "Node %lu NearNodes ", node->treelist_id );
      for ( auto it = NNNearNodes.begin(); it != NNNearNodes.end(); it ++ )
      {
        printf( "%lu, ", (*it)->treelist_id );
      }
      printf( "\n" );
    }
#endif
  }
};



template<bool SYMMETRIC, bool NNPRUNE, typename TREE>
class NearNodesTask : public hmlp::Task
{
  public:

    TREE *arg;

    void Set( TREE *user_arg )
    {
      arg = user_arg;
      name = std::string( "near" );

      //--------------------------------------
      double flops = 0.0, mops = 0.0;

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** asuume computation bound */
      cost = 1.0;

      /** low priority */
      priority = true;
    }

    void DependencyAnalysis()
    {
      TREE &tree = *arg;
      size_t n_nodes = 1 << tree.depth;
      auto level_beg = tree.treelist.begin() + n_nodes - 1;
      for ( size_t node_ind = 0; node_ind < n_nodes; node_ind ++ )
      {
        auto *node = *(level_beg + node_ind);
        node->DependencyAnalysis( hmlp::ReadWriteType::R, this );
      }
    };

    void Execute( Worker* user_worker )
    {
      NearNodes<SYMMETRIC, NNPRUNE, TREE>( *arg );
    };

}; /** class NearNodesTask */


template<bool NNPRUNE, typename NODE>
class CacheNearNodesTask : public hmlp::Task
{
  public:

    NODE *arg;

    void Set( NODE *user_arg )
    {
      arg = user_arg;
      name = std::string( "c-n" );
      {
        //label = std::to_string( arg->treelist_id );
        std::ostringstream ss;
        ss << arg->treelist_id;
        label = ss.str();
      }

      double flops = 0.0, mops = 0.0;

      /** setup the event */
      event.Set( label + name, flops, mops );

      /** asuume computation bound */
      cost = 1.0;
    };

    void DependencyAnalysis()
    {
      arg->DependencyAnalysis( hmlp::ReadWriteType::RW, this );
    };

    void Execute( Worker* user_worker )
    {
      NODE *node = arg;
      auto *NearNodes = &node->NearNodes;
      if ( NNPRUNE ) NearNodes = &node->NNNearNodes;
      auto &K = *node->setup->K;
      auto &data = node->data;
      auto &amap = node->lids;
      std::vector<size_t> bmap;
      for ( auto it = NearNodes->begin(); it != NearNodes->end(); it ++ )
      {
        bmap.insert( bmap.end(), (*it)->lids.begin(), (*it)->lids.end() );
      }
      data.NearKab = K( amap, bmap );
    };
}; 





/**
 *  TODO: change to task.
 *
 */
template<bool SYMMETRIC, bool NNPRUNE, typename TREE>
void FarNodes( TREE &tree )
{
  for ( int l = tree.depth; l >= 0; l -- )
  {
    std::size_t n_nodes = 1 << l;
    auto level_beg = tree.treelist.begin() + n_nodes - 1;

    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);

      // If I don't have any skeleton, then I'm nobody's far field.
      if ( !node->data.isskel ) continue;

      if ( node->isleaf )
      {
        Evaluate<SYMMETRIC, NNPRUNE>( tree.treelist[ 0 ], node );
        //printf( "Leaf nodes evaluate\n" );
      }
      else
      {
        // Merging FarNodes from children
        auto *lchild = node->lchild;
        auto *rchild = node->rchild;
        if ( NNPRUNE )
        {
          auto &pFarNodes =   node->NNFarNodes;
          auto &lFarNodes = lchild->NNFarNodes;
          auto &rFarNodes = rchild->NNFarNodes;

          for ( auto it = lFarNodes.begin(); it != lFarNodes.end(); ++ it )
          {
            if ( rFarNodes.count( *it ) ) pFarNodes.insert( *it );
          }
          for ( auto it = pFarNodes.begin(); it != pFarNodes.end(); it ++ )
          {
            lFarNodes.erase( *it );
            rFarNodes.erase( *it );
          }

        }
        else
        {
          auto &pFarNodes =   node->FarNodes;
          auto &lFarNodes = lchild->FarNodes;
          auto &rFarNodes = rchild->FarNodes;
          for ( auto it = lFarNodes.begin(); it != lFarNodes.end(); it ++ )
          {
            if ( rFarNodes.count( *it ) ) pFarNodes.insert( *it );
          }
          for ( auto it = pFarNodes.begin(); it != pFarNodes.end(); it ++ )
          {
            lFarNodes.erase( *it );
            rFarNodes.erase( *it );
          }
        }
      }
    }
  }

  if ( SYMMETRIC )
  {
    // Symmetrinize FarNodes to FarNodes interaction.
    for ( int l = tree.depth; l >= 0; l -- )
    {
      std::size_t n_nodes = 1 << l;
      auto level_beg = tree.treelist.begin() + n_nodes - 1;

      for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
      {
        auto *node = *(level_beg + node_ind);
        auto &pFarNodes =   node->NNFarNodes;
        for ( auto it = pFarNodes.begin(); it != pFarNodes.end(); it ++ )
        {
          (*it)->NNFarNodes.insert( node );
        }
      }
    }
  }
  
#ifdef DEBUG_SPDASKIT
  for ( int l = tree.depth; l >= 0; l -- )
  {
    std::size_t n_nodes = 1 << l;
    auto level_beg = tree.treelist.begin() + n_nodes - 1;

    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);
      auto &pFarNodes =   node->NNFarNodes;
      for ( auto it = pFarNodes.begin(); it != pFarNodes.end(); it ++ )
      {
        if ( !( (*it)->NNFarNodes.count( node ) ) )
        {
          printf( "Unsymmetric FarNodes %lu, %lu\n", node->treelist_id, (*it)->treelist_id );
          printf( "Near\n" );
          PrintSet(  node->NNNearNodes );
          PrintSet( (*it)->NNNearNodes );
          printf( "Far\n" );
          PrintSet(  node->NNFarNodes );
          PrintSet( (*it)->NNFarNodes );
          printf( "======\n" );
          break;
        }
      }
      if ( pFarNodes.size() )
      {
        printf( "l %2lu FarNodes(%lu) ", node->l, node->treelist_id );
        PrintSet( pFarNodes );
      }
    }
  }
#endif
};


/**
 *  @brief Create a list of near nodes for  each leave (direct evaluation).
 *         Create a list of far nodes for each node (low-rank).
 *
 *         If ( SYMMETRIC ) Both near and far lists are symmetric.
 *         If ( NNPRUNE )   All neighbors are near nodes. Lists stored in
 *                          NNNearNodes and NNFarNodes.
 *         Else             Only the leave itself is the near node. Use
 *                          NearNodes and FarNodes.
 *        
 */ 
template<bool SYMMETRIC, bool NNPRUNE, bool CACHE = true, typename NODE, typename TREE>
void NearFarNodes( TREE &tree )
{
  //printf( "Enter NearNodes\n" );
  //NearNodes<SYMMETRIC, NNPRUNE>( tree );

  //auto *nearnodestask = new NearNodesTask<SYMMETRIC, NNPRUNE, TREE>();
  //nearnodestask->Set( &tree );
  //nearnodestask->Execute( NULL );

  //nearnodestaks->Enqueue();

  //printf( "Finish NearNodes\n" );
  FarNodes<SYMMETRIC, NNPRUNE>( tree );

  /** reserve space for u_leaf */
  #pragma omp parallel for schedule( dynamic )
  for ( size_t i = 0; i < tree.treelist.size(); i ++ )
  {
    auto *node = tree.treelist[ i ];
    if ( node->isleaf )
    {
      node->data.u_leaf[ 0 ].reserve( 1024, node->lids.size() );
    }
  }

  /** cache Kab by request */
  if ( CACHE )
  {
    /** cache FarKab */
    #pragma omp parallel for schedule( dynamic )
    for ( size_t i = 0; i < tree.treelist.size(); i ++ )
    {
      auto *node = tree.treelist[ i ];
      auto *FarNodes = &node->FarNodes;
      if ( NNPRUNE ) FarNodes = &node->NNFarNodes;
      auto &K = *node->setup->K;
      auto &data = node->data;
      auto &amap = data.skels;
      std::vector<size_t> bmap;
      for ( auto it = FarNodes->begin(); it != FarNodes->end(); it ++ )
      {
        bmap.insert( bmap.end(), (*it)->data.skels.begin(), 
                                 (*it)->data.skels.end() );
      }
      data.FarKab = K( amap, bmap );
    }

    /** cache NearKab */
    //#pragma omp parallel for schedule( dynamic )
    //for ( size_t i = 0; i < tree.treelist.size(); i ++ )
    //{
    //  auto *node = tree.treelist[ i ];
    //  auto *NearNodes = &node->NearNodes;
    //  if ( NNPRUNE ) NearNodes = &node->NNNearNodes;
    //  auto &K = *node->setup->K;
    //  auto &data = node->data;
    //  auto &amap = node->lids;
    //  std::vector<size_t> bmap;
    //  for ( auto it = NearNodes->begin(); it != NearNodes->end(); it ++ )
    //  {
    //    bmap.insert( bmap.end(), (*it)->lids.begin(), (*it)->lids.end() );
    //  }
    //  data.NearKab = K( amap, bmap );
    //}

    //const bool AUTO_DEPENDENCY = false;
    //const bool USE_RUNTIME= false;
    //CacheNearNodesTask<NNPRUNE, NODE> cachenearnodestask;
    //tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( cachenearnodestask );
    //if ( USE_RUNTIME ) hmlp_run();

  }

}; // end void NearFarNodes()


template<bool NNPRUNE, typename TREE>
double DrawInteraction( TREE &tree )
{
  double exact_ratio = 0.0;
  FILE * pFile;
  //int n;
  char name[ 100 ];

  pFile = fopen ( "interaction.m", "w" );

  fprintf( pFile, "figure('Position',[100,100,800,800]);" );
  fprintf( pFile, "hold on;" );
  fprintf( pFile, "axis square;" );
  fprintf( pFile, "axis ij;" );

  for ( int l = tree.depth; l >= 0; l -- )
  {
    std::size_t n_nodes = 1 << l;
    auto level_beg = tree.treelist.begin() + n_nodes - 1;

    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);

      if ( NNPRUNE )
      {
        auto &pNearNodes = node->NNNearNodes;
        auto &pFarNodes = node->NNFarNodes;
        for ( auto it = pFarNodes.begin(); it != pFarNodes.end(); it ++ )
        {
          double gb = (double)std::min( node->l, (*it)->l ) / tree.depth;
          //printf( "node->l %lu (*it)->l %lu depth %lu\n", node->l, (*it)->l, tree.depth );
          fprintf( pFile, "rectangle('position',[%lu %lu %lu %lu],'facecolor',[1.0,%lf,%lf]);\n",
              node->offset,      (*it)->offset,
              node->lids.size(), (*it)->lids.size(),
              gb, gb );
        }
        for ( auto it = pNearNodes.begin(); it != pNearNodes.end(); it ++ )
        {
          fprintf( pFile, "rectangle('position',[%lu %lu %lu %lu],'facecolor',[0.2,0.4,1.0]);\n",
              node->offset,      (*it)->offset,
              node->lids.size(), (*it)->lids.size() );

          /** accumulate exact evaluation */
          exact_ratio += node->lids.size() * (*it)->lids.size();
        }  
      }
      else
      {
      }
    }
  }
  fprintf( pFile, "hold off;" );
  fclose( pFile );

  return exact_ratio / ( tree.n * tree.n );
}; // end DrawInteration()


/**
 *  @breif This is a fake evaluation setup aimming to figure out
 *         which tree node will prun which points. The result
 *         will be stored in each node as two lists, prune and noprune.
 *
 */ 
template<bool SYMBOLIC, bool NNPRUNE, typename NODE, typename T>
void Evaluate
( 
  NODE *node, 
  size_t lid, 
  std::vector<size_t> &nnandi, // k + 1 non-prunable lists
  hmlp::Data<T> &potentials 
)
{
  auto &K = *node->setup->K;
  auto &w = *node->setup->w;
  auto &lids = node->lids;
  auto &data = node->data;
  auto *lchild = node->lchild;
  auto *rchild = node->rchild;

  auto amap = std::vector<size_t>( 1 );

  amap[ 0 ] = lid;

  if ( !SYMBOLIC ) // No potential evaluation.
  {
    assert( potentials.size() == amap.size() * w.row() );
  }

  if ( !data.isskel || node->ContainAny( nnandi ) )
  {
    //printf( "level %lu is not prunable\n", node->l );
    if ( node->isleaf )
    {
      if ( SYMBOLIC )
      {
        data.lock.Acquire();
        {
          // Add lid to notprune list. We use a lock.
          if ( NNPRUNE ) 
          {
            node->NNNearIDs.insert( lid );
          }
          else           
          {
            node->NearIDs.insert( lid );
          }
        }
        data.lock.Release();
      }
      else
      {
#ifdef DEBUG_SPDASKIT
        printf( "level %lu direct evaluation\n", node->l );
#endif
        auto Kab = K( amap, lids ); // amap.size()-by-lids.size()
        auto wb  = w( lids ); // nrhs-by-lids.size()
        xgemm
        (
          "N", "T",
          Kab.row(), wb.row(), wb.col(),
          1.0, Kab.data(),        Kab.row(),
          wb.data(),         wb.row(),
          1.0, potentials.data(), potentials.row()
        );
      }
    }
    else
    {
      Evaluate<SYMBOLIC, NNPRUNE>( lchild, lid, nnandi, potentials );      
      Evaluate<SYMBOLIC, NNPRUNE>( rchild, lid, nnandi, potentials );
    }
  }
  else // need lid's morton and neighbors' mortons
  {
    //printf( "level %lu is prunable\n", node->l );
    if ( SYMBOLIC )
    {
      data.lock.Acquire();
      {
        // Add lid to prunable list.
        if ( NNPRUNE ) 
        {
          node->FarIDs.insert( lid );
        }
        else           
        {
          node->NNFarIDs.insert( lid );
        }
      }
      data.lock.Release();
    }
    else
    {
#ifdef DEBUG_SPDASKIT
      printf( "level %lu is prunable\n", node->l );
#endif
      auto Kab = K( amap, node->data.skels );
      auto &w_skel = node->data.w_skel;
      xgemm
      (
        "N", "N",
        Kab.row(), w_skel.col(), w_skel.row(),
        1.0, Kab.data(),        Kab.row(),
        w_skel.data(),     w_skel.row(),
        1.0, potentials.data(), potentials.row()
      );          
    }
  }



}; // end T Evaluate()


template<bool SYMBOLIC, bool NNPRUNE, typename TREE, typename T>
void Evaluate
( 
  TREE &tree, 
  size_t gid, 
  hmlp::Data<T> &potentials
)
{
  std::vector<size_t> nnandi;
  auto &w = *tree.setup.w;
  size_t lid = tree.Getlid( gid );

  potentials.clear();
  potentials.resize( 1, w.row(), 0.0 );

  if ( NNPRUNE )
  {
    auto &NN = *tree.setup.NN;
    nnandi.reserve( NN.row() + 1 );
    nnandi.push_back( lid );
    for ( size_t i = 0; i < NN.row(); i ++ )
    {
      nnandi.push_back( tree.Getlid( NN[ lid * NN.row() + i ].second ) );
    }
#ifdef DEBUG_SPDASKIT
    printf( "nnandi.size() %lu\n", nnandi.size() );
#endif
  }
  else
  {
    nnandi.reserve( 1 );
    nnandi.push_back( lid );
  }

  Evaluate<SYMBOLIC, NNPRUNE>( tree.treelist[ 0 ], lid, nnandi, potentials );

}; // end Evaluate()


template<
bool USE_RUNTIME, bool USE_OMP_TASK, 
bool SYMMETRIC_PRUNE, bool NNPRUNE, 
typename NODE, typename TREE, typename T>
hmlp::Data<T> ComputeAll
( 
  TREE &tree,
  hmlp::Data<T> &weights
)
{
  const bool AUTO_DEPENDENCY = true;

  double beg, computeall_time = 0.0, overhead_time = 0.0;

  hmlp::Data<T> potentials( weights.row(), weights.col() );

  tree.setup.w = &weights;
  tree.setup.u = &potentials;

  if ( SYMMETRIC_PRUNE )
  {
    //using LEAFTOLEAFTASK = LeavesToLeavesTask<NNPRUNE, NODE, T>;
    using LEAFTOLEAFTASK1 = LeavesToLeavesTask<1, NNPRUNE, NODE, T>;
    using LEAFTOLEAFTASK2 = LeavesToLeavesTask<2, NNPRUNE, NODE, T>;
    using LEAFTOLEAFTASK3 = LeavesToLeavesTask<3, NNPRUNE, NODE, T>;
    using LEAFTOLEAFTASK4 = LeavesToLeavesTask<4, NNPRUNE, NODE, T>;

    using NODETOSKELTASK  = UpdateWeightsTask<NODE>;
    using SKELTOSKELTASK  = SkeletonsToSkeletonsTask<NNPRUNE, NODE>;
    using SKELTONODETASK  = SkeletonsToNodesTask<NNPRUNE, NODE, T>;

    //LEAFTOLEAFTASK leaftoleaftask;
    LEAFTOLEAFTASK1 leaftoleaftask1;
    LEAFTOLEAFTASK2 leaftoleaftask2;
    LEAFTOLEAFTASK3 leaftoleaftask3;
    LEAFTOLEAFTASK4 leaftoleaftask4;

    NODETOSKELTASK  nodetoskeltask;
    SKELTOSKELTASK  skeltoskeltask;
    SKELTONODETASK  skeltonodetask;

    beg = omp_get_wtime();

    int n_nodes = 1 << tree.depth;
    auto level_beg = tree.treelist.begin() + n_nodes - 1;

    #pragma omp parallel for
    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);
      node->data.w_leaf = weights.GatherColumns<true>( node->lids );
    }

    if ( USE_OMP_TASK )
    {
      assert( !USE_RUNTIME );
      /** TODO: traverse leaf here */
      //tree.template TraverseLeafs<false, false>( leaftoleaftask );
      tree.template TraverseLeafs<false, false>( leaftoleaftask1 );
      tree.template TraverseLeafs<false, false>( leaftoleaftask2 );
      tree.template TraverseLeafs<false, false>( leaftoleaftask3 );
      tree.template TraverseLeafs<false, false>( leaftoleaftask4 );

      tree.template UpDown<true, true, true>( nodetoskeltask, skeltoskeltask, skeltonodetask );
    }
    else
    {
      assert( !USE_OMP_TASK );
      /** TODO: traverse leaf here */
      tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( leaftoleaftask1 );
      tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( leaftoleaftask2 );
      tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( leaftoleaftask3 );
      tree.template TraverseLeafs<AUTO_DEPENDENCY, USE_RUNTIME>( leaftoleaftask4 );

      //printf( "task creating done\n" ); fflush( stdout );
      //if ( USE_RUNTIME ) hmlp_run();
      //printf( "here\n" ); fflush( stdout );

      tree.template TraverseUp<AUTO_DEPENDENCY, USE_RUNTIME>( nodetoskeltask );
      //if ( USE_RUNTIME ) hmlp_run();
      //printf( "UpdateWeights\n" );
      tree.template TraverseUnOrdered<AUTO_DEPENDENCY, USE_RUNTIME>( skeltoskeltask );
      //if ( USE_RUNTIME ) hmlp_run();
      //printf( "Skel2Skel\n" );
      
      tree.template TraverseDown<AUTO_DEPENDENCY, USE_RUNTIME>( skeltonodetask );
      overhead_time = omp_get_wtime() - beg;
      if ( USE_RUNTIME ) hmlp_run();

    }

    /** reduce direct iteractions from 4 copies */
    #pragma omp parallel for
    for ( int node_ind = 0; node_ind < n_nodes; node_ind ++ )
    {
      auto *node = *(level_beg + node_ind);
      auto &amap = node->lids;
      auto &u_leaf = node->data.u_leaf[ 0 ];

      /** reduce all u_leaf[0:4] */
      for ( size_t p = 1; p < 5; p ++ )
        for ( size_t i = 0; i < node->data.u_leaf[ p ].size(); i ++ )
          u_leaf[ i ] += node->data.u_leaf[ p ][ i ];

      /** assemble u_leaf back to u */
      for ( size_t j = 0; j < amap.size(); j ++ )
        for ( size_t i = 0; i < potentials.row(); i ++ )
          potentials[ amap[ j ] * potentials.row() + i ] = u_leaf( j, i );
    }
    computeall_time = omp_get_wtime() - beg;

    printf( "ComputeAll %5.2lfs (overhead %5.2lfs)\n", computeall_time, overhead_time ); fflush( stdout );
  }
  else // TODO: implement unsymmetric prunning
  {
    using NODETOSKELTASK = UpdateWeightsTask<NODE>;

    NODETOSKELTASK nodetoskeltask;

    tree.template TraverseUp<false, USE_RUNTIME>( nodetoskeltask );
    if ( USE_RUNTIME ) hmlp_run();

    // Not yet implemented.
    printf( "Non symmetric ComputeAll is not yet implemented\n" );
  }

  return potentials;
}; // end ComputeAll()




template<typename NODE, typename T>
void ComputeError( NODE *node, hmlp::Data<T> potentials )
{
  auto &K = *node->setup->K;
  auto &w = node->setup->w;
  auto &lids = node->lids;

  auto &amap = node->lids;
  std::vector<size_t> bmap = std::vector<size_t>( K.col() );

  for ( size_t j = 0; j < bmap.size(); j ++ ) bmap[ j ] = j;

  auto Kab = K( amap, bmap );

  auto nrm2 = hmlp_norm( potentials.row(), potentials.col(), 
                         potentials.data(), potentials.row() ); 

  xgemm
  (
    "N", "T",
    Kab.row(), w.row(), w.col(),
    -1.0, Kab.data(),        Kab.row(),
          w.data(),          w.row(),
     1.0, potentials.data(), potentials.row()
  );          

  auto err = hmlp_norm( potentials.row(), potentials.col(), 
                        potentials.data(), potentials.row() ); 
  
  printf( "node relative error %E, nrm2 %E\n", err / nrm2, nrm2 );


}; // end void ComputeError()








/**
 *  @brief
 */ 
template<typename TREE, typename T>
T ComputeError( TREE &tree, size_t gid, hmlp::Data<T> potentials )
{
  auto &K = *tree.setup.K;
  auto &w = *tree.setup.w;
  auto lid = tree.Getlid( gid );

  auto amap = std::vector<size_t>( 1 );
  auto bmap = std::vector<size_t>( K.col() );
  amap[ 0 ] = lid;
  for ( size_t j = 0; j < bmap.size(); j ++ ) bmap[ j ] = j;

  auto Kab = K( amap, bmap );
  auto exact = potentials;

  xgemm
  (
    "N", "T",
    Kab.row(), w.row(), w.col(),
    1.0, Kab.data(),        Kab.row(),
          w.data(),          w.row(),
    0.0, exact.data(), exact.row()
  );          


  auto nrm2 = hmlp_norm( exact.row(),  exact.col(), 
                         exact.data(), exact.row() ); 

  xgemm
  (
    "N", "T",
    Kab.row(), w.row(), w.col(),
    -1.0, Kab.data(),        Kab.row(),
          w.data(),          w.row(),
     1.0, potentials.data(), potentials.row()
  );          

  auto err = hmlp_norm( potentials.row(), potentials.col(), 
                        potentials.data(), potentials.row() ); 

  return err / nrm2;
}; // end T ComputeError()




}; // end namespace spdaskit
}; // end namespace hmlp


#endif // define SPDASKIT_HPP
