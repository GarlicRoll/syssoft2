#include "node.h"
/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_PARSER_TAB_H_INCLUDED
# define YY_YY_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    SUM = 258,
    SUB = 259,
    MUL = 260,
    DIV = 261,
    SLASH = 262,
    PERCENT = 263,
    SET = 264,
    NOTEQUAL = 265,
    EQUALITY = 266,
    LESSTHAN = 267,
    GREATERTHAN = 268,
    LESSTHANEQ = 269,
    GREATERTHANEQ = 270,
    AND = 271,
    OR = 272,
    NOT = 273,
    DECREMENT = 274,
    INCREMENT = 275,
    FUNCTION = 276,
    AS = 277,
    ARRAY = 278,
    DEF = 279,
    END = 280,
    BEGIN_BLOCK = 281,
    WEND = 282,
    IDENTIFIER = 283,
    STR = 284,
    COMMA = 285,
    CHAR = 286,
    BIN = 287,
    HEX = 288,
    DEC = 289,
    BOOL = 290,
    IF = 291,
    ELSE = 292,
    WHILE = 293,
    UNTIL = 294,
    DO = 295,
    BREAK = 296,
    THEN = 297,
    LOOP = 298,
    SEMICOLON = 299,
    LPAREN = 300,
    RPAREN = 301,
    TYPEDEF = 302,
    DIM = 303
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 10 "source/parser.y"

    TreeNode* node;

#line 110 "parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;
extern YYLTYPE yylloc;
int yyparse (void);

#endif /* !YY_YY_PARSER_TAB_H_INCLUDED  */
