class_name GlowMaterial
extends RefCounted
## Unlit emissive material for orbs, beams, the beacon and the rocket flame
## (port of M_Glow: emissive = Color x Intensity, tinted per actor).

const SHADER := preload("res://assets/materials/glow.gdshader")


static func make(color: Color, intensity: float = 8.0) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = SHADER
	material.set_shader_parameter("color", color)
	material.set_shader_parameter("intensity", intensity)
	return material
