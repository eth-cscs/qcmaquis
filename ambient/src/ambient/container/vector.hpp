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

#ifndef AMBIENT_CONTAINER_VECTOR
#define AMBIENT_CONTAINER_VECTOR

namespace ambient {

    template<typename T> class vector;
    namespace detail {
        template<typename T>
        void add(vector<T>& a, const vector<T>& b){
            size_t size = get_length(a)-1;
            T* a_ = versioned(a).data;
            T* b_ = versioned(b).data;
            for(size_t i = 0; i < size; ++i) a_[i] += b_[i];
        }
    }

    AMBIENT_EXPORT(detail::add, add)

    template <typename T>
    class vector : public ambient::block<T> {
    public:
        typedef T value_type;
        class iterator {
        public:
            typedef T value_type;
            iterator(vector<T>& base) : base(base) {}
            vector<T>& base;
        };
        vector(size_t length, T value) : ambient::block<T>(length, 1) {
            this->init(value);
        }
        iterator begin(){
            return iterator(*this);
        }
        iterator end(){
            return iterator(*this);
        }
        vector& operator += (const vector& rhs){
            add<T>(*this, rhs);
            return *this;
        }
    };


    template<class InputIterator, class Function>
    void for_each (InputIterator first, InputIterator last, Function fn){
        ambient::lambda([fn](vector<int>& a){ 
            int* a_ = versioned(a).data;
            std::for_each(a_, a_+get_length(a)-1, fn);
        })(first.base);
    }

    template <class InputIterator, class OutputIterator, class BinaryOperation>
    void transform (InputIterator first1, InputIterator last1,
                    InputIterator first2, 
                    OutputIterator result,
                    BinaryOperation binary_op)
    {
        ambient::lambda([binary_op](const vector<int>& first1_, const vector<int>& first2_, unbound< vector<int> >& result_){
            int* ifirst1 = versioned(first1_).data;
            int* ifirst2 = versioned(first2_).data;
            int* iresult = versioned(result_).data;
            std::transform(ifirst1, ifirst1+get_length(first1_)-1, ifirst2, iresult, binary_op);
        })(first1.base, first2.base, result.base);
    }

    template <class InputIterator, class OutputIterator, class UnaryOperation>
    void transform (InputIterator first1, InputIterator last1,
                    OutputIterator result, UnaryOperation op)
    {
        ambient::lambda([op](const vector<int>& first1_, unbound< vector<int> >& result_){
            int* ifirst1 = versioned(first1_).data;
            int* iresult = versioned(result_).data;
            std::transform(ifirst1, ifirst1+get_length(first1_)-1, iresult, op);
        })(first1.base, result.base);
    }

    template<class InputIterator>
    void sequence (InputIterator first, InputIterator last){
    }

    template <class ForwardIterator, class T>
    void fill (ForwardIterator first, ForwardIterator last, const T& val){
    }

    template <class ForwardIterator, class T>
    void replace (ForwardIterator first, ForwardIterator last, const T& old_value, const T& new_value){
    }

    template <class ForwardIterator, class Generator>
    void generate (ForwardIterator first, ForwardIterator last, Generator gen){
    }

    template <class InputIterator, class OutputIterator>
    OutputIterator copy (InputIterator first, InputIterator last, OutputIterator result){
    }

    template <class ForwardIterator, class T>
    ForwardIterator remove (ForwardIterator first, ForwardIterator last, const T& val){
    }

    template <class ForwardIterator>
    ForwardIterator unique (ForwardIterator first, ForwardIterator last){
    }

    template <class ForwardIterator, class BinaryPredicate>
    ForwardIterator unique (ForwardIterator first, ForwardIterator last, BinaryPredicate pred){
    }

    template <class RandomAccessIterator>
    void sort (RandomAccessIterator first, RandomAccessIterator last){
    }

    template <class RandomAccessIterator, class Compare>
    void sort (RandomAccessIterator first, RandomAccessIterator last, Compare comp){
    }

    template< class InputIt, class T>
    T reduce( InputIt first, InputIt last, T init ){
    }

    template< class InputIt, class T, class BinaryOperation>
    T reduce( InputIt first, InputIt last, T init, BinaryOperation op ){
    }


    template <class InputIterator, class T>
    InputIterator find (InputIterator first, InputIterator last, const T& val){
    }

}

#endif
