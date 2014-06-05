# makefile for root dir
# auther: richard
# time:   2014.1.8
include makefile.begin
export ROOTDIRS = $(shell pwd)
TARGET_DIRS = 	util	\
				core	\

EXE = npserver

FUNC = $(MAKE) -fPIC -o ./bin/$(EXE) ./obj/*.o

include makefile.in

.PHONY:clean
clean:
	@-rm ./obj/*.o ./bin/$(EXE) -rvf

