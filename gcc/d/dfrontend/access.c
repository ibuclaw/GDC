
// Copyright (c) 1999-2012 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "root.h"
#include "rmem.h"

#include "enum.h"
#include "aggregate.h"
#include "init.h"
#include "attrib.h"
#include "scope.h"
#include "id.h"
#include "mtype.h"
#include "declaration.h"
#include "aggregate.h"
#include "expression.h"
#include "module.h"

#define LOG 0

/* Code to do access checks
 */

bool hasPackageAccess(Scope *sc, Dsymbol *s);
bool hasPrivateAccess(AggregateDeclaration *ad, Dsymbol *smember);
bool isFriendOf(AggregateDeclaration *ad, AggregateDeclaration *cd);

/****************************************
 * Return PROT access for Dsymbol smember in this declaration.
 */

PROT getAccess(AggregateDeclaration *ad, Dsymbol *smember)
{
    PROT access_ret = PROTnone;

#if LOG
    printf("+AggregateDeclaration::getAccess(this = '%s', smember = '%s')\n",
        ad->toChars(), smember->toChars());
#endif

    assert(ad->isStructDeclaration() || ad->isClassDeclaration());
    if (smember->toParent() == ad)
    {
        access_ret = smember->prot();
    }
    else if (smember->isDeclaration()->isStatic())
    {
        access_ret = smember->prot();
    }
    if (ClassDeclaration *cd = ad->isClassDeclaration())
    {
        for (size_t i = 0; i < cd->baseclasses->dim; i++)
        {
            BaseClass *b = (*cd->baseclasses)[i];

            PROT access = getAccess(b->base, smember);
            switch (access)
            {
                case PROTnone:
                    break;

                case PROTprivate:
                    access_ret = PROTnone;  // private members of base class not accessible
                    break;

                case PROTpackage:
                case PROTprotected:
                case PROTpublic:
                case PROTexport:
                    // If access is to be tightened
                    if (b->protection < access)
                        access = b->protection;

                    // Pick path with loosest access
                    if (access > access_ret)
                        access_ret = access;
                    break;

                default:
                    assert(0);
            }
        }
    }

#if LOG
    printf("-AggregateDeclaration::getAccess(this = '%s', smember = '%s') = %d\n",
        ad->toChars(), smember->toChars(), access_ret);
#endif
    return access_ret;
}

/********************************************************
 * Helper function for ClassDeclaration::accessCheck()
 * Returns:
 *      0       no access
 *      1       access
 */

static int accessCheckX(
        Dsymbol *smember,
        Dsymbol *sfunc,
        AggregateDeclaration *dthis,
        AggregateDeclaration *cdscope)
{
    assert(dthis);

#if 0
    printf("accessCheckX for %s.%s in function %s() in scope %s\n",
        dthis->toChars(), smember->toChars(),
        sfunc ? sfunc->toChars() : "NULL",
        cdscope ? cdscope->toChars() : "NULL");
#endif
    if (hasPrivateAccess(dthis, sfunc) ||
        isFriendOf(dthis, cdscope))
    {
        if (smember->toParent() == dthis)
            return 1;
        else
        {
            ClassDeclaration *cdthis = dthis->isClassDeclaration();
            if (cdthis)
            {
                for (size_t i = 0; i < cdthis->baseclasses->dim; i++)
                {   BaseClass *b = (*cdthis->baseclasses)[i];
                    PROT access = getAccess(b->base, smember);
                    if (access >= PROTprotected ||
                        accessCheckX(smember, sfunc, b->base, cdscope)
                       )
                        return 1;

                }
            }
        }
    }
    else
    {
        if (smember->toParent() != dthis)
        {
            ClassDeclaration *cdthis = dthis->isClassDeclaration();
            if (cdthis)
            {
                for (size_t i = 0; i < cdthis->baseclasses->dim; i++)
                {   BaseClass *b = (*cdthis->baseclasses)[i];

                    if (accessCheckX(smember, sfunc, b->base, cdscope))
                        return 1;
                }
            }
        }
    }
    return 0;
}

/*******************************
 * Do access check for member of this class, this class being the
 * type of the 'this' pointer used to access smember.
 */

void accessCheck(AggregateDeclaration *ad, Loc loc, Scope *sc, Dsymbol *smember)
{
    int result;

    FuncDeclaration *f = sc->func;
    AggregateDeclaration *cdscope = sc->getStructClassScope();
    PROT access;

#if LOG
    printf("AggregateDeclaration::accessCheck() for %s.%s in function %s() in scope %s\n",
        ad->toChars(), smember->toChars(),
        f ? f->toChars() : NULL,
        cdscope ? cdscope->toChars() : NULL);
#endif

    Dsymbol *smemberparent = smember->toParent();
    if (!smemberparent || !smemberparent->isAggregateDeclaration())
    {
#if LOG
        printf("not an aggregate member\n");
#endif
        return;                         // then it is accessible
    }

    // BUG: should enable this check
    //assert(smember->parent->isBaseOf(this, NULL));

    if (smemberparent == ad)
    {
        PROT access2 = smember->prot();
        result = access2 >= PROTpublic ||
                hasPrivateAccess(ad, f) ||
                isFriendOf(ad, cdscope) ||
                (access2 == PROTpackage && hasPackageAccess(sc, ad)) ||
                ad->getAccessModule() == sc->module;
#if LOG
        printf("result1 = %d\n", result);
#endif
    }
    else if ((access = getAccess(ad, smember)) >= PROTpublic)
    {
        result = 1;
#if LOG
        printf("result2 = %d\n", result);
#endif
    }
    else if (access == PROTpackage && hasPackageAccess(sc, ad))
    {
        result = 1;
#if LOG
        printf("result3 = %d\n", result);
#endif
    }
    else
    {
        result = accessCheckX(smember, f, ad, cdscope);
#if LOG
        printf("result4 = %d\n", result);
#endif
    }
    if (!result)
    {
        ad->error(loc, "member %s is not accessible", smember->toChars());
    }
}

/****************************************
 * Determine if this is the same or friend of cd.
 */

bool isFriendOf(AggregateDeclaration *ad, AggregateDeclaration *cd)
{
#if LOG
    printf("AggregateDeclaration::isFriendOf(this = '%s', cd = '%s')\n", ad->toChars(), cd ? cd->toChars() : "null");
#endif
    if (ad == cd)
        return true;

    // Friends if both are in the same module
    //if (toParent() == cd->toParent())
    if (cd && ad->getAccessModule() == cd->getAccessModule())
    {
#if LOG
        printf("\tin same module\n");
#endif
        return true;
    }

#if LOG
    printf("\tnot friend\n");
#endif
    return false;
}

/****************************************
 * Determine if scope sc has package level access to s.
 */

bool hasPackageAccess(Scope *sc, Dsymbol *s)
{
#if LOG
    printf("hasPackageAccess(s = '%s', sc = '%p')\n", s->toChars(), sc);
#endif

    Package *pkg = NULL;
    for (; s; s = s->parent)
    {
        if (Module *m = s->isModule())
        {
            DsymbolTable *dst = Package::resolve(m->md ? m->md->packages : NULL, NULL, NULL);
            assert(dst);
            Dsymbol *s2 = dst->lookup(m->ident);
            assert(s2);
            Package *p = s2->isPackage();
            if (p && p->isPackageMod())
            {
                pkg = p;
                break;
            }
        }
        else if ((pkg = s->isPackage()) != NULL)
            break;
    }
#if LOG
    if (pkg)
        printf("\tthis is in package '%s'\n", pkg->toChars());
#endif

    if (pkg)
    {
        if (pkg == sc->module->parent)
        {
#if LOG
            printf("\ts is in same package as sc\n");
#endif
            return true;
        }
        if (pkg->isPackageMod() == sc->module)
        {
#if LOG
            printf("\ts is in same package.d module as sc\n");
#endif
            return true;
        }
        s = sc->module->parent;
        for (; s; s = s->parent)
        {
            if (s == pkg)
            {
#if LOG
                printf("\ts is in ancestor package of sc\n");
#endif
                return true;
            }
        }
    }

#if LOG
    printf("\tno package access\n");
#endif
    return false;
}

/**********************************
 * Determine if smember has access to private members of this declaration.
 */

bool hasPrivateAccess(AggregateDeclaration *ad, Dsymbol *smember)
{
    if (smember)
    {
        AggregateDeclaration *cd = NULL;
        Dsymbol *smemberparent = smember->toParent();
        if (smemberparent)
            cd = smemberparent->isAggregateDeclaration();

#if LOG
        printf("AggregateDeclaration::hasPrivateAccess(class %s, member %s)\n",
                ad->toChars(), smember->toChars());
#endif

        if (ad == cd)         // smember is a member of this class
        {
#if LOG
            printf("\tyes 1\n");
#endif
            return true;           // so we get private access
        }

        // If both are members of the same module, grant access
        while (1)
        {
            Dsymbol *sp = smember->toParent();
            if (sp->isFuncDeclaration() && smember->isFuncDeclaration())
                smember = sp;
            else
                break;
        }
        if (!cd && ad->toParent() == smember->toParent())
        {
#if LOG
            printf("\tyes 2\n");
#endif
            return true;
        }
        if (!cd && ad->getAccessModule() == smember->getAccessModule())
        {
#if LOG
            printf("\tyes 3\n");
#endif
            return true;
        }
    }
#if LOG
    printf("\tno\n");
#endif
    return false;
}

/****************************************
 * Check access to d for expression e.d
 */

void accessCheck(Loc loc, Scope *sc, Expression *e, Declaration *d)
{
    if (sc->flags & SCOPEnoaccesscheck)
        return;

#if LOG
    if (e)
    {
        printf("accessCheck(%s . %s)\n", e->toChars(), d->toChars());
        printf("\te->type = %s\n", e->type->toChars());
    }
    else
    {
        printf("accessCheck(%s)\n", d->toPrettyChars());
    }
#endif
    if(d->isUnitTestDeclaration()) 
    {
        // Unittests are always accessible.
        return;
    }
    if (!e)
    {
        if (d->prot() == PROTprivate && d->getAccessModule() != sc->module ||
            d->prot() == PROTpackage && !hasPackageAccess(sc, d))
        {
            error(loc, "%s %s is not accessible from module %s",
                d->kind(), d->toPrettyChars(), sc->module->toChars());
        }
    }
    else if (e->type->ty == Tclass)
    {
        // Do access check
        ClassDeclaration *cd = (ClassDeclaration *)(((TypeClass *)e->type)->sym);
        if (e->op == TOKsuper)
        {
            ClassDeclaration *cd2 = sc->func->toParent()->isClassDeclaration();
            if (cd2)
                cd = cd2;
        }
        accessCheck(cd, loc, sc, d);
    }
    else if (e->type->ty == Tstruct)
    {
        // Do access check
        StructDeclaration *cd = (StructDeclaration *)(((TypeStruct *)e->type)->sym);
        accessCheck(cd, loc, sc, d);
    }
}
