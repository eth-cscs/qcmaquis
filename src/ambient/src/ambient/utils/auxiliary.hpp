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

#ifndef AMBIENT_UTILS_AUXILIARY
#define AMBIENT_UTILS_AUXILIARY

#include "ambient/utils/mem.h"

namespace ambient {

    using ambient::models::ssm::history;
    using ambient::models::ssm::revision;

    inline bool master(){
        return (ambient::rank() == 0);
    }

    inline bool parallel(){
        return (ambient::controller.context != ambient::controller.context_base);
    }

    template<typename T>
    inline void destroy(T* o){ 
        controller.collect(o); 
    }

    inline bool verbose(){ 
        return rank.verbose;   
    }

    inline void log(const char* msg){
        if(ambient::rank()) printf("%s\n", msg);
    }

    inline void meminfo(){ 
        size_t currentSize = getCurrentRSS( );
        size_t peakSize    = getPeakRSS( );
        for(int i = 0; i < ambient::channel.wk_dim(); i++){
            if(ambient::rank() == i){
                std::cout << "R" << i << ": currentSize: " << currentSize << " " << currentSize/1024/1024/1024 << "G.\n";
                std::cout << "R" << i << ": peakSize: " << peakSize << " " << peakSize/1024/1024/1024 << "G.\n";
            }
            ambient::channel.barrier();
        }
    }

    inline void sync(){ 
        controller.flush();
        controller.clear();  
        bulk::drop();
    }

    template<typename V>
    inline bool weak(const V& obj){
        return obj.versioned.core->weak();
    }

    template<typename V>
    inline void merge(const V& src, V& dst){
        assert(dst.versioned.core->current == NULL);
        if(weak(src)) return;
        revision* r = src.versioned.core->back();
        dst.versioned.core->current = r;
        // do not deallocate or reuse
        if(!r->valid()) r->spec.protect();
        assert(!r->valid() || !r->spec.bulked() || ambient::model.remote(r)); // can't rely on bulk memory
        r->spec.crefs++;
    }

    template<typename V>
    inline void swap_with(V& left, V& right){
        std::swap(left.versioned.core, right.versioned.core);
    }

    template<typename V>
    inline dim2 get_dim(const V& obj){
        return obj.versioned.core->dim;
    }

    template<typename V> 
    inline size_t get_square_dim(V& obj){ 
        return get_dim(obj).square();
    }

    template<typename V>
    inline size_t get_length(V& obj){
        return get_dim(obj).y;
    }
    
    template<typename V> 
    inline size_t extent(V& obj){ 
        return obj.versioned.core->extent;
    }

    template<typename V>
    inline void make_persistent(V& o){ 
        o.versioned.core->back()->spec.zombie();
    }

    template<typename V>
    inline int get_owner(const V& o){
        return o.versioned.core->current->owner;
    }
}

#endif
