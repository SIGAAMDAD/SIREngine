glslangValidator -H -V -o $1VP.spv -S vert $1VP.glsl
glslangValidator -H -V -o $1FP.spv -S frag $1FP.glsl