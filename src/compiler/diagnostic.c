/*
   Copyright 2022 Alessandro Bozzato

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "diagnostic.h"

#define COLOR_RED "\033[0;31m"
#define COLOR_MAGENTA "\033[0;35m"
#define COLOR_UWHITE "\033[4;37m"
#define COLOR_RESET "\033[0m"

static char const *start_of_line(stringview content, uint32_t pos)
{
    pos = MIN(content.length, pos);
    char const *str = content.str + pos;

    while (str > content.str && *str != '\n') {
        str--;
    }

    return str;
}

static char const *end_of_line(stringview content, uint32_t pos)
{
    pos = MIN(content.length, pos);
    char const *str = content.str + pos;
    char const *end = content.str + content.length;

    while (str < end && *str != '\n') {
        str++;
    }

    str += *str == '\n';
    return str;
}

static int get_line(stringview content, uint32_t pos)
{
    pos = MIN(content.length, pos);
    char const *str = content.str;
    char const *end = str + pos;

    int line = 1;

    while (str < end) {
        if (*str == '\n') {
            line++;
        }
        str++;
    }

    return line;
}

static int get_column(stringview content, uint32_t pos)
{
    pos = MIN(content.length, pos);
    char const *str = content.str;
    char const *end = str + pos;

    return end - start_of_line(content, pos) + 1;
}

static int count_digits(int num)
{
    int result = 0;
    while (num) {
        num /= 10;
        result++;
    }
    return result;
}

static array(stringview) gather_lines(stringview content, Span span)
{
    char const *str = start_of_line(content, span.start);
    char const *end = end_of_line(content, span.end);
    char const *start = str;

    array(stringview) lines = NULL;

    while (*str && str < end) {
        if (*str == '\n') {
            stringview line = { .length = str - start, .str = start };
            array_push(lines, line);
            start = str + 1;
        }
        str++;
    }

    return lines;
}

static inline void print_line(stringview line, int line_num, int width)
{
    fprintf(stderr, " %*d | " SV_FMT "\n", width, line_num, SV_ARGS(line));
}

#define UNDERLINE_CHAR "^"

static void print_snippet(stringview content, Diagnostic diag)
{
    int num = get_line(content, diag.location.start);
    int last_line = get_line(content, diag.location.end);

    int width = count_digits(last_line);

    array(stringview) lines = gather_lines(content, diag.location);

    stringview line = lines[0];
    Span span = diag.location;

    // First line
    // We assume: content.str + span.start >= line.str
    fprintf(stderr, " %*s |\n", width, "");
    print_line(line, num, width);
    num++;

    fprintf(stderr, " %*s | ", width, "");

    int spaces = content.str + span.start - line.str;
    for (int i = 0; i < spaces; ++i) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, COLOR_RED);

    if (content.str + span.end <= line.str + line.length) {
        while (line.str <= content.str + span.end) {
            fprintf(stderr, UNDERLINE_CHAR);
            line.str++;
        }

        fprintf(stderr, COLOR_RESET " %s\n", diag.label);
        return;
    } else {
        while (line.length > 0) {
            fprintf(stderr, UNDERLINE_CHAR);
            line.length--;
        }
        fprintf(stderr, COLOR_RESET "\n");
    }

    for (size_t i = 1; i < array_length(lines) - 1; ++i) {
        line = lines[i];
        print_line(line, num, width);
        num++;

        fprintf(stderr, " %*s | " COLOR_RED, width, "");

        while (line.length > 0) {
            fprintf(stderr, UNDERLINE_CHAR);
            line.length--;
        }
        fprintf(stderr, COLOR_RESET "\n");
    }

    // Last line
    line = array_last(lines);
    print_line(line, num, width);

    fprintf(stderr, " %*s | " COLOR_RED, width, "");

    while (line.str < content.str + span.end) {
        fprintf(stderr, UNDERLINE_CHAR);
        line.str++;
    }

    fprintf(stderr, COLOR_RESET " %s\n", diag.label);
}

static void emit_diagnostic(Diagnostic diag)
{
    char const *path = filepath(diag.location.file_id);
    stringview content = filecontent(diag.location.file_id);
    if (path == NULL || content.str == NULL) {
        panic("Internal Compiler Bug: invalid `file_id`");
    }

    // Fix wrong span start and end
    diag.location.start = MAX(diag.location.start, 0);
    diag.location.end = MIN(diag.location.end, content.length);

    // Header
    int line = get_line(content, diag.location.start);
    int column = get_column(content, diag.location.start);

    fprintf(stderr, "%s:%d:%d: ", path, line, column);
    fprintf(stderr, diag.is_error ? COLOR_RED "error:" : COLOR_MAGENTA "warning:");
    fprintf(stderr, COLOR_RESET " %s\n", diag.message);

    // Snippet
    print_snippet(content, diag);

    // Hint
    if (diag.hint != NULL) {
        fprintf(stderr, COLOR_UWHITE "Hint" COLOR_RESET ": %s\n", diag.hint);
    }

    xfree(diag.message);
    xfree(diag.label);
    xfree(diag.hint);
}

void emit_diagnostics(array(Diagnostic) diags)
{
    for (size_t i = 0; i < array_length(diags); ++i) {
        emit_diagnostic(diags[i]);
    }

    array_free(diags);
}
