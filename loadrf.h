/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef LOADRF_H
#define LOADRF_H
#include "classlist.h"

class LoadRF {
public:
	LoadRF(EPRecord *_load, ConstGen *cg);
	~LoadRF();
	void genConstraints(ConstGen *cg);

	MEMALLOC;
private:
	EPRecord *load;
	uint numvars;
	Constraint ** vars;
};
#endif
