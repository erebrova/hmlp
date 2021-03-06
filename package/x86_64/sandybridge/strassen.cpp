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




/** STRASSEN templates */
#include <primitives/strassen.hpp>

/** Sandy-bridge micro-kernel */
#include <stra_k_d8x4.hpp>
//#include <rank_k_d8x4.hpp>

using namespace hmlp::strassen;

void dstrassen(
  hmlpOperation_t transA, hmlpOperation_t transB,
	int m, int n, int k,
	double *A, int lda,
	double *B, int ldb,
	double *C, int ldc
	)
{
  // Sandy-bridge
  //stra_k_ref_d8x4 stra_semiringkernel;
  //stra_k_ref_d8x4 stra_microkernel;

  stra_k_asm_d8x4 stra_semiringkernel;
  stra_k_asm_d8x4 stra_microkernel;


  //rank_k_ref_d8x4 rank_semiringkernel;
  //rank_k_ref_d8x4 rank_microkernel;

  strassen
  <104, 4096, 256, 8, 4, 104, 4096, 8, 4, 32,
  false,
	//rank_k_ref_d8x4, rank_k_ref_d8x4,
	//stra_k_ref_d8x4, stra_k_ref_d8x4,
	stra_k_asm_d8x4, stra_k_asm_d8x4,
	double, double, double, double>
	(
    transA, transB,
	  m, n, k,
	  A, lda,
	  B, ldb,
	  C, ldc,
	  //rank_semiringkernel,
	  //rank_microkernel,
	  //stra_semiringkernel,
	  //stra_microkernel
	  stra_semiringkernel,
	  stra_microkernel
	);
}
