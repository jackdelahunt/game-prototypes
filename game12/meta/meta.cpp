#include "../src/ack.cpp"

#include <tree_sitter/api.h>

#include <iostream>

#define NODE_META_TYPE "meta_specifier"
#define NODE_STRUCT_DECL_TYPE "struct_specifier"
#define NODE_ENUM_DECL_TYPE "enum_specifier"

#define NODE_ENUM_TYPE "enumerator"

#define BUILDER_START_CAPACITY 500

extern "C" const TSLanguage* tree_sitter_cpp(void);

void run_meta_program(Arena *arena, TSTree *tree, string source);
string meta_generate_enum(Arena *arena, TSNode enum_node, string source);

void run_repl(Arena *arena, TSTree *tree, string source);
TSTree *parse_source(string source);
slice<TSNode> tree_query(Arena *arena, TSTree *tree, string query_source);

bool node_is_type(TSNode node, const char *type);
TSNode node_expect_child(TSNode node, const char *type, u32 index);
TSNode node_child_field(TSNode node, string field);
string node_to_string(TSNode node, string source);
void node_print_lisp(TSNode node);
void node_print_ast(TSNode node, i32 indent);

i32 main() {
    log_set_options(false, false);

    Arena arena = arena_create(MB(10));

    string source = read_entire_file("meta/example/foo.h");

    TSTree *tree = parse_source(source);
    TSNode root_node = ts_tree_root_node(tree);

    Info("Root node:");
    node_print_lisp(root_node);
    node_print_ast(root_node, 0);

    if (false) {
        run_repl(&arena, tree, source);
    }
    else {
        run_meta_program(&arena, tree, source);
    }

#if 0
    std::cin.get();
#endif
}

void run_meta_program(Arena *arena, TSTree *tree, string source) {
    slice<TSNode> meta_nodes = tree_query(arena, tree, "(meta_specifier) @meta");

    for (TSNode &meta_node : meta_nodes) {
        TSNode type_node = ts_node_named_child(meta_node, 0);
        
        if (node_is_type(type_node, NODE_ENUM_DECL_TYPE)) {
            string generated = meta_generate_enum(arena, type_node, source);
            Log(generated);
        }
        else {
            Warnf("Not supported meta type yet: {}", ts_node_type(type_node));
        }
    }
}

/*
enum Foo : u32 {
    Bar = 1 << 0,
    Baz = 1 << 1
};

struct MetaEnumInfo {
    string base_type;
    i64 count;
    string *names;
    u32 *values;
};

static string FooNames[2] = {"Hello", "World"};
static u32 FooValues[2] = {(1 << 0), (1 << 1)};

MetaInfo MetaFoo = MetaInfo {
    .base_type = "u32",
    .count = 2,
    .names = FooNames,
    .names = FooValues,
};
*/

string meta_generate_enum(Arena *arena, TSNode enum_node, string source) {
    // TODO: create a string builder wrapper to clean up generation code
    // i.e. append_string, append_stringln, auto indentation
    DynamicArray<u8> builder = dynamic_array_create<u8>(arena, BUILDER_START_CAPACITY);

    TSNode type_node = node_child_field(enum_node, "name");
    string type_string = node_to_string(type_node, source);

    TSNode body_node = node_child_field(enum_node, "body");
    u32 member_count = ts_node_named_child_count(body_node);

    append_many(&builder, string("template<>\n"));
    append_many(&builder, fmt(arena, "struct MetaEnum<{}> {\n", type_string));
    append_many(&builder, fmt(arena, "const static int count = {};\n\n", member_count));

    { // generate values
        append_many(&builder, string("inline static EnumValue values[count] = {\n"));

        for (u32 i = 0; i < member_count; i++) {
            TSNode value_node = node_expect_child(body_node, NODE_ENUM_TYPE, i); // breaks if there is a comment luulull
            TSNode value_name_node = node_child_field(value_node, "name");
            string value_name_string = node_to_string(value_name_node, source);

            append_many(&builder, fmt(arena, "    {.name = \"{}\", .value = int({})},\n", value_name_string, value_name_string));
        }

        append_many(&builder, string("};\n\n"));
    }

    { // generate name()
        append_many(&builder, fmt(arena, "static std::string name({} value) {\n", type_string));

        append_many(&builder, fmt(arena, "    switch (value) {\n", type_string));
        for (u32 i = 0; i < member_count; i++) {
            TSNode value_node = node_expect_child(body_node, NODE_ENUM_TYPE, i); // breaks if there is a comment luulull
            TSNode value_name_node = node_child_field(value_node, "name");
            string value_name_string = node_to_string(value_name_node, source);

            append_many(&builder, fmt(arena, "        case {}: return values[{}].name;\n", value_name_string, value_name_string));
        }
        append_many(&builder, string("    }\n"));

        append_many(&builder, string("}\n\n"));
    }

    { // generate value()
        append_many(&builder, fmt(arena, "static {} value(std::string name) {\n", type_string));
        append_many(&builder,     string("    for (int i = 0; i < count; i++) {\n"));
        append_many(&builder, fmt(arena, "        if (values[i].name == name) return ({}) values[i].value;\n", type_string));
        append_many(&builder,     string("    }\n"));
        append_many(&builder, fmt(arena, "    return ({}) 0;\n", type_string));
        append_many(&builder,     string("}\n\n"));
    }

    append_many(&builder, string("};\n"));

    return to_slice(&builder);
}

void run_repl(Arena *arena, TSTree *tree, string source) {
    while (true) {
        std::cout << "> ";

        std::string input;
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }

        string query_source = slice_create((u8 *) input.c_str(), input.length());

#if 0
        query_source = "(meta_specifier) @meta";
#endif

        slice<TSNode> nodes = tree_query(arena, tree, query_source);

        if (nodes.len == 0) {
            continue;
        }
    
        for (i64 i = 0; i < nodes.len; i++) {
            TSNode node = nodes[i];

            Info(node_to_string(node, source));
            node_print_ast(node, 0);
        }
    }
}

TSTree *parse_source(string source) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_cpp());

    TSTree* tree = ts_parser_parse_string(
        parser,
        NULL,
        source.c(),
        source.len 
    );

#if 0
    ts_tree_print_dot_graph(tree, 1);
#endif

    return tree;
}

slice<TSNode> tree_query(Arena *arena, TSTree *tree, string query_source) {
    TSNode root_node = ts_tree_root_node(tree);

    u32 error_offset = 0;
    TSQueryError error = {};

    TSQuery *query = ts_query_new(tree_sitter_cpp(), query_source.c(), query_source.len, &error_offset, &error);
    
    if (error != TSQueryErrorNone) {
        switch (error) {
            case TSQueryErrorSyntax:    Err("Query failed: syntax error");      break;
            case TSQueryErrorNodeType:  Err("Query failed: node type error");   break;
            case TSQueryErrorField:     Err("Query failed: field error");       break;
            case TSQueryErrorCapture:   Err("Query failed: capture error");     break;
            case TSQueryErrorStructure: Err("Query failed: structure error");   break;
            case TSQueryErrorLanguage:  Err("Query failed: language error");    break;
            default: Assert(0);
        }

        return {};
    }

    TSQueryCursor *cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, root_node);

    DynamicArray<TSNode> nodes = dynamic_array_create<TSNode>(arena, 50);

    TSQueryMatch match = {};
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (u16 i = 0; i < match.capture_count; i++) {
            const TSQueryCapture* capture = &match.captures[i];
            append(&nodes, capture->node); 
        }
    }

    return to_slice(&nodes);
}

bool node_is_type(TSNode node, const char *type) {
    if (ts_node_is_null(node)) {
        return false;
    }

    return strcmp(ts_node_type(node), type) == 0;
}

TSNode node_expect_child(TSNode node, const char *type, u32 index) {
    TSNode child = ts_node_named_child(node, index);
    Assert(!ts_node_is_null(child) && node_is_type(child, type));

    return child;
}

TSNode node_child_field(TSNode node, string field) {
    return ts_node_child_by_field_name(node, field.c(), field.len);
}

string node_to_string(TSNode node, string source) {
    u32 start = ts_node_start_byte(node);
    u32 end = ts_node_end_byte(node);
    
    return slice_range(source, start, end);
}

void node_print_lisp(TSNode node) {
    Log(ts_node_string(node));
}

void node_print_ast(TSNode node, i32 indent) {
    for (i32 i = 0; i < indent; i++) {
        if (i == indent - 1) {
            printf("v "); 
        }
        else {
            printf("| "); 
        }
    }

    Log(ts_node_type(node));

    u32 count = ts_node_child_count(node);
    for (u32 i = 0; i < count; i++) {
        TSNode child = ts_node_named_child(node, i);
        if (!ts_node_is_null(child)) {
            node_print_ast(child, indent + 1);
        }
    }
}
