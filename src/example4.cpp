/******************************************************************************
 **  example4.cpp 
 **
 **  Fun w/ c++ 11 forwarding
 **  MIT license
 *****************************************************************************/

#include "recyclealloc.h"
#include <iostream>

// compile with:
// g++ -std=c++11 example4.cpp

struct ExampleStruct
{

  ExampleStruct( int i, double d )
  {
    std::cout << "ExampleStruct(i,d);" << std::endl;
  }

  ExampleStruct( ExampleStruct& e )
  {
    std::cout << "ExampleStruct( ExampleStruct& ); " << std::endl;
  }

};


int main()
{
  
  ArenaAlloc::RecycleAlloc<char> charAlloc;

  ExampleStruct* e = reinterpret_cast<ExampleStruct*>( charAlloc.allocate( sizeof( ExampleStruct ) ) );
  charAlloc.construct( e, 10, 100.0 );

  
  ExampleStruct * f = reinterpret_cast<ExampleStruct*>( charAlloc.allocate( sizeof( ExampleStruct ) ) );
  charAlloc.construct( f, *e ); 
  
  return 0;
}
