# grafika1 - Graph on Hyperbolic Plane

## Specification

Create a program that displays a graph on a hyperbolic plane. <br />
The graph is projected onto the screen using the Beltrami-Klein method.

THe graph is random, with 50 nodes and 5% saturation.

The initial position of nodes is determined by a heuristic (in my case randomly moving the nodes). <br />
The following posiitions are determined by a force-driven model (under hyperbolic topology).

The force-driven model starts when pressing the SPACE. <br />
Subsequent presses of SPACE rearranges the graph.

The user can move the graph by pressing the mouse. <br />
Note that the whole graph remains visible as it is on a hyperbolic plane.

Each node is a circle of the hyperbolic plane. <br />
Each node has a unique texture.

## Result

Before SPACE: <br />
![Before](https://user-images.githubusercontent.com/59647190/161547995-0287c917-93bd-48aa-9f29-59b5a64e1b69.png)

After SPACE: <br />
![After](https://user-images.githubusercontent.com/59647190/161548504-80d5dd6c-f38d-4abc-a52e-2994f6a7767e.png)
