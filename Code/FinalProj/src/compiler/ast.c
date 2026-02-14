#include "ast.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#include "darray.h"
#include "parser.h"

ast_node_t *ParseVariableDeclaration(parser_t *p) {

}

ast_node_t *ParseFunction(parser_t *p) {

}

ast_node_t *ParseDeclaration(parser_t *p) {
    if (MatchKeyword(p, KEYWORD_LET)) {
        return ParseVariableDeclaration(p);
    }

    if (MatchKeyword(p, KEYWORD_FUNC)) {
        return ParseFunction(p);
    }

    printf("Expected function or global variable declaration\n");
    return NULL;
}

ast_node_t *AstFromTokens(token_t *tokens) {
    ast_node_t *program = (ast_node_t *)malloc(sizeof(ast_node_t));
    program->type = AST_PROGRAM;
    program->program.decls = DArrayCreate(ast_node_t *);

    parser_t p = {0};
    p.tokens = tokens;
    p.count = DArrayLength(tokens);
    p.pos = 0;
    
    while (!Match(&p, TOKEN_EOF)) {
        ast_node_t *decl = ParseDeclaration(&p);
        if (!decl) {
            return NULL;
        }

        DArrayPush(program->program.decls, decl);
    }

    return program;
}