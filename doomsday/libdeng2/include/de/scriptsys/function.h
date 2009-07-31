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

#ifndef DMETHOD_HH
#define DMETHOD_HH

#include "../derror.hh"
#include "../doh/dnode.hh"

#include <list>
#include <map>

namespace de
{
    class Statement;
    class Context;
    class Object;
    class Expression;
    class Value;
    class ArrayValue;
    
    class Method : public Node
    {
    public:
        /// This exception is thrown if an incorrect number of arguments
        /// is given in a method call.
        DEFINE_ERROR(WrongArgumentsError);

        typedef std::list<std::string> Arguments;
        typedef std::map<std::string, Value*> Defaults;
        typedef std::list<const Value*> ArgumentValues;
        
    public:
        /**
         * Constructor.
         *
         * @param name      Name of the method.
         * @param args      Names of the method arguments.
         * @param defaults  Default values for some or all of the arguments.
         * @param firstStatement  First statement of the method. Owned by the
         *                  MethodStatement.
         */
        Method(const std::string& name, const Arguments& args, const Defaults& defaults,
            const Statement* firstStatement = 0);
        
        ~Method();
        
        const Arguments& arguments() const { return arguments_; }
        
        const Defaults& defaults() const { return defaults_; }
      
        const Statement* firstStatement() const { return firstStatement_; }
        
        void mapArgumentValues(const ArrayValue& args, ArgumentValues& values);
        
        /**
         * Perform a native call of the method. Always throws an exception
         * if the wrong number of arguments is provided.
         *
         * @param context  Execution context. Any results generated by a
         *                 native method are placed here.
         * @param self     Self object during the execution of the method.
         * @param args     Arguments to the method. The array's first element
         *                 is always a dictionary that contains the labeled values.
         *
         * @return @c false, if the context should proceed with the non-native
         *         method call by creating a new execution context and running
         *         the statements of the method there. @c true, if the 
         *         native call handles everything, including placing the 
         *         return value into the evaluator.
         */
        virtual bool callNative(Context& context, Object& self, const ArgumentValues& args);
        
    private:
        /// Argument names.
        Arguments arguments_;

        /// The Method owns the default values stored in the arguments list.
        Defaults defaults_;
        
        /// The statements of this method are owned by the Script that
        /// defines the method (MethodStatement).
        const Statement* firstStatement_;
    };
}

#endif /* DMETHOD_HH */
