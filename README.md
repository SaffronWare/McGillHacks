<img width="873" height="563" alt="image" src="https://github.com/user-attachments/assets/96819d82-af59-4989-a146-ee764f893509" />

# Physics on a 4D Hypersphere

A C++/OpenGL physics-based game and simulation built for the 2026 McGill Physics Hackathon, where the player tries to follow gravitationally interacting bodies moving on the surface of a 4D hypersphere.

This project won **1st Place in the Astrophysics Category** at the 2026 McGill Physics Hackathon.

## Overview

This project explores what orbital-style motion might look like when constrained to the surface of a 4D hypersphere. Instead of simulating motion in flat 2D or 3D space, the objects move on a curved manifold embedded in 4D.

The player observes and follows multiple gravitationally interacting bodies whose motion is constrained to remain on the hypersphere. The result is an interactive physics/graphics prototype combining differential geometry, gravity-inspired motion, ray marching, and real-time rendering.

## Features

* C++ / OpenGL / GLSL implementation
* Physics simulation on the surface of a 4D hypersphere
* Gravitationally interacting bodies
* Constraint-based motion on a curved manifold
* Geodesic-inspired movement
* Real-time ray-marched visualization
* Interactive game-style controls
* Built during the McGill Physics Hackathon
* Winner of the Astrophysics Category

## Technical Notes

The system represents each object's position and velocity in 4D. Motion is constrained so that positions remain on the surface of the hypersphere, and velocities remain tangent to the surface.

The core constraints are:

$$\vec{p} \cdot \vec{p} = R^2$$
$$\vec{p} \cdot \vec{v} = 0$$

where $\vec{p}$ is the 4D position vector, $\vec{v}$ is the 4D velocity vector, and `R` is the hypersphere radius.

The condition `p · v = 0` ensures that velocity remains tangent to the hypersphere rather than pointing outward or inward. During integration, the simulation corrects or projects the state to preserve these constraints over time.

The visualization is implemented using OpenGL and GLSL ray marching, allowing the curved 4D motion to be represented through an interactive rendered scene.

## Motivation

The goal of this project was to combine physics, geometry, and real-time graphics into an unusual interactive simulation. I wanted to explore how gravitational motion and geodesic constraints could be approximated on a curved 4D surface while still making the result playable and visually understandable during a short hackathon.

## Status

This project was built as a hackathon prototype, so the code prioritizes experimentation, visualization, and gameplay over long-term architecture.

Future improvements could include:

* More accurate geodesic integration
* Better numerical stability
* Improved gravitational model on curved geometry
* Cleaner projection methods for maintaining constraints
* More polished visualization and controls
* Better explanation of the underlying geometry
* Extended gameplay mechanics

## Running the Project

Build the project with CMake and run the executable.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Exact build steps may vary depending on the current repository structure and development environment.
