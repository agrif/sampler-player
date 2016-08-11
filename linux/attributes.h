// this file lists all readable device attributes
// to use it, define ATTRIBUTE, and include this file
//
// #define ATTRIBUTE(name)
// #include "attributes.h"
//
// to handle struct attributes / csr attributes differently, define
// STRUCT_ATTRIBUTE(name, format) or CSR_ATTRIBUTE(name, write, mask)

#ifndef STRUCT_ATTRIBUTE
#define STRUCT_ATTRIBUTE(name, format) ATTRIBUTE(name)
#endif

#ifndef CSR_ATTRIBUTE
#define CSR_ATTRIBUTE(name, write, mask) ATTRIBUTE(name)
#endif

STRUCT_ATTRIBUTE(sample_width, "%i")
STRUCT_ATTRIBUTE(sample_bits, "%i")
STRUCT_ATTRIBUTE(sample_length, "%i")
STRUCT_ATTRIBUTE(time_bits, "%i")
STRUCT_ATTRIBUTE(time_length, "%i")
STRUCT_ATTRIBUTE(bits, "%i")
STRUCT_ATTRIBUTE(length, "%i")

// disabled, I figure the ioctls are better for this
//CSR_ATTRIBUTE(enabled, 1, CSR_ENABLED)
//CSR_ATTRIBUTE(done, 0, CSR_DONE)

#undef STRUCT_ATTRIBUTE
#undef CSR_ATTRIBUTE
#undef ATTRIBUTE
