// -*- c++ -*-
/*****************************************************************************
 **  arenaallocimpl.h
 **  
 **  Internal implementation types of the arena allocator
 **  MIT license
 *****************************************************************************/

#ifndef _ARENA_ALLOC_IMPL_H
#define _ARENA_ALLOC_IMPL_H

#ifdef ARENA_ALLOC_DEBUG
#include <stdio.h>
#endif

namespace ArenaAlloc
{

  template< typename T, typename A >
  class Alloc;
  
  // internal structure for tracking memory blocks
  template < typename AllocImpl >
  struct _memblock
  {
    // allocations are rounded up to a multiple of the size of this
    // struct to maintain proper alignment for any pointer and double
    // values stored in the allocation.
    // A future goal is to support even stricter alignment for example
    // to support cache alignment, special device  dependent mappings,
    // or GPU ops.    
    union _roundsize
    {
      double d;
      void * p;
    };
    
    _memblock * m_next; // blocks kept link listed for cleanup at end
    std::size_t m_bufferSize; // size of the buffer
    std::size_t m_index; // index of next allocatable byte in the block
    char * m_buffer; // pointer to large block to allocate from
    
    _memblock( std::size_t bufferSize, AllocImpl& allocImpl ):
      m_bufferSize( roundSize( bufferSize ) ),
      m_index( 0 ),
      m_buffer( reinterpret_cast<char*>( allocImpl.allocate( bufferSize ) ) ) // this works b/c of order of decl
    {
    }

    std::size_t roundSize( std::size_t numBytes )
    {
      // this is subject to overflow.  calling logic should not permit
      // an attempt to allocate a really massive size.
      // i.e. an attempt to allocate 10s of terabytes should be an error      
      return ( ( numBytes + sizeof( _roundsize ) - 1 ) / 
	       sizeof( _roundsize ) ) * sizeof( _roundsize );
    }

    char * allocate( std::size_t numBytes )
    {
      std::size_t roundedSize = roundSize( numBytes );
      if( roundedSize + m_index > m_bufferSize )
	return 0;

      char * ptrToReturn = &m_buffer[ m_index ];
      m_index += roundedSize;
      return ptrToReturn;
    }
  
    void dispose( AllocImpl& impl )
    {
      impl.deallocate( m_buffer );
    }

    ~_memblock()
    {
    }  
  
  };


  // Each allocator points to an instance of _memblockimpl which
  // contains the list of _memblock objects and other tracking info
  // including a refcount.
  // This object is instantiated in space obtained from the allocator
  // implementation. The allocator implementation is the component
  // on which allocate/deallocate are called to obtain storage from.
  template< typename AllocatorImpl >
  struct _memblockimpl
  { 
    
  private:
    // to get around some sticky access issues between Alloc<T1> and Alloc<T2> when sharing
    // the implementation.
    template <typename U, typename A>
    friend class Alloc;

    template< typename T >
    static void assign( const Alloc<T,AllocatorImpl>& src, _memblockimpl *& dest );
    
    AllocatorImpl m_alloc;
    std::size_t m_refCount; // when refs -> 0 delete this
    std::size_t m_defaultSize;
    
    // when m_numAllocate is large and m_numDeallocate approaches m_numAllocate
    // there may be significant free space available which can be 
    // reclaimed by copying the remaining active elements into a new 
    // a container with a new instance of the allocator and then
    // deleting the old allocator.
    
    std::size_t m_numAllocate; // number of times allocate called
    std::size_t m_numDeallocate; // number of time deallocate called
    std::size_t m_numBytesAllocated; // A good estimate of amount of space used
    
    _memblock<AllocatorImpl> * m_head;
    _memblock<AllocatorImpl> * m_current;
    
    static _memblockimpl<AllocatorImpl> * create( size_t defaultSize, AllocatorImpl& alloc )
    {
      return new ( alloc.allocate( sizeof( _memblockimpl ) ) ) _memblockimpl<AllocatorImpl>( defaultSize, alloc );
    }
   
    static void destroy( _memblockimpl<AllocatorImpl> * objToDestroy )
    {
      
      AllocatorImpl allocImpl = objToDestroy->m_alloc;
      objToDestroy-> ~_memblockimpl<AllocatorImpl>();
      allocImpl.deallocate( objToDestroy );      
    }
    
    _memblockimpl( std::size_t defaultSize, AllocatorImpl& allocImpl ):
      m_head(0),
      m_current(0),
      m_defaultSize( defaultSize ),
      m_refCount( 1 ),
      m_alloc( allocImpl ),
      m_numBytesAllocated( 0 ),
      m_numAllocate( 0 ),
      m_numDeallocate( 0 )
    {
      // This is practical software.  Avoid academic matters at all cost
      if( m_defaultSize < 256 )
	m_defaultSize = 256; 
      
      allocateNewBlock( m_defaultSize );

#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "_memblockimpl=%p constructed with default size=%ld\n", this, m_defaultSize );
#endif

    }

    char * allocate( std::size_t numBytes )
    {
      char * ptrToReturn = m_current->allocate( numBytes );
      if( !ptrToReturn )
      {
	allocateNewBlock( numBytes > m_defaultSize / 2 ? numBytes*2 : 
			  m_defaultSize );

	ptrToReturn = m_current->allocate( numBytes );	
      }

#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "_memblockimpl=%p allocated %ld bytes at address=%p\n", this, numBytes, ptrToReturn );
#endif

      ++ m_numAllocate;
      m_numBytesAllocated += numBytes; // does not account for the small overhead in tracking the allocation
      
      return ptrToReturn;
    }
    
    void deallocate( void * ptr )
    {
      ++ m_numDeallocate;
    }
  
    ~_memblockimpl( )
    {
#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "~memblockimpl() called on _memblockimpl=%p\n", this );
#endif

      _memblock<AllocatorImpl> * block = m_head;
      while( block )
      {
	_memblock<AllocatorImpl> * curr = block;
	block = block->m_next;
	curr->dispose( m_alloc );
	curr->~_memblock<AllocatorImpl>();
	m_alloc.deallocate( curr );
      }
    }
  
    // The ref counting model does not permit the sharing of 
    // this object across multiple threads unless an external locking mechanism is applied 
    // to ensure the atomicity of the reference count.  
    void incrementRefCount() 
    { 
      ++m_refCount; 
#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "ref count on _memblockimpl=%p incremented to %ld\n", this, m_refCount );
#endif      
    }

    void decrementRefCount()
    {
      --m_refCount;
#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "ref count on _memblockimpl=%p decremented to %ld\n", this, m_refCount );
#endif      
      
      if( m_refCount == 0 )
      {
	_memblockimpl<AllocatorImpl>::destroy( this );
      }
    }          
    
    void allocateNewBlock( std::size_t blockSize )
    {
      
      _memblock<AllocatorImpl> * newBlock = new ( m_alloc.allocate( sizeof( _memblock<AllocatorImpl> ) ) )
	_memblock<AllocatorImpl>( blockSize, m_alloc );
						  
#ifdef ARENA_ALLOC_DEBUG
      fprintf( stdout, "_memblockimpl=%p allocating a new block of size=%ld\n", this, blockSize );
#endif      
      
      if( m_head == 0 )
      {
	m_head = m_current = newBlock;
      }
      else
      {
	m_current->m_next = newBlock;
	m_current = newBlock;
      }      
    }    
    
    size_t getNumAllocations() { return m_numAllocate; }
    size_t getNumDeallocations() { return m_numDeallocate; }
    size_t getNumBytesAllocated() { return m_numBytesAllocated; }
  };


}

#endif
