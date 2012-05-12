#include <vector>
#include <algorithm>

#include "vli/detail/kernels_gpu.h"
#include "vli/detail/gpu_hardware_carryover_implementation.h"
#include "vli/detail/gpu_mem_block.h"
#include "vli/detail/kernels_gpu_asm.hpp"

/*
*  (((X>>1)<<1)+1) == X if odd, X+1 if even
*/
namespace vli {
    namespace detail {

    template <typename BaseInt, std::size_t Size, unsigned int Order>
    __global__ void
    __launch_bounds__(MUL_BLOCK_SIZE, 2)
    polynomial_mul_full(
    	const unsigned int * __restrict__ in1,
    	const unsigned int * __restrict__ in2,
           const unsigned int element_count,
           unsigned int * __restrict__ out)
    {
           __shared__ unsigned int in_buffer1[(((Order>>1)<<1)+1) * Order * Size];
           __shared__ unsigned int in_buffer2[(((Order>>1)<<1)+1) * Order * Size];
   
           const unsigned int local_thread_id = threadIdx.x;
           const unsigned int element_id = blockIdx.x;
   
           // Copy both input polynomials into the shared memory
           {
           	const unsigned int * in_shifted1 = in1 + (element_id * (Order * Order * Size));
           	const unsigned int * in_shifted2 = in2 + (element_id * (Order * Order * Size));
           	unsigned int index_id = local_thread_id;
           	#pragma unroll
           	for(unsigned int i = 0; i < (Order * Order * Size) / MUL_BLOCK_SIZE; ++i)
           	{
           		unsigned int coefficient_id = index_id / Size;
           		unsigned int degree_id = index_id % Size;
           		unsigned int current_degree_y = coefficient_id / Order;
           		unsigned int current_degree_x = coefficient_id % Order;
           		unsigned int local_index_id = current_degree_x + (current_degree_y * (((Order>>1)<<1)+1)) + (degree_id * ((((Order>>1)<<1)+1) * Order));
           		in_buffer1[local_index_id] = in_shifted1[index_id];
           		in_buffer2[local_index_id] = in_shifted2[index_id];
           		index_id += MUL_BLOCK_SIZE;
           	}

           	if (index_id < (Order * Order * Size))
           	{
           		unsigned int coefficient_id = index_id / Size;
           		unsigned int degree_id = index_id % Size;
           		unsigned int current_degree_y = coefficient_id / Order;
           		unsigned int current_degree_x = coefficient_id % Order;
           		unsigned int local_index_id = current_degree_x + (current_degree_y * (((Order>>1)<<1)+1)) + (degree_id * ((((Order>>1)<<1)+1) * Order));
           		in_buffer1[local_index_id] = in_shifted1[index_id];
           		in_buffer2[local_index_id] = in_shifted2[index_id];
           	}
           	__syncthreads();
           }
   
           unsigned int c1[Size],c2[Size];
           unsigned int res[2*Size];
           unsigned int res1[2*Size];
   
           unsigned int iteration_count = workblock_count_by_warp[local_thread_id / 32];
           for(unsigned int iteration_id = 0; iteration_id < iteration_count; ++iteration_id)
           {
           	vli::detail::single_coefficient_task task = execution_plan[local_thread_id + (iteration_id * MUL_BLOCK_SIZE)];
           	const unsigned int step_count = task.step_count;
   
           	if (step_count > 0)
           	{
           		const unsigned int output_degree_y = task.output_degree_y;
           		const unsigned int output_degree_x = task.output_degree_x;
   
           		#pragma unroll
           		for(unsigned int i = 0; i < 2*Size; ++i)
           			res[i] = 0;
   
           		const unsigned int start_degree_x_inclusive = output_degree_x > (Order - 1) ? output_degree_x - (Order - 1) : 0;
           		const unsigned int end_degree_x_inclusive = output_degree_x < Order ? output_degree_x : (Order - 1);
           		unsigned int current_degree_x = start_degree_x_inclusive;
           		unsigned int current_degree_y = output_degree_y > (Order - 1) ? output_degree_y - (Order - 1) : 0;
   
           		for(unsigned int step_id = 0; step_id < step_count; ++step_id) {
                               unsigned int * in_polynomial1 = in_buffer1 + current_degree_x + (current_degree_y * (((Order>>1)<<1)+1));
                               unsigned int * in_polynomial2 = in_buffer2 + (output_degree_x - current_degree_x) + ((output_degree_y - current_degree_y) * (((Order>>1)<<1)+1));
   
                               #pragma unroll
                               for(unsigned int i = 0; i < 2*Size; ++i)
                                  res1[i] = 0;
   
                               #pragma unroll
                               for(unsigned int degree1 = 0; degree1 < Size; ++degree1)
                                   c1[degree1] = in_polynomial1[degree1 * ((((Order>>1)<<1)+1) * Order)];
    
                               #pragma unroll
                               for(unsigned int degree2 = 0; degree2 < Size; ++degree2)
                                   c2[degree2] = in_polynomial2[degree2  * ((((Order>>1)<<1)+1) * Order)];
    
                               unsigned int sign = (c1[Size-1]>>31) ^ (c2[Size-1]>>31);
   
                               if(c1[Size-1] >> 31 != 0)
                                   vli::detail::negate192_gpu(c1); 
   
                               if(c2[Size-1] >> 31 != 0)
                                   vli::detail::negate192_gpu(c2); 
   
                               vli::detail::mul384_384_gpu(res1,c1,c2);
   
                               if(sign != 0)
                                   vli::detail::negate384_gpu(res1);
   
                               vli::detail::add384_384_gpu(res,res1);
           			// Calculate the next pair of input coefficients to be multiplied and added to the result
                               current_degree_x++;
                               if (current_degree_x > end_degree_x_inclusive) {
                                   current_degree_x = start_degree_x_inclusive;
                                   current_degree_y++;
                               }
           		}
   
           		unsigned int coefficient_id = output_degree_y * (Order*2-1) + output_degree_x;
           		unsigned int * out2 = out + (coefficient_id * element_count *2* Size) + element_id; // coefficient->int_degree->element_id
           		#pragma unroll
           		for(unsigned int i = 0; i < 2*Size; ++i) {
           			// This is a strongly compute-bound kernel,
           			// so it is fine to waste memory bandwidth by using non-coalesced writes in order to have less instructions,
           			//     less synchronization points, less shared memory used (and thus greater occupancy) and greater scalability.
           			*out2 = res[i];
           			out2 += element_count;
           		}
           	} // if (step_count > 0)
           } //for(unsigned int iteration_id
    }
   
    template <typename BaseInt, std::size_t Size, unsigned int Order>
    __global__ void
    __launch_bounds__(SUM_BLOCK_SIZE, 2)
    polynomial_sum_intermediate_full(
           const unsigned int * __restrict__ intermediate,
           const unsigned int element_count,
           unsigned int * __restrict__ out)
    {
           __shared__ unsigned int buf[SUM_BLOCK_SIZE * 2*(((Order>>1)<<1)+1)];
   
           unsigned int local_thread_id = threadIdx.x;
           unsigned int coefficient_id = blockIdx.x;
   
           unsigned int * t1 = buf + (local_thread_id * 2*(((Order>>1)<<1)+1));
           #pragma unroll
           for(unsigned int i = 0; i < 2*Size; ++i)
           	t1[i] = 0;
   
           const unsigned int * in2 = intermediate + (coefficient_id * element_count *2*Size) + local_thread_id;
           for(unsigned int element_id = local_thread_id; element_id < element_count; element_id += SUM_BLOCK_SIZE)
           {
           	unsigned int carry_over = 0;
           	#pragma unroll
           	for(unsigned int degree = 0; degree < 2*Size; ++degree)
           	{
           		unsigned long long res_wide = (unsigned long long)t1[degree] + in2[degree * element_count] + carry_over;
           		t1[degree] = res_wide;
           		carry_over = res_wide >> 32;
           	}
           	in2 += SUM_BLOCK_SIZE;
           }
   
           #pragma unroll
           for(unsigned int stride = SUM_BLOCK_SIZE >> 1; stride > 0; stride >>= 1) {
           	__syncthreads();
           	if (local_thread_id < stride) {
   
           		unsigned int * t2 = buf + ((local_thread_id + stride) * 2 *(((Order>>1)<<1)+1));
   
           		unsigned int carry_over = 0;
           		#pragma unroll
           		for(unsigned int degree = 0; degree < 2*Size; ++degree) {
           			unsigned long long res_wide = (unsigned long long)t1[degree] + t2[degree] + carry_over;
           			t1[degree] = res_wide;
           			carry_over = res_wide >> 32;
           		}
           	}
           }
   
           if (local_thread_id == 0) {
           	unsigned int * out2 = out+(coefficient_id*2*Size) /* ---> offset there ------> */ + coefficient_id/(2*Order-1)*(2*Size); // I add an offset to fit with vli, to do find a way to remove ghost element into VLI
           	#pragma unroll
           	for(unsigned int i=0; i<2*Size; ++i)
           		out2[i] = buf[i];
           }
    }

    template <typename BaseInt, std::size_t Size, unsigned int Order>
    void inner_product_vector_nvidia(std::size_t VectorSize, BaseInt const* A, BaseInt const* B) {
                gpu_memblock<BaseInt, Size, Order>* gm = gpu_memblock<BaseInt, Size, Order>::Instance(); // allocate memory for vector input, intermediate and output, singleton only one time 
                gpu_hardware_carryover_implementation<BaseInt, Size, Order>* ghc = gpu_hardware_carryover_implementation<BaseInt, Size, Order>::Instance(); // calculate the different packet, singleton only one time 

                cudaMemcpyAsync((void*)gm->V1Data_,(void*)A,VectorSize*Order*Order*Size*sizeof(BaseInt),cudaMemcpyHostToDevice);
                cudaMemcpyAsync((void*)gm->V2Data_,(void*)B,VectorSize*Order*Order*Size*sizeof(BaseInt),cudaMemcpyHostToDevice);

		{
			dim3 grid(VectorSize) ;
			dim3 threads(MUL_BLOCK_SIZE);
			polynomial_mul_full<BaseInt, Size, Order><<<grid,threads>>>(gm->V1Data_, gm->V2Data_,VectorSize, gm->VinterData_);
		}

		{
			dim3 grid((Order*2-1)*(Order*2-1));
			dim3 threads(SUM_BLOCK_SIZE);
			polynomial_sum_intermediate_full<BaseInt, Size, Order><<<grid,threads>>>(gm->VinterData_, VectorSize, gm->PoutData_);
		}
    } 

    template <typename BaseInt, std::size_t Size, unsigned int Order>
    BaseInt* get_polynomial(){
          gpu_memblock<BaseInt, Size, Order>* gm = gpu_memblock<BaseInt, Size, Order>::Instance(); // I just get the mem pointer
          return gm->PoutData_;
    }

    #define VLI_IMPLEMENT_GPU_FUNCTIONS(TYPE, VLI_SIZE, POLY_ORDER) \
        void inner_product_vector_nvidia(vli_size_tag<VLI_SIZE, POLY_ORDER>, std::size_t vector_size, TYPE const* A, TYPE const* B) \
            {inner_product_vector_nvidia<unsigned int, 2*VLI_SIZE, POLY_ORDER>(vector_size, const_cast<unsigned int*>(reinterpret_cast<unsigned int const*>(A)), const_cast<unsigned int*>(reinterpret_cast<unsigned int const*>(B)));} \
        unsigned int* get_polynomial(vli_size_tag<VLI_SIZE, POLY_ORDER>) /* cuda mem allocated on unsigned int (gpu_mem_block class), do not change the return type */ \
            {return get_polynomial<unsigned int, 2*VLI_SIZE, POLY_ORDER>();}\
   
    #define VLI_IMPLEMENT_GPU_FUNCTIONS_FOR(r, data, BASEINT_SIZE_ORDER_TUPLE) \
        VLI_IMPLEMENT_GPU_FUNCTIONS( BOOST_PP_TUPLE_ELEM(3,0,BASEINT_SIZE_ORDER_TUPLE), BOOST_PP_TUPLE_ELEM(3,1,BASEINT_SIZE_ORDER_TUPLE), BOOST_PP_TUPLE_ELEM(3,2,BASEINT_SIZE_ORDER_TUPLE) )
   
    BOOST_PP_SEQ_FOR_EACH(VLI_IMPLEMENT_GPU_FUNCTIONS_FOR, _, VLI_COMPILE_BASEINT_SIZE_ORDER_TUPLE_SEQ)
   
    #undef VLI_IMPLEMENT_GPU_FUNCTIONS_FOR
    #undef VLI_IMPLEMENT_GPU_FUNCTIONS

    }
}
