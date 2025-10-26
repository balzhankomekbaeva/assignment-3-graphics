// main.cpp
// Assignment 3 - Full Part 2 implementation (macOS OpenGL + GLUT)
// Features: SMF loader, averaged vertex normals, Flat/Gouraud/Phong shading,
// two lights (camera-space & object-space), three materials (one matches assignment),
// interactive camera and light-on-cylinder controls, on-screen HUD.

#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// -------- Math helpers --------
struct Vec3 { float x,y,z; Vec3():x(0),y(0),z(0){} Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){} };
static Vec3 operator+(const Vec3&a,const Vec3&b){return Vec3(a.x+b.x,a.y+b.y,a.z+b.z);}
static Vec3 operator-(const Vec3&a,const Vec3&b){return Vec3(a.x-b.x,a.y-b.y,a.z-b.z);}
static Vec3 operator*(const Vec3&a,float s){return Vec3(a.x*s,a.y*s,a.z*s);}
static float len(const Vec3&a){return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);}
static Vec3 normalize(const Vec3&a){float L=len(a); if(L<1e-6f) return Vec3(0,0,0); return Vec3(a.x/L,a.y/L,a.z/L);}
static Vec3 cross(const Vec3&a,const Vec3&b){ return Vec3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }

// -------- Mesh --------
struct Vertex { Vec3 p; Vec3 n; };
struct Tri { int a,b,c; Vec3 fn; };

std::vector<Vertex> vertices;
std::vector<Tri> triangles;
Vec3 centroid(0,0,0);
float modelScale = 1.0f;

// -------- VBOs --------
GLuint vboPos=0, vboNorm=0, ibo=0;
GLsizei triCount=0;
bool buffersReady=false;

// -------- Camera & UI state --------
float camAngle=0.0f, camRadius=3.0f, camHeight=0.0f;
bool perspectiveOn=true;
int shadeMode=3; // 1=Flat,2=Gouraud,3=Phong
int materialIndex=0;
bool autoRotateLight=false; // by default light does NOT move automatically (requirement: move when user changes)
float lightAngle=0.0f, lightRadius=1.2f, lightHeight=0.5f;

int winW=900, winH=700;

// -------- Materials --------
struct Material {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float shininess;
    std::string name;
};
std::vector<Material> materials;

void initMaterials(){
    materials.clear();
    // White shiny
    materials.push_back({ {0.25f,0.25f,0.25f,1.0f}, {0.8f,0.8f,0.8f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, 120.0f, "White Shiny" });
    // Gold (different shading properties)
    materials.push_back({ {0.24725f,0.1995f,0.0745f,1.0f}, {0.75164f,0.60648f,0.22648f,1.0f}, {0.628281f,0.555802f,0.366065f,1.0f}, 51.2f, "Gold" });
    // Required red high-specular material (matches assignment)
    materials.push_back({ {0.6f,0.2f,0.2f,1.0f}, {0.9f,0.1f,0.1f,1.0f}, {0.8f,0.8f,0.8f,1.0f}, 80.0f, "Red Bright Spec" });
}

// -------- SMF loader & normal averaging --------
bool loadSMF(const std::string &path){
    std::ifstream fin(path);
    if(!fin.is_open()){ std::cerr << "Cannot open " << path << "\n"; return false; }
    std::vector<Vec3> pts;
    std::string line;
    while(std::getline(fin, line)){
        std::stringstream ss(line); char c; ss >> c;
        if(!ss) continue;
        if(c=='v'){ float x,y,z; ss>>x>>y>>z; pts.emplace_back(x,y,z); }
        else if(c=='f'){ int a,b,c2; ss>>a>>b>>c2; Tri t; t.a=a-1; t.b=b-1; t.c=c2-1;
            Vec3 u = pts[t.b] - pts[t.a]; Vec3 v = pts[t.c] - pts[t.a];
            t.fn = normalize(cross(u,v)); triangles.push_back(t); }
    }
    // build vertex list
    vertices.resize(pts.size());
    for(size_t i=0;i<pts.size();++i){ vertices[i].p=pts[i]; vertices[i].n=Vec3(0,0,0); }
    for(auto &t : triangles){
        vertices[t.a].n = vertices[t.a].n + t.fn;
        vertices[t.b].n = vertices[t.b].n + t.fn;
        vertices[t.c].n = vertices[t.c].n + t.fn;
    }
    for(auto &v : vertices) v.n = normalize(v.n);
    // centroid & scale
    centroid = Vec3(0,0,0);
    for(auto &v : vertices) centroid = centroid + v.p;
    centroid = centroid * (1.0f / (float)vertices.size());
    float maxd = 0.0f;
    for(auto &v : vertices){ Vec3 d = v.p - centroid; maxd = std::max(maxd, len(d)); }
    if(maxd < 1e-6f) maxd = 1.0f;
    modelScale = 1.0f / maxd;
    std::cout << "Loaded " << vertices.size() << " verts, " << triangles.size() << " tris.\n";
    return true;
}

// -------- Build VBOs --------
void buildBuffers(){
    if(buffersReady){
        glDeleteBuffers(1, &vboPos); glDeleteBuffers(1, &vboNorm); glDeleteBuffers(1, &ibo);
    }
    std::vector<float> pos, norm;
    std::vector<unsigned int> idx;
    pos.reserve(vertices.size()*3); norm.reserve(vertices.size()*3);
    for(auto &v : vertices){ pos.push_back(v.p.x); pos.push_back(v.p.y); pos.push_back(v.p.z);
                             norm.push_back(v.n.x); norm.push_back(v.n.y); norm.push_back(v.n.z); }
    idx.reserve(triangles.size()*3);
    for(auto &t : triangles){ idx.push_back(t.a); idx.push_back(t.b); idx.push_back(t.c); }
    glGenBuffers(1, &vboPos); glBindBuffer(GL_ARRAY_BUFFER, vboPos); glBufferData(GL_ARRAY_BUFFER, pos.size()*sizeof(float), pos.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &vboNorm); glBindBuffer(GL_ARRAY_BUFFER, vboNorm); glBufferData(GL_ARRAY_BUFFER, norm.size()*sizeof(float), norm.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    triCount = (GLsizei)triangles.size();
    buffersReady = true;
}

// -------- Simple shader utilities (GLSL 1.20) --------
static GLuint compileShader(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){ GLint len=0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len); std::vector<char> log(len+1); glGetShaderInfoLog(s, len, NULL, log.data()); std::cerr<<"Shader compile error: "<<log.data()<<"\n"; }
    return s;
}
static GLuint linkProgram(GLuint vs, GLuint fs){
    GLuint p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ GLint len=0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len); std::vector<char> log(len+1); glGetProgramInfoLog(p, len, NULL, log.data()); std::cerr<<"Program link error: "<<log.data()<<"\n"; }
    return p;
}

// -------- Shaders (Gouraud: vertex-lit, Phong: fragment-lit) --------
static const char* gouraud_vs = R"GLSL(
#version 120
attribute vec3 inPos;
attribute vec3 inNorm;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
uniform vec4 material_ambient;
uniform vec4 material_diffuse;
uniform vec4 material_specular;
uniform float material_shininess;
uniform vec3 light0_pos_eye;
uniform vec4 light0_ambient;
uniform vec4 light0_diffuse;
uniform vec4 light0_specular;
uniform vec3 light1_pos_eye;
uniform vec4 light1_ambient;
uniform vec4 light1_diffuse;
uniform vec4 light1_specular;
varying vec4 vColor;
void main(){
    vec4 posEye = modelViewMatrix * vec4(inPos,1.0);
    vec3 N = normalize(normalMatrix * inNorm);
    vec3 V = normalize(-posEye.xyz);
    vec3 L0 = normalize(light0_pos_eye - posEye.xyz);
    float nL0 = max(dot(N,L0), 0.0);
    vec3 R0 = reflect(-L0,N);
    float s0 = (nL0>0.0)?pow(max(dot(R0,V),0.0), material_shininess):0.0;
    vec3 L1 = normalize(light1_pos_eye - posEye.xyz);
    float nL1 = max(dot(N,L1), 0.0);
    vec3 R1 = reflect(-L1,N);
    float s1 = (nL1>0.0)?pow(max(dot(R1,V),0.0), material_shininess):0.0;
    vec4 ambient = material_ambient * (light0_ambient + light1_ambient);
    vec4 diffuse = material_diffuse * (light0_diffuse * nL0 + light1_diffuse * nL1);
    vec4 spec = material_specular * (light0_specular * s0 + light1_specular * s1);
    vColor = ambient + diffuse + spec;
    gl_Position = projectionMatrix * posEye;
}
)GLSL";

static const char* gouraud_fs = R"GLSL(
#version 120
varying vec4 vColor;
void main(){ gl_FragColor = clamp(vColor, 0.0, 1.0); }
)GLSL";

static const char* phong_vs = R"GLSL(
#version 120
attribute vec3 inPos;
attribute vec3 inNorm;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
varying vec3 vPosEye;
varying vec3 vNormalEye;
void main(){
    vec4 posEye = modelViewMatrix * vec4(inPos,1.0);
    vPosEye = posEye.xyz;
    vNormalEye = normalize(normalMatrix * inNorm);
    gl_Position = projectionMatrix * posEye;
}
)GLSL";

static const char* phong_fs = R"GLSL(
#version 120
varying vec3 vPosEye;
varying vec3 vNormalEye;
uniform vec4 material_ambient;
uniform vec4 material_diffuse;
uniform vec4 material_specular;
uniform float material_shininess;
uniform vec3 light0_pos_eye;
uniform vec4 light0_ambient;
uniform vec4 light0_diffuse;
uniform vec4 light0_specular;
uniform vec3 light1_pos_eye;
uniform vec4 light1_ambient;
uniform vec4 light1_diffuse;
uniform vec4 light1_specular;
void main(){
    vec3 N = normalize(vNormalEye);
    vec3 V = normalize(-vPosEye);
    vec3 L0 = normalize(light0_pos_eye - vPosEye);
    float nL0 = max(dot(N,L0), 0.0);
    vec3 R0 = reflect(-L0, N);
    float s0 = (nL0>0.0)?pow(max(dot(R0,V),0.0), material_shininess):0.0;
    vec3 L1 = normalize(light1_pos_eye - vPosEye);
    float nL1 = max(dot(N,L1), 0.0);
    vec3 R1 = reflect(-L1, N);
    float s1 = (nL1>0.0)?pow(max(dot(R1,V),0.0), material_shininess):0.0;
    vec4 ambient = material_ambient * (light0_ambient + light1_ambient);
    vec4 diffuse = material_diffuse * (light0_diffuse * nL0 + light1_diffuse * nL1);
    vec4 spec = material_specular * (light0_specular * s0 + light1_specular * s1);
    vec4 color = ambient + diffuse + spec;
    gl_FragColor = clamp(color, 0.0, 1.0);
}
)GLSL";

// Program IDs
GLuint progGouraud=0, progPhong=0;

void createPrograms(){
    GLuint g_vs = compileShader(GL_VERTEX_SHADER, gouraud_vs);
    GLuint g_fs = compileShader(GL_FRAGMENT_SHADER, gouraud_fs);
    progGouraud = linkProgram(g_vs, g_fs);
    GLuint p_vs = compileShader(GL_VERTEX_SHADER, phong_vs);
    GLuint p_fs = compileShader(GL_FRAGMENT_SHADER, phong_fs);
    progPhong = linkProgram(p_vs, p_fs);
}

// Utility: push current modelview & projection into program uniforms
void setCommonUniforms(GLuint prog){
    if(!prog) return;
    GLfloat mv[16], proj[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, proj);
    GLint loc = glGetUniformLocation(prog, "modelViewMatrix"); if(loc>=0) glUniformMatrix4fv(loc,1,GL_FALSE,mv);
    loc = glGetUniformLocation(prog, "projectionMatrix"); if(loc>=0) glUniformMatrix4fv(loc,1,GL_FALSE,proj);
    // normalMatrix 3x3
    GLfloat nm[9] = { mv[0], mv[1], mv[2], mv[4], mv[5], mv[6], mv[8], mv[9], mv[10] };
    loc = glGetUniformLocation(prog, "normalMatrix"); if(loc>=0) glUniformMatrix3fv(loc,1,GL_FALSE,nm);
}

// -------- Draw mesh (flat via fixed-function, or shader paths using VBOs) --------
void drawMesh(){
    glPushMatrix();
    glTranslatef(-centroid.x, -centroid.y, -centroid.z);
    glScalef(modelScale, modelScale, modelScale);

    // compute object-space rotating light position using current lightAngle/radius/height
    float la = lightAngle;
    Vec3 light1_obj = Vec3(lightRadius * cosf(la), lightRadius * sinf(la), lightHeight);

    if(shadeMode == 1){
        // Flat shading with face normal color
        glUseProgram(0);
        glShadeModel(GL_FLAT);
        glBegin(GL_TRIANGLES);
        for(auto &t : triangles){
            Vec3 fn = normalize(t.fn);
            glColor3f(fabs(fn.x), fabs(fn.y), fabs(fn.z));
            glNormal3f(fn.x, fn.y, fn.z);
            Vec3 &A = vertices[t.a].p; glVertex3f(A.x, A.y, A.z);
            Vec3 &B = vertices[t.b].p; glVertex3f(B.x, B.y, B.z);
            Vec3 &C = vertices[t.c].p; glVertex3f(C.x, C.y, C.z);
        }
        glEnd();
    } else {
        // Shader path (Gouraud or Phong)
        GLuint prog = (shadeMode==2)?progGouraud:progPhong;
        glUseProgram(prog);
        setCommonUniforms(prog);
        // material uniforms
        Material &m = materials[materialIndex];
        GLint loc;
        loc = glGetUniformLocation(prog,"material_ambient"); if(loc>=0) glUniform4fv(loc,1,m.ambient);
        loc = glGetUniformLocation(prog,"material_diffuse"); if(loc>=0) glUniform4fv(loc,1,m.diffuse);
        loc = glGetUniformLocation(prog,"material_specular"); if(loc>=0) glUniform4fv(loc,1,m.specular);
        loc = glGetUniformLocation(prog,"material_shininess"); if(loc>=0) glUniform1f(loc,m.shininess);

        // Light0: camera-space near eye
        float light0_eye[3] = { 0.0f, 0.0f, 1.5f };
        loc = glGetUniformLocation(prog,"light0_pos_eye"); if(loc>=0) glUniform3fv(loc,1,light0_eye);
        float l0amb[4]={0.2f,0.2f,0.2f,1.0f}, l0dif[4]={0.6f,0.6f,0.6f,1.0f}, l0spec[4]={1.0f,1.0f,1.0f,1.0f};
        loc = glGetUniformLocation(prog,"light0_ambient"); if(loc>=0) glUniform4fv(loc,1,l0amb);
        loc = glGetUniformLocation(prog,"light0_diffuse"); if(loc>=0) glUniform4fv(loc,1,l0dif);
        loc = glGetUniformLocation(prog,"light0_specular"); if(loc>=0) glUniform4fv(loc,1,l0spec);

        // Light1: object-space light transformed to eye coords using current MODELVIEW (we are in model transform now)
        // get current modelview
        GLfloat mv[16]; glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        // multiply mv * vec4(light1_obj, 1.0)
        float lx = mv[0]*light1_obj.x + mv[4]*light1_obj.y + mv[8]*light1_obj.z + mv[12];
        float ly = mv[1]*light1_obj.x + mv[5]*light1_obj.y + mv[9]*light1_obj.z + mv[13];
        float lz = mv[2]*light1_obj.x + mv[6]*light1_obj.y + mv[10]*light1_obj.z + mv[14];
        float light1_eye[3] = { lx, ly, lz };
        loc = glGetUniformLocation(prog,"light1_pos_eye"); if(loc>=0) glUniform3fv(loc,1,light1_eye);
        float l1amb[4]={0.0f,0.0f,0.0f,1.0f}, l1dif[4]={0.8f,0.5f,0.2f,1.0f}, l1spec[4]={0.8f,0.8f,0.8f,1.0f};
        loc = glGetUniformLocation(prog,"light1_ambient"); if(loc>=0) glUniform4fv(loc,1,l1amb);
        loc = glGetUniformLocation(prog,"light1_diffuse"); if(loc>=0) glUniform4fv(loc,1,l1dif);
        loc = glGetUniformLocation(prog,"light1_specular"); if(loc>=0) glUniform4fv(loc,1,l1spec);

        // attribute locations
        GLint posLoc = glGetAttribLocation(prog, "inPos");
        GLint normLoc = glGetAttribLocation(prog, "inNorm");

        if(posLoc>=0){
            glEnableVertexAttribArray((GLuint)posLoc);
            glBindBuffer(GL_ARRAY_BUFFER, vboPos);
            glVertexAttribPointer((GLuint)posLoc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        }
        if(normLoc>=0){
            glEnableVertexAttribArray((GLuint)normLoc);
            glBindBuffer(GL_ARRAY_BUFFER, vboNorm);
            glVertexAttribPointer((GLuint)normLoc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glDrawElements(GL_TRIANGLES, triCount*3, GL_UNSIGNED_INT, (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        if(posLoc>=0) glDisableVertexAttribArray((GLuint)posLoc);
        if(normLoc>=0) glDisableVertexAttribArray((GLuint)normLoc);
        glUseProgram(0);
    }

    // Draw the light marker cube in object coordinates
    glPushMatrix();
        glTranslatef(light1_obj.x, light1_obj.y, light1_obj.z);
        glScalef(0.03f / modelScale, 0.03f / modelScale, 0.03f / modelScale);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.6f, 0.2f);
        glutSolidCube(1.0);
        glEnable(GL_LIGHTING);
    glPopMatrix();

    glPopMatrix();
}

// -------- HUD overlay (translucent box + text) --------
void drawOverlay(const std::vector<std::string> &lines){
    // Save state
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,1,0,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // translucent box
    glColor4f(0.0f, 0.0f, 0.0f, 0.62f);
    glBegin(GL_QUADS);
        glVertex2f(0.58f, 0.96f); glVertex2f(0.98f, 0.96f);
        glVertex2f(0.98f, 0.18f); glVertex2f(0.58f, 0.18f);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    float y = 0.92f;
    for(auto &s : lines){
        glRasterPos2f(0.60f, y);
        for(char c : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        y -= 0.05f;
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// -------- Setup camera & projection --------
void setupCamera(){
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (winH==0) ? 1.0f : (float)winW / (float)winH;
    if(perspectiveOn) gluPerspective(60.0, aspect, 0.1, 50.0);
    else {
        float s=1.8f;
        glOrtho(-s*aspect, s*aspect, -s, s, 0.1, 50.0);
    }
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float camX = camRadius * cosf(camAngle);
    float camY = camRadius * sinf(camAngle);
    float camZ = camHeight;
    gluLookAt(camX, camY, camZ,  0.0,0.0,0.0,  0.0,0.0,1.0);
}

// -------- Display / idle / input --------
void display(){
    glClearColor(0.06f,0.06f,0.06f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera();

    // set fixed-function light0 (camera-space) to keep GL state consistent
    GLfloat l0pos[4] = {0.0f, 0.0f, 1.5f, 1.0f};
    GLfloat l0amb[4] = {0.2f,0.2f,0.2f,1.0f}, l0dif[4] = {0.6f,0.6f,0.6f,1.0f}, l0spec[4] = {1.0f,1.0f,1.0f,1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, l0amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l0dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, l0spec);

    // set material (for fixed-function fallback & cube)
    Material &mat = materials[materialIndex];
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat.ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat.shininess);

    // draw model
    drawMesh();

    // HUD lines (including light controls & current light params)
    std::vector<std::string> lines;
    lines.push_back("Material: " + materials[materialIndex].name);
    lines.push_back(std::string("Shade: ") + (shadeMode==1?"Flat":shadeMode==2?"Gouraud":"Phong"));
    lines.push_back("Controls:");
    lines.push_back("A/D - orbit   W/S - height   Q/E - radius   P - projection");
    lines.push_back("1-Flat  2-Gouraud  3-Phong   M - material");
    lines.push_back("L - toggle auto-rotate light (default OFF)");
    lines.push_back("Light1 (object coords): Z/X angle  C/V radius  B/N height");
    // show numeric light params
    char buf[128];
    snprintf(buf, sizeof(buf), "Light angle: %.2f  radius: %.2f  height: %.2f", lightAngle, lightRadius, lightHeight);
    lines.push_back(std::string(buf));
    lines.push_back("R - reset   ESC - exit");
    drawOverlay(lines);

    glutSwapBuffers();
}

void idle(){
    if(autoRotateLight) {
        lightAngle += 0.01f;
        if(lightAngle > 6.28318530718f) lightAngle -= 6.28318530718f;
        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int x, int y){
    switch(key){
        case 27: exit(0); break;
        case 'a': camAngle -= 0.05f; break;
        case 'd': camAngle += 0.05f; break;
        case 'w': camHeight += 0.08f; break;
        case 's': camHeight -= 0.08f; break;
        case 'q': camRadius += 0.08f; break;
        case 'e': camRadius = std::max(0.1f, camRadius - 0.08f); break;
        case 'p': perspectiveOn = !perspectiveOn; break;
        case '1': shadeMode = 1; break;
        case '2': shadeMode = 2; break;
        case '3': shadeMode = 3; break;
        case 'm': materialIndex = (materialIndex + 1) % materials.size(); break;
        case 'l': autoRotateLight = !autoRotateLight; break;
        case 'r': camAngle=0; camRadius=3.0f; camHeight=0.0f; lightAngle=0.0f; lightRadius=1.2f; lightHeight=0.5f; break;

        // Light1 manual controls (object-space cylinder)
        case 'z': lightAngle -= 0.08f; break; // angle--
        case 'x': lightAngle += 0.08f; break; // angle++
        case 'c': lightRadius = std::max(0.05f, lightRadius - 0.05f); break; // radius--
        case 'v': lightRadius += 0.05f; break; // radius++
        case 'b': lightHeight -= 0.05f; break; // height--
        case 'n': lightHeight += 0.05f; break; // height++
    }
    glutPostRedisplay();
}

void reshape(int w, int h){ winW=w; winH=h; glViewport(0,0,w,h); }

// -------- GL init & main --------
void initGL(){
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);
    glClearDepth(1.0f);
}

int main(int argc, char** argv){
    if(argc < 2){ std::cerr << "Usage: ./Assignment3 models/your.smf\n"; return 1; }
    if(!loadSMF(argv[1])) return 1;

    initMaterials();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Assignment 3 - Part 2 Complete");

    initGL();
    createPrograms();
    buildBuffers();

    // default: light does NOT auto-rotate (user must change it or toggle auto-rotate)
    autoRotateLight = false;

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);

    glutMainLoop();
    return 0;
}
