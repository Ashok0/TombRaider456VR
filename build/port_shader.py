from pathlib import Path
p=Path('src/MotionHandSkin.h'); s=p.read_text(); s=s.replace('"\\n    vTrackedHandWeight = 1.0;\\n"','''"\\n    ivec3 bodyIndices = ivec3(j.xyz);\\n"
        "    uint bodyBits = uint(uVisibleBody.x) | (uint(uVisibleBody.y) << 16u);\\n"
        "    vVisibleBodyWeight = 0.0;\\n"
        "    for (int b=0;b<3;++b) {\\n"
        "        int joint=bodyIndices[b];\\n"
        "        if (joint>=0 && joint<33 && ((joint<32 ? bodyBits : uint(uVisibleBody.z)) & (1u << uint(joint % 32)))!=0u)\\n"
        "            vVisibleBodyWeight += w[b];\\n"
        "    }\\n"
        "    vTrackedHandWeight = 1.0;\\n"''')
s=s.replace('out float vTrackedHandWeight;\\n"','out float vTrackedHandWeight;\\nuniform vec4 uVisibleBody;\\nout float vVisibleBodyWeight;\\n"')
s=s.replace('if (vTrackedHandWeight < 0.5) discard;\\n"','if (vTrackedHandWeight < 0.5) discard;\\n    if (uVisibleBody.w > 0.5 && vVisibleBodyWeight < 0.5) discard;\\n"')
s=s.replace('in float vTrackedHandWeight;\\n"','in float vTrackedHandWeight;\\nuniform vec4 uVisibleBody;\\nin float vVisibleBodyWeight;\\n"'); p.write_text(s)
p=Path('src/BoneSkin.cpp'); s=p.read_text().replace('    GLint locHand = -2;','    GLint locBodyMask=-2, locJoints=-2;\n    bool bodyMaskLive=false;\n    GLint locHand = -2;'); a=s.index('    if (!PathPossible()) return;',s.index('void BoneSkinAfterValidate'))
s=s[:a]+'''    if (ps.locBodyMask==-2) {
        ps.locBodyMask=gl::GetUniformLocation(prog,"uVisibleBody");
        ps.locJoints=gl::GetUniformLocation(prog,"uJoints");
    }
    if (ps.locBodyMask>=0) {
        uint64_t mask=0; int count=0;
        const float* palette=FirstPersonBodyPalette(mask,count);
        const bool active=palette && ps.locJoints>=0;
        if (active || ps.bodyMaskLive) {
            const float visible[4]={float(mask&0xffffu),float((mask>>16)&0xffffu),
                float(mask>>32),active ? 1.f : 0.f};
            gl::Uniform4fv(ps.locBodyMask,1,visible);
            ps.bodyMaskLive=active;
        }
        if (active) {
            gl::Uniform4fv(ps.locJoints,count*3,palette);
            // Force the engine to restore its own palette on the following draw.
            VidState().consts|=kJoints;
        }
    }
'''+s[a:]; p.write_text(s)
