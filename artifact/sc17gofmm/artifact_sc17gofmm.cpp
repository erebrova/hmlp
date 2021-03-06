#include <tuple>
#include <cmath>
#include <algorithm>
#include <stdio.h>
#include <iomanip>
#include <stdlib.h>
#include <omp.h>
#include <math.h>
#include <limits>


#ifdef HMLP_AVX512
/** this is for hbw_malloc() and hnw_free */
#include <hbwmalloc.h>
/** we need hbw::allocator<T> to replace std::allocator<T> */
#include <hbw_allocator.h>
/** MKL headers */
#include <mkl.h>
#endif


/** GOFMM templates */
#include <gofmm/gofmm.hpp>
/** use an implicit kernel matrix (only coordinates are stored) */
#include <containers/KernelMatrix.hpp>
/** use an implicit matrix */
#include <containers/VirtualMatrix.hpp>


#ifdef HMLP_USE_CUDA
#include <hmlp_gpu.hpp>
#endif

#define GFLOPS 1073741824 
#define TOLERANCE 1E-13

/** by default, we use binary tree */
#define N_CHILDREN 2

using namespace hmlp::tree;
using namespace hmlp::gofmm;



template<
  bool        ADAPTIVE, 
  bool        LEVELRESTRICTION, 
  typename    SPLITTER, 
  typename    RKDTSPLITTER, 
  typename    T, 
  typename    SPDMATRIX>
void test_gofmm
( 
  hmlp::Data<T> *X,
  SPDMATRIX &K, 
  hmlp::Data<std::pair<T, std::size_t>> &NN,
  DistanceMetric metric,
  SPLITTER splitter, 
  RKDTSPLITTER rkdtsplitter,
  size_t n, size_t m, size_t k, size_t s, 
  double stol, double budget, size_t nrhs 
)
{
  /** instantiation for the Spd-Askit tree */
  using SETUP = hmlp::gofmm::Setup<SPDMATRIX, SPLITTER, T>;
  using DATA  = hmlp::gofmm::Data<T>;
  using NODE  = Node<SETUP, N_CHILDREN, DATA, T>;
 
  /** all timers */
  double beg, dynamic_time, omptask45_time, omptask_time, ref_time;
  double ann_time, tree_time, overhead_time;
  double nneval_time, nonneval_time, fmm_evaluation_time, symbolic_evaluation_time;

  const bool CACHE = true;

	/** creatgin configuration for all user-define arguments */
	Configuration<T> config( metric, n, m, k, s, stol, budget );

  /** compress K */
  auto *tree_ptr = Compress<ADAPTIVE, LEVELRESTRICTION, SPLITTER, RKDTSPLITTER, T>
  ( X, K, NN, //metric, 
		splitter, rkdtsplitter, //n, m, k, s, stol, budget, 
	  config );
	auto &tree = *tree_ptr;

  /** Evaluate u ~ K * w */
  hmlp::Data<T> w( nrhs, n ); w.rand();
  auto u = Evaluate<true, false, true, true, CACHE>( tree, w );

  /** examine accuracy with 3 setups, ASKIT, HODLR, and GOFMM */
  std::size_t ntest = 100;
  T nnerr_avg = 0.0;
  T nonnerr_avg = 0.0;
  T fmmerr_avg = 0.0;
  printf( "========================================================\n");
  printf( "Accuracy report\n" );
  printf( "========================================================\n");
  for ( size_t i = 0; i < ntest; i ++ )
  {
    hmlp::Data<T> potentials;
    /** ASKIT treecode with NN pruning */
    Evaluate<false, true>( tree, i, potentials );
    auto nnerr = ComputeError( tree, i, potentials );
    /** ASKIT treecode without NN pruning */
    Evaluate<false, false>( tree, i, potentials );
    auto nonnerr = ComputeError( tree, i, potentials );
    /** get results from GOFMM */
    for ( size_t p = 0; p < potentials.col(); p ++ )
    {
      potentials[ p ] = u( p, i );
    }
    auto fmmerr = ComputeError( tree, i, potentials );

    /** only print 10 values. */
    if ( i < 10 )
    {
#ifdef DUMP_ANALYSIS_DATA
      printf( "@DATA\n" );
      printf( "%5lu, %E, %E\n", i, nnerr, nonnerr );
#endif
      printf( "gid %6lu, ASKIT %3.1E, HODLR %3.1E, GOFMM %3.1E\n", 
          i, nnerr, nonnerr, fmmerr );
    }
    nnerr_avg += nnerr;
    nonnerr_avg += nonnerr;
    fmmerr_avg += fmmerr;
  }
  printf( "========================================================\n");
  printf( "            ASKIT %3.1E, HODLR %3.1E, GOFMM %3.1E\n", 
      nnerr_avg / ntest , nonnerr_avg / ntest, fmmerr_avg / ntest );
  printf( "========================================================\n");
  // ------------------------------------------------------------------------


	/** delete tree_ptr */
  delete tree_ptr;

}; /** end test_gofmm() */


/**
 *  @brief Instantiate the splitters here.
 */ 
template<bool ADAPTIVE, bool LEVELRESTRICTION, typename T, typename SPDMATRIX>
void test_gofmm_setup
( 
  hmlp::Data<T> *X,
  SPDMATRIX &K, 
  hmlp::Data<std::pair<T, std::size_t>> &NN,
  DistanceMetric metric,
  size_t n, size_t m, size_t k, size_t s, 
  double stol, double budget, size_t nrhs
)
{
  switch ( metric )
  {
    case GEOMETRY_DISTANCE:
    {
      assert( X );
			/** using geometric splitters from hmlp::tree */
      using SPLITTER     = hmlp::tree::centersplit<N_CHILDREN, T>;
      using RKDTSPLITTER = hmlp::tree::randomsplit<N_CHILDREN, T>;
			/** GOFMM tree splitter */
      SPLITTER splitter;
      splitter.Coordinate = X;
			/** randomized tree splitter */
      RKDTSPLITTER rkdtsplitter;
      rkdtsplitter.Coordinate = X;
      test_gofmm<ADAPTIVE, LEVELRESTRICTION, SPLITTER, RKDTSPLITTER, T>
      ( X, K, NN, metric, splitter, rkdtsplitter, n, m, k, s, stol, budget, nrhs );
      break;
    }
    case KERNEL_DISTANCE:
    case ANGLE_DISTANCE:
    {
			/** using geometric-oblivious splitters from hmlp::gofmm */
      using SPLITTER     = hmlp::gofmm::centersplit<SPDMATRIX, N_CHILDREN, T>;
      using RKDTSPLITTER = hmlp::gofmm::randomsplit<SPDMATRIX, N_CHILDREN, T>;
			/** GOFMM tree splitter */
      SPLITTER splitter;
      splitter.Kptr = &K;
			splitter.metric = metric;
			/** randomized tree splitter */
      RKDTSPLITTER rkdtsplitter;
      rkdtsplitter.Kptr = &K;
			rkdtsplitter.metric = metric;
      test_gofmm<ADAPTIVE, LEVELRESTRICTION, SPLITTER, RKDTSPLITTER, T>
      ( X, K, NN, metric, splitter, rkdtsplitter, n, m, k, s, stol, budget, nrhs );
      break;
    }
    default:
    {
      exit( 1 );
    }
  }
}; /** end test_gofmm_setup() */



/**
 *  @brief Top level driver that reads arguments from the command line.
 */ 
int main( int argc, char *argv[] )
{
  printf( "\n--- Artifact of GOFMM for Super Computing 2017\n" );

  /** default adaptive scheme */
  const bool ADAPTIVE = true;
  const bool LEVELRESTRICTION = false;

  /** default geometric-oblivious scheme */
  DistanceMetric metric = ANGLE_DISTANCE;

  /** test suit options */
  const bool RANDOMMATRIX = true;
  const bool USE_LOWRANK = true;
  const bool SPARSETESTSUIT = false;

  /** default data directory */
  std::string DATADIR( "/" );

  /** default precision */
  using T = float;

  /** read all parameters */
  size_t n, m, d, k, s, nrhs;
  double stol, budget;

	/** (optional) */
  size_t nnz; 
	std::string distance_type;
	std::string spdmatrix_type;
  std::string user_matrix_filename;
  std::string user_points_filename;

  /** (optional) set the default Gaussian kernel bandwidth */
  float h = 1.0;

  /** number of columns and rows, i.e. problem size */
  sscanf( argv[ 1 ], "%lu", &n );

  /** on-diagonal block size, such that the tree has log(n/m) levels */
  sscanf( argv[ 2 ], "%lu", &m );

  /** number of neighbors to use */
  sscanf( argv[ 3 ], "%lu", &k );

  /** maximum off-diagonal ranks */
  sscanf( argv[ 4 ], "%lu", &s );

  /** number of right hand sides */
  sscanf( argv[ 5 ], "%lu", &nrhs );

  /** desired approximation accuracy */
  sscanf( argv[ 6 ], "%lf", &stol );

  /** the maximum percentage of direct matrix-multiplication */
  sscanf( argv[ 7 ], "%lf", &budget );

	/** specify distance type */
	distance_type = argv[ 8 ];

	if ( !distance_type.compare( "geometry" ) )
	{
    printf( "--- using geometry distance\n" );
    metric = GEOMETRY_DISTANCE;
	}
	else if ( !distance_type.compare( "kernel" ) )
	{
    printf( "--- using Gram vector distance\n" );
    metric = KERNEL_DISTANCE;
	}
	else if ( !distance_type.compare( "angle" ) )
	{
    printf( "--- using Gram vector consine similarity\n" );
    metric = ANGLE_DISTANCE;
	}
	else
	{
		printf( "%s is not supported\n", argv[ 9 ] );
		exit( 1 );
	}


	/** specify what kind of spdmatrix is used */
  spdmatrix_type = argv[ 9 ];
  printf( "--- mode (%s)\n", spdmatrix_type.data() );

	if ( !spdmatrix_type.compare( "testsuit" ) )
	{
		/** do nothing */
	}
	else if ( !spdmatrix_type.compare( "userdefine" ) )
	{
		/** do nothing */
	}
	else if ( !spdmatrix_type.compare( "dense" ) )
	{
    /** (optional) provide the path to the matrix file */
    user_matrix_filename = argv[ 10 ];
    printf( "--- dense binary matrix file (%s)\n", user_matrix_filename.data() );
    if ( argc > 11 ) 
    {
      /** (optional) provide the path to the data file */
      user_points_filename = argv[ 11 ];
		  /** dimension of the data set */
      sscanf( argv[ 12 ], "%lu", &d );
      printf( "--- with auxilary points in %lu dimensions (%s)\n", 
          d, user_points_filename.data() );
    }
	}
	else if ( !spdmatrix_type.compare( "kernel" ) )
	{
    user_points_filename = argv[ 10 ];
		/** number of attributes (dimensions) */
    sscanf( argv[ 11 ], "%lu", &d );
		/** (optional) provide Gaussian kernel bandwidth */
    if ( argc > 12 ) sscanf( argv[ 12 ], "%f", &h );
    printf( "--- Gaussian kernel matrix (h=%5.2f) in %lu dimensions (%s)\n",
        h, d, user_points_filename.data() );
	}
	else
	{
		printf( "--- mode (%s) is not supported\n", argv[ 9 ] );
		exit( 1 );
	}

  /** HMLP API call to initialize the runtime */
  hmlp_init();
  printf( "--- runtime system initialization\n" );

  /** run the matrix file provided by users */
  if ( !spdmatrix_type.compare( "dense" ) && user_matrix_filename.size() )
  {
    {
      /** dense spd matrix format */
      hmlp::gofmm::SPDMatrix<T> K;
      K.resize( n, n );
      K.read( n, n, user_matrix_filename );
      /** (optional) provide neighbors, leave uninitialized otherwise */
      hmlp::Data<std::pair<T, std::size_t>> NN;
			/** (optional) provide coordinates */
      if ( user_points_filename.size() )
      {
        hmlp::Data<T> X( d, n, user_points_filename );
        test_gofmm_setup<ADAPTIVE, LEVELRESTRICTION, T>
        ( &X, K, NN, metric, n, m, k, s, stol, budget, nrhs );
      }
      else
      {
        hmlp::Data<T> *X = NULL;
        test_gofmm_setup<ADAPTIVE, LEVELRESTRICTION, T>
        ( X, K, NN, metric, n, m, k, s, stol, budget, nrhs );
      }
    }
  }


  /** generate a Gaussian kernel matrix from the coordinates */
  if ( !spdmatrix_type.compare( "kernel" ) && user_points_filename.size() )
  {
    {
      /** read the coordinates from the file */
      hmlp::Data<T> X( d, n, user_points_filename );
      /** setup the kernel object as Gaussian */
      kernel_s<T> kernel;
      kernel.type = KS_GAUSSIAN;
      kernel.scal = -0.5 / ( h * h );
      /** spd kernel matrix format (implicitly create) */
      hmlp::KernelMatrix<T> K( n, n, d, kernel, X );
      /** (optional) provide neighbors, leave uninitialized otherwise */
      hmlp::Data<std::pair<T, std::size_t>> NN;
      /** routine */
      test_gofmm_setup<ADAPTIVE, LEVELRESTRICTION, T>
      ( &X, K, NN, metric, n, m, k, s, stol, budget, nrhs );
    }
  }


  /** create a random spd matrix, which is diagonal-dominant */
	if ( !spdmatrix_type.compare( "testsuit" ) && RANDOMMATRIX )
  {
		{
			/** no geometric coordinates provided */
			hmlp::Data<T> *X = NULL;
			/** dense spd matrix format */
			hmlp::gofmm::SPDMatrix<T> K;
			K.resize( n, n );
			/** random spd initialization */
			K.randspd<USE_LOWRANK>( 0.0, 1.0 );
			/** (optional) provide neighbors, leave uninitialized otherwise */
			hmlp::Data<std::pair<T, std::size_t>> NN;
			/** routine */
			test_gofmm_setup<ADAPTIVE, LEVELRESTRICTION, T>
				( X, K, NN, metric, n, m, k, s, stol, budget, nrhs );
		}
		{
      d = 4;
			/** generate coordinates from normal(0,1) distribution */
			hmlp::Data<T> X( d, n ); X.randn( 0.0, 1.0 );
      /** setup the kernel object as Gaussian */
      kernel_s<T> kernel;
      kernel.type = KS_GAUSSIAN;
      kernel.scal = -0.5 / ( h * h );
      /** spd kernel matrix format (implicitly create) */
      hmlp::KernelMatrix<T> K( n, n, d, kernel, X );
			/** (optional) provide neighbors, leave uninitialized otherwise */
			hmlp::Data<std::pair<T, std::size_t>> NN;
			/** routine */
      test_gofmm_setup<ADAPTIVE, LEVELRESTRICTION, T>
      ( &X, K, NN, metric, n, m, k, s, stol, budget, nrhs );
		}
  }


  /** HMLP API call to terminate the runtime */
  hmlp_finalize();

  return 0;

}; /** end main() */
