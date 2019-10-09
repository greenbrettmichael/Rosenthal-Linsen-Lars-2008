# Image-space Point Cloud Rendering

## Usage

On Windows, the convenience scripts build.bat and win_example_hand.bat have been created to demonstrate the project. 
Run build.bat to create the binaries (requires cmake to be installed https://cmake.org )
Run win_example_hand.bat to launch the project and see the hand model. The controls are mouse to look around and WASD for movement. If the model is too close to the camera, the effect will fail. 

## Description

Rosenthal, Paul & Linsen, Lars. (2008). Image-space Point Cloud Rendering. Point-based rendering approaches have gained a major interest in recent years, basically replacing global surface reconstruction with local surface estimations us-ing, for example, splats or implicit functions. Crucial to their performance in terms of rendering quality and speed is the representation of the local surface patches. We present a novel approach that goes back to the orig-inal ideas of Grossman and Dally to avoid any object-space operations and compute high-quality renderings by only applying image-space operations. Starting from a point cloud including normals, we render the lit point cloud to a texture with color, depth, and normal information. Subsequently, we apply several filter operations. In a first step, we use a mask to fill back-ground pixels with the color and normal of the adjacent pixel with smallest depth. The mask assures that only the desired pixels are filled. Similarly, in a second pass, we fill the pixels that display occluded surface parts. The resulting piecewise constant surface representation does not exhibit holes anymore and is smoothed by a standard smoothing filter in a third step. The same three steps can also be applied to the depth channel and the normal map such that a subsequent edge detection and curva-ture filtering leads to a texture that exhibits silhouettes and feature lines. Anti-aliasing along the silhouettes and feature lines can be obtained by blending the textures. When highlighting the silhouette and feature lines dur-ing blending, one obtains illustrative renderings of the 3D objects. The GPU implementation of our approach achieves interactive rates for point cloud renderings with-out any pre-computation.

https://www.paul-rosenthal.de/wp-content/uploads/2010/03/rosenthal_cgi_2008.pdf
