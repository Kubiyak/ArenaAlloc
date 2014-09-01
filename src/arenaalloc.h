// -*- c++ -*-
/******************************************************************************
 *  arenaalloc.h
 *  
 *  Arena allocator based on the example logic provided by Nicolai Josuttis
 *  and available at http://www.josuttis.com/libbook/examples.html.
 *  This enhanced work is provided under the terms of the MIT license.
 *  
 *****************************************************************************/

#ifndef _ARENA_ALLOC_H
#define _ARENA_ALLOC_H

#include <limits>
#include <memory>

// Define macro ARENA_ALLOC_DEBUG to enable some tracing of the allocator

#ifdef ARENA_ALLOC_DEBUG
#include <stdio.h>
#endif

namespace ArenaAlloc 
{
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

  template< typename T, typename A >
  class Alloc;
  
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
    std::size_t m_num; // number of times allocate called
    
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
      m_alloc( allocImpl )
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
      return ptrToReturn;
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
    void incrementRefCount() { 
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
  private:
    
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
  };

  struct _newAllocatorImpl
  {
    // these two functions should be supported by a specialized
    // allocator for shared memory or another source of specialized
    // memory such as device mapped memory.
    void* allocate( size_t numBytes ) { return new char[ numBytes ]; }
    void deallocate( void* ptr ) { delete[]( (char*)ptr ); }
  };
  
  template <class T, class AllocatorImpl = _newAllocatorImpl >
  class Alloc {
    
  private:    
    _memblockimpl<AllocatorImpl> * m_impl;    
    
  public:
    // type definitions
    typedef T        value_type;
    typedef T*       pointer;
    typedef const T* const_pointer;
    typedef T&       reference;
    typedef const T& const_reference;
    typedef std::size_t    size_type;
    typedef std::ptrdiff_t difference_type;

    // rebind allocator to type U
    template <class U>
    struct rebind {
      typedef Alloc<U,AllocatorImpl> other;
    };
   
    // return address of values
    pointer address (reference value) const {
      return &value;
    }
    const_pointer address (const_reference value) const {
      return &value;
    }

    Alloc( std::size_t defaultSize = 32768, AllocatorImpl allocImpl = AllocatorImpl() ) throw():
      m_impl( _memblockimpl<AllocatorImpl>::create( defaultSize, allocImpl ) )
    {      
    }
    
    Alloc(const Alloc& src)  throw(): 
      m_impl( src.m_impl )
    {
      m_impl->incrementRefCount();
    }
    
    
    friend struct _memblockimpl<AllocatorImpl>;
    
    template <class U>
    Alloc (const Alloc<U,AllocatorImpl>& src) throw(): 
      m_impl( 0 )
    {
      _memblockimpl<AllocatorImpl>::assign( src, m_impl );
      m_impl->incrementRefCount();
    }
    
    ~Alloc() throw() 
    {
      m_impl->decrementRefCount();
    }

    // return maximum number of elements that can be allocated
    size_type max_size () const throw() 
    {
      return std::numeric_limits<std::size_t>::max() / sizeof(T);
    }

    // allocate but don't initialize num elements of type T
    pointer allocate (size_type num, const void* = 0) 
    {
      return reinterpret_cast<pointer>( m_impl->allocate(num*sizeof(T)) );
    }

    // initialize elements of allocated storage p with value value
    void construct (pointer p, const T& value) 
    {
      // initialize memory with placement new
      new((void*)p)T(value);
    }

    // destroy elements of initialized storage p
    void destroy (pointer p) 
    {
      // destroy objects by calling their destructor
      p->~T();
    }

    // deallocate storage p of deleted elements
    void deallocate (pointer p, size_type num) 
    {
      // Note: This does nothing.  Space reclaimed
      // when all allocators sharing the _impl in this
      // object go out of scope.      
    }
    
    bool equals( const _memblockimpl<AllocatorImpl> * impl ) const
    {
      return impl == m_impl;
    }
  
    bool operator == ( const Alloc& t2 ) const
    {
      return m_impl == t2.m_impl;
    }
  
    template <typename U>
    friend bool operator == ( const Alloc& t1, const Alloc<U,AllocatorImpl>& t2 ) throw();

    template <typename U>
    friend bool operator != ( const Alloc& t1, const Alloc<U,AllocatorImpl>& t2 ) throw();
    
  };

  // return that all specializations of this allocator sharing an implementation
  // are equal
  template <class T1, class T2, class T3>
  bool operator== (const Alloc<T1,T3>& t1,
		   const Alloc<T2,T3>& t2) throw() 
  {    
    return t2.equals ( t1.m_impl );
  }
  
  template <class T1, class T2, class T3>
  bool operator!= (const Alloc<T1,T3>& t1,
		   const Alloc<T2,T3>& t2) throw() 
  {
    return !( t2.equals( t1.m_impl ) );
  }

  template<typename A>
  template<typename T>
  void _memblockimpl<A>::assign( const Alloc<T,A>& src, _memblockimpl<A> *& dest )
  {
    dest = const_cast<_memblockimpl<A>* >(src.m_impl);
  }
    
}


#endif
