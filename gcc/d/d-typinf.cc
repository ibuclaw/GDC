// d-typinf.cc -- D frontend for GCC.
// Copyright (C) 2011-2015 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"

#include "dfrontend/module.h"
#include "dfrontend/mtype.h"
#include "dfrontend/scope.h"
#include "dfrontend/declaration.h"
#include "dfrontend/aggregate.h"

static TypeInfoDeclaration *getTypeInfoDeclaration(Type *t);
static bool builtinTypeInfo(Type *t);

/****************************************************
 * Get the exact TypeInfo.
 */

void
genTypeInfo(Type *type, Scope *sc)
{
  if (!Type::dtypeinfo)
    {
      type->error(Loc(), "TypeInfo not found. object.d may be incorrectly installed or corrupt");
      fatal();
    }

  // do this since not all Type's are merge'd
  Type *t = type->merge2();
  if (!t->vtinfo)
    {
      // does both 'shared' and 'shared const'
      if (t->isShared())
	t->vtinfo = TypeInfoSharedDeclaration::create(t);
      else if (t->isConst())
	t->vtinfo = TypeInfoConstDeclaration::create(t);
      else if (t->isImmutable())
	t->vtinfo = TypeInfoInvariantDeclaration::create(t);
      else if (t->isWild())
	t->vtinfo = TypeInfoWildDeclaration::create(t);
      else
	t->vtinfo = getTypeInfoDeclaration(t);

      gcc_assert(t->vtinfo);

      /* If this has a custom implementation in std/typeinfo, then
       * do not generate a COMDAT for it.
       */
      if (!builtinTypeInfo(t))
	{
	  if (sc)
	    {
	      if (!sc->func || !sc->func->inNonRoot())
		{
		  // Find module that will go all the way to an object file
		  Module *m = sc->module->importedFrom;
		  m->members->push(t->vtinfo);
		  semanticTypeInfo(sc, t);
		}
	    }
	  else
	    t->vtinfo->toObjFile(false);
	}
    }
  // Types aren't merged, but we can share the vtinfo's
  if (!type->vtinfo)
    type->vtinfo = t->vtinfo;

  gcc_assert(type->vtinfo != NULL);
}

Expression *
getTypeInfo(Type *type, Scope *sc)
{
  gcc_assert(type->ty != Terror);
  genTypeInfo(type, sc);
  Expression *e = VarExp::create(Loc(), type->vtinfo);
  e = e->addressOf();
  // do this so we don't get redundant dereference
  e->type = type->vtinfo->type;
  return e;
}

TypeInfoDeclaration *
getTypeInfoDeclaration(Type *type)
{
    switch(type->ty)
    {
    case Tpointer:
      return TypeInfoPointerDeclaration::create(type);

    case Tarray:
      return TypeInfoArrayDeclaration::create(type);

    case Tsarray:
      return TypeInfoStaticArrayDeclaration::create(type);

    case Taarray:
      return TypeInfoAssociativeArrayDeclaration::create(type);

    case Tstruct:
      return TypeInfoStructDeclaration::create(type);

    case Tvector:
      return TypeInfoVectorDeclaration::create(type);

    case Tenum:
      return TypeInfoEnumDeclaration::create(type);

    case Tfunction:
      return TypeInfoFunctionDeclaration::create(type);

    case Tdelegate:
      return TypeInfoDelegateDeclaration::create(type);

    case Ttuple:
      return TypeInfoTupleDeclaration::create(type);

    case Tclass:
        if (((TypeClass *) type)->sym->isInterfaceDeclaration())
            return TypeInfoInterfaceDeclaration::create(type);
        else
            return TypeInfoClassDeclaration::create(type);

    default:
        return TypeInfoDeclaration::create(type, 0);
    }
}

/* ========================================================================= */

/* These decide if there's an instance for them already in std.typeinfo,
 * because then the compiler doesn't need to build one.
 */

static bool
builtinTypeInfo(Type *t)
{
    if (t->isTypeBasic() || t->ty == Tclass)
        return !t->mod;
    if (t->ty == Tarray)
    {
        Type *next = t->nextOf();
	// Strings are so common, make them builtin.
	return !t->mod
	  && ((next->isTypeBasic() != NULL && !next->mod)
	      || (next->ty == Tchar && next->mod == MODimmutable)
	      || (next->ty == Tchar && next->mod == MODconst));
    }
    return false;
}

