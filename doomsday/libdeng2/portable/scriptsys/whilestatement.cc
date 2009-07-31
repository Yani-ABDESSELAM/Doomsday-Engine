/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
#include "de/WhileStatement"
#include "de/Expression"
#include "de/Evaluator"
#include "de/Context"
#include "de/Value"

using namespace de;

WhileStatement::~WhileStatement()
{
    delete loopCondition_;
}

void WhileStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();

    if(eval.evaluate(loopCondition_).isTrue())
    {
        // Continue and break jump points are defined within a while compound.
        context.start(compound_.firstStatement(), this, this, next());
    }
    else
    {
        context.proceed();
    }
}
