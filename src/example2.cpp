/*******************************************************************************
 * example2.cpp
 * Measure time to write to containers in one of 2 modes:
 * 1.  using the standard STL allocator
 * 2.  using ArenaAlloc
 *
 * This software is distributed under the terms of the MIT license.  
 * Don't panic!!!! Share and enjoy.
 *****************************************************************************/
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include "arenaalloc.h"
#include "recyclealloc.h"

// As promised in example1.cpp, this example requires c++11

// compile as: g++ -O2 -DARENA_ALLOCATOR_TEST -std=c++11 -o example2-arena example2.cpp -lpthread
// to test the arena allocator logic

// compile as: g++ -O2 -std=c++11 -o example2-stdalloc example2.cpp -lpthread
// to test the standard allocator.

// If tcmalloc is available try this also:
// g++ -O2 -std=c++11 -fno-builtin-malloc -o example2-stdtcmalloc example2.cpp -ltcmalloc -lpthread
// g++ -O2 -DARENA_ALLOCATOR_TEST -std=c++11 -fno-builtin-malloc -o example2-arenatcmalloc example2.cpp -ltcmalloc -lpthread

// In my testing of this not perfectly scientific example, the standard allocator + tcmalloc was faster by
// a little bit over the arena allocator with the standard malloc.  Both of those were significantly 
// faster than the standard allocator with the standard malloc.  I kept getting a core dump when 
// running the arena allocator with tcmalloc so I cannot present results on that.  Give it a try though.
// It's possible there may be a problem in how my code is structured for tcmalloc to work with the arena allocator.

typedef std::chrono::high_resolution_clock::time_point hres_t;
typedef std::chrono::duration<size_t, std::ratio<1,1000000> > duration_t; // duration in micro-seconds

struct task
{    
  std::mutex& m_mutexRef;
  std::condition_variable& m_condVarRef;
  bool& m_run;

  task( std::mutex& m, std::condition_variable& condVar, bool& runVar ):
    m_mutexRef( m ),
    m_run( runVar ),
    m_condVarRef( condVar )
  {     
  }
   
  void  operator()()
  {     
    
    // barrier synchronize the threads as much as possible to start them as close as possible
    // to the same time.
    {
      std::unique_lock<std::mutex> lk( m_mutexRef );      
      std::cout << "Thread=" << std::this_thread::get_id() << " is waiting..." << std::endl;
      m_condVarRef.wait( lk, [this] { return m_run == true; }  ); // wait until the lambda condition is true
    }

    hres_t start = std::chrono::high_resolution_clock::now();
    
    // do map insert / delete
    std::size_t numBytesAllocated = doWork();
    
    hres_t end = std::chrono::high_resolution_clock::now();    
    duration_t timespan =  std::chrono::duration_cast< duration_t >( end - start );
    
    {
      // report timing results w/ lock held so that the output isn't all munged
      std::lock_guard<std::mutex> lg( m_mutexRef );
      std::cout << "threadid: " << std::this_thread::get_id() << " clicks: " << timespan.count();    
#if defined( ARENA_ALLOCATOR_TEST  ) || ( RECYCLE_ALLOCATOR_TEST )
      std::cout << " bytes allocated: " << numBytesAllocated << std::endl;
#else
      std::cout << std::endl;
#endif
    }
  }    
  
  std::size_t doWork()
  {
    
    // insert a million values into a map<int,strtype> using the specified map type
#if defined (ARENA_ALLOCATOR_TEST)
    typedef std::basic_string<char, std::char_traits<char>, ArenaAlloc::Alloc<char> > strtype;
    ArenaAlloc::Alloc< char > charAllocator( 65536 );
    strtype answerToEverything( "42", charAllocator );
    
    ArenaAlloc::Alloc< std::pair< const int, strtype > > alloc( 65536 );
    std::map< int, strtype , std::less<int>, ArenaAlloc::Alloc<std::pair<const int,strtype> > > intToStrMap( std::less<int>(), alloc );    
#elif defined(RECYCLE_ALLOCATOR_TEST)
    typedef std::basic_string<char, std::char_traits<char>, ArenaAlloc::RecycleAlloc<char> > strtype;
    ArenaAlloc::RecycleAlloc< char > charAllocator( 65536 );
    strtype answerToEverything( "42", charAllocator );
    
    ArenaAlloc::RecycleAlloc< std::pair< const int, strtype > > alloc( 65536 );    
    std::map< int, strtype , std::less<int>, ArenaAlloc::RecycleAlloc<std::pair<const int,strtype> > > 
      intToStrMap( std::less<int>(), alloc );      
#else 
    typedef std::string strtype;
    std::map<int, strtype> intToStrMap;
    std::string answerToEverything( "42" );
    std::allocator<char> charAllocator();
#endif
    
    for( int i = 0; i < 10000000; i++ )
    {      
      intToStrMap.insert( std::pair< const int, strtype >( i, answerToEverything ) ); // The answer to everything
      
      if( i > 10 && ( i % 5 == 0 ) )
	intToStrMap.erase( i - 5 ); // to provide some re-use for for testing the recycle allocator
    }
    
    // Note: time to clear allocator is included in the runtime.
#if defined( ARENA_ALLOCATOR_TEST ) || ( RECYCLE_ALLOCATOR_TEST )
    return charAllocator.getNumBytesAllocated() + alloc.getNumBytesAllocated();
#else
    return 0;
#endif
  }
  
};

int main()
{
  
  size_t concurrency = std::thread::hardware_concurrency();
  std::cout << "Running with " << concurrency << " threads" << std::endl;
  
  std::vector< std::thread > threads;

  std::mutex m;
  std::condition_variable cv;
  bool runBool = false;
  
  for( size_t i = 0; i < concurrency; i++ )
  {
    task t( m, cv, runBool );
    threads.push_back( std::thread( t ) );
  }
  
  std::cout << "Waiting a bit before waking the stalled threads" << std::endl;
  std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
  
  std::cout << "And they're off...!" << std::endl;
  runBool = true;
  cv.notify_all();

  std::for_each( threads.begin(), threads.end(), std::mem_fn( &std::thread::join ) );  
  return 0;
}
