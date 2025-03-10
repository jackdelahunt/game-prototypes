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
            return
        }

        command_echo(message)
    }
    else if strings.compare(name, "kill") == 0 {
        command_kill()
    }
    else if strings.compare(name, "wave") == 0 {
        wave, ok := parse_number_argument(&lexer) 
        if !ok {
            log.errorf("Couldn't parse first argument for wave as number")
            return
        }

        command_wave(wave)
    }
    else if strings.compare(name, "restart") == 0 {
        command_restart()
    }
    else if strings.compare(name, "spawning") == 0 {
        value, ok := parse_bool_argument(&lexer) 
        if !ok {
            log.errorf("Couldn't parse first argument for spawning as bool")
            return
        }

        command_spawning(value)
    }
    else if strings.compare(name, "nav") == 0 {
        command_nav()
        return
    }
    else {
        log.error("No command found with name", name)
    }
}

parse_number_argument :: proc(lexer: ^ArgumentLexer) -> (uint, bool) {
    token, token_ok := next_token_type(lexer, .NUMBER)
    if !token_ok {
        return 0, false
    }

    n, _, parse_ok := fmt._parse_int(transmute(string) token.value, 0)
    if !parse_ok {
        return 0, false
    }

    return cast(uint)n, true
}

parse_string_argument :: proc(lexer: ^ArgumentLexer) -> (string, bool) {
    token, ok := next_token_type(lexer, .STRING)
    if !ok {
        return "", false
    }

    return transmute(string) token.value, true
}

parse_bool_argument :: proc(lexer: ^ArgumentLexer) -> (bool, bool) {
    token, ok := next_token_type(lexer, .BOOL)
    if !ok {
        return false, false
    }
    
    if strings.compare(auto_cast token.value, "true") == 0 {
        return true, true
    } 
        
    return false, true
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
    BOOL,
    NUMBER
}

next_token :: proc(lexer: ^ArgumentLexer) -> Token {
    // skip whitespace
    for lexer.current_index < len(lexer.input) && lexer.input[lexer.current_index] == ' ' {
        lexer.current_index += 1
    }

    if lexer.current_index >= len(lexer.input) {
        return Token {type = .EOF, value = nil}
    }

    {
        switch lexer.input[lexer.current_index] {
        case '0'..='9':
            start := lexer.current_index
            for lexer.current_index < len(lexer.input) && !is_deliminator(lexer.input[lexer.current_index]) {
                lexer.current_index += 1
            }
    
            number := lexer.input[start:lexer.current_index]

            return Token{type = .NUMBER, value = number}
        }
    }

    { // strings and key words
        start := lexer.current_index
        for lexer.current_index < len(lexer.input) && !is_deliminator(lexer.input[lexer.current_index]) {
            lexer.current_index += 1
        }

        word := lexer.input[start:lexer.current_index]

        if strings.compare(auto_cast word, "true") == 0 {
            return Token {type = .BOOL, value = word}
        }
        else if strings.compare(auto_cast word, "false") == 0 {
            return Token {type = .BOOL, value = word}
        }

        return Token {type = .STRING, value = word}
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

















