/************************************************************************************************************************
 *
 * A modified version of the example provided by Nicolai Josuttis.  This enhanced version is provided under the
 * terms of the MIT license.
 *
 ***********************************************************************************************************************/

#include <vector>
#include <iostream>
#include <map>
#include <functional>

#include "arenaalloc.h"

#ifdef RECYCLE_ALLOCATOR_TEST
#include "recyclealloc.h"
#endif

// compile command:
// g++ example1.cpp 
// for extended tracing of the allocator do:
// g++ -DARENA_ALLOC_DEBUG example1.cpp
// Other examples will use c++11.  However, the allocator itself only uses C++98 constructs.
// Mileage will vary with compilers other than g++4.7 although this should work well with any
// g++ compiler since g++4.4.

int main()
{
    // create a vector, using ArenaAlloc::Alloc<int> as allocator
    std::vector<int,ArenaAlloc::Alloc<int> > v;
    
    // insert elements
    v.push_back(42);
    v.push_back(56);
    v.push_back(11);
    v.push_back(22);
    v.push_back(33);
    v.push_back(44);

    for( std::vector<int,ArenaAlloc::Alloc<int> >::iterator vItr = v.begin(); vItr != v.end(); ++vItr )
    {
      std::cout << *vItr << std::endl;
    }

    // Now lets try a map typedef'ed to use this allocator
#ifdef RECYCLE_ALLOCATOR_TEST
    // note this requires std=c++11 
    typedef std::basic_string< char, std::char_traits<char>, ArenaAlloc::RecycleAlloc<char> > mystring;
    typedef std::map< mystring, int, std::less<mystring>, ArenaAlloc::RecycleAlloc< std::pair< const mystring, int > > > mymap;    
    ArenaAlloc::RecycleAlloc<char> myCharAllocator( 256 );    
    ArenaAlloc::RecycleAlloc<std::pair<const mystring, int> > myMapAllocator( myCharAllocator );
#else
    typedef std::basic_string< char, std::char_traits<char>, ArenaAlloc::Alloc<char> > mystring;
    typedef std::map< mystring, int, std::less<mystring>, ArenaAlloc::Alloc< std::pair< const mystring, int > > > mymap;
    ArenaAlloc::Alloc<char> myCharAllocator( 256 );    
    ArenaAlloc::Alloc<std::pair<const mystring, int> > myMapAllocator( myCharAllocator );
#endif

    mystring m1( "hello world", myCharAllocator );

    std::cout << "mystring: " << m1 << std::endl;
        
    mymap map1( std::less< mystring >(), myMapAllocator );
    
    // to prevent the proliferation of unrelated instances of the allocator, some care must be taken when
    // constructing string types.  
    map1.insert( std::pair< const mystring, int >( mystring( "hello", myCharAllocator ), 1 ) );		  
    map1.insert( std::pair< const mystring, int >( mystring( "world", myCharAllocator ), 2 ) );
    
    for( mymap::iterator mItr = map1.begin(); mItr != map1.end(); ++mItr )
    {
      std::cout << mItr->first << ": " << mItr->second << std::endl;
    }

    // construct a large string to force addition of a new memory block
    mystring largeString( 255, 'c', myCharAllocator );

    // check equality of the allocators
    bool eq = myCharAllocator == myMapAllocator;
    bool neq = myCharAllocator != myMapAllocator;
    
    std::cout << "Char allocator == Map allocator: " << eq << std::endl;
    std::cout << "Char allocator != Map allocator: " << neq << std::endl;

    // other examples will explore the runtime characteristics of the allocator in the presence of threads in comparison
    // to the standard STL allocator.
        
    return 0;
}

