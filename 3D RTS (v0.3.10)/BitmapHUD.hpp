#pragma once
#include "GLCore.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

// Tiny dependency-free 5x7 bitmap font. HUD geometry is submitted as a single
// batch per line using the existing solid-color 2D shader and VAO.
inline std::array<unsigned char,7> glyph5x7(char input) {
    switch(std::toupper(static_cast<unsigned char>(input))) {
#define G(c,a,b,d,e,f,g,h) case c: return {{a,b,d,e,f,g,h}};
    G('A',14,17,17,31,17,17,17)
    G('B',30,17,17,30,17,17,30)
    G('C',15,16,16,16,16,16,15)
    G('D',30,17,17,17,17,17,30)
    G('E',31,16,16,30,16,16,31)
    G('F',31,16,16,30,16,16,16)
    G('G',15,16,16,23,17,17,15)
    G('H',17,17,17,31,17,17,17)
    G('I',31,4,4,4,4,4,31)
    G('J',7,2,2,2,18,18,12)
    G('K',17,18,20,24,20,18,17)
    G('L',16,16,16,16,16,16,31)
    G('M',17,27,21,21,17,17,17)
    G('N',17,25,21,19,17,17,17)
    G('O',14,17,17,17,17,17,14)
    G('P',30,17,17,30,16,16,16)
    G('Q',14,17,17,17,21,18,13)
    G('R',30,17,17,30,20,18,17)
    G('S',15,16,16,14,1,1,30)
    G('T',31,4,4,4,4,4,4)
    G('U',17,17,17,17,17,17,14)
    G('V',17,17,17,17,17,10,4)
    G('W',17,17,17,21,21,21,10)
    G('X',17,17,10,4,10,17,17)
    G('Y',17,17,10,4,4,4,4)
    G('Z',31,1,2,4,8,16,31)
    G('0',14,17,19,21,25,17,14)
    G('1',4,12,4,4,4,4,14)
    G('2',14,17,1,2,4,8,31)
    G('3',30,1,1,14,1,1,30)
    G('4',2,6,10,18,31,2,2)
    G('5',31,16,16,30,1,1,30)
    G('6',14,16,16,30,17,17,14)
    G('7',31,1,2,4,8,8,8)
    G('8',14,17,17,14,17,17,14)
    G('9',14,17,17,15,1,1,14)
    G(':',0,4,4,0,4,4,0)
    G('/',1,1,2,4,8,16,16)
    G('-',0,0,0,31,0,0,0)
    G('.',0,0,0,0,0,12,12)
    G('(',2,4,8,8,8,4,2)
    G(')',8,4,2,2,2,4,8)
    G('!',4,4,4,4,4,0,4)
    G('?',14,17,1,2,4,0,4)
    G('+',0,4,4,31,4,4,0)
    G('=',0,31,0,31,0,0,0)
    G('|',4,4,4,4,4,4,4)
    G('[',14,8,8,8,8,8,14)
    G(']',14,2,2,2,2,2,14)
#undef G
    default:return {{0,0,0,0,0,0,0}};
    }
}
inline void drawHUDText(GLuint shader,GLuint vao,GLuint vbo,
                        int width,int height,float x,float y,float pixel,
                        const std::string& message,float red,float green,float blue) {
    if(width<=0 || height<=0 || pixel<=0)return;
    std::vector<float> vertices;
    vertices.reserve(message.size()*140);
    const auto quad=[&](float x0,float y0,float x1,float y1){
        float a=2.f*x0/width-1.f,b=1.f-2.f*y0/height;
        float c=2.f*x1/width-1.f,d=1.f-2.f*y1/height;
        vertices.insert(vertices.end(),{a,b,c,b,c,d,a,b,c,d,a,d});
    };
    const float left=x;
    for(char c:message) {
        if(c=='\n'){x=left;y+=9.f*pixel;continue;}
        auto pattern=glyph5x7(c);
        for(int row=0;row<7;++row)
            for(int col=0;col<5;++col)
                if(pattern[static_cast<size_t>(row)] & (1u << (4-col)))
                    quad(x+col*pixel,y+row*pixel,x+(col+1)*pixel,y+(row+1)*pixel);
        x+=6.f*pixel;
    }
    if(vertices.empty())return;
    glUseProgram(shader);
    glUniform4f(glGetUniformLocation(shader,"uColor"),red,green,blue,0.95f);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size()*sizeof(float)),
                 vertices.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()/2));
}
