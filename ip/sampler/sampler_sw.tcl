# we are a hardware driver
create_driver sampler_driver
set_sw_property hw_class_name sampler

# versioning
set_sw_property version 1.0
set_sw_property min_compatible_hw_version 1.0

# these are our sources
add_sw_property c_source HAL/src/sampler.c
add_sw_property include_source HAL/inc/sampler.h
add_sw_property include_source inc/sampler_regs.h

# we support the HAL bsp
add_sw_property supported_bsp_type hal

# we support HAL auto-initialization
set_sw_property auto_initialize true

# we support both interrupt apis
set_sw_property supported_interrupt_apis "legacy_interrupt_api enhanced_interrupt_api"

