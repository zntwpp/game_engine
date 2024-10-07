﻿# game_engine

Mandatory functions:

LoadTexture(filepath, name)
after init:
CreateMaterial(name, diffuse_albedo, fresnel, roughnes, mat_transform = {1,1,1})
CreateGeometry(obj, pos, mat_name, name)
CreateWorld()

Other:
SetAmbient(color) - set ambinet light
SetLight(pos/dir, strength) - set point/directional/spot light
EditAmbinet(color) - edit ambient light
EditLight(pos/dir, strength, index) - edit point/directional/spot light
MoveObject(name, pos) - to move object
DrawObject(name) - to draw object if the object has been removed from the drawing cycle
DoNotDrawObject(name) - to remove object from drawing cycle
IsKeyPresed(key) - check key
RotateObject(name, matrix) - comming soon
