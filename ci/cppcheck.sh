#!/bin/bash

set -e

wget --no-check-certificate "https://sourceforge.net/projects/cppcheck/files/cppcheck/1.90/cppcheck-1.90.tar.gz"
tar -xzf cppcheck-1.90.tar.gz
cd ./cppcheck-1.90
cmake .
make -j${NUM_JOBS} && sudo make install
cd .. && rm -rf ./cppcheck-1.90

[[ ! -f .cppcheckignore ]] || CPPCHECK_OPTIONS="${CPPCHECK_OPTIONS} --suppressions-list=.cppcheckignore"
	
cppcheck --quiet --force --error-exitcode=1 $CPPCHECK_OPTIONS .
