/*
*Very Large Integer Library, License - Version 1.0 - May 3rd, 2012
*
*Timothee Ewart - University of Geneva, 
*Andreas Hehn - Swiss Federal Institute of technology Zurich.
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



#include "vli/detail/gpu/singleton.h"
#include "vli/detail/gpu/numeric.h"

#ifndef GPU_HARDWARE_CARRYOVER_IMPLEMENTATION_H
#define GPU_HARDWARE_CARRYOVER_IMPLEMENTATION_H

namespace vli {
    namespace detail {

    template <typename BaseInt, std::size_t Size, unsigned int Order, class Var0, class Var1, class Var2, class Var3>
    class gpu_hardware_carryover_implementation : public Singleton<gpu_hardware_carryover_implementation<BaseInt, Size, Order, Var0, Var1, Var2, Var3> > {
        friend class Singleton<gpu_hardware_carryover_implementation>; // to have access to the Instance, Destroy functions into the singleton class
    private:
        gpu_hardware_carryover_implementation();
        gpu_hardware_carryover_implementation(gpu_hardware_carryover_implementation const &);
        gpu_hardware_carryover_implementation& operator =(gpu_hardware_carryover_implementation const &);
    public:
        ~gpu_hardware_carryover_implementation();
        void plan();
        void resize();
        single_coefficient_task* execution_plan_;// we do not care the type
        unsigned int* workblock_count_by_warp_; // we do not care the type
    private:
        unsigned int  element_count_prepared;
    };

    } // end namespace detail
 }//end namespace vli

#include "vli/detail/gpu/gpu_hardware_carryover_implementation.hpp"

#endif 
