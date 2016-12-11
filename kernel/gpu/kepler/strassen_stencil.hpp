/**
 *  -- GKMX (version 1.1.0) --
 *
 *  NVIDIA Corp, Santa Clara
 *
 *  @date June 2016
 *  @author Chenhan D. Yu
 *
 *  @breif gkmm_template_stencil.hxx
 *
 *  This file is modified from gemm_template_device.cuh of 
 *  MAGMA (version 2.0.2). Four types NN, TN, NT, and TT are unified in
 *  this kernel. sA, rA and ra are type TA. sB, rB and rb are type TB.
 *  rC is type TV. The store back (epilog) is not included here. Depending on
 *  which kernel matrix operation we want to perform, the epilog region is
 *  implemented differently. To recap the type rules here,
 *  op2: <TA,TB> to <TX> where TX is implicit.
 *  op1: <TX,TX> to <TV>
 * 
 */

  //int LDA0, LDA1, LDB0, LDB1;

  //LDA0 = LDA;
  //LDA1 = LDA;
  //LDB0 = LDB;
  //LDB1 = LDB;

  int idx = threadIdx.x;  // thread's m dimension
  int idy = threadIdx.y;  // thread's n dimension

  int idt = DIM_X * idy + idx;    // thread's global number

  int idxA = idt % DIM_XA;    // idx within A
  int idyA = idt / DIM_XA;    // idy within A

  int idxB = idt % DIM_XB;    // idx within B
  int idyB = idt / DIM_XB;    // idy within B

  int blx = blockIdx.x;   // block's m dimension
  int bly = blockIdx.y;   // block's n dimension

  __shared__ TA sA[BLK_K][BLK_M+1];      // +1 only required if A is transposed
  __shared__ TB sB[BLK_N][BLK_K+1];      // +1 always required

  // Registers for the innermost loop
  TV rC[THR_N][THR_M];
  TA rA[THR_M];
  TB rB[THR_N];

  TA ra[TRANSA?BLK_M/DIM_YA:BLK_K/DIM_YA][TRANSA?BLK_K/DIM_XA:BLK_M/DIM_XA];
  TB rb[TRANSB?BLK_K/DIM_YB:BLK_N/DIM_YB][TRANSB?BLK_N/DIM_XB:BLK_K/DIM_XB];

  const TA *offs_dA0, *offs_dA1;
  const TB *offs_dB0, *offs_dB1;
  ptrdiff_t boundA, boundB;

  if ( TRANSA ) 
  {
    offs_dA0 = A0 + blx*BLK_M*LDA + idyA*LDA + idxA;
    offs_dA1 = A1 + blx*BLK_M*LDA + idyA*LDA + idxA;
    boundA   = (LDA*(M-1) + K) - ( blx*BLK_M*LDA + idyA*LDA + idxA ) -1;
  }
  else 
  {
    offs_dA0 = A0 + blx*BLK_M     + idyA*LDA + idxA;
    offs_dA1 = A1 + blx*BLK_M     + idyA*LDA + idxA;
    boundA  = (LDA*(K-1) + M) - ( blx*BLK_M  + idyA*LDA + idxA ) -1;
  }

  if ( TRANSB ) 
  {
    offs_dB0 = B0 + bly*BLK_N     + idyB*LDB + idxB;
    offs_dB1 = B1 + bly*BLK_N     + idyB*LDB + idxB;
    boundB   = (LDB*(K-1) + N) - ( bly*BLK_N     + idyB*LDB + idxB ) -1;
  }
  else 
  {
    offs_dB0 = B0 + bly*BLK_N*LDB + idyB*LDB + idxB;
    offs_dB1 = B1 + bly*BLK_N*LDB + idyB*LDB + idxB;
    boundB   = (LDB*(N-1) + K) - ( bly*BLK_N*LDB + idyB*LDB + idxB ) -1;
  }

  int m, n, k, kk;

  // Initialize rC<TV> with initV.
  #pragma unroll
  for (n = 0; n < THR_N; n++)
    #pragma unroll
    for (m = 0; m < THR_M; m++)
      rC[ n ][ m ] = initV;



  // pack A0 + A1
  if ( TRANSA ) 
  {
    #pragma unroll
    for ( n = 0; n < BLK_M; n += DIM_YA )
      #pragma unroll
      for ( m = 0; m < BLK_K; m += DIM_XA )
      {
        if ( GAMMA )
          //sA[m+idxA][n+idyA] =   1.0 * fetch(A0, m, n, boundA) 
          //                   + GAMMA * fetch(A1, m, n, boundA);
          sA[m+idxA][n+idyA] =   1.0 * strfetch(A, 0, m, n, boundA) 
                             + GAMMA * strfetch(A, 1, m, n, boundA);
        else
          //sA[m+idxA][n+idyA] =   1.0 * fetch(A0, m, n, boundA);
          sA[m+idxA][n+idyA] =   1.0 * strfetch(A, 0, m, n, boundA);
      }
  }
  else 
  {
    #pragma unroll
    for ( n = 0; n < BLK_K; n += DIM_YA )
      #pragma unroll
      for ( m = 0; m < BLK_M; m += DIM_XA )
      {
        if ( GAMMA )
          //sA[n+idyA][m+idxA] =   1.0 * fetch(A0, m, n, boundA) 
          //                   + GAMMA * fetch(A1, m, n, boundA);
          sA[n+idyA][m+idxA] =   1.0 * strfetch(A, 0, m, n, boundA) 
                             + GAMMA * strfetch(A, 1, m, n, boundA);
        else
          //sA[n+idyA][m+idxA] =   1.0 * fetch(A0, m, n, boundA);
          sA[n+idyA][m+idxA] =   1.0 * strfetch(A, 0, m, n, boundA);
      }
  }

  if ( TRANSB ) 
  {
    #pragma unroll
    for ( n = 0; n < BLK_K; n += DIM_YB )
      #pragma unroll
      for ( m = 0; m < BLK_N; m += DIM_XB )
      {
        if ( DELTA )
          //sB[m+idxB][n+idyB] =   1.0 * fetch(B0, m, n, boundB) 
          //                   + DELTA * fetch(B1, m, n, boundB);
          sB[m+idxB][n+idyB] =   1.0 * strfetch(B, 0, m, n, boundB) 
                             + DELTA * strfetch(B, 1, m, n, boundB);
        else
          //sB[m+idxB][n+idyB] =   1.0 * fetch(B0, m, n, boundB);
          sB[m+idxB][n+idyB] =   1.0 * strfetch(B, 0, m, n, boundB);
      }
  }
  else 
  { 
    #pragma unroll
    for ( n = 0; n < BLK_N; n += DIM_YB )
      #pragma unroll
      for ( m = 0; m < BLK_K; m += DIM_XB )
      {
        if ( DELTA )
          //sB[n+idyB][m+idxB] =   1.0 * fetch(B0, m, n, boundB) 
          //                   + DELTA * fetch(B1, m, n, boundB);
          sB[n+idyB][m+idxB] =   1.0 * strfetch(B, 0, m, n, boundB) 
                             + DELTA * strfetch(B, 1, m, n, boundB);
        else
          //sB[n+idyB][m+idxB] =   1.0 * fetch(B0, m, n, boundB); 
          sB[n+idyB][m+idxB] =   1.0 * strfetch(B, 0, m, n, boundB); 
      }
  }

  __syncthreads();

  for ( kk = 0; kk < K-BLK_K; kk += BLK_K ) {

    if ( TRANSA ) 
    {
      offs_dA0 += BLK_K;
      offs_dA1 += BLK_K;
      boundA   -= BLK_K;
    }
    else 
    {
      offs_dA0 += BLK_K*LDA;
      offs_dA1 += BLK_K*LDA;
      boundA   -= BLK_K*LDA;
    }

    if ( TRANSB ) 
    {
      offs_dB0 += BLK_K*LDB;
      offs_dB1 += BLK_K*LDB;
      boundB   -= BLK_K*LDB;
    }
    else 
    {
      offs_dB0 += BLK_K;
      offs_dB1 += BLK_K;
      boundB   -= BLK_K;
    }


    if ( GAMMA )
    {
      //#pragma unroll 2
      for ( n = 0; n < ( TRANSA?BLK_M/DIM_YA:BLK_K/DIM_YA ); n++ )
        #pragma unroll
        for ( m = 0; m < ( TRANSA?BLK_K/DIM_XA:BLK_M/DIM_XA ); m++ )
          //ra[n][m] =   1.0 * fetch(A0, m*DIM_XA, n*DIM_YA, boundA)
          //         + GAMMA * fetch(A1, m*DIM_XA, n*DIM_YA, boundA);
          ra[n][m] =   1.0 * strfetch(A, 0, m*DIM_XA, n*DIM_YA, boundA)
                   + GAMMA * strfetch(A, 1, m*DIM_XA, n*DIM_YA, boundA);
    }
    else
    {
      #pragma unroll
      for ( n = 0; n < ( TRANSA?BLK_M/DIM_YA:BLK_K/DIM_YA ); n++ )
        #pragma unroll
        for ( m = 0; m < ( TRANSA?BLK_K/DIM_XA:BLK_M/DIM_XA ); m++ )
          //ra[n][m] =   1.0 * fetch(A0, m*DIM_XA, n*DIM_YA, boundA);
          ra[n][m] =   1.0 * strfetch(A, 0, m*DIM_XA, n*DIM_YA, boundA);
    }

    if ( DELTA )
    {
      //#pragma unroll 2
      for ( n = 0; n < ( TRANSB?BLK_K/DIM_YB:BLK_N/DIM_YB ); n++ )
        #pragma unroll
        for ( m = 0; m < ( TRANSB?BLK_N/DIM_XB:BLK_K/DIM_XB ); m++ )
          rb[n][m] =   1.0 * strfetch(B, 0, m*DIM_XB, n*DIM_YB, boundB)
                   + DELTA * strfetch(B, 1, m*DIM_XB, n*DIM_YB, boundB);
    }
    else
    {
      #pragma unroll
      for ( n = 0; n < ( TRANSB?BLK_K/DIM_YB:BLK_N/DIM_YB ); n++ )
        #pragma unroll
        for ( m = 0; m < ( TRANSB?BLK_N/DIM_XB:BLK_K/DIM_XB ); m++ )
          rb[n][m] =   1.0 * strfetch(B, 0, m*DIM_XB, n*DIM_YB, boundB);
    }




    // Multiply
    #pragma unroll
    for ( k = 0; k < BLK_K; k++ ) 
    {
      // Load A shmem->regs
      #pragma unroll
      for ( m = 0; m < THR_M; m++ )
        rA[m] = sA[k][m*DIM_X+idx];

      // Load B shmem->regs
      #pragma unroll
      for ( n = 0; n < THR_N; n++ )
        rB[n] = sB[n*DIM_Y+idy][k];

      // Compute
      #pragma unroll
      for ( n = 0; n < THR_N; n++ ) 
      {
        #pragma unroll
        for ( m = 0; m < THR_M; m++ ) 
        {
          rC[ n ][ m ] = op1( rC[ n ][ m ], op2( rA[ m ], rB[ n ] ) );
        }
      }
    }

    __syncthreads();


    if ( TRANSA ) 
    {
      #pragma unroll
      for ( n = 0; n < BLK_M/DIM_YA; n++ )
        #pragma unroll
        for (m = 0; m < BLK_K/DIM_XA; m++)
          sA[m*DIM_XA+idxA][n*DIM_YA+idyA] = ra[n][m];
    }
    else 
    {
      #pragma unroll
      for ( n = 0; n < BLK_K/DIM_YA; n++ )
        #pragma unroll
        for ( m = 0; m < BLK_M/DIM_XA; m++ )
          sA[n*DIM_YA+idyA][m*DIM_XA+idxA] = ra[n][m];
    }


    if ( TRANSB ) 
    {
      #pragma unroll
      for ( n = 0; n < BLK_K/DIM_YB; n++ )
        #pragma unroll
        for ( m = 0; m < BLK_N/DIM_XB; m++)
          sB[m*DIM_XB+idxB][n*DIM_YB+idyB] = rb[n][m];
    }
    else 
    {
      #pragma unroll
      for ( n = 0; n < BLK_N/DIM_YB; n++ )
        #pragma unroll
        for ( m = 0; m < BLK_K/DIM_XB; m++ )
          sB[n*DIM_YB+idyB][m*DIM_XB+idxB] = rb[n][m];
    }

    __syncthreads();
  }

  kk = K - kk;
  #pragma unroll
  for ( k = 0; k < kk; k++ ) {
    // Load A shmem->regs
    #pragma unroll
    for ( m = 0; m < THR_M; m++ )
      rA[m] = sA[k][m*DIM_X+idx];

    // Load B shmem->regs
    #pragma unroll
    for ( n = 0; n < THR_N; n++ )
      rB[n] = sB[n*DIM_Y+idy][k];

    // Compute
    #pragma unroll
    for ( n = 0; n < THR_N; n++ ) 
    {
      #pragma unroll
      for ( m = 0; m < THR_M; m++ ) {
        rC[ n ][ m ] = op1( rC[ n ][ m ], op2( rA[ m ], rB[ n ] ) );
      }
    }
  }
