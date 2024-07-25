#ifndef __SIRENGINE_HPP__
#define __SIRENGINE_HPP__

#pragma once

#include "Compiler.hpp"
#include "Config.hpp"
#include "Platform.hpp"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#define SIRENGINE_XSTRING_HELPER( x ) #x
#define SIRENGINE_XSTRING( x ) SIRENGINE_XSTRING_HELPER( x )

#define SIRENGINE_VERSION 0
#define SIRENGINE_VERSION_STRING "SIR Engine v" SIRENGINE_XSTRING( SIRENGINE_VERSION )

#endif