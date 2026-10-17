class_name Layers
extends RefCounted
## Physics layer bits (named in Project Settings > Layer Names > 3D Physics).
##
## Mirrors the UE collision setup: walls, the planet and the rocket body block
## everything; pawns block each other; pickups and trigger zones only overlap pawns.
## The ray gun's "RayGun" trace channel becomes [constant RAY_GUN_MASK]: it passes
## through pickups and trigger zones but stops on walls, the ground, the rocket and aliens.

const WORLD := 1
const PLAYER := 2
const ALIEN := 4
const PICKUP := 8

const RAY_GUN_MASK := WORLD | ALIEN
