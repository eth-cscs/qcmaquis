/*
 * Ambient, License - Version 1.0 - May 3rd, 2012
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ambient/ambient.hpp"

namespace ambient {

    void* fence::nptr = NULL;
    int scope<single>::grain = 1;
    std::vector<int> scope<single>::permutation;

    class universe {
    public:
        typedef controllers::ssm::controller controller_type;
        controller_type c;

        universe(){
            c.get_channel().init();
            c.init();
            #ifdef AMBIENT_PARALLEL_MKL
            ambient::mkl_set_num_threads(1);
            #endif
            #ifdef AMBIENT_VERBOSE
                #ifdef AMBIENT_CILK
                ambient::cout << "ambient: initialized (using cilk)\n";
                #elif defined(AMBIENT_OMP)
                ambient::cout << "ambient: initialized (using openmp)\n";
                #else
                ambient::cout << "ambient: initialized (no threading)\n";
                #endif
                ambient::cout << "ambient: size of instr bulk chunks: " << AMBIENT_INSTR_BULK_CHUNK << "\n";
                ambient::cout << "ambient: size of data bulk chunks: " << AMBIENT_DATA_BULK_CHUNK << "\n";
                ambient::cout << "ambient: maximum number of bulk chunks: " << AMBIENT_BULK_LIMIT << "\n";
                ambient::cout << "ambient: maximum sid value: " << AMBIENT_MAX_SID << "\n";
                ambient::cout << "ambient: number of database proc: " << AMBIENT_DB_PROCS << "\n";
                ambient::cout << "ambient: number of work proc: " << c.get_num_workers() << "\n";
                ambient::cout << "ambient: number of threads per proc: " << ambient::num_threads() << "\n";
                #ifdef AMBIENT_PARALLEL_MKL
                ambient::cout << "ambient: using MKL: threaded\n";
                #endif 
                ambient::cout << "\n";
            #endif
        }
        controller_type& operator()(size_t n){
            return c;
        }
        void sync(){
            c.flush();
            c.clear();  
            memory::data_bulk::drop();
            memory::instr_bulk::drop();
        }
    } u;

    controllers::ssm::controller& get_controller(){ return u(AMBIENT_THREAD_ID); }
    void sync(){ u.sync(); }
    utils::mpostream cout;
    utils::mpostream cerr;

}
