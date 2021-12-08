// The MIT License (MIT)
//
// Copyright (c) 2013 Dan Ginsburg, Budirijanto Purnomo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
// Book:      OpenGL(R) ES 3.0 Programming Guide, 2nd Edition
// Authors:   Dan Ginsburg, Budirijanto Purnomo, Dave Shreiner, Aaftab Munshi
// ISBN-10:   0-321-93388-5
// ISBN-13:   978-0-321-93388-1
// Publisher: Addison-Wesley Professional
// URLs:      http://www.opengles-book.com
//            http://my.safaribooksonline.com/book/animation-and-3d/9780133440133
//
// Simple_Texture2D.c
//
//    This is a simple example that draws a quad with a 2D
//    texture image. The purpose of this example is to demonstrate
//    the basics of 2D texturing
//
#include <stdlib.h>
#include <initguid.h>
#include <d3d12.h>
#include "esUtil.h"

#pragma comment(lib, "d3d12.lib")

PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

ID3D12Device *d3d12device;
ID3D12Resource *yuvTex;

typedef struct
{
   // Handle to a program object
   GLuint programObject;

   // Sampler location
   GLint samplerLoc;

   EGLImageKHR image;
   GLuint importedTextureId;

} UserData;

///
// Create a simple 2x2 texture image with four different colors
//
EGLImageKHR CreateSimpleTexture2D(ESContext *esContext)
{
    ID3D12Debug *debug;
    D3D12GetDebugInterface(&IID_ID3D12Debug, &debug);
    debug->lpVtbl->EnableDebugLayer(debug);

    D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &d3d12device);

    ID3D12CommandQueue *queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    d3d12device->lpVtbl->CreateCommandQueue(d3d12device, &queue_desc, &IID_ID3D12CommandQueue, &queue);

    ID3D12CommandAllocator *allocator;
    d3d12device->lpVtbl->CreateCommandAllocator(d3d12device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, &allocator);

    ID3D12Fence *fence;
    d3d12device->lpVtbl->CreateFence(d3d12device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &fence);

    ID3D12GraphicsCommandList *list;
    d3d12device->lpVtbl->CreateCommandList(d3d12device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, &IID_ID3D12GraphicsCommandList, &list);

    D3D12_RESOURCE_DESC nv12_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Format = DXGI_FORMAT_NV12,
        .Width = 4,
        .Height = 4,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .SampleDesc = {.Count = 1},
    };
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[2];
    UINT64 staging_size = 0;
    d3d12device->lpVtbl->GetCopyableFootprints(d3d12device, &nv12_desc, 0, 2, 0, layouts, NULL, NULL, &staging_size);

    D3D12_HEAP_PROPERTIES nv12_props = { .Type = D3D12_HEAP_TYPE_DEFAULT };

    d3d12device->lpVtbl->CreateCommittedResource(d3d12device, &nv12_props, D3D12_HEAP_FLAG_SHARED, &nv12_desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, &yuvTex);

    ID3D12Resource *staging;
    D3D12_RESOURCE_DESC staging_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Format = DXGI_FORMAT_UNKNOWN,
        .Width = staging_size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .SampleDesc = {.Count = 1 },
    };
    D3D12_HEAP_PROPERTIES staging_props = { .Type = D3D12_HEAP_TYPE_UPLOAD };
    d3d12device->lpVtbl->CreateCommittedResource(d3d12device, &staging_props, D3D12_HEAP_FLAG_NONE, &staging_desc, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &staging);

    void *texelData;
    staging->lpVtbl->Map(staging, 0, NULL, &texelData);

    UINT8 *ys = (UINT *)texelData;
    UINT8 *uvs = ys + layouts[1].Offset;
    for (unsigned y = 0; y < layouts[0].Footprint.Height; ++y) {
        UINT8 *ys_row = ys + layouts[0].Footprint.RowPitch * y;
        for (unsigned x = 0; x < layouts[0].Footprint.Width; ++x) {
            ys_row[x] = y * x * 8;
        }
    }
    for (unsigned y = 0; y < layouts[1].Footprint.Height; ++y) {
        UINT8 *uvs_row = uvs + layouts[1].Footprint.RowPitch * y;
        for (unsigned x = 0; x < layouts[1].Footprint.Width; ++x) {
            uvs_row[x * 2] = y * 255;
            uvs_row[x * 2 + 1] = x * 255;
        }
    }

    staging->lpVtbl->Unmap(staging, 0, NULL);

    for (unsigned i = 0; i < 2; ++i) {
        D3D12_TEXTURE_COPY_LOCATION copy_src = {
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = layouts[i],
            .pResource = staging
        };
        D3D12_TEXTURE_COPY_LOCATION copy_dest = {
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = i,
            .pResource = yuvTex
        };

        list->lpVtbl->CopyTextureRegion(list, &copy_dest, 0, 0, 0, &copy_src, NULL);
    }

    D3D12_RESOURCE_BARRIER barrier = { .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = {
            .pResource = yuvTex,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = D3D12_RESOURCE_STATE_COMMON,
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        }
    };
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);

    list->lpVtbl->Close(list);
    queue->lpVtbl->ExecuteCommandLists(queue, 1, &list);
    queue->lpVtbl->Signal(queue, fence, 1);
    fence->lpVtbl->SetEventOnCompletion(fence, 1, NULL);

    HANDLE shared_handle = NULL;
    d3d12device->lpVtbl->CreateSharedHandle(d3d12device, yuvTex, NULL, GENERIC_ALL, NULL, &shared_handle);

    EGLint attrib[] = { EGL_NONE };
    return eglCreateImageKHR(esContext->eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, shared_handle, attrib);
}


///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
   UserData *userData = esContext->userData;
   char vShaderStr[] =
      "#version 300 es                            \n"
      "layout(location = 0) in vec4 a_position;   \n"
      "layout(location = 1) in vec2 a_texCoord;   \n"
      "out vec2 v_texCoord;                       \n"
      "void main()                                \n"
      "{                                          \n"
      "   gl_Position = a_position;               \n"
      "   v_texCoord = a_texCoord;                \n"
      "}                                          \n";

   char fShaderStr[] =
      "#version 300 es                                     \n"
      "#extension GL_OES_EGL_image_external_essl3 : require\n"
      "precision mediump float;                            \n"
      "in vec2 v_texCoord;                                 \n"
      "layout(location = 0) out vec4 outColor;             \n"
      "uniform samplerExternalOES s_texture;               \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  outColor = texture( s_texture, v_texCoord );      \n"
      "}                                                   \n";

   // Load the shaders and get a linked program object
   userData->programObject = esLoadProgram ( vShaderStr, fShaderStr );

   // Get the sampler location
   userData->samplerLoc = glGetUniformLocation ( userData->programObject, "s_texture" );

   eglCreateImageKHR = eglGetProcAddress("eglCreateImageKHR");
   eglDestroyImageKHR = eglGetProcAddress("eglDestroyImageKHR");
   glEGLImageTargetTexture2DOES = eglGetProcAddress("glEGLImageTargetTexture2DOES");

   // Load the texture
   userData->image = CreateSimpleTexture2D (esContext);

   glGenTextures(1, &userData->importedTextureId);
   glBindTexture(GL_TEXTURE_EXTERNAL_OES, userData->importedTextureId);
   glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, userData->image);

   glClearColor ( 1.0f, 1.0f, 1.0f, 0.0f );
   return TRUE;
}

///
// Draw a triangle using the shader pair created in Init()
//
void Draw ( ESContext *esContext )
{
   UserData *userData = esContext->userData;
   GLfloat vVertices[] = { -0.5f,  0.5f, 0.0f,  // Position 0
                            0.0f,  0.0f,        // TexCoord 0 
                           -0.5f, -0.5f, 0.0f,  // Position 1
                            0.0f,  1.0f,        // TexCoord 1
                            0.5f, -0.5f, 0.0f,  // Position 2
                            1.0f,  1.0f,        // TexCoord 2
                            0.5f,  0.5f, 0.0f,  // Position 3
                            1.0f,  0.0f         // TexCoord 3
                         };
   GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );

   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex position
   glVertexAttribPointer ( 0, 3, GL_FLOAT,
                           GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );
   // Load the texture coordinate
   glVertexAttribPointer ( 1, 2, GL_FLOAT,
                           GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

   glEnableVertexAttribArray ( 0 );
   glEnableVertexAttribArray ( 1 );

   // Bind the texture
   glActiveTexture ( GL_TEXTURE0 );
   //glBindTexture ( GL_TEXTURE_2D, userData->textureId );

   // Set the sampler texture unit to 0
   glUniform1i ( userData->samplerLoc, 0 );

   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
}

///
// Cleanup
//
void ShutDown ( ESContext *esContext )
{
   UserData *userData = esContext->userData;

   // Delete texture object
   glDeleteTextures ( 1, &userData->importedTextureId );
   eglDestroyImageKHR(esContext->eglDisplay, userData->image);

   // Delete program object
   glDeleteProgram ( userData->programObject );
}


int esMain ( ESContext *esContext )
{
   esContext->userData = malloc ( sizeof ( UserData ) );

   esCreateWindow ( esContext, "Simple Texture 2D", 320, 240, ES_WINDOW_RGB );

   if ( !Init ( esContext ) )
   {
      return GL_FALSE;
   }

   esRegisterDrawFunc ( esContext, Draw );
   esRegisterShutdownFunc ( esContext, ShutDown );

   return GL_TRUE;
}
