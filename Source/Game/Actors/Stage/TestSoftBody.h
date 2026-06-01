// SOFT_BODY

// Making a 2D soft-body physics engine
// https://lisyarus.github.io/blog/posts/soft-body-physics.html
// 3D shape matching with quaternions
// https://lisyarus.github.io/blog/posts/3d-shape-matching-with-quaternions.html

#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>

//#define TINYGLTF_NO_EXTERNAL_IMAGE
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/unordered_map.hpp>


// SOFT_BODY
#define CLOTH_EDGES_PER_VERTEX 8

class soft_body
{
	std::string filename;
public:
	soft_body(ID3D11Device* device, const std::string& filename);
	virtual ~soft_body() = default;

	struct scene
	{
		std::string name;
		std::vector<int> nodes; // Array of 'root' nodes

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name),
				cereal::make_nvp("nodes", nodes)
			);
		}
	};
	std::vector<scene> scenes;
	int default_scene = 0;

	struct node
	{
		std::string name;
		int skin = -1;  // index of skin referenced by this node
		int mesh = -1;  // index of mesh referenced by this node

		std::vector<int> children; // An array of indices of child nodes of this node

		// Local transforms
		DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };
		DirectX::XMFLOAT3 scale = { 1, 1, 1 };
		DirectX::XMFLOAT3 translation = { 0, 0, 0 };

		DirectX::XMFLOAT4X4 global_transform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name),
				cereal::make_nvp("skin", skin),
				cereal::make_nvp("mesh", mesh),
				cereal::make_nvp("children", children),
				cereal::make_nvp("rotation", rotation),
				cereal::make_nvp("scale", scale),
				cereal::make_nvp("translation", translation),
				cereal::make_nvp("global_transform", global_transform)
			);
		}
	};
	std::vector<node> nodes;


	struct index_buffer_view
	{
		int buffer = -1;
		UINT size_in_bytes = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("buffer", buffer),
				cereal::make_nvp("size_in_bytes", size_in_bytes),
				cereal::make_nvp("format", format)
			);
		}
	};
	struct vertex_buffer_view
	{
		int buffer = -1;
		UINT size_in_bytes = 0;
		UINT stride_in_bytes = 0;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("buffer", buffer),
				cereal::make_nvp("size_in_bytes", size_in_bytes),
				cereal::make_nvp("stride_in_bytes", stride_in_bytes)
			);
		}
	};



	// SOFT_BODY
	struct cloth_vertex
	{
		DirectX::XMFLOAT3 old_velocity = { 0, 0, 0 };
		float padding_0 = 0;
		DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
		float padding_1 = 0;
		DirectX::XMFLOAT3 new_position = { 0, 0, 0 };
		float padding_2 = 0;
		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("old_velocity", old_velocity),
				cereal::make_nvp("velocity", velocity),
				cereal::make_nvp("new_position", new_position)
			);
		}
	};

	// SOFT_BODY
	struct cloth_edge
	{
		DirectX::XMFLOAT4 delta = { 0, 0, 0, 0 };
		std::uint32_t id = std::uint32_t(-1);
		float padding[3] = {};
		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("delta", delta),
				cereal::make_nvp("id", id)
			);
		}
	};

	struct vertex
	{
		DirectX::XMFLOAT3 position = { 0, 0, 0 };
		DirectX::XMFLOAT3 normal = { 0, 0, 1 };
		DirectX::XMFLOAT4 tangent = { 1, 0, 0, 1 };
		DirectX::XMFLOAT2 texcoord = { 0, 0 };
		DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("position", position),
				cereal::make_nvp("normal", normal),
				cereal::make_nvp("tangent", tangent),
				cereal::make_nvp("texcoord", texcoord),
				cereal::make_nvp("rotation", rotation)
			);
		}
	};
	struct mesh
	{
		std::string name;

		struct primitive
		{
			int material;

			std::vector<std::uint32_t/*SOFT_BODY*/> cached_indices;
			index_buffer_view index_buffer_view;

			std::vector<vertex> cached_vertices;
			vertex_buffer_view vertex_buffer_view;

			// SOFT_BODY
			std::vector<cloth_edge> cloth_edges;
			std::vector<cloth_vertex> cloth_vertices;

			// SOFT_BODY
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> vertex_uav;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertex_srv;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cloth_vertex_uav;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloth_vertex_srv;
			//Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cloth_edge_uav;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloth_edge_srv;

			std::unordered_map<std::string, DXGI_FORMAT> attributes;

			bool has(const char* attribute) const
			{
				return attributes.find(attribute) != attributes.end();
			}

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("material", material),
					cereal::make_nvp("cached_indices", cached_indices),
					cereal::make_nvp("index_buffer_view", index_buffer_view),
					cereal::make_nvp("cached_vertices", cached_vertices),
					cereal::make_nvp("vertex_buffer_view", vertex_buffer_view),
					// SOFT_BODY
					cereal::make_nvp("cloth_edges", cloth_edges),
					cereal::make_nvp("cloth_vertices", cloth_vertices)
				);
			}
		};
		std::vector<primitive> primitives;

		template<class T>
		void serialize(T& archive)
		{
			archive(cereal::make_nvp("name", name), cereal::make_nvp("primitives", primitives));
		}
	};
	std::vector<mesh> meshes;

	// INTERLEAVED_GLTF_MODEL
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;

	void render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world);
	// SOFT_BODY
	void simulate(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, float delta_time);

	struct texture_info
	{
		int index = -1; // required.
		int texcoord = 0; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("index", index),
				cereal::make_nvp("texcoord", texcoord)
			);
		}
	};
	// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-normaltextureinfo
	struct normal_texture_info
	{
		int index = -1;  // required
		int texcoord = 0;    // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
		float scale = 1;    // scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("index", index),
				cereal::make_nvp("texcoord", texcoord),
				cereal::make_nvp("scale", scale)
			);
		}
	};
	// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-occlusiontextureinfo
	struct occlusion_texture_info
	{
		int index = -1;   // required
		int texcoord = 0;     // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
		float strength = 1;  // A scalar parameter controlling the amount of occlusion applied. A value of `0.0` means no occlusion. A value of `1.0` means full occlusion. This value affects the final occlusion value as: `1.0 + strength * (<sampled occlusion texture value> - 1.0)`.

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("index", index),
				cereal::make_nvp("texcoord", texcoord),
				cereal::make_nvp("strength", strength)
			);
		}
	};
	// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-pbrmetallicroughness
	struct pbr_metallic_roughness
	{
		float basecolor_factor[4] = { 1, 1, 1, 1 };  // len = 4. default [1,1,1,1]
		texture_info basecolor_texture;
		float metallic_factor = 1;   // default 1
		float roughness_factor = 1;  // default 1
		texture_info metallic_roughness_texture;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("basecolor_factor", basecolor_factor),
				cereal::make_nvp("basecolor_texture", basecolor_texture),
				cereal::make_nvp("metallic_factor", metallic_factor),
				cereal::make_nvp("roughness_factor", roughness_factor),
				cereal::make_nvp("metallic_roughness_texture", metallic_roughness_texture)
			);
		}
	};
	struct material {
		std::string name;
		struct buffer
		{
			float emissive_factor[3] = { 0, 0, 0 };  // length 3. default [0, 0, 0]
			int alpha_mode = 0;	// "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
			float alpha_cutoff = 0.5f; // default 0.5
			int double_sided = 0; // default false;

			pbr_metallic_roughness pbr_metallic_roughness;

			normal_texture_info normal_texture;
			occlusion_texture_info occlusion_texture;
			texture_info emissive_texture;

			template<class T>
			void serialize(T& archive)
			{
				archive(
					cereal::make_nvp("emissive_factor", emissive_factor),
					cereal::make_nvp("alpha_mode", alpha_mode),
					cereal::make_nvp("alpha_cutoff", alpha_cutoff),
					cereal::make_nvp("double_sided", double_sided),
					cereal::make_nvp("pbr_metallic_roughness", pbr_metallic_roughness),
					cereal::make_nvp("normal_texture", normal_texture),
					cereal::make_nvp("occlusion_texture", occlusion_texture),
					cereal::make_nvp("emissive_texture", emissive_texture)
				);
			}
		};
		buffer data;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name),
				cereal::make_nvp("data", data)
			);
		}
	};
	std::vector<material> materials;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material_resource_view;

	// UNIT.36
	struct texture
	{
		std::string name;
		int source = -1;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name),
				cereal::make_nvp("source", source)
			);
		}
	};
	std::vector<texture> textures;
	struct image
	{
		std::string name;
		int width = -1;
		int height = -1;
		int component = -1;
		int bits = -1;			// bit depth per channel. 8(byte), 16 or 32.
		int pixel_type = -1;	// pixel type(TINYGLTF_COMPONENT_TYPE_***). usually UBYTE(bits = 8) or USHORT(bits = 16)
		std::string mime_type;	// (required if no uri) ["image/jpeg", "image/png", "image/bmp", "image/gif"]
		std::string uri;		// (required if no mimeType) uri is not decoded(e.g. whitespace may be represented as %20)

		// When this flag is true, data is stored to `image` in as-is format(e.g. jpeg
		// compressed for "image/jpeg" mime) This feature is good if you use custom
		// image loader function. (e.g. delayed decoding of images for faster glTF
		// parsing) Default parser for Image does not provide as-is loading feature at
		// the moment. (You can manipulate this by providing your own LoadImageData
		// function)
		bool as_is = false;

		std::vector<unsigned char> cache_data;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("name", name),
				cereal::make_nvp("width", width),
				cereal::make_nvp("height", height),
				cereal::make_nvp("component", component),
				cereal::make_nvp("bits", bits),
				cereal::make_nvp("pixel_type", pixel_type),
				cereal::make_nvp("mime_type", mime_type),
				cereal::make_nvp("uri", uri),
				cereal::make_nvp("as_is", as_is),
				cereal::make_nvp("cache_data", cache_data)
			);
		}
	};
	std::vector<image> images;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texture_resource_views;

private:
	void fetch_nodes(const tinygltf::Model& gltf_model);
	void cumulate_transforms(std::vector<node>& nodes);
	void fetch_meshes(ID3D11Device* device, const tinygltf::Model& gltf_model);


	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;

	// SOFT_BODY
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> simulate_cloth_cs;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> simulate_cloth_copy_cs;

	struct primitive_constants
	{
		DirectX::XMFLOAT4X4 world;
		int material{ -1 };
		int has_tangent{ 0 };
		int skin{ -1 };
		int padding{ 0 };
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_cbuffer;

	struct simulation_constants
	{
		DirectX::XMFLOAT4X4 inv_world;
		float dt;
		float padding[3]{ 0 };
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> simulation_cbuffer;

	void fetch_materials(ID3D11Device* device, const tinygltf::Model& gltf_model);
	void fetch_textures(ID3D11Device* device, const tinygltf::Model& gltf_model);

	void create_and_upload_resources(ID3D11Device* device);


};
