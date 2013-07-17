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

#ifndef AMBIENT_CONTROLLERS_VELVET_CONTROLLER
#define AMBIENT_CONTROLLERS_VELVET_CONTROLLER

#include "ambient/controllers/velvet/cfunctor.h"
#include "ambient/memory/collector.h"

namespace ambient { namespace controllers { namespace velvet {

    using ambient::models::velvet::history;
    using ambient::models::velvet::revision;

    class controller : public singleton< controller >
    {
    public:
        class scope {
        public:
            int sector;
            int round;
            int gauge;
            ambient::locality state;
            virtual bool tunable() = 0;
            virtual void consider_transfer(size_t size, ambient::locality l){}
            virtual void consider_allocation(size_t size){}
            virtual void toss(){}
        };

        controller();
       ~controller();
        bool empty();
        void flush();
        void clear();
        bool queue (cfunctor* f);
        void sync  (revision* r);
        void lsync (revision* r);
        void rsync (revision* r);
        void lsync (transformable* v);
        void rsync (transformable* v);
        template<typename T> void destroy(T* o);

        bool tunable();
        template<complexity O> void schedule();
        void intend_fetch(history* o);
        void intend_write(history* o);

        void set_context(scope* s);
        void pop_context();
        bool remote();
        bool local();
        int which();

        scope* context;
        scope* context_base;
    private:
        std::vector< cfunctor* > stack_m;
        std::vector< cfunctor* > stack_s;
        std::vector< cfunctor* >* chains;
        std::vector< cfunctor* >* mirror;
        ambient::memory::collector garbage;
    };
    
} } }

namespace ambient {
    extern controllers::velvet::controller& controller;
}

#include "ambient/controllers/velvet/scope.hpp"
#include "ambient/controllers/velvet/controller.hpp"
#include "ambient/controllers/velvet/cfunctor.hpp"
#include "ambient/memory/collector.hpp"
#endif
