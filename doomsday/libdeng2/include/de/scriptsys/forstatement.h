/*
 * The Doomsday Engine Project -- Hawthorn
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

#ifndef DFORSTATEMENT_HH
#define DFORSTATEMENT_HH

#include "dstatement.hh"
#include "dcompound.hh"

#include <string>

namespace de
{
    class Expression;
    
/**
 * For statement keeps looping until the iterable value runs out of
 * elements.
 */
	class ForStatement : public Statement
	{
	public:
	    ForStatement(const std::string& path, Expression* iteration) 
	        : path_(path), iteration_(iteration) {}
	    
        ~ForStatement();
        
        /// Returns the compound of the statement.
        Compound& compound() {
            return compound_;
        }
        
		void execute(Context& context) const;

	private:
        std::string path_;
		Expression* iteration_;
		Compound compound_;
	};
}

#endif /* DFORSTATEMENT_HH */
