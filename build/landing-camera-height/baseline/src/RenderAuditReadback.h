#pragma once
#include "GL.h"
#include <cstdio>

namespace tr::renderaudit {
// Read-only, bounded diagnostic of what the driver will actually draw.
// Uniform arrays require a separate query for each vec4 element.
struct Draw {
    float view[12]{},projection[16]{},skin[24]{};
    bool skinned=false;
};
inline bool Read(GLuint program,bool body,Draw& out) {
    if(!gl::GetUniformfv || !gl::GetUniformLocation || !program) return false;
    Draw sample{};
    const GLint projection=gl::GetUniformLocation(program,"uProjMatrix");
    if(projection<0) return false;
    GLint view[3]{},skin[6]{};
    char name[40]{};
    for(int row=0;row<3;++row) {
        std::snprintf(name,sizeof(name),"uViewMatrix[%d]",row);
        view[row]=gl::GetUniformLocation(program,name);
        if(view[row]<0) return false;
    }
    if(body) for(int row=0;row<6;++row) {
        const int index=row<3 ? row : 21+row-3;
        std::snprintf(name,sizeof(name),"uJoints[%d]",index);
        skin[row]=gl::GetUniformLocation(program,name);
        if(skin[row]<0) return false;
    }
    gl::GetUniformfv(program,projection,sample.projection);
    for(int row=0;row<3;++row) gl::GetUniformfv(program,view[row],sample.view+row*4);
    if(body) for(int row=0;row<6;++row) gl::GetUniformfv(program,skin[row],sample.skin+row*4);
    sample.skinned=body;out=sample;return true;
}
}
