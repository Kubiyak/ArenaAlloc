/******************************************************************************
 ** example3.cpp:
 ** Demonstrates usage of the arena allocator for allocation from an 
 ** mmapped region.  Future examples will extend this to a multi-process
 ** scenario with appropriate locking and lock free semantics.
 **
 ** Released under the terms of the MIT license
 *****************************************************************************/
#include <stdlib.h>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <sys/mman.h>
#include <stdexcept>
#include <errno.h>
#include <vector>
#include <iostream>
#include "arenaalloc.h"

struct MMappedRegion
{
  
  char * m_addr;
  size_t m_numTotalBytes;
  size_t m_numBytesAllocated;  
  
  MMappedRegion( size_t numBytes ):
    m_addr(0),
    m_numTotalBytes( numBytes ),
    m_numBytesAllocated( 0 )
  {
  }
  
  // must be called to map the region
  bool init()
  {
    if( m_addr )
      return true;

    m_addr = reinterpret_cast<char*>( mmap( 0, m_numTotalBytes, 
					    PROT_READ | PROT_WRITE, 
					    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 ) );    
    if( m_addr == ((char*)-1) )
    {      
      fprintf( stderr, "MMappedRegion::init failed: errno=%d\n", errno );
      m_addr = 0;
      return false;
    }
    
    return true;
  }
  
  void dispose()
  {
    if( m_addr )
    {
      munmap( m_addr, m_numTotalBytes );
      m_addr = 0;
    }
  }
  
  void * allocate( size_t numBytes )
  {
    if( m_numTotalBytes > m_numBytesAllocated + numBytes )
    {
      char * addrToReturn =  m_addr + m_numBytesAllocated;
      m_numBytesAllocated += numBytes;
      
      fprintf( stderr, "MMappedRegion: allocating %ld bytes. addr=%p\n",
	       numBytes, addrToReturn );
	       
      return addrToReturn;
    }
    else
    {
      // this won't happen in the example.
      throw std::runtime_error( "Insufficient space in mapping" );
    }
  }
  
  void deallocate( void * addr )
  {
    // does nothing.
    // todo: expand on this example into a more robust mmap backed
    // allocator.
    fprintf( stderr, "MMappedRegion deallocating addr=%p\n",
	     addr );
    
  }
  
};


int main()
{
  
  MMappedRegion r1( 1024*1024*8 ); // 8 mb mapping
  MMappedRegion r2( 1024*1024*8 );
  if( !( r1.init() && r2.init() ) )
    exit( 1 );
  
  {
    auto alloc1 = new ArenaAlloc::Alloc<int, MMappedRegion> ( 32768, r1 );
    auto v1 = new std::vector<int, ArenaAlloc::Alloc<int,MMappedRegion> >( *alloc1 );
    
    // inserting into an array will cause several deallocations of the internal
    // array which become outsized.
    for( size_t i = 0; i < 1024; i++ )
    {
      v1->push_back(i);
    }    
    
    std::cout << "Num bytes allocated in original allocator: " << alloc1->getNumBytesAllocated() << std::endl;
    
    // now create a new vector with a new allocator and copy in the original
    // note that vector.swap is ineffective for recovering space.  
    auto alloc2 = new ArenaAlloc::Alloc<int,MMappedRegion>( 32768, r2 );
    auto v2 = new std::vector<int, ArenaAlloc::Alloc<int,MMappedRegion> >( *v1, *alloc2 );
    
    delete v1;
    delete alloc1;
    
    std::cout << "Num bytes allocated in second allocator: " << alloc2->getNumBytesAllocated() << std::endl;
    delete v2;
    delete alloc2;
  }
  
  r1.dispose();
  r2.dispose();
  return 0;
}






