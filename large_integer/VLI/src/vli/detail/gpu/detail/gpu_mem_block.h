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

#ifndef GPU_MEM_BLOCK_H
#define GPU_MEM_BLOCK_H

namespace vli {
    namespace detail {

    texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> tex_reference_1; 
    texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> tex_reference_2; 

    // we allocate the mem only one time so pattern of this class singleton
    struct gpu_memblock {
        typedef boost::uint32_t value_type;
        gpu_memblock();
        std::size_t const& BlockSize() const;
        mutable std::size_t block_size_;
        value_type* V1Data_; // input vector 1
        value_type* V2Data_; // input vector 2
        value_type* VinterData_; // inter value before the final reduction
        value_type* PoutData_; // final output
    };
   

    
    } //end namespace detail
} //end namespce vli  

#endif //INNER_PRODUCT_GPU_BOOSTER_HPP
