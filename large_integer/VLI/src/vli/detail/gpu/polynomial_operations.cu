/*
*Very Large Integer Library, License - Version 1.0 - May 3rd, 2012
*
*Timothee Ewart - University of Geneva, 
*Andreas Hehn - Swiss Federal Institute of technology Zurich.
*Maxim Milakov - NVIDIA 
*
*Permission is hereby granted, free of charge, to any person or organization
*obtaining a copy of the software and accompanying documentation covered by
*this license (the "Software") to use, reproduce, display, distribute,
*execute, and transmit the Software, and to prepare derivative works of the
*Software, and to permit third-parties to whom the Software is furnished to
*do so, all subject to the following:
*
*The copyright notices in the Software and this entire statement, including
*the above license grant, this restriction and the following disclaimer,
*must be included in all copies of the Software, in whole or in part, and
*all derivative works of the Software, unless such copies or derivative
*works are solely in the form of machine-executable object code generated by
*a source language processor.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
*SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
*FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
*ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*DEALINGS IN THE SOFTWARE.
*/

#ifndef POLYNOMIAL_OPERATION_CU
#define POLYNOMIAL_OPERATION_CU

#include "vli/detail/gpu/polynomial_operations.h" //all include, system, boost, me

namespace vli {
    namespace detail {

    gpu_memblock const& pgm  = boost::serialization::singleton<gpu_memblock>::get_const_instance(); // create memory block

    template <std::size_t NumBits, class MaxOrder, int NumVars>
    __global__ void
    __launch_bounds__(mul_block_size<MaxOrder, NumVars,2>::value, 2)
    polynomial_multiply_full(
        const boost::uint32_t* in1,
        const boost::uint32_t* in2,
        const boost::uint32_t element_count,
        boost::uint32_t*  out,
        boost::uint32_t*  workblock_count_by_warp,
        single_coefficient_task* execution_plan)
    {
        accelerator<NumBits, MaxOrder, NumVars>::polynomial_multiplication_max_order(in1,in2,element_count, out, workblock_count_by_warp, execution_plan);
    }

    template <std::size_t NumBits, class MaxOrder, int NumVars>
    void gpu_inner_product_vecto_helper(std::size_t VectorSize, boost::uint32_t const* A, boost::uint32_t const* B) {
            tasklist_keep_order<NumBits, MaxOrder, NumVars> const& ghc =  boost::serialization::singleton< tasklist_keep_order<NumBits, MaxOrder, NumVars> >::get_const_instance(); // calculate the different packet, singleton only one time 
            memory_transfer_helper<NumBits, MaxOrder, NumVars>::transfer_up(pgm, A, B, VectorSize); //transfer data poly to gpu
            //first kernels multiplications polynomials
	    {
                dim3 grid(VectorSize) ;
                dim3 threads(mul_block_size<MaxOrder, NumVars,2>::value);
                polynomial_multiply_full<NumBits, MaxOrder, NumVars><<<grid,threads>>>(pgm.V1Data_, pgm.V2Data_,VectorSize, pgm.VinterData_,ghc.workblock_count_by_warp_,ghc.execution_plan_);
	    }
            //second kernels reduction polynomials
	    {
                dim3 grid, threads(sum_block_size::value);
                std::size_t num_block_offset(0),quotient(1),rest(0);
                if(num_coefficients<MaxOrder, NumVars,2>::value > numblock_constant_reduction::value){
                    quotient = num_coefficients<MaxOrder, NumVars,2>::value/numblock_constant_reduction::value;
                    rest = num_coefficients<MaxOrder, NumVars,2>::value%numblock_constant_reduction::value;
                    grid.x = numblock_constant_reduction::value;
                }else{
                    grid.x = num_coefficients<MaxOrder, NumVars,2>::value ;
                } 

                for(std::size_t i=0; i < quotient; ++i){ 
                    polynomial_sum_intermediate_full<NumBits, MaxOrder::value, NumVars><<<grid,threads>>>(pgm.VinterData_, VectorSize, pgm.PoutData_, num_block_offset); 
                    num_block_offset += numblock_constant_reduction::value;
                }

                if(rest != 0){
                    grid.x = rest;
                    polynomial_sum_intermediate_full<NumBits, MaxOrder::value, NumVars><<<grid,threads>>>(pgm.VinterData_, VectorSize, pgm.PoutData_, num_block_offset); 
                }
	    }
    }

    template <std::size_t NumBits, class MaxOrder, int NumVars>
    void gpu_inner_product_vector(std::size_t VectorSize, boost::uint32_t const* A, boost::uint32_t const* B) {
        scheduler sch;
        scheduler_helper<NumBits, MaxOrder, NumVars>::determine_memory(sch,VectorSize);
   //     sch.print();
        resize_helper<NumBits, MaxOrder, NumVars>::resize(pgm, boost::get<0>(sch.get_tupple_data())  ); // allocate mem
        sch.execute(gpu_inner_product_vecto_helper<NumBits,MaxOrder, NumVars>, A, B, full_value<NumBits, MaxOrder, NumVars>::value);
        memory_transfer_helper<NumBits, MaxOrder, NumVars>::unbind_texture();      
    } 

    boost::uint32_t* gpu_get_polynomial(){
	    gpu_memblock const& gm =  boost::serialization::singleton<gpu_memblock>::get_const_instance(); // I just get the mem pointer
	    return gm.PoutData_;
    }

#define VLI_IMPLEMENT_GPU_FUNCTIONS(NUM_BITS, POLY_ORDER, VAR) \
    template<std::size_t NumBits, class MaxOrder, int NumVars > \
    void gpu_inner_product_vector(std::size_t vector_size, boost::uint64_t const* A, boost::uint64_t const* B); \
    \
    template<> \
    void gpu_inner_product_vector<NUM_BITS, max_order_each<POLY_ORDER>, VAR >(std::size_t vector_size, boost::uint64_t const* A, boost::uint64_t const* B){ \
        gpu_inner_product_vector<NUM_BITS, max_order_each<POLY_ORDER>, VAR >(vector_size, const_cast<boost::uint32_t*>(reinterpret_cast<boost::uint32_t const*>(A)), const_cast<boost::uint32_t*>(reinterpret_cast<boost::uint32_t const*>(B))); \
    } \
    \
    template<> \
    void gpu_inner_product_vector<NUM_BITS, max_order_combined<POLY_ORDER>, VAR >(std::size_t vector_size, boost::uint64_t const* A, boost::uint64_t const* B){ \
        gpu_inner_product_vector<NUM_BITS, max_order_combined<POLY_ORDER>, VAR >(vector_size, const_cast<boost::uint32_t*>(reinterpret_cast<boost::uint32_t const*>(A)), const_cast<boost::uint32_t*>(reinterpret_cast<boost::uint32_t const*>(B))); \
    } \

#define VLI_IMPLEMENT_GPU_FUNCTIONS_FOR(r, data, NUMBITS_ORDER_VAR_TUPLE_SEQ) \
    VLI_IMPLEMENT_GPU_FUNCTIONS( BOOST_PP_TUPLE_ELEM(3,0,NUMBITS_ORDER_VAR_TUPLE_SEQ), BOOST_PP_TUPLE_ELEM(3,1,NUMBITS_ORDER_VAR_TUPLE_SEQ), BOOST_PP_TUPLE_ELEM(3,2,NUMBITS_ORDER_VAR_TUPLE_SEQ) )

    BOOST_PP_SEQ_FOR_EACH(VLI_IMPLEMENT_GPU_FUNCTIONS_FOR, _, VLI_COMPILE_NUMBITS_ORDER_VAR_TUPLE_SEQ)

#undef VLI_IMPLEMENT_GPU_FUNCTIONS_FOR
#undef VLI_IMPLEMENT_GPU_FUNCTIONS

    }
}

#endif
