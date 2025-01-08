package src

import "base:runtime"
import "core:encoding/ansi"
import "core:log"
import "core:fmt"
import "core:path/filepath"

// allocator to be used for data that lifetime is bounded by a level
// default allocator for the context, eternal needs to be intentionally used
level_allocator := runtime.Allocator {
    procedure = level_allocator_callback,
    data = nil
}

// allocator to be used for data that lifetime is for the length of the program 
eternal_allocator := runtime.Allocator {
    procedure = eternal_allocator_callback,
    data = nil
}

logger := runtime.Logger {
    procedure = log_callback,
    lowest_level = .Debug when ODIN_DEBUG else .Warning
}

custom_context :: proc() -> runtime.Context {
    c := runtime.default_context()

    c.logger = logger
    c.allocator = level_allocator

    return c
}

AllocatorType :: enum {
    LEVEL,
    ETERNAL
}

// The level of indirection for these two callbacks are just to have the logger 
// print out the allocator type along with the allocation info
@(private="file")
level_allocator_callback :: proc(
        allocator_data: rawptr, 
        mode: runtime.Allocator_Mode, 
        size: int, 
        alignment: int, 
        old_memory: rawptr, 
        old_size: int, 
        location: runtime.Source_Code_Location = #caller_location
    ) -> ([]byte, runtime.Allocator_Error) {
    
    return real_allocator_callback(.LEVEL, allocator_data, mode, size, alignment, old_memory, old_size, location)
}

@(private="file")
eternal_allocator_callback :: proc(
        allocator_data: rawptr, 
        mode: runtime.Allocator_Mode, 
        size: int, 
        alignment: int, 
        old_memory: rawptr, 
        old_size: int, 
        location: runtime.Source_Code_Location = #caller_location
    ) -> ([]byte, runtime.Allocator_Error) {
    
    return real_allocator_callback(.ETERNAL, allocator_data, mode, size, alignment, old_memory, old_size, location)
}

@(private="file")
real_allocator_callback :: proc(
        type: AllocatorType,
        allocator_data: rawptr, 
        mode: runtime.Allocator_Mode, 
        size: int, 
        alignment: int, 
        old_memory: rawptr, 
        old_size: int, 
        location: runtime.Source_Code_Location = #caller_location
    ) -> ([]byte, runtime.Allocator_Error) {

    KB :: 1024
    MB :: KB * 1024
    GB :: MB * 1024

    size_string := "b"
    converted_size := size
    if size >= KB && size < MB {
        size_string = "Kb"
        converted_size /= KB
    }
    else if size >= MB && size < GB {
        size_string = "Mb" 
        converted_size /= MB
    }
    else if size >= GB {
        size_string = "Gb" 
        converted_size /= GB
    }

    log.debugf("[%v::%v] %v -> %v (%v%v)", type, mode, old_size, size, converted_size, size_string, location = location)
    return runtime.default_context().allocator.procedure(allocator_data, mode, size, alignment, old_memory, old_size, location)
}

@(private="file")
log_callback :: proc(data: rawptr, level: runtime.Logger_Level, text: string, options: bit_set[runtime.Logger_Option], location := #caller_location) {
    switch level {
    case .Debug:
    case .Info:
        fmt.print(ansi.CSI + ansi.FG_CYAN + ansi.SGR)
    case .Warning: 
        fmt.print(ansi.CSI + ansi.FG_YELLOW + ansi.SGR)
    case .Error:
        fmt.print(ansi.CSI + ansi.FG_BRIGHT_RED + ansi.SGR)
    case .Fatal:
        fmt.print(ansi.CSI + ansi.FG_RED + ansi.SGR)
    }


    file := filepath.base(location.file_path)
    fmt.printfln("[%v] %v(%v:%v) %v", level, file, location.line, location.column, text) 

    if level != .Debug {
        fmt.print(ansi.CSI + ansi.RESET + ansi.SGR)
    }
}
