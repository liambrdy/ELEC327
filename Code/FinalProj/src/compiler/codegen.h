#ifndef _CODE_GEN_H
#define _CODE_GEN_H

#include "ast.h"
#include "stringbuf.h"

string_buf CodeGenx86_64(ast_node_t *node);

#endif