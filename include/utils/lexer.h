#ifndef UTILS_LEXER_H
#define UTILS_LEXER_H

char **tokenize_string(const char *text, int *count_out);
void free_tokens(char **tokens, int count);

#endif
