
/* Copyright (c) 1999-2014 by Digital Mars
 * All Rights Reserved, written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/stringtable.h
 */

#ifndef STRINGTABLE_H
#define STRINGTABLE_H

#if __SC__
#pragma once
#endif

#include "root.h"

struct StringEntry;

// StringValue is a variable-length structure as indicated by the last array
// member with unspecified size.  It has neither proper c'tors nor a factory
// method because the only thing which should be creating these is StringTable.
struct StringValue
{
    void *ptrvalue;
private:
    size_t length;

#ifndef IN_GCC
    // Disable warning about nonstandard extension
    #pragma warning (disable : 4200)
#endif
    char lstring[];

public:
    size_t len() const { return length; }
    const char *toDchars() const { return lstring; }

private:
    friend struct StringEntry;
    StringValue();  // not constructible
    // This is more like a placement new c'tor
    void ctor(const char *p, size_t length);
};

struct StringTable
{
private:
    void **table;
    size_t count;
    size_t tabledim;

public:
    void _init(size_t size = 37);
    ~StringTable();

    StringValue *lookup(const char *s, size_t len);
    StringValue *insert(const char *s, size_t len);
    StringValue *update(const char *s, size_t len);

private:
    void **search(const char *s, size_t len);
};

#endif
