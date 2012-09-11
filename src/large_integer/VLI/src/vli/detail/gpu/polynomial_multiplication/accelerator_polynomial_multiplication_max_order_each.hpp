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


namespace vli {
    namespace detail {

    #define VLI_SIZE num_words<NumBits>::value 
    /*
    *  I do this to remove the cloud of if of the general version l 92 to 101, for polynomial of 1 and 2 variables
    *  on fermi 2 variables with cloud = 0.22 s withou 0.17 s, to retest on kerpler and remove if necessarry
    * just recopy the 4 variables version (compatible 1 <-> 4) into polynomial_mul_full_kepler and remove the accelerator    
    */
    template <std::size_t NumBits, class MaxOrder, int NumVars>
    struct accelerator;

    // 4 variables
    template <std::size_t NumBits, int Order>
    struct accelerator<NumBits, max_order_each<Order>, 4>{
    inline static __device__ void polynomial_multiplication_max_order( const unsigned int element_count, unsigned int*  out, unsigned int*  workblock_count_by_warp, single_coefficient_task* execution_plan) {

        const unsigned int local_thread_id = threadIdx.x;
        const unsigned int element_id = blockIdx.x;
        
        unsigned int c1[VLI_SIZE],c2[VLI_SIZE];
        unsigned int res[2*VLI_SIZE];
        unsigned int res1[2*VLI_SIZE];
        
        unsigned int iteration_count = workblock_count_by_warp[local_thread_id / 32];
        
        const unsigned int input_elem_offset = element_id * stride<0,4,Order>::value * stride<1,4,Order>::value *  stride<2,4,Order>::value * stride<3,4,Order>::value * VLI_SIZE;
        
        for(unsigned int iteration_id = 0; iteration_id < iteration_count; ++iteration_id) {
            single_coefficient_task task = execution_plan[local_thread_id + (iteration_id * mul_block_size<max_order_each<2*Order>, 4>::value)];
            const unsigned int step_count = task.step_count;
        
            if (step_count > 0) {
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                    res[i] = 0;
                
                const unsigned int output_degree_x = task.output_degree_x;
                const unsigned int output_degree_y = task.output_degree_y;
                const unsigned int output_degree_z = task.output_degree_z;
                const unsigned int output_degree_w = task.output_degree_w;
                
                const unsigned int start_degree_x_inclusive = output_degree_x > Order ? output_degree_x - Order : 0;
                const unsigned int end_degree_x_inclusive = output_degree_x < (Order+1) ? output_degree_x : Order;
                unsigned int current_degree_x = start_degree_x_inclusive;
                
                const unsigned int start_degree_y_inclusive = output_degree_y > Order ? output_degree_y - Order : 0;
                const unsigned int end_degree_y_inclusive = output_degree_y < (Order+1) ? output_degree_y : Order;
                unsigned int current_degree_y = start_degree_y_inclusive;
                
                const unsigned int start_degree_z_inclusive = output_degree_z > Order ? output_degree_z - Order : 0;
                const unsigned int end_degree_z_inclusive = output_degree_z < (Order+1) ? output_degree_z : Order;
                unsigned int current_degree_z = start_degree_z_inclusive;
               
                unsigned int current_degree_w = output_degree_w > Order ? output_degree_w - Order : 0;
                
                for(unsigned int step_id = 0; step_id < step_count; ++step_id) {
                
                    unsigned int in_polynomial_offset1 = (   current_degree_w  * stride_pad<3, 4, Order>::value * stride_pad<2, 4, Order>::value * stride_pad<1, 4, Order>::value 
                                                           + current_degree_z  * stride_pad<2, 4, Order>::value * stride_pad<1, 4, Order>::value  
                                                           + current_degree_y  * stride_pad<1, 4, Order>::value
                                                           + current_degree_x
                                                         ) * VLI_SIZE + input_elem_offset;
                    
                    unsigned int in_polynomial_offset2 = (   (output_degree_w - current_degree_w) * stride_pad<3, 4, Order>::value * stride_pad<2, 4, Order>::value * stride_pad<1, 4, Order>::value
                                                           + (output_degree_z - current_degree_z) * stride_pad<2, 4, Order>::value * stride_pad<1, 4, Order>::value  
                                                           + (output_degree_y - current_degree_y) * stride_pad<1, 4, Order>::value
                                                           + (output_degree_x - current_degree_x)
                                                         ) * VLI_SIZE + input_elem_offset;
                
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c1[i] = tex1Dfetch(tex_reference_1,in_polynomial_offset1+i);        
                    }
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c2[i] = tex1Dfetch(tex_reference_2,in_polynomial_offset2+i);                 
                    } 
                
                    #pragma unroll
                    for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                        res1[i] = 0;
                 
                    multiplies<NumBits>(res, res1, c1, c2); // the multiplication using boost pp
                 
                    // Calculate the next pair of input coefficients to be multiplied and added to the result
                    // TO SPECIALIZE ..... all this dynamic if kill the perfs 
                    current_degree_x++;
                    if (current_degree_x > end_degree_x_inclusive) {
                        current_degree_x = start_degree_x_inclusive;
                        current_degree_y++;
                        if (current_degree_y > end_degree_y_inclusive) {
                            current_degree_y = start_degree_y_inclusive;
                            current_degree_z++;
                            if (current_degree_z > end_degree_z_inclusive) {
                                current_degree_z = start_degree_z_inclusive;
                                current_degree_w++;                        
                            }                    
                        }            
                    }
                }
                
                unsigned int coefficient_id =   output_degree_w * (Order*2+1)*(Order*2+1)*(Order*2+1)
                                              + output_degree_z * (Order*2+1)*(Order*2+1) 
                                              + output_degree_y * (Order*2+1) 
                                              + output_degree_x;
                
                unsigned int * out2 = out + (coefficient_id * element_count *2* VLI_SIZE) + element_id; // coefficient->int_degree->element_id
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i) {
                        // This is a strongly compute-bound kernel,
                        // so it is fine to waste memory bandwidth by using non-coalesced writes in order to have less instructions,
                        //     less synchronization points, less shared memory used (and thus greater occupancy) and greater scalability.
                        *out2 = res[i];
                        out2 += element_count;
                }
            } // end step count
        } // end for it

        }; // end function
    }; //end struct

    // 3 variables
    template <std::size_t NumBits, int Order>
    struct accelerator<NumBits, max_order_each<Order>, 3>{
    inline static __device__ void polynomial_multiplication_max_order(const unsigned int element_count, unsigned int*  out, unsigned int*  workblock_count_by_warp, single_coefficient_task* execution_plan) {
        const unsigned int local_thread_id = threadIdx.x;
        const unsigned int element_id = blockIdx.x;
        
        unsigned int c1[VLI_SIZE],c2[VLI_SIZE];
        unsigned int res[2*VLI_SIZE];
        unsigned int res1[2*VLI_SIZE];
        
        unsigned int iteration_count = workblock_count_by_warp[local_thread_id / 32];
        
        const unsigned int input_elem_offset = element_id * stride<0,3,Order>::value * stride<1,3,Order>::value *  stride<2,3,Order>::value * VLI_SIZE;
        
        for(unsigned int iteration_id = 0; iteration_id < iteration_count; ++iteration_id) {
            single_coefficient_task task = execution_plan[local_thread_id + (iteration_id * mul_block_size<max_order_each<2*Order>, 3>::value)];
            const unsigned int step_count = task.step_count;
        
            if (step_count > 0) {
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                    res[i] = 0;
                
                const unsigned int output_degree_x = task.output_degree_x;
                const unsigned int output_degree_y = task.output_degree_y;
                const unsigned int output_degree_z = task.output_degree_z;
                
                const unsigned int start_degree_x_inclusive = output_degree_x > Order ? output_degree_x - Order : 0;
                const unsigned int end_degree_x_inclusive = output_degree_x < (Order+1) ? output_degree_x : Order;
                unsigned int current_degree_x = start_degree_x_inclusive;
                
                const unsigned int start_degree_y_inclusive = output_degree_y > Order ? output_degree_y - Order : 0;
                const unsigned int end_degree_y_inclusive = output_degree_y < (Order+1) ? output_degree_y : Order;
                unsigned int current_degree_y = start_degree_y_inclusive;
                
                const unsigned int start_degree_z_inclusive = output_degree_z > Order ? output_degree_z - Order : 0;
                unsigned int current_degree_z = start_degree_z_inclusive;
               
                for(unsigned int step_id = 0; step_id < step_count; ++step_id) {
                
                    unsigned int in_polynomial_offset1 = (   current_degree_z  * stride_pad<2, 3, Order>::value * stride_pad<1 ,3, Order>::value  
                                                           + current_degree_y  * stride_pad<1, 3, Order>::value
                                                           + current_degree_x
                                                         ) * VLI_SIZE + input_elem_offset;
                    
                    unsigned int in_polynomial_offset2 = (   (output_degree_z - current_degree_z) * stride_pad<2, 3, Order>::value * stride_pad<1, 3, Order>::value  
                                                           + (output_degree_y - current_degree_y) * stride_pad<1, 3, Order>::value
                                                           + (output_degree_x - current_degree_x)
                                                         ) * VLI_SIZE + input_elem_offset;
                
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c1[i] = tex1Dfetch(tex_reference_1,in_polynomial_offset1+i);        
                    }

                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c2[i] = tex1Dfetch(tex_reference_2,in_polynomial_offset2+i);                 
                    } 
                
                    #pragma unroll
                    for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                        res1[i] = 0;
                 
                    multiplies<NumBits>(res, res1, c1, c2); // the multiplication using boost pp
                 
                    // Calculate the next pair of input coefficients to be multiplied and added to the result
                    // TO SPECIALIZE ..... all this dynamic if kill the perfs 
                    current_degree_x++;
                    if (current_degree_x > end_degree_x_inclusive) {
                        current_degree_x = start_degree_x_inclusive;
                        current_degree_y++;
                        if (current_degree_y > end_degree_y_inclusive) {
                            current_degree_y = start_degree_y_inclusive;
                            current_degree_z++;
                        }            
                    }
                }
                
                unsigned int coefficient_id =   output_degree_z * (Order*2+1)*(Order*2+1) 
                                              + output_degree_y * (Order*2+1) 
                                              + output_degree_x;
                
                unsigned int * out2 = out + (coefficient_id * element_count *2* VLI_SIZE) + element_id; // coefficient->int_degree->element_id
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i) {
                        // This is a strongly compute-bound kernel,
                        // so it is fine to waste memory bandwidth by using non-coalesced writes in order to have less instructions,
                        //     less synchronization points, less shared memory used (and thus greater occupancy) and greater scalability.
                        *out2 = res[i];
                        out2 += element_count;
                }
            } // end step count
        } // end for it

        }; //end function
    }; //end struct

    // 2 variables
    template <std::size_t NumBits, int Order>
    struct accelerator<NumBits, max_order_each<Order>, 2>{
    inline static __device__ void polynomial_multiplication_max_order( const unsigned int element_count, unsigned int*  out, unsigned int*  workblock_count_by_warp, single_coefficient_task* execution_plan) {

        const unsigned int local_thread_id = threadIdx.x;
        const unsigned int element_id = blockIdx.x;
        
        unsigned int c1[VLI_SIZE],c2[VLI_SIZE];
        unsigned int res[2*VLI_SIZE];
        unsigned int res1[2*VLI_SIZE];
        
        unsigned int iteration_count = workblock_count_by_warp[local_thread_id / 32];
        
        const unsigned int input_elem_offset = element_id * stride<0,2,Order>::value * stride<1,2,Order>::value * VLI_SIZE;
        
        for(unsigned int iteration_id = 0; iteration_id < iteration_count; ++iteration_id) {
            single_coefficient_task task = execution_plan[local_thread_id + (iteration_id * mul_block_size<max_order_each<2*Order>, 2>::value)];
            const unsigned int step_count = task.step_count;
        
            if (step_count > 0) {
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                    res[i] = 0;
                
                const unsigned int output_degree_x = task.output_degree_x;
                const unsigned int output_degree_y = task.output_degree_y;
                // TO DO try the version with 2 loop for like max_order_combined         
                const unsigned int start_degree_x_inclusive = output_degree_x > Order ? output_degree_x - Order : 0;
                const unsigned int end_degree_x_inclusive = output_degree_x < (Order+1) ? output_degree_x : Order;
                unsigned int current_degree_x = start_degree_x_inclusive;
                
                unsigned int current_degree_y = output_degree_y > Order ? output_degree_y - Order : 0;
                
                for(unsigned int step_id = 0; step_id < step_count; ++step_id) {
                
                    unsigned int in_polynomial_offset1 = (   current_degree_y  * stride_pad<1,2, Order>::value
                                                           + current_degree_x
                                                         ) * VLI_SIZE + input_elem_offset;
                    
                    unsigned int in_polynomial_offset2 = (   (output_degree_y - current_degree_y) * stride_pad<1,2, Order>::value
                                                           + (output_degree_x - current_degree_x)
                                                         ) * VLI_SIZE + input_elem_offset;
                
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c1[i] = tex1Dfetch(tex_reference_1,in_polynomial_offset1+i);        
                    }

                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c2[i] = tex1Dfetch(tex_reference_2,in_polynomial_offset2+i);                 
                    } 
                
                    #pragma unroll
                    for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                        res1[i] = 0;
                 
                    multiplies<NumBits>(res, res1, c1, c2); // the multiplication using boost pp
                 
                    current_degree_x++;
                    if (current_degree_x > end_degree_x_inclusive) {
                        current_degree_x = start_degree_x_inclusive;
                        current_degree_y++;
                    }
                }
                
                unsigned int coefficient_id =   output_degree_y * (Order*2+1) 
                                              + output_degree_x;
                
                unsigned int * out2 = out + (coefficient_id * element_count *2* VLI_SIZE) + element_id; // coefficient->int_degree->element_id
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i) {
                        // This is a strongly compute-bound kernel,
                        // so it is fine to waste memory bandwidth by using non-coalesced writes in order to have less instructions,
                        //     less synchronization points, less shared memory used (and thus greater occupancy) and greater scalability.
                        *out2 = res[i];
                        out2 += element_count;
                }
            } // end step count
        } // end for it

        }; //end function
    };// end struct

    // 1 variables
    template <std::size_t NumBits, int Order>
    struct accelerator<NumBits, max_order_each<Order>, 1>{
    inline static __device__ void polynomial_multiplication_max_order( const unsigned int element_count, unsigned int*  out, unsigned int*  workblock_count_by_warp, single_coefficient_task* execution_plan) {

        const unsigned int local_thread_id = threadIdx.x;
        const unsigned int element_id = blockIdx.x;
        
        unsigned int c1[VLI_SIZE],c2[VLI_SIZE];
        unsigned int res[2*VLI_SIZE];
        unsigned int res1[2*VLI_SIZE];
        
        unsigned int iteration_count = workblock_count_by_warp[local_thread_id / 32];
        
        const unsigned int input_elem_offset = element_id * stride<0,1,Order>::value * VLI_SIZE;
        
        for(unsigned int iteration_id = 0; iteration_id < iteration_count; ++iteration_id) {
            single_coefficient_task task = execution_plan[local_thread_id + (iteration_id * mul_block_size<max_order_each<2*Order>, 1>::value)];
            const unsigned int step_count = task.step_count;
        
            if (step_count > 0) {
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                    res[i] = 0;
                
                const unsigned int output_degree_x = task.output_degree_x;
                
                unsigned int current_degree_x = output_degree_x > Order ? output_degree_x - Order : 0;
                
                for(unsigned int step_id = 0; step_id < step_count; ++step_id) {
                
                    unsigned int in_polynomial_offset1 = (  current_degree_x
                                                         ) * VLI_SIZE + input_elem_offset;
                    
                    unsigned int in_polynomial_offset2 = (  (output_degree_x - current_degree_x)
                                                         ) * VLI_SIZE + input_elem_offset;
                
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c1[i] = tex1Dfetch(tex_reference_1,in_polynomial_offset1+i);        
                   }
                    #pragma unroll
                    for(unsigned int i = 0; i < VLI_SIZE; ++i){
                        c2[i] = tex1Dfetch(tex_reference_2,in_polynomial_offset2+i);                 
                    } 
                    #pragma unroll
                    for(unsigned int i = 0; i < 2*VLI_SIZE; ++i)
                        res1[i] = 0;
                 
                    multiplies<NumBits>(res, res1, c1, c2); // the multiplication using boost pp
                 
                    current_degree_x++;
                }
                
                unsigned int coefficient_id =  output_degree_x;
                
                unsigned int * out2 = out + (coefficient_id * element_count *2* VLI_SIZE) + element_id; // coefficient->int_degree->element_id
                #pragma unroll
                for(unsigned int i = 0; i < 2*VLI_SIZE; ++i) {
                        // This is a strongly compute-bound kernel,
                        // so it is fine to waste memory bandwidth by using non-coalesced writes in order to have less instructions,
                        //     less synchronization points, less shared memory used (and thus greater occupancy) and greater scalability.
                        *out2 = res[i];
                        out2 += element_count;
                }
            } // end step count
        } // end for it

        };  // end function
    }; // end struct

    #undef VLI_SIZE 
    }//end namesoace detail
}//end namespace vli
