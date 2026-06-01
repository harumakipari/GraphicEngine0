#pragma once
#include <d3d11.h>
//#define TINYGLTF_NO_EXTERNAL_IMAGE
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"
#include "Engine/Debug/Assert.h"

// DXGI_FORMAT の要素サイズ（1要素分）を返す
inline UINT GetFormatByteSize(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8_UINT: return 1;
    case DXGI_FORMAT_R16_UINT: return 2;
    case DXGI_FORMAT_R32_UINT: return 4;
    case DXGI_FORMAT_R32G32_FLOAT: return 8;
    case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
    case DXGI_FORMAT_R8G8B8A8_UINT: return 4; 
    case DXGI_FORMAT_R16G16B16A16_UINT: return 8;
    case DXGI_FORMAT_R32G32B32A32_UINT: return 16;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
    default:
        assert(false && "Unsupported format.");
        return 0;
    }
}

// glTF Accessor → DXGI_FORMAT
inline DXGI_FORMAT ToDxgiFormat(const tinygltf::Accessor& accessor)
{
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_SCALAR:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return DXGI_FORMAT_R8_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return DXGI_FORMAT_R16_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return DXGI_FORMAT_R32_UINT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC2:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC3:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
	case TINYGLTF_TYPE_VEC4:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			return DXGI_FORMAT_UNKNOWN;
		}
		break;
	default:
		_ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
		return DXGI_FORMAT_UNKNOWN;
	}
}

// tinygltf の画像を無視する時のコールバック
inline bool NullLoadImage(tinygltf::Image*, const int, std::string*, std::string*,
    int, int, const unsigned char*, int, void*)
{
    return true;
}

// ストライドあり memcpy（型 T 基準）
template<class T>
inline static void CopyStride(unsigned char* d_data, const size_t d_stride, const unsigned char* s_data, const size_t s_stride, size_t count)
{
	while (count-- > 0)
	{
		*reinterpret_cast<T*>(d_data) = *reinterpret_cast<const T*>(s_data);
		s_data += s_stride;
		d_data += d_stride;
	}
};
