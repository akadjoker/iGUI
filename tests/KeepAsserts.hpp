// Forced into the test programs that are made of assert() (CMakeLists.txt, igui_test_keep_asserts), ahead of everything they include.
//
// In a build with NDEBUG assert() is gone, the body of each test stops running and a whole file tests nothing, so these targets take the
// NDEBUG away. But ct (containers) lets NDEBUG decide what its HashMap and HashSet are (CT_DEBUG_ITERATORS: a debug build keeps a table
// version in them, and the iterators check it), and that has to be the same in every file of a program: a test that took NDEBUG away
// and linked the library built with it had a HashSet of another size than the library's, and crashed in the destructor of a syntax
// highlighter. So ct is read here, with the NDEBUG the library was built with, and only then does assert() come back.
#include "ct/detail/utils.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
