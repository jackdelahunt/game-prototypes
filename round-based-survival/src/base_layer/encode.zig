const std = @import("std");
const log = std.log.scoped(.encode);

pub fn serialize(allocator: std.mem.Allocator, args: anytype) ![]u8 {
    const ArgsType = @TypeOf(args);
    const args_type_info = @typeInfo(ArgsType);
    if (args_type_info != .Struct or args_type_info.Struct.is_tuple) {
        @compileError("expected non-tuple struct argument, found " ++ @typeName(ArgsType));
    }

    var byte_buffer = std.ArrayList(u8).init(allocator);
    defer byte_buffer.deinit(); // no needed but might aswell

    try byte_buffer.appendSlice("{\n");

    const fields_info = args_type_info.Struct.fields;
    inline for(fields_info) |field_info| {
        try byte_buffer.appendSlice(field_info.name);
        try byte_buffer.appendSlice(": ");
        try serialize_value(allocator, &byte_buffer, @field(args, field_info.name));
        try byte_buffer.appendSlice("\n");
    }

    try byte_buffer.appendSlice("}\n");

    return try byte_buffer.toOwnedSlice();
}

fn serialize_value(allocator: std.mem.Allocator, byte_buffer: *std.ArrayList(u8), value: anytype) !void {
    const value_type_info = @typeInfo(@TypeOf(value));
    switch (value_type_info) {
        .Type, .Void, .NoReturn, .Pointer, .Array, .Struct, .Null, .ErrorUnion, .ErrorSet, .Union, .Fn, .Opaque, .Frame, .AnyFrame, .EnumLiteral, .Undefined, .ComptimeFloat, .ComptimeInt => {
            std.debug.panic("cannot serialize this value with this type {}", .{@Type(value_type_info)});
        },
        .Int, .Float, => {
            const bytes = try std.fmt.allocPrint(allocator, "{d}", .{value});
            defer allocator.free(bytes);

            try byte_buffer.appendSlice(bytes);
        },
        .Bool => {
            const bytes = try std.fmt.allocPrint(allocator, "{}", .{value});
            defer allocator.free(bytes);

            try byte_buffer.appendSlice(bytes);
        },
        .Optional => {
            if(value == null) {
                try byte_buffer.appendSlice("null");
            } else {
                try byte_buffer.appendSlice("?");
                try serialize_value(allocator, byte_buffer, value.?);
            }
        },
        .Enum => {
            try byte_buffer.appendSlice("enum(");
            const enum_value: u64 = @intFromEnum(value);
            try serialize_value(allocator, byte_buffer, enum_value);
            try byte_buffer.appendSlice(")");
        },
        .Vector => {
            try byte_buffer.appendSlice("v(");

            for(0..value_type_info.Vector.len) |i| {
                try serialize_value(allocator, byte_buffer, value[i]);

                if(i < value_type_info.Vector.len - 1) {
                    try byte_buffer.appendSlice(" ");
                }
            }

            try byte_buffer.appendSlice(")");
        },
    }
}

pub fn deserialize(allocator: std.mem.Allocator, T: type, bytes: []const u8, ptr: *T) !usize {
    const type_info = @typeInfo(T);
    if (type_info != .Struct) {
        @compileError("expected non-tuple struct, got " ++ @typeName(T));
    }

    var hash = std.hash_map.StringHashMap([]const u8).init(allocator);
    defer hash.deinit();

    // index that points to the \n after the closing } for each object
    var struct_end_index: usize = 0;

    { // parsing file and filling in hash map
        while (struct_end_index < bytes.len) {
            if(bytes[struct_end_index] == '}') {
                struct_end_index += 1;
                break; // line start with } so it is the end of the struct we are reading
            }
    
            struct_end_index = read_until(bytes, '\n', struct_end_index) + 1;
        }

        // skip starting {\n which is the start of every struct
        var read_index = eat_bytes(bytes, "{\n", 0);

        // go through each line and get the name and value and add it to the 
        // hash map, if the line is starting with } then we know it is not
        // the end of the struct yet
        while(bytes[read_index] != '}') {
            const name_end = read_until(bytes, ':', read_index);
            const value_start = eat_bytes(bytes, ": ", name_end);
            const value_end = read_until(bytes, '\n', value_start);
    
            try hash.put(bytes[read_index..name_end], bytes[value_start..value_end]);
         
            read_index = eat_bytes(bytes, "\n", value_end);
        }
    }

    if(false) {
        var iter = hash.iterator();
        while(iter.next()) |entry| {
            log.err("{s} -> {s}", .{entry.key_ptr.*, entry.value_ptr.*});
        }
    }

    const struct_type_info = @typeInfo(T).Struct;
    inline for(struct_type_info.fields) |*field| {
        if(hash.get(field.name)) |value_slice| {
            const value = try deserialize_value(allocator, field.type, value_slice);
            @field(ptr.*, field.name) = value;
        } else {
            // log.err("failed on name: {s}", .{field.name});
        }
    }

    return struct_end_index;
}

fn deserialize_value(allocator: std.mem.Allocator, T: type, value_slice: []const u8) !T {
    const type_info = @typeInfo(T);

    switch (type_info) {
        .Type, .Void, .NoReturn, .Pointer, .Array, .Struct, .Null, .ErrorUnion, .ErrorSet, .Union, .Fn, .Opaque, .Frame, .AnyFrame, .EnumLiteral, .Undefined, .ComptimeInt, .ComptimeFloat => {
            std.debug.panic("cannot de-serialize a value with this type {}", .{type_info});
        },
        .Int => {
            return try std.fmt.parseInt(T, value_slice, 10);
        },
        .Float => {
            return try std.fmt.parseFloat(T, value_slice);
        },
        .Bool => {
            if(std.mem.eql(u8, value_slice, "true")) {
                return true;
            }

            if(std.mem.eql(u8, value_slice, "false")) {
                return false;
            }

            std.debug.panic("cannot de-serialize a value as bool with these bytes {s}", .{value_slice});
        },
        .Optional => {
            if(std.mem.eql(u8, value_slice, "null")) {
                return null;
            }

            const sub_slice = value_slice[eat_bytes(value_slice, "?", 0)..];
            const non_null_value = try deserialize_value(allocator, type_info.Optional.child, sub_slice);

            // need to do this to get it to cast to ?T
            const value: T = non_null_value;
            return value;
        },
        .Enum => {
            const number_start = eat_bytes(value_slice, "enum(", 0);
            const number_end = read_until(value_slice, ')', number_start);

            const number = try deserialize_value(allocator, u64, value_slice[number_start..number_end]);
            return @enumFromInt(number);
        },
        .Vector => {
            const values_start = eat_bytes(value_slice, "v(", 0);
            const values_end = read_until(value_slice, ')', values_start);

            var read_index: usize = values_start;
            var vector = std.mem.zeroes(T);

            for(0..type_info.Vector.len) |i| {
                if(i != 0) {
                    read_index += eat_bytes(value_slice[read_index..values_end], " ", 0);
                }

                var value_end_index = read_index;
                if(i == type_info.Vector.len - 1) {
                    value_end_index = read_index + read_until(value_slice[read_index..values_end], ')', 0);
                } else {
                    value_end_index = read_index + read_until(value_slice[read_index..values_end], ' ', 0);
                }

                vector[i] = try deserialize_value(allocator, type_info.Vector.child, value_slice[read_index..value_end_index]);

                read_index = value_end_index;
            }

            return vector;
        },
    }

    return std.mem.zeroes(T);
}

fn eat_bytes(byte_buffer: []const u8, expected_bytes: []const u8, start_index: usize) usize {            
    var eat_amount: usize = 0;
    while(eat_amount < expected_bytes.len) : (eat_amount += 1) {
        const source_byte = byte_buffer[start_index + eat_amount];
        const expected_byte = expected_bytes[eat_amount];
        if(source_byte != expected_byte) {
            std.debug.panic("error reading bytes, expected {} got {}\n", .{expected_byte, source_byte});
        }
    }
        return start_index + expected_bytes.len;
}

fn read_until(byte_buffer: []const u8, target: u8, start_index: usize) usize {
    var read_amount: usize = 0;
    while(read_amount < byte_buffer.len) : (read_amount += 1) {
        const source_byte = byte_buffer[start_index + read_amount];

        if(source_byte == target) {
            break;
        }
    }

    return start_index + read_amount;
}

test "single struct" {
    const Foo = struct {
        number: i32,
        other: bool
    };

    var bytes: []const u8 = undefined;
    defer std.testing.allocator.free(bytes);

    const foo_a = Foo{.number = 123, .other = true};
    bytes = try serialize(std.testing.allocator, foo_a);

    var foo_b = std.mem.zeroes(Foo);
    _ = try deserialize(std.testing.allocator, Foo, bytes, &foo_b);

    try std.testing.expectEqual(foo_a, foo_b);
}

test "many structs" {
    const Foo = struct {
        number: usize,
        other: bool,
        v: @Vector(2, f32)
    };

    var byte_buffer = std.ArrayList(u8).init(std.testing.allocator);
    defer byte_buffer.deinit();

    var foos = [_]Foo{undefined} ** 100;

    // init and serialize each foo
    for(0..foos.len) |i| {
        foos[i] = Foo{.number = i, .other = i % 2 == 0, .v = @Vector(2, f32){123.3, 432.234}};

        const bytes = try serialize(std.testing.allocator, foos[i]);
        defer std.testing.allocator.free(bytes);

        try byte_buffer.appendSlice(bytes);
    }

    const bytes = try byte_buffer.toOwnedSlice();
    defer std.testing.allocator.free(bytes);

    {
        var read_index: usize = 0;
        var read_count: usize = 0;

        while(read_index < bytes.len) {
            var foo = std.mem.zeroes(Foo);
            const read_amount = try deserialize(std.testing.allocator, Foo, bytes[read_index..], &foo);

            try std.testing.expectEqual(foos[read_count], foo);

            read_count += 1;
            read_index += read_amount + 1;
        }
    } 
}
