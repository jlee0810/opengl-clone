#include <vector>
#include <iostream>
#include "clonegl.h"
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include <iostream>

const int width = 800;
const int height = 800;
Model *model = NULL;

Vec3f light_dir(1, 1, 1);
Vec3f eye(1, 1, 3);
Vec3f center(0, 0, 0);
Vec3f up(0, 1, 0);

// Pipeline:
// Vertex Data -> Primitive Processing -> Vertex Shader -> Primitive Assembly -> Rasterization -> Fragment Shader -> Depth & Stencil -> Color Buffer Blend -> Dither -> Frame Buffer
struct Shader : public IShader
{
    mat<2, 3, float> varying_uv;
    mat<4, 4, float> uniform_M;   //  Projection*ModelView
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    Shader(Matrix M, Matrix MIT) : varying_uv(), uniform_M(M), uniform_MIT(MIT) {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        return Viewport * Projection * ModelView * gl_Vertex;    // transform it to screen coordinates
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        Vec2f uv = varying_uv * bar;
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize(); // normal
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // light direction
        Vec3f r = (n * (n * l * 2.f) - l).normalize(); // reflected light

        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for (int i = 0; i < 3; i++)
            color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);
        return false;
    }
};

int main(int argc, char **argv)
{
    // Parse the obj file
    if (2 == argc)
    {
        model = new Model(argv[1]);
    }
    else
    {
        model = new Model("obj/african_head.obj");
    }

    lookat(eye, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (eye - center).norm());
    light_dir.normalize();

    TGAImage image(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    Shader shader(ModelView, (Projection * ModelView).invert_transpose());

    // iterate through all the triangles
    for (int i = 0; i < model->nfaces(); i++)
    {
        // iterate through all the vertices of the triangle and call the shader and interpolate the color
        Vec4f screen_coords[3];
        for (int j = 0; j < 3; j++)
        {
            // ith face and the jth vertex of that face
            screen_coords[j] = shader.vertex(i, j);
        }
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.flip_vertically();
    zbuffer.flip_vertically();
    image.write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}