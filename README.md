# Game prototypes
History of my progression learning and building various game features. Each iteration is defined in the 'gameXX' folder, some versions are a POC of a game/game-engine feature. While others are in a more completed state. The only intended target is windows x86-64. While some versions do have windows specific code some don't, so there is a chance it could build but who knows.

Below is a summary of each game from the newest iteration to the very beginning. See a more pretty version with screen shots and some other projects [here](https://jackdelahunt.github.io/).

## game12 (In progress)
`Summary`: Mutltiplayer first-person arena shooter

`Goal`: Combine the work from game11 (networking and multiplayer) into the engine last worked on in game10 (3D renderer, lighting, 3D models etc.) 

`Notable achievements`:

`Languages/tools`: C++, OpenGL, ValveGameNetworkingSockets

## game11
`Summary`: Simple player multiplayer lobby to walk around in with others. Used raylib to reduce complexity and focus on networking problems

`Goal`: Gain an understanding and build a multiplayer game with a client/server architecture. Also have the ability to "host" games from a client

`Notable achievements`: Built network layer for client and server connections. Server and clients communicate game state with messages and (try to) sync state

`Languages/tools`: C++, Raylib, ValveGameNetworkingSockets

## game10
`Summary`: 3D mesh rendering with renderer built from game09

`Goal`: Learn how to load 3D models and render them in game with previous graphics effects

`Notable achievements`: 3D renderer can now load 3D models

`Languages/tools`: C++, OpenGL, glsl

## game09
`Summary`: 3D real-time voxel mesh editor

`Goal`: Learn how to render complex scenes in 3D and try advanced graphics effects like lighting, shadows and screen-space aambient occlusion (SSAO)

`Notable achievements`: Rewrite renderer from ground up to support 3D and optimise performance to support much higher vertex counts in draw calls. 3D lighting with ambient + diffuse + specular. Screen space shadow maps, and SSAO

`Languages/tools`: C++, OpenGL, glsl

## game08
`Summary`: Spooky cave scene with the player exploring as a flying bat 

`Goal`: Again a continuation on learning more about graphics programming, improve lighting from game07 and add more features

`Notable achievements`: Dynamic lighting, normal mapped textures that interact with lighting, water reflections

`Languages/tools`: C++, OpenGL, glsl

## game07
`Summary`: Cozy forest scene

`Goal`: Expand on 2D renderer and work on features such as basic lighting and animated textures

`Notable achievements`: Renderer now upgraded to use multiple render passes, 2D screenspace lighting, animated textures, depth of field

`Languages/tools`: C++, OpenGL, glsl

## game06
`Summary`: Clone of the classic game asteroids

`Goal`: Remake the game Asteroids to port all of the work so far into C++

`Notable achievements`: All previous features are ported to C++, including the openGL renderer and sound

`Languages/tools`: C++, OpenGL, glsl

## game05
`Summary`: Arcade survival game with various enemies and progression with the player inheriting enemies abilities as the game progresses 

`Goal`: Move the renderer to OpenGL while keeping roughly the same interface and expand on a few features. First use of textures and custom effects with shaders. Old features were also ported like 2D primitives and text rendering

`Notable achievements`: Really getting to grips with 2D rendering and now building on top of OpenGL directly, sounds, another game with various levels and challenges. Also first level editor built for this game

`Languages/tools`: Odin, OpenGL, glsl

## game04
`Summary`: Top down sokoban style puzzle game

`Goal`: Continue the structure built first in game02, and expand on ideas I have for puzzles games. Most of the work here was spent in gameplay code and the "engine" mainly stayed the same

`Notable achievements`: Refined 2D renderer from game02, actual gameplay with various levels and challenge

`Languages/tools`: Odin, Sokol (Graphics API abstraction), glsl

## game03
`Summary`: Use sokol but now in C++, this attempt died pretty fast

`Goal`: Port the sokol renderer to C++. Tried to get away from `sokol-app` and just use the graphics API abstraction but this just ended up being too complicated for what I needed so I stopped soon after

`Languages/tools`: C++, Sokol (Graphics API abstraction), glsl

## game02
`Summary`: First attempt into custom rendering, with sort of a procedurally generated tower defence game on top 

`Goal`: Learn how to write a custom renderer, and develop my own rendering API. After using raylib many times I decided to copy that simple and powerful interface for rendering

`Notable achievements`: Custom 2D renderer, basic 2D primitives and text rendering

`Languages/tools`: Odin, Sokol (Graphics API abstraction), glsl

## game01
`Summary`: Top down arcade survival horde shooter

`Goal`: Learn how to structure a game, design how entites should work and interact with each other

`Languages/tools`: Zig, Raylib







