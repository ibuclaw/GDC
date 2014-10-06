// d-asmstmt.cc -- D frontend for GCC.
// Copyright (C) 2011-2013 Free Software Foundation, Inc.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "d-system.h"
#include "statement.h"
#include "scope.h"
#include "id.h"

#include "d-lang.h"
#include "d-codegen.h"


// Semantically analyze AsmStatement where SC is the scope.

Statement *
AsmStatement::semantic (Scope *sc)
{
  sc->func->hasReturnExp |= 8;
  return Statement::semantic (sc);
}

