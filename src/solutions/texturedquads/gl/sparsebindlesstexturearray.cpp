#include "pch.h"

#include "sparsebindlesstexturearray.h"
#include "framework/gfx_gl.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
TexturedQuadsGLSparseBindlessTextureArray::TexturedQuadsGLSparseBindlessTextureArray()
: mIndexBuffer()
, mVertexBuffer()
, mProgram()
, mTransformBuffer()
, mTexAddressBuffer()
{ }

// --------------------------------------------------------------------------------------------------------------------
bool TexturedQuadsGLSparseBindlessTextureArray::Init(const std::vector<TexturedQuadsProblem::Vertex>& _vertices,
                                                     const std::vector<TexturedQuadsProblem::Index>& _indices,
                                                     const std::vector<TextureDetails*>& _textures,
                                                     size_t _objectCount)
{
    if (!TexturedQuadsSolution::Init(_vertices, _indices, _textures, _objectCount)) {
        return false;
    }

    // Prerequisites
    if (!mTexManager.Init()) {
        return false;
    }

    if (glGetTextureHandleARB == nullptr) {
        console::warn("Unable to initialize solution '%s', requires support for bindless textures (not present).", GetName().c_str());
        return false;
    }

    // Program
    const char* kUniformNames[] = { "ViewProjection", "DrawID", nullptr };

    mProgram = CreateProgramT("textures_gl_sparse_bindless_texture_array_vs.glsl",
                              "textures_gl_sparse_bindless_texture_array_fs.glsl",
                              kUniformNames, &mUniformLocation);

    if (mProgram == 0) {
        console::warn("Unable to initialize solution '%s', shader compilation/linking failed.", GetName().c_str());
        return false;
    }

    // Textures
    for (auto it = _textures.begin(); it != _textures.end(); ++it) {
        mTextures.push_back(mTexManager.newTexture2DFromDetails(*it));
    }

    // Buffers
    mVertexBuffer = NewBufferFromVector(GL_ARRAY_BUFFER, _vertices, GL_STATIC_DRAW);
    mIndexBuffer = NewBufferFromVector(GL_ELEMENT_ARRAY_BUFFER, _indices, GL_STATIC_DRAW);

    glGenBuffers(1, &mTransformBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mTransformBuffer);

    auto srcIt = mTextures.cbegin();
    std::vector<TexAddress> texAddressContents(_objectCount);
    for (auto dstIt = texAddressContents.begin(); dstIt != texAddressContents.end(); ++dstIt) {
        if (srcIt == mTextures.cend()) {
            srcIt = mTextures.cbegin();
        }

        (*dstIt) = (*srcIt)->GetAddress();
        ++srcIt;
    }

    mTexAddressBuffer = NewBufferFromVector(GL_SHADER_STORAGE_BUFFER, texAddressContents, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mTexAddressBuffer);

    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    return glGetError() == GL_NO_ERROR;
}

// --------------------------------------------------------------------------------------------------------------------
void TexturedQuadsGLSparseBindlessTextureArray::Render(const std::vector<Matrix>& _transforms)
{
    // Program
    Vec3 dir = { 0, 0, 1 };
    Vec3 at = { 0, 0, 0 };
    Vec3 up = { 0, 1, 0 };
    dir = normalize(dir);
    Vec3 eye = at - 250 * dir;
    Matrix view = matrix_look_at(eye, at, up);
    Matrix view_proj = mProj * view;

    glUseProgram(mProgram);
    glUniformMatrix4fv(mUniformLocation.ViewProjection, 1, GL_TRUE, &view_proj.x.x);

    // Input Layout. First the IB
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);

    // Then the VBs.
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedQuadsProblem::Vertex), (void*)offsetof(TexturedQuadsProblem::Vertex, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedQuadsProblem::Vertex), (void*)offsetof(TexturedQuadsProblem::Vertex, tex));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Rasterizer State
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);

    glDisable(GL_SCISSOR_TEST);

    // Blend State
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Depth Stencil State
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mTransformBuffer);
    BufferData(GL_SHADER_STORAGE_BUFFER, _transforms, GL_DYNAMIC_DRAW);
    size_t xformCount = _transforms.size();
    assert(xformCount <= mObjectCount);

    for (size_t u = 0; u < xformCount; ++u) {
        // Update the Draw ID (since we cannot use multi_draw here
        glUniform1i(mUniformLocation.DrawID, u);

        glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_SHORT, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------
void TexturedQuadsGLSparseBindlessTextureArray::Shutdown()
{
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    for (auto it = mTextures.begin(); it != mTextures.end(); ++it) {
        SafeDelete(*it);
    }

    glDeleteVertexArrays(1, &mVAO);
    glDeleteBuffers(1, &mIndexBuffer);
    glDeleteBuffers(1, &mVertexBuffer);
    glDeleteBuffers(1, &mTransformBuffer);
    glDeleteBuffers(1, &mTexAddressBuffer);
    glDeleteProgram(mProgram);

    mTextures.clear();

    mTexManager.Shutdown();
}

