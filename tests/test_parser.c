#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

enum {
    STATUS_OK = 0,
    STATUS_LEX_ERROR = 3,
    STATUS_PARSE_ERROR = 4
};

static void fail(const char *message) {
    fprintf(stderr, "test_parser failed: %s\n", message);
    exit(1);
}

static void assert_int_eq(int expected, int actual, const char *message) {
    if (expected != actual) {
        fprintf(stderr, "test_parser failed: %s (expected=%d actual=%d)\n", message, expected, actual);
        exit(1);
    }
}

static void assert_size_eq(size_t expected, size_t actual, const char *message) {
    if (expected != actual) {
        fprintf(stderr, "test_parser failed: %s (expected=%zu actual=%zu)\n", message, expected, actual);
        exit(1);
    }
}

static void assert_str_eq(const char *expected, const char *actual, const char *message) {
    if (strcmp(expected, actual) != 0) {
        fprintf(stderr, "test_parser failed: %s (expected=%s actual=%s)\n", message, expected, actual);
        exit(1);
    }
}

static int tokenize_and_parse(const char *sql, Statement *stmt, char *errbuf, size_t errbuf_size) {
    TokenArray tokens = {0};
    int status = tokenize_sql(sql, &tokens, errbuf, errbuf_size);
    if (status != STATUS_OK) {
        return status;
    }

    status = parse_statement(&tokens, stmt, errbuf, errbuf_size);
    free_token_array(&tokens);
    return status;
}

static void test_insert_without_column_list(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_OK,
                  tokenize_and_parse("INSERT INTO users VALUES (1, 'Alice', 20);", &stmt, errbuf, sizeof(errbuf)),
                  "INSERT without column list should parse");
    assert_int_eq(STMT_INSERT, stmt.type, "statement type should be INSERT");
    assert_str_eq("users", stmt.insert_stmt.table_name, "table name should match");
    assert_size_eq(0, stmt.insert_stmt.column_count, "column list should be omitted");
    assert_size_eq(3, stmt.insert_stmt.value_count, "value count should match");
    assert_int_eq(VALUE_NUMBER, stmt.insert_stmt.values[0].type, "first value should be number");
    assert_str_eq("1", stmt.insert_stmt.values[0].text, "first number text should match");
    assert_int_eq(VALUE_STRING, stmt.insert_stmt.values[1].type, "second value should be string");
    assert_str_eq("Alice", stmt.insert_stmt.values[1].text, "string text should match");
    free_statement(&stmt);
}

static void test_insert_with_column_list(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_OK,
                  tokenize_and_parse("INSERT INTO users (name, id) VALUES ('Bob', 2);", &stmt, errbuf, sizeof(errbuf)),
                  "INSERT with column list should parse");
    assert_size_eq(2, stmt.insert_stmt.column_count, "column count should match");
    assert_str_eq("name", stmt.insert_stmt.columns[0], "first column should match");
    assert_str_eq("id", stmt.insert_stmt.columns[1], "second column should match");
    assert_str_eq("Bob", stmt.insert_stmt.values[0].text, "first literal should match");
    assert_str_eq("2", stmt.insert_stmt.values[1].text, "second literal should match");
    free_statement(&stmt);
}

static void test_select_star(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_OK,
                  tokenize_and_parse("SELECT * FROM users", &stmt, errbuf, sizeof(errbuf)),
                  "SELECT * without semicolon should parse");
    assert_int_eq(STMT_SELECT, stmt.type, "statement type should be SELECT");
    assert_int_eq(1, stmt.select_stmt.select_all, "select_all should be enabled");
    assert_size_eq(0, stmt.select_stmt.column_count, "column count should be zero for select all");
    assert_str_eq("users", stmt.select_stmt.table_name, "table name should match");
    free_statement(&stmt);
}

static void test_select_column_list(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_OK,
                  tokenize_and_parse("SELECT id, name FROM users;", &stmt, errbuf, sizeof(errbuf)),
                  "SELECT projection should parse");
    assert_int_eq(0, stmt.select_stmt.select_all, "projection should not be select_all");
    assert_size_eq(2, stmt.select_stmt.column_count, "projection column count should match");
    assert_str_eq("id", stmt.select_stmt.columns[0], "first projection column should match");
    assert_str_eq("name", stmt.select_stmt.columns[1], "second projection column should match");
    free_statement(&stmt);
}

static void test_select_missing_projection_fails(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_PARSE_ERROR,
                  tokenize_and_parse("SELECT FROM users;", &stmt, errbuf, sizeof(errbuf)),
                  "missing projection should fail");
    if (strstr(errbuf, "PARSE ERROR") == NULL) {
        fail("parse error message should be present");
    }
}

static void test_insert_missing_into_fails(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_PARSE_ERROR,
                  tokenize_and_parse("INSERT users VALUES (1);", &stmt, errbuf, sizeof(errbuf)),
                  "missing INTO should fail");
    if (strstr(errbuf, "expected INTO") == NULL) {
        fail("missing INTO message should be present");
    }
}

static void test_missing_from_fails(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_PARSE_ERROR,
                  tokenize_and_parse("SELECT id, name users;", &stmt, errbuf, sizeof(errbuf)),
                  "missing FROM should fail");
    if (strstr(errbuf, "expected FROM") == NULL) {
        fail("missing FROM message should be present");
    }
}

static void test_missing_comma_or_paren_fails(void) {
    Statement stmt;
    char errbuf[256] = {0};

    assert_int_eq(STATUS_PARSE_ERROR,
                  tokenize_and_parse("INSERT INTO users (id name) VALUES (1, 'Alice');", &stmt, errbuf, sizeof(errbuf)),
                  "missing comma in column list should fail");
    assert_int_eq(STATUS_PARSE_ERROR,
                  tokenize_and_parse("INSERT INTO users (id, name VALUES (1, 'Alice');", &stmt, errbuf, sizeof(errbuf)),
                  "missing right paren should fail");
}

int main(void) {
    test_insert_without_column_list();
    test_insert_with_column_list();
    test_select_star();
    test_select_column_list();
    test_select_missing_projection_fails();
    test_insert_missing_into_fails();
    test_missing_from_fails();
    test_missing_comma_or_paren_fails();
    printf("test_parser: OK\n");
    return 0;
}
