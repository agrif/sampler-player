# TCL File Generated by Component Editor 13.1
# Wed Jun 22 14:23:23 EDT 2016
# Has since been modified.


# 
# player "player" v1.0
# Aaron Griffith 2016.06.22.14:23:23
# Memory-mapped player that plays itself out for a short burst of time.
# 

# 
# request TCL package from ACDS 13.1
# 
package require -exact qsys 13.1


# 
# module player
# 
set_module_property DESCRIPTION "Memory-mapped player that plays itself out for a short burst of time."
set_module_property NAME player
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR "Aaron Griffith"
set_module_property DISPLAY_NAME player
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property ANALYZE_HDL AUTO
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property ELABORATION_CALLBACK elaborate

# device tree
set_module_assignment embeddedsw.dts.vendor "osuql"
set_module_assignment embeddedsw.dts.name "player"
set_module_assignment embeddedsw.dts.group "sampler-player"
# set_module_assignment embeddedsw.dts.compatible "osuql,player"


# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL qsys_player
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
add_fileset_file player.v VERILOG PATH player.v TOP_LEVEL_FILE

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL qsys_player
set_fileset_property SIM_VERILOG ENABLE_RELATIVE_INCLUDE_PATHS false
add_fileset_file player.v VERILOG PATH player.v


# 
# parameters
#
add_parameter outputBits POSITIVE 1 "number of bits of data to play at each step"
set_parameter_property outputBits DEFAULT_VALUE 32
set_parameter_property outputBits DISPLAY_NAME "Output Width"
set_parameter_property outputBits WIDTH ""
set_parameter_property outputBits TYPE POSITIVE
set_parameter_property outputBits UNITS bits
set_parameter_property outputBits ALLOWED_RANGES 1:32768
set_parameter_property outputBits DESCRIPTION ""
set_parameter_property outputBits HDL_PARAMETER true
set_parameter_property outputBits DESCRIPTION "number of bits of data to play at each step"
add_parameter words POSITIVE 1 "number of 32 bit words of data to play at each step"
set_parameter_property words DERIVED true
set_parameter_property words DEFAULT_VALUE 1
set_parameter_property words DISPLAY_NAME "Output Width (in 32-bit Words)"
set_parameter_property words WIDTH ""
set_parameter_property words TYPE POSITIVE
set_parameter_property words UNITS None
set_parameter_property words ALLOWED_RANGES 1:1024
set_parameter_property words DESCRIPTION ""
set_parameter_property words HDL_PARAMETER true
set_parameter_property words DESCRIPTION "number of 32 bit words of data to play at each step"
add_parameter words_log_2 NATURAL 0 "number of 32 bit words of data to play at each step (log-2'd)"
set_parameter_property words_log_2 DERIVED true
set_parameter_property words_log_2 DEFAULT_VALUE 0
set_parameter_property words_log_2 DISPLAY_NAME "Output Width (log2)"
set_parameter_property words_log_2 WIDTH ""
set_parameter_property words_log_2 TYPE NATURAL
set_parameter_property words_log_2 UNITS None
set_parameter_property words_log_2 ALLOWED_RANGES 0:10
set_parameter_property words_log_2 DESCRIPTION "number of 32 bit words of data to play at each step (log-2'd)"
set_parameter_property words_log_2 HDL_PARAMETER true
add_parameter timeBits NATURAL 10 "number of bits of time data to keep"
set_parameter_property timeBits DEFAULT_VALUE 10
set_parameter_property timeBits DISPLAY_NAME "Time Width"
set_parameter_property timeBits WIDTH ""
set_parameter_property timeBits TYPE POSITIVE
set_parameter_property timeBits UNITS bits
set_parameter_property timeBits ALLOWED_RANGES 1:32
set_parameter_property timeBits DESCRIPTION "number of bits of time data to keep"
set_parameter_property timeBits HDL_PARAMETER true
add_parameter addrBits POSITIVE 1 "total bits of address space occupied"
set_parameter_property addrBits DERIVED true
set_parameter_property addrBits DEFAULT_VALUE 12
set_parameter_property addrBits DISPLAY_NAME "Address Size"
set_parameter_property addrBits WIDTH ""
set_parameter_property addrBits TYPE POSITIVE
set_parameter_property addrBits UNITS bits
set_parameter_property addrBits ALLOWED_RANGES 1:32
set_parameter_property addrBits DESCRIPTION ""
set_parameter_property addrBits HDL_PARAMETER false
set_parameter_property addrBits DESCRIPTION "total bits of address space occupied"


#
# elaboration
#
proc elaborate {} {
    set our_bits [get_parameter_value outputBits]
    set our_words [expr {ceil($our_bits / 32.0)}]
    set our_words_log_2 [expr {ceil(log($our_words)/log(2))}]
    set our_time_bits [get_parameter_value timeBits]
    set our_sample_bits [expr {int($our_words_log_2 + 2)}]
    set our_addr_bits [expr {$our_sample_bits + $our_time_bits}]
    set_parameter_value words $our_words
    set_parameter_value words_log_2 $our_words_log_2
    set_parameter_value addrBits $our_addr_bits

    # set up system.h
    set_module_assignment embeddedsw.CMacro.WIDTH $our_bits
    set_module_assignment embeddedsw.CMacro.TIME_BITS $our_time_bits
    set_module_assignment embeddedsw.CMacro.SAMPLE_BITS $our_sample_bits

    # set up device tree
    set_module_assignment embeddedsw.dts.params.sample-width $our_bits
    set_module_assignment embeddedsw.dts.params.time-bits $our_time_bits
    set_module_assignment embeddedsw.dts.params.sample-bits $our_sample_bits
}


# 
# display items
# 


# 
# connection point buffer_clk
# 
add_interface buffer_clk clock end
set_interface_property buffer_clk clockRate 0
set_interface_property buffer_clk ENABLED true
set_interface_property buffer_clk EXPORT_OF ""
set_interface_property buffer_clk PORT_NAME_MAP ""
set_interface_property buffer_clk CMSIS_SVD_VARIABLES ""
set_interface_property buffer_clk SVD_ADDRESS_GROUP ""

add_interface_port buffer_clk clk clk Input 1


# 
# connection point buffer_reset
# 
add_interface buffer_reset reset end
set_interface_property buffer_reset associatedClock buffer_clk
set_interface_property buffer_reset synchronousEdges DEASSERT
set_interface_property buffer_reset ENABLED true
set_interface_property buffer_reset EXPORT_OF ""
set_interface_property buffer_reset PORT_NAME_MAP ""
set_interface_property buffer_reset CMSIS_SVD_VARIABLES ""
set_interface_property buffer_reset SVD_ADDRESS_GROUP ""

add_interface_port buffer_reset reset_n reset_n Input 1


# 
# connection point csr
# 
add_interface csr avalon end
set_interface_property csr addressUnits WORDS
set_interface_property csr associatedClock buffer_clk
set_interface_property csr associatedReset buffer_reset
set_interface_property csr bitsPerSymbol 8
set_interface_property csr burstOnBurstBoundariesOnly false
set_interface_property csr burstcountUnits WORDS
set_interface_property csr explicitAddressSpan 0
set_interface_property csr holdTime 0
set_interface_property csr linewrapBursts false
set_interface_property csr maximumPendingReadTransactions 0
set_interface_property csr readLatency 0
set_interface_property csr readWaitTime 1
set_interface_property csr setupTime 0
set_interface_property csr timingUnits Cycles
set_interface_property csr writeWaitTime 0
set_interface_property csr ENABLED true
set_interface_property csr EXPORT_OF ""
set_interface_property csr PORT_NAME_MAP ""
set_interface_property csr CMSIS_SVD_VARIABLES ""
set_interface_property csr SVD_ADDRESS_GROUP ""

add_interface_port csr csr_write write Input 1
add_interface_port csr csr_writedata writedata Input 32
add_interface_port csr csr_read read Input 1
add_interface_port csr csr_readdata readdata Output 32
set_interface_assignment csr embeddedsw.configuration.isFlash 0
set_interface_assignment csr embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment csr embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment csr embeddedsw.configuration.isPrintableDevice 0


# 
# connection point buffer
# 
add_interface buffer avalon end
set_interface_property buffer addressUnits WORDS
set_interface_property buffer associatedClock buffer_clk
set_interface_property buffer associatedReset buffer_reset
set_interface_property buffer bitsPerSymbol 8
set_interface_property buffer burstOnBurstBoundariesOnly false
set_interface_property buffer burstcountUnits WORDS
set_interface_property buffer explicitAddressSpan 0
set_interface_property buffer holdTime 0
set_interface_property buffer linewrapBursts false
set_interface_property buffer maximumPendingReadTransactions 0
set_interface_property buffer readLatency 0
set_interface_property buffer readWaitTime 1
set_interface_property buffer setupTime 0
set_interface_property buffer timingUnits Cycles
set_interface_property buffer writeWaitTime 0
set_interface_property buffer ENABLED true
set_interface_property buffer EXPORT_OF ""
set_interface_property buffer PORT_NAME_MAP ""
set_interface_property buffer CMSIS_SVD_VARIABLES ""
set_interface_property buffer SVD_ADDRESS_GROUP ""

add_interface_port buffer buffer_write write Input 1
add_interface_port buffer buffer_address address Input timeBits+words_log_2
add_interface_port buffer buffer_writedata writedata Input 32
set_interface_assignment buffer embeddedsw.configuration.isFlash 0
set_interface_assignment buffer embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment buffer embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment buffer embeddedsw.configuration.isPrintableDevice 0


# 
# connection point done
# 
add_interface done interrupt end
set_interface_property done associatedAddressablePoint csr
set_interface_property done associatedClock buffer_clk
set_interface_property done associatedReset buffer_reset
set_interface_property done ENABLED true
set_interface_property done EXPORT_OF ""
set_interface_property done PORT_NAME_MAP ""
set_interface_property done CMSIS_SVD_VARIABLES ""
set_interface_property done SVD_ADDRESS_GROUP ""

add_interface_port done irq irq Output 1


# 
# connection point play_clk
# 
add_interface play_clk clock end
set_interface_property play_clk clockRate 0
set_interface_property play_clk ENABLED true
set_interface_property play_clk EXPORT_OF ""
set_interface_property play_clk PORT_NAME_MAP ""
set_interface_property play_clk CMSIS_SVD_VARIABLES ""
set_interface_property play_clk SVD_ADDRESS_GROUP ""

add_interface_port play_clk r_clk clk Input 1


# 
# connection point play_reset
#
add_interface play_reset reset start
set_interface_property play_reset associatedClock buffer_clk
set_interface_property play_reset associatedDirectReset ""
set_interface_property play_reset associatedResetSinks buffer_reset
set_interface_property play_reset synchronousEdges DEASSERT
set_interface_property play_reset ENABLED true
set_interface_property play_reset EXPORT_OF ""
set_interface_property play_reset PORT_NAME_MAP ""
set_interface_property play_reset CMSIS_SVD_VARIABLES ""
set_interface_property play_reset SVD_ADDRESS_GROUP ""

add_interface_port play_reset r_reset_n reset_n Output 1


# 
# connection point play
# 
add_interface play conduit end
set_interface_property play associatedClock play_clk
set_interface_property play associatedReset ""
set_interface_property play ENABLED true
set_interface_property play EXPORT_OF ""
set_interface_property play PORT_NAME_MAP ""
set_interface_property play CMSIS_SVD_VARIABLES ""
set_interface_property play SVD_ADDRESS_GROUP ""

add_interface_port play r_out export Output outputBits


# 
# connection point play_enable
# 
add_interface play_enable conduit end
set_interface_property play_enable associatedClock buffer_clk
set_interface_property play_enable associatedReset ""
set_interface_property play_enable ENABLED true
set_interface_property play_enable EXPORT_OF ""
set_interface_property play_enable PORT_NAME_MAP ""
set_interface_property play_enable CMSIS_SVD_VARIABLES ""
set_interface_property play_enable SVD_ADDRESS_GROUP ""

add_interface_port play_enable r_enable export Input 1
