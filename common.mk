# A few common Makefile items

CC := gcc
CXX := g++

UNAME := $(shell uname)

LIB_NAME := model
TSO_LIB_SO := libtso_$(LIB_NAME).so
SC_LIB_SO := libsc_$(LIB_NAME).so

CPPFLAGS += -Wall -g -O3

# Mac OSX options
ifeq ($(UNAME), Darwin)
CPPFLAGS += -D_XOPEN_SOURCE -DMAC
endif
