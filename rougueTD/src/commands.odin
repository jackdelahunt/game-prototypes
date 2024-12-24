package src

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:strings"

run_command :: proc(input: []u8) {
    context.logger.procedure = command_log_callback

    lexer := ArgumentLexer {
        input = input,
        current_index = 0
    }

    name, ok := parse_string_argument(&lexer)
    if !ok {
        log.errorf("Expected first token of command input to be string")
        return
    }

    if strings.compare(name, "echo") == 0 {
        message, ok := parse_string_argument(&lexer) 
        if !ok {
            log.errorf("Couldn't parse first argument for echo as string")
        }

        command_echo(message)
    }
    else if strings.compare(name, "restart") == 0 {
        command_restart()
    }
    else {
        log.error("No command found with name", name)
    }
}

parse_string_argument :: proc(lexer: ^ArgumentLexer) -> (string, bool) {
    t := next_token(lexer)
    if t.type != .STRING {
        return "", false
    }

    return transmute(string) t.value, true
}

ArgumentLexer :: struct {
    input: []u8,
    current_index: uint,
}

Token :: struct {
    type: TokenType,
    value: []u8
}

TokenType :: enum {
    INVALID,
    EOF,
    STRING,
}

next_token :: proc(lexer: ^ArgumentLexer) -> Token {
    // skip whitespace
    for lexer.current_index < len(lexer.input) && lexer.input[lexer.current_index] == ' ' {
        lexer.current_index += 1
    }

    if lexer.current_index >= len(lexer.input) {
        return Token {type = .EOF, value = nil}
    }

    { // string token
        start := lexer.current_index
        for lexer.current_index < len(lexer.input) && !is_deliminator(lexer.input[lexer.current_index]) {
            lexer.current_index += 1
        }

        string_slice := lexer.input[start:lexer.current_index]
        return Token {type = .STRING, value = string_slice}
    }

    return Token {type = .INVALID, value = nil}
}

next_token_type :: proc(lexer: ^ArgumentLexer, type: TokenType) -> (Token, bool) {
    token := next_token(lexer)
    return token, token.type == type
}

is_deliminator :: proc(b: u8) -> bool {
    switch b {
    case ' ', '\n', '\t', '{', '}', ',', '[', ']':
        return true
    }

    return false
}

















