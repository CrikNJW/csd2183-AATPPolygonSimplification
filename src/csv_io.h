#ifndef CSV_IO_H
#define CSV_IO_H

#include "polygon_dcel.h"

#include <string>
#include <vector>

std::vector<RingInput> read_input_csv(const std::string& input_file);

#endif
