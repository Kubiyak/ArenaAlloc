ArenaAlloc
==============

Arena allocator for STL containers.  Based on the sample code provided by Nicolai Josuttis available from here:
http://www.josuttis.com/libbook/examples.html

License
=======
MIT license


Intended Pattern Of Usage
=========================

1.  An instance or instances of arena allocator are created and are used to instantiate contents in various containers.
2.  The containers are used.
3.  The containers reach end of life and are destructed along with their contents.
4.  The instances of arena allocator are destructed at which time there should be no live references whatsover to the objects which were allocated with the arena objects.
5.  Each thread must have its own set of arena allocator objects.  It's possible to share arena allocator instances between threads but that would require locking and defeats one of the main aims of this code.  
6.  Read access to containers between threads is permissible as long as the arena instances used to instantiate the containers remain live while such accesses are possible.
7.  Memory is only freed when the allocator instance is destructed.  See the next note on reclaiming memory.

Reclaiming Memory
=================

The intent of this code is to provide an allocator for code which conforms in whole or substantially with the pattern of usage described above.  The allocator will NOT re-use deleted resources directly.  In order to improve memory usage characteristics, the application should implement a generational garbage collection strategy as needed.  What that means is, periodically, copy stuff you need to keep around into a different container backed by a different allocator.  Then clean up the original containers and the allocator backing those containers.  Examples will be provided for further clarification.

Caveats
=======

Expect memory usage to be somewhat higher when using the arena allocator type with containers such as std::map and std::set as the allocator instance has some data in it and every node stored in std::map and std::set contains an allocator object.  This overhead should be about 16 bytes in a 64 bit application per std::map or std::set node.  That's the cost of avoiding the much more expensive malloc calls incurred when using the standard allocator.  Note also that memory allocated with malloc has its own overhead in terms of a header on the block etc.  Thus the true memory overhead of this allocator library in comparison to the standard allocator provided with STL may be lower than it seems.  If the overhead is of concern, I would recommend measuring what the actual impact is before making a design decision.  Another suggestion would be to only use std::set and std::map instantiations on struct types with significant data in them in lieu of maps and sets of simple types such as ints.


