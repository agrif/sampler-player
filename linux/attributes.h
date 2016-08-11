// this file lists all readable device attributes
// to use it, define ATTRIBUTE, and include this file
//
// #define ATTRIBUTE(name)
// #include "attributes.h"
//
// to handle struct attributes / csr attributes differently, define
// STRUCT_ATTRIBUTE(name, format) or CSR_ATTRIBUTE(name, write, mask)

#ifndef STRUCT_ATTRIBUTE
#define STRUCT_ATTRIBUTE(name, ...) ATTRIBUTE(name)
#endif

#ifndef CSR_ATTRIBUTE
#define CSR_ATTRIBUTE(name, write, mask) ATTRIBUTE(name)
#endif

STRUCT_ATTRIBUTE(sample_width, "%i\n", sp->sample_width)
STRUCT_ATTRIBUTE(sample_bits, "%i\n", sp->sample_bits)
STRUCT_ATTRIBUTE(sample_length, "%i\n", sp->sample_length)
STRUCT_ATTRIBUTE(time_bits, "%i\n", sp->time_bits)
STRUCT_ATTRIBUTE(time_length, "%i\n", sp->time_length)
STRUCT_ATTRIBUTE(bits, "%i\n", sp->bits)
STRUCT_ATTRIBUTE(length, "%i\n", sp->length)
STRUCT_ATTRIBUTE(type, "%s\n", BY_TYPE(sp->type, "sampler", "player"))

// disabled, I figure the ioctls are better for this
//CSR_ATTRIBUTE(enabled, 1, CSR_ENABLED)
//CSR_ATTRIBUTE(done, 0, CSR_DONE)

#undef STRUCT_ATTRIBUTE
#undef CSR_ATTRIBUTE
#undef ATTRIBUTE
