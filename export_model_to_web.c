// This program converts any 3D asset file into a list of verticies and vertex colours.
// This is because emscripten does not seem to work with the assimp library.
// Hence I will implement my own parser for the verticies and colours for this purpose.

#include <stdio.h>
#include <stdlib.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cglm/cglm.h>

// Load the mesh from the scene nodes
void object_load_mesh(FILE* output_file, struct aiMesh* mesh) {
    // Initialise current mesh
    // Iterate through the mesh poisitons
    for (unsigned int i=0; i < mesh->mNumVertices; i++) {
        // Copy the vertex positions and print them to the new model file
        fprintf(output_file, "v %f %f %f", mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        // Copy the vertex colours and print them to the new model file
        if (mesh->mColors[0] != NULL) {
            fprintf(output_file, " %f %f %f\n", mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b);
        }
        else {
            fprintf(output_file, " %f %f %f\n", 0.0, 0.0, 0.0);
        }
    }

    // Copy the face indicies
    for (unsigned int i=0; i < mesh->mNumFaces; i++) {
        struct aiFace face = mesh->mFaces[i];
        for (unsigned int j=0; j < face.mNumIndices; j++) {
            fprintf(output_file, "f %d\n", face.mIndices[j]);
        }
    }
}

// A recursive function that goes through all the meshes in the scene and calls the load mesh function whenever it hits a mesh node
void object_assimp_load_node(FILE* output_file, struct aiNode* node, const struct aiScene* scene) {
    if (node == NULL) return;

    for (size_t i=0; i < node->mNumMeshes; i++) {
        struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        object_load_mesh(output_file, mesh);
    }

    for (size_t i=0; i < node->mNumChildren; i++) {
        object_assimp_load_node(output_file, node->mChildren[i], scene);
    }
}

int main(int argc, char* argv[]) {
    // Process arguments.
    if (argc != 2) {
        printf("export_model_to_web: Usage: ./export_model_to_web 'model_name'. Exiting.\n");
        return -1;
    }

    // Open the model file and scene and calculate the filesize:
    const struct aiScene* scene = aiImportFile(argv[1], aiProcess_CalcTangentSpace|aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_SortByPType|aiProcess_GenUVCoords);
    if (scene == NULL) {
        printf("object_new(): Failed to import model. Error: %s. Exiting.\n", aiGetErrorString());
        return -1;
    }
    struct aiNode* node = scene->mRootNode;

    // Create the file to write the output model to.
    FILE* output_file = fopen("output_model", "w+");
    if (output_file == NULL) {
        printf("object_new(): Failed to create new model file to export, exiting.\n");

        return -1;
    }

    // Load the recursive function to search the loaded scene and copy all meshes to the output model file
    object_assimp_load_node(output_file, node, scene);

    // Close loaded resources.
    aiReleaseImport(scene);
    fclose(output_file);
}