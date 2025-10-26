# Assignment 3 — Lighting, Materials, Gouraud and Phong Shading

**Name:** Balzhan Komekbaeva  
**Group:** SE-2301  
**Course:** Computer Graphics  

---

## 🎯 Project Overview
This project implements **Gouraud** and **Phong** shading algorithms in OpenGL using **two light sources** and **three materials**.  
It extends a 3D SMF model viewer to support realistic lighting, material reflection, and interactive light movement.

---

## 🧩 Features
- Averaged vertex normals for smooth shading  
- Two point lights:
  - One fixed near camera (eye space)
  - One rotating on a cylinder around the model (object space)
- Gouraud (per-vertex) and Phong (per-fragment) shading  
- Three materials:
  1. White Shiny  
  2. Gold  
  3. Red Bright Specular (assignment required parameters)  
- On-screen overlay instructions  
- Full interactivity (camera + light controls)

---

## 🛠️ Libraries Used
- OpenGL 2.1  
- FreeGLUT (GLUT for macOS)  
- GLSL 1.20 (for shaders)  
- C++17  

---

## ⚙️ Build & Run (macOS)
```bash
brew install freeglut
make
./Assignment3 models/bound-lo-sphere.smf
```

---

## 🎮 Controls

| Key | Function |
|-----|-----------|
| 1 | Flat shading |
| 2 | Gouraud shading |
| 3 | Phong shading |
| M | Switch materials |
| A / D | Orbit camera |
| W / S | Move camera up/down |
| Q / E | Zoom in/out |
| P | Toggle perspective / orthographic |
| Z / X | Adjust light angle |
| C / V | Adjust light radius |
| B / N | Adjust light height |
| L | Toggle auto-rotate light |
| R | Reset |
| ESC | Exit |

---

## 🎥 Demo Video

[![Watch the video]
https://youtu.be/W2-5gkAIZkA?si=EFaz4ZzmPFuxJfua 

---

## 🧠 Implementation Summary
- **Normals:** Averaged per vertex for smooth lighting  
- **Shaders:** Implemented Gouraud (vertex) and Phong (fragment)  
- **Lights:** Two point lights (eye-space + object-space)  
- **Materials:** Ambient, diffuse, specular, shininess values adjustable  
- **HUD:** Displays shading mode, material, and controls  

---

## 📊 Results
| Shading | Description |
|----------|--------------|
| **Gouraud** | Smooth, fast, less precise highlights |
| **Phong** | Realistic reflections, detailed specular lighting |

**Red Bright Specular Material Properties (Required):**
```glsl
material_ambient  = vec4(0.6, 0.2, 0.2, 1.0);
material_diffuse  = vec4(0.9, 0.1, 0.1, 1.0);
material_specular = vec4(0.8, 0.8, 0.8, 1.0);
material_shininess = 80.0;
```

---

## 🧾 Author
**Balzhan Komekbaeva**  
Group SE-2301 
Assignment 3: Lighting, Materials, Gouraud and Phong Shading
