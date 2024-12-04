const std = @import("std");

const main = @import("main.zig");

const raylib = @cImport(@cInclude("raylib.h"));

pub fn rectangle(position: main.V2f, size: main.V2f, color: raylib.Color) void {
    const centerd = position - (size * main.V2f{0.5, 0.5});

    raylib.DrawRectangle(
        @as(c_int, @intFromFloat(centerd[0])), 
        @as(c_int, @intFromFloat(centerd[1])), 
        @as(c_int, @intFromFloat(size[0])), 
        @as(c_int, @intFromFloat(size[1])), 
        color
    );
}

pub fn rectangle_outline(x: f32, y: f32, width: f32, height: f32, thickness: f32, color: raylib.Color) void {
    raylib.DrawRectangleLinesEx(
        .{
            .x = x,
            .y = y, 
            .width = width, 
            .height = height,
        },
        thickness,
        color
    );
}

pub fn rectangle_gradient_vertical(position: main.V2f, size: main.V2f, start_color: raylib.Color, end_color: raylib.Color) void {
    raylib.DrawRectangleGradientV(
        @as(c_int, @intFromFloat(position[0])), 
        @as(c_int, @intFromFloat(position[1])), 
        @as(c_int, @intFromFloat(size[0])), 
        @as(c_int, @intFromFloat(size[1])), 
        start_color,
        end_color
    );
}

pub fn rectangle_gradient_horizontal(position: main.V2f, size: main.V2f, start_color: raylib.Color, end_color: raylib.Color) void {
    raylib.DrawRectangleGradientH(
        @as(c_int, @intFromFloat(position[0])), 
        @as(c_int, @intFromFloat(position[1])), 
        @as(c_int, @intFromFloat(size[0])), 
        @as(c_int, @intFromFloat(size[1])), 
        start_color,
        end_color
    );
}

// the y component of 'size' is the max height of the progress bar
// any 'value' given that is less then 'max_value' will cause the height
// of the bar to be smaller then 'size'
//
// unlike other drawing functions this 'position' is relative to the top left of the 
// final image
pub fn progress_bar_vertical(position: main.V2f, size: main.V2f, color: raylib.Color, value: u64, max_value: u64) void {
    const progress =  @as(f32, @floatFromInt(value)) / @as(f32, @floatFromInt(max_value));
    const progress_bar_end_y = position[1] + size[1];
    const progress_bar_start_y = progress_bar_end_y - (progress * size[1]);
    const progress_bar_height = progress_bar_end_y - progress_bar_start_y;

    rectangle(main.V2f{position[0], progress_bar_start_y}, main.V2f{size[0], progress_bar_height}, color);
}

pub fn progress_bar_horizontal(position: main.V2f, size: main.V2f, color: raylib.Color, value: u64, max_value: u64) void {
    const progress =  @as(f32, @floatFromInt(value)) / @as(f32, @floatFromInt(max_value));
    rectangle(position, main.V2f{size[0] * progress, size[1]}, color);
}

pub fn circle(position: main.V2f, radius: f32, color: raylib.Color) void {
    raylib.DrawCircle(
        @as(c_int, @intFromFloat(position[0])), 
        @as(c_int, @intFromFloat(position[1])), 
        radius, 
        color
    );
}

pub fn line(start: main.V2f, end: main.V2f, thickness: f32, color: raylib.Color) void {
    raylib.DrawLineEx(
        .{.x = start[0], .y = start[1]},
        .{.x = end[0], .y = end[1]},
        thickness,
        color
    );
}

pub fn text(string: []const u8, position: main.V2f, font_size: i32, color: raylib.Color) void {
    if(string.len == 0) return;

    const text_width = @as(f32, @floatFromInt(raylib.MeasureText(&string[0], font_size)));
    const size = main.V2f{text_width, @floatFromInt(font_size)};

    const centerd = position - (size * main.V2f{0.5, 0.5});

    raylib.DrawText(
        &string[0],
        @as(c_int, @intFromFloat(centerd[0])), 
        @as(c_int, @intFromFloat(centerd[1])), 
        @as(c_int, @intCast(font_size)), 
        color
    );
}

// WARNING: this is slow
pub fn text_bounded(string: []const u8, x: f32, y: f32, font_size: i32, max_width: i32, color: raylib.Color) void {
    if(string.len == 0) return;

    var real_font_size = font_size;
    while(raylib.MeasureText(&string[0], real_font_size) > @as(c_int, @intCast(max_width))) {
        real_font_size -= 1;

        if(font_size == 1) break;
    }

    raylib.DrawText(
        &string[0],
        @as(c_int, @intFromFloat(x)), 
        @as(c_int, @intFromFloat(y)), 
        @as(c_int, @intCast(real_font_size)), 
        color
    );
}

const RenderTextureSettings = struct {
    position: main.V2f          = main.V2f{0, 0},
    size: main.V2f              = main.V2f{0, 0},
    roation: f32                = 0,
    tint: raylib.Color          = raylib.WHITE,
    // used for drawing sub sections of the texture
    source_position: main.V2f   = main.V2f{0, 0},
    source_size: ?main.V2f      = null,
};

pub fn texture(t: raylib.Texture, settings: RenderTextureSettings) void {
    // Source rectangle (part of the texture to use for drawing)
    const source_rectagle = raylib.Rectangle { 
        .x = settings.source_position[0], 
        .y = settings.source_position[1], 
        .width =  if(settings.source_size == null) @as(f32, @floatFromInt(t.width)) else settings.source_size.?[0], 
        .height = if(settings.source_size == null) @as(f32, @floatFromInt(t.width)) else settings.source_size.?[0]
    };

    // Destination rectangle (screen rectangle where drawing part of texture)
    const destination_rectangle = raylib.Rectangle{ 
        .x = settings.position[0], 
        .y = settings.position[1], 
        .width = settings.size[0], 
        .height = settings.size[1]
    };

    raylib.DrawTexturePro(t, source_rectagle, destination_rectangle, .{}, settings.roation, settings.tint);
}

// only keys in the text input list are allowed
fn key_to_byte(key: c_int) u8 {
    // for alphabetical lowercase
    const letter_offset = 97 - raylib.KEY_A;
    const number_offset = 48 - raylib.KEY_ZERO;

    return switch (key) {
        raylib.KEY_A...raylib.KEY_Z => @as(u8, @intCast(key + letter_offset)),
        raylib.KEY_ZERO...raylib.KEY_NINE => @as(u8, @intCast(key + number_offset)),
        raylib.KEY_MINUS => 45,
        raylib.KEY_PERIOD => 46,
        else => unreachable
    };
}

fn append_to_input(buffer: []u8, byte: u8) void {
    for(buffer, 0..) |c, i| {
        if(c != 0) {
            continue;
        }

        buffer[i] = byte;
        break;
    }
}

fn backspace_from_input(buffer: []u8) void {
    // check the last character to remove first
    // this just makes the code after easier
    if(buffer[buffer.len - 1] != 0) {
        buffer[buffer.len - 1] = 0;
        return;
    }

    for(buffer, 0..) |c, i| {
        if(c != 0) {
            continue;
        }

        // empty input buffer
        if(i == 0) {
            break;
        }

        buffer[i - 1] = 0;
        break;
    }
}
