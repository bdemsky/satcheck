/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "output.h"
#include "model.h"

/* global "model" object */
#include "params.h"
#include "snapshot-interface.h"

static void param_defaults(struct model_params *params)
{
	params->branches = false;
	params->noyields = false;
	params->verbose = !!DBG_ENABLED();
}

static void print_usage(const char *program_name, struct model_params *params)
{
	/* Reset defaults before printing */
	param_defaults(params);

	model_print(
		"Model-checker options:\n"
		"-h, --help                  Display this help message and exit\n"
		"-Y, --avoidyields           Fairness support by not executing yields\n"
		"-b, --branches              Only explore all branches\n"
		"-v[NUM], --verbose[=NUM]    Print verbose execution information. NUM is optional:\n"
		" --                         Program arguments follow.\n\n");
	exit(EXIT_SUCCESS);
}

static void parse_options(struct model_params *params, int argc, char **argv)
{
	const char *shortopts = "hbYv::";
	const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"avoidyields", no_argument, NULL, 'Y'},
		{"branches", no_argument, NULL, 'b'},
		{"verbose", optional_argument, NULL, 'v'},
		{0, 0, 0, 0}						/* Terminator */
	};
	int opt, longindex;
	bool error = false;
	while (!error && (opt = getopt_long(argc, argv, shortopts, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0], params);
			break;
		case 'b':
			params->branches = true;
			break;
		case 'Y':
			params->noyields = true;
			break;
		case 'v':
			params->verbose = optarg ? atoi(optarg) : 1;
			break;
		default:						/* '?' */
			error = true;
			break;
		}
	}

	/* Pass remaining arguments to user program */
	params->argc = argc - (optind - 1);
	params->argv = argv + (optind - 1);

	/* Reset program name */
	params->argv[0] = argv[0];

	/* Reset (global) optind for potential use by user program */
	optind = 1;

	if (error)
		print_usage(argv[0], params);
}

int main_argc;
char **main_argv;

/** The model_main function contains the main model checking loop. */
static void model_main()
{
	struct model_params params;

	param_defaults(&params);

	parse_options(&params, main_argc, main_argv);

#ifdef TSO
	model_print("TSO\n");
#endif
	snapshot_stack_init();

	model = new MC(params);

	model->check();
	delete model;
	DEBUG("Exiting\n");
}

/**
 * Main function.  Just initializes snapshotting library and the
 * snapshotting library calls the model_main function.
 */
int main(int argc, char **argv)
{
	main_argc = argc;
	main_argv = argv;

	/*
	 * If this printf statement is removed, CDSChecker will fail on an
	 * assert on some versions of glibc.  The first time printf is
	 * called, it allocated internal buffers.  We can't easily snapshot
	 * libc since we also use it.
	 */

	printf("SATCheck\n"
				 "Copyright (c) 2016 Regents of the University of California. All rights reserved.\n"
				 "Distributed under the GPLv2\n"
				 "Written by Brian Demsky and Patrick Lam\n\n");

	/* Configure output redirection for the model-checker */
	redirect_output();

	/* Let's jump in quickly and start running stuff */
	snapshot_system_init(200000, 1024, 1024, 90000, &model_main);
}
