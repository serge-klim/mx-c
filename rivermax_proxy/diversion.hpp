#pragma once
#include "rmaxx/rmaxx_utils.hpp"

extern "C" {

rmax_status_t diversion_rmax_init(struct rmax_init_config *init_config);

rmax_status_t diversion_rmax_init_version(unsigned api_major_version,
                                       unsigned api_minor_version,
                                       struct rmax_init_config* init_config);



rmax_status_t diversion_rmax_get_version(unsigned* major_version, unsigned* minor_version,
                               unsigned *release_number, unsigned *build);

 
}

