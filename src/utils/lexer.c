#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../../include/utils/lexer.h"

static void push_token(char ***tokens, int *count, int *cap, const char *start, int len)
{
    if (*count >= *cap)
    {
        *cap = *cap == 0 ? 64 : *cap * 2;
        *tokens = realloc(*tokens, *cap * sizeof(char *));
    }
    char *tok = malloc(len + 1);
    memcpy(tok, start, len);
    tok[len] = '\0';
    (*tokens)[(*count)++] = tok;
}

//Tokenize when line level myers diff finds a substitution

char **tokenize_string(const char *text, int *count_out)
{
    char **tokens = NULL;
    int count = 0, cap = 0;
    int i = 0;
    int len = strlen(text);

    while (i < len)
    {
        if (isspace((unsigned char)text[i]))
        {
            int start = i;
            while (i < len && isspace((unsigned char)text[i]))
                i++;
            push_token(&tokens, &count, &cap, text + start, i - start);
        }
        else if (isalpha((unsigned char)text[i]) || text[i] == '_')
        {
            int start = i;
            while (i < len && (isalnum((unsigned char)text[i]) || text[i] == '_'))
                i++;
            push_token(&tokens, &count, &cap, text + start, i - start);
        }
        else if (isdigit((unsigned char)text[i]))
        {
            int start = i;
            while (i < len && isdigit((unsigned char)text[i]))
                i++;
            push_token(&tokens, &count, &cap, text + start, i - start);
        }
        else
        {
            push_token(&tokens, &count, &cap, text + i, 1);
            i++;
        }
    }

    *count_out = count;
    return tokens;
}

void free_tokens(char **tokens, int count)
{
    for (int i = 0; i < count; i++)
        free(tokens[i]);
    free(tokens);
}
