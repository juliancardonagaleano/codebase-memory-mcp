/*
 * test_grammar_regression.c — Per-language extraction regression net.
 *
 * Guards against silent extraction breakage when vendored tree-sitter grammars
 * are refreshed. For each language a minimal sample is extracted and checked
 * for (a) a catastrophic-break floor (defs >= min_defs) and (b) specific
 * expected definition names. A future grammar upgrade that renames/removes the
 * node types extraction depends on (e.g. the fwcd kotlin `name`-field drop that
 * produced 0 defs) fails this suite loudly.
 *
 * To add a language: append a row to CASES with a sample that defines >=1
 * function/type and the names you expect extraction to surface.
 */
#include "test_framework.h"
#include "cbm.h"

static int reg_has_def_any(CBMFileResult *r, const char *name) {
    for (int i = 0; i < r->defs.count; i++) {
        if (strcmp(r->defs.items[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static CBMFileResult *extract(const char *src, CBMLanguage lang, const char *proj,
                              const char *path) {
    return cbm_extract_file(src, (int)strlen(src), lang, proj, path, 0, NULL, NULL);
}

typedef struct {
    const char *name; /* label for diagnostics */
    CBMLanguage lang;
    const char *path;
    const char *src;
    int min_defs;          /* catastrophic-break floor */
    const char *expect[4]; /* def names that must be present (NULL-terminated) */
} GrammarCase;

static const GrammarCase CASES[] = {
    /* ── LSP-backed languages ── */
    {"go", CBM_LANG_GO, "a.go", "package m\nfunc Foo() {}\nfunc Bar() int { return 0 }\n", 2, {"Foo", "Bar", NULL}},
    {"c", CBM_LANG_C, "a.c", "int foo(void){return 0;}\nint bar(void){return 1;}\n", 2, {"foo", "bar", NULL}},
    {"cpp", CBM_LANG_CPP, "a.cpp", "struct A {};\nint foo(){return 0;}\n", 2, {"A", "foo", NULL}},
    {"python", CBM_LANG_PYTHON, "a.py", "def foo():\n    pass\nclass A:\n    pass\n", 2, {"foo", "A", NULL}},
    {"javascript", CBM_LANG_JAVASCRIPT, "a.js", "function foo(){}\nclass A{}\n", 2, {"foo", "A", NULL}},
    {"typescript", CBM_LANG_TYPESCRIPT, "a.ts", "function foo(): number { return 1; }\nclass A {}\n", 2, {"foo", "A", NULL}},
    {"tsx", CBM_LANG_TSX, "a.tsx", "function foo(): number { return 1; }\n", 1, {"foo", NULL}},
    {"java", CBM_LANG_JAVA, "A.java", "class A {\n    void foo() {}\n}\n", 2, {"A", "foo", NULL}},
    {"kotlin", CBM_LANG_KOTLIN, "a.kt", "fun foo() {}\nclass A\n", 2, {"foo", "A", NULL}},
    {"rust", CBM_LANG_RUST, "a.rs", "fn foo() {}\nstruct A;\n", 2, {"foo", "A", NULL}},
    {"ruby", CBM_LANG_RUBY, "a.rb", "def foo\nend\nclass A\nend\n", 2, {"foo", "A", NULL}},
    {"php", CBM_LANG_PHP, "a.php", "<?php\nfunction foo() {}\nclass A {}\n", 2, {"foo", "A", NULL}},
    {"c_sharp", CBM_LANG_CSHARP, "A.cs", "class A {\n    void Foo() {}\n}\n", 2, {"A", "Foo", NULL}},

    /* ── extraction-only code languages ── */
    {"bash", CBM_LANG_BASH, "a.sh", "foo() {\n  echo hi\n}\nbar() {\n  foo\n}\n", 2, {"foo", "bar", NULL}},
    {"lua", CBM_LANG_LUA, "a.lua", "function foo() end\nfunction bar() end\n", 2, {"foo", "bar", NULL}},
    {"r", CBM_LANG_R, "a.R", "foo <- function() {}\nbar <- function() {}\n", 1, {"foo", NULL}},
    {"julia", CBM_LANG_JULIA, "a.jl", "function foo() end\nstruct A end\n", 1, {"foo", NULL}},
    {"dart", CBM_LANG_DART, "a.dart", "class A {}\nvoid foo() {}\n", 2, {"A", "foo", NULL}},
    {"swift", CBM_LANG_SWIFT, "a.swift", "func foo() {}\nclass A {}\n", 2, {"foo", "A", NULL}},
    {"scala", CBM_LANG_SCALA, "a.scala", "object A {\n  def foo() = 1\n}\n", 1, {"A", NULL}},
    {"elixir", CBM_LANG_ELIXIR, "a.ex", "defmodule A do\n  def foo do\n  end\nend\n", 1, {"foo", NULL}},
    /* count-only: name representation is grammar-specific; floor catches a 0-def break */
    {"haskell", CBM_LANG_HASKELL, "a.hs", "foo :: Int\nfoo = 1\n", 1, {NULL}},
    {"ocaml", CBM_LANG_OCAML, "a.ml", "let foo () = 1\nlet bar () = 2\n", 1, {"foo", NULL}},
    {"perl", CBM_LANG_PERL, "a.pl", "sub foo {}\nsub bar {}\n", 2, {"foo", "bar", NULL}},
    {"gdscript", CBM_LANG_GDSCRIPT, "a.gd", "func foo():\n    pass\n", 1, {"foo", NULL}},
    {"groovy", CBM_LANG_GROOVY, "a.groovy", "class A {\n  def foo() {}\n}\n", 1, {"A", NULL}},
    {"zig", CBM_LANG_ZIG, "a.zig", "fn foo() void {}\nfn bar() void {}\n", 1, {"foo", NULL}},
    {"nim", CBM_LANG_NIM, "a.nim", "proc foo() = discard\nproc bar() = discard\n", 1, {"foo", NULL}},
    {"fsharp", CBM_LANG_FSHARP, "a.fs", "let foo () = 1\nlet bar () = 2\n", 1, {NULL}},
    {"erlang", CBM_LANG_ERLANG, "a.erl", "-module(a).\nfoo() -> ok.\nbar() -> ok.\n", 1, {"foo", NULL}},
    {"clojure", CBM_LANG_CLOJURE, "a.clj", "(defn foo [] 1)\n(defn bar [] 2)\n", 1, {NULL}},
    {"solidity", CBM_LANG_SOLIDITY, "a.sol", "contract A {\n  function foo() public {}\n}\n", 1, {"A", NULL}},
    {"tcl", CBM_LANG_TCL, "a.tcl", "proc foo {} {}\nproc bar {} {}\n", 1, {"foo", NULL}},
    {"powershell", CBM_LANG_POWERSHELL, "a.ps1", "function Get-Foo {\n}\nfunction Get-Bar {\n}\n", 1, {"Get-Foo", NULL}},
    {"fortran", CBM_LANG_FORTRAN, "a.f90", "subroutine foo()\nend subroutine\n", 1, {NULL}},
};

TEST(grammar_regression_all) {
    int failures = 0;
    size_t n = sizeof(CASES) / sizeof(CASES[0]);
    for (size_t i = 0; i < n; i++) {
        const GrammarCase *c = &CASES[i];
        CBMFileResult *r = extract(c->src, c->lang, "reg", c->path);
        if (!r) {
            fprintf(stderr, "  [REG] %-12s extract returned NULL\n", c->name);
            failures++;
            continue;
        }
        if (r->defs.count < c->min_defs) {
            fprintf(stderr, "  [REG] %-12s defs=%d < min=%d  (extraction regression?)\n", c->name,
                    r->defs.count, c->min_defs);
            failures++;
        }
        for (int e = 0; c->expect[e]; e++) {
            if (!reg_has_def_any(r, c->expect[e])) {
                fprintf(stderr, "  [REG] %-12s missing def '%s' (defs=%d)\n", c->name, c->expect[e],
                        r->defs.count);
                failures++;
            }
        }
        cbm_free_result(r);
    }
    if (failures > 0) {
        fprintf(stderr, "  [REG] %d grammar-regression check(s) failed across %zu languages\n",
                failures, n);
    }
    ASSERT_EQ(failures, 0);
    PASS();
}

void suite_grammar_regression(void) {
    RUN_TEST(grammar_regression_all);
}
