# Tribes Ascend Server Lag Compensation

## Cloning

> git clone git@github.com:karan283861/Tribes-Ascend-Server-Lag-Compensation.git --recurse-submodules

## CMake generation

> cmake -S . -B x86 -A Win32 -T v142

## Building

> cmake --build x86 --config=RelWithDebInfo

## Requirements
1. Microsoft Visual C++ Redistributable 2019 (x86)

## Usage
Clients must enable "Simulated Projectiles" setting for their projectiles to be lag compensated by the server.

## Features
* Lag compensates all projectile weapons (including arcing/gravity influenced) and all radial damage