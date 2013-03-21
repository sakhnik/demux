#!/bin/bash

aclocal || exit 1

automake --add-missing || exit 1

autoreconf || exit 1
