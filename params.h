/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef __PARAMS_H__
#define __PARAMS_H__

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
	/** @brief Verbosity (0 = quiet; 1 = noisy; 2 = noisier) */
	int verbose;

	/** @brief Avoid executing yields. */
	bool noexecyields;

	/** @brief Only explore branches. */
	bool branches;

	/** @brief Command-line argument count to pass to user program */
	int argc;

	/** @brief Command-line arguments to pass to user program */
	char **argv;
};

#endif/* __PARAMS_H__ */
