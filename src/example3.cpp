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
  
  MMappedRegion r1( 1024*1024*16 ); // 16 mb mapping
  if( !r1.init() )
    exit( 1 );
  
  {
    ArenaAlloc::Alloc<int, MMappedRegion> alloc( 32768, r1 );
    std::vector<int, ArenaAlloc::Alloc<int,MMappedRegion> > v1( alloc );
    
    for( size_t i = 0; i < 100; i++ )
    {
      v1.push_back(i);
    }    

    for( int i : v1 )
    {
      std::cout << i << " ";
    }
    std::cout << std::endl;    
  }

  
  r1.dispose();  
  return 0;
}






